/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArray.h"
#include "nsContentSecurityManager.h"
#include "nsEscape.h"
#include "nsDataHandler.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsINode.h"
#include "nsIStreamListener.h"
#include "nsILoadInfo.h"
#include "nsIOService.h"
#include "nsContentUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsIStreamListener.h"
#include "nsIURIFixup.h"
#include "nsIImageLoadingContent.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsReadableUtils.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/Components.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"
#include "xpcpublic.h"

#include "jsapi.h"
#include "js/RegExp.h"

using namespace mozilla::Telemetry;

NS_IMPL_ISUPPORTS(nsContentSecurityManager, nsIContentSecurityManager,
                  nsIChannelEventSink)

static mozilla::LazyLogModule sCSMLog("CSMLog");

static Atomic<bool, mozilla::Relaxed> sTelemetryEventEnabled(false);

/* static */
bool nsContentSecurityManager::AllowTopLevelNavigationToDataURI(
    nsIChannel* aChannel) {
  // Let's block all toplevel document navigations to a data: URI.
  // In all cases where the toplevel document is navigated to a
  // data: URI the triggeringPrincipal is a contentPrincipal, or
  // a NullPrincipal. In other cases, e.g. typing a data: URL into
  // the URL-Bar, the triggeringPrincipal is a SystemPrincipal;
  // we don't want to block those loads. Only exception, loads coming
  // from an external applicaton (e.g. Thunderbird) don't load
  // using a contentPrincipal, but we want to block those loads.
  if (!mozilla::net::nsIOService::BlockToplevelDataUriNavigations()) {
    return true;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (loadInfo->GetExternalContentPolicyType() !=
      nsIContentPolicy::TYPE_DOCUMENT) {
    return true;
  }
  if (loadInfo->GetForceAllowDataURI()) {
    // if the loadinfo explicitly allows the data URI navigation, let's allow it
    // now
    return true;
  }
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, true);
  bool isDataURI = uri->SchemeIs("data");
  if (!isDataURI) {
    return true;
  }

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, true);
  nsAutoCString contentType;
  bool base64;
  rv = nsDataHandler::ParseURI(spec, contentType, nullptr, base64, nullptr);
  NS_ENSURE_SUCCESS(rv, true);

  // Whitelist data: images as long as they are not SVGs
  if (StringBeginsWith(contentType, NS_LITERAL_CSTRING("image/")) &&
      !contentType.EqualsLiteral("image/svg+xml")) {
    return true;
  }
  // Whitelist all plain text types as well as data: PDFs.
  if (nsContentUtils::IsPlainTextType(contentType) ||
      contentType.EqualsLiteral("application/pdf")) {
    return true;
  }
  // Redirecting to a toplevel data: URI is not allowed, hence we make
  // sure the RedirectChain is empty.
  if (!loadInfo->GetLoadTriggeredFromExternal() &&
      nsContentUtils::IsSystemPrincipal(loadInfo->TriggeringPrincipal()) &&
      loadInfo->RedirectChain().IsEmpty()) {
    return true;
  }
  nsAutoCString dataSpec;
  uri->GetSpec(dataSpec);
  if (dataSpec.Length() > 50) {
    dataSpec.Truncate(50);
    dataSpec.AppendLiteral("...");
  }
  nsCOMPtr<nsISupports> context = loadInfo->ContextForTopLevelLoad();
  nsCOMPtr<nsIBrowserChild> browserChild = do_QueryInterface(context);
  nsCOMPtr<Document> doc;
  if (browserChild) {
    doc = static_cast<mozilla::dom::BrowserChild*>(browserChild.get())
              ->GetTopLevelDocument();
  }
  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(NS_UnescapeURL(dataSpec), *params.AppendElement());
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("DATA_URI_BLOCKED"), doc,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  "BlockTopLevelDataURINavigation", params);
  return false;
}

/* static */
bool nsContentSecurityManager::AllowInsecureRedirectToDataURI(
    nsIChannel* aNewChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aNewChannel->LoadInfo();
  if (loadInfo->GetExternalContentPolicyType() !=
      nsIContentPolicy::TYPE_SCRIPT) {
    return true;
  }
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  if (NS_FAILED(rv) || !newURI) {
    return true;
  }
  bool isDataURI = newURI->SchemeIs("data");
  if (!isDataURI) {
    return true;
  }

  // Web Extensions are exempt from that restriction and are allowed to redirect
  // a channel to a data: URI. When a web extension redirects a channel, we set
  // a flag on the loadInfo which allows us to identify such redirects here.
  if (loadInfo->GetAllowInsecureRedirectToDataURI()) {
    return true;
  }

  nsAutoCString dataSpec;
  newURI->GetSpec(dataSpec);
  if (dataSpec.Length() > 50) {
    dataSpec.Truncate(50);
    dataSpec.AppendLiteral("...");
  }
  nsCOMPtr<Document> doc;
  nsINode* node = loadInfo->LoadingNode();
  if (node) {
    doc = node->OwnerDoc();
  }
  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(NS_UnescapeURL(dataSpec), *params.AppendElement());
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("DATA_URI_BLOCKED"), doc,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  "BlockSubresourceRedirectToData", params);
  return false;
}

