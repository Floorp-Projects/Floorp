/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *  Ere Maijala <emaijala@kolumbus.fi>
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

#include "nsPluginDirServiceProvider.h"

#include "nsCRT.h"
#include "nsILocalFile.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsDependentString.h"
#include "nsXPIDLString.h"
#include "prmem.h"
#include "nsArrayEnumerator.h"

#include <windows.h>
#include "nsIWindowsRegKey.h"

typedef struct structVer
{
  WORD wMajor;
  WORD wMinor;
  WORD wRelease;
  WORD wBuild;
} verBlock;

static void
ClearVersion(verBlock *ver)
{
  ver->wMajor   = 0;
  ver->wMinor   = 0;
  ver->wRelease = 0;
  ver->wBuild   = 0;
}

static BOOL
FileExists(LPCWSTR szFile)
{
  return GetFileAttributesW(szFile) != 0xFFFFFFFF;
}

// Get file version information from a file
static BOOL
GetFileVersion(LPCWSTR szFile, verBlock *vbVersion)
{
  UINT              uLen;
  UINT              dwLen;
  BOOL              bRv;
  DWORD             dwHandle;
  LPVOID            lpData;
  LPVOID            lpBuffer;
  VS_FIXEDFILEINFO  *lpBuffer2;

  ClearVersion(vbVersion);
  if (FileExists(szFile)) {
    bRv    = TRUE;
    LPCWSTR lpFilepath = szFile;
    dwLen  = GetFileVersionInfoSizeW(lpFilepath, &dwHandle);
    lpData = (LPVOID)malloc(dwLen);
    uLen   = 0;

    if (lpData && GetFileVersionInfoW(lpFilepath, dwHandle, dwLen, lpData) != 0) {
      if (VerQueryValueW(lpData, L"\\", &lpBuffer, &uLen) != 0) {
        lpBuffer2 = (VS_FIXEDFILEINFO *)lpBuffer;

        vbVersion->wMajor   = HIWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->wMinor   = LOWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->wRelease = HIWORD(lpBuffer2->dwFileVersionLS);
        vbVersion->wBuild   = LOWORD(lpBuffer2->dwFileVersionLS);
      }
    }

    free(lpData);
  } else {
    /* File does not exist */
    bRv = FALSE;
  }

  return bRv;
}

// Will deep copy ver2 into ver1
static void
CopyVersion(verBlock *ver1, verBlock *ver2)
{
  ver1->wMajor   = ver2->wMajor;
  ver1->wMinor   = ver2->wMinor;
  ver1->wRelease = ver2->wRelease;
  ver1->wBuild   = ver2->wBuild;
}

// Convert a string version to a version struct
static void
TranslateVersionStr(const WCHAR* szVersion, verBlock *vbVersion)
{
  WCHAR* szNum1 = NULL;
  WCHAR* szNum2 = NULL;
  WCHAR* szNum3 = NULL;
  WCHAR* szNum4 = NULL;
  WCHAR* szJavaBuild = NULL;

  WCHAR *strVer = nsnull;
  if (szVersion) {
    strVer = wcsdup(szVersion);
  }

  if (!strVer) {
    // Out of memory
    ClearVersion(vbVersion);
    return;
  }

  // Java may be using an underscore instead of a dot for the build ID
  szJavaBuild = wcschr(strVer, '_');
  if (szJavaBuild) {
    szJavaBuild[0] = '.';
  }

  szNum1 = wcstok(strVer, L".");
  szNum2 = wcstok(NULL,   L".");
  szNum3 = wcstok(NULL,   L".");
  szNum4 = wcstok(NULL,   L".");

  vbVersion->wMajor   = szNum1 ? (WORD) _wtoi(szNum1) : 0;
  vbVersion->wMinor   = szNum2 ? (WORD) _wtoi(szNum2) : 0;
  vbVersion->wRelease = szNum3 ? (WORD) _wtoi(szNum3) : 0;
  vbVersion->wBuild   = szNum4 ? (WORD) _wtoi(szNum4) : 0;

  free(strVer);
}

