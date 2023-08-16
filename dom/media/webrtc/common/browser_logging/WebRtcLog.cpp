/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRtcLog.h"

#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"
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
static mozilla::LazyLogModule sLogAEC("AEC");

class LogSinkImpl : public rtc::LogSink {
 public:
  LogSinkImpl() {}

 private:
  void OnLogMessage(const std::string& message) override {
    MOZ_LOG(sWebRtcLog, LogLevel::Debug, ("%s", message.data()));
  }
};

// For RTC_LOG()
static mozilla::StaticAutoPtr<LogSinkImpl> sSink;

mozilla::LogLevel CheckOverrides() {
  mozilla::LogModule* log_info = sWebRtcLog;
  mozilla::LogLevel log_level = log_info->Level();

  log_info = sLogAEC;
  if (sLogAEC && (log_info->Level() != mozilla::LogLevel::Disabled)) {
    rtc::LogMessage::set_aec_debug(true);
  }

  return log_level;
}

void ConfigWebRtcLog(mozilla::LogLevel level) {
  rtc::LoggingSeverity log_level;
  switch (level) {
    case mozilla::LogLevel::Verbose:
      log_level = rtc::LoggingSeverity::LS_VERBOSE;
      break;
    case mozilla::LogLevel::Debug:
    case mozilla::LogLevel::Info:
      log_level = rtc::LoggingSeverity::LS_INFO;
      break;
    case mozilla::LogLevel::Warning:
      log_level = rtc::LoggingSeverity::LS_WARNING;
      break;
    case mozilla::LogLevel::Error:
      log_level = rtc::LoggingSeverity::LS_ERROR;
      break;
    case mozilla::LogLevel::Disabled:
      log_level = rtc::LoggingSeverity::LS_NONE;
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }
  rtc::LogMessage::LogToDebug(log_level);
  if (level != mozilla::LogLevel::Disabled) {
    // always capture LOG(...) << ... logging in webrtc.org code to nspr logs
    if (!sSink) {
      sSink = new LogSinkImpl();
      rtc::LogMessage::AddLogToStream(sSink, log_level);
      // it's ok if this leaks to program end
    }
  } else if (sSink) {
    rtc::LogMessage::RemoveLogToStream(sSink);
    sSink = nullptr;
  }
}

void StartWebRtcLog(mozilla::LogLevel log_level) {
  if (log_level == mozilla::LogLevel::Disabled) {
    return;
  }

  mozilla::LogLevel level = CheckOverrides();

  ConfigWebRtcLog(level);
}

void EnableWebRtcLog() {
  mozilla::LogLevel level = CheckOverrides();
  ConfigWebRtcLog(level);
}

// Called when we destroy the singletons from PeerConnectionCtx or if the
// user changes logging in about:webrtc
void StopWebRtcLog() {
  if (sSink) {
    rtc::LogMessage::RemoveLogToStream(sSink);
    sSink = nullptr;
  }
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

  CheckOverrides();
  aecLogDir = ConfigAecLog();

  rtc::LogMessage::set_aec_debug(true);

  return aecLogDir;
}

void StopAecLog() { rtc::LogMessage::set_aec_debug(false); }