/*
 * Performs a Regular Expression match, optionally returning the results.
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
FilenameType nsContentSecurityManager::FilenameToEvalType(
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

/* static */
void nsContentSecurityManager::AssertEvalNotRestricted(
    JSContext* cx, nsIPrincipal* aSubjectPrincipal, const nsAString& aScript) {
  // This allowlist contains files that are permanently allowed to use
  // eval()-like functions. It is supposed to be restricted to files that are
  // exclusively used in testing contexts.
  static nsLiteralCString evalAllowlist[] = {
      // Test-only third-party library
      NS_LITERAL_CSTRING("resource://testing-common/sinon-7.2.7.js"),
      // Test-only third-party library
      NS_LITERAL_CSTRING("resource://testing-common/ajv-4.1.1.js"),
      // Test-only utility
      NS_LITERAL_CSTRING("resource://testing-common/content-task.js"),

      // The Browser Toolbox/Console
      NS_LITERAL_CSTRING("debugger"),

      // The following files are NOT supposed to stay on this whitelist.
      // Bug numbers indicate planned removal of each file.

      // Bug 1498560
      NS_LITERAL_CSTRING("chrome://global/content/bindings/autocomplete.xml"),
  };

  // We also permit two specific idioms in eval()-like contexts. We'd like to
  // elminate these too; but there are in-the-wild Mozilla privileged extensions
  // that use them.
  static NS_NAMED_LITERAL_STRING(sAllowedEval1, "this");
  static NS_NAMED_LITERAL_STRING(sAllowedEval2,
                                 "function anonymous(\n) {\nreturn this\n}");

  bool systemPrincipal = aSubjectPrincipal->IsSystemPrincipal();
  if (systemPrincipal &&
      StaticPrefs::security_allow_eval_with_system_principal()) {
    MOZ_LOG(
        sCSMLog, LogLevel::Debug,
        ("Allowing eval() %s because allowing pref is "
         "enabled",
         (systemPrincipal ? "with System Principal" : "in parent process")));
    return;
  }

  if (XRE_IsE10sParentProcess() &&
      StaticPrefs::security_allow_eval_in_parent_process()) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("Allowing eval() in parent process because allowing pref is "
             "enabled"));
    return;
  }

  if (!systemPrincipal && !XRE_IsE10sParentProcess()) {
    // Usage of eval we are unconcerned with.
    return;
  }

  // This preference is a file used for autoconfiguration of Firefox
  // by administrators. It has also been (ab)used by the userChromeJS
  // project to run legacy-style 'extensions', some of which use eval,
  // all of which run in the System Principal context.
  nsAutoString jsConfigPref;
  Preferences::GetString("general.config.filename", jsConfigPref);
  if (!jsConfigPref.IsEmpty()) {
    MOZ_LOG(
        sCSMLog, LogLevel::Debug,
        ("Allowing eval() %s because of "
         "general.config.filename",
         (systemPrincipal ? "with System Principal" : "in parent process")));
    return;
  }

  // This preference is better known as userchrome.css which allows
  // customization of the Firefox UI. Believe it or not, you can also
  // use XBL bindings to get it to run Javascript in the same manner
  // as userChromeJS above, so even though 99.9% of people using
  // userchrome.css aren't doing that, we're still going to need to
  // disable the eval() assertion for them.
  if (Preferences::GetBool(
          "toolkit.legacyUserProfileCustomizations.stylesheets")) {
    MOZ_LOG(
        sCSMLog, LogLevel::Debug,
        ("Allowing eval() %s because of "
         "toolkit.legacyUserProfileCustomizations.stylesheets",
         (systemPrincipal ? "with System Principal" : "in parent process")));
    return;
  }

  // We permit these two common idioms to get access to the global JS object
  if (!aScript.IsEmpty() &&
      (aScript == sAllowedEval1 || aScript == sAllowedEval2)) {
    MOZ_LOG(
        sCSMLog, LogLevel::Debug,
        ("Allowing eval() %s because a key string is "
         "provided",
         (systemPrincipal ? "with System Principal" : "in parent process")));
    return;
  }

  // Check the allowlist for the provided filename. getFilename is a helper
  // function
  auto getFilename = [](JSContext* cx) -> const nsCString {
    JS::AutoFilename scriptFilename;
    if (JS::DescribeScriptedCaller(cx, &scriptFilename)) {
      nsDependentCSubstring fileName_(scriptFilename.get(),
                                      strlen(scriptFilename.get()));
      ToLowerCase(fileName_);
      // Extract file name alone if scriptFilename contains line number
      // separated by multiple space delimiters in few cases.
      int32_t fileNameIndex = fileName_.FindChar(' ');
      if (fileNameIndex != -1) {
        fileName_.SetLength(fileNameIndex);
      }

      nsAutoCString fileName(fileName_);
      return std::move(fileName);
    }
    return NS_LITERAL_CSTRING("unknown-file");
  };

  nsCString fileName = getFilename(cx);
  for (const nsLiteralCString& allowlistEntry : evalAllowlist) {
    if (fileName.Equals(allowlistEntry)) {
      MOZ_LOG(
          sCSMLog, LogLevel::Debug,
          ("Allowing eval() %s because the containing "
           "file is in the allowlist",
           (systemPrincipal ? "with System Principal" : "in parent process")));
      return;
    }
  }

  // Send Telemetry
  Telemetry::EventID eventType =
      systemPrincipal ? Telemetry::EventID::Security_Evalusage_Systemcontext
                      : Telemetry::EventID::Security_Evalusage_Parentprocess;

  FilenameType fileNameType =
      FilenameToEvalType(NS_ConvertUTF8toUTF16(fileName));
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

  // Crash or Log