// Compare two version struct, return zero if the same
static int
CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew)
{
  if (vbVersionOld.wMajor > vbVersionNew.wMajor) {
    return 4;
  } else if (vbVersionOld.wMajor < vbVersionNew.wMajor) {
    return -4;
  }

  if (vbVersionOld.wMinor > vbVersionNew.wMinor) {
    return 3;
  } else if (vbVersionOld.wMinor < vbVersionNew.wMinor) {
    return -3;
  }

  if (vbVersionOld.wRelease > vbVersionNew.wRelease) {
    return 2;
  } else if (vbVersionOld.wRelease < vbVersionNew.wRelease) {
    return -2;
  }

  if (vbVersionOld.wBuild > vbVersionNew.wBuild) {
    return 1;
  } else if (vbVersionOld.wBuild < vbVersionNew.wBuild) {
    return -1;
  }

  /* the versions are all the same */
  return 0;
}

//*****************************************************************************
// nsPluginDirServiceProvider::Constructor/Destructor
//*****************************************************************************

nsPluginDirServiceProvider::nsPluginDirServiceProvider()
{
}

nsPluginDirServiceProvider::~nsPluginDirServiceProvider()
{
}

//*****************************************************************************
// nsPluginDirServiceProvider::nsISupports
//*****************************************************************************

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPluginDirServiceProvider,
                              nsIDirectoryServiceProvider)

