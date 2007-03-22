/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "shlobj.h"

static BOOL LoadMozLibraryFromCurrentDir();
static BOOL LoadMozLibraryFromRegistry(BOOL bAskUserToSetPath);
static BOOL LoadLibraryWithPATHFixup(const TCHAR *szBinDirPath);


static HMODULE ghMozCtlX = NULL;
static HMODULE ghMozCtl = NULL;

static LPCTSTR c_szMozCtlDll = _T("mozctl.dll");
static LPCTSTR c_szMozillaControlKey = _T("Software\\Mozilla\\");
static LPCTSTR c_szMozillaBinDirPathValue = _T("BinDirectoryPath");
static LPCTSTR c_szMozCtlInProcServerKey =
    _T("CLSID\\{1339B54C-3453-11D2-93B9-000000000000}\\InProcServer32");

// Message strings - should come from a resource file
static LPCTSTR c_szWarningTitle =
    _T("Mozilla Control Warning");
static LPCTSTR c_szErrorTitle =
    _T("Mozilla Control Error");
static LPCTSTR c_szPrePickFolderMsg =
    _T("The Mozilla control was unable to detect where your Mozilla "
       "layout libraries may be found. You will now be shown a directory "
       "picker for you to locate them.");
static LPCTSTR c_szPickFolderDlgTitle =
    _T("Pick where the Mozilla bin directory is located, "
       "e.g. (c:\\mozilla\\bin)");
static LPCTSTR c_szRegistryErrorMsg =
    _T("The Mozilla control was unable to store the location of the Mozilla"
       "layout libraries in the registry. This may be due to the host "
       "program running with insufficient permissions to write there.\n\n"
       "You should consider running the control once as Administrator "
       "to enable this entry to added or alternatively move the "
       "control files mozctlx.dll and mozctl.dll into the same folder as "
       "the other libraries.");
static LPCTSTR c_szLoadErrorMsg =
    _T("The Mozilla control could not be loaded correctly. This is could "
       "be due to an invalid location being specified for the Mozilla "
       "layout libraries or missing files from your installation.\n\n"
       "Visit http://www.iol.ie/~locka/mozilla/mozilla.htm for in-depth"
       "troubleshooting advice.");

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
        if (ghMozCtl)
        {
            FreeLibrary(ghMozCtl);
            ghMozCtl = NULL;
        }
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

    if (LoadMozLibraryFromCurrentDir())
    {
        return TRUE;
    }

    if (LoadMozLibraryFromRegistry(bAskUserToSetPath))
    {
        return TRUE;
    }

    ::MessageBox(NULL, c_szLoadErrorMsg, c_szErrorTitle, MB_OK);

    return FALSE;
}

static BOOL LoadMozLibraryFromCurrentDir()
{
    TCHAR szMozCtlXPath[MAX_PATH + 1];

    // Get this module's path
    ZeroMemory(szMozCtlXPath, sizeof(szMozCtlXPath));
    GetModuleFileName(ghMozCtlX, szMozCtlXPath,
        sizeof(szMozCtlXPath) / sizeof(szMozCtlXPath[0]));

    // Make the control path
    TCHAR szTmpDrive[_MAX_DRIVE];
    TCHAR szTmpDir[_MAX_DIR];
    TCHAR szTmpFname[_MAX_FNAME];
    TCHAR szTmpExt[_MAX_EXT];
    _tsplitpath(szMozCtlXPath, szTmpDrive, szTmpDir, szTmpFname, szTmpExt);

    TCHAR szMozCtlPath[MAX_PATH + 1];
    ZeroMemory(szMozCtlPath, sizeof(szMozCtlPath));
    _tmakepath(szMozCtlPath, szTmpDrive, szTmpDir, c_szMozCtlDll, NULL);

    // Stat to see if the control exists
    struct _stat dirStat;
    if (_tstat(szMozCtlPath, &dirStat) == 0)
    {
        TCHAR szBinDirPath[MAX_PATH + 1];
        ZeroMemory(szBinDirPath, sizeof(szBinDirPath));
        _tmakepath(szBinDirPath, szTmpDrive, szTmpDir, NULL, NULL);
        if (LoadLibraryWithPATHFixup(szBinDirPath))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL LoadMozLibraryFromRegistry(BOOL bAskUserToSetPath)
{
    // 
    HKEY hkey = NULL;
    
    TCHAR szBinDirPath[MAX_PATH + 1];
    DWORD dwBufSize = sizeof(szBinDirPath);

    ZeroMemory(szBinDirPath, dwBufSize);

    // First try and read the path from the registry
    RegOpenKeyEx(HKEY_LOCAL_MACHINE, c_szMozillaControlKey, 0, KEY_READ, &hkey);
    if (hkey)
    {
        DWORD dwType = REG_SZ;
        RegQueryValueEx(hkey, c_szMozillaBinDirPathValue, NULL, &dwType,
            (LPBYTE) szBinDirPath, &dwBufSize);
        RegCloseKey(hkey);
        hkey = NULL;

        if (lstrlen(szBinDirPath) > 0 && LoadLibraryWithPATHFixup(szBinDirPath))
        {
            return TRUE;
        }

        // NOTE: We will drop through here if the registry key existed
        //       but didn't lead to the control being loaded.
    }
    
    // Ask the user to pick to pick the path?
    if (bAskUserToSetPath)
    {
        // Tell the user that they have to pick the bin dir path
        ::MessageBox(NULL, c_szPrePickFolderMsg, c_szWarningTitle, MB_OK);

        // Show a folder picker for the user to choose the bin directory
        BROWSEINFO bi;
        ZeroMemory(&bi, sizeof(bi));
        bi.hwndOwner = NULL;
        bi.pidlRoot = NULL;
        bi.pszDisplayName = szBinDirPath;
        bi.lpszTitle = c_szPickFolderDlgTitle;
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
                    // Chop off the end path separator
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
                    MultiByteToWideChar(CP_ACP, 0, szPath, szBinDirPath, -1,
                        sizeof(szBinDirPath) / sizeof(TCHAR));
#else
                    lstrcpy(szBinDirPath, szPath);
#endif
                }
                pShellAllocator->Free(pItemList);
                pShellAllocator->Release();
            }
        }
        
        // Store the new path if we can
        if (lstrlen(szBinDirPath) > 0)
        {
            RegCreateKeyEx(HKEY_LOCAL_MACHINE, c_szMozillaControlKey, 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL);
            if (hkey)
            {
                RegSetValueEx(hkey, c_szMozillaBinDirPathValue, 0, REG_SZ,
                    (LPBYTE) szBinDirPath, lstrlen(szBinDirPath) * sizeof(TCHAR));
                RegCloseKey(hkey);
            }
            else
            {
                // Tell the user the key can't be stored but the path they 
                // chose will be used for this session
                ::MessageBox(NULL, c_szRegistryErrorMsg, c_szErrorTitle, MB_OK);
            }
        }
    }

    if (lstrlen(szBinDirPath) > 0 && LoadLibraryWithPATHFixup(szBinDirPath))
    {
        return TRUE;
    }

    return FALSE;
}