#ifdef DEBUG
  MOZ_CRASH_UNSAFE_PRINTF(
      "Blocking eval() %s from file %s and script provided "
      "%s",
      (systemPrincipal ? "with System Principal" : "in parent process"),
      fileName.get(), NS_ConvertUTF16toUTF8(aScript).get());
#else
  MOZ_LOG(sCSMLog, LogLevel::Warning,
          ("Blocking eval() %s from file %s and script "
           "provided %s",
           (systemPrincipal ? "with System Principal" : "in parent process"),
           fileName.get(), NS_ConvertUTF16toUTF8(aScript).get()));
#endif

  // In the future, we will change this function to return false and abort JS
  // execution without crashing the process. For now, just collect data.
}

/* static */
nsresult nsContentSecurityManager::CheckFTPSubresourceLoad(
    nsIChannel* aChannel) {
  // We dissallow using FTP resources as a subresource everywhere.
  // The only valid way to use FTP resources is loading it as
  // a top level document.

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsContentPolicyType type = loadInfo->GetExternalContentPolicyType();

  // Allow top-level FTP documents and save-as download of FTP files on
  // HTTP pages.
  if (type == nsIContentPolicy::TYPE_DOCUMENT ||
      type == nsIContentPolicy::TYPE_SAVEAS_DOWNLOAD) {
    return NS_OK;
  }

  // Allow the system principal to load everything. This is meant to
  // temporarily fix downloads and pdf.js.
  nsIPrincipal* triggeringPrincipal = loadInfo->TriggeringPrincipal();
  if (nsContentUtils::IsSystemPrincipal(triggeringPrincipal)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!uri) {
    return NS_OK;
  }

  bool isFtpURI = uri->SchemeIs("ftp");
  if (!isFtpURI) {
    return NS_OK;
  }

  nsCOMPtr<Document> doc;
  if (nsINode* node = loadInfo->LoadingNode()) {
    doc = node->OwnerDoc();
  }

  nsAutoCString spec;
  uri->GetSpec(spec);
  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(NS_UnescapeURL(spec), *params.AppendElement());

  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("FTP_URI_BLOCKED"), doc,
      nsContentUtils::eSECURITY_PROPERTIES, "BlockSubresourceFTP", params);

  return NS_ERROR_CONTENT_BLOCKED;
}

static nsresult ValidateSecurityFlags(nsILoadInfo* aLoadInfo) {
  nsSecurityFlags securityMode = aLoadInfo->GetSecurityMode();

  // We should never perform a security check on a loadInfo that uses the flag
  // SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK, because that is only used for
  // temporary loadInfos used for explicit nsIContentPolicy checks, but never be
  // set as a security flag on an actual channel.
  if (securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS &&
      securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
      securityMode != nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    MOZ_ASSERT(
        false,
        "need one securityflag from nsILoadInfo to perform security checks");
    return NS_ERROR_FAILURE;
  }

  // all good, found the right security flags
  return NS_OK;
}

static bool IsImageLoadInEditorAppType(nsILoadInfo* aLoadInfo) {
  // Editor apps get special treatment here, editors can load images
  // from anywhere.  This allows editor to insert images from file://
  // into documents that are being edited.
  nsContentPolicyType type = aLoadInfo->InternalContentPolicyType();
  if (type != nsIContentPolicy::TYPE_INTERNAL_IMAGE &&
      type != nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD &&
      type != nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON &&
      type != nsIContentPolicy::TYPE_IMAGESET) {
    return false;
  }

  auto appType = nsIDocShell::APP_TYPE_UNKNOWN;
  nsINode* node = aLoadInfo->LoadingNode();
  if (!node) {
    return false;
  }
  Document* doc = node->OwnerDoc();
  if (!doc) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = doc->GetDocShell();
  if (!docShellTreeItem) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> root;
  docShellTreeItem->GetInProcessRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(root));
  if (docShell) {
    appType = docShell->GetAppType();
  }

  return appType == nsIDocShell::APP_TYPE_EDITOR;
}

static nsresult DoCheckLoadURIChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo) {
  // In practice, these DTDs are just used for localization, so applying the
  // same principal check as Fluent.
  if (aLoadInfo->InternalContentPolicyType() ==
      nsIContentPolicy::TYPE_INTERNAL_DTD) {
    return nsContentUtils::PrincipalAllowsL10n(aLoadInfo->TriggeringPrincipal())
               ? NS_OK
               : NS_ERROR_DOM_BAD_URI;
  }

  // This is used in order to allow a privileged DOMParser to parse documents
  // that need to access localization DTDs. We just allow through
  // TYPE_INTERNAL_FORCE_ALLOWED_DTD no matter what the triggering principal is.
  if (aLoadInfo->InternalContentPolicyType() ==
      nsIContentPolicy::TYPE_INTERNAL_FORCE_ALLOWED_DTD) {
    return NS_OK;
  }

  if (IsImageLoadInEditorAppType(aLoadInfo)) {
    return NS_OK;
  }

  // Only call CheckLoadURIWithPrincipal() using the TriggeringPrincipal and not
  // the LoadingPrincipal when SEC_ALLOW_CROSS_ORIGIN_* security flags are set,
  // to allow, e.g. user stylesheets to load chrome:// URIs.
  return nsContentUtils::GetSecurityManager()->CheckLoadURIWithPrincipal(
      aLoadInfo->TriggeringPrincipal(), aURI, aLoadInfo->CheckLoadURIFlags());
}