//*****************************************************************************
// nsPluginDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsPluginDirServiceProvider::GetFile(const char *charProp, bool *persistant,
                                    nsIFile **_retval)
{
  nsCOMPtr<nsILocalFile>  localFile;
  nsresult rv = NS_ERROR_FAILURE;

  NS_ENSURE_ARG(charProp);

  *_retval = nsnull;
  *persistant = PR_FALSE;

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefs)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_ENSURE_TRUE(regKey, NS_ERROR_FAILURE);

  if (nsCRT::strcmp(charProp, NS_WIN_JRE_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(charProp, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(NS_ConvertASCIItoUTF16(strVer).get(), &minVer);

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                      NS_LITERAL_STRING("Software\\JavaSoft\\Java Runtime Environment"),
                      nsIWindowsRegKey::ACCESS_READ);
    NS_ENSURE_SUCCESS(rv, rv);

    verBlock maxVer;
    ClearVersion(&maxVer);

    nsAutoString newestPath;
    nsAutoString browserJavaVersion;
    regKey->ReadStringValue(NS_LITERAL_STRING("BrowserJavaVersion"),
                            browserJavaVersion);

    PRUint32 childCount = 0;
    regKey->GetChildCount(&childCount);

    // We must enumerate through the keys because what if there is
    // more than one version?
    for (PRUint32 index = 0; index < childCount; ++index) {
      nsAutoString childName;
      rv = regKey->GetChildName(index, childName);
      if (NS_SUCCEEDED(rv)) {
        // Skip major.minor as it always points to latest in its family
        PRUint32 numChars = 0;
        PRInt32 offset = 0;
        while ((offset = childName.FindChar(L'.', offset + 1)) >= 0) {
          ++numChars;
        }
        if (numChars < 2)
          continue;

        nsCOMPtr<nsIWindowsRegKey> childKey;
        rv = regKey->OpenChild(childName, nsIWindowsRegKey::ACCESS_QUERY_VALUE,
                               getter_AddRefs(childKey));
        if (NS_SUCCEEDED(rv) && childKey) {
          nsAutoString path;
          rv = childKey->ReadStringValue(NS_LITERAL_STRING("JavaHome"), path);
          if (NS_SUCCEEDED(rv)) {
            verBlock curVer;
            TranslateVersionStr(childName.get(), &curVer);
            if (CompareVersion(curVer, minVer) >= 0) {
              if (browserJavaVersion == childName) {
                newestPath = path;
                break;
              }

              if (CompareVersion(curVer, maxVer) >= 0) {
                newestPath = path;
                CopyVersion(&maxVer, &curVer);
              }
            }
          }
        }
      }
    }

    if (newestPath.IsEmpty()) {
      return NS_ERROR_FAILURE;
    }

    // We require the newer NPAPI Java plugin.
    newestPath += NS_LITERAL_STRING("\\bin\\new_plugin");

    rv = NS_NewLocalFile(newestPath,
                         PR_TRUE, getter_AddRefs(localFile));

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIWindowsRegKey> newKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1");
      NS_ENSURE_TRUE(newKey, NS_ERROR_FAILURE);

      rv = newKey->Create(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                          NS_LITERAL_STRING("Software\\mozilla.org\\Mozilla"),
                          nsIWindowsRegKey::ACCESS_QUERY_VALUE |
                          nsIWindowsRegKey::ACCESS_SET_VALUE);
      if (NS_SUCCEEDED(rv)) {
        bool currentVersionExists = false;
        newKey->HasValue(NS_LITERAL_STRING("CurrentVersion"),
                         &currentVersionExists);
        if (!currentVersionExists) {
          newKey->WriteStringValue(NS_LITERAL_STRING("CurrentVersion"),
                                   NS_LITERAL_STRING(MOZILLA_VERSION));
        }
      }
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_QUICKTIME_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(charProp, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(NS_ConvertASCIItoUTF16(strVer).get(), &minVer);

    // Look for the Quicktime system installation plugins directory
    verBlock qtVer;
    ClearVersion(&qtVer);

    // First we need to check the version of Quicktime via checking
    // the EXE's version table
    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                      NS_LITERAL_STRING("software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\QuickTimePlayer.exe"),
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString path;
      rv = regKey->ReadStringValue(NS_LITERAL_STRING(""), path);
      if (NS_SUCCEEDED(rv)) {
        GetFileVersion(path.get(), &qtVer);
      }
      regKey->Close();
    }
    if (CompareVersion(qtVer, minVer) < 0)
      return rv;

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                      NS_LITERAL_STRING("software\\Apple Computer, Inc.\\QuickTime"),
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString path;
      rv = regKey->ReadStringValue(NS_LITERAL_STRING("InstallDir"), path);
      if (NS_SUCCEEDED(rv)) {
        path += NS_LITERAL_STRING("\\Plugins");
        rv = NS_NewLocalFile(path, PR_TRUE,
                             getter_AddRefs(localFile));
      }
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_WMP_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(charProp, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(NS_ConvertASCIItoUTF16(strVer).get(), &minVer);

    // Look for Windows Media Player system installation plugins directory
    verBlock wmpVer;
    ClearVersion(&wmpVer);

    // First we need to check the version of WMP
    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                      NS_LITERAL_STRING("software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\wmplayer.exe"),
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString path;
      rv = regKey->ReadStringValue(NS_LITERAL_STRING(""), path);
      if (NS_SUCCEEDED(rv)) {
        GetFileVersion(path.get(), &wmpVer);
      }
      regKey->Close();
    }
    if (CompareVersion(wmpVer, minVer) < 0)
      return rv;

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                      NS_LITERAL_STRING("software\\Microsoft\\MediaPlayer"),
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString path;
      rv = regKey->ReadStringValue(NS_LITERAL_STRING("Installation Directory"),
                                   path);
      if (NS_SUCCEEDED(rv)) {
        rv = NS_NewLocalFile(path, PR_TRUE,
                             getter_AddRefs(localFile));
      }
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_ACROBAT_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(charProp, getter_Copies(strVer)))) {
      return NS_ERROR_FAILURE;
    }

    verBlock minVer;
    TranslateVersionStr(NS_ConvertASCIItoUTF16(strVer).get(), &minVer);

    // Look for Adobe Acrobat system installation plugins directory
    verBlock maxVer;
    ClearVersion(&maxVer);

    nsAutoString newestPath;

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                      NS_LITERAL_STRING("software\\Adobe\\Acrobat Reader"),
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_FAILED(rv)) {
      rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                        NS_LITERAL_STRING("software\\Adobe\\Adobe Acrobat"),
                        nsIWindowsRegKey::ACCESS_READ);
      if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
      }
    }

    // We must enumerate through the keys because what if there is
    // more than one version?
    PRUint32 childCount = 0;
    regKey->GetChildCount(&childCount);

    for (PRUint32 index = 0; index < childCount; ++index) {
      nsAutoString childName;
      rv = regKey->GetChildName(index, childName);
      if (NS_SUCCEEDED(rv)) {
        verBlock curVer;
        TranslateVersionStr(childName.get(), &curVer);

        childName += NS_LITERAL_STRING("\\InstallPath");

        nsCOMPtr<nsIWindowsRegKey> childKey;
        rv = regKey->OpenChild(childName, nsIWindowsRegKey::ACCESS_QUERY_VALUE,
                               getter_AddRefs(childKey));
        if (NS_SUCCEEDED(rv)) {
          // We have a sub key
          nsAutoString path;
          rv = childKey->ReadStringValue(NS_LITERAL_STRING(""), path);
          if (NS_SUCCEEDED(rv)) {
            if (CompareVersion(curVer, maxVer) >= 0 &&
                CompareVersion(curVer, minVer) >= 0) {
              newestPath = path;
              CopyVersion(&maxVer, &curVer);
            }
          }
        }
      }
    }

    if (!newestPath.IsEmpty()) {
      newestPath += NS_LITERAL_STRING("\\browser");
      rv = NS_NewLocalFile(newestPath, PR_TRUE,
                           getter_AddRefs(localFile));
    }
  }

  if (localFile && NS_SUCCEEDED(rv))
    return CallQueryInterface(localFile, _retval);

  return rv;
}

