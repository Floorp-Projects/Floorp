/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ErrorHandler.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/ProcessRuntime.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinTokenUtils.h"
#include "mozilla/XREAppData.h"
#include "nsWindowsHelpers.h"

#if defined(MOZ_LAUNCHER_PROCESS)
#  include "mozilla/LauncherRegistryInfo.h"
#endif  // defined(MOZ_LAUNCHER_PROCESS)

#include <algorithm>
#include <process.h>
#include <sstream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <objbase.h>
#include <rpc.h>
#if !defined(__MINGW32__)
#  include <comutil.h>
#  include <iwscapi.h>
#  include <wscapi.h>
#endif  // !defined(__MINGW32__)
#include <tlhelp32.h>

#if !defined(RRF_SUBKEY_WOW6464KEY)
#  define RRF_SUBKEY_WOW6464KEY 0x00010000
#endif  // !defined(RRF_SUBKEY_WOW6464KEY)

#define QUOTE_ME2(x) #x
#define QUOTE_ME(x) QUOTE_ME2(x)

#define TELEMETRY_BASE_URL L"https://incoming.telemetry.mozilla.org/submit"
#define TELEMETRY_NAMESPACE L"/firefox-launcher-process"
#define TELEMETRY_LAUNCHER_PING_DOCTYPE L"/launcher-process-failure"
#define TELEMETRY_LAUNCHER_PING_VERSION L"/1"

static const wchar_t kUrl[] = TELEMETRY_BASE_URL TELEMETRY_NAMESPACE
    TELEMETRY_LAUNCHER_PING_DOCTYPE TELEMETRY_LAUNCHER_PING_VERSION L"/";
static const uint32_t kGuidCharLenWithNul = 39;
static const uint32_t kGuidCharLenNoBracesNoNul = 36;
static const mozilla::StaticXREAppData* gAppData;
static bool gForceEventLog = false;

namespace {

struct EventSourceDeleter {
  using pointer = HANDLE;

  void operator()(pointer aEvtSrc) { ::DeregisterEventSource(aEvtSrc); }
};

using EventLog = mozilla::UniquePtr<HANDLE, EventSourceDeleter>;

struct SerializedEventData {
  HRESULT mHr;
  uint32_t mLine;
  char mFile[1];
};

}  // anonymous namespace

static void PostErrorToLog(const mozilla::LauncherError& aError) {
  // This is very bare-bones; just enough to spit out an HRESULT to the
  // Application event log.
  EventLog log(::RegisterEventSourceW(nullptr, L"Firefox"));
  if (!log) {
    return;
  }

  size_t fileLen = strlen(aError.mFile);
  size_t dataLen = sizeof(HRESULT) + sizeof(uint32_t) + fileLen;
  auto evtDataBuf = mozilla::MakeUnique<char[]>(dataLen);
  SerializedEventData& evtData =
      *reinterpret_cast<SerializedEventData*>(evtDataBuf.get());
  evtData.mHr = aError.mError.AsHResult();
  evtData.mLine = aError.mLine;
  // Since this is binary data, we're not concerning ourselves with null
  // terminators.
  memcpy(evtData.mFile, aError.mFile, fileLen);

  ::ReportEventW(log.get(), EVENTLOG_ERROR_TYPE, 0, aError.mError.AsHResult(),
                 nullptr, 0, dataLen, nullptr, evtDataBuf.get());
}

#if defined(MOZ_TELEMETRY_REPORTING)

namespace {

// This JSONWriteFunc writes directly to a temp file. By creating this file
// with the FILE_ATTRIBUTE_TEMPORARY attribute, we hint to the OS that this
// file is short-lived. The OS will try to avoid flushing it to disk if at
// all possible.
class TempFileWriter final : public mozilla::JSONWriteFunc {
 public:
  TempFileWriter() : mFailed(false), mSuccessfulHandoff(false) {
    wchar_t name[MAX_PATH + 1] = {};
    if (_wtmpnam_s(name)) {
      mFailed = true;
      return;
    }

    mTempFileName = name;

    mTempFile.own(::CreateFileW(name, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr));
    if (mTempFile.get() == INVALID_HANDLE_VALUE) {
      mFailed = true;
    }
  }