static bool URIHasFlags(nsIURI* aURI, uint32_t aURIFlags) {
  bool hasFlags;
  nsresult rv = NS_URIChainHasFlags(aURI, aURIFlags, &hasFlags);
  NS_ENSURE_SUCCESS(rv, false);

  return hasFlags;
}

static nsresult DoSOPChecks(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                            nsIChannel* aChannel) {
  if (aLoadInfo->GetAllowChrome() &&
      (URIHasFlags(aURI, nsIProtocolHandler::URI_IS_UI_RESOURCE) ||
       nsContentUtils::SchemeIs(aURI, "moz-safe-about"))) {
    // UI resources are allowed.
    return DoCheckLoadURIChecks(aURI, aLoadInfo);
  }

  NS_ENSURE_FALSE(NS_HasBeenCrossOrigin(aChannel, true), NS_ERROR_DOM_BAD_URI);

  return NS_OK;
}

static nsresult DoCORSChecks(nsIChannel* aChannel, nsILoadInfo* aLoadInfo,
                             nsCOMPtr<nsIStreamListener>& aInAndOutListener) {
  MOZ_RELEASE_ASSERT(aInAndOutListener,
                     "can not perform CORS checks without a listener");

  // No need to set up CORS if TriggeringPrincipal is the SystemPrincipal.
  // For example, allow user stylesheets to load XBL from external files
  // without requiring CORS.
  if (nsContentUtils::IsSystemPrincipal(aLoadInfo->TriggeringPrincipal())) {
    return NS_OK;
  }

  // We use the triggering principal here, rather than the loading principal
  // to ensure that anonymous CORS content in the browser resources and in
  // WebExtensions is allowed to load.
  nsIPrincipal* principal = aLoadInfo->TriggeringPrincipal();
  RefPtr<nsCORSListenerProxy> corsListener = new nsCORSListenerProxy(
      aInAndOutListener, principal,
      aLoadInfo->GetCookiePolicy() == nsILoadInfo::SEC_COOKIES_INCLUDE);
  // XXX: @arg: DataURIHandling::Allow
  // lets use  DataURIHandling::Allow for now and then decide on callsite basis.
  // see also:
  // http://mxr.mozilla.org/mozilla-central/source/dom/security/nsCORSListenerProxy.h#33
  nsresult rv = corsListener->Init(aChannel, DataURIHandling::Allow);
  NS_ENSURE_SUCCESS(rv, rv);
  aInAndOutListener = corsListener;
  return NS_OK;
}

static nsresult DoContentSecurityChecks(nsIChannel* aChannel,
                                        nsILoadInfo* aLoadInfo) {
  nsContentPolicyType contentPolicyType =
      aLoadInfo->GetExternalContentPolicyType();
  nsContentPolicyType internalContentPolicyType =
      aLoadInfo->InternalContentPolicyType();
  nsCString mimeTypeGuess;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  switch (contentPolicyType) {
    case nsIContentPolicy::TYPE_OTHER: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_SCRIPT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/javascript");
      break;
    }

    case nsIContentPolicy::TYPE_IMAGE: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_STYLESHEET: {
      mimeTypeGuess = NS_LITERAL_CSTRING("text/css");
      break;
    }

    case nsIContentPolicy::TYPE_OBJECT: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_DOCUMENT: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_SUBDOCUMENT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("text/html");
      break;
    }

    case nsIContentPolicy::TYPE_REFRESH: {
      MOZ_ASSERT(false, "contentPolicyType not supported yet");
      break;
    }

    case nsIContentPolicy::TYPE_XBL: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_PING: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_XMLHTTPREQUEST: {
      // alias nsIContentPolicy::TYPE_DATAREQUEST:
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_xml requires requestingContext of type Document");
      }
#endif
      // We're checking for the external TYPE_XMLHTTPREQUEST here in case
      // an addon creates a request with that type.
      if (internalContentPolicyType ==
              nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST ||
          internalContentPolicyType == nsIContentPolicy::TYPE_XMLHTTPREQUEST) {
        mimeTypeGuess = EmptyCString();
      } else {
        MOZ_ASSERT(internalContentPolicyType ==
                       nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE,
                   "can not set mime type guess for unexpected internal type");
        mimeTypeGuess = NS_LITERAL_CSTRING(TEXT_EVENT_STREAM);
      }
      break;
    }

    case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST: {
      mimeTypeGuess = EmptyCString();
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(
            !node || node->NodeType() == nsINode::ELEMENT_NODE,
            "type_subrequest requires requestingContext of type Element");
      }
#endif
      break;
    }

    case nsIContentPolicy::TYPE_DTD: {
      mimeTypeGuess = EmptyCString();
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_dtd requires requestingContext of type Document");
      }
#endif
      break;
    }

    case nsIContentPolicy::TYPE_FONT: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_MEDIA: {
      if (internalContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_TRACK) {
        mimeTypeGuess = NS_LITERAL_CSTRING("text/vtt");
      } else {
        mimeTypeGuess = EmptyCString();
      }
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::ELEMENT_NODE,
                   "type_media requires requestingContext of type Element");
      }
