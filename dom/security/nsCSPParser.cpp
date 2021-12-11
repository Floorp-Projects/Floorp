/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_security.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCSPParser.h"
#include "nsCSPUtils.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

static LogModule* GetCspParserLog() {
  static LazyLogModule gCspParserPRLog("CSPParser");
  return gCspParserPRLog;
}

#define CSPPARSERLOG(args) \
  MOZ_LOG(GetCspParserLog(), mozilla::LogLevel::Debug, args)
#define CSPPARSERLOGENABLED() \
  MOZ_LOG_TEST(GetCspParserLog(), mozilla::LogLevel::Debug)

static const uint32_t kSubHostPathCharacterCutoff = 512;

static const char* const kHashSourceValidFns[] = {"sha256", "sha384", "sha512"};
static const uint32_t kHashSourceValidFnsLen = 3;

/* ===== nsCSPParser ==================== */

nsCSPParser::nsCSPParser(policyTokens& aTokens, nsIURI* aSelfURI,
                         nsCSPContext* aCSPContext, bool aDeliveredViaMetaTag)
    : mCurChar(nullptr),
      mEndChar(nullptr),
      mHasHashOrNonce(false),
      mStrictDynamic(false),
      mUnsafeInlineKeywordSrc(nullptr),
      mChildSrc(nullptr),
      mFrameSrc(nullptr),
      mWorkerSrc(nullptr),
      mScriptSrc(nullptr),
      mParsingFrameAncestorsDir(false),
      mTokens(aTokens.Clone()),
      mSelfURI(aSelfURI),
      mPolicy(nullptr),
      mCSPContext(aCSPContext),
      mDeliveredViaMetaTag(aDeliveredViaMetaTag) {
  CSPPARSERLOG(("nsCSPParser::nsCSPParser"));
}

nsCSPParser::~nsCSPParser() { CSPPARSERLOG(("nsCSPParser::~nsCSPParser")); }

static bool isCharacterToken(char16_t aSymbol) {
  return (aSymbol >= 'a' && aSymbol <= 'z') ||
         (aSymbol >= 'A' && aSymbol <= 'Z');
}

bool isNumberToken(char16_t aSymbol) {
  return (aSymbol >= '0' && aSymbol <= '9');
}

bool isValidHexDig(char16_t aHexDig) {
  return (isNumberToken(aHexDig) || (aHexDig >= 'A' && aHexDig <= 'F') ||
          (aHexDig >= 'a' && aHexDig <= 'f'));
}

static bool isValidBase64Value(const char16_t* cur, const char16_t* end) {
  // Using grammar at
  // https://w3c.github.io/webappsec-csp/#grammardef-nonce-source

  // May end with one or two =
  if (end > cur && *(end - 1) == EQUALS) end--;
  if (end > cur && *(end - 1) == EQUALS) end--;

  // Must have at least one character aside from any =
  if (end == cur) {
    return false;
  }

  // Rest must all be A-Za-z0-9+/-_
  for (; cur < end; ++cur) {
    if (!(isCharacterToken(*cur) || isNumberToken(*cur) || *cur == PLUS ||
          *cur == SLASH || *cur == DASH || *cur == UNDERLINE)) {
      return false;
    }
  }

  return true;
}

void nsCSPParser::resetCurChar(const nsAString& aToken) {
  mCurChar = aToken.BeginReading();
  mEndChar = aToken.EndReading();
  resetCurValue();
}

// The path is terminated by the first question mark ("?") or
// number sign ("#") character, or by the end of the URI.
// http://tools.ietf.org/html/rfc3986#section-3.3
bool nsCSPParser::atEndOfPath() {
  return (atEnd() || peek(QUESTIONMARK) || peek(NUMBER_SIGN));
}

// unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
bool nsCSPParser::atValidUnreservedChar() {
  return (peek(isCharacterToken) || peek(isNumberToken) || peek(DASH) ||
          peek(DOT) || peek(UNDERLINE) || peek(TILDE));
}

// sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
//                 / "*" / "+" / "," / ";" / "="
// Please note that even though ',' and ';' appear to be
// valid sub-delims according to the RFC production of paths,
// both can not appear here by itself, they would need to be
// pct-encoded in order to be part of the path.
bool nsCSPParser::atValidSubDelimChar() {
  return (peek(EXCLAMATION) || peek(DOLLAR) || peek(AMPERSAND) ||
          peek(SINGLEQUOTE) || peek(OPENBRACE) || peek(CLOSINGBRACE) ||
          peek(WILDCARD) || peek(PLUS) || peek(EQUALS));
}

// pct-encoded   = "%" HEXDIG HEXDIG
bool nsCSPParser::atValidPctEncodedChar() {
  const char16_t* pctCurChar = mCurChar;

  if ((pctCurChar + 2) >= mEndChar) {
    // string too short, can't be a valid pct-encoded char.
    return false;
  }

  // Any valid pct-encoding must follow the following format:
  // "% HEXDIG HEXDIG"
  if (PERCENT_SIGN != *pctCurChar || !isValidHexDig(*(pctCurChar + 1)) ||
      !isValidHexDig(*(pctCurChar + 2))) {
    return false;
  }
  return true;
}

// pchar = unreserved / pct-encoded / sub-delims / ":" / "@"
// http://tools.ietf.org/html/rfc3986#section-3.3
bool nsCSPParser::atValidPathChar() {
  return (atValidUnreservedChar() || atValidSubDelimChar() ||
          atValidPctEncodedChar() || peek(COLON) || peek(ATSYMBOL));
}

