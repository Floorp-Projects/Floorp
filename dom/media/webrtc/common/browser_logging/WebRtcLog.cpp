/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRtcLog.h"

#include "MainThreadUtils.h"
#include "mozilla/Logging.h"
#include "prenv.h"
#include "rtc_base/logging.h"

#include "nscore.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "mozilla/Preferences.h"

#include "nsIFile.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNativeCharsetUtils.h"

using mozilla::LogLevel;

static mozilla::LazyLogModule sWebRtcLog("webrtc_trace");

static rtc::LoggingSeverity LevelToSeverity(mozilla::LogLevel aLevel) {
  switch (aLevel) {
    case mozilla::LogLevel::Verbose:
      return rtc::LoggingSeverity::LS_VERBOSE;
    case mozilla::LogLevel::Debug:
    case mozilla::LogLevel::Info:
      return rtc::LoggingSeverity::LS_INFO;
    case mozilla::LogLevel::Warning:
      return rtc::LoggingSeverity::LS_WARNING;
    case mozilla::LogLevel::Error:
      return rtc::LoggingSeverity::LS_ERROR;
    case mozilla::LogLevel::Disabled:
      return rtc::LoggingSeverity::LS_NONE;
  }
  MOZ_ASSERT_UNREACHABLE("Unexpected log level");
  return rtc::LoggingSeverity::LS_NONE;
}

/**
 * Implementation of rtc::LogSink that forwards RTC_LOG() to MOZ_LOG().
 */
class LogSinkImpl : public WebrtcLogSinkHandle, public rtc::LogSink {
  NS_INLINE_DECL_REFCOUNTING(LogSinkImpl, override)

 public:
  static already_AddRefed<WebrtcLogSinkHandle> EnsureLogSink() {
    mozilla::AssertIsOnMainThread();
    if (sSingleton) {
      return do_AddRef(sSingleton);
    }
    return mozilla::MakeAndAddRef<LogSinkImpl>();
  }

  LogSinkImpl() {
    mozilla::AssertIsOnMainThread();
    MOZ_RELEASE_ASSERT(!sSingleton);

    rtc::LogMessage::AddLogToStream(this, rtc::LoggingSeverity::LS_NONE);
    sSingleton = this;
  }

 private:
  ~LogSinkImpl() {
    mozilla::AssertIsOnMainThread();
    MOZ_RELEASE_ASSERT(sSingleton == this);

    rtc::LogMessage::RemoveLogToStream(this);
    sSingleton = nullptr;
  }

  void UpdateLogLevels() {
    mozilla::AssertIsOnMainThread();
    mozilla::LogModule* webrtcModule = sWebRtcLog;
    mozilla::LogLevel webrtcLevel = webrtcModule->Level();

    rtc::LogMessage::RemoveLogToStream(this);
    rtc::LogMessage::AddLogToStream(this, LevelToSeverity(webrtcLevel));
  }

  void OnLogMessage(const std::string& message) override {
    MOZ_LOG(sWebRtcLog, LogLevel::Debug, ("%s", message.data()));
  }

  static LogSinkImpl* sSingleton MOZ_GUARDED_BY(mozilla::sMainThreadCapability);
};

LogSinkImpl* LogSinkImpl::sSingleton = nullptr;

already_AddRefed<WebrtcLogSinkHandle> EnsureWebrtcLogging() {
  mozilla::AssertIsOnMainThread();
  return LogSinkImpl::EnsureLogSink();
}

nsCString ConfigAecLog() {
  nsCString aecLogDir;
  if (rtc::LogMessage::aec_debug()) {
    return ""_ns;
  }
#if defined(ANDROID)
  const char* default_tmp_dir = "/dev/null";
  aecLogDir.Assign(default_tmp_dir);
#else
  nsCOMPtr<nsIFile> tempDir;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempDir));
  if (NS_SUCCEEDED(rv)) {
#  ifdef XP_WIN
    // WebRTC wants a path encoded in the native charset, not UTF-8.
    nsAutoString temp;
    tempDir->GetPath(temp);
    NS_CopyUnicodeToNative(temp, aecLogDir);
#  else
    tempDir->GetNativePath(aecLogDir);
#  endif
  }
#endif
  rtc::LogMessage::set_aec_debug_filename(aecLogDir.get());

  return aecLogDir;
}

nsCString StartAecLog() {
  nsCString aecLogDir;
  if (rtc::LogMessage::aec_debug()) {
    return ""_ns;
  }

  aecLogDir = ConfigAecLog();

  rtc::LogMessage::set_aec_debug(true);

  return aecLogDir;
}

void StopAecLog() { rtc::LogMessage::set_aec_debug(false); }
