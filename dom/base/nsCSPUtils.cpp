/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCSPUtils.h"
#include "nsDebug.h"
#include "nsIConsoleService.h"
#include "nsICryptoHash.h"
#include "nsIScriptError.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"

#if defined(PR_LOGGING)
static PRLogModuleInfo*
GetCspUtilsLog()
{
  static PRLogModuleInfo* gCspUtilsPRLog;
  if (!gCspUtilsPRLog)
    gCspUtilsPRLog = PR_NewLogModule("CSPUtils");
  return gCspUtilsPRLog;
}
#endif

#define CSPUTILSLOG(args) PR_LOG(GetCspUtilsLog(), 4, args)

void
CSP_GetLocalizedStr(const char16_t* aName,
                    const char16_t** aParams,
                    uint32_t aLength,
                    char16_t** outResult)
{
  nsCOMPtr<nsIStringBundle> keyStringBundle;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
    mozilla::services::GetStringBundleService();

  NS_ASSERTION(stringBundleService, "String bundle service must be present!");
  stringBundleService->CreateBundle("chrome://global/locale/security/csp.properties",
                                      getter_AddRefs(keyStringBundle));

  NS_ASSERTION(keyStringBundle, "Key string bundle must be available!");

  if (!keyStringBundle) {
    return;
  }
  keyStringBundle->FormatStringFromName(aName, aParams, aLength, outResult);
}

void
CSP_LogStrMessage(const nsAString& aMsg)
{
  nsCOMPtr<nsIConsoleService> console(do_GetService("@mozilla.org/consoleservice;1"));

  if (!console) {
    return;
  }
  nsString msg = PromiseFlatString(aMsg);
  console->LogStringMessage(msg.get());
}

void
CSP_LogMessage(const nsAString& aMessage,
               const nsAString& aSourceName,
               const nsAString& aSourceLine,
               uint32_t aLineNumber,
               uint32_t aColumnNumber,
               uint32_t aFlags,
               const char *aCategory,
               uint32_t aInnerWindowID)
{
  nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));

  if (!console || !error) {
    return;
  }

  // Prepending CSP to the outgoing console message
  nsString cspMsg;
  cspMsg.Append(NS_LITERAL_STRING("Content Security Policy: "));
  cspMsg.Append(aMessage);

  nsresult rv;
  if (aInnerWindowID > 0) {
    nsCString catStr;
    catStr.AssignASCII(aCategory);
    rv = error->InitWithWindowID(cspMsg, aSourceName,
                                 aSourceLine, aLineNumber,
                                 aColumnNumber, aFlags,
                                 catStr, aInnerWindowID);
  }
  else {
    rv = error->Init(cspMsg, aSourceName,
                     aSourceLine, aLineNumber,
                     aColumnNumber, aFlags,
                     aCategory);
  }
  if (NS_FAILED(rv)) {
    return;
  }
  console->LogMessage(error);
}

/**
 * Combines CSP_LogMessage and CSP_GetLocalizedStr into one call.
 */
void
CSP_LogLocalizedStr(const char16_t* aName,
                    const char16_t** aParams,
                    uint32_t aLength,
                    const nsAString& aSourceName,
                    const nsAString& aSourceLine,
                    uint32_t aLineNumber,
                    uint32_t aColumnNumber,
                    uint32_t aFlags,
                    const char* aCategory,
                    uint32_t aInnerWindowID)
{
  nsXPIDLString logMsg;
  CSP_GetLocalizedStr(aName, aParams, aLength, getter_Copies(logMsg));
  CSP_LogMessage(logMsg, aSourceName, aSourceLine,
                 aLineNumber, aColumnNumber, aFlags,
                 aCategory, aInnerWindowID);
}

/* ===== Helpers ============================ */