void nsCSPParser::logWarningErrorToConsole(uint32_t aSeverityFlag,
                                           const char* aProperty,
                                           const nsTArray<nsString>& aParams) {
  CSPPARSERLOG(("nsCSPParser::logWarningErrorToConsole: %s", aProperty));
  // send console messages off to the context and let the context
  // deal with it (potentially messages need to be queued up)
  mCSPContext->logToConsole(aProperty, aParams,
                            u""_ns,          // aSourceName
                            u""_ns,          // aSourceLine
                            0,               // aLineNumber
                            0,               // aColumnNumber
                            aSeverityFlag);  // aFlags
}

bool nsCSPParser::hostChar() {
  if (atEnd()) {
    return false;
  }
  return accept(isCharacterToken) || accept(isNumberToken) || accept(DASH);
}

// (ALPHA / DIGIT / "+" / "-" / "." )
bool nsCSPParser::schemeChar() {
  if (atEnd()) {
    return false;
  }
  return accept(isCharacterToken) || accept(isNumberToken) || accept(PLUS) ||
         accept(DASH) || accept(DOT);
}

// port = ":" ( 1*DIGIT / "*" )
bool nsCSPParser::port() {
  CSPPARSERLOG(("nsCSPParser::port, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Consume the COLON we just peeked at in houstSource
  accept(COLON);

  // Resetting current value since we start to parse a port now.
  // e.g; "http://www.example.com:8888" then we have already parsed
  // everything up to (including) ":";
  resetCurValue();

  // Port might be "*"
  if (accept(WILDCARD)) {
    return true;
  }

  // Port must start with a number
  if (!accept(isNumberToken)) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag, "couldntParsePort",
                             params);
    return false;
  }
  // Consume more numbers and set parsed port to the nsCSPHost
  while (accept(isNumberToken)) { /* consume */
  }
  return true;
}

bool nsCSPParser::subPath(nsCSPHostSrc* aCspHost) {
  CSPPARSERLOG(("nsCSPParser::subPath, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Emergency exit to avoid endless loops in case a path in a CSP policy
  // is longer than 512 characters, or also to avoid endless loops
  // in case we are parsing unrecognized characters in the following loop.
  uint32_t charCounter = 0;
  nsString pctDecodedSubPath;

  while (!atEndOfPath()) {
    if (peek(SLASH)) {
      // before appendig any additional portion of a subpath we have to
      // pct-decode that portion of the subpath. atValidPathChar() already
      // verified a correct pct-encoding, now we can safely decode and append
      // the decoded-sub path.
      CSP_PercentDecodeStr(mCurValue, pctDecodedSubPath);
      aCspHost->appendPath(pctDecodedSubPath);
      // Resetting current value since we are appending parts of the path
      // to aCspHost, e.g; "http://www.example.com/path1/path2" then the
      // first part is "/path1", second part "/path2"
      resetCurValue();
    } else if (!atValidPathChar()) {
      AutoTArray<nsString, 1> params = {mCurToken};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "couldntParseInvalidSource", params);
      return false;
    }
    // potentially we have encountred a valid pct-encoded character in
    // atValidPathChar(); if so, we have to account for "% HEXDIG HEXDIG" and
    // advance the pointer past the pct-encoded char.
    if (peek(PERCENT_SIGN)) {
      advance();
      advance();
    }
    advance();
    if (++charCounter > kSubHostPathCharacterCutoff) {
      return false;
    }
  }
  // before appendig any additional portion of a subpath we have to pct-decode
  // that portion of the subpath. atValidPathChar() already verified a correct
  // pct-encoding, now we can safely decode and append the decoded-sub path.
  CSP_PercentDecodeStr(mCurValue, pctDecodedSubPath);
  aCspHost->appendPath(pctDecodedSubPath);
  resetCurValue();
  return true;
}

bool nsCSPParser::path(nsCSPHostSrc* aCspHost) {
  CSPPARSERLOG(("nsCSPParser::path, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Resetting current value and forgetting everything we have parsed so far
  // e.g. parsing "http://www.example.com/path1/path2", then
  // "http://www.example.com" has already been parsed so far
  // forget about it.
  resetCurValue();

  if (!accept(SLASH)) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "couldntParseInvalidSource", params);
    return false;
  }
  if (atEndOfPath()) {
    // one slash right after host [port] is also considered a path, e.g.
    // www.example.com/ should result in www.example.com/
    // please note that we do not have to perform any pct-decoding here
    // because we are just appending a '/' and not any actual chars.
    aCspHost->appendPath(u"/"_ns);
    return true;
  }
  // path can begin with "/" but not "//"
  // see http://tools.ietf.org/html/rfc3986#section-3.3
  if (peek(SLASH)) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "couldntParseInvalidSource", params);
    return false;
  }
  return subPath(aCspHost);
}

