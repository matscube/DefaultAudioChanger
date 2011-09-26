#include "StdAfx.h"
#include "DevicesManager.h"
#include <strsafe.h>

#define RETURN_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

CDevicesManager::CDevicesManager(void):pEnum(NULL)
{
}


CDevicesManager::~CDevicesManager(void)
{
	for(auto it=devices.begin();it!=devices.end();++it)
	{
		delete[] (*it).deviceId;
		delete[] (*it).deviceName;
		DestroyIcon((*it).largeIcon);
		DestroyIcon((*it).smallIcon);
	}
	devices.clear();
}

void CDevicesManager::ReleaseDeviceEnumerator()
{
	SAFE_RELEASE(pEnum)
}

HRESULT CDevicesManager::InitializeDeviceEnumerator()
{
	if(!pEnum)
	{
		return CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
			CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
	}

	return S_OK;
}
HRESULT CDevicesManager::SwitchDevices(std::vector<LPWSTR> *ids)
{
	if(ids->size()<=0 ||!ids) return E_FAIL;
	const PAUDIODEVICE defaultDevice=GetDefaultDevice();
	if(!defaultDevice) return E_FAIL;
	int defaultDeviceIndex=-1;
	int count=0;
	for(auto it=ids->cbegin();it!=ids->cend();++it)
	{
		if(!wcscmp(defaultDevice->deviceId,(*it)))
		{
			defaultDeviceIndex=count;
			break;
		}
		count++;
	}
	int newDefaultDeviceIndex=defaultDeviceIndex;
	newDefaultDeviceIndex++;
	if(newDefaultDeviceIndex>=ids->size())
	{
		newDefaultDeviceIndex=0;
	}

	LPWSTR newId=ids->at(newDefaultDeviceIndex);
	return SetDefaultDevice(newId);
}


HRESULT CDevicesManager::SetDefaultDevice(LPWSTR id)
{
	IPolicyConfigVista *pPolicyConfig;
	ERole reserved = eConsole;

    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient), 
		NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID *)&pPolicyConfig);
	if (SUCCEEDED(hr))
	{		
		hr = pPolicyConfig->SetDefaultEndpoint(id, eConsole);
		hr = pPolicyConfig->SetDefaultEndpoint(id, eMultimedia);
		hr = pPolicyConfig->SetDefaultEndpoint(id, eCommunications);
		pPolicyConfig->Release();
	}

	return hr;

}

HRESULT CDevicesManager::LoadAudioDevices()
{
	if(!pEnum)
	{
		return E_FAIL;
	}
	IMMDeviceCollection *devicesCollection=NULL;
	IMMDevice *device=NULL;
	IPropertyStore *pProps = NULL;
	LPWSTR pwszID = NULL;
	HRESULT hr=pEnum->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&devicesCollection);
	RETURN_ON_ERROR(hr)
	UINT count;
	hr=devicesCollection->GetCount(&count);
	RETURN_ON_ERROR(hr)

	for(UINT i=0;i<count;i++)
	{
		
		hr=devicesCollection->Item(i,&device);
		RETURN_ON_ERROR(hr)
		
		hr=device->GetId(&pwszID);
		RETURN_ON_ERROR(hr)
		hr=device->OpenPropertyStore(STGM_READ,&pProps);
		RETURN_ON_ERROR(hr)
		PROPVARIANT varName;
		PropVariantInit(&varName);
		hr=pProps->GetValue(PKEY_Device_FriendlyName,&varName);
		RETURN_ON_ERROR(hr)
		PROPVARIANT varIconPath;
		PropVariantInit(&varIconPath);
		hr=pProps->GetValue(PKEY_DeviceClass_IconPath,&varIconPath);
		RETURN_ON_ERROR(hr)

		AUDIODEVICE audioDeviceStruct;
		audioDeviceStruct.deviceId=new WCHAR[wcslen(pwszID)+sizeof(WCHAR)];
		wcscpy_s(audioDeviceStruct.deviceId,wcslen(pwszID)+sizeof(WCHAR),pwszID);
		audioDeviceStruct.deviceName=new WCHAR[wcslen(varName.pwszVal)+sizeof(WCHAR)];
		wcscpy_s(audioDeviceStruct.deviceName,wcslen(varName.pwszVal)+sizeof(WCHAR),varName.pwszVal);
		ExtractDeviceIcons(varIconPath.pwszVal,&audioDeviceStruct.largeIcon,&audioDeviceStruct.smallIcon);

		devices.push_back(audioDeviceStruct);

		CoTaskMemFree(pwszID);
		pwszID=NULL;
		PropVariantClear(&varIconPath);
		PropVariantClear(&varName);
		SAFE_RELEASE(pProps)
		SAFE_RELEASE(device)
	}

Exit:
	CoTaskMemFree(pwszID);
	SAFE_RELEASE(devicesCollection)
	SAFE_RELEASE(device)
	SAFE_RELEASE(pProps)
	return hr;
}
UINT CDevicesManager::ExtractDeviceIcons(LPWSTR iconPath,HICON *iconLarge,HICON *iconSmall)
{
	TCHAR *filePath;
	int iconIndex;
	WCHAR* token=NULL;
	filePath=wcstok_s(iconPath,L",",&token);
	TCHAR *iconIndexStr=wcstok_s(NULL,L",",&token);
	iconIndex=_wtoi(iconIndexStr);
	TCHAR filePathExp[MAX_PATH];
	ExpandEnvironmentStrings(filePath,filePathExp,MAX_PATH);
	//HMODULE module=LoadLibraryEx(filePathExp,NULL,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	return ExtractIconEx(filePathExp,iconIndex,iconLarge,iconSmall,1);
}
const std::vector<AUDIODEVICE>* CDevicesManager::GetAudioDevices() const
{
	return &devices;
}

const PAUDIODEVICE CDevicesManager::GetDefaultDevice()
{
	if(!pEnum)
	{
		return NULL;
	}
	
	LPWSTR pwszID = NULL;
	PAUDIODEVICE audioDevice=NULL;

	IMMDevice *device;
	HRESULT hr = pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
	RETURN_ON_ERROR(hr)
	hr=device->GetId(&pwszID);
	RETURN_ON_ERROR(hr)	

	for(auto it=devices.begin();it!=devices.end();++it)	
	{
		if(!wcscmp((*it).deviceId,pwszID))
		{
			audioDevice=&(*it);
			break;
		}
	}

Exit:	
	CoTaskMemFree(pwszID);
	SAFE_RELEASE(device)	

	return audioDevice;
}