/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static content security utilities. */

#include "nsContentSecurityUtils.h"

#include "mozilla/Components.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsComponentManagerUtils.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIURI.h"
#include "nsITransfer.h"
#include "nsNetUtil.h"
#include "nsSandboxFlags.h"
#if defined(XP_WIN)
#  include "WinUtils.h"
#  include <wininet.h>
#endif

#include "FramingChecker.h"
#include "js/Array.h"  // JS::GetArrayLength
#include "js/ContextOptions.h"
#include "js/PropertyAndElement.h"  // JS_GetElement
#include "js/RegExp.h"
#include "js/RegExpFlags.h"  // JS::RegExpFlags
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/StaticPrefs_security.h"
#include "LoadInfo.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"
#include "mozilla/TelemetryEventEnums.h"
#include "nsIConsoleService.h"
#include "nsIStringBundle.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::Telemetry;

extern mozilla::LazyLogModule sCSMLog;
extern Atomic<bool, mozilla::Relaxed> sJSHacksChecked;
extern Atomic<bool, mozilla::Relaxed> sJSHacksPresent;
extern Atomic<bool, mozilla::Relaxed> sTelemetryEventEnabled;

// Helper function for IsConsideredSameOriginForUIR which makes
// Principals of scheme 'http' return Principals of scheme 'https'.
static already_AddRefed<nsIPrincipal> MakeHTTPPrincipalHTTPS(
    nsIPrincipal* aPrincipal) {
  nsCOMPtr<nsIPrincipal> principal = aPrincipal;
  // if the principal is not http, then it can also not be upgraded
  // to https.
  if (!principal->SchemeIs("http")) {
    return principal.forget();
  }

  nsAutoCString spec;
  aPrincipal->GetAsciiSpec(spec);
  // replace http with https
  spec.ReplaceLiteral(0, 4, "https");

  nsCOMPtr<nsIURI> newURI;
  nsresult rv = NS_NewURI(getter_AddRefs(newURI), spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  mozilla::OriginAttributes OA =
      BasePrincipal::Cast(aPrincipal)->OriginAttributesRef();

  principal = BasePrincipal::CreateContentPrincipal(newURI, OA);
  return principal.forget();
}

/* static */
bool nsContentSecurityUtils::IsConsideredSameOriginForUIR(
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aResultPrincipal) {
  MOZ_ASSERT(aTriggeringPrincipal);
  MOZ_ASSERT(aResultPrincipal);
  // we only have to make sure that the following truth table holds:
  // aTriggeringPrincipal         | aResultPrincipal             | Result
  // ----------------------------------------------------------------
  // http://example.com/foo.html  | http://example.com/bar.html  | true
  // http://example.com/foo.html  | https://example.com/bar.html | true
  // https://example.com/foo.html | https://example.com/bar.html | true
  // https://example.com/foo.html | http://example.com/bar.html  | true

  // fast path if both principals are same-origin
  if (aTriggeringPrincipal->Equals(aResultPrincipal)) {
    return true;
  }

  // in case a principal uses a scheme of 'http' then we just upgrade to
  // 'https' and use the principal equals comparison operator to check
  // for same-origin.
  nsCOMPtr<nsIPrincipal> compareTriggeringPrincipal =
      MakeHTTPPrincipalHTTPS(aTriggeringPrincipal);

  nsCOMPtr<nsIPrincipal> compareResultPrincipal =
      MakeHTTPPrincipalHTTPS(aResultPrincipal);

  return compareTriggeringPrincipal->Equals(compareResultPrincipal);
}

/*
 * Performs a Regular Expression match, optionally returning the results.
 * This function is not safe to use OMT.
 *
 * @param aPattern      The regex pattern
 * @param aString       The string to compare against
 * @param aOnlyMatch    Whether we want match results or only a true/false for
 * the match
 * @param aMatchResult  Out param for whether or not the pattern matched
 * @param aRegexResults Out param for the matches of the regex, if requested
 * @returns nsresult indicating correct function operation or error
 */
nsresult RegexEval(const nsAString& aPattern, const nsAString& aString,
                   bool aOnlyMatch, bool& aMatchResult,
                   nsTArray<nsString>* aRegexResults = nullptr) {
  MOZ_ASSERT(NS_IsMainThread());
  aMatchResult = false;

  mozilla::dom::AutoJSAPI jsapi;
  jsapi.Init();

  JSContext* cx = jsapi.cx();
  mozilla::AutoDisableJSInterruptCallback disabler(cx);

  // We can use the junk scope here, because we're just using it for regexp
  // evaluation, not actual script execution, and we disable statics so that the
  // evaluation does not interact with the execution global.
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  JS::RootedObject regexp(
      cx, JS::NewUCRegExpObject(cx, aPattern.BeginReading(), aPattern.Length(),
                                JS::RegExpFlag::Unicode));
  if (!regexp) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  JS::RootedValue regexResult(cx, JS::NullValue());

  size_t index = 0;
  if (!JS::ExecuteRegExpNoStatics(cx, regexp, aString.BeginReading(),
                                  aString.Length(), &index, aOnlyMatch,
                                  &regexResult)) {
    return NS_ERROR_FAILURE;
  }

  if (regexResult.isNull()) {
    // On no match, ExecuteRegExpNoStatics returns Null
    return NS_OK;
  }
  if (aOnlyMatch) {
    // On match, with aOnlyMatch = true, ExecuteRegExpNoStatics returns boolean
    // true.
    MOZ_ASSERT(regexResult.isBoolean() && regexResult.toBoolean());
    aMatchResult = true;
    return NS_OK;
  }
  if (aRegexResults == nullptr) {
    return NS_ERROR_INVALID_ARG;
  }

  // Now we know we have a result, and we need to extract it so we can read it.
  uint32_t length;
  JS::RootedObject regexResultObj(cx, &regexResult.toObject());
  if (!JS::GetArrayLength(cx, regexResultObj, &length)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  MOZ_LOG(sCSMLog, LogLevel::Verbose, ("Regex Matched %i strings", length));

  for (uint32_t i = 0; i < length; i++) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, regexResultObj, i, &element)) {
      return NS_ERROR_NO_CONTENT;
    }

    nsAutoJSString value;
    if (!value.init(cx, element)) {
      return NS_ERROR_NO_CONTENT;
    }

    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("Regex Matching: %i: %s", i, NS_ConvertUTF16toUTF8(value).get()));
    aRegexResults->AppendElement(value);
  }

  aMatchResult = true;
  return NS_OK;
}

