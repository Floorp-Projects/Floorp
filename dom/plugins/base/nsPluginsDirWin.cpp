/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
  nsPluginsDirWin.cpp
  
  Windows implementation of the nsPluginsDir/nsPluginsFile classes.
  
  by Alex Musil
 */

#include "nsPluginsDir.h"
#include "prlink.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#include "windows.h"
#include "winbase.h"

#include "nsString.h"
#include "nsILocalFile.h"
#include "nsUnicharUtils.h"
#include "nsSetDllDirectory.h"

/* Local helper functions */

static char* GetKeyValue(void* verbuf, const WCHAR* key,
                         UINT language, UINT codepage)
{
  WCHAR keybuf[64]; // plenty for the template below, with the longest key
                    // we use (currently "FileDescription")
  const WCHAR keyFormat[] = L"\\StringFileInfo\\%04X%04X\\%s";
  WCHAR *buf = NULL;
  UINT blen;

  if (_snwprintf_s(keybuf, NS_ARRAY_LENGTH(keybuf), _TRUNCATE,
                   keyFormat, language, codepage, key) < 0)
  {
    NS_NOTREACHED("plugin info key too long for buffer!");
    return nsnull;
  }

  if (::VerQueryValueW(verbuf, keybuf, (void **)&buf, &blen) == 0 ||
      buf == nsnull || blen == 0)
  {
    return nsnull;
  }

  return PL_strdup(NS_ConvertUTF16toUTF8(buf, blen).get());
}

static char* GetVersion(void* verbuf)
{
  VS_FIXEDFILEINFO *fileInfo;
  UINT fileInfoLen;

  ::VerQueryValueW(verbuf, L"\\", (void **)&fileInfo, &fileInfoLen);

  if (fileInfo) {
    return PR_smprintf("%ld.%ld.%ld.%ld",
                       HIWORD(fileInfo->dwFileVersionMS),
                       LOWORD(fileInfo->dwFileVersionMS),
                       HIWORD(fileInfo->dwFileVersionLS),
                       LOWORD(fileInfo->dwFileVersionLS));
  }

  return nsnull;
}

static PRUint32 CalculateVariantCount(char* mimeTypes)
{
  PRUint32 variants = 1;

  if (!mimeTypes)
    return 0;

  char* index = mimeTypes;
  while (*index) {
    if (*index == '|')
      variants++;

    ++index;
  }
  return variants;
}

static char** MakeStringArray(PRUint32 variants, char* data)
{
  // The number of variants has been calculated based on the mime
  // type array. Plugins are not explicitely required to match
  // this number in two other arrays: file extention array and mime
  // description array, and some of them actually don't. 
  // We should handle such situations gracefully

  if ((variants <= 0) || !data)
    return NULL;

  char ** array = (char **)PR_Calloc(variants, sizeof(char *));
  if (!array)
    return NULL;

  char * start = data;

  for (PRUint32 i = 0; i < variants; i++) {
    char * p = PL_strchr(start, '|');
    if (p)
      *p = 0;

    array[i] = PL_strdup(start);

    if (!p) {
      // nothing more to look for, fill everything left 
      // with empty strings and break
      while(++i < variants)
        array[i] = PL_strdup("");

      break;
    }

    start = ++p;
  }
  return array;
}

static void FreeStringArray(PRUint32 variants, char ** array)
{
  if ((variants == 0) || !array)
    return;

  for (PRUint32 i = 0; i < variants; i++) {
    if (array[i]) {
      PL_strfree(array[i]);
      array[i] = NULL;
    }
  }
  PR_Free(array);
}

