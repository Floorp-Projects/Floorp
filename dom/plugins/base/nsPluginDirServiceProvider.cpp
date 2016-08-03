/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginDirServiceProvider.h"

#include "nsCRT.h"
#include "nsIFile.h"
#include "nsDependentString.h"
#include "nsArrayEnumerator.h"
#include "mozilla/Preferences.h"

#include <windows.h>
#include "nsIWindowsRegKey.h"

using namespace mozilla;

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
  WCHAR* szNum1 = nullptr;
  WCHAR* szNum2 = nullptr;
  WCHAR* szNum3 = nullptr;
  WCHAR* szNum4 = nullptr;
  WCHAR* szJavaBuild = nullptr;

  WCHAR *strVer = nullptr;
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

#if defined(__MINGW32__)
  // MSVC 2013 and earlier provided only a non-standard two-argument variant of
  // wcstok that is generally not thread-safe. For our purposes here, it works
  // fine, though.
  auto wcstok = [](wchar_t* strToken, const wchar_t* strDelimit,
                   wchar_t** /*ctx*/) {
    return ::std::wcstok(strToken, strDelimit);
  };
#endif

  wchar_t* ctx = nullptr;
  szNum1 = wcstok(strVer,  L".", &ctx);
  szNum2 = wcstok(nullptr, L".", &ctx);
  szNum3 = wcstok(nullptr, L".", &ctx);
  szNum4 = wcstok(nullptr, L".", &ctx);

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

NS_IMPL_ISUPPORTS(nsPluginDirServiceProvider,
                  nsIDirectoryServiceProvider)

//*****************************************************************************
// nsPluginDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsPluginDirServiceProvider::GetFile(const char *charProp, bool *persistant,
                                    nsIFile **_retval)
{
  nsCOMPtr<nsIFile>  localFile;
  nsresult rv = NS_ERROR_FAILURE;

  NS_ENSURE_ARG(charProp);

  *_retval = nullptr;
  *persistant = false;

  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_ENSURE_TRUE(regKey, NS_ERROR_FAILURE);

  if (nsCRT::strcmp(charProp, NS_WIN_QUICKTIME_SCAN_KEY) == 0) {
    nsAdoptingCString strVer = Preferences::GetCString(charProp);
    if (!strVer) {
      return NS_ERROR_FAILURE;
    }
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
        rv = NS_NewLocalFile(path, true,
                             getter_AddRefs(localFile));
      }
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_WMP_SCAN_KEY) == 0) {
    nsAdoptingCString strVer = Preferences::GetCString(charProp);
    if (!strVer) {
      return NS_ERROR_FAILURE;
    }
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
        rv = NS_NewLocalFile(path, true,
                             getter_AddRefs(localFile));
      }
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_ACROBAT_SCAN_KEY) == 0) {
    nsAdoptingCString strVer = Preferences::GetCString(charProp);
    if (!strVer) {
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
    uint32_t childCount = 0;
    regKey->GetChildCount(&childCount);

    for (uint32_t index = 0; index < childCount; ++index) {
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
      rv = NS_NewLocalFile(newestPath, true,
                           getter_AddRefs(localFile));
    }
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  localFile.forget(_retval);
  return NS_OK;
}

nsresult
nsPluginDirServiceProvider::GetPLIDDirectories(nsISimpleEnumerator **aEnumerator)
{
  NS_ENSURE_ARG_POINTER(aEnumerator);
  *aEnumerator = nullptr;

  nsCOMArray<nsIFile> dirs;

  GetPLIDDirectoriesWithRootKey(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, dirs);
  GetPLIDDirectoriesWithRootKey(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, dirs);

  return NS_NewArrayEnumerator(aEnumerator, dirs);
}

nsresult
nsPluginDirServiceProvider::GetPLIDDirectoriesWithRootKey(uint32_t aKey, nsCOMArray<nsIFile> &aDirs)
{
  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  NS_ENSURE_TRUE(regKey, NS_ERROR_FAILURE);

  nsresult rv = regKey->Open(aKey,
                             NS_LITERAL_STRING("Software\\MozillaPlugins"),
                             nsIWindowsRegKey::ACCESS_READ);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t childCount = 0;
  regKey->GetChildCount(&childCount);

  for (uint32_t index = 0; index < childCount; ++index) {
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
          nsCOMPtr<nsIFile> localFile;
          if (NS_SUCCEEDED(NS_NewLocalFile(path, true,
                                           getter_AddRefs(localFile))) &&
              localFile) {
            // Some vendors use a path directly to the DLL so chop off
            // the filename
            bool isDir = false;
            if (NS_SUCCEEDED(localFile->IsDirectory(&isDir)) && !isDir) {
              nsCOMPtr<nsIFile> temp;
              localFile->GetParent(getter_AddRefs(temp));
              if (temp)
                localFile = temp;
            }

            // Now we check to make sure it's actually on disk and
            // To see if we already have this directory in the array
            bool isFileThere = false;
            bool isDupEntry = false;
            if (NS_SUCCEEDED(localFile->Exists(&isFileThere)) && isFileThere) {
              int32_t c = aDirs.Count();
              for (int32_t i = 0; i < c; i++) {
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
