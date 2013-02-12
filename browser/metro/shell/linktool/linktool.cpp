/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x602
#define _WIN32_WINNT 0x602

#include <windows.h>
#include <shobjidl.h>
#include <objbase.h>
#include <shlguid.h>
#include <shlobj.h>
#define INITGUID
#include <propvarutil.h>
#include <propkey.h>
#include <stdio.h>

//  Indicates that an application supports dual desktop and immersive modes. In Windows 8, this property is only applicable for web browsers.
//DEFINE_PROPERTYKEY(PKEY_AppUserModel_IsDualMode, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 11);

void DumpParameters(LPCWSTR aTargetPath, LPCWSTR aShortcutPath, LPCWSTR aAppModelID, LPCWSTR aDescription)
{
  if (aTargetPath)
    wprintf(L"target path: '%s'\n", aTargetPath);
  if (aShortcutPath)
    wprintf(L"shortcut path: '%s'\n", aShortcutPath);
  if (aAppModelID)
    wprintf(L"app id: '%s'\n", aAppModelID);
  if (aDescription)
    wprintf(L"description: '%s'\n", aDescription);
}

HRESULT
SetShortcutProps(LPCWSTR aShortcutPath, LPCWSTR aAppModelID, bool aSetID, bool aSetMode) 
{
  HRESULT hres; 
  ::CoInitialize(NULL);

  IPropertyStore *m_pps = NULL;
  if (FAILED(hres = SHGetPropertyStoreFromParsingName(aShortcutPath, NULL, GPS_READWRITE, IID_PPV_ARGS(&m_pps)))) {
    printf("SHGetPropertyStoreFromParsingName failed\n");
    goto Exit;
  }

  if (aSetMode) {
    PROPVARIANT propvar;
    if (FAILED(hres = InitPropVariantFromBoolean(true, &propvar)) ||
        FAILED(hres = m_pps->SetValue(PKEY_AppUserModel_IsDualMode, propvar))) {
      goto Exit;
    }
    PropVariantClear(&propvar);
  }

  if (aSetID && aAppModelID) {
    PROPVARIANT propvar;
    if (FAILED(hres = InitPropVariantFromString(aAppModelID, &propvar)) ||
        FAILED(hres = m_pps->SetValue(PKEY_AppUserModel_ID, propvar))) {
      goto Exit;
    }
    PropVariantClear(&propvar);
  }

  hres = m_pps->Commit();

  Exit:

  if (m_pps) {
    m_pps->Release();
  }

  CoUninitialize();
  return hres;
}

HRESULT
PrintShortcutProps(LPCWSTR aTargetPath) 
{
  HRESULT hres; 
  ::CoInitialize(NULL);

  IPropertyStore *m_pps = NULL;
  if (FAILED(hres = SHGetPropertyStoreFromParsingName(aTargetPath, NULL, GPS_READWRITE, IID_PPV_ARGS(&m_pps)))) {
    printf("SHGetPropertyStoreFromParsingName failed\n");
    goto Exit;
  }

  bool found = false;

  PROPVARIANT propvar;
  if (SUCCEEDED(hres = m_pps->GetValue(PKEY_AppUserModel_IsDualMode, &propvar)) && propvar.vt == VT_BOOL && propvar.boolVal == -1) {
    printf("PKEY_AppUserModel_IsDualMode found\n");
    PropVariantClear(&propvar);
    found = true;
  }

  if (SUCCEEDED(hres = m_pps->GetValue(PKEY_AppUserModel_ID, &propvar)) && propvar.pwszVal) {
    printf("PKEY_AppUserModel_ID found ");
    wprintf(L"value: '%s'\n", propvar.pwszVal);
    PropVariantClear(&propvar);
    found = true;
  }

  if (!found) {
    printf("no known properties found.\n");
  }

  Exit:

  if (m_pps) {
    m_pps->Release();
  }

  CoUninitialize();
  return hres;
}

HRESULT
CreateLink(LPCWSTR aTargetPath, LPCWSTR aShortcutPath, LPCWSTR aDescription) 
{ 
    HRESULT hres;
    IShellLink* psl;
 
    wprintf(L"creating shortcut: '%s'\n",  aShortcutPath);

    CoInitialize(NULL);

    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                            IID_IShellLink, (LPVOID*)&psl);
    if (FAILED(hres)) {
      CoUninitialize();
      return hres;
    }
    psl->SetPath(aTargetPath);
    if (aDescription) {
      psl->SetDescription(aDescription);
    } else {
      psl->SetDescription(L"");
    }
 
    IPersistFile* ppf = NULL;
    hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
 
    if (SUCCEEDED(hres)) {
        hres = ppf->Save(aShortcutPath, TRUE);
        ppf->Release();
    }
    psl->Release();
    CoUninitialize();
    return hres;
}