static bool CanLoadPlugin(const PRUnichar* aBinaryPath)
{
#if defined(_M_IX86) || defined(_M_X64) || defined(_M_IA64)
  bool canLoad = false;

  HANDLE file = CreateFileW(aBinaryPath, GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file != INVALID_HANDLE_VALUE) {
    HANDLE map = CreateFileMappingW(file, NULL, PAGE_READONLY, 0,
                                    GetFileSize(file, NULL), NULL);
    if (map != NULL) {
      LPVOID mapView = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
      if (mapView != NULL) {
        if (((IMAGE_DOS_HEADER*)mapView)->e_magic == IMAGE_DOS_SIGNATURE) {
          long peImageHeaderStart = (((IMAGE_DOS_HEADER*)mapView)->e_lfanew);
          if (peImageHeaderStart != 0L) {
            DWORD arch = (((IMAGE_NT_HEADERS*)((LPBYTE)mapView + peImageHeaderStart))->FileHeader.Machine);
#ifdef _M_IX86
            canLoad = (arch == IMAGE_FILE_MACHINE_I386);
#elif defined(_M_X64)
            canLoad = (arch == IMAGE_FILE_MACHINE_AMD64);
#elif defined(_M_IA64)
            canLoad = (arch == IMAGE_FILE_MACHINE_IA64);
#endif
          }
        }
        UnmapViewOfFile(mapView);
      }
      CloseHandle(map);
    }
    CloseHandle(file);
  }

  return canLoad;
#else
  // Assume correct binaries for unhandled cases.
  return true;
#endif
}

/* nsPluginsDir implementation */

// The file name must be in the form "np*.dll"
bool nsPluginsDir::IsPluginFile(nsIFile* file)
{
  nsCAutoString path;
  if (NS_FAILED(file->GetNativePath(path)))
    return false;

  const char *cPath = path.get();

  // this is most likely a path, so skip to the filename
  const char* filename = PL_strrchr(cPath, '\\');
  if (filename)
    ++filename;
  else
    filename = cPath;

  char* extension = PL_strrchr(filename, '.');
  if (extension)
    ++extension;

  PRUint32 fullLength = PL_strlen(filename);
  PRUint32 extLength = PL_strlen(extension);
  if (fullLength >= 7 && extLength == 3) {
    if (!PL_strncasecmp(filename, "np", 2) && !PL_strncasecmp(extension, "dll", 3)) {
      // don't load OJI-based Java plugins
      if (!PL_strncasecmp(filename, "npoji", 5) ||
          !PL_strncasecmp(filename, "npjava", 6))
        return false;
      return true;
    }
  }

  return false;
}

/* nsPluginFile implementation */

nsPluginFile::nsPluginFile(nsIFile* file)
: mPlugin(file)
{
  // nada
}

nsPluginFile::~nsPluginFile()
{
  // nada
}

/**
 * Loads the plugin into memory using NSPR's shared-library loading
 * mechanism. Handles platform differences in loading shared libraries.
 */