nsCSPHostSrc*
CSP_CreateHostSrcFromURI(nsIURI* aURI)
{
  // Create the host first
  nsCString host;
  aURI->GetHost(host);
  nsCSPHostSrc *hostsrc = new nsCSPHostSrc(NS_ConvertUTF8toUTF16(host));

  // Add the scheme.
  nsCString scheme;
  aURI->GetScheme(scheme);
  hostsrc->setScheme(NS_ConvertUTF8toUTF16(scheme));

  int32_t port;
  aURI->GetPort(&port);
  // Only add port if it's not default port.
  if (port > 0) {
    nsAutoString portStr;
    portStr.AppendInt(port);
    hostsrc->setPort(portStr);
  }
  return hostsrc;
}

bool
CSP_IsValidDirective(const nsAString& aDir)
{
  static_assert(CSP_LAST_DIRECTIVE_VALUE ==
                (sizeof(CSPStrDirectives) / sizeof(CSPStrDirectives[0])),
                "CSP_LAST_DIRECTIVE_VALUE does not match length of CSPStrDirectives");

  for (uint32_t i = 0; i < CSP_LAST_DIRECTIVE_VALUE; i++) {
    if (aDir.LowerCaseEqualsASCII(CSPStrDirectives[i])) {
      return true;
    }
  }
  return false;
}
bool
CSP_IsDirective(const nsAString& aValue, enum CSPDirective aDir)
{
  return aValue.LowerCaseEqualsASCII(CSP_EnumToDirective(aDir));
}

bool
CSP_IsKeyword(const nsAString& aValue, enum CSPKeyword aKey)
{
  return aValue.LowerCaseEqualsASCII(CSP_EnumToKeyword(aKey));
}

bool
CSP_IsQuotelessKeyword(const nsAString& aKey)
{
  nsString lowerKey = PromiseFlatString(aKey);
  ToLowerCase(lowerKey);

  static_assert(CSP_LAST_KEYWORD_VALUE ==
                (sizeof(CSPStrKeywords) / sizeof(CSPStrKeywords[0])),
                "CSP_LAST_KEYWORD_VALUE does not match length of CSPStrKeywords");

  nsAutoString keyword;
  for (uint32_t i = 0; i < CSP_LAST_KEYWORD_VALUE; i++) {
    // skipping the leading ' and trimming the trailing '
    keyword.AssignASCII(CSPStrKeywords[i] + 1);
    keyword.Trim("'", false, true);
    if (lowerKey.Equals(keyword)) {
      return true;
    }
  }
  return false;
}

/* ===== nsCSPSrc ============================ */

nsCSPBaseSrc::nsCSPBaseSrc()
{
}

nsCSPBaseSrc::~nsCSPBaseSrc()
{
}

// ::permits is only called for external load requests, therefore:
// nsCSPKeywordSrc and nsCSPHashSource fall back to this base class
// implementation which will never allow the load.
bool
nsCSPBaseSrc::permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected) const
{
#ifdef PR_LOGGING
  {
    nsAutoCString spec;
    aUri->GetSpec(spec);
    CSPUTILSLOG(("nsCSPBaseSrc::permits, aUri: %s", spec.get()));
  }
#endif
  return false;
}

// ::allows is only called for inlined loads, therefore:
// nsCSPSchemeSrc, nsCSPHostSrc fall back
// to this base class implementation which will never allow the load.
bool
nsCSPBaseSrc::allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const
{
  CSPUTILSLOG(("nsCSPBaseSrc::allows, aKeyWord: %s, a HashOrNonce: %s",
              aKeyword == CSP_HASH ? "hash" : CSP_EnumToKeyword(aKeyword),
              NS_ConvertUTF16toUTF8(aHashOrNonce).get()));
  return false;
}

/* ====== nsCSPSchemeSrc ===================== */

nsCSPSchemeSrc::nsCSPSchemeSrc(const nsAString& aScheme)
  : mScheme(aScheme)
{
  ToLowerCase(mScheme);
}

nsCSPSchemeSrc::~nsCSPSchemeSrc()
{
}