void DumpCommands()
{
  printf("control options:\n");
  printf("  /CREATE     create a shortcut for the target file.\n");
  printf("  /UPDATE     update properties on the target file.\n");
  printf("  /PRINT      print the known properties set on the target file.\n");
  printf("parameters:\n");
  printf("  /T(path)    the full path and filename of the target file.\n");
  printf("  /S(path)    with CREATE, the full path and filename of the shortcut to create.\n");
  printf("  /D(string)  with CREATE, adds a description to the shortcut.\n");
  printf("  /A(id)      the app model id to assign to the shortcut or target file.\n");
  printf("  /M          enable support for dual desktop and immersive modes on the shortcut or target file.\n");
}

int wmain(int argc, WCHAR* argv[])
{
  WCHAR shortcutPathStr[MAX_PATH];
  WCHAR targetPathStr[MAX_PATH];
  WCHAR appModelIDStr[MAX_PATH];
  WCHAR descriptionStr[MAX_PATH];
  
  shortcutPathStr[0] = '\0';
  targetPathStr[0] = '\0';
  appModelIDStr[0] = '\0';
  descriptionStr[0] = '\0';

  bool createShortcutFound = false;
  bool updateFound = false;
  bool shortcutPathFound = false;
  bool targetPathFound = false;
  bool appModelIDFound = false;
  bool modeFound = false;
  bool descriptionFound = false;
  bool printFound = false;

  int idx;
  for (idx = 1; idx < argc; idx++) {
    if (!wcscmp(L"/CREATE", argv[idx])) {
      createShortcutFound = true;
      continue;
    }
    if (!wcscmp(L"/UPDATE", argv[idx])) {
      updateFound = true;
      continue;
    }
    if (!wcscmp(L"/PRINT", argv[idx])) {
      printFound = true;
      continue;
    }

    if (!wcsncmp(L"/S", argv[idx], 2) && wcslen(argv[idx]) > 2) {
      wcscpy_s(shortcutPathStr, MAX_PATH, (argv[idx]+2));
      shortcutPathFound = true;
      continue;
    }
    if (!wcsncmp(L"/T", argv[idx], 2) && wcslen(argv[idx]) > 2) {
      wcscpy_s(targetPathStr, MAX_PATH, (argv[idx]+2));
      targetPathFound = true;
      continue;
    }
    if (!wcsncmp(L"/A", argv[idx], 2) && wcslen(argv[idx]) > 2) {
      wcscpy_s(appModelIDStr, MAX_PATH, (argv[idx]+2));
      appModelIDFound = true;
      continue;
    }
    if (!wcsncmp(L"/D", argv[idx], 2) && wcslen(argv[idx]) > 2 && wcslen(argv[idx]) < MAX_PATH) {
      wcscpy_s(descriptionStr, MAX_PATH, (argv[idx]+2));
      descriptionFound = true;
      continue;
    }
    if (!wcscmp(L"/M", argv[idx])) {
      modeFound = true;
      continue;
    }
  }

  DumpParameters(targetPathStr, shortcutPathStr, appModelIDStr, descriptionStr);

  if (!createShortcutFound && !updateFound && !printFound) {
    DumpCommands();
    return 0;
  }

  if (!targetPathFound) {
    printf("missing target file path.\n");
    return -1;
  }

  HRESULT hres;

  if (printFound) {
    if (FAILED(hres = PrintShortcutProps(targetPathStr))) {
      printf("failed printing target props HRESULT=%X\n", hres);
      return -1;
    }
    return 0;
  }

  if (createShortcutFound && !shortcutPathFound) {
    printf("missing shortcut file path.\n");
    return -1;
  }

  if (updateFound && !appModelIDFound && !modeFound) {
    printf("no properties selected.\n");
    return -1;
  }

  if (createShortcutFound) {
    if (FAILED(hres = CreateLink(targetPathStr, shortcutPathStr, (descriptionFound ? descriptionStr : NULL)))) {
      printf("failed creating shortcut HRESULT=%X\n", hres);
      return -1;
    }
  }

  LPCWSTR target;
  if (createShortcutFound) {
    target = shortcutPathStr;
  } else {
    target = targetPathStr;
  }

  if (appModelIDFound || modeFound) {
    if (FAILED(hres = SetShortcutProps(target, (appModelIDFound ? appModelIDStr : NULL), appModelIDFound, modeFound))) {
      printf("failed adding property HRESULT=%X\n", hres);
      return -1;
    }
  }

	return 0;
}