nsresult nsPluginFile::LoadPlugin(PRLibrary **outLibrary)
{
  nsCOMPtr<nsILocalFile> plugin = do_QueryInterface(mPlugin);

  if (!plugin)
    return NS_ERROR_NULL_POINTER;

  bool protectCurrentDirectory = true;

  nsAutoString pluginFolderPath;
  plugin->GetPath(pluginFolderPath);

  PRInt32 idx = pluginFolderPath.RFindChar('\\');
  if (kNotFound == idx)
    return NS_ERROR_FILE_INVALID_PATH;

  if (Substring(pluginFolderPath, idx).LowerCaseEqualsLiteral("\\np32dsw.dll")) {
    protectCurrentDirectory = false;
  }

  pluginFolderPath.SetLength(idx);

  BOOL restoreOrigDir = FALSE;
  WCHAR aOrigDir[MAX_PATH + 1];
  DWORD dwCheck = GetCurrentDirectoryW(MAX_PATH, aOrigDir);
  NS_ASSERTION(dwCheck <= MAX_PATH + 1, "Error in Loading plugin");

  if (dwCheck <= MAX_PATH + 1) {
    restoreOrigDir = SetCurrentDirectoryW(pluginFolderPath.get());
    NS_ASSERTION(restoreOrigDir, "Error in Loading plugin");
  }

  if (protectCurrentDirectory) {
    mozilla::NS_SetDllDirectory(NULL);
  }

  nsresult rv = plugin->Load(outLibrary);
  if (NS_FAILED(rv))
      *outLibrary = NULL;

  if (protectCurrentDirectory) {
    mozilla::NS_SetDllDirectory(L"");
  }

  if (restoreOrigDir) {
    BOOL bCheck = SetCurrentDirectoryW(aOrigDir);
    NS_ASSERTION(bCheck, "Error in Loading plugin");
  }

  return rv;
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info, PRLibrary **outLibrary)
{
  *outLibrary = nsnull;

  nsresult rv = NS_OK;
  DWORD zerome, versionsize;
  void* verbuf = nsnull;

  if (!mPlugin)
    return NS_ERROR_NULL_POINTER;

  nsAutoString fullPath;
  if (NS_FAILED(rv = mPlugin->GetPath(fullPath)))
    return rv;

  if (!CanLoadPlugin(fullPath.get()))
    return NS_ERROR_FAILURE;

  nsAutoString fileName;
  if (NS_FAILED(rv = mPlugin->GetLeafName(fileName)))
    return rv;

  LPCWSTR lpFilepath = fullPath.get();

  versionsize = ::GetFileVersionInfoSizeW(lpFilepath, &zerome);

  if (versionsize > 0)
    verbuf = PR_Malloc(versionsize);
  if (!verbuf)
    return NS_ERROR_OUT_OF_MEMORY;

  if (::GetFileVersionInfoW(lpFilepath, NULL, versionsize, verbuf))
  {
    // TODO: get appropriately-localized info from plugin file
    UINT lang = 1033; // language = English
    UINT cp = 1252;   // codepage = Western
    info.fName = GetKeyValue(verbuf, L"ProductName", lang, cp);
    info.fDescription = GetKeyValue(verbuf, L"FileDescription", lang, cp);
 
    char *mimeType = GetKeyValue(verbuf, L"MIMEType", lang, cp);
    char *mimeDescription = GetKeyValue(verbuf, L"FileOpenName", lang, cp);
    char *extensions = GetKeyValue(verbuf, L"FileExtents", lang, cp);

    info.fVariantCount = CalculateVariantCount(mimeType);
    info.fMimeTypeArray = MakeStringArray(info.fVariantCount, mimeType);
    info.fMimeDescriptionArray = MakeStringArray(info.fVariantCount, mimeDescription);
    info.fExtensionArray = MakeStringArray(info.fVariantCount, extensions);
    info.fFullPath = PL_strdup(NS_ConvertUTF16toUTF8(fullPath).get());
    info.fFileName = PL_strdup(NS_ConvertUTF16toUTF8(fileName).get());
    info.fVersion = GetVersion(verbuf);

    PL_strfree(mimeType);
    PL_strfree(mimeDescription);
    PL_strfree(extensions);
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

  PR_Free(verbuf);

  return rv;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  if (info.fName)
    PL_strfree(info.fName);

  if (info.fDescription)
    PL_strfree(info.fDescription);

  if (info.fMimeTypeArray)
    FreeStringArray(info.fVariantCount, info.fMimeTypeArray);

  if (info.fMimeDescriptionArray)
    FreeStringArray(info.fVariantCount, info.fMimeDescriptionArray);

  if (info.fExtensionArray)
    FreeStringArray(info.fVariantCount, info.fExtensionArray);

  if (info.fFullPath)
    PL_strfree(info.fFullPath);

  if (info.fFileName)
    PL_strfree(info.fFileName);

  if (info.fVersion)
    PR_smprintf_free(info.fVersion);

  ZeroMemory((void *)&info, sizeof(info));

  return NS_OK;
}