/*
 * MOZ_CRASH_UNSAFE_PRINTF has a sPrintfCrashReasonSize-sized buffer. We need
 * to make sure we don't exceed it.  These functions perform this check and
 * munge things for us.
 *
 */

/*
 * Destructively truncates a string to fit within the limit
 */
char* nsContentSecurityUtils::SmartFormatCrashString(const char* str) {
  return nsContentSecurityUtils::SmartFormatCrashString(strdup(str));
}

char* nsContentSecurityUtils::SmartFormatCrashString(char* str) {
  auto str_len = strlen(str);

  if (str_len > sPrintfCrashReasonSize) {
    str[sPrintfCrashReasonSize - 1] = '\0';
    str_len = strlen(str);
  }
  MOZ_RELEASE_ASSERT(sPrintfCrashReasonSize > str_len);

  return str;
}

/*
 * Destructively truncates two strings to fit within the limit.
 * format_string is a format string containing two %s entries
 * The second string will be truncated to the _last_ 25 characters
 * The first string will be truncated to the remaining limit.
 */
nsCString nsContentSecurityUtils::SmartFormatCrashString(
    const char* part1, const char* part2, const char* format_string) {
  return SmartFormatCrashString(strdup(part1), strdup(part2), format_string);
}

nsCString nsContentSecurityUtils::SmartFormatCrashString(
    char* part1, char* part2, const char* format_string) {
  auto part1_len = strlen(part1);
  auto part2_len = strlen(part2);

  auto constant_len = strlen(format_string) - 4;

  if (part1_len + part2_len + constant_len > sPrintfCrashReasonSize) {
    if (part2_len > 25) {
      part2 += (part2_len - 25);
    }
    part2_len = strlen(part2);

    part1[sPrintfCrashReasonSize - (constant_len + part2_len + 1)] = '\0';
    part1_len = strlen(part1);
  }
  MOZ_RELEASE_ASSERT(sPrintfCrashReasonSize >
                     constant_len + part1_len + part2_len);

  auto parts = nsPrintfCString(format_string, part1, part2);
  return std::move(parts);
}

/*
 * Telemetry Events extra data only supports 80 characters, so we optimize the
 * filename to be smaller and collect more data.
 */
nsString OptimizeFileName(const nsAString& aFileName) {
  nsString optimizedName(aFileName);

  MOZ_LOG(
      sCSMLog, LogLevel::Verbose,
      ("Optimizing FileName: %s", NS_ConvertUTF16toUTF8(optimizedName).get()));

  optimizedName.ReplaceSubstring(u".xpi!"_ns, u"!"_ns);
  optimizedName.ReplaceSubstring(u"shield.mozilla.org!"_ns, u"s!"_ns);
  optimizedName.ReplaceSubstring(u"mozilla.org!"_ns, u"m!"_ns);
  if (optimizedName.Length() > 80) {
    optimizedName.Truncate(80);
  }

  MOZ_LOG(
      sCSMLog, LogLevel::Verbose,
      ("Optimized FileName: %s", NS_ConvertUTF16toUTF8(optimizedName).get()));
  return optimizedName;
}

/*
 * FilenameToFilenameType takes a fileName and returns a Pair of strings.
 * The First entry is a string indicating the type of fileName
 * The Second entry is a Maybe<string> that can contain additional details to
 * report.
 *
 * The reason we use strings (instead of an int/enum) is because the Telemetry
 * Events API only accepts strings.
 *
 * Function is a static member of the class to enable gtests.
 */