#endif
      break;
    }

    case nsIContentPolicy::TYPE_WEBSOCKET: {
      // Websockets have to use the proxied URI:
      // ws:// instead of http:// for CSP checks
      nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
          do_QueryInterface(aChannel);
      MOZ_ASSERT(httpChannelInternal);
      if (httpChannelInternal) {
        rv = httpChannelInternal->GetProxyURI(getter_AddRefs(uri));
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_CSP_REPORT: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_XSLT: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/xml");
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_xslt requires requestingContext of type Document");
      }
#endif
      break;
    }

    case nsIContentPolicy::TYPE_BEACON: {
      mimeTypeGuess = EmptyCString();
#ifdef DEBUG
      {
        nsCOMPtr<nsINode> node = aLoadInfo->LoadingNode();
        MOZ_ASSERT(!node || node->NodeType() == nsINode::DOCUMENT_NODE,
                   "type_beacon requires requestingContext of type Document");
      }
#endif
      break;
    }

    case nsIContentPolicy::TYPE_FETCH: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_IMAGESET: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_WEB_MANIFEST: {
      mimeTypeGuess = NS_LITERAL_CSTRING("application/manifest+json");
      break;
    }

    case nsIContentPolicy::TYPE_SAVEAS_DOWNLOAD: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    case nsIContentPolicy::TYPE_SPECULATIVE: {
      mimeTypeGuess = EmptyCString();
      break;
    }

    default:
      // nsIContentPolicy::TYPE_INVALID
      MOZ_ASSERT(false,
                 "can not perform security check without a valid contentType");
  }

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(uri, aLoadInfo, mimeTypeGuess, &shouldLoad,
                                 nsContentUtils::GetContentPolicy());

  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    NS_SetRequestBlockingReasonIfNull(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_GENERAL);

    if ((NS_SUCCEEDED(rv) && shouldLoad == nsIContentPolicy::REJECT_TYPE) &&
        (contentPolicyType == nsIContentPolicy::TYPE_DOCUMENT ||
         contentPolicyType == nsIContentPolicy::TYPE_SUBDOCUMENT)) {
      // for docshell loads we might have to return SHOW_ALT.
      return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
    }
    return NS_ERROR_CONTENT_BLOCKED;
  }

  return NS_OK;
}

static void LogPrincipal(nsIPrincipal* aPrincipal,
                         const nsAString& aPrincipalName) {
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  %s: SystemPrincipal\n",
             NS_ConvertUTF16toUTF8(aPrincipalName).get()));
    return;
  }
  if (aPrincipal) {
    if (aPrincipal->GetIsNullPrincipal()) {
      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("  %s: NullPrincipal\n",
               NS_ConvertUTF16toUTF8(aPrincipalName).get()));
      return;
    }
    if (aPrincipal->GetIsExpandedPrincipal()) {
      nsCOMPtr<nsIExpandedPrincipal> expanded(do_QueryInterface(aPrincipal));
      const nsTArray<nsCOMPtr<nsIPrincipal>>& allowList = expanded->AllowList();
      nsAutoCString origin;
      origin.AssignLiteral("[Expanded Principal [");
      for (size_t i = 0; i < allowList.Length(); ++i) {
        if (i != 0) {
          origin.AppendLiteral(", ");
        }

        nsAutoCString subOrigin;
        DebugOnly<nsresult> rv = allowList.ElementAt(i)->GetOrigin(subOrigin);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        origin.Append(subOrigin);
      }
      origin.AppendLiteral("]]");

      MOZ_LOG(sCSMLog, LogLevel::Debug,
              ("  %s: %s\n", NS_ConvertUTF16toUTF8(aPrincipalName).get(),
               origin.get()));
      return;
    }
    nsCOMPtr<nsIURI> principalURI;
    nsAutoCString principalSpec;
    aPrincipal->GetURI(getter_AddRefs(principalURI));
    if (principalURI) {
      principalURI->GetSpec(principalSpec);
    }
    MOZ_LOG(sCSMLog, LogLevel::Debug,
            ("  %s: %s\n", NS_ConvertUTF16toUTF8(aPrincipalName).get(),
             principalSpec.get()));
    return;
  }
  MOZ_LOG(sCSMLog, LogLevel::Debug,
          ("  %s: nullptr\n", NS_ConvertUTF16toUTF8(aPrincipalName).get()));
}