bool
nsCSPSchemeSrc::permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected) const
{
#ifdef PR_LOGGING
  {
    nsAutoCString spec;
    aUri->GetSpec(spec);
    CSPUTILSLOG(("nsCSPSchemeSrc::permits, aUri: %s", spec.get()));
  }
#endif

  NS_ASSERTION((!mScheme.EqualsASCII("")), "scheme can not be the empty string");
  nsAutoCString scheme;
  nsresult rv = aUri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, false);
  return mScheme.EqualsASCII(scheme.get());
}

void
nsCSPSchemeSrc::toString(nsAString& outStr) const
{
  outStr.Append(mScheme);
  outStr.AppendASCII(":");
}

/* ===== nsCSPHostSrc ======================== */

nsCSPHostSrc::nsCSPHostSrc(const nsAString& aHost)
  : mHost(aHost)
  , mAllowHttps(false)
{
  ToLowerCase(mHost);
}

nsCSPHostSrc::~nsCSPHostSrc()
{
}

bool
nsCSPHostSrc::permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected) const
{
#ifdef PR_LOGGING
  {
    nsAutoCString spec;
    aUri->GetSpec(spec);
    CSPUTILSLOG(("nsCSPHostSrc::permits, aUri: %s", spec.get()));
  }
#endif

  // we are following the enforcement rules from the spec, see:
  // http://www.w3.org/TR/CSP11/#match-source-expression

  // 4.3) scheme matching: Check if the scheme matches.
  nsAutoCString scheme;
  nsresult rv = aUri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, false);
  if (!mScheme.IsEmpty() &&
      !mScheme.EqualsASCII(scheme.get())) {

    // We should not return false for scheme-less sources where the protected resource
    // is http and the load is https, see:
    // http://www.w3.org/TR/CSP2/#match-source-expression
    bool isHttpsScheme =
      (NS_SUCCEEDED(aUri->SchemeIs("https", &isHttpsScheme)) && isHttpsScheme);

    if (!(isHttpsScheme && mAllowHttps)) {
      return false;
    }
  }

  // The host in nsCSpHostSrc should never be empty. In case we are enforcing
  // just a specific scheme, the parser should generate a nsCSPSchemeSource.
  NS_ASSERTION((!mHost.IsEmpty()), "host can not be the empty string");

  // 2) host matching: Enforce a single *
  if (mHost.EqualsASCII("*")) {
    return true;
  }

  // Before we can check if the host matches, we have to
  // extract the host part from aUri.
  nsAutoCString uriHost;
  rv = aUri->GetHost(uriHost);
  NS_ENSURE_SUCCESS(rv, false);

  // 4.5) host matching: Check if the allowed host starts with a wilcard.
  if (mHost.First() == '*') {
    NS_ASSERTION(mHost[1] == '.', "Second character needs to be '.' whenever host starts with '*'");

    // Eliminate leading "*", but keeping the FULL STOP (.) thereafter before checking
    // if the remaining characters match
    nsString wildCardHost = mHost;
    wildCardHost = Substring(wildCardHost, 1, wildCardHost.Length() - 1);
    if (!StringEndsWith(NS_ConvertUTF8toUTF16(uriHost), wildCardHost)) {
      return false;
    }
  }
  // 4.6) host matching: Check if hosts match.
  else if (!mHost.Equals(NS_ConvertUTF8toUTF16(uriHost))) {
    return false;
  }

  // 4.9) Path matching: If there is a path, we have to enforce
  // path-level matching, unless the channel got redirected, see:
  // http://www.w3.org/TR/CSP11/#source-list-paths-and-redirects
  if (!aWasRedirected && !mPath.IsEmpty()) {
    // cloning uri so we can ignore the ref
    nsCOMPtr<nsIURI> uri;
    aUri->CloneIgnoringRef(getter_AddRefs(uri));

    nsAutoCString uriPath;
    rv = uri->GetPath(uriPath);
    NS_ENSURE_SUCCESS(rv, false);
    // check if the last character of mPath is '/'; if so
    // we just have to check loading resource is within
    // the allowed path.
    if (mPath.Last() == '/') {
      if (!StringBeginsWith(NS_ConvertUTF8toUTF16(uriPath), mPath)) {
        return false;
      }
    }
    // otherwise mPath whitelists a specific file, and we have to
    // check if the loading resource matches that whitelisted file.
    else {
      if (!mPath.Equals(NS_ConvertUTF8toUTF16(uriPath))) {
        return false;
      }
    }
  }

  // 4.8) Port matching: If port uses wildcard, allow the load.
  if (mPort.EqualsASCII("*")) {
    return true;
  }

  // Before we can check if the port matches, we have to
  // query the port from aUri.
  int32_t uriPort;
  rv = aUri->GetPort(&uriPort);
  NS_ENSURE_SUCCESS(rv, false);
  uriPort = (uriPort > 0) ? uriPort : NS_GetDefaultPort(scheme.get());

  // 4.7) Default port matching: If mPort is empty, we have to compare default ports.
  if (mPort.IsEmpty()) {
    int32_t port = NS_GetDefaultPort(NS_ConvertUTF16toUTF8(mScheme).get());
    if (port != uriPort) {
      // We should not return false for scheme-less sources where the protected resource
      // is http and the load is https, see: http://www.w3.org/TR/CSP2/#match-source-expression
      // BUT, we only allow scheme-less sources to be upgraded from http to https if CSP
      // does not explicitly define a port.
      if (!(uriPort == NS_GetDefaultPort("https") && mAllowHttps)) {
        return false;
      }
    }
  }
  // 4.7) Port matching: Compare the ports.
  else {
    nsString portStr;
    portStr.AppendInt(uriPort);
    if (!mPort.Equals(portStr)) {
      return false;
    }
  }

  // At the end: scheme, host, path, and port match -> allow the load.
  return true;
}