/* static */
FilenameTypeAndDetails nsContentSecurityUtils::FilenameToFilenameType(
    const nsString& fileName, bool collectAdditionalExtensionData) {
  // These are strings because the Telemetry Events API only accepts strings
  static constexpr auto kChromeURI = "chromeuri"_ns;
  static constexpr auto kResourceURI = "resourceuri"_ns;
  static constexpr auto kBlobUri = "bloburi"_ns;
  static constexpr auto kDataUri = "dataurl"_ns;
  static constexpr auto kDataUriWebExtCStyle =
      "dataurl-extension-contentstyle"_ns;
  static constexpr auto kSingleString = "singlestring"_ns;
  static constexpr auto kMozillaExtension = "mozillaextension"_ns;
  static constexpr auto kOtherExtension = "otherextension"_ns;
  static constexpr auto kSuspectedUserChromeJS = "suspectedUserChromeJS"_ns;
#if defined(XP_WIN)
  static constexpr auto kSanitizedWindowsURL = "sanitizedWindowsURL"_ns;
  static constexpr auto kSanitizedWindowsPath = "sanitizedWindowsPath"_ns;
#endif
  static constexpr auto kOther = "other"_ns;
  static constexpr auto kOtherWorker = "other-on-worker"_ns;
  static constexpr auto kRegexFailure = "regexfailure"_ns;

  static constexpr auto kUCJSRegex = u"(.+).uc.js\\?*[0-9]*$"_ns;
  static constexpr auto kExtensionRegex = u"extensions/(.+)@(.+)!(.+)$"_ns;
  static constexpr auto kSingleFileRegex = u"^[a-zA-Z0-9.?]+$"_ns;

  if (fileName.IsEmpty()) {
    return FilenameTypeAndDetails(kOther, Nothing());
  }

  // resource:// and chrome://
  if (StringBeginsWith(fileName, u"chrome://"_ns)) {
    return FilenameTypeAndDetails(kChromeURI, Some(fileName));
  }
  if (StringBeginsWith(fileName, u"resource://"_ns)) {
    return FilenameTypeAndDetails(kResourceURI, Some(fileName));
  }

  // blob: and data:
  if (StringBeginsWith(fileName, u"blob:"_ns)) {
    return FilenameTypeAndDetails(kBlobUri, Nothing());
  }
  if (StringBeginsWith(fileName, u"data:text/css;extension=style;"_ns)) {
    return FilenameTypeAndDetails(kDataUriWebExtCStyle, Nothing());
  }
  if (StringBeginsWith(fileName, u"data:"_ns)) {
    return FilenameTypeAndDetails(kDataUri, Nothing());
  }

  if (!NS_IsMainThread()) {
    // We can't do Regex matching off the main thread; so just report.
    return FilenameTypeAndDetails(kOtherWorker, Nothing());
  }

  // Extension
  bool regexMatch;
  nsTArray<nsString> regexResults;
  nsresult rv = RegexEval(kExtensionRegex, fileName, /* aOnlyMatch = */ false,
                          regexMatch, &regexResults);
  if (NS_FAILED(rv)) {
    return FilenameTypeAndDetails(kRegexFailure, Nothing());
  }
  if (regexMatch) {
    nsCString type = StringEndsWith(regexResults[2], u"mozilla.org.xpi"_ns)
                         ? kMozillaExtension
                         : kOtherExtension;
    auto& extensionNameAndPath =
        Substring(regexResults[0], ArrayLength("extensions/") - 1);
    return FilenameTypeAndDetails(type,
                                  Some(OptimizeFileName(extensionNameAndPath)));
  }

  // Single File
  rv = RegexEval(kSingleFileRegex, fileName, /* aOnlyMatch = */ true,
                 regexMatch);
  if (NS_FAILED(rv)) {
    return FilenameTypeAndDetails(kRegexFailure, Nothing());
  }
  if (regexMatch) {
    return FilenameTypeAndDetails(kSingleString, Some(fileName));
  }

  // Suspected userChromeJS script
  rv = RegexEval(kUCJSRegex, fileName, /* aOnlyMatch = */ true, regexMatch);
  if (NS_FAILED(rv)) {
    return FilenameTypeAndDetails(kRegexFailure, Nothing());
  }
  if (regexMatch) {
    return FilenameTypeAndDetails(kSuspectedUserChromeJS, Nothing());
  }

#if defined(XP_WIN)
  auto flags = mozilla::widget::WinUtils::PathTransformFlags::Default |
               mozilla::widget::WinUtils::PathTransformFlags::RequireFilePath;
  nsAutoString strSanitizedPath(fileName);
  if (widget::WinUtils::PreparePathForTelemetry(strSanitizedPath, flags)) {
    DWORD cchDecodedUrl = INTERNET_MAX_URL_LENGTH;
    WCHAR szOut[INTERNET_MAX_URL_LENGTH];
    HRESULT hr;
    SAFECALL_URLMON_FUNC(CoInternetParseUrl, fileName.get(), PARSE_SCHEMA, 0,
                         szOut, INTERNET_MAX_URL_LENGTH, &cchDecodedUrl, 0);
    if (hr == S_OK && cchDecodedUrl) {
      nsAutoString sanitizedPathAndScheme;
      sanitizedPathAndScheme.Append(szOut);
      if (sanitizedPathAndScheme == u"about"_ns) {
        int32_t desired_length = fileName.Length();
        int32_t possible_new_length = 0;

        possible_new_length = fileName.FindChar('?');
        if (possible_new_length != -1 && possible_new_length < desired_length) {
          desired_length = possible_new_length;
        }

        possible_new_length = fileName.FindChar('#');
        if (possible_new_length != -1 && possible_new_length < desired_length) {
          desired_length = possible_new_length;
        }

        sanitizedPathAndScheme = Substring(fileName, 0, desired_length);
      } else if (sanitizedPathAndScheme == u"file"_ns) {
        sanitizedPathAndScheme.Append(u"://.../"_ns);
        sanitizedPathAndScheme.Append(strSanitizedPath);
      } else if (sanitizedPathAndScheme == u"moz-extension"_ns &&
                 collectAdditionalExtensionData) {
        sanitizedPathAndScheme.Append(u"://["_ns);

        nsCOMPtr<nsIURI> uri;
        nsresult rv = NS_NewURI(getter_AddRefs(uri), fileName);
        if (NS_FAILED(rv)) {
          // Return after adding ://[ so we know we failed here.
          return FilenameTypeAndDetails(kSanitizedWindowsURL,
                                        Some(sanitizedPathAndScheme));
        }

        mozilla::extensions::URLInfo url(uri);
        if (NS_IsMainThread()) {
          // EPS is only usable on main thread
          auto* policy =
              ExtensionPolicyService::GetSingleton().GetByHost(url.Host());
          if (policy) {
            nsString addOnId;
            policy->GetId(addOnId);

            sanitizedPathAndScheme.Append(addOnId);
            sanitizedPathAndScheme.Append(u": "_ns);
            sanitizedPathAndScheme.Append(policy->Name());
          } else {
            sanitizedPathAndScheme.Append(u"failed finding addon by host"_ns);
          }
        } else {
          sanitizedPathAndScheme.Append(u"can't get addon off main thread"_ns);
        }
        sanitizedPathAndScheme.Append(u"]"_ns);
        sanitizedPathAndScheme.Append(url.FilePath());
      }
      return FilenameTypeAndDetails(kSanitizedWindowsURL,
                                    Some(sanitizedPathAndScheme));
    } else {
      return FilenameTypeAndDetails(kSanitizedWindowsPath,
                                    Some(strSanitizedPath));
    }
  }
#endif

  return FilenameTypeAndDetails(kOther, Nothing());
}

#ifdef NIGHTLY_BUILD
// Crash String must be safe from a telemetry point of view.
// This will be ensured when this function is used.
void PossiblyCrash(const char* pref_suffix, const nsCString crash_string) {
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {
    // We only crash in the parent (unfortunately) because it's
    // the only place we can be sure that our only-crash-once
    // pref-writing works.
    return;
  }

  nsCString previous_crashes("security.crash_tracking.");
  previous_crashes.Append(pref_suffix);
  previous_crashes.Append(".prevCrashes");

  nsCString max_crashes("security.crash_tracking.");
  max_crashes.Append(pref_suffix);
  max_crashes.Append(".maxCrashes");

  int32_t numberOfPreviousCrashes = 0;
  numberOfPreviousCrashes = Preferences::GetInt(previous_crashes.get(), 0);

  int32_t maxAllowableCrashes = 0;
  maxAllowableCrashes = Preferences::GetInt(max_crashes.get(), 0);

  if (numberOfPreviousCrashes >= maxAllowableCrashes) {
    return;
  }

  nsresult rv =
      Preferences::SetInt(previous_crashes.get(), ++numberOfPreviousCrashes);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIPrefService> prefsCom = Preferences::GetService();
  Preferences* prefs = static_cast<Preferences*>(prefsCom.get());

  if (!prefs->AllowOffMainThreadSave()) {
    // Do not crash if we can't save prefs off the main thread
    return;
  }

  rv = prefs->SavePrefFileBlocking();
  if (!NS_FAILED(rv)) {
    MOZ_CRASH_UNSAFE_PRINTF("%s", nsContentSecurityUtils::SmartFormatCrashString(crash_string.get()));
  }
}
#endif