static void LogSecurityFlags(nsSecurityFlags securityFlags) {
  struct DebugSecFlagType {
    unsigned long secFlag;
    char secTypeStr[128];
  };
  static const DebugSecFlagType secTypes[] = {
      {nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
       "SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK"},
      {nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
       "SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS"},
      {nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
       "SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED"},
      {nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS,
       "SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS"},
      {nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
       "SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL"},
      {nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS,
       "SEC_REQUIRE_CORS_DATA_INHERITS"},
      {nsILoadInfo::SEC_COOKIES_DEFAULT, "SEC_COOKIES_DEFAULT"},
      {nsILoadInfo::SEC_COOKIES_INCLUDE, "SEC_COOKIES_INCLUDE"},
      {nsILoadInfo::SEC_COOKIES_SAME_ORIGIN, "SEC_COOKIES_SAME_ORIGIN"},
      {nsILoadInfo::SEC_COOKIES_OMIT, "SEC_COOKIES_OMIT"},
      {nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL, "SEC_FORCE_INHERIT_PRINCIPAL"},
      {nsILoadInfo::SEC_SANDBOXED, "SEC_SANDBOXED"},
      {nsILoadInfo::SEC_ABOUT_BLANK_INHERITS, "SEC_ABOUT_BLANK_INHERITS"},
      {nsILoadInfo::SEC_ALLOW_CHROME, "SEC_ALLOW_CHROME"},
      {nsILoadInfo::SEC_DISALLOW_SCRIPT, "SEC_DISALLOW_SCRIPT"},
      {nsILoadInfo::SEC_DONT_FOLLOW_REDIRECTS, "SEC_DONT_FOLLOW_REDIRECTS"},
      {nsILoadInfo::SEC_LOAD_ERROR_PAGE, "SEC_LOAD_ERROR_PAGE"},
      {nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER,
       "SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER"}};

  for (const DebugSecFlagType flag : secTypes) {
    if (securityFlags & flag.secFlag) {
      MOZ_LOG(sCSMLog, LogLevel::Debug, ("    %s,\n", flag.secTypeStr));
    }
  }
}
static void DebugDoContentSecurityCheck(nsIChannel* aChannel,
                                        nsILoadInfo* aLoadInfo) {
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));

  // we only log http channels, unless loglevel is 5.
  if (httpChannel || MOZ_LOG_TEST(sCSMLog, LogLevel::Verbose)) {
    nsCOMPtr<nsIURI> channelURI;
    nsAutoCString channelSpec;
    nsAutoCString channelMethod;
    NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
    if (channelURI) {
      channelURI->GetSpec(channelSpec);
    }

    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("doContentSecurityCheck {\n"));
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  channelURI: %s\n", channelSpec.get()));

    // Log HTTP-specific things
    if (httpChannel) {
      nsresult rv;
      rv = httpChannel->GetRequestMethod(channelMethod);
      if (!NS_FAILED(rv)) {
        MOZ_LOG(sCSMLog, LogLevel::Verbose,
                ("  HTTP Method: %s\n", channelMethod.get()));
      }
    }

    // Log Principals
    nsCOMPtr<nsIPrincipal> requestPrincipal = aLoadInfo->TriggeringPrincipal();
    LogPrincipal(aLoadInfo->LoadingPrincipal(),
                 NS_LITERAL_STRING("loadingPrincipal"));
    LogPrincipal(requestPrincipal, NS_LITERAL_STRING("triggeringPrincipal"));
    LogPrincipal(aLoadInfo->PrincipalToInherit(),
                 NS_LITERAL_STRING("principalToInherit"));

    // Log Redirect Chain
    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("  RedirectChain:\n"));
    for (nsIRedirectHistoryEntry* redirectHistoryEntry :
         aLoadInfo->RedirectChain()) {
      nsCOMPtr<nsIPrincipal> principal;
      redirectHistoryEntry->GetPrincipal(getter_AddRefs(principal));
      LogPrincipal(principal, NS_LITERAL_STRING("->"));
    }

    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  internalContentPolicyType: %d\n",
             aLoadInfo->InternalContentPolicyType()));
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  externalContentPolicyType: %d\n",
             aLoadInfo->GetExternalContentPolicyType()));
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  upgradeInsecureRequests: %s\n",
             aLoadInfo->GetUpgradeInsecureRequests() ? "true" : "false"));
    MOZ_LOG(sCSMLog, LogLevel::Verbose,
            ("  initalSecurityChecksDone: %s\n",
             aLoadInfo->GetInitialSecurityCheckDone() ? "true" : "false"));

    // Log CSPrequestPrincipal
    nsCOMPtr<nsIContentSecurityPolicy> csp = aLoadInfo->GetCsp();
    if (csp) {
      nsAutoString parsedPolicyStr;
      uint32_t count = 0;
      csp->GetPolicyCount(&count);
      MOZ_LOG(sCSMLog, LogLevel::Debug, ("  CSP (%d): ", count));
      for (uint32_t i = 0; i < count; ++i) {
        csp->GetPolicyString(i, parsedPolicyStr);
        MOZ_LOG(sCSMLog, LogLevel::Debug,
                ("    %s\n", NS_ConvertUTF16toUTF8(parsedPolicyStr).get()));
      }
    }

    // Security Flags
    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("  securityFlags: "));
    LogSecurityFlags(aLoadInfo->GetSecurityFlags());
    MOZ_LOG(sCSMLog, LogLevel::Verbose, ("}\n\n"));
  }
}

#ifdef EARLY_BETA_OR_EARLIER
// Assert that we never use the SystemPrincipal to load remote documents
// i.e., HTTP, HTTPS, FTP URLs
static void AssertSystemPrincipalMustNotLoadRemoteDocuments(
    nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  // bail out, if we're not loading with a SystemPrincipal
  if (!nsContentUtils::IsSystemPrincipal(loadInfo->LoadingPrincipal())) {
    return;
  }
  nsContentPolicyType contentPolicyType =
      loadInfo->GetExternalContentPolicyType();
  if ((contentPolicyType != nsIContentPolicy::TYPE_DOCUMENT) &&
      (contentPolicyType != nsIContentPolicy::TYPE_SUBDOCUMENT)) {
    return;
  }
  nsCOMPtr<nsIURI> finalURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalURI));
  // bail out, if URL isn't pointing to remote resource
  if (!nsContentUtils::SchemeIs(finalURI, "http") &&
      !nsContentUtils::SchemeIs(finalURI, "https") &&
      !nsContentUtils::SchemeIs(finalURI, "ftp")) {
    return;
  }

  // FIXME The discovery feature in about:addons uses the SystemPrincpal.
  // We should remove the exception for AMO with bug 1544011.
  // We should remove the exception for Firefox Accounts with bug 1561318.
  static nsAutoCString sDiscoveryPrePath;