  ~TempFileWriter() {
    if (mSuccessfulHandoff) {
      // It is no longer our responsibility to delete the temp file if we have
      // successfully handed it off to pingsender.
      return;
    }

    mTempFile.reset();
    ::DeleteFileW(mTempFileName.c_str());
  }

  explicit operator bool() const { return !mFailed; }

  void Write(const char* aStr) override {
    if (mFailed) {
      return;
    }

    size_t len = strlen(aStr);
    DWORD bytesWritten = 0;
    if (!::WriteFile(mTempFile, aStr, len, &bytesWritten, nullptr) ||
        bytesWritten != len) {
      mFailed = true;
    }
  }

  const std::wstring& GetFileName() const { return mTempFileName; }

  void SetSuccessfulHandoff() { mSuccessfulHandoff = true; }

 private:
  bool mFailed;
  bool mSuccessfulHandoff;
  std::wstring mTempFileName;
  nsAutoHandle mTempFile;
};

using SigMap = mozilla::Vector<std::wstring, 0, InfallibleAllocPolicy>;

}  // anonymous namespace

// This is the guideline for maximum string length for telemetry intake
static const size_t kMaxStrLen = 80;

static mozilla::UniquePtr<char[]> WideToUTF8(const wchar_t* aStr,
                                             const size_t aStrLenExclNul) {
  // Yes, this might not handle surrogate pairs correctly. Let's just let
  // WideCharToMultiByte fail in that unlikely case.
  size_t cvtLen = std::min(aStrLenExclNul, kMaxStrLen);

  int numConv = ::WideCharToMultiByte(CP_UTF8, 0, aStr, cvtLen, nullptr, 0,
                                      nullptr, nullptr);
  if (!numConv) {
    return nullptr;
  }

  // Include room for the null terminator by adding one
  auto buf = mozilla::MakeUnique<char[]>(numConv + 1);

  numConv = ::WideCharToMultiByte(CP_UTF8, 0, aStr, cvtLen, buf.get(), numConv,
                                  nullptr, nullptr);
  if (!numConv) {
    return nullptr;
  }

  // Add null termination. numConv does not include the terminator, so we don't
  // subtract 1 when indexing into buf.
  buf[numConv] = 0;

  return buf;
}

static mozilla::UniquePtr<char[]> WideToUTF8(const wchar_t* aStr) {
  return WideToUTF8(aStr, wcslen(aStr));
}

static mozilla::UniquePtr<char[]> WideToUTF8(const std::wstring& aStr) {
  return WideToUTF8(aStr.c_str(), aStr.length());
}

// MinGW does not support the Windows Security Center APIs.
#  if !defined(__MINGW32__)

static mozilla::UniquePtr<char[]> WideToUTF8(const _bstr_t& aStr) {
  return WideToUTF8(static_cast<const wchar_t*>(aStr), aStr.length());
}

namespace {

struct ProviderKey {
  WSC_SECURITY_PROVIDER mProviderType;
  const char* mKey;
};

}  // anonymous namespace

static bool EnumWSCProductList(RefPtr<IWSCProductList>& aProdList,
                               mozilla::JSONWriter& aJson) {
  LONG count;
  HRESULT hr = aProdList->get_Count(&count);
  if (FAILED(hr)) {
    return false;
  }

  // Unlikely, but put a bound on the max length of the output array for the
  // purposes of telemetry intake.
  count = std::min(count, 1000L);

  // Record the name(s) of each active registered product in this category
  for (LONG index = 0; index < count; ++index) {
    RefPtr<IWscProduct> product;
    hr = aProdList->get_Item(index, getter_AddRefs(product));
    if (FAILED(hr)) {
      return false;
    }

    WSC_SECURITY_PRODUCT_STATE state;
    hr = product->get_ProductState(&state);
    if (FAILED(hr)) {
      return false;
    }

    // We only care about products that are active
    if (state == WSC_SECURITY_PRODUCT_STATE_OFF ||
        state == WSC_SECURITY_PRODUCT_STATE_SNOOZED ||
        state == WSC_SECURITY_PRODUCT_STATE_EXPIRED) {
      continue;
    }

    _bstr_t bName;
    hr = product->get_ProductName(bName.GetAddress());
    if (FAILED(hr)) {
      return false;
    }

    auto buf = WideToUTF8(bName);
    if (!buf) {
      return false;
    }

    aJson.StringElement(buf.get());
  }

  return true;
}