class EvalUsageNotificationRunnable final : public Runnable {
 public:
  EvalUsageNotificationRunnable(bool aIsSystemPrincipal,
                                NS_ConvertUTF8toUTF16& aFileNameA,
                                uint64_t aWindowID, uint32_t aLineNumber,
                                uint32_t aColumnNumber)
      : mozilla::Runnable("EvalUsageNotificationRunnable"),
        mIsSystemPrincipal(aIsSystemPrincipal),
        mFileNameA(aFileNameA),
        mWindowID(aWindowID),
        mLineNumber(aLineNumber),
        mColumnNumber(aColumnNumber) {}

  NS_IMETHOD Run() override {
    nsContentSecurityUtils::NotifyEvalUsage(
        mIsSystemPrincipal, mFileNameA, mWindowID, mLineNumber, mColumnNumber);
    return NS_OK;
  }

  void Revoke() {}

 private:
  bool mIsSystemPrincipal;
  NS_ConvertUTF8toUTF16 mFileNameA;
  uint64_t mWindowID;
  uint32_t mLineNumber;
  uint32_t mColumnNumber;
};

/* static */
bool nsContentSecurityUtils::IsEvalAllowed(JSContext* cx,
                                           bool aIsSystemPrincipal,
                                           const nsAString& aScript) {
  // This allowlist contains files that are permanently allowed to use
  // eval()-like functions. It will ideally be restricted to files that are
  // exclusively used in testing contexts.
  static nsLiteralCString evalAllowlist[] = {
      // Test-only third-party library
      "resource://testing-common/sinon-7.2.7.js"_ns,
      // Test-only third-party library
      "resource://testing-common/ajv-4.1.1.js"_ns,
      // Test-only utility
      "resource://testing-common/content-task.js"_ns,

      // Tracked by Bug 1584605
      "resource:///modules/translation/cld-worker.js"_ns,

      // require.js implements a script loader for workers. It uses eval
      // to load the script; but injection is only possible in situations
      // that you could otherwise control script that gets executed, so
      // it is okay to allow eval() as it adds no additional attack surface.
      // Bug 1584564 tracks requiring safe usage of require.js
      "resource://gre/modules/workers/require.js"_ns,

      // The Browser Toolbox/Console
      "debugger"_ns,
  };

  // We also permit two specific idioms in eval()-like contexts. We'd like to
  // elminate these too; but there are in-the-wild Mozilla privileged extensions
  // that use them.
  static constexpr auto sAllowedEval1 = u"this"_ns;
  static constexpr auto sAllowedEval2 =
      u"function anonymous(\n) {\nreturn this\n}"_ns;

  if (MOZ_LIKELY(!aIsSystemPrincipal && !XRE_IsE10sParentProcess())) {
    // We restrict eval in the system principal and parent process.
    // Other uses (like web content and null principal) are allowed.
    return true;
  }

  if (JS::ContextOptionsRef(cx).disableEvalSecurityChecks()) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing eval() because this JSContext was set to allow it"));
    return true;
  }

  if (aIsSystemPrincipal &&
      StaticPrefs::security_allow_eval_with_system_principal()) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing eval() with System Principal because allowing pref is "
             "enabled"));
    return true;
  }

  if (XRE_IsE10sParentProcess() &&
      StaticPrefs::security_allow_eval_in_parent_process()) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing eval() in parent process because allowing pref is "
             "enabled"));
    return true;
  }

  DetectJsHacks();
  if (MOZ_UNLIKELY(sJSHacksPresent)) {
    MOZ_LOG(
        sCSMLog, LogLevel::Debug,
        ("Allowing eval() %s because some "
         "JS hacks may be present.",
         (aIsSystemPrincipal ? "with System Principal" : "in parent process")));
    return true;
  }

  if (XRE_IsE10sParentProcess() &&
      !StaticPrefs::extensions_webextensions_remote()) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing eval() in parent process because the web extension "
             "process is disabled"));
    return true;
  }

  // We permit these two common idioms to get access to the global JS object
  if (!aScript.IsEmpty() &&
      (aScript == sAllowedEval1 || aScript == sAllowedEval2)) {
    MOZ_LOG(
        sCSMLog, LogLevel::Debug,
        ("Allowing eval() %s because a key string is "
         "provided",
         (aIsSystemPrincipal ? "with System Principal" : "in parent process")));
    return true;
  }

  // Check the allowlist for the provided filename. getFilename is a helper
  // function
  nsAutoCString fileName;
  uint32_t lineNumber = 0, columnNumber = 0;
  nsJSUtils::GetCallingLocation(cx, fileName, &lineNumber, &columnNumber);
  if (fileName.IsEmpty()) {
    fileName = "unknown-file"_ns;
  }

  NS_ConvertUTF8toUTF16 fileNameA(fileName);
  for (const nsLiteralCString& allowlistEntry : evalAllowlist) {
    // checking if current filename begins with entry, because JS Engine
    // gives us additional stuff for code inside eval or Function ctor
    // e.g., "require.js > Function"
    if (StringBeginsWith(fileName, allowlistEntry)) {
      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("Allowing eval() %s because the containing "
               "file is in the allowlist",
               (aIsSystemPrincipal ? "with System Principal"
                                   : "in parent process")));
      return true;
    }
  }

  // Send Telemetry and Log to the Console
  uint64_t windowID = nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(cx);
  if (NS_IsMainThread()) {
    nsContentSecurityUtils::NotifyEvalUsage(aIsSystemPrincipal, fileNameA,
                                            windowID, lineNumber, columnNumber);
  } else {
    auto runnable = new EvalUsageNotificationRunnable(
        aIsSystemPrincipal, fileNameA, windowID, lineNumber, columnNumber);
    NS_DispatchToMainThread(runnable);
  }

  // Log to MOZ_LOG
  MOZ_LOG(sCSMLog, LogLevel::Warning,
          ("Blocking eval() %s from file %s and script "
           "provided %s",
           (aIsSystemPrincipal ? "with System Principal" : "in parent process"),
           fileName.get(), NS_ConvertUTF16toUTF8(aScript).get()));

  // Maybe Crash