static BOOL LoadLibraryWithPATHFixup(const TCHAR *szBinDirPath)
{
    TCHAR szOldPath[8192];
    TCHAR szNewPath[8192];
    
    ZeroMemory(szOldPath, sizeof(szOldPath));
    ZeroMemory(szNewPath, sizeof(szNewPath));

    // Append path to front of PATH
    GetEnvironmentVariable("PATH", szOldPath, sizeof(szOldPath) / sizeof(TCHAR));
    lstrcat(szNewPath, szBinDirPath);
    lstrcat(szNewPath, _T(";"));
    lstrcat(szNewPath, szOldPath);
    SetEnvironmentVariable("PATH", szNewPath);
    
    // Attempt load
    ghMozCtl = LoadLibrary(c_szMozCtlDll);
    if (ghMozCtl)
    {
        return TRUE;
    }

    // Restore old PATH
    SetEnvironmentVariable("PATH", szOldPath);

    return FALSE;
}


static BOOL FixupInProcRegistryEntry()
{
    TCHAR szMozCtlXLongPath[MAX_PATH+1];
    ZeroMemory(szMozCtlXLongPath, sizeof(szMozCtlXLongPath));
    GetModuleFileName(ghMozCtlX, szMozCtlXLongPath,
        sizeof(szMozCtlXLongPath) * sizeof(TCHAR));

    TCHAR szMozCtlXPath[MAX_PATH+1];
    ZeroMemory(szMozCtlXPath, sizeof(szMozCtlXPath));
    GetShortPathName(szMozCtlXLongPath, szMozCtlXPath,
        sizeof(szMozCtlXPath) * sizeof(TCHAR));

    HKEY hkey = NULL;
    RegOpenKeyEx(HKEY_CLASSES_ROOT, c_szMozCtlInProcServerKey, 0, KEY_ALL_ACCESS, &hkey);
    if (hkey)
    {
        RegSetValueEx(hkey, NULL, 0, REG_SZ, (LPBYTE) szMozCtlXPath,
            lstrlen(szMozCtlXPath) * sizeof(TCHAR));
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
        DllCanUnloadNowFn pfn = (DllCanUnloadNowFn)
            GetProcAddress(ghMozCtl, _T("DllCanUnloadNow"));
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
        DllGetClassObjectFn pfn = (DllGetClassObjectFn) 
            GetProcAddress(ghMozCtl, _T("DllGetClassObject"));
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
        DllRegisterServerFn pfn = (DllRegisterServerFn) 
            GetProcAddress(ghMozCtl, _T("DllRegisterServer"));
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
        DllUnregisterServerFn pfn = (DllUnregisterServerFn)
            GetProcAddress(ghMozCtl, _T("DllUnregisterServer"));
        if (pfn)
        {
            return pfn();
        }
    }
    return E_FAIL;
}
