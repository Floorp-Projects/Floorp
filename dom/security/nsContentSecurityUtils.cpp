/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static content security utilities. */

#include "nsContentSecurityUtils.h"

#include "nsIContentSecurityPolicy.h"
#include "nsIURI.h"

#include "mozilla/dom/Document.h"

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
  if (!JS_GetArrayLength(cx, regexResultObj, &length)) {
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
 * FilenameToEvalType takes a fileName and returns a Pair of strings.
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
FilenameType nsContentSecurityUtils::FilenameToEvalType(
    const nsString& fileName) {
  // These are strings because the Telemetry Events API only accepts strings
  static NS_NAMED_LITERAL_CSTRING(kChromeURI, "chromeuri");
  static NS_NAMED_LITERAL_CSTRING(kResourceURI, "resourceuri");
  static NS_NAMED_LITERAL_CSTRING(kSingleString, "singlestring");
  static NS_NAMED_LITERAL_CSTRING(kMozillaExtension, "mozillaextension");
  static NS_NAMED_LITERAL_CSTRING(kOtherExtension, "otherextension");
  static NS_NAMED_LITERAL_CSTRING(kSuspectedUserChromeJS,
                                  "suspectedUserChromeJS");
  static NS_NAMED_LITERAL_CSTRING(kOther, "other");
  static NS_NAMED_LITERAL_CSTRING(kOtherWorker, "other-on-worker");
  static NS_NAMED_LITERAL_CSTRING(kRegexFailure, "regexfailure");

  static NS_NAMED_LITERAL_STRING(kUCJSRegex, "(.+).uc.js\\?*[0-9]*$");
  static NS_NAMED_LITERAL_STRING(kExtensionRegex, "extensions/(.+)@(.+)!(.+)$");
  static NS_NAMED_LITERAL_STRING(kSingleFileRegex, "^[a-zA-Z0-9.?]+$");

  // resource:// and chrome://
  if (StringBeginsWith(fileName, NS_LITERAL_STRING("chrome://"))) {
    return FilenameType(kChromeURI, Some(fileName));
  }
  if (StringBeginsWith(fileName, NS_LITERAL_STRING("resource://"))) {
    return FilenameType(kResourceURI, Some(fileName));
  }

  if (!NS_IsMainThread()) {
    // We can't do Regex matching off the main thread; so just report.
    return FilenameType(kOtherWorker, Nothing());
  }

  // Extension
  bool regexMatch;
  nsTArray<nsString> regexResults;
  nsresult rv = RegexEval(kExtensionRegex, fileName, /* aOnlyMatch = */ false,
                          regexMatch, &regexResults);
  if (NS_FAILED(rv)) {
    return FilenameType(kRegexFailure, Nothing());
  }
  if (regexMatch) {
    nsCString type =
        StringEndsWith(regexResults[2], NS_LITERAL_STRING("mozilla.org.xpi"))
            ? kMozillaExtension
            : kOtherExtension;
    auto& extensionNameAndPath =
        Substring(regexResults[0], ArrayLength("extensions/") - 1);
    return FilenameType(type, Some(OptimizeFileName(extensionNameAndPath)));
  }

  // Single File
  rv = RegexEval(kSingleFileRegex, fileName, /* aOnlyMatch = */ true,
                 regexMatch);
  if (NS_FAILED(rv)) {
    return FilenameType(kRegexFailure, Nothing());
  }
  if (regexMatch) {
    return FilenameType(kSingleString, Some(fileName));
  }

  // Suspected userChromeJS script
  rv = RegexEval(kUCJSRegex, fileName, /* aOnlyMatch = */ true, regexMatch);
  if (NS_FAILED(rv)) {
    return FilenameType(kRegexFailure, Nothing());
  }
  if (regexMatch) {
    return FilenameType(kSuspectedUserChromeJS, Nothing());
  }

  return FilenameType(kOther, Nothing());
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

  if (aIsSystemPrincipal &&
      StaticPrefs::security_allow_eval_with_system_principal()) {
    MOZ_LOG(
        sCSMLog, LogLevel::Debug,
        ("Allowing eval() %s because allowing pref is "
         "enabled",
         (aIsSystemPrincipal ? "with System Principal" : "in parent process")));
    return true;
  }

  if (XRE_IsE10sParentProcess() &&
      StaticPrefs::security_allow_eval_in_parent_process()) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing eval() in parent process because allowing pref is "
             "enabled"));
    return true;
  }

  if (!aIsSystemPrincipal && !XRE_IsE10sParentProcess()) {
    // Usage of eval we are unconcerned with.
    return true;
  }

  // We only perform checks of these two preferences on the Main Thread
  // (because a String-based preference checks is only safe on Main Thread.)
  // The consequence of this is that if a user is using userChromeJS _and_
  // the scripts they use start a worker and that worker uses eval - we will
  // enter this function, skip over these pref checks that would normally cause
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

    // This preference is better known as userchrome.css which allows
    // customization of the Firefox UI. Believe it or not, you can also
    // use XBL bindings to get it to run Javascript in the same manner
    // as userChromeJS above, so even though 99.9% of people using
    // userchrome.css aren't doing that, we're still going to need to
    // disable the eval() assertion for them.
    if (Preferences::GetBool(
            "toolkit.legacyUserProfileCustomizations.stylesheets")) {
      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("Allowing eval() %s because of "
               "toolkit.legacyUserProfileCustomizations.stylesheets",
               (aIsSystemPrincipal ? "with System Principal"
                                   : "in parent process")));
      return true;
    }
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

  // Do not enforce eval usage blocking on Worker threads; because this is
  // new behavior and we want to be conservative so we don't accidently break
  // Nightly. Bug 1584602 will enforce things.
  return !NS_IsMainThread();
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

  FilenameType fileNameType = FilenameToEvalType(aFileNameA);
  mozilla::Maybe<nsTArray<EventExtraEntry>> extra;
  if (fileNameType.second().isSome()) {
    extra = Some<nsTArray<EventExtraEntry>>({EventExtraEntry{
        NS_LITERAL_CSTRING("fileinfo"),
        NS_ConvertUTF16toUTF8(fileNameType.second().value())}});
  } else {
    extra = Nothing();
  }
  if (!sTelemetryEventEnabled.exchange(true)) {
    sTelemetryEventEnabled = true;
    Telemetry::SetEventRecordingEnabled(NS_LITERAL_CSTRING("security"), true);
  }
  Telemetry::RecordEvent(eventType, mozilla::Some(fileNameType.first()), extra);

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
  if (Preferences::GetBool("csp.skip_about_page_has_csp_assert")) {
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
  if (Preferences::GetBool("csp.skip_about_page_csp_allowlist_and_assert")) {
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