void
nsCSPHostSrc::toString(nsAString& outStr) const
{
  // If mHost is a single "*", we append the wildcard and return.
  if (mHost.EqualsASCII("*") &&
      mScheme.IsEmpty() &&
      mPort.IsEmpty()) {
    outStr.Append(mHost);
    return;
  }

  // append scheme
  outStr.Append(mScheme);

  // append host
  outStr.AppendASCII("://");
  outStr.Append(mHost);

  // append port
  if (!mPort.IsEmpty()) {
    outStr.AppendASCII(":");
    outStr.Append(mPort);
  }

  // append path
  outStr.Append(mPath);
}

void
nsCSPHostSrc::setScheme(const nsAString& aScheme, bool aAllowHttps)
{
  mScheme = aScheme;
  ToLowerCase(mScheme);
  mAllowHttps = aAllowHttps;
}

void
nsCSPHostSrc::setPort(const nsAString& aPort)
{
  mPort = aPort;
  ToLowerCase(mPort);
}

void
nsCSPHostSrc::appendPath(const nsAString& aPath)
{
  mPath.Append(aPath);
  ToLowerCase(mPath);
}

/* ===== nsCSPKeywordSrc ===================== */

nsCSPKeywordSrc::nsCSPKeywordSrc(CSPKeyword aKeyword)
{
  NS_ASSERTION((aKeyword != CSP_SELF),
               "'self' should have been replaced in the parser");
  mKeyword = aKeyword;
}

nsCSPKeywordSrc::~nsCSPKeywordSrc()
{
}

bool
nsCSPKeywordSrc::allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const
{
  CSPUTILSLOG(("nsCSPKeywordSrc::allows, aKeyWord: %s, a HashOrNonce: %s",
              CSP_EnumToKeyword(aKeyword), NS_ConvertUTF16toUTF8(aHashOrNonce).get()));
  return mKeyword == aKeyword;
}

