/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAttrValue.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsCSPUtils.h"
#include "nsDebug.h"
#include "nsCSPParser.h"
#include "nsComponentManagerUtils.h"
#include "nsIConsoleService.h"
#include "nsIChannel.h"
#include "nsICryptoHash.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsSandboxFlags.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/Components.h"
#include "mozilla/dom/CSPDictionariesBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/StaticPrefs_security.h"

#define DEFAULT_PORT -1

static mozilla::LogModule* GetCspUtilsLog() {
  static mozilla::LazyLogModule gCspUtilsPRLog("CSPUtils");
  return gCspUtilsPRLog;
}

#define CSPUTILSLOG(args) \
  MOZ_LOG(GetCspUtilsLog(), mozilla::LogLevel::Debug, args)
#define CSPUTILSLOGENABLED() \
  MOZ_LOG_TEST(GetCspUtilsLog(), mozilla::LogLevel::Debug)

void CSP_PercentDecodeStr(const nsAString& aEncStr, nsAString& outDecStr) {
  outDecStr.Truncate();

  // helper function that should not be visible outside this methods scope
  struct local {
    static inline char16_t convertHexDig(char16_t aHexDig) {
      if (isNumberToken(aHexDig)) {
        return aHexDig - '0';
      }
      if (aHexDig >= 'A' && aHexDig <= 'F') {
        return aHexDig - 'A' + 10;
      }
      // must be a lower case character
      // (aHexDig >= 'a' && aHexDig <= 'f')
      return aHexDig - 'a' + 10;
    }
  };

  const char16_t *cur, *end, *hexDig1, *hexDig2;
  cur = aEncStr.BeginReading();
  end = aEncStr.EndReading();

  while (cur != end) {
    // if it's not a percent sign then there is
    // nothing to do for that character
    if (*cur != PERCENT_SIGN) {
      outDecStr.Append(*cur);
      cur++;
      continue;
    }

    // get the two hexDigs following the '%'-sign
    hexDig1 = cur + 1;
    hexDig2 = cur + 2;

    // if there are no hexdigs after the '%' then
    // there is nothing to do for us.
    if (hexDig1 == end || hexDig2 == end || !isValidHexDig(*hexDig1) ||
        !isValidHexDig(*hexDig2)) {
      outDecStr.Append(PERCENT_SIGN);
      cur++;
      continue;
    }

    // decode "% hexDig1 hexDig2" into a character.
    char16_t decChar =
        (local::convertHexDig(*hexDig1) << 4) + local::convertHexDig(*hexDig2);
    outDecStr.Append(decChar);

    // increment 'cur' to after the second hexDig
    cur = ++hexDig2;
  }
}

// The Content Security Policy should be inherited for
// local schemes like: "about", "blob", "data", or "filesystem".
// see: https://w3c.github.io/webappsec-csp/#initialize-document-csp
bool CSP_ShouldResponseInheritCSP(nsIChannel* aChannel) {
  if (!aChannel) {
    return false;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, false);

  bool isAbout = uri->SchemeIs("about");
  if (isAbout) {
    nsAutoCString aboutSpec;
    rv = uri->GetSpec(aboutSpec);
    NS_ENSURE_SUCCESS(rv, false);
    // also allow about:blank#foo
    if (StringBeginsWith(aboutSpec, "about:blank"_ns) ||
        StringBeginsWith(aboutSpec, "about:srcdoc"_ns)) {
      return true;
    }
  }

  return uri->SchemeIs("blob") || uri->SchemeIs("data") ||
         uri->SchemeIs("filesystem") || uri->SchemeIs("javascript");
}

void CSP_ApplyMetaCSPToDoc(mozilla::dom::Document& aDoc,
                           const nsAString& aPolicyStr) {
  if (!mozilla::StaticPrefs::security_csp_enable() || aDoc.IsLoadedAsData()) {
    return;
  }

  nsAutoString policyStr(
      nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
          aPolicyStr));

  if (policyStr.IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp = aDoc.GetCsp();
  if (!csp) {
    MOZ_ASSERT(false, "how come there is no CSP");
    return;
  }

  // Multiple CSPs (delivered through either header of meta tag) need to
  // be joined together, see:
  // https://w3c.github.io/webappsec/specs/content-security-policy/#delivery-html-meta-element
  nsresult rv =
      csp->AppendPolicy(policyStr,
                        false,  // csp via meta tag can not be report only
                        true);  // delivered through the meta tag
  NS_ENSURE_SUCCESS_VOID(rv);
  if (nsPIDOMWindowInner* inner = aDoc.GetInnerWindow()) {
    inner->SetCsp(csp);
  }
  aDoc.ApplySettingsFromCSP(false);
}

void CSP_GetLocalizedStr(const char* aName, const nsTArray<nsString>& aParams,
                         nsAString& outResult) {
  nsCOMPtr<nsIStringBundle> keyStringBundle;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      mozilla::components::StringBundle::Service();

  NS_ASSERTION(stringBundleService, "String bundle service must be present!");
  stringBundleService->CreateBundle(
      "chrome://global/locale/security/csp.properties",
      getter_AddRefs(keyStringBundle));

  NS_ASSERTION(keyStringBundle, "Key string bundle must be available!");

  if (!keyStringBundle) {
    return;
  }
  keyStringBundle->FormatStringFromName(aName, aParams, outResult);
}

void CSP_LogStrMessage(const nsAString& aMsg) {
  nsCOMPtr<nsIConsoleService> console(
      do_GetService("@mozilla.org/consoleservice;1"));

  if (!console) {
    return;
  }
  nsString msg(aMsg);
  console->LogStringMessage(msg.get());
}

void CSP_LogMessage(const nsAString& aMessage, const nsAString& aSourceName,
                    const nsAString& aSourceLine, uint32_t aLineNumber,
                    uint32_t aColumnNumber, uint32_t aFlags,
                    const nsACString& aCategory, uint64_t aInnerWindowID,
                    bool aFromPrivateWindow) {
  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));

  if (!console || !error) {
    return;
  }

  // Prepending CSP to the outgoing console message
  nsString cspMsg;
  cspMsg.AppendLiteral(u"Content Security Policy: ");
  cspMsg.Append(aMessage);

  // Currently 'aSourceLine' is not logged to the console, because similar
  // information is already included within the source link of the message.
  // For inline violations however, the line and column number are 0 and
  // information contained within 'aSourceLine' can be really useful for devs.
  // E.g. 'aSourceLine' might be: 'onclick attribute on DIV element'.
  // In such cases we append 'aSourceLine' directly to the error message.
  if (!aSourceLine.IsEmpty()) {
    cspMsg.AppendLiteral(u" Source: ");
    cspMsg.Append(aSourceLine);
    cspMsg.AppendLiteral(u".");
  }

  // Since we are leveraging csp errors as the category names which
  // we pass to devtools, we should prepend them with "CSP_" to
  // allow easy distincution in devtools code. e.g.
  // upgradeInsecureRequest -> CSP_upgradeInsecureRequest
  nsCString category("CSP_");
  category.Append(aCategory);

  nsresult rv;
  if (aInnerWindowID > 0) {
    rv = error->InitWithWindowID(cspMsg, aSourceName, aSourceLine, aLineNumber,
                                 aColumnNumber, aFlags, category,
                                 aInnerWindowID);
  } else {
    rv = error->Init(cspMsg, aSourceName, aSourceLine, aLineNumber,
                     aColumnNumber, aFlags, category.get(), aFromPrivateWindow,
                     true /* from chrome context */);
  }
  if (NS_FAILED(rv)) {
    return;
  }
  console->LogMessage(error);
}