bool nsCSPParser::subHost() {
  CSPPARSERLOG(("nsCSPParser::subHost, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Emergency exit to avoid endless loops in case a host in a CSP policy
  // is longer than 512 characters, or also to avoid endless loops
  // in case we are parsing unrecognized characters in the following loop.
  uint32_t charCounter = 0;

  while (!atEndOfPath() && !peek(COLON) && !peek(SLASH)) {
    ++charCounter;
    while (hostChar()) {
      /* consume */
      ++charCounter;
    }
    if (accept(DOT) && !hostChar()) {
      return false;
    }
    if (charCounter > kSubHostPathCharacterCutoff) {
      return false;
    }
  }
  return true;
}

// host = "*" / [ "*." ] 1*host-char *( "." 1*host-char )
nsCSPHostSrc* nsCSPParser::host() {
  CSPPARSERLOG(("nsCSPParser::host, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if the token starts with "*"; please remember that we handle
  // a single "*" as host in sourceExpression, but we still have to handle
  // the case where a scheme was defined, e.g., as:
  // "https://*", "*.example.com", "*:*", etc.
  if (accept(WILDCARD)) {
    // Might solely be the wildcard
    if (atEnd() || peek(COLON)) {
      return new nsCSPHostSrc(mCurValue);
    }
    // If the token is not only the "*", a "." must follow right after
    if (!accept(DOT)) {
      AutoTArray<nsString, 1> params = {mCurToken};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "couldntParseInvalidHost", params);
      return nullptr;
    }
  }

  // Expecting at least one host-char
  if (!hostChar()) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "couldntParseInvalidHost", params);
    return nullptr;
  }

  // There might be several sub hosts defined.
  if (!subHost()) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "couldntParseInvalidHost", params);
    return nullptr;
  }

  // HostName might match a keyword, log to the console.
  if (CSP_IsQuotelessKeyword(mCurValue)) {
    nsString keyword = mCurValue;
    ToLowerCase(keyword);
    AutoTArray<nsString, 2> params = {mCurToken, keyword};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "hostNameMightBeKeyword", params);
  }

  // Create a new nsCSPHostSrc with the parsed host.
  return new nsCSPHostSrc(mCurValue);
}

// keyword-source = "'self'" / "'unsafe-inline'" / "'unsafe-eval'"
nsCSPBaseSrc* nsCSPParser::keywordSource() {
  CSPPARSERLOG(("nsCSPParser::keywordSource, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Special case handling for 'self' which is not stored internally as a
  // keyword, but rather creates a nsCSPHostSrc using the selfURI
  if (CSP_IsKeyword(mCurToken, CSP_SELF)) {
    return CSP_CreateHostSrcFromSelfURI(mSelfURI);
  }

  if (CSP_IsKeyword(mCurToken, CSP_REPORT_SAMPLE)) {
    return new nsCSPKeywordSrc(CSP_UTF16KeywordToEnum(mCurToken));
  }

  if (CSP_IsKeyword(mCurToken, CSP_STRICT_DYNAMIC)) {
    if (!CSP_IsDirective(mCurDir[0],
                         nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE)) {
      // Todo: Enforce 'strict-dynamic' within default-src; see Bug 1313937
      AutoTArray<nsString, 1> params = {u"strict-dynamic"_ns};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "ignoringStrictDynamic", params);
      return nullptr;
    }
    mStrictDynamic = true;
    return new nsCSPKeywordSrc(CSP_UTF16KeywordToEnum(mCurToken));
  }

  if (CSP_IsKeyword(mCurToken, CSP_UNSAFE_INLINE)) {
    nsWeakPtr ctx = mCSPContext->GetLoadingContext();
    nsCOMPtr<Document> doc = do_QueryReferent(ctx);
    if (doc) {
      doc->SetHasUnsafeInlineCSP(true);
    }
    // make sure script-src only contains 'unsafe-inline' once;
    // ignore duplicates and log warning
    if (mUnsafeInlineKeywordSrc) {
      AutoTArray<nsString, 1> params = {mCurToken};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "ignoringDuplicateSrc", params);
      return nullptr;
    }
    // cache if we encounter 'unsafe-inline' so we can invalidate (ignore) it in
    // case that script-src directive also contains hash- or nonce-.
    mUnsafeInlineKeywordSrc =
        new nsCSPKeywordSrc(CSP_UTF16KeywordToEnum(mCurToken));
    return mUnsafeInlineKeywordSrc;
  }

  if (CSP_IsKeyword(mCurToken, CSP_UNSAFE_EVAL)) {
    nsWeakPtr ctx = mCSPContext->GetLoadingContext();
    nsCOMPtr<Document> doc = do_QueryReferent(ctx);
    if (doc) {
      doc->SetHasUnsafeEvalCSP(true);
    }
    return new nsCSPKeywordSrc(CSP_UTF16KeywordToEnum(mCurToken));
  }

  if (CSP_IsKeyword(mCurToken, CSP_UNSAFE_ALLOW_REDIRECTS)) {
    if (!CSP_IsDirective(mCurDir[0],
                         nsIContentSecurityPolicy::NAVIGATE_TO_DIRECTIVE)) {
      // Only allow 'unsafe-allow-redirects' within navigate-to.
      AutoTArray<nsString, 2> params = {u"unsafe-allow-redirects"_ns,
                                        u"navigate-to"_ns};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "IgnoringSourceWithinDirective", params);
      return nullptr;
    }

    return new nsCSPKeywordSrc(CSP_UTF16KeywordToEnum(mCurToken));
  }

  return nullptr;
}