void
nsCSPKeywordSrc::toString(nsAString& outStr) const
{
  outStr.AppendASCII(CSP_EnumToKeyword(mKeyword));
}

/* ===== nsCSPNonceSrc ==================== */

nsCSPNonceSrc::nsCSPNonceSrc(const nsAString& aNonce)
  : mNonce(aNonce)
{
}

nsCSPNonceSrc::~nsCSPNonceSrc()
{
}

bool
nsCSPNonceSrc::permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected) const
{
#ifdef PR_LOGGING
  {
    nsAutoCString spec;
    aUri->GetSpec(spec);
    CSPUTILSLOG(("nsCSPNonceSrc::permits, aUri: %s, aNonce: %s",
                spec.get(), NS_ConvertUTF16toUTF8(aNonce).get()));
  }
#endif

  return mNonce.Equals(aNonce);
}

bool
nsCSPNonceSrc::allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const
{
  CSPUTILSLOG(("nsCSPNonceSrc::allows, aKeyWord: %s, a HashOrNonce: %s",
              CSP_EnumToKeyword(aKeyword), NS_ConvertUTF16toUTF8(aHashOrNonce).get()));

  if (aKeyword != CSP_NONCE) {
    return false;
  }
  return mNonce.Equals(aHashOrNonce);
}

void
nsCSPNonceSrc::toString(nsAString& outStr) const
{
  outStr.AppendASCII(CSP_EnumToKeyword(CSP_NONCE));
  outStr.Append(mNonce);
  outStr.AppendASCII("'");
}

/* ===== nsCSPHashSrc ===================== */

nsCSPHashSrc::nsCSPHashSrc(const nsAString& aAlgo, const nsAString& aHash)
 : mAlgorithm(aAlgo)
 , mHash(aHash)
{
  // Only the algo should be rewritten to lowercase, the hash must remain the same.
  ToLowerCase(mAlgorithm);
}

nsCSPHashSrc::~nsCSPHashSrc()
{
}

bool
nsCSPHashSrc::allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const
{
  CSPUTILSLOG(("nsCSPHashSrc::allows, aKeyWord: %s, a HashOrNonce: %s",
              CSP_EnumToKeyword(aKeyword), NS_ConvertUTF16toUTF8(aHashOrNonce).get()));

  if (aKeyword != CSP_HASH) {
    return false;
  }

  // Convert aHashOrNonce to UTF-8
  NS_ConvertUTF16toUTF8 utf8_hash(aHashOrNonce);

  nsresult rv;
  nsCOMPtr<nsICryptoHash> hasher;
  hasher = do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  NS_ENSURE_SUCCESS(rv, false);

  rv = hasher->InitWithString(NS_ConvertUTF16toUTF8(mAlgorithm));
  NS_ENSURE_SUCCESS(rv, false);

  rv = hasher->Update((uint8_t *)utf8_hash.get(), utf8_hash.Length());
  NS_ENSURE_SUCCESS(rv, false);

  nsAutoCString hash;
  rv = hasher->Finish(true, hash);
  NS_ENSURE_SUCCESS(rv, false);

  // The NSS Base64 encoder automatically adds linebreaks "\r\n" every 64
  // characters. We need to remove these so we can properly validate longer
  // (SHA-512) base64-encoded hashes
  hash.StripChars("\r\n");
  return NS_ConvertUTF16toUTF8(mHash).Equals(hash);
}

void
nsCSPHashSrc::toString(nsAString& outStr) const
{
  outStr.AppendASCII("'");
  outStr.Append(mAlgorithm);
  outStr.AppendASCII("-");
  outStr.Append(mHash);
  outStr.AppendASCII("'");
}

/* ===== nsCSPReportURI ===================== */

nsCSPReportURI::nsCSPReportURI(nsIURI *aURI)
  :mReportURI(aURI)
{
}

nsCSPReportURI::~nsCSPReportURI()
{
}