/**
 * Combines CSP_LogMessage and CSP_GetLocalizedStr into one call.
 */
void CSP_LogLocalizedStr(const char* aName, const nsTArray<nsString>& aParams,
                         const nsAString& aSourceName,
                         const nsAString& aSourceLine, uint32_t aLineNumber,
                         uint32_t aColumnNumber, uint32_t aFlags,
                         const nsACString& aCategory, uint64_t aInnerWindowID,
                         bool aFromPrivateWindow) {
  nsAutoString logMsg;
  CSP_GetLocalizedStr(aName, aParams, logMsg);
  CSP_LogMessage(logMsg, aSourceName, aSourceLine, aLineNumber, aColumnNumber,
                 aFlags, aCategory, aInnerWindowID, aFromPrivateWindow);
}

/* ===== Helpers ============================ */
CSPDirective CSP_ContentTypeToDirective(nsContentPolicyType aType) {
  // We need to know if this is a worker so child-src  can handle that case
  // correctly.
  switch (aType) {
    case nsIContentPolicy::TYPE_IMAGE:
    case nsIContentPolicy::TYPE_IMAGESET:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON:
      return nsIContentSecurityPolicy::IMG_SRC_DIRECTIVE;

    // BLock XSLT as script, see bug 910139
    case nsIContentPolicy::TYPE_XSLT:
    case nsIContentPolicy::TYPE_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS:
    case nsIContentPolicy::TYPE_INTERNAL_AUDIOWORKLET:
    case nsIContentPolicy::TYPE_INTERNAL_PAINTWORKLET:
    case nsIContentPolicy::TYPE_INTERNAL_CHROMEUTILS_COMPILED_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_FRAME_MESSAGEMANAGER_SCRIPT:
      return nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_STYLESHEET:
    case nsIContentPolicy::TYPE_INTERNAL_STYLESHEET:
    case nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD:
      return nsIContentSecurityPolicy::STYLE_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_FONT:
    case nsIContentPolicy::TYPE_INTERNAL_FONT_PRELOAD:
      return nsIContentSecurityPolicy::FONT_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_MEDIA:
    case nsIContentPolicy::TYPE_INTERNAL_AUDIO:
    case nsIContentPolicy::TYPE_INTERNAL_VIDEO:
    case nsIContentPolicy::TYPE_INTERNAL_TRACK:
      return nsIContentSecurityPolicy::MEDIA_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_WEB_MANIFEST:
      return nsIContentSecurityPolicy::WEB_MANIFEST_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_INTERNAL_WORKER:
    case nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER:
    case nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER:
      return nsIContentSecurityPolicy::WORKER_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_SUBDOCUMENT:
    case nsIContentPolicy::TYPE_INTERNAL_FRAME:
    case nsIContentPolicy::TYPE_INTERNAL_IFRAME:
      return nsIContentSecurityPolicy::FRAME_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_WEBSOCKET:
    case nsIContentPolicy::TYPE_XMLHTTPREQUEST:
    case nsIContentPolicy::TYPE_BEACON:
    case nsIContentPolicy::TYPE_PING:
    case nsIContentPolicy::TYPE_FETCH:
    case nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST:
    case nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE:
    case nsIContentPolicy::TYPE_INTERNAL_FETCH_PRELOAD:
      return nsIContentSecurityPolicy::CONNECT_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_OBJECT:
    case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST:
    case nsIContentPolicy::TYPE_INTERNAL_EMBED:
    case nsIContentPolicy::TYPE_INTERNAL_OBJECT:
      return nsIContentSecurityPolicy::OBJECT_SRC_DIRECTIVE;

    case nsIContentPolicy::TYPE_DTD:
    case nsIContentPolicy::TYPE_OTHER:
    case nsIContentPolicy::TYPE_SPECULATIVE:
    case nsIContentPolicy::TYPE_INTERNAL_DTD:
    case nsIContentPolicy::TYPE_INTERNAL_FORCE_ALLOWED_DTD:
      return nsIContentSecurityPolicy::DEFAULT_SRC_DIRECTIVE;

    // CSP does not apply to webrtc connections
    case nsIContentPolicy::TYPE_PROXIED_WEBRTC_MEDIA:
    // csp shold not block top level loads, e.g. in case
    // of a redirect.
    case nsIContentPolicy::TYPE_DOCUMENT:
    // CSP can not block csp reports
    case nsIContentPolicy::TYPE_CSP_REPORT:
      return nsIContentSecurityPolicy::NO_DIRECTIVE;

    case nsIContentPolicy::TYPE_SAVEAS_DOWNLOAD:
    case nsIContentPolicy::TYPE_UA_FONT:
      return nsIContentSecurityPolicy::NO_DIRECTIVE;

    // Fall through to error for all other directives
    // Note that we should never end up here for navigate-to
    case nsIContentPolicy::TYPE_INVALID:
      MOZ_ASSERT(false, "Can not map nsContentPolicyType to CSPDirective");
      // Do not add default: so that compilers can catch the missing case.
  }
  return nsIContentSecurityPolicy::DEFAULT_SRC_DIRECTIVE;
}

nsCSPHostSrc* CSP_CreateHostSrcFromSelfURI(nsIURI* aSelfURI) {
  // Create the host first
  nsCString host;
  aSelfURI->GetAsciiHost(host);
  nsCSPHostSrc* hostsrc = new nsCSPHostSrc(NS_ConvertUTF8toUTF16(host));
  hostsrc->setGeneratedFromSelfKeyword();

  // Add the scheme.
  nsCString scheme;
  aSelfURI->GetScheme(scheme);
  hostsrc->setScheme(NS_ConvertUTF8toUTF16(scheme));

  // An empty host (e.g. for data:) indicates it's effectively a unique origin.
  // Please note that we still need to set the scheme on hostsrc (see above),
  // because it's used for reporting.
  if (host.EqualsLiteral("")) {
    hostsrc->setIsUniqueOrigin();
    // no need to query the port in that case.
    return hostsrc;
  }

  int32_t port;
  aSelfURI->GetPort(&port);
  // Only add port if it's not default port.
  if (port > 0) {
    nsAutoString portStr;
    portStr.AppendInt(port);
    hostsrc->setPort(portStr);
  }
  return hostsrc;
}

bool CSP_IsEmptyDirective(const nsAString& aValue, const nsAString& aDir) {
  return (aDir.Length() == 0 && aValue.Length() == 0);
}
bool CSP_IsDirective(const nsAString& aValue, CSPDirective aDir) {
  return aValue.LowerCaseEqualsASCII(CSP_CSPDirectiveToString(aDir));
}

bool CSP_IsKeyword(const nsAString& aValue, enum CSPKeyword aKey) {
  return aValue.LowerCaseEqualsASCII(CSP_EnumToUTF8Keyword(aKey));
}