static const ProviderKey gProvKeys[] = {
    {WSC_SECURITY_PROVIDER_ANTIVIRUS, "av"},
    {WSC_SECURITY_PROVIDER_ANTISPYWARE, "antispyware"},
    {WSC_SECURITY_PROVIDER_FIREWALL, "firewall"}};

static bool AddWscInfo(mozilla::JSONWriter& aJson) {
  if (!mozilla::IsWin8OrLater()) {
    // We haven't written anything yet, so we can return true here and continue
    // capturing data.
    return true;
  }

  // We need COM for this. Using ProcessRuntime so that process-global COM
  // configuration is done correctly
  mozilla::mscom::ProcessRuntime mscom(
      mozilla::mscom::ProcessRuntime::ProcessCategory::Launcher);
  if (!mscom) {
    // We haven't written anything yet, so we can return true here and continue
    // capturing data.
    return true;
  }

  aJson.StartObjectProperty("security");

  const CLSID clsid = __uuidof(WSCProductList);
  const IID iid = __uuidof(IWSCProductList);

  for (uint32_t index = 0; index < mozilla::ArrayLength(gProvKeys); ++index) {
    // NB: A separate instance of IWSCProductList is needed for each distinct
    // security provider type; MSDN says that we cannot reuse the same object
    // and call Initialize() to pave over the previous data.
    RefPtr<IWSCProductList> prodList;
    HRESULT hr = ::CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, iid,
                                    getter_AddRefs(prodList));
    if (FAILED(hr)) {
      return false;
    }

    hr = prodList->Initialize(gProvKeys[index].mProviderType);
    if (FAILED(hr)) {
      return false;
    }

    aJson.StartArrayProperty(gProvKeys[index].mKey);

    if (!EnumWSCProductList(prodList, aJson)) {
      return false;
    }

    aJson.EndArray();
  }

  aJson.EndObject();

  return true;
}
#  endif  // !defined(__MINGW32__)

// Max array length for telemetry intake.
static const size_t kMaxArrayLen = 1000;

static bool AddModuleInfo(const nsAutoHandle& aSnapshot,
                          mozilla::JSONWriter& aJson) {
  if (aSnapshot.get() == INVALID_HANDLE_VALUE) {
    // We haven't written anything yet, so we can return true here and continue
    // capturing data.
    return true;
  }

  SigMap signatures;
  size_t moduleCount = 0;

  MODULEENTRY32W module = {sizeof(module)};
  if (!::Module32FirstW(aSnapshot, &module)) {
    // We haven't written anything yet, so we can return true here and continue
    // capturing data.
    return true;
  }

  mozilla::glue::BasicDllServices dllServices;

  aJson.StartObjectProperty("modules");

  // For each module, add its version number (or empty string if not present),
  // followed by an optional index into the signatures array
  do {
    ++moduleCount;

    wchar_t leaf[_MAX_FNAME] = {};
    if (::_wsplitpath_s(module.szExePath, nullptr, 0, nullptr, 0, leaf,
                        mozilla::ArrayLength(leaf), nullptr, 0)) {
      return false;
    }

    if (_wcslwr_s(leaf, mozilla::ArrayLength(leaf))) {
      return false;
    }

    auto leafUtf8 = WideToUTF8(leaf);
    if (!leafUtf8) {
      return false;
    }

    aJson.StartArrayProperty(leafUtf8.get());

    std::string version;
    DWORD verInfoSize = ::GetFileVersionInfoSizeW(module.szExePath, nullptr);
    if (verInfoSize) {
      auto verInfoBuf = mozilla::MakeUnique<BYTE[]>(verInfoSize);

      if (::GetFileVersionInfoW(module.szExePath, 0, verInfoSize,
                                verInfoBuf.get())) {
        VS_FIXEDFILEINFO* fixedInfo = nullptr;
        UINT fixedInfoLen = 0;

        if (::VerQueryValueW(verInfoBuf.get(), L"\\",
                             reinterpret_cast<LPVOID*>(&fixedInfo),
                             &fixedInfoLen)) {
          std::ostringstream oss;
          oss << HIWORD(fixedInfo->dwFileVersionMS) << '.'
              << LOWORD(fixedInfo->dwFileVersionMS) << '.'
              << HIWORD(fixedInfo->dwFileVersionLS) << '.'
              << LOWORD(fixedInfo->dwFileVersionLS);
          version = oss.str();
        }
      }
    }

    aJson.StringElement(version.c_str());

    mozilla::Maybe<ptrdiff_t> sigIndex;
    auto signedBy = dllServices.GetBinaryOrgName(module.szExePath);
    if (signedBy) {
      std::wstring strSignedBy(signedBy.get());
      auto entry = std::find(signatures.begin(), signatures.end(), strSignedBy);
      if (entry == signatures.end()) {
        mozilla::Unused << signatures.append(std::move(strSignedBy));
        entry = &signatures.back();
      }

      sigIndex = mozilla::Some(entry - signatures.begin());
    }

    if (sigIndex) {
      aJson.IntElement(sigIndex.value());
    }

    aJson.EndArray();
  } while (moduleCount < kMaxArrayLen && ::Module32NextW(aSnapshot, &module));

  aJson.EndObject();

  aJson.StartArrayProperty("signatures");

  // Serialize each entry in the signatures array
  for (auto&& itr : signatures) {
    auto sigUtf8 = WideToUTF8(itr);
    if (!sigUtf8) {
      continue;
    }

    aJson.StringElement(sigUtf8.get());
  }

  aJson.EndArray();

  return true;
}