void
nsCSPReportURI::toString(nsAString& outStr) const
{
  nsAutoCString spec;
  nsresult rv = mReportURI->GetSpec(spec);
  if (NS_FAILED(rv)) {
    return;
  }
  outStr.AppendASCII(spec.get());
}

/* ===== nsCSPDirective ====================== */

nsCSPDirective::nsCSPDirective(enum CSPDirective aDirective)
{
  mDirective = aDirective;
}

nsCSPDirective::~nsCSPDirective()
{
  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    delete mSrcs[i];
  }
}

bool
nsCSPDirective::permits(nsIURI* aUri, const nsAString& aNonce, bool aWasRedirected) const
{
#ifdef PR_LOGGING
  {
    nsAutoCString spec;
    aUri->GetSpec(spec);
    CSPUTILSLOG(("nsCSPDirective::permits, aUri: %s", spec.get()));
  }
#endif

  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    if (mSrcs[i]->permits(aUri, aNonce, aWasRedirected)) {
      return true;
    }
  }
  return false;
}

bool
nsCSPDirective::permits(nsIURI* aUri) const
{
  nsString dummyNonce;
  return permits(aUri, dummyNonce, false);
}

bool
nsCSPDirective::allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const
{
  CSPUTILSLOG(("nsCSPDirective::allows, aKeyWord: %s, a HashOrNonce: %s",
              CSP_EnumToKeyword(aKeyword), NS_ConvertUTF16toUTF8(aHashOrNonce).get()));

  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    if (mSrcs[i]->allows(aKeyword, aHashOrNonce)) {
      return true;
    }
  }
  return false;
}

void
nsCSPDirective::toString(nsAString& outStr) const
{
  // Append directive name
  outStr.AppendASCII(CSP_EnumToDirective(mDirective));
  outStr.AppendASCII(" ");

  // Append srcs
  uint32_t length = mSrcs.Length();
  for (uint32_t i = 0; i < length; i++) {
    mSrcs[i]->toString(outStr);
    if (i != (length - 1)) {
      outStr.AppendASCII(" ");
    }
  }
}

enum CSPDirective
CSP_ContentTypeToDirective(nsContentPolicyType aType)
{
  switch (aType) {
    case nsIContentPolicy::TYPE_IMAGE:
    case nsIContentPolicy::TYPE_IMAGESET:
      return CSP_IMG_SRC;

    case nsIContentPolicy::TYPE_SCRIPT:
      return CSP_SCRIPT_SRC;

    case nsIContentPolicy::TYPE_STYLESHEET:
      return CSP_STYLE_SRC;

    case nsIContentPolicy::TYPE_FONT:
      return CSP_FONT_SRC;

    case nsIContentPolicy::TYPE_MEDIA:
      return CSP_MEDIA_SRC;

    case nsIContentPolicy::TYPE_SUBDOCUMENT:
      return CSP_FRAME_SRC;

    // BLock XSLT as script, see bug 910139
    case nsIContentPolicy::TYPE_XSLT:
      return CSP_SCRIPT_SRC;

    // TODO(sid): fix this mapping to be more precise (bug 999656)
    case nsIContentPolicy::TYPE_DOCUMENT:
      return CSP_FRAME_ANCESTORS;

    case nsIContentPolicy::TYPE_WEBSOCKET:
    case nsIContentPolicy::TYPE_XMLHTTPREQUEST:
    case nsIContentPolicy::TYPE_BEACON:
    case nsIContentPolicy::TYPE_FETCH:
      return CSP_CONNECT_SRC;

    case nsIContentPolicy::TYPE_OBJECT:
    case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST:
      return CSP_OBJECT_SRC;

    case nsIContentPolicy::TYPE_XBL:
    case nsIContentPolicy::TYPE_PING:
    case nsIContentPolicy::TYPE_DTD:
    case nsIContentPolicy::TYPE_OTHER:
      return CSP_DEFAULT_SRC;

    // CSP can not block csp reports, fall through to error
    case nsIContentPolicy::TYPE_CSP_REPORT:
    // Fall through to error for all other directives
    default:
      NS_ASSERTION(false, "Can not map nsContentPolicyType to CSPDirective");
  }
  return CSP_DEFAULT_SRC;
}