bool CSP_IsQuotelessKeyword(const nsAString& aKey) {
  nsString lowerKey;
  ToLowerCase(aKey, lowerKey);

  nsAutoString keyword;
  for (uint32_t i = 0; i < CSP_LAST_KEYWORD_VALUE; i++) {
    // skipping the leading ' and trimming the trailing '
    keyword.AssignASCII(gCSPUTF8Keywords[i] + 1);
    keyword.Trim("'", false, true);
    if (lowerKey.Equals(keyword)) {
      return true;
    }
  }
  return false;
}

/*
 * Checks whether the current directive permits a specific
 * scheme. This function is called from nsCSPSchemeSrc() and
 * also nsCSPHostSrc.
 * @param aEnforcementScheme
 *        The scheme that this directive allows
 * @param aUri
 *        The uri of the subresource load.
 * @param aReportOnly
 *        Whether the enforced policy is report only or not.
 * @param aUpgradeInsecure
 *        Whether the policy makes use of the directive
 *        'upgrade-insecure-requests'.
 * @param aFromSelfURI
 *        Whether a scheme was generated from the keyword 'self'
 *        which then allows schemeless sources to match ws and wss.
 */

bool permitsScheme(const nsAString& aEnforcementScheme, nsIURI* aUri,
                   bool aReportOnly, bool aUpgradeInsecure, bool aFromSelfURI) {
  nsAutoCString scheme;
  nsresult rv = aUri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, false);

  // no scheme to enforce, let's allow the load (e.g. script-src *)
  if (aEnforcementScheme.IsEmpty()) {
    return true;
  }

  // if the scheme matches, all good - allow the load
  if (aEnforcementScheme.EqualsASCII(scheme.get())) {
    return true;
  }

  // allow scheme-less sources where the protected resource is http
  // and the load is https, see:
  // http://www.w3.org/TR/CSP2/#match-source-expression
  if (aEnforcementScheme.EqualsASCII("http")) {
    if (scheme.EqualsASCII("https")) {
      return true;
    }
    if ((scheme.EqualsASCII("ws") || scheme.EqualsASCII("wss")) &&
        aFromSelfURI) {
      return true;
    }
  }
  if (aEnforcementScheme.EqualsASCII("https")) {
    if (scheme.EqualsLiteral("wss") && aFromSelfURI) {
      return true;
    }
  }
  if (aEnforcementScheme.EqualsASCII("ws") && scheme.EqualsASCII("wss")) {
    return true;
  }

  // Allow the load when enforcing upgrade-insecure-requests with the
  // promise the request gets upgraded from http to https and ws to wss.
  // See nsHttpChannel::Connect() and also WebSocket.cpp. Please note,
  // the report only policies should not allow the load and report
  // the error back to the page.
  return (
      (aUpgradeInsecure && !aReportOnly) &&
      ((scheme.EqualsASCII("http") &&
        aEnforcementScheme.EqualsASCII("https")) ||
       (scheme.EqualsASCII("ws") && aEnforcementScheme.EqualsASCII("wss"))));
}

/*
 * A helper function for appending a CSP header to an existing CSP
 * policy.
 *
 * @param aCsp           the CSP policy
 * @param aHeaderValue   the header
 * @param aReportOnly    is this a report-only header?
 */

nsresult CSP_AppendCSPFromHeader(nsIContentSecurityPolicy* aCsp,
                                 const nsAString& aHeaderValue,
                                 bool aReportOnly) {
  NS_ENSURE_ARG(aCsp);

  // Need to tokenize the header value since multiple headers could be
  // concatenated into one comma-separated list of policies.
  // See RFC2616 section 4.2 (last paragraph)
  nsresult rv = NS_OK;
  for (const nsAString& policy :
       nsCharSeparatedTokenizer(aHeaderValue, ',').ToRange()) {
    rv = aCsp->AppendPolicy(policy, aReportOnly, false);
    NS_ENSURE_SUCCESS(rv, rv);
    {
      CSPUTILSLOG(("CSP refined with policy: \"%s\"",
                   NS_ConvertUTF16toUTF8(policy).get()));
    }
  }
  return NS_OK;
}

/* ===== nsCSPSrc ============================ */

nsCSPBaseSrc::nsCSPBaseSrc() : mInvalidated(false) {}

nsCSPBaseSrc::~nsCSPBaseSrc() = default;

// ::permits is only called for external load requests, therefore:
// nsCSPKeywordSrc and nsCSPHashSource fall back to this base class
// implementation which will never allow the load.
bool nsCSPBaseSrc::permits(nsIURI* aUri, const nsAString& aNonce,
                           bool aWasRedirected, bool aReportOnly,
                           bool aUpgradeInsecure, bool aParserCreated) const {
  if (CSPUTILSLOGENABLED()) {
    CSPUTILSLOG(
        ("nsCSPBaseSrc::permits, aUri: %s", aUri->GetSpecOrDefault().get()));
  }
  return false;
}

// ::allows is only called for inlined loads, therefore:
// nsCSPSchemeSrc, nsCSPHostSrc fall back
// to this base class implementation which will never allow the load.
bool nsCSPBaseSrc::allows(enum CSPKeyword aKeyword,
                          const nsAString& aHashOrNonce,
                          bool aParserCreated) const {
  CSPUTILSLOG(("nsCSPBaseSrc::allows, aKeyWord: %s, a HashOrNonce: %s",
               aKeyword == CSP_HASH ? "hash" : CSP_EnumToUTF8Keyword(aKeyword),
               NS_ConvertUTF16toUTF8(aHashOrNonce).get()));
  return false;
}

/* ====== nsCSPSchemeSrc ===================== */

nsCSPSchemeSrc::nsCSPSchemeSrc(const nsAString& aScheme) : mScheme(aScheme) {
  ToLowerCase(mScheme);
}

nsCSPSchemeSrc::~nsCSPSchemeSrc() = default;

bool nsCSPSchemeSrc::permits(nsIURI* aUri, const nsAString& aNonce,
                             bool aWasRedirected, bool aReportOnly,
                             bool aUpgradeInsecure, bool aParserCreated) const {
  if (CSPUTILSLOGENABLED()) {
    CSPUTILSLOG(
        ("nsCSPSchemeSrc::permits, aUri: %s", aUri->GetSpecOrDefault().get()));
  }
  MOZ_ASSERT((!mScheme.EqualsASCII("")), "scheme can not be the empty string");
  if (mInvalidated) {
    return false;
  }
  return permitsScheme(mScheme, aUri, aReportOnly, aUpgradeInsecure, false);
}

bool nsCSPSchemeSrc::visit(nsCSPSrcVisitor* aVisitor) const {
  return aVisitor->visitSchemeSrc(*this);
}

void nsCSPSchemeSrc::toString(nsAString& outStr) const {
  outStr.Append(mScheme);
  outStr.AppendLiteral(":");
}

/* ===== nsCSPHostSrc ======================== */

nsCSPHostSrc::nsCSPHostSrc(const nsAString& aHost)
    : mHost(aHost),
      mGeneratedFromSelfKeyword(false),
      mIsUniqueOrigin(false),
      mWithinFrameAncstorsDir(false) {
  ToLowerCase(mHost);
}

