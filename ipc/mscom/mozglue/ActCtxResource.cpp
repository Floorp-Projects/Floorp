/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActCtxResource.h"

#include <string>

#include "mozilla/GetKnownFolderPath.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsHelpers.h"

namespace mozilla {
namespace mscom {

#if !defined(HAVE_64BIT_BUILD)

static bool ReadCOMRegDefaultString(const std::wstring& aRegPath,
                                    std::wstring& aOutBuf) {
  aOutBuf.clear();

  std::wstring fullyQualifiedRegPath;
  fullyQualifiedRegPath.append(L"SOFTWARE\\Classes\\");
  fullyQualifiedRegPath.append(aRegPath);

  // Get the required size and type of the registry value.
  // We expect either REG_SZ or REG_EXPAND_SZ.
  DWORD type;
  DWORD bufLen = 0;
  LONG result =
      ::RegGetValueW(HKEY_LOCAL_MACHINE, fullyQualifiedRegPath.c_str(), nullptr,
                     RRF_RT_ANY, &type, nullptr, &bufLen);
  if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
    return false;
  }

  // Now obtain the value
  DWORD flags = type == REG_SZ ? RRF_RT_REG_SZ : RRF_RT_REG_EXPAND_SZ;

  aOutBuf.resize((bufLen + 1) / sizeof(char16_t));

  result = ::RegGetValueW(HKEY_LOCAL_MACHINE, fullyQualifiedRegPath.c_str(),
                          nullptr, flags, nullptr, &aOutBuf[0], &bufLen);
  if (result != ERROR_SUCCESS) {
    aOutBuf.clear();
    return false;
  }

  // Truncate terminator
  aOutBuf.resize((bufLen + 1) / sizeof(char16_t) - 1);
  return true;
}

static bool IsSystemOleAcc(HANDLE aFile) {
  if (aFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  BY_HANDLE_FILE_INFORMATION info = {};
  if (!::GetFileInformationByHandle(aFile, &info)) {
    return false;
  }

  // Use FOLDERID_SystemX86 so that Windows doesn't give us a redirected
  // system32 if we're a 32-bit process running on a 64-bit OS. This is
  // necessary because the values that we are reading from the registry
  // are not redirected; they reference SysWOW64 directly.
  auto systemPath = GetKnownFolderPath(FOLDERID_SystemX86);
  if (!systemPath) {
    return false;
  }

  std::wstring oleAccPath(systemPath.get());
  oleAccPath.append(L"\\oleacc.dll");

  nsAutoHandle oleAcc(
      ::CreateFileW(oleAccPath.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

  if (oleAcc.get() == INVALID_HANDLE_VALUE) {
    return false;
  }

  BY_HANDLE_FILE_INFORMATION oleAccInfo = {};
  if (!::GetFileInformationByHandle(oleAcc, &oleAccInfo)) {
    return false;
  }

  return info.dwVolumeSerialNumber == oleAccInfo.dwVolumeSerialNumber &&
         info.nFileIndexLow == oleAccInfo.nFileIndexLow &&
         info.nFileIndexHigh == oleAccInfo.nFileIndexHigh;
}

static bool IsTypelibPreferred() {
  // If IAccessible's Proxy/Stub CLSID is kUniversalMarshalerClsid, then any
  // external a11y clients are expecting to use a typelib.
  const wchar_t kUniversalMarshalerClsid[] =
      L"{00020424-0000-0000-C000-000000000046}";

  const wchar_t kIAccessiblePSClsidPath[] =
      L"Interface\\{618736E0-3C3D-11CF-810C-00AA00389B71}"
      L"\\ProxyStubClsid32";

  std::wstring psClsid;
  if (!ReadCOMRegDefaultString(kIAccessiblePSClsidPath, psClsid)) {
    return false;
  }

  if (psClsid.size() !=
      sizeof(kUniversalMarshalerClsid) / sizeof(kUniversalMarshalerClsid)[0] -
          1) {
    return false;
  }

  int index = 0;
  while (kUniversalMarshalerClsid[index]) {
    if (toupper(psClsid[index]) != kUniversalMarshalerClsid[index]) {
      return false;
    }
    index++;
  }
  return true;
}

static bool IsIAccessibleTypelibRegistered() {
  // The system default IAccessible typelib is always registered with version
  // 1.1, under the neutral locale (LCID 0).
  const wchar_t kIAccessibleTypelibRegPath[] =
      L"TypeLib\\{1EA4DBF0-3C3B-11CF-810C-00AA00389B71}\\1.1\\0\\win32";

  std::wstring typelibPath;
  if (!ReadCOMRegDefaultString(kIAccessibleTypelibRegPath, typelibPath)) {
    return false;
  }

  nsAutoHandle libTestFile(
      ::CreateFileW(typelibPath.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

  return IsSystemOleAcc(libTestFile);
}

static bool IsIAccessiblePSRegistered() {
  const wchar_t kIAccessiblePSRegPath[] =
      L"CLSID\\{03022430-ABC4-11D0-BDE2-00AA001A1953}\\InProcServer32";

  std::wstring proxyStubPath;
  if (!ReadCOMRegDefaultString(kIAccessiblePSRegPath, proxyStubPath)) {
    return false;
  }

  nsAutoHandle libTestFile(
      ::CreateFileW(proxyStubPath.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

  return IsSystemOleAcc(libTestFile);
}

static bool UseIAccessibleProxyStub() {
  // If a typelib is preferred then external clients are expecting to use
  // typelib marshaling, so we should use that whenever available.
  if (IsTypelibPreferred() && IsIAccessibleTypelibRegistered()) {
    return false;
  }

  // Otherwise we try the proxy/stub
  if (IsIAccessiblePSRegistered()) {
    return true;
  }

  return false;
}

#endif  // !defined(HAVE_64BIT_BUILD)

#if defined(_MSC_VER)
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

static HMODULE GetContainingModuleHandle() {
  HMODULE thisModule = nullptr;
#if defined(_MSC_VER)
  thisModule = reinterpret_cast<HMODULE>(&__ImageBase);
#else
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&GetContainingModuleHandle),
                          &thisModule)) {
    return 0;
  }
#endif
  return thisModule;
}

ActCtxResource ActCtxResource::GetAccessibilityResource() {
  ActCtxResource result = {};
  result.mModule = GetContainingModuleHandle();
#if defined(HAVE_64BIT_BUILD)
  // The manifest for 64-bit Windows is embedded with resource ID 64.
  result.mId = 64;
#else
  // The manifest for 32-bit Windows is embedded with resource ID 32.
  // Beginning with Windows 10 Creators Update, 32-bit builds always use the
  // 64-bit manifest. Older builds of Windows may or may not require the 64-bit
  // manifest: UseIAccessibleProxyStub() determines the course of action.
  if (mozilla::IsWin10CreatorsUpdateOrLater() || UseIAccessibleProxyStub()) {
    result.mId = 64;
  } else {
    result.mId = 32;
  }
#endif  // defined(HAVE_64BIT_BUILD)
  return result;
}

}  // namespace mscom
}  // namespace mozilla