bool
nsCSPDirective::restrictsContentType(nsContentPolicyType aContentType) const
{
  // make sure we do not check for the default src before any other sources
  if (isDefaultDirective()) {
    return false;
  }
  return mDirective == CSP_ContentTypeToDirective(aContentType);
}

void
nsCSPDirective::getReportURIs(nsTArray<nsString> &outReportURIs) const
{
  NS_ASSERTION((mDirective == CSP_REPORT_URI), "not a report-uri directive");

  // append uris
  nsString tmpReportURI;
  for (uint32_t i = 0; i < mSrcs.Length(); i++) {
    tmpReportURI.Truncate();
    mSrcs[i]->toString(tmpReportURI);
    outReportURIs.AppendElement(tmpReportURI);
  }
}

/* ===== nsCSPPolicy ========================= */

nsCSPPolicy::nsCSPPolicy()
  : mReportOnly(false)
{
  CSPUTILSLOG(("nsCSPPolicy::nsCSPPolicy"));
}

nsCSPPolicy::~nsCSPPolicy()
{
  CSPUTILSLOG(("nsCSPPolicy::~nsCSPPolicy"));

  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    delete mDirectives[i];
  }
}

bool
nsCSPPolicy::permits(nsContentPolicyType aContentType,
                     nsIURI* aUri,
                     const nsAString& aNonce,
                     bool aWasRedirected,
                     nsAString& outViolatedDirective) const
{
#ifdef PR_LOGGING
  {
    nsAutoCString spec;
    aUri->GetSpec(spec);
    CSPUTILSLOG(("nsCSPPolicy::permits, aContentType: %d, aUri: %s, aNonce: %s",
                aContentType, spec.get(), NS_ConvertUTF16toUTF8(aNonce).get()));
  }
#endif

  NS_ASSERTION(aUri, "permits needs an uri to perform the check!");

  nsCSPDirective* defaultDir = nullptr;

  // These directive arrays are short (1-5 elements), not worth using a hashtable.

  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    // Check if the directive name matches
    if (mDirectives[i]->restrictsContentType(aContentType)) {
      if (!mDirectives[i]->permits(aUri, aNonce, aWasRedirected)) {
        mDirectives[i]->toString(outViolatedDirective);
        return false;
      }
      return true;
    }
    if (mDirectives[i]->isDefaultDirective()) {
      defaultDir = mDirectives[i];
    }
  }

  // If [frame-ancestors] is not listed explicitly then default to true
  // without consulting [default-src]
  // TODO: currently [frame-ancestors] is mapped to TYPE_DOCUMENT (needs to be fixed)
  if (aContentType == nsIContentPolicy::TYPE_DOCUMENT) {
    return true;
  }

  // If the above loop runs through, we haven't found a matching directive.
  // Avoid relooping, just store the result of default-src while looping.
  if (defaultDir) {
    if (!defaultDir->permits(aUri, aNonce, aWasRedirected)) {
      defaultDir->toString(outViolatedDirective);
      return false;
    }
    return true;
  }

  // unspecified default-src should default to no restrictions
  // see bug 764937
  return true;
}

bool
nsCSPPolicy::permitsBaseURI(nsIURI* aUri) const
{
#ifdef PR_LOGGING
  {
    nsAutoCString spec;
    aUri->GetSpec(spec);
    CSPUTILSLOG(("nsCSPPolicy::permitsBaseURI, aUri: %s", spec.get()));
  }
#endif

  // Try to find a base-uri directive
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(CSP_BASE_URI)) {
      return mDirectives[i]->permits(aUri);
    }
  }

  // base-uri is only enforced if explicitly defined in the
  // policy - do *not* consult default-src, see:
  // http://www.w3.org/TR/CSP11/#directive-default-src
  return true;
}