#  ifdef ANDROID
  static nsAutoCString sFxaSPrePath;
#  endif
  static bool recvdPrefValues = false;
  if (!recvdPrefValues) {
    nsAutoCString discoveryURLString;
    Preferences::GetCString("extensions.webservice.discoverURL",
                            discoveryURLString);
    // discoverURL is by default suffixed with parameters in path like
    // /%LOCALE%/ so, we use the prePath for comparison
    nsCOMPtr<nsIURI> discoveryURL;
    NS_NewURI(getter_AddRefs(discoveryURL), discoveryURLString);
    if (discoveryURL) {
      discoveryURL->GetPrePath(sDiscoveryPrePath);
    }
#  ifdef ANDROID
    nsAutoCString fxaURLString;
    Preferences::GetCString("identity.fxaccounts.remote.webchannel.uri",
                            fxaURLString);
    nsCOMPtr<nsIURI> fxaURL;
    NS_NewURI(getter_AddRefs(fxaURL), fxaURLString);
    if (fxaURL) {
      fxaURL->GetPrePath(sFxaSPrePath);
    }
#  endif
    recvdPrefValues = true;
  }
  nsAutoCString requestedPrePath;
  finalURI->GetPrePath(requestedPrePath);

  if (requestedPrePath.Equals(sDiscoveryPrePath)) {
    return;
  }
#  ifdef ANDROID
  if (requestedPrePath.Equals(sFxaSPrePath)) {
    return;
  }
#  endif
  if (xpc::AreNonLocalConnectionsDisabled()) {
    bool disallowSystemPrincipalRemoteDocuments = Preferences::GetBool(
        "security.disallow_non_local_systemprincipal_in_tests");
    if (disallowSystemPrincipalRemoteDocuments) {
      // our own mochitest needs NS_ASSERTION instead of MOZ_ASSERT
      NS_ASSERTION(false, "SystemPrincipal must not load remote documents.");
      return;
    }
    // but other mochitest are exempt from this
    return;
  }
  MOZ_RELEASE_ASSERT(false, "SystemPrincipal must not load remote documents.");
}
#endif

/*
 * Based on the security flags provided in the loadInfo of the channel,
 * doContentSecurityCheck() performs the following content security checks
 * before opening the channel:
 *
 * (1) Same Origin Policy Check (if applicable)
 * (2) Allow Cross Origin but perform sanity checks whether a principal
 *     is allowed to access the following URL.
 * (3) Perform CORS check (if applicable)
 * (4) ContentPolicy checks (Content-Security-Policy, Mixed Content, ...)
 *
 * @param aChannel
 *    The channel to perform the security checks on.
 * @param aInAndOutListener
 *    The streamListener that is passed to channel->AsyncOpen() that is now
 * potentially wrappend within nsCORSListenerProxy() and becomes the
 * corsListener that now needs to be set as new streamListener on the channel.
 */