nsCSPHostSrc::~nsCSPHostSrc() = default;

/*
 * Checks whether the current directive permits a specific port.
 * @param aEnforcementScheme
 *        The scheme that this directive allows
 *        (used to query the default port for that scheme)
 * @param aEnforcementPort
 *        The port that this directive allows
 * @param aResourceURI
 *        The uri of the subresource load
 */
bool permitsPort(const nsAString& aEnforcementScheme,
                 const nsAString& aEnforcementPort, nsIURI* aResourceURI) {
  // If enforcement port is the wildcard, don't block the load.
  if (aEnforcementPort.EqualsASCII("*")) {
    return true;
  }

  int32_t resourcePort;
  nsresult rv = aResourceURI->GetPort(&resourcePort);
  if (NS_FAILED(rv) && aEnforcementPort.IsEmpty()) {
    // If we cannot get a Port (e.g. because of an Custom Protocol handler)
    // We need to check if a default port is associated with the Scheme
    if (aEnforcementScheme.IsEmpty()) {
      return false;
    }
    int defaultPortforScheme =
        NS_GetDefaultPort(NS_ConvertUTF16toUTF8(aEnforcementScheme).get());

    // If there is no default port associated with the Scheme (
    // defaultPortforScheme == -1) or it is an externally handled protocol (
    // defaultPortforScheme == 0 ) and the csp does not enforce a port - we can
    // allow not having a port
    return (defaultPortforScheme == -1 || defaultPortforScheme == -0);
  }
  // Avoid unnecessary string creation/manipulation and don't block the
  // load if the resource to be loaded uses the default port for that
  // scheme and there is no port to be enforced.
  // Note, this optimization relies on scheme checks within permitsScheme().
  if (resourcePort == DEFAULT_PORT && aEnforcementPort.IsEmpty()) {
    return true;
  }

  // By now we know at that either the resourcePort does not use the default
  // port or there is a port restriction to be enforced. A port value of -1
  // corresponds to the protocol's default port (eg. -1 implies port 80 for
  // http URIs), in such a case we have to query the default port of the
  // resource to be loaded.
  if (resourcePort == DEFAULT_PORT) {
    nsAutoCString resourceScheme;
    rv = aResourceURI->GetScheme(resourceScheme);
    NS_ENSURE_SUCCESS(rv, false);
    resourcePort = NS_GetDefaultPort(resourceScheme.get());
  }

  // If there is a port to be enforced and the ports match, then
  // don't block the load.
  nsString resourcePortStr;
  resourcePortStr.AppendInt(resourcePort);
  if (aEnforcementPort.Equals(resourcePortStr)) {
    return true;
  }

  // If there is no port to be enforced, query the default port for the load.
  nsString enforcementPort(aEnforcementPort);
  if (enforcementPort.IsEmpty()) {
    // For scheme less sources, our parser always generates a scheme
    // which is the scheme of the protected resource.
    MOZ_ASSERT(!aEnforcementScheme.IsEmpty(),
               "need a scheme to query default port");
    int32_t defaultEnforcementPort =
        NS_GetDefaultPort(NS_ConvertUTF16toUTF8(aEnforcementScheme).get());
    enforcementPort.Truncate();
    enforcementPort.AppendInt(defaultEnforcementPort);
  }

  // If default ports match, don't block the load
  if (enforcementPort.Equals(resourcePortStr)) {
    return true;
  }

  // Additional port matching where the regular URL matching algorithm
  // treats insecure ports as matching their secure variants.
  // default port for http is  :80
  // default port for https is :443
  if (enforcementPort.EqualsLiteral("80") &&
      resourcePortStr.EqualsLiteral("443")) {
    return true;
  }

  // ports do not match, block the load.
  return false;
}

bool nsCSPHostSrc::permits(nsIURI* aUri, const nsAString& aNonce,
                           bool aWasRedirected, bool aReportOnly,
                           bool aUpgradeInsecure, bool aParserCreated) const {
  if (CSPUTILSLOGENABLED()) {
    CSPUTILSLOG(
        ("nsCSPHostSrc::permits, aUri: %s", aUri->GetSpecOrDefault().get()));
  }

  if (mInvalidated || mIsUniqueOrigin) {
    return false;
  }

  // we are following the enforcement rules from the spec, see:
  // http://www.w3.org/TR/CSP11/#match-source-expression

  // 4.3) scheme matching: Check if the scheme matches.
  if (!permitsScheme(mScheme, aUri, aReportOnly, aUpgradeInsecure,
                     mGeneratedFromSelfKeyword)) {
    return false;
  }

  // The host in nsCSpHostSrc should never be empty. In case we are enforcing
  // just a specific scheme, the parser should generate a nsCSPSchemeSource.
  NS_ASSERTION((!mHost.IsEmpty()), "host can not be the empty string");

  // Before we can check if the host matches, we have to
  // extract the host part from aUri.
  nsAutoCString uriHost;
  nsresult rv = aUri->GetAsciiHost(uriHost);
  NS_ENSURE_SUCCESS(rv, false);

  nsString decodedUriHost;
  CSP_PercentDecodeStr(NS_ConvertUTF8toUTF16(uriHost), decodedUriHost);

  // 2) host matching: Enforce a single *
  if (mHost.EqualsASCII("*")) {
    // The single ASTERISK character (*) does not match a URI's scheme of a type
    // designating a globally unique identifier (such as blob:, data:, or
    // filesystem:) At the moment firefox does not support filesystem; but for
    // future compatibility we support it in CSP according to the spec,
    // see: 4.2.2 Matching Source Expressions Note, that allowlisting any of
    // these schemes would call nsCSPSchemeSrc::permits().
    if (aUri->SchemeIs("blob") || aUri->SchemeIs("data") ||
        aUri->SchemeIs("filesystem")) {
      return false;
    }

    // If no scheme is present there also wont be a port and folder to check
    // which means we can return early
    if (mScheme.IsEmpty()) {
      return true;
    }
  }
  // 4.5) host matching: Check if the allowed host starts with a wilcard.
  else if (mHost.First() == '*') {
    NS_ASSERTION(
        mHost[1] == '.',
        "Second character needs to be '.' whenever host starts with '*'");

    // Eliminate leading "*", but keeping the FULL STOP (.) thereafter before
    // checking if the remaining characters match
    nsString wildCardHost = mHost;
    wildCardHost = Substring(wildCardHost, 1, wildCardHost.Length() - 1);
    if (!StringEndsWith(decodedUriHost, wildCardHost)) {
      return false;
    }
  }
  // 4.6) host matching: Check if hosts match.
  else if (!mHost.Equals(decodedUriHost)) {
    return false;
  }

  // Port matching: Check if the ports match.
  if (!permitsPort(mScheme, mPort, aUri)) {
    return false;
  }

  // 4.9) Path matching: If there is a path, we have to enforce
  // path-level matching, unless the channel got redirected, see:
  // http://www.w3.org/TR/CSP11/#source-list-paths-and-redirects
  if (!aWasRedirected && !mPath.IsEmpty()) {
    // converting aUri into nsIURL so we can strip query and ref
    // example.com/test#foo     -> example.com/test
    // example.com/test?val=foo -> example.com/test
    nsCOMPtr<nsIURL> url = do_QueryInterface(aUri);
    if (!url) {
      NS_ASSERTION(false, "can't QI into nsIURI");
      return false;
    }
    nsAutoCString uriPath;
    rv = url->GetFilePath(uriPath);
    NS_ENSURE_SUCCESS(rv, false);

    if (mWithinFrameAncstorsDir) {
      // no path matching for frame-ancestors to not leak any path information.
      return true;
    }

    nsString decodedUriPath;
    CSP_PercentDecodeStr(NS_ConvertUTF8toUTF16(uriPath), decodedUriPath);

    // check if the last character of mPath is '/'; if so
    // we just have to check loading resource is within
    // the allowed path.
    if (mPath.Last() == '/') {
      if (!StringBeginsWith(decodedUriPath, mPath)) {
        return false;
      }
    }
    // otherwise mPath refers to a specific file, and we have to
    // check if the loading resource matches the file.
    else {
      if (!mPath.Equals(decodedUriPath)) {
        return false;
      }
    }
  }

  // At the end: scheme, host, port and path match -> allow the load.
  return true;
}