// host-source = [ scheme "://" ] host [ port ] [ path ]
nsCSPHostSrc* nsCSPParser::hostSource() {
  CSPPARSERLOG(("nsCSPParser::hostSource, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  nsCSPHostSrc* cspHost = host();
  if (!cspHost) {
    // Error was reported in host()
    return nullptr;
  }

  // Calling port() to see if there is a port to parse, if an error
  // occurs, port() reports the error, if port() returns true;
  // we have a valid port, so we add it to cspHost.
  if (peek(COLON)) {
    if (!port()) {
      delete cspHost;
      return nullptr;
    }
    cspHost->setPort(mCurValue);
  }

  if (atEndOfPath()) {
    return cspHost;
  }

  // Calling path() to see if there is a path to parse, if an error
  // occurs, path() reports the error; handing cspHost as an argument
  // which simplifies parsing of several paths.
  if (!path(cspHost)) {
    // If the host [port] is followed by a path, it has to be a valid path,
    // otherwise we pass the nullptr, indicating an error, up the callstack.
    // see also http://www.w3.org/TR/CSP11/#source-list
    delete cspHost;
    return nullptr;
  }
  return cspHost;
}

// scheme-source = scheme ":"
nsCSPSchemeSrc* nsCSPParser::schemeSource() {
  CSPPARSERLOG(("nsCSPParser::schemeSource, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  if (!accept(isCharacterToken)) {
    return nullptr;
  }
  while (schemeChar()) { /* consume */
  }
  nsString scheme = mCurValue;

  // If the potential scheme is not followed by ":" - it's not a scheme
  if (!accept(COLON)) {
    return nullptr;
  }

  // If the chraracter following the ":" is a number or the "*"
  // then we are not parsing a scheme; but rather a host;
  if (peek(isNumberToken) || peek(WILDCARD)) {
    return nullptr;
  }

  return new nsCSPSchemeSrc(scheme);
}

// nonce-source = "'nonce-" nonce-value "'"
nsCSPNonceSrc* nsCSPParser::nonceSource() {
  CSPPARSERLOG(("nsCSPParser::nonceSource, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if mCurToken begins with "'nonce-" and ends with "'"
  if (!StringBeginsWith(mCurToken,
                        nsDependentString(CSP_EnumToUTF16Keyword(CSP_NONCE)),
                        nsASCIICaseInsensitiveStringComparator) ||
      mCurToken.Last() != SINGLEQUOTE) {
    return nullptr;
  }

  // Trim surrounding single quotes
  const nsAString& expr = Substring(mCurToken, 1, mCurToken.Length() - 2);

  int32_t dashIndex = expr.FindChar(DASH);
  if (dashIndex < 0) {
    return nullptr;
  }
  if (!isValidBase64Value(expr.BeginReading() + dashIndex + 1,
                          expr.EndReading())) {
    return nullptr;
  }

  // cache if encountering hash or nonce to invalidate unsafe-inline
  mHasHashOrNonce = true;
  return new nsCSPNonceSrc(
      Substring(expr, dashIndex + 1, expr.Length() - dashIndex + 1));
}

// hash-source = "'" hash-algo "-" base64-value "'"
nsCSPHashSrc* nsCSPParser::hashSource() {
  CSPPARSERLOG(("nsCSPParser::hashSource, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if mCurToken starts and ends with "'"
  if (mCurToken.First() != SINGLEQUOTE || mCurToken.Last() != SINGLEQUOTE) {
    return nullptr;
  }

  // Trim surrounding single quotes
  const nsAString& expr = Substring(mCurToken, 1, mCurToken.Length() - 2);

  int32_t dashIndex = expr.FindChar(DASH);
  if (dashIndex < 0) {
    return nullptr;
  }

  if (!isValidBase64Value(expr.BeginReading() + dashIndex + 1,
                          expr.EndReading())) {
    return nullptr;
  }

  nsAutoString algo(Substring(expr, 0, dashIndex));
  nsAutoString hash(
      Substring(expr, dashIndex + 1, expr.Length() - dashIndex + 1));

  for (uint32_t i = 0; i < kHashSourceValidFnsLen; i++) {
    if (algo.LowerCaseEqualsASCII(kHashSourceValidFns[i])) {
      // cache if encountering hash or nonce to invalidate unsafe-inline
      mHasHashOrNonce = true;
      return new nsCSPHashSrc(algo, hash);
    }
  }
  return nullptr;
}

// source-expression = scheme-source / host-source / keyword-source
//                     / nonce-source / hash-source
nsCSPBaseSrc* nsCSPParser::sourceExpression() {
  CSPPARSERLOG(("nsCSPParser::sourceExpression, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if it is a keyword
  if (nsCSPBaseSrc* cspKeyword = keywordSource()) {
    return cspKeyword;
  }

  // Check if it is a nonce-source
  if (nsCSPNonceSrc* cspNonce = nonceSource()) {
    return cspNonce;
  }

  // Check if it is a hash-source
  if (nsCSPHashSrc* cspHash = hashSource()) {
    return cspHash;
  }

  // We handle a single "*" as host here, to avoid any confusion when applying
  // the default scheme. However, we still would need to apply the default
  // scheme in case we would parse "*:80".
  if (mCurToken.EqualsASCII("*")) {
    return new nsCSPHostSrc(u"*"_ns);
  }

  // Calling resetCurChar allows us to use mCurChar and mEndChar
  // to parse mCurToken; e.g. mCurToken = "http://www.example.com", then
  // mCurChar = 'h'
  // mEndChar = points just after the last 'm'
  // mCurValue = ""
  resetCurChar(mCurToken);

  // Check if mCurToken starts with a scheme
  nsAutoString parsedScheme;
  if (nsCSPSchemeSrc* cspScheme = schemeSource()) {
    // mCurToken might only enforce a specific scheme
    if (atEnd()) {
      return cspScheme;
    }
    // If something follows the scheme, we do not create
    // a nsCSPSchemeSrc, but rather a nsCSPHostSrc, which
    // needs to know the scheme to enforce; remember the
    // scheme and delete cspScheme;
    cspScheme->toString(parsedScheme);
    parsedScheme.Trim(":", false, true);
    delete cspScheme;

    // If mCurToken provides not only a scheme, but also a host, we have to
    // check if two slashes follow the scheme.
    if (!accept(SLASH) || !accept(SLASH)) {
      AutoTArray<nsString, 1> params = {mCurToken};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "failedToParseUnrecognizedSource", params);
      return nullptr;
    }
  }

  // Calling resetCurValue allows us to keep pointers for mCurChar and mEndChar
  // alive, but resets mCurValue; e.g. mCurToken = "http://www.example.com",
  // then mCurChar = 'w' mEndChar = 'm' mCurValue = ""
  resetCurValue();

  // If mCurToken does not provide a scheme (scheme-less source), we apply the
  // scheme from selfURI
  if (parsedScheme.IsEmpty()) {
    // Resetting internal helpers, because we might already have parsed some of
    // the host when trying to parse a scheme.
    resetCurChar(mCurToken);
    nsAutoCString selfScheme;
    mSelfURI->GetScheme(selfScheme);
    parsedScheme.AssignASCII(selfScheme.get());
  }

  // At this point we are expecting a host to be parsed.
  // Trying to create a new nsCSPHost.
  if (nsCSPHostSrc* cspHost = hostSource()) {
    // Do not forget to set the parsed scheme.
    cspHost->setScheme(parsedScheme);
    cspHost->setWithinFrameAncestorsDir(mParsingFrameAncestorsDir);
    return cspHost;
  }
  // Error was reported in hostSource()
  return nullptr;
}

// source-list = *WSP [ source-expression *( 1*WSP source-expression ) *WSP ]
//               / *WSP "'none'" *WSP
void nsCSPParser::sourceList(nsTArray<nsCSPBaseSrc*>& outSrcs) {
  bool isNone = false;

  // remember, srcs start at index 1
  for (uint32_t i = 1; i < mCurDir.Length(); i++) {
    // mCurToken is only set here and remains the current token
    // to be processed, which avoid passing arguments between functions.
    mCurToken = mCurDir[i];
    resetCurValue();

    CSPPARSERLOG(("nsCSPParser::sourceList, mCurToken: %s, mCurValue: %s",
                  NS_ConvertUTF16toUTF8(mCurToken).get(),
                  NS_ConvertUTF16toUTF8(mCurValue).get()));

    // Special case handling for none:
    // Ignore 'none' if any other src is available.
    // (See http://www.w3.org/TR/CSP11/#parsing)
    if (CSP_IsKeyword(mCurToken, CSP_NONE)) {
      isNone = true;
      continue;
    }
    // Must be a regular source expression
    nsCSPBaseSrc* src = sourceExpression();
    if (src) {
      outSrcs.AppendElement(src);
    }
  }

  // Check if the directive contains a 'none'
  if (isNone) {
    // If the directive contains no other srcs, then we set the 'none'
    if (outSrcs.IsEmpty() ||
        (outSrcs.Length() == 1 && outSrcs[0]->isReportSample())) {
      nsCSPKeywordSrc* keyword = new nsCSPKeywordSrc(CSP_NONE);
      outSrcs.InsertElementAt(0, keyword);
    }
    // Otherwise, we ignore 'none' and report a warning
    else {
      AutoTArray<nsString, 1> params;
      params.AppendElement(CSP_EnumToUTF16Keyword(CSP_NONE));
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "ignoringUnknownOption", params);
    }
  }
}

void nsCSPParser::reportURIList(nsCSPDirective* aDir) {
  CSPPARSERLOG(("nsCSPParser::reportURIList"));

  nsTArray<nsCSPBaseSrc*> srcs;
  nsCOMPtr<nsIURI> uri;
  nsresult rv;

  // remember, srcs start at index 1
  for (uint32_t i = 1; i < mCurDir.Length(); i++) {
    mCurToken = mCurDir[i];

    CSPPARSERLOG(("nsCSPParser::reportURIList, mCurToken: %s, mCurValue: %s",
                  NS_ConvertUTF16toUTF8(mCurToken).get(),
                  NS_ConvertUTF16toUTF8(mCurValue).get()));

    rv = NS_NewURI(getter_AddRefs(uri), mCurToken, "", mSelfURI);

    // If creating the URI casued an error, skip this URI
    if (NS_FAILED(rv)) {
      AutoTArray<nsString, 1> params = {mCurToken};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "couldNotParseReportURI", params);
      continue;
    }

    // Create new nsCSPReportURI and append to the list.
    nsCSPReportURI* reportURI = new nsCSPReportURI(uri);
    srcs.AppendElement(reportURI);
  }

  if (srcs.Length() == 0) {
    AutoTArray<nsString, 1> directiveName = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "ignoringDirectiveWithNoValues", directiveName);
    delete aDir;
    return;
  }

  aDir->addSrcs(srcs);
  mPolicy->addDirective(aDir);
}

/* Helper function for parsing sandbox flags. This function solely concatenates
 * all the source list tokens (the sandbox flags) so the attribute parser
 * (nsContentUtils::ParseSandboxAttributeToFlags) can parse them.
 */
void nsCSPParser::sandboxFlagList(nsCSPDirective* aDir) {
  CSPPARSERLOG(("nsCSPParser::sandboxFlagList"));

  nsAutoString flags;

  // remember, srcs start at index 1
  for (uint32_t i = 1; i < mCurDir.Length(); i++) {
    mCurToken = mCurDir[i];

    CSPPARSERLOG(("nsCSPParser::sandboxFlagList, mCurToken: %s, mCurValue: %s",
                  NS_ConvertUTF16toUTF8(mCurToken).get(),
                  NS_ConvertUTF16toUTF8(mCurValue).get()));

    if (!nsContentUtils::IsValidSandboxFlag(mCurToken)) {
      AutoTArray<nsString, 1> params = {mCurToken};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "couldntParseInvalidSandboxFlag", params);
      continue;
    }

    flags.Append(mCurToken);
    if (i != mCurDir.Length() - 1) {
      flags.AppendLiteral(" ");
    }
  }

  // Please note that the sandbox directive can exist
  // by itself (not containing any flags).
  nsTArray<nsCSPBaseSrc*> srcs;
  srcs.AppendElement(new nsCSPSandboxFlags(flags));
  aDir->addSrcs(srcs);
  mPolicy->addDirective(aDir);
}

// directive-value = *( WSP / <VCHAR except ";" and ","> )
void nsCSPParser::directiveValue(nsTArray<nsCSPBaseSrc*>& outSrcs) {
  CSPPARSERLOG(("nsCSPParser::directiveValue"));

  // Just forward to sourceList
  sourceList(outSrcs);
}

// directive-name = 1*( ALPHA / DIGIT / "-" )
nsCSPDirective* nsCSPParser::directiveName() {
  CSPPARSERLOG(("nsCSPParser::directiveName, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if it is a valid directive
  CSPDirective directive = CSP_StringToCSPDirective(mCurToken);
  if (directive == nsIContentSecurityPolicy::NO_DIRECTIVE) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "couldNotProcessUnknownDirective", params);
    return nullptr;
  }

  // The directive 'reflected-xss' is part of CSP 1.1, see:
  // http://www.w3.org/TR/2014/WD-CSP11-20140211/#reflected-xss
  // Currently we are not supporting that directive, hence we log a
  // warning to the console and ignore the directive including its values.
  if (directive == nsIContentSecurityPolicy::REFLECTED_XSS_DIRECTIVE) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "notSupportingDirective", params);
    return nullptr;
  }

  // Bug 1529068: Implement navigate-to directive.
  // Once all corner cases are resolved we can remove that special
  // if-handling here and let the parser just fall through to
  // return new nsCSPDirective.
  if (directive == nsIContentSecurityPolicy::NAVIGATE_TO_DIRECTIVE &&
      !StaticPrefs::security_csp_enableNavigateTo()) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "couldNotProcessUnknownDirective", params);
    return nullptr;
  }

  // Make sure the directive does not already exist
  // (see http://www.w3.org/TR/CSP11/#parsing)
  if (mPolicy->hasDirective(directive)) {
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag, "duplicateDirective",
                             params);
    return nullptr;
  }

  // CSP delivered via meta tag should ignore the following directives:
  // report-uri, frame-ancestors, and sandbox, see:
  // http://www.w3.org/TR/CSP11/#delivery-html-meta-element
  if (mDeliveredViaMetaTag &&
      ((directive == nsIContentSecurityPolicy::REPORT_URI_DIRECTIVE) ||
       (directive == nsIContentSecurityPolicy::FRAME_ANCESTORS_DIRECTIVE) ||
       (directive == nsIContentSecurityPolicy::SANDBOX_DIRECTIVE))) {
    // log to the console to indicate that meta CSP is ignoring the directive
    AutoTArray<nsString, 1> params = {mCurToken};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "ignoringSrcFromMetaCSP", params);
    return nullptr;
  }

  // special case handling for block-all-mixed-content
  if (directive == nsIContentSecurityPolicy::BLOCK_ALL_MIXED_CONTENT) {
    return new nsBlockAllMixedContentDirective(directive);
  }

  // special case handling for upgrade-insecure-requests
  if (directive == nsIContentSecurityPolicy::UPGRADE_IF_INSECURE_DIRECTIVE) {
    return new nsUpgradeInsecureDirective(directive);
  }

  // if we have a child-src, cache it as a fallback for
  //   * workers (if worker-src is not explicitly specified)
  //   * frames  (if frame-src is not explicitly specified)
  if (directive == nsIContentSecurityPolicy::CHILD_SRC_DIRECTIVE) {
    mChildSrc = new nsCSPChildSrcDirective(directive);
    return mChildSrc;
  }

  // if we have a frame-src, cache it so we can discard child-src for frames
  if (directive == nsIContentSecurityPolicy::FRAME_SRC_DIRECTIVE) {
    mFrameSrc = new nsCSPDirective(directive);
    return mFrameSrc;
  }

  // if we have a worker-src, cache it so we can discard child-src for workers
  if (directive == nsIContentSecurityPolicy::WORKER_SRC_DIRECTIVE) {
    mWorkerSrc = new nsCSPDirective(directive);
    return mWorkerSrc;
  }

  // if we have a script-src, cache it as a fallback for worker-src
  // in case child-src is not present
  if (directive == nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE) {
    mScriptSrc = new nsCSPScriptSrcDirective(directive);
    return mScriptSrc;
  }

  return new nsCSPDirective(directive);
}