#if defined(DEBUG)
  auto crashString = nsContentSecurityUtils::SmartFormatCrashString(
      NS_ConvertUTF16toUTF8(aScript).get(), fileName.get(),
      (aIsSystemPrincipal
           ? "Blocking eval() with System Principal with script %s from file %s"
           : "Blocking eval() in parent process with script %s from file %s"));
  MOZ_CRASH_UNSAFE_PRINTF("%s", crashString.get());
#endif

  return false;
}

/* static */
void nsContentSecurityUtils::NotifyEvalUsage(bool aIsSystemPrincipal,
                                             NS_ConvertUTF8toUTF16& aFileNameA,
                                             uint64_t aWindowID,
                                             uint32_t aLineNumber,
                                             uint32_t aColumnNumber) {
  // Send Telemetry
  Telemetry::EventID eventType =
      aIsSystemPrincipal ? Telemetry::EventID::Security_Evalusage_Systemcontext
                         : Telemetry::EventID::Security_Evalusage_Parentprocess;

  FilenameTypeAndDetails fileNameTypeAndDetails =
      FilenameToFilenameType(aFileNameA, false);
  mozilla::Maybe<nsTArray<EventExtraEntry>> extra;
  if (fileNameTypeAndDetails.second.isSome()) {
    extra = Some<nsTArray<EventExtraEntry>>({EventExtraEntry{
        "fileinfo"_ns,
        NS_ConvertUTF16toUTF8(fileNameTypeAndDetails.second.value())}});
  } else {
    extra = Nothing();
  }
  if (!sTelemetryEventEnabled.exchange(true)) {
    sTelemetryEventEnabled = true;
    Telemetry::SetEventRecordingEnabled("security"_ns, true);
  }
  Telemetry::RecordEvent(eventType, mozilla::Some(fileNameTypeAndDetails.first),
                         extra);

  // Report an error to console
  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) {
    return;
  }
  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  if (!error) {
    return;
  }
  nsCOMPtr<nsIStringBundle> bundle;
  nsCOMPtr<nsIStringBundleService> stringService =
      mozilla::components::StringBundle::Service();
  if (!stringService) {
    return;
  }
  stringService->CreateBundle(
      "chrome://global/locale/security/security.properties",
      getter_AddRefs(bundle));
  if (!bundle) {
    return;
  }
  nsAutoString message;
  AutoTArray<nsString, 1> formatStrings = {aFileNameA};
  nsresult rv = bundle->FormatStringFromName("RestrictBrowserEvalUsage",
                                             formatStrings, message);
  if (NS_FAILED(rv)) {
    return;
  }

  rv = error->InitWithWindowID(message, aFileNameA, u""_ns, aLineNumber,
                               aColumnNumber, nsIScriptError::errorFlag,
                               "BrowserEvalUsage", aWindowID,
                               true /* From chrome context */);
  if (NS_FAILED(rv)) {
    return;
  }
  console->LogMessage(error);
}

/* static */
void nsContentSecurityUtils::DetectJsHacks() {
  // We can only perform the check of this preference on the Main Thread
  // (because a String-based preference check is only safe on Main Thread.)
  // In theory, it would be possible that a separate thread could get here
  // before the main thread, resulting in the other thread not being able to
  // perform this check, but the odds of that are small (and probably zero.)
  if (!NS_IsMainThread()) {
    return;
  }
  // No need to check again.
  if (MOZ_LIKELY(sJSHacksChecked)) {
    return;
  }
  // This preference is a file used for autoconfiguration of Firefox
  // by administrators. It has also been (ab)used by the userChromeJS
  // project to run legacy-style 'extensions', some of which use eval,
  // all of which run in the System Principal context.
  nsAutoString jsConfigPref;
  Preferences::GetString("general.config.filename", jsConfigPref);
  if (!jsConfigPref.IsEmpty()) {
    sJSHacksPresent = true;
  }

  // This preference is required by bootstrapLoader.xpi, which is an
  // alternate way to load legacy-style extensions. It only works on
  // DevEdition/Nightly.
  bool xpinstallSignatures;
  Preferences::GetBool("xpinstall.signatures.required", &xpinstallSignatures);
  if (!xpinstallSignatures) {
    sJSHacksPresent = true;
  }

  sJSHacksChecked = true;
}