bool nsCSPHostSrc::visit(nsCSPSrcVisitor* aVisitor) const {
  return aVisitor->visitHostSrc(*this);
}

void nsCSPHostSrc::toString(nsAString& outStr) const {
  if (mGeneratedFromSelfKeyword) {
    outStr.AppendLiteral("'self'");
    return;
  }

  // If mHost is a single "*", we append the wildcard and return.
  if (mHost.EqualsASCII("*") && mScheme.IsEmpty() && mPort.IsEmpty()) {
    outStr.Append(mHost);
    return;
  }

  // append scheme
  outStr.Append(mScheme);

  // append host
  outStr.AppendLiteral("://");
  outStr.Append(mHost);

  // append port
  if (!mPort.IsEmpty()) {
    outStr.AppendLiteral(":");
    outStr.Append(mPort);
  }

  // append path
  outStr.Append(mPath);
}

void nsCSPHostSrc::setScheme(const nsAString& aScheme) {
  mScheme = aScheme;
  ToLowerCase(mScheme);
}

void nsCSPHostSrc::setPort(const nsAString& aPort) { mPort = aPort; }

void nsCSPHostSrc::appendPath(const nsAString& aPath) { mPath.Append(aPath); }

/* ===== nsCSPKeywordSrc ===================== */

nsCSPKeywordSrc::nsCSPKeywordSrc(enum CSPKeyword aKeyword)
    : mKeyword(aKeyword) {
  NS_ASSERTION((aKeyword != CSP_SELF),
               "'self' should have been replaced in the parser");
}

nsCSPKeywordSrc::~nsCSPKeywordSrc() = default;

bool nsCSPKeywordSrc::permits(nsIURI* aUri, const nsAString& aNonce,
                              bool aWasRedirected, bool aReportOnly,
                              bool aUpgradeInsecure,
                              bool aParserCreated) const {
  // no need to check for invalidated, this will always return false unless
  // it is an nsCSPKeywordSrc for 'strict-dynamic', which should allow non
  // parser created scripts.
  return ((mKeyword == CSP_STRICT_DYNAMIC) && !aParserCreated);
}

bool nsCSPKeywordSrc::allows(enum CSPKeyword aKeyword,
                             const nsAString& aHashOrNonce,
                             bool aParserCreated) const {
  CSPUTILSLOG(
      ("nsCSPKeywordSrc::allows, aKeyWord: %s, aHashOrNonce: %s, mInvalidated: "
       "%s",
       CSP_EnumToUTF8Keyword(aKeyword),
       NS_ConvertUTF16toUTF8(aHashOrNonce).get(),
       mInvalidated ? "yes" : "false"));

  if (mInvalidated) {
    // only 'self', 'report-sample' and 'unsafe-inline' are keywords that can be
    // ignored. Please note that the parser already translates 'self' into a uri
    // (see assertion in constructor).
    MOZ_ASSERT(mKeyword == CSP_UNSAFE_INLINE || mKeyword == CSP_REPORT_SAMPLE,
               "should only invalidate unsafe-inline");
    return false;
  }
  // either the keyword allows the load or the policy contains 'strict-dynamic',
  // in which case we have to make sure the script is not parser created before
  // allowing the load and also eval should be blocked even if 'strict-dynamic'
  // is present. Should be allowed only if 'unsafe-eval' is present.
  return ((mKeyword == aKeyword) ||
          ((mKeyword == CSP_STRICT_DYNAMIC) && !aParserCreated &&
           aKeyword != CSP_UNSAFE_EVAL));
}

bool nsCSPKeywordSrc::visit(nsCSPSrcVisitor* aVisitor) const {
  return aVisitor->visitKeywordSrc(*this);
}

void nsCSPKeywordSrc::toString(nsAString& outStr) const {
  outStr.Append(CSP_EnumToUTF16Keyword(mKeyword));
}

/* ===== nsCSPNonceSrc ==================== */

nsCSPNonceSrc::nsCSPNonceSrc(const nsAString& aNonce) : mNonce(aNonce) {}

nsCSPNonceSrc::~nsCSPNonceSrc() = default;

bool nsCSPNonceSrc::permits(nsIURI* aUri, const nsAString& aNonce,
                            bool aWasRedirected, bool aReportOnly,
                            bool aUpgradeInsecure, bool aParserCreated) const {
  if (CSPUTILSLOGENABLED()) {
    CSPUTILSLOG(("nsCSPNonceSrc::permits, aUri: %s, aNonce: %s",
                 aUri->GetSpecOrDefault().get(),
                 NS_ConvertUTF16toUTF8(aNonce).get()));
  }

  if (aReportOnly && aWasRedirected && aNonce.IsEmpty()) {
    /* Fix for Bug 1505412
     *  If we land here, we're currently handling a script-preload which got
     *  redirected. Preloads do not have any info about the nonce assiociated.
     *  Because of Report-Only the preload passes the 1st CSP-check so the
     *  preload does not get retried with a nonce attached.
     *  Currently we're relying on the script-manager to
     *  provide a fake loadinfo to check the preloads against csp.
     *  So during HTTPChannel->OnRedirect we cant check csp for this case.
     *  But as the script-manager already checked the csp,
     *  a report would already have been send,
     *  if the nonce didnt match.
     *  So we can pass the check here for Report-Only Cases.
     */
    MOZ_ASSERT(aParserCreated == false,
               "Skipping nonce-check is only allowed for Preloads");
    return true;
  }

  // nonces can not be invalidated by strict-dynamic
  return mNonce.Equals(aNonce);
}

bool nsCSPNonceSrc::allows(enum CSPKeyword aKeyword,
                           const nsAString& aHashOrNonce,
                           bool aParserCreated) const {
  CSPUTILSLOG(("nsCSPNonceSrc::allows, aKeyWord: %s, a HashOrNonce: %s",
               CSP_EnumToUTF8Keyword(aKeyword),
               NS_ConvertUTF16toUTF8(aHashOrNonce).get()));

  if (aKeyword != CSP_NONCE) {
    return false;
  }
  // nonces can not be invalidated by strict-dynamic
  return mNonce.Equals(aHashOrNonce);
}

