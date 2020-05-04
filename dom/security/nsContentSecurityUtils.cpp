/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static content security utilities. */

#include "nsContentSecurityUtils.h"

#include "nsIContentSecurityPolicy.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIURI.h"
#if defined(XP_WIN)
#  include "WinUtils.h"
#  include <wininet.h>
#endif

#include "js/Array.h"  // JS::GetArrayLength
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/Document.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/StaticPrefs_dom.h"

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

  AutoJSAPI jsapi;
  jsapi.Init();

  JSContext* cx = jsapi.cx();
  AutoDisableJSInterruptCallback disabler(cx);

  JSAutoRealm ar(cx, xpc::UnprivilegedJunkScope());

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
 * Telemetry Events extra data only supports 80 characters, so we optimize the
 * filename to be smaller and collect more data.
 */
nsString OptimizeFileName(const nsAString& aFileName) {
  nsString optimizedName(aFileName);

  MOZ_LOG(
      sCSMLog, LogLevel::Verbose,
      ("Optimizing FileName: %s", NS_ConvertUTF16toUTF8(optimizedName).get()));

  optimizedName.ReplaceSubstring(NS_LITERAL_STRING(".xpi!"),
                                 NS_LITERAL_STRING("!"));
  optimizedName.ReplaceSubstring(NS_LITERAL_STRING("shield.mozilla.org!"),
                                 NS_LITERAL_STRING("s!"));
  optimizedName.ReplaceSubstring(NS_LITERAL_STRING("mozilla.org!"),
                                 NS_LITERAL_STRING("m!"));
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
  static NS_NAMED_LITERAL_CSTRING(kChromeURI, "chromeuri");
  static NS_NAMED_LITERAL_CSTRING(kResourceURI, "resourceuri");
  static NS_NAMED_LITERAL_CSTRING(kBlobUri, "bloburi");
  static NS_NAMED_LITERAL_CSTRING(kDataUri, "dataurl");
  static NS_NAMED_LITERAL_CSTRING(kSingleString, "singlestring");
  static NS_NAMED_LITERAL_CSTRING(kMozillaExtension, "mozillaextension");
  static NS_NAMED_LITERAL_CSTRING(kOtherExtension, "otherextension");
  static NS_NAMED_LITERAL_CSTRING(kSuspectedUserChromeJS,
                                  "suspectedUserChromeJS");
#if defined(XP_WIN)
  static NS_NAMED_LITERAL_CSTRING(kSanitizedWindowsURL, "sanitizedWindowsURL");
  static NS_NAMED_LITERAL_CSTRING(kSanitizedWindowsPath,
                                  "sanitizedWindowsPath");
#endif
  static NS_NAMED_LITERAL_CSTRING(kOther, "other");
  static NS_NAMED_LITERAL_CSTRING(kOtherWorker, "other-on-worker");
  static NS_NAMED_LITERAL_CSTRING(kRegexFailure, "regexfailure");

  static NS_NAMED_LITERAL_STRING(kUCJSRegex, "(.+).uc.js\\?*[0-9]*$");
  static NS_NAMED_LITERAL_STRING(kExtensionRegex, "extensions/(.+)@(.+)!(.+)$");
  static NS_NAMED_LITERAL_STRING(kSingleFileRegex, "^[a-zA-Z0-9.?]+$");

  // resource:// and chrome://
  if (StringBeginsWith(fileName, NS_LITERAL_STRING("chrome://"))) {
    return FilenameTypeAndDetails(kChromeURI, Some(fileName));
  }
  if (StringBeginsWith(fileName, NS_LITERAL_STRING("resource://"))) {
    return FilenameTypeAndDetails(kResourceURI, Some(fileName));
  }

  // blob: and data:
  if (StringBeginsWith(fileName, NS_LITERAL_STRING("blob:"))) {
    return FilenameTypeAndDetails(kBlobUri, Nothing());
  }
  if (StringBeginsWith(fileName, NS_LITERAL_STRING("data:"))) {
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
    nsCString type =
        StringEndsWith(regexResults[2], NS_LITERAL_STRING("mozilla.org.xpi"))
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
    HRESULT hr =
        ::CoInternetParseUrl(fileName.get(), PARSE_SCHEMA, 0, szOut,
                             INTERNET_MAX_URL_LENGTH, &cchDecodedUrl, 0);
    if (hr == S_OK && cchDecodedUrl) {
      nsAutoString sanitizedPathAndScheme;
      sanitizedPathAndScheme.Append(szOut);
      if (sanitizedPathAndScheme == NS_LITERAL_STRING("file")) {
        sanitizedPathAndScheme.Append(NS_LITERAL_STRING("://.../"));
        sanitizedPathAndScheme.Append(strSanitizedPath);
      } else if (sanitizedPathAndScheme == NS_LITERAL_STRING("moz-extension") &&
                 collectAdditionalExtensionData) {
        sanitizedPathAndScheme.Append(NS_LITERAL_STRING("://["));

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
            sanitizedPathAndScheme.Append(NS_LITERAL_STRING(": "));
            sanitizedPathAndScheme.Append(policy->Name());
          } else {
            sanitizedPathAndScheme.Append(
                NS_LITERAL_STRING("failed finding addon by host"));
          }
        } else {
          sanitizedPathAndScheme.Append(
              NS_LITERAL_STRING("can't get addon off main thread"));
        }
        sanitizedPathAndScheme.Append(NS_LITERAL_STRING("]"));
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

// The Web Extension process pref may be toggled during a session, at which
// point stuff may be loaded in the parent process but we would send telemetry
// for it. Avoid this by observing if the pref ever was disabled.
static bool sWebExtensionsRemoteWasEverDisabled = false;

/* static */
bool nsContentSecurityUtils::IsEvalAllowed(JSContext* cx,
                                           bool aIsSystemPrincipal,
                                           const nsAString& aScript) {
  // This allowlist contains files that are permanently allowed to use
  // eval()-like functions. It will ideally be restricted to files that are
  // exclusively used in testing contexts.
  static nsLiteralCString evalAllowlist[] = {
      // Test-only third-party library
      NS_LITERAL_CSTRING("resource://testing-common/sinon-7.2.7.js"),
      // Test-only third-party library
      NS_LITERAL_CSTRING("resource://testing-common/ajv-4.1.1.js"),
      // Test-only utility
      NS_LITERAL_CSTRING("resource://testing-common/content-task.js"),

      // Tracked by Bug 1584605
      NS_LITERAL_CSTRING("resource:///modules/translation/cld-worker.js"),

      // require.js implements a script loader for workers. It uses eval
      // to load the script; but injection is only possible in situations
      // that you could otherwise control script that gets executed, so
      // it is okay to allow eval() as it adds no additional attack surface.
      // Bug 1584564 tracks requiring safe usage of require.js
      NS_LITERAL_CSTRING("resource://gre/modules/workers/require.js"),

      // The Browser Toolbox/Console
      NS_LITERAL_CSTRING("debugger"),
  };

  // We also permit two specific idioms in eval()-like contexts. We'd like to
  // elminate these too; but there are in-the-wild Mozilla privileged extensions
  // that use them.
  static NS_NAMED_LITERAL_STRING(sAllowedEval1, "this");
  static NS_NAMED_LITERAL_STRING(sAllowedEval2,
                                 "function anonymous(\n) {\nreturn this\n}");

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

  // We only perform a check of this preference on the Main Thread
  // (because a String-based preference check is only safe on Main Thread.)
  // The consequence of this is that if a user is using userChromeJS _and_
  // the scripts they use start a worker and that worker uses eval - we will
  // enter this function, skip over this pref check that would normally cause
  // us to allow the eval usage - and we will block it.
  // While not ideal, we do not officially support userChromeJS, and hopefully
  // the usage of workers and eval in workers is even lower that userChromeJS
  // usage.
  if (NS_IsMainThread()) {
    // This preference is a file used for autoconfiguration of Firefox
    // by administrators. It has also been (ab)used by the userChromeJS
    // project to run legacy-style 'extensions', some of which use eval,
    // all of which run in the System Principal context.
    nsAutoString jsConfigPref;
    Preferences::GetString("general.config.filename", jsConfigPref);
    if (!jsConfigPref.IsEmpty()) {
      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("Allowing eval() %s because of "
               "general.config.filename",
               (aIsSystemPrincipal ? "with System Principal"
                                   : "in parent process")));
      return true;
    }
  }

  if (XRE_IsE10sParentProcess() &&
      !StaticPrefs::extensions_webextensions_remote()) {
    sWebExtensionsRemoteWasEverDisabled = true;
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing eval() in parent process because the web extension "
             "process is disabled"));
    return true;
  }
  if (XRE_IsE10sParentProcess() && sWebExtensionsRemoteWasEverDisabled) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing eval() in parent process because the web extension "
             "process was disabled at some point"));
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
  JS::AutoFilename rawScriptFilename;
  if (JS::DescribeScriptedCaller(cx, &rawScriptFilename, &lineNumber,
                                 &columnNumber)) {
    nsDependentCSubstring fileName_(rawScriptFilename.get(),
                                    strlen(rawScriptFilename.get()));
    ToLowerCase(fileName_);
    // Extract file name alone if scriptFilename contains line number
    // separated by multiple space delimiters in few cases.
    int32_t fileNameIndex = fileName_.FindChar(' ');
    if (fileNameIndex != -1) {
      fileName_.SetLength(fileNameIndex);
    }

    fileName = std::move(fileName_);
  } else {
    fileName = NS_LITERAL_CSTRING("unknown-file");
  }

  NS_ConvertUTF8toUTF16 fileNameA(fileName);
  for (const nsLiteralCString& allowlistEntry : evalAllowlist) {
    if (fileName.Equals(allowlistEntry)) {
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
#ifdef DEBUG
  MOZ_CRASH_UNSAFE_PRINTF(
      "Blocking eval() %s from file %s and script provided "
      "%s",
      (aIsSystemPrincipal ? "with System Principal" : "in parent process"),
      fileName.get(), NS_ConvertUTF16toUTF8(aScript).get());
#endif

  return true;
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
        NS_LITERAL_CSTRING("fileinfo"),
        NS_ConvertUTF16toUTF8(fileNameTypeAndDetails.second.value())}});
  } else {
    extra = Nothing();
  }
  if (!sTelemetryEventEnabled.exchange(true)) {
    sTelemetryEventEnabled = true;
    Telemetry::SetEventRecordingEnabled(NS_LITERAL_CSTRING("security"), true);
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
      mozilla::services::GetStringBundleService();
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

  rv = error->InitWithWindowID(message, aFileNameA, EmptyString(), aLineNumber,
                               aColumnNumber, nsIScriptError::errorFlag,
                               "BrowserEvalUsage", aWindowID,
                               true /* From chrome context */);
  if (NS_FAILED(rv)) {
    return;
  }
  console->LogMessage(error);
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
  nsContentPolicyType contentType = loadInfo->GetExternalContentPolicyType();
  // frame-ancestor check only makes sense for subdocument and object loads,
  // if this is not a load of such type, there is nothing to do here.
  if (contentType != nsIContentPolicy::TYPE_SUBDOCUMENT &&
      contentType != nsIContentPolicy::TYPE_OBJECT) {
    return NS_OK;
  }

  nsAutoCString tCspHeaderValue, tCspROHeaderValue;

  Unused << httpChannel->GetResponseHeader(
      NS_LITERAL_CSTRING("content-security-policy"), tCspHeaderValue);

  Unused << httpChannel->GetResponseHeader(
      NS_LITERAL_CSTRING("content-security-policy-report-only"),
      tCspROHeaderValue);

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
    NS_LITERAL_CSTRING("about:blank"),
    // about:srcdoc is a special about page -> no CSP
    NS_LITERAL_CSTRING("about:srcdoc"),
    // about:sync-log displays plain text only -> no CSP
    NS_LITERAL_CSTRING("about:sync-log"),
    // about:printpreview displays plain text only -> no CSP
    NS_LITERAL_CSTRING("about:printpreview"),
    // about:logo just displays the firefox logo -> no CSP
    NS_LITERAL_CSTRING("about:logo"),
#  if defined(ANDROID)
    NS_LITERAL_CSTRING("about:config"),
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
      NS_LITERAL_CSTRING("about:preferences"),
      // Bug 1571346: Remove 'unsafe-inline' from style-src within about:addons
      NS_LITERAL_CSTRING("about:addons"),
      // Bug 1584485: Remove 'unsafe-inline' from style-src within:
      // * about:newtab
      // * about:welcome
      // * about:home
      NS_LITERAL_CSTRING("about:newtab"),
      NS_LITERAL_CSTRING("about:welcome"),
      NS_LITERAL_CSTRING("about:home"),
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
  static Maybe<bool> sGeneralConfigFilenameSet;
  // If the pref is permissive, allow everything
  if (StaticPrefs::security_allow_parent_unrestricted_js_loads()) {
    return true;
  }

  // If we're not in the parent process allow everything (presently)
  if (!XRE_IsE10sParentProcess()) {
    return true;
  }

  // We only perform a check of this preference on the Main Thread
  // (because a String-based preference check is only safe on Main Thread.)
  // The consequence of this is that if a user is using userChromeJS _and_
  // the scripts they use start a worker - we will enter this function,
  // skip over this pref check that would normally cause us to allow the
  // load - and we will block it.
  // While not ideal, we do not officially support userChromeJS, and hopefully
  // the usage of workers is even lower than userChromeJS usage.
  if (NS_IsMainThread()) {
    // This preference is a file used for autoconfiguration of Firefox
    // by administrators. It will also run in the parent process and throw
    // assumptions about what can run where out of the window.
    if (!sGeneralConfigFilenameSet.isSome()) {
      nsAutoString jsConfigPref;
      Preferences::GetString("general.config.filename", jsConfigPref);
      sGeneralConfigFilenameSet.emplace(!jsConfigPref.IsEmpty());
    }
    if (sGeneralConfigFilenameSet.value()) {
      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("Allowing a javascript load of %s because "
               "general.config.filename is set",
               aFilename));
      return true;
    }
  }

  if (XRE_IsE10sParentProcess() &&
      !StaticPrefs::extensions_webextensions_remote()) {
    sWebExtensionsRemoteWasEverDisabled = true;
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing a javascript load of %s because the web extension "
             "process is disabled.",
             aFilename));
    return true;
  }
  if (XRE_IsE10sParentProcess() && sWebExtensionsRemoteWasEverDisabled) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing a javascript load of %s because the web extension "
             "process was disabled at some point.",
             aFilename));
    return true;
  }

  NS_ConvertUTF8toUTF16 filenameU(aFilename);
  if (StringBeginsWith(filenameU, NS_LITERAL_STRING("chrome://"))) {
    // If it's a chrome:// url, allow it
    return true;
  }
  if (StringBeginsWith(filenameU, NS_LITERAL_STRING("resource://"))) {
    // If it's a resource:// url, allow it
    return true;
  }
  if (StringBeginsWith(filenameU, NS_LITERAL_STRING("file://"))) {
    // We will temporarily allow all file:// URIs through for now
    return true;
  }
  if (StringBeginsWith(filenameU, NS_LITERAL_STRING("jar:file://"))) {
    // We will temporarily allow all jar URIs through for now
    return true;
  }
  if (filenameU.Equals(NS_LITERAL_STRING("about:sync-log"))) {
    // about:sync-log runs in the parent process and displays a directory
    // listing. The listing has inline javascript that executes on load.
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
        NS_LITERAL_CSTRING("fileinfo"),
        NS_ConvertUTF16toUTF8(fileNameTypeAndDetails.second.value())}});
  } else {
    extra = Nothing();
  }

  if (!sTelemetryEventEnabled.exchange(true)) {
    sTelemetryEventEnabled = true;
    Telemetry::SetEventRecordingEnabled(NS_LITERAL_CSTRING("security"), true);
  }
  Telemetry::RecordEvent(eventType, mozilla::Some(fileNameTypeAndDetails.first),
                         extra);

  // Presently we are not enforcing any restrictions for the script filename,
  // we're only reporting Telemetry. In the future we will assert in debug
  // builds and return false to prevent execution in non-debug builds.
  return true;
}