/* static */
nsresult nsContentSecurityUtils::GetHttpChannelFromPotentialMultiPart(
    nsIChannel* aChannel, nsIHttpChannel** aHttpChannel) {
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    httpChannel.forget(aHttpChannel);
    return NS_OK;
  }

  nsCOMPtr<nsIMultiPartChannel> multipart = do_QueryInterface(aChannel);
  if (!multipart) {
    *aHttpChannel = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> baseChannel;
  nsresult rv = multipart->GetBaseChannel(getter_AddRefs(baseChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  httpChannel = do_QueryInterface(baseChannel);
  httpChannel.forget(aHttpChannel);

  return NS_OK;
}

nsresult ParseCSPAndEnforceFrameAncestorCheck(
    nsIChannel* aChannel, nsIContentSecurityPolicy** aOutCSP) {
  MOZ_ASSERT(aChannel);

  // CSP can only hang off an http channel, if this channel is not
  // an http channel then there is nothing to do here.
  nsCOMPtr<nsIHttpChannel> httpChannel;
  nsresult rv = nsContentSecurityUtils::GetHttpChannelFromPotentialMultiPart(
      aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!httpChannel) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  ExtContentPolicyType contentType = loadInfo->GetExternalContentPolicyType();
  // frame-ancestor check only makes sense for subdocument and object loads,
  // if this is not a load of such type, there is nothing to do here.
  if (contentType != ExtContentPolicy::TYPE_SUBDOCUMENT &&
      contentType != ExtContentPolicy::TYPE_OBJECT) {
    return NS_OK;
  }

  nsAutoCString tCspHeaderValue, tCspROHeaderValue;

  Unused << httpChannel->GetResponseHeader("content-security-policy"_ns,
                                           tCspHeaderValue);

  Unused << httpChannel->GetResponseHeader(
      "content-security-policy-report-only"_ns, tCspROHeaderValue);

  // if there are no CSP values, then there is nothing to do here.
  if (tCspHeaderValue.IsEmpty() && tCspROHeaderValue.IsEmpty()) {
    return NS_OK;
  }

  NS_ConvertASCIItoUTF16 cspHeaderValue(tCspHeaderValue);
  NS_ConvertASCIItoUTF16 cspROHeaderValue(tCspROHeaderValue);

  RefPtr<nsCSPContext> csp = new nsCSPContext();
  nsCOMPtr<nsIPrincipal> resultPrincipal;
  rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      aChannel, getter_AddRefs(resultPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> selfURI;
  aChannel->GetURI(getter_AddRefs(selfURI));

  nsCOMPtr<nsIReferrerInfo> referrerInfo = httpChannel->GetReferrerInfo();
  nsAutoString referrerSpec;
  if (referrerInfo) {
    referrerInfo->GetComputedReferrerSpec(referrerSpec);
  }
  uint64_t innerWindowID = loadInfo->GetInnerWindowID();

  rv = csp->SetRequestContextWithPrincipal(resultPrincipal, selfURI,
                                           referrerSpec, innerWindowID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // ----- if there's a full-strength CSP header, apply it.
  if (!cspHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(csp, cspHeaderValue, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ----- if there's a report-only CSP header, apply it.
  if (!cspROHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(csp, cspROHeaderValue, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ----- Enforce frame-ancestor policy on any applied policies
  bool safeAncestry = false;
  // PermitsAncestry sends violation reports when necessary
  rv = csp->PermitsAncestry(loadInfo, &safeAncestry);

  if (NS_FAILED(rv) || !safeAncestry) {
    // stop!  ERROR page!
    aChannel->Cancel(NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION);
    return NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION;
  }

  // return the CSP for x-frame-options check
  csp.forget(aOutCSP);

  return NS_OK;
}

void EnforceXFrameOptionsCheck(nsIChannel* aChannel,
                               nsIContentSecurityPolicy* aCsp) {
  MOZ_ASSERT(aChannel);
  if (!FramingChecker::CheckFrameOptions(aChannel, aCsp)) {
    // stop!  ERROR page!
    aChannel->Cancel(NS_ERROR_XFO_VIOLATION);
  }
}

/* static */
void nsContentSecurityUtils::PerformCSPFrameAncestorAndXFOCheck(
    nsIChannel* aChannel) {
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsresult rv =
      ParseCSPAndEnforceFrameAncestorCheck(aChannel, getter_AddRefs(csp));
  if (NS_FAILED(rv)) {
    return;
  }

  // X-Frame-Options needs to be enforced after CSP frame-ancestors
  // checks because if frame-ancestors is present, then x-frame-options
  // will be discarded
  EnforceXFrameOptionsCheck(aChannel, csp);
}

#if defined(DEBUG)
/* static */
void nsContentSecurityUtils::AssertAboutPageHasCSP(Document* aDocument) {
  // We want to get to a point where all about: pages ship with a CSP. This
  // assertion ensures that we can not deploy new about: pages without a CSP.
  // Please note that any about: page should not use inline JS or inline CSS,
  // and instead should load JS and CSS from an external file (*.js, *.css)
  // which allows us to apply a strong CSP omitting 'unsafe-inline'. Ideally,
  // the CSP allows precisely the resources that need to be loaded; but it
  // should at least be as strong as:
  // <meta http-equiv="Content-Security-Policy" content="default-src chrome:;
  // object-src 'none'"/>

  // Check if we should skip the assertion
  if (StaticPrefs::dom_security_skip_about_page_has_csp_assert()) {
    return;
  }

  // Check if we are loading an about: URI at all
  nsCOMPtr<nsIURI> documentURI = aDocument->GetDocumentURI();
  if (!documentURI->SchemeIs("about")) {
    return;
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp = aDocument->GetCsp();
  bool foundDefaultSrc = false;
  bool foundObjectSrc = false;
  bool foundUnsafeEval = false;
  bool foundUnsafeInline = false;
  bool foundScriptSrc = false;
  bool foundWorkerSrc = false;
  bool foundWebScheme = false;
  if (csp) {
    uint32_t policyCount = 0;
    csp->GetPolicyCount(&policyCount);
    nsAutoString parsedPolicyStr;
    for (uint32_t i = 0; i < policyCount; ++i) {
      csp->GetPolicyString(i, parsedPolicyStr);
      if (parsedPolicyStr.Find("default-src") >= 0) {
        foundDefaultSrc = true;
      }
      if (parsedPolicyStr.Find("object-src 'none'") >= 0) {
        foundObjectSrc = true;
      }
      if (parsedPolicyStr.Find("'unsafe-eval'") >= 0) {
        foundUnsafeEval = true;
      }
      if (parsedPolicyStr.Find("'unsafe-inline'") >= 0) {
        foundUnsafeInline = true;
      }
      if (parsedPolicyStr.Find("script-src") >= 0) {
        foundScriptSrc = true;
      }
      if (parsedPolicyStr.Find("worker-src") >= 0) {
        foundWorkerSrc = true;
      }
      if (parsedPolicyStr.Find("http:") >= 0 ||
          parsedPolicyStr.Find("https:") >= 0) {
        foundWebScheme = true;
      }
    }
  }

  // Check if we should skip the allowlist and assert right away. Please note
  // that this pref can and should only be set for automated testing.
  if (StaticPrefs::dom_security_skip_about_page_csp_allowlist_and_assert()) {
    NS_ASSERTION(foundDefaultSrc, "about: page must have a CSP");
    return;
  }

  nsAutoCString aboutSpec;
  documentURI->GetSpec(aboutSpec);
  ToLowerCase(aboutSpec);

  // This allowlist contains about: pages that are permanently allowed to
  // render without a CSP applied.
  static nsLiteralCString sAllowedAboutPagesWithNoCSP[] = {
    // about:blank is a special about page -> no CSP
    "about:blank"_ns,
    // about:srcdoc is a special about page -> no CSP
    "about:srcdoc"_ns,
    // about:sync-log displays plain text only -> no CSP
    "about:sync-log"_ns,
    // about:printpreview displays plain text only -> no CSP
    "about:printpreview"_ns,
    // about:logo just displays the firefox logo -> no CSP
    "about:logo"_ns,
#  if defined(ANDROID)
    "about:config"_ns,
#  endif
  };

  for (const nsLiteralCString& allowlistEntry : sAllowedAboutPagesWithNoCSP) {
    // please note that we perform a substring match here on purpose,
    // so we don't have to deal and parse out all the query arguments
    // the various about pages rely on.
    if (StringBeginsWith(aboutSpec, allowlistEntry)) {
      return;
    }
  }

  MOZ_ASSERT(foundDefaultSrc,
             "about: page must contain a CSP including default-src");
  MOZ_ASSERT(foundObjectSrc,
             "about: page must contain a CSP denying object-src");

  // preferences and downloads allow legacy inline scripts through hash src.
  MOZ_ASSERT(!foundScriptSrc ||
                 StringBeginsWith(aboutSpec, "about:preferences"_ns) ||
                 StringBeginsWith(aboutSpec, "about:downloads"_ns) ||
                 StringBeginsWith(aboutSpec, "about:newtab"_ns) ||
                 StringBeginsWith(aboutSpec, "about:logins"_ns) ||
                 StringBeginsWith(aboutSpec, "about:compat"_ns) ||
                 StringBeginsWith(aboutSpec, "about:welcome"_ns) ||
                 StringBeginsWith(aboutSpec, "about:profiling"_ns) ||
                 StringBeginsWith(aboutSpec, "about:studies"_ns) ||
                 StringBeginsWith(aboutSpec, "about:home"_ns),
             "about: page must not contain a CSP including script-src");

  MOZ_ASSERT(!foundWorkerSrc,
             "about: page must not contain a CSP including worker-src");

  // addons, preferences, debugging, ion, devtools all have to allow some
  // remote web resources
  MOZ_ASSERT(!foundWebScheme ||
                 StringBeginsWith(aboutSpec, "about:preferences"_ns) ||
                 StringBeginsWith(aboutSpec, "about:addons"_ns) ||
                 StringBeginsWith(aboutSpec, "about:newtab"_ns) ||
                 StringBeginsWith(aboutSpec, "about:debugging"_ns) ||
                 StringBeginsWith(aboutSpec, "about:ion"_ns) ||
                 StringBeginsWith(aboutSpec, "about:compat"_ns) ||
                 StringBeginsWith(aboutSpec, "about:logins"_ns) ||
                 StringBeginsWith(aboutSpec, "about:home"_ns) ||
                 StringBeginsWith(aboutSpec, "about:welcome"_ns) ||
                 StringBeginsWith(aboutSpec, "about:devtools"_ns) ||
                 StringBeginsWith(aboutSpec, "about:pocket-saved"_ns),
             "about: page must not contain a CSP including a web scheme");

  if (aDocument->IsExtensionPage()) {
    // Extensions have two CSP policies applied where the baseline CSP
    // includes 'unsafe-eval' and 'unsafe-inline', hence we have to skip
    // the 'unsafe-eval' and 'unsafe-inline' assertions for extension
    // pages.
    return;
  }

  MOZ_ASSERT(!foundUnsafeEval,
             "about: page must not contain a CSP including 'unsafe-eval'");

  static nsLiteralCString sLegacyUnsafeInlineAllowList[] = {
      // Bug 1579160: Remove 'unsafe-inline' from style-src within
      // about:preferences
      "about:preferences"_ns,
      // Bug 1571346: Remove 'unsafe-inline' from style-src within about:addons
      "about:addons"_ns,
      // Bug 1584485: Remove 'unsafe-inline' from style-src within:
      // * about:newtab
      // * about:welcome
      // * about:home
      "about:newtab"_ns,
      "about:welcome"_ns,
      "about:home"_ns,
  };

  for (const nsLiteralCString& aUnsafeInlineEntry :
       sLegacyUnsafeInlineAllowList) {
    // please note that we perform a substring match here on purpose,
    // so we don't have to deal and parse out all the query arguments
    // the various about pages rely on.
    if (StringBeginsWith(aboutSpec, aUnsafeInlineEntry)) {
      return;
    }
  }

  MOZ_ASSERT(!foundUnsafeInline,
             "about: page must not contain a CSP including 'unsafe-inline'");
}
#endif

/* static */
bool nsContentSecurityUtils::ValidateScriptFilename(const char* aFilename,
                                                    bool aIsSystemRealm) {
  // If the pref is permissive, allow everything
  if (StaticPrefs::security_allow_parent_unrestricted_js_loads()) {
    return true;
  }

  // If we're not in the parent process allow everything (presently)
  if (!XRE_IsE10sParentProcess()) {
    return true;
  }

  // If we have allowed eval (because of a user configuration or more
  // likely a test has requested it), and the script is an eval, allow it.
  NS_ConvertUTF8toUTF16 filenameU(aFilename);
  if (StaticPrefs::security_allow_eval_with_system_principal() ||
      StaticPrefs::security_allow_eval_in_parent_process()) {
    if (StringEndsWith(filenameU, u"> eval"_ns)) {
      return true;
    }
  }

  DetectJsHacks();

  if (MOZ_UNLIKELY(sJSHacksPresent)) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing a javascript load of %s because "
             "some JS hacks may be present",
             aFilename));
    return true;
  }

  if (XRE_IsE10sParentProcess() &&
      !StaticPrefs::extensions_webextensions_remote()) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing a javascript load of %s because the web extension "
             "process is disabled.",
             aFilename));
    return true;
  }

  if (StringBeginsWith(filenameU, u"chrome://"_ns)) {
    // If it's a chrome:// url, allow it
    return true;
  }
  if (StringBeginsWith(filenameU, u"resource://"_ns)) {
    // If it's a resource:// url, allow it
    return true;
  }
  if (StringBeginsWith(filenameU, u"file://"_ns)) {
    // We will temporarily allow all file:// URIs through for now
    return true;
  }
  if (StringBeginsWith(filenameU, u"jar:file://"_ns)) {
    // We will temporarily allow all jar URIs through for now
    return true;
  }
  if (filenameU.Equals(u"about:sync-log"_ns)) {
    // about:sync-log runs in the parent process and displays a directory
    // listing. The listing has inline javascript that executes on load.
    return true;
  }

  bool bergamont_disabled =
      Preferences::GetBool("extensions.translations.disabled", true);
  if (!bergamont_disabled &&
      StringBeginsWith(filenameU, u"moz-extension://"_ns)) {
    // Allow all moz-extensions through if bergamont is enabled; because there
    // seem to be multiple bergamont extensions with different names.
    return true;
  }

  // Log to MOZ_LOG
  MOZ_LOG(sCSMLog, LogLevel::Info,
          ("ValidateScriptFilename System:%i %s\n", (aIsSystemRealm ? 1 : 0),
           aFilename));

  // Send Telemetry
  FilenameTypeAndDetails fileNameTypeAndDetails =
      FilenameToFilenameType(filenameU, true);

  Telemetry::EventID eventType =
      Telemetry::EventID::Security_Javascriptload_Parentprocess;

  mozilla::Maybe<nsTArray<EventExtraEntry>> extra;
  if (fileNameTypeAndDetails.second.isSome()) {
    extra = Some<nsTArray<EventExtraEntry>>({EventExtraEntry{
        "fileinfo"_ns,
        NS_ConvertUTF16toUTF8(fileNameTypeAndDetails.second.value())}});
  } else {
    extra = Nothing();
  }

  if (!sTelemetryEventEnabled.exchange(true)) {
    sTelemetryEventEnabled = true;
    Telemetry::SetEventRecordingEnabled("security"_ns, true);
  }
  Telemetry::RecordEvent(eventType, mozilla::Some(fileNameTypeAndDetails.first),
                         extra);

#ifdef NIGHTLY_BUILD
  // Cause a crash (if we've never crashed before and we can ensure we won't do
  // it again.)
  // The details in the second arg, passed to UNSAFE_PRINTF, are also included
  // in Event Telemetry and have received data review.
  if (fileNameTypeAndDetails.second.isSome()) {
    PossiblyCrash("js_load_1",
                  NS_ConvertUTF16toUTF8(fileNameTypeAndDetails.second.value()));
  } else {
    PossiblyCrash("js_load_1", "(None)"_ns);
  }
#endif

  // Presently we are not enforcing any restrictions for the script filename,
  // we're only reporting Telemetry. In the future we will assert in debug
  // builds and return false to prevent execution in non-debug builds.
  return true;
}

/* static */
void nsContentSecurityUtils::LogMessageToConsole(nsIHttpChannel* aChannel,
                                                 const char* aMsg) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return;
  }

  uint64_t windowID = 0;
  rv = aChannel->GetTopLevelContentWindowId(&windowID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  if (!windowID) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    loadInfo->GetInnerWindowID(&windowID);
  }

  nsAutoString localizedMsg;
  nsAutoCString spec;
  uri->GetSpec(spec);
  AutoTArray<nsString, 1> params = {NS_ConvertUTF8toUTF16(spec)};
  rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eSECURITY_PROPERTIES, aMsg, params, localizedMsg);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsContentUtils::ReportToConsoleByWindowID(
      localizedMsg, nsIScriptError::warningFlag, "Security"_ns, windowID, uri);
}