bool nsCSPNonceSrc::visit(nsCSPSrcVisitor* aVisitor) const {
  return aVisitor->visitNonceSrc(*this);
}

void nsCSPNonceSrc::toString(nsAString& outStr) const {
  outStr.Append(CSP_EnumToUTF16Keyword(CSP_NONCE));
  outStr.Append(mNonce);
  outStr.AppendLiteral("'");
}

/* ===== nsCSPHashSrc ===================== */

nsCSPHashSrc::nsCSPHashSrc(const nsAString& aAlgo, const nsAString& aHash)
    : mAlgorithm(aAlgo), mHash(aHash) {
  // Only the algo should be rewritten to lowercase, the hash must remain the
  // same.
  ToLowerCase(mAlgorithm);
}

nsCSPHashSrc::~nsCSPHashSrc() = default;

bool nsCSPHashSrc::allows(enum CSPKeyword aKeyword,
                          const nsAString& aHashOrNonce,
                          bool aParserCreated) const {
  CSPUTILSLOG(("nsCSPHashSrc::allows, aKeyWord: %s, a HashOrNonce: %s",
               CSP_EnumToUTF8Keyword(aKeyword),
               NS_ConvertUTF16toUTF8(aHashOrNonce).get()));

  if (aKeyword != CSP_HASH) {
    return false;
  }

  // hashes can not be invalidated by strict-dynamic

  // Convert aHashOrNonce to UTF-8
  NS_ConvertUTF16toUTF8 utf8_hash(aHashOrNonce);

  nsresult rv;
  nsCOMPtr<nsICryptoHash> hasher;
  hasher = do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  NS_ENSURE_SUCCESS(rv, false);

  rv = hasher->InitWithString(NS_ConvertUTF16toUTF8(mAlgorithm));
  NS_ENSURE_SUCCESS(rv, false);

  rv = hasher->Update((uint8_t*)utf8_hash.get(), utf8_hash.Length());
  NS_ENSURE_SUCCESS(rv, false);

  nsAutoCString hash;
  rv = hasher->Finish(true, hash);
  NS_ENSURE_SUCCESS(rv, false);

  return NS_ConvertUTF16toUTF8(mHash).Equals(hash);
}

bool nsCSPHashSrc::visit(nsCSPSrcVisitor* aVisitor) const {
  return aVisitor->visitHashSrc(*this);
}

void nsCSPHashSrc::toString(nsAString& outStr) const {
  outStr.AppendLiteral("'");
  outStr.Append(mAlgorithm);
  outStr.AppendLiteral("-");
  outStr.Append(mHash);
  outStr.AppendLiteral("'");
}

/* ===== nsCSPReportURI ===================== */

nsCSPReportURI::nsCSPReportURI(nsIURI* aURI) : mReportURI(aURI) {}

nsCSPReportURI::~nsCSPReportURI() = default;

bool nsCSPReportURI::visit(nsCSPSrcVisitor* aVisitor) const { return false; }

void nsCSPReportURI::toString(nsAString& outStr) const {
  nsAutoCString spec;
  nsresult rv = mReportURI->GetSpec(spec);
  if (NS_FAILED(rv)) {
    return;
  }
  outStr.AppendASCII(spec.get());
}

/* ===== nsCSPSandboxFlags ===================== */

nsCSPSandboxFlags::nsCSPSandboxFlags(const nsAString& aFlags) : mFlags(aFlags) {
  ToLowerCase(mFlags);
}

nsCSPSandboxFlags::~nsCSPSandboxFlags() = default;

bool nsCSPSandboxFlags::visit(nsCSPSrcVisitor* aVisitor) const { return false; }

void nsCSPSandboxFlags::toString(nsAString& outStr) const {
  outStr.Append(mFlags);
}

/* ===== nsCSPDirective ====================== */

nsCSPDirective::nsCSPDirective(CSPDirective aDirective) {
  mDirective = aDirective;
}

nsCSPDirective::~nsCSPDirective() {
  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    delete mSrcs[i];
  }
}

bool nsCSPDirective::permits(nsIURI* aUri, const nsAString& aNonce,
                             bool aWasRedirected, bool aReportOnly,
                             bool aUpgradeInsecure, bool aParserCreated) const {
  if (CSPUTILSLOGENABLED()) {
    CSPUTILSLOG(
        ("nsCSPDirective::permits, aUri: %s", aUri->GetSpecOrDefault().get()));
  }

  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    if (mSrcs[i]->permits(aUri, aNonce, aWasRedirected, aReportOnly,
                          aUpgradeInsecure, aParserCreated)) {
      return true;
    }
  }
  return false;
}

bool nsCSPDirective::allows(enum CSPKeyword aKeyword,
                            const nsAString& aHashOrNonce,
                            bool aParserCreated) const {
  CSPUTILSLOG(("nsCSPDirective::allows, aKeyWord: %s, a HashOrNonce: %s",
               CSP_EnumToUTF8Keyword(aKeyword),
               NS_ConvertUTF16toUTF8(aHashOrNonce).get()));

  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    if (mSrcs[i]->allows(aKeyword, aHashOrNonce, aParserCreated)) {
      return true;
    }
  }
  return false;
}

void nsCSPDirective::toString(nsAString& outStr) const {
  // Append directive name
  outStr.AppendASCII(CSP_CSPDirectiveToString(mDirective));
  outStr.AppendLiteral(" ");

  // Append srcs
  StringJoinAppend(outStr, u" "_ns, mSrcs,
                   [](nsAString& dest, nsCSPBaseSrc* cspBaseSrc) {
                     cspBaseSrc->toString(dest);
                   });
}