nsresult
nsPluginDirServiceProvider::GetPLIDDirectories(nsISimpleEnumerator **aEnumerator)
{
  NS_ENSURE_ARG_POINTER(aEnumerator);
  *aEnumerator = nsnull;

  nsCOMArray<nsILocalFile> dirs;

  GetPLIDDirectoriesWithRootKey(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, dirs);
  GetPLIDDirectoriesWithRootKey(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, dirs);

  return NS_NewArrayEnumerator(aEnumerator, dirs);
}

nsresult
nsPluginDirServiceProvider::GetPLIDDirectoriesWithRootKey(PRUint32 aKey, nsCOMArray<nsILocalFile> &aDirs)
{
  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_ENSURE_TRUE(regKey, NS_ERROR_FAILURE);

  nsresult rv = regKey->Open(aKey,
                             NS_LITERAL_STRING("Software\\MozillaPlugins"),
                             nsIWindowsRegKey::ACCESS_READ);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = 0;
  regKey->GetChildCount(&childCount);

  for (PRUint32 index = 0; index < childCount; ++index) {
    nsAutoString childName;
    rv = regKey->GetChildName(index, childName);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIWindowsRegKey> childKey;
      rv = regKey->OpenChild(childName, nsIWindowsRegKey::ACCESS_QUERY_VALUE,
                             getter_AddRefs(childKey));
      if (NS_SUCCEEDED(rv) && childKey) {
        nsAutoString path;
        rv = childKey->ReadStringValue(NS_LITERAL_STRING("Path"), path);
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsILocalFile> localFile;
          if (NS_SUCCEEDED(NS_NewLocalFile(path, PR_TRUE,
                                           getter_AddRefs(localFile))) &&
              localFile) {
            // Some vendors use a path directly to the DLL so chop off
            // the filename
            bool isDir = false;
            if (NS_SUCCEEDED(localFile->IsDirectory(&isDir)) && !isDir) {
              nsCOMPtr<nsIFile> temp;
              localFile->GetParent(getter_AddRefs(temp));
              if (temp)
                localFile = do_QueryInterface(temp);
            }

            // Now we check to make sure it's actually on disk and
            // To see if we already have this directory in the array
            bool isFileThere = false;
            bool isDupEntry = false;
            if (NS_SUCCEEDED(localFile->Exists(&isFileThere)) && isFileThere) {
              PRInt32 c = aDirs.Count();
              for (PRInt32 i = 0; i < c; i++) {
                nsIFile *dup = static_cast<nsIFile*>(aDirs[i]);
                if (dup &&
                    NS_SUCCEEDED(dup->Equals(localFile, &isDupEntry)) &&
                    isDupEntry) {
                  break;
                }
              }

              if (!isDupEntry) {
                aDirs.AppendObject(localFile);
              }
            }
          }
        }
      }
    }
  }
  return NS_OK;
}