bool
nsCSPPolicy::allows(nsContentPolicyType aContentType,
                    enum CSPKeyword aKeyword,
                    const nsAString& aHashOrNonce) const
{
  CSPUTILSLOG(("nsCSPPolicy::allows, aKeyWord: %s, a HashOrNonce: %s",
              CSP_EnumToKeyword(aKeyword), NS_ConvertUTF16toUTF8(aHashOrNonce).get()));

  nsCSPDirective* defaultDir = nullptr;

  // Try to find a matching directive
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->restrictsContentType(aContentType)) {
      if (mDirectives[i]->allows(aKeyword, aHashOrNonce)) {
        return true;
      }
      return false;
    }
    if (mDirectives[i]->isDefaultDirective()) {
      defaultDir = mDirectives[i];
    }
  }

  // Only match {nonce,hash}-source on specific directives (not default-src)
  if (aKeyword == CSP_NONCE || aKeyword == CSP_HASH) {
    return false;
  }

  // If the above loop runs through, we haven't found a matching directive.
  // Avoid relooping, just store the result of default-src while looping.
  if (defaultDir) {
    return defaultDir->allows(aKeyword, aHashOrNonce);
  }

  // Allowing the load; see Bug 885433
  // a) inline scripts (also unsafe eval) should only be blocked
  //    if there is a [script-src] or [default-src]
  // b) inline styles should only be blocked
  //    if there is a [style-src] or [default-src]
  return true;
}

bool
nsCSPPolicy::allows(nsContentPolicyType aContentType,
                    enum CSPKeyword aKeyword) const
{
  return allows(aContentType, aKeyword, NS_LITERAL_STRING(""));
}

void
nsCSPPolicy::toString(nsAString& outStr) const
{
  uint32_t length = mDirectives.Length();
  for (uint32_t i = 0; i < length; ++i) {
    mDirectives[i]->toString(outStr);
    if (i != (length - 1)) {
      outStr.AppendASCII("; ");
    }
  }
}

bool
nsCSPPolicy::directiveExists(enum CSPDirective aDir) const
{
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(aDir)) {
      return true;
    }
  }
  return false;
}

/*
 * Use this function only after ::allows() returned 'false'. Most and
 * foremost it's used to get the violated directive before sending reports.
 * The parameter outDirective is the equivalent of 'outViolatedDirective'
 * for the ::permits() function family.
 */
void
nsCSPPolicy::getDirectiveStringForContentType(nsContentPolicyType aContentType,
                                              nsAString& outDirective) const
{
  nsCSPDirective* defaultDir = nullptr;
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->restrictsContentType(aContentType)) {
      mDirectives[i]->toString(outDirective);
      return;
    }
    if (mDirectives[i]->isDefaultDirective()) {
      defaultDir = mDirectives[i];
    }
  }
  // if we haven't found a matching directive yet,
  // the contentType must be restricted by the default directive
  if (defaultDir) {
    defaultDir->toString(outDirective);
    return;
  }
  NS_ASSERTION(false, "Can not query directive string for contentType!");
  outDirective.AppendASCII("couldNotQueryViolatedDirective");
}

void
nsCSPPolicy::getDirectiveStringForBaseURI(nsAString& outDirective) const
{
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(CSP_BASE_URI)) {
      mDirectives[i]->toString(outDirective);
      return;
    }
  }
}

void
nsCSPPolicy::getReportURIs(nsTArray<nsString>& outReportURIs) const
{
  for (uint32_t i = 0; i < mDirectives.Length(); i++) {
    if (mDirectives[i]->equals(CSP_REPORT_URI)) {
      mDirectives[i]->getReportURIs(outReportURIs);
      return;
    }
  }
}