void nsCSPDirective::toDomCSPStruct(mozilla::dom::CSP& outCSP) const {
  mozilla::dom::Sequence<nsString> srcs;
  nsString src;
  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    src.Truncate();
    mSrcs[i]->toString(src);
    if (!srcs.AppendElement(src, mozilla::fallible)) {
      // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
      // involve multiple reallocations) and potentially crashing here,
      // SetCapacity could be called outside the loop once.
      mozalloc_handle_oom(0);
    }
  }

  switch (mDirective) {
    case nsIContentSecurityPolicy::DEFAULT_SRC_DIRECTIVE:
      outCSP.mDefault_src.Construct();
      outCSP.mDefault_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE:
      outCSP.mScript_src.Construct();
      outCSP.mScript_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::OBJECT_SRC_DIRECTIVE:
      outCSP.mObject_src.Construct();
      outCSP.mObject_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::STYLE_SRC_DIRECTIVE:
      outCSP.mStyle_src.Construct();
      outCSP.mStyle_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::IMG_SRC_DIRECTIVE:
      outCSP.mImg_src.Construct();
      outCSP.mImg_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::MEDIA_SRC_DIRECTIVE:
      outCSP.mMedia_src.Construct();
      outCSP.mMedia_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::FRAME_SRC_DIRECTIVE:
      outCSP.mFrame_src.Construct();
      outCSP.mFrame_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::FONT_SRC_DIRECTIVE:
      outCSP.mFont_src.Construct();
      outCSP.mFont_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::CONNECT_SRC_DIRECTIVE:
      outCSP.mConnect_src.Construct();
      outCSP.mConnect_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::REPORT_URI_DIRECTIVE:
      outCSP.mReport_uri.Construct();
      outCSP.mReport_uri.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::FRAME_ANCESTORS_DIRECTIVE:
      outCSP.mFrame_ancestors.Construct();
      outCSP.mFrame_ancestors.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::WEB_MANIFEST_SRC_DIRECTIVE:
      outCSP.mManifest_src.Construct();
      outCSP.mManifest_src.Value() = std::move(srcs);
      return;
      // not supporting REFLECTED_XSS_DIRECTIVE

    case nsIContentSecurityPolicy::BASE_URI_DIRECTIVE:
      outCSP.mBase_uri.Construct();
      outCSP.mBase_uri.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::FORM_ACTION_DIRECTIVE:
      outCSP.mForm_action.Construct();
      outCSP.mForm_action.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::BLOCK_ALL_MIXED_CONTENT:
      outCSP.mBlock_all_mixed_content.Construct();
      // does not have any srcs
      return;

    case nsIContentSecurityPolicy::UPGRADE_IF_INSECURE_DIRECTIVE:
      outCSP.mUpgrade_insecure_requests.Construct();
      // does not have any srcs
      return;

    case nsIContentSecurityPolicy::CHILD_SRC_DIRECTIVE:
      outCSP.mChild_src.Construct();
      outCSP.mChild_src.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::SANDBOX_DIRECTIVE:
      outCSP.mSandbox.Construct();
      outCSP.mSandbox.Value() = std::move(srcs);
      return;

    case nsIContentSecurityPolicy::WORKER_SRC_DIRECTIVE:
      outCSP.mWorker_src.Construct();
      outCSP.mWorker_src.Value() = std::move(srcs);
      return;

    default:
      NS_ASSERTION(false, "cannot find directive to convert CSP to JSON");
  }
}

void nsCSPDirective::getReportURIs(nsTArray<nsString>& outReportURIs) const {
  NS_ASSERTION((mDirective == nsIContentSecurityPolicy::REPORT_URI_DIRECTIVE),
               "not a report-uri directive");

  // append uris
  nsString tmpReportURI;
  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    tmpReportURI.Truncate();
    mSrcs[i]->toString(tmpReportURI);
    outReportURIs.AppendElement(tmpReportURI);
  }
}

bool nsCSPDirective::visitSrcs(nsCSPSrcVisitor* aVisitor) const {
  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    if (!mSrcs[i]->visit(aVisitor)) {
      return false;
    }
  }
  return true;
}

bool nsCSPDirective::equals(CSPDirective aDirective) const {
  return (mDirective == aDirective);
}

void nsCSPDirective::getDirName(nsAString& outStr) const {
  outStr.AppendASCII(CSP_CSPDirectiveToString(mDirective));
}

bool nsCSPDirective::hasReportSampleKeyword() const {
  for (nsCSPBaseSrc* src : mSrcs) {
    if (src->isReportSample()) {
      return true;
    }
  }

  return false;
}

/* =============== nsCSPChildSrcDirective ============= */

nsCSPChildSrcDirective::nsCSPChildSrcDirective(CSPDirective aDirective)
    : nsCSPDirective(aDirective),
      mRestrictFrames(false),
      mRestrictWorkers(false) {}

nsCSPChildSrcDirective::~nsCSPChildSrcDirective() = default;

bool nsCSPChildSrcDirective::equals(CSPDirective aDirective) const {
  if (aDirective == nsIContentSecurityPolicy::FRAME_SRC_DIRECTIVE) {
    return mRestrictFrames;
  }
  if (aDirective == nsIContentSecurityPolicy::WORKER_SRC_DIRECTIVE) {
    return mRestrictWorkers;
  }
  return (mDirective == aDirective);
}

/* =============== nsCSPScriptSrcDirective ============= */

nsCSPScriptSrcDirective::nsCSPScriptSrcDirective(CSPDirective aDirective)
    : nsCSPDirective(aDirective), mRestrictWorkers(false) {}

nsCSPScriptSrcDirective::~nsCSPScriptSrcDirective() = default;

bool nsCSPScriptSrcDirective::equals(CSPDirective aDirective) const {
  if (aDirective == nsIContentSecurityPolicy::WORKER_SRC_DIRECTIVE) {
    return mRestrictWorkers;
  }
  return (mDirective == aDirective);
}

/* =============== nsBlockAllMixedContentDirective ============= */

nsBlockAllMixedContentDirective::nsBlockAllMixedContentDirective(
    CSPDirective aDirective)
    : nsCSPDirective(aDirective) {}

nsBlockAllMixedContentDirective::~nsBlockAllMixedContentDirective() = default;

void nsBlockAllMixedContentDirective::toString(nsAString& outStr) const {
  outStr.AppendASCII(CSP_CSPDirectiveToString(
      nsIContentSecurityPolicy::BLOCK_ALL_MIXED_CONTENT));
}

void nsBlockAllMixedContentDirective::getDirName(nsAString& outStr) const {
  outStr.AppendASCII(CSP_CSPDirectiveToString(
      nsIContentSecurityPolicy::BLOCK_ALL_MIXED_CONTENT));
}

/* =============== nsUpgradeInsecureDirective ============= */

nsUpgradeInsecureDirective::nsUpgradeInsecureDirective(CSPDirective aDirective)
    : nsCSPDirective(aDirective) {}

nsUpgradeInsecureDirective::~nsUpgradeInsecureDirective() = default;

void nsUpgradeInsecureDirective::toString(nsAString& outStr) const {
  outStr.AppendASCII(CSP_CSPDirectiveToString(
      nsIContentSecurityPolicy::UPGRADE_IF_INSECURE_DIRECTIVE));
}

void nsUpgradeInsecureDirective::getDirName(nsAString& outStr) const {
  outStr.AppendASCII(CSP_CSPDirectiveToString(
      nsIContentSecurityPolicy::UPGRADE_IF_INSECURE_DIRECTIVE));
}

/* ===== nsCSPPolicy ========================= */

nsCSPPolicy::nsCSPPolicy()
    : mUpgradeInsecDir(nullptr),
      mReportOnly(false),
      mDeliveredViaMetaTag(false) {
  CSPUTILSLOG(("nsCSPPolicy::nsCSPPolicy"));
}

nsCSPPolicy::~nsCSPPolicy() {
  CSPUTILSLOG(("nsCSPPolicy::~nsCSPPolicy"));

  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    delete mDirectives[i];
  }
}