namespace {

struct PingThreadContext {
  explicit PingThreadContext(const mozilla::LauncherError& aError)
      : mLauncherError(aError),
        mModulesSnapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0)) {}
  mozilla::LauncherError mLauncherError;
  nsAutoHandle mModulesSnapshot;
};

}  // anonymous namespace

static bool PrepPing(const PingThreadContext& aContext, const std::wstring& aId,
                     mozilla::JSONWriter& aJson) {
#  if defined(DEBUG)
  const mozilla::JSONWriter::CollectionStyle style =
      mozilla::JSONWriter::MultiLineStyle;
#  else
  const mozilla::JSONWriter::CollectionStyle style =
      mozilla::JSONWriter::SingleLineStyle;
#  endif  // defined(DEBUG)

  aJson.Start(style);

  aJson.StringProperty("type", "launcher-process-failure");
  aJson.IntProperty("version", 1);

  auto idUtf8 = WideToUTF8(aId);
  if (idUtf8) {
    aJson.StringProperty("id", idUtf8.get());
  }

  time_t now;
  time(&now);
  tm gmTm;
  if (!gmtime_s(&gmTm, &now)) {
    char isoTimeBuf[32] = {};
    if (strftime(isoTimeBuf, mozilla::ArrayLength(isoTimeBuf), "%FT%T.000Z",
                 &gmTm)) {
      aJson.StringProperty("creationDate", isoTimeBuf);
    }
  }

  aJson.StringProperty("update_channel", QUOTE_ME(MOZ_UPDATE_CHANNEL));

  if (gAppData) {
    aJson.StringProperty("build_id", gAppData->buildID);
    aJson.StringProperty("build_version", gAppData->version);
  }

  OSVERSIONINFOEXW osv = {sizeof(osv)};
  if (::GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osv))) {
    std::ostringstream oss;
    oss << osv.dwMajorVersion << "." << osv.dwMinorVersion << "."
        << osv.dwBuildNumber;

    if (osv.dwMajorVersion == 10 && osv.dwMinorVersion == 0) {
      // Get the "Update Build Revision" (UBR) value
      DWORD ubrValue;
      DWORD ubrValueLen = sizeof(ubrValue);
      LSTATUS ubrOk =
          ::RegGetValueW(HKEY_LOCAL_MACHINE,
                         L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                         L"UBR", RRF_RT_DWORD | RRF_SUBKEY_WOW6464KEY, nullptr,
                         &ubrValue, &ubrValueLen);
      if (ubrOk == ERROR_SUCCESS) {
        oss << "." << ubrValue;
      }
    }

    if (oss) {
      aJson.StringProperty("os_version", oss.str().c_str());
    }

    bool isServer = osv.wProductType == VER_NT_DOMAIN_CONTROLLER ||
                    osv.wProductType == VER_NT_SERVER;
    aJson.BoolProperty("server_os", isServer);
  }

  WCHAR localeName[LOCALE_NAME_MAX_LENGTH] = {};
  int localeNameLen =
      ::GetUserDefaultLocaleName(localeName, mozilla::ArrayLength(localeName));
  if (localeNameLen) {
    auto localeNameUtf8 = WideToUTF8(localeName, localeNameLen - 1);
    if (localeNameUtf8) {
      aJson.StringProperty("os_locale", localeNameUtf8.get());
    }
  }

  SYSTEM_INFO sysInfo;
  ::GetNativeSystemInfo(&sysInfo);
  aJson.IntProperty("cpu_arch", sysInfo.wProcessorArchitecture);
  aJson.IntProperty("num_logical_cpus", sysInfo.dwNumberOfProcessors);

  mozilla::LauncherResult<bool> isAdminWithoutUac =
      mozilla::IsAdminWithoutUac();
  if (isAdminWithoutUac.isOk()) {
    aJson.BoolProperty("is_admin_without_uac", isAdminWithoutUac.unwrap());
  }

  MEMORYSTATUSEX memStatus = {sizeof(memStatus)};
  if (::GlobalMemoryStatusEx(&memStatus)) {
    aJson.StartObjectProperty("memory");
    aJson.IntProperty("total_phys", memStatus.ullTotalPhys);
    aJson.IntProperty("avail_phys", memStatus.ullAvailPhys);
    aJson.IntProperty("avail_page_file", memStatus.ullAvailPageFile);
    aJson.IntProperty("avail_virt", memStatus.ullAvailVirtual);
    aJson.EndObject();
  }

  aJson.StringProperty("xpcom_abi", TARGET_XPCOM_ABI);

  aJson.StartObjectProperty("launcher_error", style);

  std::string srcFileLeaf(aContext.mLauncherError.mFile);
  // Obtain the leaf name of the file for privacy reasons
  // (In case this is somebody's local build)
  auto pos = srcFileLeaf.find_last_of("/\\");
  if (pos != std::string::npos) {
    srcFileLeaf = srcFileLeaf.substr(pos + 1);
  }

  aJson.StringProperty("source_file", srcFileLeaf.c_str());

  aJson.IntProperty("source_line", aContext.mLauncherError.mLine);
  aJson.IntProperty("hresult", aContext.mLauncherError.mError.AsHResult());
  aJson.EndObject();