// directive = *WSP [ directive-name [ WSP directive-value ] ]
void nsCSPParser::directive() {
  // Set the directiveName to mCurToken
  // Remember, the directive name is stored at index 0
  mCurToken = mCurDir[0];

  CSPPARSERLOG(("nsCSPParser::directive, mCurToken: %s, mCurValue: %s",
                NS_ConvertUTF16toUTF8(mCurToken).get(),
                NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Make sure that the directive-srcs-array contains at least
  // one directive and one src.
  if (mCurDir.Length() < 1) {
    AutoTArray<nsString, 1> params = {u"directive missing"_ns};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "failedToParseUnrecognizedSource", params);
    return;
  }

  if (CSP_IsEmptyDirective(mCurValue, mCurToken)) {
    return;
  }

  // Try to create a new CSPDirective
  nsCSPDirective* cspDir = directiveName();
  if (!cspDir) {
    // if we can not create a CSPDirective, we can skip parsing the srcs for
    // that array
    return;
  }

  // special case handling for block-all-mixed-content, which is only specified
  // by a directive name but does not include any srcs.
  if (cspDir->equals(nsIContentSecurityPolicy::BLOCK_ALL_MIXED_CONTENT)) {
    if (mCurDir.Length() > 1) {
      AutoTArray<nsString, 1> params = {u"block-all-mixed-content"_ns};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "ignoreSrcForDirective", params);
    }
    // add the directive and return
    mPolicy->addDirective(cspDir);
    return;
  }

  // special case handling for upgrade-insecure-requests, which is only
  // specified by a directive name but does not include any srcs.
  if (cspDir->equals(nsIContentSecurityPolicy::UPGRADE_IF_INSECURE_DIRECTIVE)) {
    if (mCurDir.Length() > 1) {
      AutoTArray<nsString, 1> params = {u"upgrade-insecure-requests"_ns};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "ignoreSrcForDirective", params);
    }
    // add the directive and return
    mPolicy->addUpgradeInsecDir(
        static_cast<nsUpgradeInsecureDirective*>(cspDir));
    return;
  }

  // special case handling for report-uri directive (since it doesn't contain
  // a valid source list but rather actual URIs)
  if (CSP_IsDirective(mCurDir[0],
                      nsIContentSecurityPolicy::REPORT_URI_DIRECTIVE)) {
    reportURIList(cspDir);
    return;
  }

  // special case handling for sandbox directive (since it doe4sn't contain
  // a valid source list but rather special sandbox flags)
  if (CSP_IsDirective(mCurDir[0],
                      nsIContentSecurityPolicy::SANDBOX_DIRECTIVE)) {
    sandboxFlagList(cspDir);
    return;
  }

  // make sure to reset cache variables when trying to invalidate unsafe-inline;
  // unsafe-inline might not only appear in script-src, but also in default-src
  mHasHashOrNonce = false;
  mStrictDynamic = false;
  mUnsafeInlineKeywordSrc = nullptr;

  mParsingFrameAncestorsDir = CSP_IsDirective(
      mCurDir[0], nsIContentSecurityPolicy::FRAME_ANCESTORS_DIRECTIVE);

  // Try to parse all the srcs by handing the array off to directiveValue
  nsTArray<nsCSPBaseSrc*> srcs;
  directiveValue(srcs);

  // If we can not parse any srcs; we let the source expression be the empty set
  // ('none') see, http://www.w3.org/TR/CSP11/#source-list-parsing
  if (srcs.IsEmpty() || (srcs.Length() == 1 && srcs[0]->isReportSample())) {
    nsCSPKeywordSrc* keyword = new nsCSPKeywordSrc(CSP_NONE);
    srcs.InsertElementAt(0, keyword);
  }

  // If policy contains 'strict-dynamic' invalidate all srcs within script-src.
  if (mStrictDynamic) {
    MOZ_ASSERT(cspDir->equals(nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE),
               "strict-dynamic only allowed within script-src");
    for (uint32_t i = 0; i < srcs.Length(); i++) {
      // Please note that nsCSPNonceSrc as well as nsCSPHashSrc overwrite
      // invalidate(), so it's fine to just call invalidate() on all srcs.
      // Please also note that nsCSPKeywordSrc() can not be invalidated and
      // always returns false unless the keyword is 'strict-dynamic' in which
      // case we allow the load if the script is not parser created!
      srcs[i]->invalidate();
      // Log a message to the console that src will be ignored.
      nsAutoString srcStr;
      srcs[i]->toString(srcStr);
      // Even though we invalidate all of the srcs internally, we don't want to
      // log messages for the srcs: (1) strict-dynamic, (2) unsafe-inline, (3)
      // nonces, and (4) hashes
      if (!srcStr.EqualsASCII(CSP_EnumToUTF8Keyword(CSP_STRICT_DYNAMIC)) &&
          !srcStr.EqualsASCII(CSP_EnumToUTF8Keyword(CSP_UNSAFE_EVAL)) &&
          !StringBeginsWith(
              srcStr, nsDependentString(CSP_EnumToUTF16Keyword(CSP_NONCE))) &&
          !StringBeginsWith(srcStr, u"'sha"_ns)) {
        AutoTArray<nsString, 1> params = {srcStr};
        logWarningErrorToConsole(nsIScriptError::warningFlag,
                                 "ignoringSrcForStrictDynamic", params);
      }
    }
    // Log a warning that all scripts might be blocked because the policy
    // contains 'strict-dynamic' but no valid nonce or hash.
    if (!mHasHashOrNonce) {
      AutoTArray<nsString, 1> params = {mCurDir[0]};
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "strictDynamicButNoHashOrNonce", params);
    }
  } else if (mHasHashOrNonce && mUnsafeInlineKeywordSrc &&
             (cspDir->equals(nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE) ||
              cspDir->equals(nsIContentSecurityPolicy::STYLE_SRC_DIRECTIVE))) {
    mUnsafeInlineKeywordSrc->invalidate();
    // log to the console that unsafe-inline will be ignored
    AutoTArray<nsString, 1> params = {u"'unsafe-inline'"_ns};
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "ignoringSrcWithinScriptStyleSrc", params);
  }

  // Add the newly created srcs to the directive and add the directive to the
  // policy
  cspDir->addSrcs(srcs);
  mPolicy->addDirective(cspDir);
}