bool nsCSPPolicy::permits(CSPDirective aDir, nsIURI* aUri,
                          const nsAString& aNonce, bool aWasRedirected,
                          bool aSpecific, bool aParserCreated,
                          nsAString& outViolatedDirective) const {
  if (CSPUTILSLOGENABLED()) {
    CSPUTILSLOG(("nsCSPPolicy::permits, aUri: %s, aDir: %d, aSpecific: %s",
                 aUri->GetSpecOrDefault().get(), aDir,
                 aSpecific ? "true" : "false"));
  }

  NS_ASSERTION(aUri, "permits needs an uri to perform the check!");
  outViolatedDirective.Truncate();

  nsCSPDirective* defaultDir = nullptr;

  // Try to find a relevant directive
  // These directive arrays are short (1-5 elements), not worth using a
  // hashtable.
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(aDir)) {
      if (!mDirectives[i]->permits(aUri, aNonce, aWasRedirected, mReportOnly,
                                   mUpgradeInsecDir, aParserCreated)) {
        mDirectives[i]->getDirName(outViolatedDirective);
        return false;
      }
      return true;
    }
    if (mDirectives[i]->isDefaultDirective()) {
      defaultDir = mDirectives[i];
    }
  }

  // If the above loop runs through, we haven't found a matching directive.
  // Avoid relooping, just store the result of default-src while looping.
  if (!aSpecific && defaultDir) {
    if (!defaultDir->permits(aUri, aNonce, aWasRedirected, mReportOnly,
                             mUpgradeInsecDir, aParserCreated)) {
      defaultDir->getDirName(outViolatedDirective);
      return false;
    }
    return true;
  }

  // Nothing restricts this, so we're allowing the load
  // See bug 764937
  return true;
}

bool nsCSPPolicy::allows(CSPDirective aDirective, enum CSPKeyword aKeyword,
                         const nsAString& aHashOrNonce,
                         bool aParserCreated) const {
  CSPUTILSLOG(("nsCSPPolicy::allows, aKeyWord: %s, a HashOrNonce: %s",
               CSP_EnumToUTF8Keyword(aKeyword),
               NS_ConvertUTF16toUTF8(aHashOrNonce).get()));

  nsCSPDirective* defaultDir = nullptr;

  // Try to find a matching directive
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->isDefaultDirective()) {
      defaultDir = mDirectives[i];
      continue;
    }
    if (mDirectives[i]->equals(aDirective)) {
      if (mDirectives[i]->allows(aKeyword, aHashOrNonce, aParserCreated)) {
        return true;
      }
      return false;
    }
  }

  // {nonce,hash}-source should not consult default-src:
  //   * return false if default-src is specified
  //   * but allow the load if default-src is *not* specified (Bug 1198422)
  if (aKeyword == CSP_NONCE || aKeyword == CSP_HASH) {
    if (!defaultDir) {
      return true;
    }
    return false;
  }

  // If the above loop runs through, we haven't found a matching directive.
  // Avoid relooping, just store the result of default-src while looping.
  if (defaultDir) {
    return defaultDir->allows(aKeyword, aHashOrNonce, aParserCreated);
  }

  // Allowing the load; see Bug 885433
  // a) inline scripts (also unsafe eval) should only be blocked
  //    if there is a [script-src] or [default-src]
  // b) inline styles should only be blocked
  //    if there is a [style-src] or [default-src]
  return true;
}

void nsCSPPolicy::toString(nsAString& outStr) const {
  StringJoinAppend(outStr, u"; "_ns, mDirectives,
                   [](nsAString& dest, nsCSPDirective* cspDirective) {
                     cspDirective->toString(dest);
                   });
}

void nsCSPPolicy::toDomCSPStruct(mozilla::dom::CSP& outCSP) const {
  outCSP.mReport_only = mReportOnly;

  for (uint32_t i = 0; i < mDirectives.Length(); ++i) {
    mDirectives[i]->toDomCSPStruct(outCSP);
  }
}

bool nsCSPPolicy::hasDirective(CSPDirective aDir) const {
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(aDir)) {
      return true;
    }
  }
  return false;
}

bool nsCSPPolicy::allowsNavigateTo(nsIURI* aURI, bool aWasRedirected,
                                   bool aEnforceAllowlist) const {
  bool allowsNavigateTo = true;

  for (unsigned long i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(
            nsIContentSecurityPolicy::NAVIGATE_TO_DIRECTIVE)) {
      // Early return if we can skip the allowlist AND 'unsafe-allow-redirects'
      // is present.
      if (!aEnforceAllowlist &&
          mDirectives[i]->allows(CSP_UNSAFE_ALLOW_REDIRECTS, u""_ns, false)) {
        return true;
      }
      // Otherwise, check against the allowlist.
      if (!mDirectives[i]->permits(aURI, u""_ns, aWasRedirected, false, false,
                                   false)) {
        allowsNavigateTo = false;
      }
    }
  }

  return allowsNavigateTo;
}

/*
 * Use this function only after ::allows() returned 'false'. Most and
 * foremost it's used to get the violated directive before sending reports.
 * The parameter outDirective is the equivalent of 'outViolatedDirective'
 * for the ::permits() function family.
 */
void nsCSPPolicy::getDirectiveStringAndReportSampleForContentType(
    CSPDirective aDirective, nsAString& outDirective,
    bool* aReportSample) const {
  MOZ_ASSERT(aReportSample);
  *aReportSample = false;

  nsCSPDirective* defaultDir = nullptr;
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->isDefaultDirective()) {
      defaultDir = mDirectives[i];
      continue;
    }
    if (mDirectives[i]->equals(aDirective)) {
      mDirectives[i]->getDirName(outDirective);
      *aReportSample = mDirectives[i]->hasReportSampleKeyword();
      return;
    }
  }
  // if we haven't found a matching directive yet,
  // the contentType must be restricted by the default directive
  if (defaultDir) {
    defaultDir->getDirName(outDirective);
    *aReportSample = defaultDir->hasReportSampleKeyword();
    return;
  }
  NS_ASSERTION(false, "Can not query directive string for contentType!");
  outDirective.AppendLiteral("couldNotQueryViolatedDirective");
}

void nsCSPPolicy::getDirectiveAsString(CSPDirective aDir,
                                       nsAString& outDirective) const {
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(aDir)) {
      mDirectives[i]->toString(outDirective);
      return;
    }
  }
}

/*
 * Helper function that returns the underlying bit representation of sandbox
 * flags. The function returns SANDBOXED_NONE if there are no sandbox
 * directives.
 */
uint32_t nsCSPPolicy::getSandboxFlags() const {
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(nsIContentSecurityPolicy::SANDBOX_DIRECTIVE)) {
      nsAutoString flags;
      mDirectives[i]->toString(flags);

      if (flags.IsEmpty()) {
        return SANDBOX_ALL_FLAGS;
      }

      nsAttrValue attr;
      attr.ParseAtomArray(flags);

      return nsContentUtils::ParseSandboxAttributeToFlags(&attr);
    }
  }

  return SANDBOXED_NONE;
}

void nsCSPPolicy::getReportURIs(nsTArray<nsString>& outReportURIs) const {
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(
            nsIContentSecurityPolicy::REPORT_URI_DIRECTIVE)) {
      mDirectives[i]->getReportURIs(outReportURIs);
      return;
    }
  }
}

bool nsCSPPolicy::visitDirectiveSrcs(CSPDirective aDir,
                                     nsCSPSrcVisitor* aVisitor) const {
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(aDir)) {
      return mDirectives[i]->visitSrcs(aVisitor);
    }
  }
  return false;
}