/* static */
long nsContentSecurityUtils::ClassifyDownload(
    nsIChannel* aChannel, const nsAutoCString& aMimeTypeGuess) {
  MOZ_ASSERT(aChannel, "IsDownloadAllowed without channel?");

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  nsCOMPtr<nsIURI> contentLocation;
  aChannel->GetURI(getter_AddRefs(contentLocation));

  nsCOMPtr<nsIPrincipal> loadingPrincipal = loadInfo->GetLoadingPrincipal();
  if (!loadingPrincipal) {
    loadingPrincipal = loadInfo->TriggeringPrincipal();
  }
  // Creating a fake Loadinfo that is just used for the MCB check.
  nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new mozilla::net::LoadInfo(
      loadingPrincipal, loadInfo->TriggeringPrincipal(), nullptr,
      nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
      nsIContentPolicy::TYPE_FETCH);

  int16_t decission = nsIContentPolicy::ACCEPT;
  nsMixedContentBlocker::ShouldLoad(false,  //  aHadInsecureImageRedirect
                                    contentLocation,   //  aContentLocation,
                                    secCheckLoadInfo,  //  aLoadinfo
                                    aMimeTypeGuess,    //  aMimeGuess,
                                    false,             //  aReportError
                                    &decission         // aDecision
  );
  Telemetry::Accumulate(mozilla::Telemetry::MIXED_CONTENT_DOWNLOADS,
                        decission != nsIContentPolicy::ACCEPT);

  if (StaticPrefs::dom_block_download_insecure() &&
      decission != nsIContentPolicy::ACCEPT) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
    if (httpChannel) {
      LogMessageToConsole(httpChannel, "MixedContentBlockedDownload");
    }
    return nsITransfer::DOWNLOAD_POTENTIALLY_UNSAFE;
  }

  if (loadInfo->TriggeringPrincipal()->IsSystemPrincipal()) {
    return nsITransfer::DOWNLOAD_ACCEPTABLE;
  }

  if (!StaticPrefs::dom_block_download_in_sandboxed_iframes()) {
    return nsITransfer::DOWNLOAD_ACCEPTABLE;
  }

  uint32_t triggeringFlags = loadInfo->GetTriggeringSandboxFlags();
  uint32_t currentflags = loadInfo->GetSandboxFlags();

  if ((triggeringFlags & SANDBOXED_ALLOW_DOWNLOADS) ||
      (currentflags & SANDBOXED_ALLOW_DOWNLOADS)) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
    if (httpChannel) {
      LogMessageToConsole(httpChannel, "IframeSandboxBlockedDownload");
    }
    return nsITransfer::DOWNLOAD_FORBIDDEN;
  }

  return nsITransfer::DOWNLOAD_ACCEPTABLE;
}