// policy = [ directive *( ";" [ directive ] ) ]
nsCSPPolicy* nsCSPParser::policy() {
  CSPPARSERLOG(("nsCSPParser::policy"));

  mPolicy = new nsCSPPolicy();
  for (uint32_t i = 0; i < mTokens.Length(); i++) {
    // All input is already tokenized; set one tokenized array in the form of
    // [ name, src, src, ... ]
    // to mCurDir and call directive which processes the current directive.
    mCurDir = mTokens[i].Clone();
    directive();
  }

  if (mChildSrc) {
    if (!mFrameSrc) {
      // if frame-src is specified explicitly for that policy than child-src
      // should not restrict frames; if not, than child-src needs to restrict
      // frames.
      mChildSrc->setRestrictFrames();
    }
    if (!mWorkerSrc) {
      // if worker-src is specified explicitly for that policy than child-src
      // should not restrict workers; if not, than child-src needs to restrict
      // workers.
      mChildSrc->setRestrictWorkers();
    }
  }
  // if script-src is specified, but not worker-src and also no child-src, then
  // script-src has to govern workers.
  if (mScriptSrc && !mWorkerSrc && !mChildSrc) {
    mScriptSrc->setRestrictWorkers();
  }

  return mPolicy;
}

nsCSPPolicy* nsCSPParser::parseContentSecurityPolicy(
    const nsAString& aPolicyString, nsIURI* aSelfURI, bool aReportOnly,
    nsCSPContext* aCSPContext, bool aDeliveredViaMetaTag) {
  if (CSPPARSERLOGENABLED()) {
    CSPPARSERLOG(("nsCSPParser::parseContentSecurityPolicy, policy: %s",
                  NS_ConvertUTF16toUTF8(aPolicyString).get()));
    CSPPARSERLOG(("nsCSPParser::parseContentSecurityPolicy, selfURI: %s",
                  aSelfURI->GetSpecOrDefault().get()));
    CSPPARSERLOG(("nsCSPParser::parseContentSecurityPolicy, reportOnly: %s",
                  (aReportOnly ? "true" : "false")));
    CSPPARSERLOG(
        ("nsCSPParser::parseContentSecurityPolicy, deliveredViaMetaTag: %s",
         (aDeliveredViaMetaTag ? "true" : "false")));
  }

  NS_ASSERTION(aSelfURI, "Can not parseContentSecurityPolicy without aSelfURI");

  // Separate all input into tokens and store them in the form of:
  // [ [ name, src, src, ... ], [ name, src, src, ... ], ... ]
  // The tokenizer itself can not fail; all eventual errors
  // are detected in the parser itself.

  nsTArray<CopyableTArray<nsString> > tokens;
  PolicyTokenizer::tokenizePolicy(aPolicyString, tokens);

  nsCSPParser parser(tokens, aSelfURI, aCSPContext, aDeliveredViaMetaTag);

  // Start the parser to generate a new CSPPolicy using the generated tokens.
  nsCSPPolicy* policy = parser.policy();

  // Check that report-only policies define a report-uri, otherwise log warning.
  if (aReportOnly) {
    policy->setReportOnlyFlag(true);
    if (!policy->hasDirective(nsIContentSecurityPolicy::REPORT_URI_DIRECTIVE)) {
      nsAutoCString prePath;
      nsresult rv = aSelfURI->GetPrePath(prePath);
      NS_ENSURE_SUCCESS(rv, policy);
      AutoTArray<nsString, 1> params;
      CopyUTF8toUTF16(prePath, *params.AppendElement());
      parser.logWarningErrorToConsole(nsIScriptError::warningFlag,
                                      "reportURInotInReportOnlyHeader", params);
    }
  }

  policy->setDeliveredViaMetaTagFlag(aDeliveredViaMetaTag);

  if (policy->getNumDirectives() == 0) {
    // Individual errors were already reported in the parser, but if
    // we do not have an enforcable directive at all, we return null.
    delete policy;
    return nullptr;
  }

  if (CSPPARSERLOGENABLED()) {
    nsString parsedPolicy;
    policy->toString(parsedPolicy);
    CSPPARSERLOG(("nsCSPParser::parseContentSecurityPolicy, parsedPolicy: %s",
                  NS_ConvertUTF16toUTF8(parsedPolicy).get()));
  }

  return policy;
}
