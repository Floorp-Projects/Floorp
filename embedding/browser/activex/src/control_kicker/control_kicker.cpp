// control_kicker.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include "shlobj.h"

static HMODULE ghMozCtlX = NULL;
static HMODULE ghMozCtl = NULL;

static const _TCHAR *c_szMozCtlDll = _T("mozctl.dll");
static const _TCHAR *c_szMozillaControlKey = _T("Software\\Mozilla\\");
static const _TCHAR *c_szMozillaBinDirPathValue = _T("BinDirectoryPath");
static const _TCHAR *c_szMozCtlInProcServerKey = _T("CLSID\\{1339B54C-3453-11D2-93B9-000000000000}\\InProcServer32");

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			ghMozCtlX = (HMODULE) hModule;
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}


static BOOL LoadMozLibrary(BOOL bAskUserToSetPath = TRUE)
{
	if (ghMozCtl)
	{
		return TRUE;
	}

	ghMozCtl = LoadLibrary(c_szMozCtlDll);
	if (ghMozCtl)
	{
		return TRUE;
	}

	// Try modifying the PATH environment variable
	HKEY hkey = NULL;
	DWORD dwDisposition = 0;
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, c_szMozillaControlKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL);
	if (hkey)
	{
		_TCHAR szBinDirPath[MAX_PATH + 1];
		DWORD dwBufSize = sizeof(szBinDirPath);
		DWORD dwType = REG_SZ;
		
		ZeroMemory(szBinDirPath, dwBufSize);
		RegQueryValueEx(hkey, c_szMozillaBinDirPathValue, NULL, &dwType, (LPBYTE) szBinDirPath, &dwBufSize);

		// Append the bin dir path to the PATH environment variable
		int nPathLength = lstrlen(szBinDirPath);
		if (bAskUserToSetPath && nPathLength == 0)
		{
			// TODO ask the user if they wish to set the bin dir path to something
			// Show a folder picker for the user to choose the bin directory
			BROWSEINFO bi;
			ZeroMemory(&bi, sizeof(bi));
			bi.hwndOwner = NULL;
			bi.pidlRoot = NULL;
			bi.pszDisplayName = szBinDirPath;
			bi.lpszTitle = _T("Pick where the Mozilla bin directory is located, e.g. (c:\\mozilla\\bin)");
			LPITEMIDLIST pItemList = SHBrowseForFolder(&bi);
			if (pItemList)
			{
				// Get the path from the user selection
				IMalloc *pShellAllocator = NULL;
				SHGetMalloc(&pShellAllocator);
				if (pShellAllocator)
				{
					char szPath[MAX_PATH + 1];

					if (SHGetPathFromIDList(pItemList, szPath))
					{
						// Chop off the end path seperator
						int nPathSize = strlen(szPath);
						if (nPathSize > 0)
						{
							if (szPath[nPathSize - 1] == '\\')
							{
								szPath[nPathSize - 1] = '\0';
							}
						}

						// Form the file pattern
#ifdef UNICODE
						MultiByteToWideChar(CP_ACP, 0, szPath, szBinDirPath, -1, sizeof(szBinDirPath) / sizeof(_TCHAR));
#else
						lstrcpy(szBinDirPath, szPath);
#endif
					}
					pShellAllocator->Free(pItemList);
					pShellAllocator->Release();
				}
			}
			
			// Store the new path if it was set
			nPathLength = lstrlen(szBinDirPath);
			if (nPathLength > 0)
			{
				RegSetValueEx(hkey, c_szMozillaBinDirPathValue, 0, REG_SZ,
					(LPBYTE) szBinDirPath, _tcslen(szBinDirPath) * sizeof(_TCHAR));
			}
		}

		RegCloseKey(hkey);

		if (nPathLength > 0)
		{
			_TCHAR szPath[8192];
			
			ZeroMemory(szPath, sizeof(szPath));
			GetEnvironmentVariable("PATH", szPath, sizeof(szPath) / sizeof(_TCHAR));
			lstrcat(szPath, _T(";"));
			lstrcat(szPath, szBinDirPath);
			SetEnvironmentVariable("PATH", szPath);

			ghMozCtl = LoadLibrary(c_szMozCtlDll);
			if (ghMozCtl)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}


static BOOL FixupInProcRegistryEntry()
{
	_TCHAR szMozCtlXLongPath[MAX_PATH+1];
	ZeroMemory(szMozCtlXLongPath, sizeof(szMozCtlXLongPath));
	GetModuleFileName(ghMozCtlX, szMozCtlXLongPath, sizeof(szMozCtlXLongPath) * sizeof(_TCHAR));

	_TCHAR szMozCtlXPath[MAX_PATH+1];
	ZeroMemory(szMozCtlXPath, sizeof(szMozCtlXPath));
	GetShortPathName(szMozCtlXLongPath, szMozCtlXPath, sizeof(szMozCtlXPath) * sizeof(_TCHAR));

	HKEY hkey = NULL;
	RegOpenKeyEx(HKEY_CLASSES_ROOT, c_szMozCtlInProcServerKey, 0, KEY_ALL_ACCESS, &hkey);
	if (hkey)
	{
		RegSetValueEx(hkey, NULL, 0, REG_SZ, (LPBYTE) szMozCtlXPath, lstrlen(szMozCtlXPath) * sizeof(_TCHAR));
		RegCloseKey(hkey);
		return TRUE;
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

typedef HRESULT (STDAPICALLTYPE *DllCanUnloadNowFn)(void);

STDAPI DllCanUnloadNow(void)
{
	if (LoadMozLibrary())
	{
		DllCanUnloadNowFn pfn = (DllCanUnloadNowFn) GetProcAddress(ghMozCtl, _T("DllCanUnloadNow"));
		if (pfn)
		{
			return pfn();
		}
	}
	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

typedef HRESULT (STDAPICALLTYPE *DllGetClassObjectFn)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	if (LoadMozLibrary())
	{
		DllGetClassObjectFn pfn = (DllGetClassObjectFn) GetProcAddress(ghMozCtl, _T("DllGetClassObject"));
		if (pfn)
		{
			return pfn(rclsid, riid, ppv);
		}
	}
	return E_FAIL;
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

typedef HRESULT (STDAPICALLTYPE *DllRegisterServerFn)(void);

STDAPI DllRegisterServer(void)
{
	if (LoadMozLibrary())
	{
		DllRegisterServerFn pfn = (DllRegisterServerFn) GetProcAddress(ghMozCtl, _T("DllRegisterServer"));
		if (pfn)
		{
			HRESULT hr = pfn();
			if (SUCCEEDED(hr))
			{
				FixupInProcRegistryEntry();
			}
			return hr;
		}
	}
	return E_FAIL;
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

typedef HRESULT (STDAPICALLTYPE *DllUnregisterServerFn)(void);

STDAPI DllUnregisterServer(void)
{
	if (LoadMozLibrary())
	{
		DllUnregisterServerFn pfn = (DllUnregisterServerFn) GetProcAddress(ghMozCtl, _T("DllUnregisterServer"));
		if (pfn)
		{
			return pfn();
		}
	}
	return E_FAIL;
}