nsresult nsContentSecurityManager::doContentSecurityCheck(
    nsIChannel* aChannel, nsCOMPtr<nsIStreamListener>& aInAndOutListener) {
  NS_ENSURE_ARG(aChannel);
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (MOZ_UNLIKELY(MOZ_LOG_TEST(sCSMLog, LogLevel::Verbose))) {
    DebugDoContentSecurityCheck(aChannel, loadInfo);
  }

#ifdef EARLY_BETA_OR_EARLIER
  AssertSystemPrincipalMustNotLoadRemoteDocuments(aChannel);
#endif

  // if dealing with a redirected channel then we have already installed
  // streamlistener and redirect proxies and so we are done.
  if (loadInfo->GetInitialSecurityCheckDone()) {
    return NS_OK;
  }

  // make sure that only one of the five security flags is set in the loadinfo
  // e.g. do not require same origin and allow cross origin at the same time
  nsresult rv = ValidateSecurityFlags(loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  if (loadInfo->GetSecurityMode() ==
      nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    rv = DoCORSChecks(aChannel, loadInfo, aInAndOutListener);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = CheckChannel(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // Perform all ContentPolicy checks (MixedContent, CSP, ...)
  rv = DoContentSecurityChecks(aChannel, loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // Apply this after CSP to match Chrome.
  rv = CheckFTPSubresourceLoad(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // now lets set the initalSecurityFlag for subsequent calls
  loadInfo->SetInitialSecurityCheckDone(true);

  // all security checks passed - lets allow the load
  return NS_OK;
}

NS_IMETHODIMP
nsContentSecurityManager::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aRedirFlags,
    nsIAsyncVerifyRedirectCallback* aCb) {
  nsCOMPtr<nsILoadInfo> loadInfo = aOldChannel->LoadInfo();
  nsresult rv = CheckChannel(aNewChannel);
  if (NS_SUCCEEDED(rv)) {
    rv = CheckFTPSubresourceLoad(aNewChannel);
  }
  if (NS_FAILED(rv)) {
    aOldChannel->Cancel(rv);
    return rv;
  }

  // Also verify that the redirecting server is allowed to redirect to the
  // given URI
  nsCOMPtr<nsIPrincipal> oldPrincipal;
  nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      aOldChannel, getter_AddRefs(oldPrincipal));

  nsCOMPtr<nsIURI> newURI;
  Unused << NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  NS_ENSURE_STATE(oldPrincipal && newURI);

  // Do not allow insecure redirects to data: URIs
  if (!AllowInsecureRedirectToDataURI(aNewChannel)) {
    // cancel the old channel and return an error
    aOldChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
    return NS_ERROR_CONTENT_BLOCKED;
  }

  const uint32_t flags =
      nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
      nsIScriptSecurityManager::DISALLOW_SCRIPT;
  rv = nsContentUtils::GetSecurityManager()->CheckLoadURIWithPrincipal(
      oldPrincipal, newURI, flags);
  NS_ENSURE_SUCCESS(rv, rv);

  aCb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

static void AddLoadFlags(nsIRequest* aRequest, nsLoadFlags aNewFlags) {
  nsLoadFlags flags;
  aRequest->GetLoadFlags(&flags);
  flags |= aNewFlags;
  aRequest->SetLoadFlags(flags);
}

/*
 * Check that this channel passes all security checks. Returns an error code
 * if this requesst should not be permitted.
 */
nsresult nsContentSecurityManager::CheckChannel(nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Handle cookie policies
  uint32_t cookiePolicy = loadInfo->GetCookiePolicy();
  if (cookiePolicy == nsILoadInfo::SEC_COOKIES_SAME_ORIGIN) {
    // We shouldn't have the SEC_COOKIES_SAME_ORIGIN flag for top level loads
    MOZ_ASSERT(loadInfo->GetExternalContentPolicyType() !=
               nsIContentPolicy::TYPE_DOCUMENT);
    nsIPrincipal* loadingPrincipal = loadInfo->LoadingPrincipal();

    // It doesn't matter what we pass for the third, data-inherits, argument.
    // Any protocol which inherits won't pay attention to cookies anyway.
    rv = loadingPrincipal->CheckMayLoad(uri, false, false);
    if (NS_FAILED(rv)) {
      AddLoadFlags(aChannel, nsIRequest::LOAD_ANONYMOUS);
    }
  } else if (cookiePolicy == nsILoadInfo::SEC_COOKIES_OMIT) {
    AddLoadFlags(aChannel, nsIRequest::LOAD_ANONYMOUS);
  }

  nsSecurityFlags securityMode = loadInfo->GetSecurityMode();

  // CORS mode is handled by nsCORSListenerProxy
  if (securityMode == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    if (NS_HasBeenCrossOrigin(aChannel)) {
      loadInfo->MaybeIncreaseTainting(LoadTainting::CORS);
    }
    return NS_OK;
  }

  // Allow subresource loads if TriggeringPrincipal is the SystemPrincipal.
  // For example, allow user stylesheets to load XBL from external files.
  if (nsContentUtils::IsSystemPrincipal(loadInfo->TriggeringPrincipal()) &&
      loadInfo->GetExternalContentPolicyType() !=
          nsIContentPolicy::TYPE_DOCUMENT &&
      loadInfo->GetExternalContentPolicyType() !=
          nsIContentPolicy::TYPE_SUBDOCUMENT) {
    return NS_OK;
  }

  // if none of the REQUIRE_SAME_ORIGIN flags are set, then SOP does not apply
  if ((securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS) ||
      (securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED)) {
    rv = DoSOPChecks(uri, loadInfo, aChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if ((securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS) ||
      (securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL)) {
    if (NS_HasBeenCrossOrigin(aChannel)) {
      NS_ENSURE_FALSE(loadInfo->GetDontFollowRedirects(), NS_ERROR_DOM_BAD_URI);
      loadInfo->MaybeIncreaseTainting(LoadTainting::Opaque);
    }
    // Please note that DoCheckLoadURIChecks should only be enforced for
    // cross origin requests. If the flag SEC_REQUIRE_CORS_DATA_INHERITS is set
    // within the loadInfo, then then CheckLoadURIWithPrincipal is performed
    // within nsCorsListenerProxy
    rv = DoCheckLoadURIChecks(uri, loadInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    // TODO: Bug 1371237
    // consider calling SetBlockedRequest in
    // nsContentSecurityManager::CheckChannel
  }

  return NS_OK;
}

// ==== nsIContentSecurityManager implementation =====

NS_IMETHODIMP
nsContentSecurityManager::PerformSecurityCheck(
    nsIChannel* aChannel, nsIStreamListener* aStreamListener,
    nsIStreamListener** outStreamListener) {
  nsCOMPtr<nsIStreamListener> inAndOutListener = aStreamListener;
  nsresult rv = doContentSecurityCheck(aChannel, inAndOutListener);
  NS_ENSURE_SUCCESS(rv, rv);

  inAndOutListener.forget(outStreamListener);
  return NS_OK;
}

NS_IMETHODIMP
nsContentSecurityManager::IsOriginPotentiallyTrustworthy(
    nsIPrincipal* aPrincipal, bool* aIsTrustWorthy) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aIsTrustWorthy);

  if (aPrincipal->IsSystemPrincipal()) {
    *aIsTrustWorthy = true;
    return NS_OK;
  }
  *aIsTrustWorthy = false;
  if (aPrincipal->GetIsNullPrincipal()) {
    return NS_OK;
  }

  MOZ_ASSERT(aPrincipal->GetIsContentPrincipal(),
             "Nobody is expected to call us with an nsIExpandedPrincipal");

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  *aIsTrustWorthy = nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(uri);

  return NS_OK;
}