#  if !defined(__MINGW32__)
  if (!AddWscInfo(aJson)) {
    return false;
  }
#  endif  // !defined(__MINGW32__)

  if (!AddModuleInfo(aContext.mModulesSnapshot, aJson)) {
    return false;
  }

  aJson.End();

  return true;
}

static bool DoSendPing(const PingThreadContext& aContext) {
  auto writeFunc = mozilla::MakeUnique<TempFileWriter>();
  if (!(*writeFunc)) {
    return false;
  }

  mozilla::JSONWriter json(std::move(writeFunc));

  UUID uuid;
  if (::UuidCreate(&uuid) != RPC_S_OK) {
    return false;
  }

  wchar_t guidBuf[kGuidCharLenWithNul] = {};
  if (::StringFromGUID2(uuid, guidBuf, kGuidCharLenWithNul) !=
      kGuidCharLenWithNul) {
    return false;
  }

  // Strip the curly braces off of the guid
  std::wstring guidNoBraces(guidBuf + 1, kGuidCharLenNoBracesNoNul);

  // Populate json with the ping information
  if (!PrepPing(aContext, guidNoBraces, json)) {
    return false;
  }

  // Obtain the name of the temp file that we have written
  TempFileWriter& tempFile = *static_cast<TempFileWriter*>(json.WriteFunc());
  if (!tempFile) {
    return false;
  }

  const std::wstring& fileName = tempFile.GetFileName();

  // Using the path to our executable binary, construct the path to
  // pingsender.exe
  mozilla::UniquePtr<wchar_t[]> exePath(mozilla::GetFullBinaryPath());

  wchar_t drive[_MAX_DRIVE] = {};
  wchar_t dir[_MAX_DIR] = {};
  if (_wsplitpath_s(exePath.get(), drive, mozilla::ArrayLength(drive), dir,
                    mozilla::ArrayLength(dir), nullptr, 0, nullptr, 0)) {
    return false;
  }

  wchar_t pingSenderPath[MAX_PATH + 1] = {};
  if (_wmakepath_s(pingSenderPath, mozilla::ArrayLength(pingSenderPath), drive,
                   dir, L"pingsender", L"exe")) {
    return false;
  }

  // Construct the telemetry URL
  wchar_t urlBuf[mozilla::ArrayLength(kUrl) + kGuidCharLenNoBracesNoNul] = {};
  if (wcscpy_s(urlBuf, kUrl)) {
    return false;
  }

  if (wcscat_s(urlBuf, guidNoBraces.c_str())) {
    return false;
  }

  // Now build the command line arguments to pingsender
  wchar_t* pingSenderArgv[] = {pingSenderPath, urlBuf,
                               const_cast<wchar_t*>(fileName.c_str())};

  mozilla::UniquePtr<wchar_t[]> pingSenderCmdLine(mozilla::MakeCommandLine(
      mozilla::ArrayLength(pingSenderArgv), pingSenderArgv));

  // Now start pingsender to handle the rest
  PROCESS_INFORMATION pi;

  STARTUPINFOW si = {sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  if (!::CreateProcessW(pingSenderPath, pingSenderCmdLine.get(), nullptr,
                        nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    return false;
  }

  tempFile.SetSuccessfulHandoff();

  nsAutoHandle proc(pi.hProcess);
  nsAutoHandle thread(pi.hThread);

  return true;
}

static unsigned __stdcall SendPingThread(void* aContext) {
  mozilla::UniquePtr<PingThreadContext> context(
      reinterpret_cast<PingThreadContext*>(aContext));

  if (!DoSendPing(*context) || gForceEventLog) {
    PostErrorToLog(context->mLauncherError);
  }

  return 0;
}

#endif  // defined(MOZ_TELEMETRY_REPORTING)

static bool SendPing(const mozilla::LauncherError& aError) {
#if defined(MOZ_TELEMETRY_REPORTING)
#  if defined(MOZ_LAUNCHER_PROCESS)
  mozilla::LauncherRegistryInfo regInfo;
  mozilla::LauncherResult<bool> telemetryEnabled = regInfo.IsTelemetryEnabled();
  if (telemetryEnabled.isErr() || !telemetryEnabled.unwrap()) {
    // Do not send anything if telemetry has been opted out
    return false;
  }
#  endif  // defined(MOZ_LAUNCHER_PROCESS)

  // We send this ping when the launcher process fails. After we start the
  // SendPingThread, this thread falls back from running as the launcher process
  // to running as the browser main thread. Once this happens, it will be unsafe
  // to set up PoisonIOInterposer (since we have already spun up a background
  // thread).
  mozilla::SaveToEnv("MOZ_DISABLE_POISON_IO_INTERPOSER=1");

  // Capture aError and our module list into context for processing on another
  // thread.
  auto thdParam = mozilla::MakeUnique<PingThreadContext>(aError);

  // The ping does a lot of file I/O. Since we want this thread to continue
  // executing browser startup, we should gather that information on a
  // background thread.
  uintptr_t thdHandle =
      _beginthreadex(nullptr, 0, &SendPingThread, thdParam.get(),
                     STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
  if (!thdHandle) {
    return false;
  }

  // We have handed off thdParam to the background thread
  mozilla::Unused << thdParam.release();

  ::CloseHandle(reinterpret_cast<HANDLE>(thdHandle));
  return true;
#else
  return false;
#endif
}

namespace mozilla {

void HandleLauncherError(const LauncherError& aError) {
#if defined(MOZ_LAUNCHER_PROCESS)
  LauncherRegistryInfo regInfo;
  Unused << regInfo.DisableDueToFailure();
#endif  // defined(MOZ_LAUNCHER_PROCESS)

  if (SendPing(aError) && !gForceEventLog) {
    return;
  }

  PostErrorToLog(aError);
}

void SetLauncherErrorAppData(const StaticXREAppData& aAppData) {
  gAppData = &aAppData;
}

void SetLauncherErrorForceEventLog() { gForceEventLog = true; }

}  // namespace mozilla
