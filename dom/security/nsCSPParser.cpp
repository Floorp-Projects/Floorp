/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/SizePrintfMacros.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCSPParser.h"
#include "nsCSPUtils.h"
#include "nsIConsoleService.h"
#include "nsIContentPolicy.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "mozilla/net/ReferrerPolicy.h"

using namespace mozilla;

static LogModule*
GetCspParserLog()
{
  static LazyLogModule gCspParserPRLog("CSPParser");
  return gCspParserPRLog;
}

#define CSPPARSERLOG(args) MOZ_LOG(GetCspParserLog(), mozilla::LogLevel::Debug, args)
#define CSPPARSERLOGENABLED() MOZ_LOG_TEST(GetCspParserLog(), mozilla::LogLevel::Debug)

static const char16_t COLON        = ':';
static const char16_t SEMICOLON    = ';';
static const char16_t SLASH        = '/';
static const char16_t PLUS         = '+';
static const char16_t DASH         = '-';
static const char16_t DOT          = '.';
static const char16_t UNDERLINE    = '_';
static const char16_t TILDE        = '~';
static const char16_t WILDCARD     = '*';
static const char16_t SINGLEQUOTE  = '\'';
static const char16_t NUMBER_SIGN  = '#';
static const char16_t QUESTIONMARK = '?';
static const char16_t PERCENT_SIGN = '%';
static const char16_t EXCLAMATION  = '!';
static const char16_t DOLLAR       = '$';
static const char16_t AMPERSAND    = '&';
static const char16_t OPENBRACE    = '(';
static const char16_t CLOSINGBRACE = ')';
static const char16_t EQUALS       = '=';
static const char16_t ATSYMBOL     = '@';

static const uint32_t kSubHostPathCharacterCutoff = 512;

static const char *const kHashSourceValidFns [] = { "sha256", "sha384", "sha512" };
static const uint32_t kHashSourceValidFnsLen = 3;

static const char* const kStyle    = "style";
static const char* const kScript   = "script";

/* ===== nsCSPTokenizer ==================== */

nsCSPTokenizer::nsCSPTokenizer(const char16_t* aStart,
                               const char16_t* aEnd)
  : mCurChar(aStart)
  , mEndChar(aEnd)
{
  CSPPARSERLOG(("nsCSPTokenizer::nsCSPTokenizer"));
}

nsCSPTokenizer::~nsCSPTokenizer()
{
  CSPPARSERLOG(("nsCSPTokenizer::~nsCSPTokenizer"));
}

void
nsCSPTokenizer::generateNextToken()
{
  skipWhiteSpaceAndSemicolon();
  while (!atEnd() &&
         !nsContentUtils::IsHTMLWhitespace(*mCurChar) &&
         *mCurChar != SEMICOLON) {
    mCurToken.Append(*mCurChar++);
  }
  CSPPARSERLOG(("nsCSPTokenizer::generateNextToken: %s", NS_ConvertUTF16toUTF8(mCurToken).get()));
}

void
nsCSPTokenizer::generateTokens(cspTokens& outTokens)
{
  CSPPARSERLOG(("nsCSPTokenizer::generateTokens"));

  // dirAndSrcs holds one set of [ name, src, src, src, ... ]
  nsTArray <nsString> dirAndSrcs;

  while (!atEnd()) {
    generateNextToken();
    dirAndSrcs.AppendElement(mCurToken);
    skipWhiteSpace();
    if (atEnd() || accept(SEMICOLON)) {
      outTokens.AppendElement(dirAndSrcs);
      dirAndSrcs.Clear();
    }
  }
}

void
nsCSPTokenizer::tokenizeCSPPolicy(const nsAString &aPolicyString,
                                  cspTokens& outTokens)
{
  CSPPARSERLOG(("nsCSPTokenizer::tokenizeCSPPolicy"));

  nsCSPTokenizer tokenizer(aPolicyString.BeginReading(),
                           aPolicyString.EndReading());

  tokenizer.generateTokens(outTokens);
}

/* ===== nsCSPParser ==================== */
bool nsCSPParser::sCSPExperimentalEnabled = false;
bool nsCSPParser::sStrictDynamicEnabled = false;

nsCSPParser::nsCSPParser(cspTokens& aTokens,
                         nsIURI* aSelfURI,
                         nsCSPContext* aCSPContext,
                         bool aDeliveredViaMetaTag)
 : mCurChar(nullptr)
 , mEndChar(nullptr)
 , mHasHashOrNonce(false)
 , mStrictDynamic(false)
 , mUnsafeInlineKeywordSrc(nullptr)
 , mChildSrc(nullptr)
 , mFrameSrc(nullptr)
 , mTokens(aTokens)
 , mSelfURI(aSelfURI)
 , mPolicy(nullptr)
 , mCSPContext(aCSPContext)
 , mDeliveredViaMetaTag(aDeliveredViaMetaTag)
{
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    Preferences::AddBoolVarCache(&sCSPExperimentalEnabled, "security.csp.experimentalEnabled");
    Preferences::AddBoolVarCache(&sStrictDynamicEnabled, "security.csp.enableStrictDynamic");
  }
  CSPPARSERLOG(("nsCSPParser::nsCSPParser"));
}

nsCSPParser::~nsCSPParser()
{
  CSPPARSERLOG(("nsCSPParser::~nsCSPParser"));
}

static bool
isCharacterToken(char16_t aSymbol)
{
  return (aSymbol >= 'a' && aSymbol <= 'z') ||
         (aSymbol >= 'A' && aSymbol <= 'Z');
}

static bool
isNumberToken(char16_t aSymbol)
{
  return (aSymbol >= '0' && aSymbol <= '9');
}

static bool
isValidHexDig(char16_t aHexDig)
{
  return (isNumberToken(aHexDig) ||
          (aHexDig >= 'A' && aHexDig <= 'F') ||
          (aHexDig >= 'a' && aHexDig <= 'f'));
}

static bool
isValidBase64Value(const char16_t* cur, const char16_t* end)
{
  // Using grammar at https://w3c.github.io/webappsec-csp/#grammardef-nonce-source

  // May end with one or two =
  if (end > cur && *(end-1) == EQUALS) end--;
  if (end > cur && *(end-1) == EQUALS) end--;

  // Must have at least one character aside from any =
  if (end == cur) {
    return false;
  }

  // Rest must all be A-Za-z0-9+/-_
  for (; cur < end; ++cur) {
    if (!(isCharacterToken(*cur) || isNumberToken(*cur) ||
          *cur == PLUS || *cur == SLASH ||
          *cur == DASH || *cur == UNDERLINE)) {
      return false;
    }
  }

  return true;
}

void
nsCSPParser::resetCurChar(const nsAString& aToken)
{
  mCurChar = aToken.BeginReading();
  mEndChar = aToken.EndReading();
  resetCurValue();
}

// The path is terminated by the first question mark ("?") or
// number sign ("#") character, or by the end of the URI.
// http://tools.ietf.org/html/rfc3986#section-3.3
bool
nsCSPParser::atEndOfPath()
{
  return (atEnd() || peek(QUESTIONMARK) || peek(NUMBER_SIGN));
}

// unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
bool
nsCSPParser::atValidUnreservedChar()
{
  return (peek(isCharacterToken) || peek(isNumberToken) ||
          peek(DASH) || peek(DOT) ||
          peek(UNDERLINE) || peek(TILDE));
}

// sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
//                 / "*" / "+" / "," / ";" / "="
// Please note that even though ',' and ';' appear to be
// valid sub-delims according to the RFC production of paths,
// both can not appear here by itself, they would need to be
// pct-encoded in order to be part of the path.
bool
nsCSPParser::atValidSubDelimChar()
{
  return (peek(EXCLAMATION) || peek(DOLLAR) || peek(AMPERSAND) ||
          peek(SINGLEQUOTE) || peek(OPENBRACE) || peek(CLOSINGBRACE) ||
          peek(WILDCARD) || peek(PLUS) || peek(EQUALS));
}

// pct-encoded   = "%" HEXDIG HEXDIG
bool
nsCSPParser::atValidPctEncodedChar()
{
  const char16_t* pctCurChar = mCurChar;

  if ((pctCurChar + 2) >= mEndChar) {
    // string too short, can't be a valid pct-encoded char.
    return false;
  }

  // Any valid pct-encoding must follow the following format:
  // "% HEXDIG HEXDIG"
  if (PERCENT_SIGN != *pctCurChar ||
     !isValidHexDig(*(pctCurChar+1)) ||
     !isValidHexDig(*(pctCurChar+2))) {
    return false;
  }
  return true;
}

// pchar = unreserved / pct-encoded / sub-delims / ":" / "@"
// http://tools.ietf.org/html/rfc3986#section-3.3
bool
nsCSPParser::atValidPathChar()
{
  return (atValidUnreservedChar() ||
          atValidSubDelimChar() ||
          atValidPctEncodedChar() ||
          peek(COLON) || peek(ATSYMBOL));
}

void
nsCSPParser::logWarningErrorToConsole(uint32_t aSeverityFlag,
                                      const char* aProperty,
                                      const char16_t* aParams[],
                                      uint32_t aParamsLength)
{
  CSPPARSERLOG(("nsCSPParser::logWarningErrorToConsole: %s", aProperty));
  // send console messages off to the context and let the context
  // deal with it (potentially messages need to be queued up)
  mCSPContext->logToConsole(NS_ConvertUTF8toUTF16(aProperty).get(),
                            aParams,
                            aParamsLength,
                            EmptyString(), // aSourceName
                            EmptyString(), // aSourceLine
                            0,             // aLineNumber
                            0,             // aColumnNumber
                            aSeverityFlag); // aFlags
}

bool
nsCSPParser::hostChar()
{
  if (atEnd()) {
    return false;
  }
  return accept(isCharacterToken) ||
         accept(isNumberToken) ||
         accept(DASH);
}

// (ALPHA / DIGIT / "+" / "-" / "." )
bool
nsCSPParser::schemeChar()
{
  if (atEnd()) {
    return false;
  }
  return accept(isCharacterToken) ||
         accept(isNumberToken) ||
         accept(PLUS) ||
         accept(DASH) ||
         accept(DOT);
}

// port = ":" ( 1*DIGIT / "*" )
bool
nsCSPParser::port()
{
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
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "couldntParsePort",
                             params, ArrayLength(params));
    return false;
  }
  // Consume more numbers and set parsed port to the nsCSPHost
  while (accept(isNumberToken)) { /* consume */ }
  return true;
}

bool
nsCSPParser::subPath(nsCSPHostSrc* aCspHost)
{
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
      // before appendig any additional portion of a subpath we have to pct-decode
      // that portion of the subpath. atValidPathChar() already verified a correct
      // pct-encoding, now we can safely decode and append the decoded-sub path.
      CSP_PercentDecodeStr(mCurValue, pctDecodedSubPath);
      aCspHost->appendPath(pctDecodedSubPath);
      // Resetting current value since we are appending parts of the path
      // to aCspHost, e.g; "http://www.example.com/path1/path2" then the
      // first part is "/path1", second part "/path2"
      resetCurValue();
    }
    else if (!atValidPathChar()) {
      const char16_t* params[] = { mCurToken.get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "couldntParseInvalidSource",
                               params, ArrayLength(params));
      return false;
    }
    // potentially we have encountred a valid pct-encoded character in atValidPathChar();
    // if so, we have to account for "% HEXDIG HEXDIG" and advance the pointer past
    // the pct-encoded char.
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

bool
nsCSPParser::path(nsCSPHostSrc* aCspHost)
{
  CSPPARSERLOG(("nsCSPParser::path, mCurToken: %s, mCurValue: %s",
               NS_ConvertUTF16toUTF8(mCurToken).get(),
               NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Resetting current value and forgetting everything we have parsed so far
  // e.g. parsing "http://www.example.com/path1/path2", then
  // "http://www.example.com" has already been parsed so far
  // forget about it.
  resetCurValue();

  if (!accept(SLASH)) {
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "couldntParseInvalidSource",
                             params, ArrayLength(params));
    return false;
  }
  if (atEndOfPath()) {
    // one slash right after host [port] is also considered a path, e.g.
    // www.example.com/ should result in www.example.com/
    // please note that we do not have to perform any pct-decoding here
    // because we are just appending a '/' and not any actual chars.
    aCspHost->appendPath(NS_LITERAL_STRING("/"));
    return true;
  }
  // path can begin with "/" but not "//"
  // see http://tools.ietf.org/html/rfc3986#section-3.3
  if (peek(SLASH)) {
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "couldntParseInvalidSource",
                             params, ArrayLength(params));
    return false;
  }
  return subPath(aCspHost);
}

bool
nsCSPParser::subHost()
{
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
nsCSPHostSrc*
nsCSPParser::host()
{
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
      const char16_t* params[] = { mCurToken.get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag, "couldntParseInvalidHost",
                               params, ArrayLength(params));
      return nullptr;
    }
  }

  // Expecting at least one host-char
  if (!hostChar()) {
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "couldntParseInvalidHost",
                             params, ArrayLength(params));
    return nullptr;
  }

  // There might be several sub hosts defined.
  if (!subHost()) {
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "couldntParseInvalidHost",
                             params, ArrayLength(params));
    return nullptr;
  }

  // HostName might match a keyword, log to the console.
  if (CSP_IsQuotelessKeyword(mCurValue)) {
    nsString keyword = mCurValue;
    ToLowerCase(keyword);
    const char16_t* params[] = { mCurToken.get(), keyword.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "hostNameMightBeKeyword",
                             params, ArrayLength(params));
  }

  // Create a new nsCSPHostSrc with the parsed host.
  return new nsCSPHostSrc(mCurValue);
}

// keyword-source = "'self'" / "'unsafe-inline'" / "'unsafe-eval'"
nsCSPBaseSrc*
nsCSPParser::keywordSource()
{
  CSPPARSERLOG(("nsCSPParser::keywordSource, mCurToken: %s, mCurValue: %s",
               NS_ConvertUTF16toUTF8(mCurToken).get(),
               NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Special case handling for 'self' which is not stored internally as a keyword,
  // but rather creates a nsCSPHostSrc using the selfURI
  if (CSP_IsKeyword(mCurToken, CSP_SELF)) {
    return CSP_CreateHostSrcFromSelfURI(mSelfURI);
  }

  if (CSP_IsKeyword(mCurToken, CSP_STRICT_DYNAMIC)) {
    // make sure strict dynamic is enabled
    if (!sStrictDynamicEnabled) {
      return nullptr;
    }
    if (!CSP_IsDirective(mCurDir[0], nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE)) {
      // Todo: Enforce 'strict-dynamic' within default-src; see Bug 1313937
      const char16_t* params[] = { u"strict-dynamic" };
      logWarningErrorToConsole(nsIScriptError::warningFlag, "ignoringStrictDynamic",
                               params, ArrayLength(params));
      return nullptr;
    }
    mStrictDynamic = true;
    return new nsCSPKeywordSrc(CSP_KeywordToEnum(mCurToken));
  }

  if (CSP_IsKeyword(mCurToken, CSP_UNSAFE_INLINE)) {
      nsWeakPtr ctx = mCSPContext->GetLoadingContext();
      nsCOMPtr<nsIDocument> doc = do_QueryReferent(ctx);
      if (doc) {
        doc->SetHasUnsafeInlineCSP(true);
      }
    // make sure script-src only contains 'unsafe-inline' once;
    // ignore duplicates and log warning
    if (mUnsafeInlineKeywordSrc) {
      const char16_t* params[] = { mCurToken.get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag, "ignoringDuplicateSrc",
                               params, ArrayLength(params));
      return nullptr;
    }
    // cache if we encounter 'unsafe-inline' so we can invalidate (ignore) it in
    // case that script-src directive also contains hash- or nonce-.
    mUnsafeInlineKeywordSrc = new nsCSPKeywordSrc(CSP_KeywordToEnum(mCurToken));
    return mUnsafeInlineKeywordSrc;
  }

  if (CSP_IsKeyword(mCurToken, CSP_UNSAFE_EVAL)) {
    nsWeakPtr ctx = mCSPContext->GetLoadingContext();
    nsCOMPtr<nsIDocument> doc = do_QueryReferent(ctx);
    if (doc) {
      doc->SetHasUnsafeEvalCSP(true);
    }
    return new nsCSPKeywordSrc(CSP_KeywordToEnum(mCurToken));
  }
  return nullptr;
}

// host-source = [ scheme "://" ] host [ port ] [ path ]
nsCSPHostSrc*
nsCSPParser::hostSource()
{
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
nsCSPSchemeSrc*
nsCSPParser::schemeSource()
{
  CSPPARSERLOG(("nsCSPParser::schemeSource, mCurToken: %s, mCurValue: %s",
               NS_ConvertUTF16toUTF8(mCurToken).get(),
               NS_ConvertUTF16toUTF8(mCurValue).get()));

  if (!accept(isCharacterToken)) {
    return nullptr;
  }
  while (schemeChar()) { /* consume */ }
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
nsCSPNonceSrc*
nsCSPParser::nonceSource()
{
  CSPPARSERLOG(("nsCSPParser::nonceSource, mCurToken: %s, mCurValue: %s",
               NS_ConvertUTF16toUTF8(mCurToken).get(),
               NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if mCurToken begins with "'nonce-" and ends with "'"
  if (!StringBeginsWith(mCurToken, NS_ConvertUTF8toUTF16(CSP_EnumToKeyword(CSP_NONCE)),
                        nsASCIICaseInsensitiveStringComparator()) ||
      mCurToken.Last() != SINGLEQUOTE) {
    return nullptr;
  }

  // Trim surrounding single quotes
  const nsAString& expr = Substring(mCurToken, 1, mCurToken.Length() - 2);

  int32_t dashIndex = expr.FindChar(DASH);
  if (dashIndex < 0) {
    return nullptr;
  }
  if (!isValidBase64Value(expr.BeginReading() + dashIndex + 1, expr.EndReading())) {
    return nullptr;
  }

  // cache if encountering hash or nonce to invalidate unsafe-inline
  mHasHashOrNonce = true;
  return new nsCSPNonceSrc(Substring(expr,
                                     dashIndex + 1,
                                     expr.Length() - dashIndex + 1));
}

// hash-source = "'" hash-algo "-" base64-value "'"
nsCSPHashSrc*
nsCSPParser::hashSource()
{
  CSPPARSERLOG(("nsCSPParser::hashSource, mCurToken: %s, mCurValue: %s",
               NS_ConvertUTF16toUTF8(mCurToken).get(),
               NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if mCurToken starts and ends with "'"
  if (mCurToken.First() != SINGLEQUOTE ||
      mCurToken.Last() != SINGLEQUOTE) {
    return nullptr;
  }

  // Trim surrounding single quotes
  const nsAString& expr = Substring(mCurToken, 1, mCurToken.Length() - 2);

  int32_t dashIndex = expr.FindChar(DASH);
  if (dashIndex < 0) {
    return nullptr;
  }

  if (!isValidBase64Value(expr.BeginReading() + dashIndex + 1, expr.EndReading())) {
    return nullptr;
  }

  nsAutoString algo(Substring(expr, 0, dashIndex));
  nsAutoString hash(Substring(expr, dashIndex + 1, expr.Length() - dashIndex + 1));

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
nsCSPBaseSrc*
nsCSPParser::sourceExpression()
{
  CSPPARSERLOG(("nsCSPParser::sourceExpression, mCurToken: %s, mCurValue: %s",
               NS_ConvertUTF16toUTF8(mCurToken).get(),
               NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if it is a keyword
  if (nsCSPBaseSrc *cspKeyword = keywordSource()) {
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

  // We handle a single "*" as host here, to avoid any confusion when applying the default scheme.
  // However, we still would need to apply the default scheme in case we would parse "*:80".
  if (mCurToken.EqualsASCII("*")) {
    return new nsCSPHostSrc(NS_LITERAL_STRING("*"));
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

    // If mCurToken provides not only a scheme, but also a host, we have to check
    // if two slashes follow the scheme.
    if (!accept(SLASH) || !accept(SLASH)) {
      const char16_t* params[] = { mCurToken.get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag, "failedToParseUnrecognizedSource",
                               params, ArrayLength(params));
      return nullptr;
    }
  }

  // Calling resetCurValue allows us to keep pointers for mCurChar and mEndChar
  // alive, but resets mCurValue; e.g. mCurToken = "http://www.example.com", then
  // mCurChar = 'w'
  // mEndChar = 'm'
  // mCurValue = ""
  resetCurValue();

  // If mCurToken does not provide a scheme (scheme-less source), we apply the scheme
  // from selfURI
  if (parsedScheme.IsEmpty()) {
    // Resetting internal helpers, because we might already have parsed some of the host
    // when trying to parse a scheme.
    resetCurChar(mCurToken);
    nsAutoCString selfScheme;
    mSelfURI->GetScheme(selfScheme);
    parsedScheme.AssignASCII(selfScheme.get());
  }

  // At this point we are expecting a host to be parsed.
  // Trying to create a new nsCSPHost.
  if (nsCSPHostSrc *cspHost = hostSource()) {
    // Do not forget to set the parsed scheme.
    cspHost->setScheme(parsedScheme);
    return cspHost;
  }
  // Error was reported in hostSource()
  return nullptr;
}

// source-list = *WSP [ source-expression *( 1*WSP source-expression ) *WSP ]
//               / *WSP "'none'" *WSP
void
nsCSPParser::sourceList(nsTArray<nsCSPBaseSrc*>& outSrcs)
{
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
    if (outSrcs.Length() == 0) {
      nsCSPKeywordSrc *keyword = new nsCSPKeywordSrc(CSP_NONE);
      outSrcs.AppendElement(keyword);
    }
    // Otherwise, we ignore 'none' and report a warning
    else {
      NS_ConvertUTF8toUTF16 unicodeNone(CSP_EnumToKeyword(CSP_NONE));
      const char16_t* params[] = { unicodeNone.get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag, "ignoringUnknownOption",
                               params, ArrayLength(params));
    }
  }
}

void
nsCSPParser::referrerDirectiveValue(nsCSPDirective* aDir)
{
  // directive-value   = "none" / "none-when-downgrade" / "origin" / "origin-when-cross-origin" / "unsafe-url"
  // directive name is token 0, we need to examine the remaining tokens (and
  // there should only be one token in the value).
  CSPPARSERLOG(("nsCSPParser::referrerDirectiveValue"));

  if (mCurDir.Length() != 2) {
    CSPPARSERLOG(("Incorrect number of tokens in referrer directive, got %" PRIuSIZE " expected 1",
                 mCurDir.Length() - 1));
    delete aDir;
    return;
  }

  if (!mozilla::net::IsValidReferrerPolicy(mCurDir[1])) {
    CSPPARSERLOG(("invalid value for referrer directive: %s",
                  NS_ConvertUTF16toUTF8(mCurDir[1]).get()));
    delete aDir;
    return;
  }

  //referrer-directive deprecation warning
  const char16_t* params[] = { mCurDir[1].get() };
  logWarningErrorToConsole(nsIScriptError::warningFlag, "deprecatedReferrerDirective",
                             params, ArrayLength(params));

  // the referrer policy is valid, so go ahead and use it.
  nsWeakPtr ctx = mCSPContext->GetLoadingContext();
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(ctx);
  if (doc) {
    doc->SetHasReferrerPolicyCSP(true);
  }
  mPolicy->setReferrerPolicy(&mCurDir[1]);
  mPolicy->addDirective(aDir);
}

void
nsCSPParser::requireSRIForDirectiveValue(nsRequireSRIForDirective* aDir)
{
  CSPPARSERLOG(("nsCSPParser::requireSRIForDirectiveValue"));

  // directive-value = "style" / "script"
  // directive name is token 0, we need to examine the remaining tokens
  for (uint32_t i = 1; i < mCurDir.Length(); i++) {
    // mCurToken is only set here and remains the current token
    // to be processed, which avoid passing arguments between functions.
    mCurToken = mCurDir[i];
    resetCurValue();
    CSPPARSERLOG(("nsCSPParser:::directive (require-sri-for directive), "
                  "mCurToken: %s (valid), mCurValue: %s",
                  NS_ConvertUTF16toUTF8(mCurToken).get(),
                  NS_ConvertUTF16toUTF8(mCurValue).get()));
    // add contentPolicyTypes to the CSP's required-SRI list for this token
    if (mCurToken.LowerCaseEqualsASCII(kScript)) {
      aDir->addType(nsIContentPolicy::TYPE_SCRIPT);
    }
    else if (mCurToken.LowerCaseEqualsASCII(kStyle)) {
      aDir->addType(nsIContentPolicy::TYPE_STYLESHEET);
    } else {
      const char16_t* invalidTokenName[] = { mCurToken.get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag, "failedToParseUnrecognizedSource",
                            invalidTokenName, ArrayLength(invalidTokenName));
      CSPPARSERLOG(("nsCSPParser:::directive (require-sri-for directive), "
                    "mCurToken: %s (invalid), mCurValue: %s",
                    NS_ConvertUTF16toUTF8(mCurToken).get(),
                    NS_ConvertUTF16toUTF8(mCurValue).get()));
    }
  }

  if (!(aDir->hasType(nsIContentPolicy::TYPE_STYLESHEET)) &&
      !(aDir->hasType(nsIContentPolicy::TYPE_SCRIPT))) {
    const char16_t* directiveName[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "ignoringDirectiveWithNoValues",
                               directiveName, ArrayLength(directiveName));
    delete aDir;
    return;
  }
  
  mPolicy->addDirective(aDir);
}

void
nsCSPParser::reportURIList(nsCSPDirective* aDir)
{
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
      const char16_t* params[] = { mCurToken.get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag, "couldNotParseReportURI",
                               params, ArrayLength(params));
      continue;
    }

    // Create new nsCSPReportURI and append to the list.
    nsCSPReportURI* reportURI = new nsCSPReportURI(uri);
    srcs.AppendElement(reportURI);
  }

  if (srcs.Length() == 0) {
    const char16_t* directiveName[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "ignoringDirectiveWithNoValues",
                             directiveName, ArrayLength(directiveName));
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
void
nsCSPParser::sandboxFlagList(nsCSPDirective* aDir)
{
  CSPPARSERLOG(("nsCSPParser::sandboxFlagList"));

  nsAutoString flags;

  // remember, srcs start at index 1
  for (uint32_t i = 1; i < mCurDir.Length(); i++) {
    mCurToken = mCurDir[i];

    CSPPARSERLOG(("nsCSPParser::sandboxFlagList, mCurToken: %s, mCurValue: %s",
                 NS_ConvertUTF16toUTF8(mCurToken).get(),
                 NS_ConvertUTF16toUTF8(mCurValue).get()));

    if (!nsContentUtils::IsValidSandboxFlag(mCurToken)) {
      const char16_t* params[] = { mCurToken.get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "couldntParseInvalidSandboxFlag",
                               params, ArrayLength(params));
      continue;
    }

    flags.Append(mCurToken);
    if (i != mCurDir.Length() - 1) {
      flags.AppendASCII(" ");
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
void
nsCSPParser::directiveValue(nsTArray<nsCSPBaseSrc*>& outSrcs)
{
  CSPPARSERLOG(("nsCSPParser::directiveValue"));

  // Just forward to sourceList
  sourceList(outSrcs);
}

// directive-name = 1*( ALPHA / DIGIT / "-" )
nsCSPDirective*
nsCSPParser::directiveName()
{
  CSPPARSERLOG(("nsCSPParser::directiveName, mCurToken: %s, mCurValue: %s",
               NS_ConvertUTF16toUTF8(mCurToken).get(),
               NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Check if it is a valid directive
  if (!CSP_IsValidDirective(mCurToken) ||
       (!sCSPExperimentalEnabled &&
         CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::REQUIRE_SRI_FOR))) {
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "couldNotProcessUnknownDirective",
                             params, ArrayLength(params));
    return nullptr;
  }

  // The directive 'reflected-xss' is part of CSP 1.1, see:
  // http://www.w3.org/TR/2014/WD-CSP11-20140211/#reflected-xss
  // Currently we are not supporting that directive, hence we log a
  // warning to the console and ignore the directive including its values.
  if (CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::REFLECTED_XSS_DIRECTIVE)) {
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "notSupportingDirective",
                             params, ArrayLength(params));
    return nullptr;
  }

  // Make sure the directive does not already exist
  // (see http://www.w3.org/TR/CSP11/#parsing)
  if (mPolicy->hasDirective(CSP_StringToCSPDirective(mCurToken))) {
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "duplicateDirective",
                             params, ArrayLength(params));
    return nullptr;
  }

  // CSP delivered via meta tag should ignore the following directives:
  // report-uri, frame-ancestors, and sandbox, see:
  // http://www.w3.org/TR/CSP11/#delivery-html-meta-element
  if (mDeliveredViaMetaTag &&
       ((CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::REPORT_URI_DIRECTIVE)) ||
        (CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::FRAME_ANCESTORS_DIRECTIVE)) ||
        (CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::SANDBOX_DIRECTIVE)))) {
    // log to the console to indicate that meta CSP is ignoring the directive
    const char16_t* params[] = { mCurToken.get() };
    logWarningErrorToConsole(nsIScriptError::warningFlag,
                             "ignoringSrcFromMetaCSP",
                             params, ArrayLength(params));
    return nullptr;
  }

  // special case handling for block-all-mixed-content
  if (CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::BLOCK_ALL_MIXED_CONTENT)) {
    return new nsBlockAllMixedContentDirective(CSP_StringToCSPDirective(mCurToken));
  }

  // special case handling for upgrade-insecure-requests
  if (CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::UPGRADE_IF_INSECURE_DIRECTIVE)) {
    return new nsUpgradeInsecureDirective(CSP_StringToCSPDirective(mCurToken));
  }

  // child-src has it's own class to handle frame-src if necessary
  if (CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::CHILD_SRC_DIRECTIVE)) {
    mChildSrc = new nsCSPChildSrcDirective(CSP_StringToCSPDirective(mCurToken));
    return mChildSrc;
  }

  // if we have a frame-src, cache it so we can decide whether to use child-src
  if (CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::FRAME_SRC_DIRECTIVE)) {
    const char16_t* params[] = { mCurToken.get(), u"child-src" };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "deprecatedDirective",
                             params, ArrayLength(params));
    mFrameSrc = new nsCSPDirective(CSP_StringToCSPDirective(mCurToken));
    return mFrameSrc;
  }

  if (CSP_IsDirective(mCurToken, nsIContentSecurityPolicy::REQUIRE_SRI_FOR)) {
    return new nsRequireSRIForDirective(CSP_StringToCSPDirective(mCurToken));
  }

  return new nsCSPDirective(CSP_StringToCSPDirective(mCurToken));
}

// directive = *WSP [ directive-name [ WSP directive-value ] ]
void
nsCSPParser::directive()
{
  // Set the directiveName to mCurToken
  // Remember, the directive name is stored at index 0
  mCurToken = mCurDir[0];

  CSPPARSERLOG(("nsCSPParser::directive, mCurToken: %s, mCurValue: %s",
               NS_ConvertUTF16toUTF8(mCurToken).get(),
               NS_ConvertUTF16toUTF8(mCurValue).get()));

  // Make sure that the directive-srcs-array contains at least
  // one directive and one src.
  if (mCurDir.Length() < 1) {
    const char16_t* params[] = { u"directive missing" };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "failedToParseUnrecognizedSource",
                             params, ArrayLength(params));
    return;
  }

  // Try to create a new CSPDirective
  nsCSPDirective* cspDir = directiveName();
  if (!cspDir) {
    // if we can not create a CSPDirective, we can skip parsing the srcs for that array
    return;
  }

  // special case handling for block-all-mixed-content, which is only specified
  // by a directive name but does not include any srcs.
  if (cspDir->equals(nsIContentSecurityPolicy::BLOCK_ALL_MIXED_CONTENT)) {
    if (mCurDir.Length() > 1) {
      const char16_t* params[] = { u"block-all-mixed-content" };
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "ignoreSrcForDirective",
                               params, ArrayLength(params));
    }
    // add the directive and return
    mPolicy->addDirective(cspDir);
    return;
  }

  // special case handling for upgrade-insecure-requests, which is only specified
  // by a directive name but does not include any srcs.
  if (cspDir->equals(nsIContentSecurityPolicy::UPGRADE_IF_INSECURE_DIRECTIVE)) {
    if (mCurDir.Length() > 1) {
      const char16_t* params[] = { u"upgrade-insecure-requests" };
      logWarningErrorToConsole(nsIScriptError::warningFlag,
                               "ignoreSrcForDirective",
                               params, ArrayLength(params));
    }
    // add the directive and return
    mPolicy->addUpgradeInsecDir(static_cast<nsUpgradeInsecureDirective*>(cspDir));
    return;
  }

  // special case handling for require-sri-for, which has directive values that
  // are well-defined tokens but are not sources
  if (cspDir->equals(nsIContentSecurityPolicy::REQUIRE_SRI_FOR)) {
    requireSRIForDirectiveValue(static_cast<nsRequireSRIForDirective*>(cspDir));
    return;
  }

  // special case handling of the referrer directive (since it doesn't contain
  // source lists)
  if (cspDir->equals(nsIContentSecurityPolicy::REFERRER_DIRECTIVE)) {
    referrerDirectiveValue(cspDir);
    return;
  }

  // special case handling for report-uri directive (since it doesn't contain
  // a valid source list but rather actual URIs)
  if (CSP_IsDirective(mCurDir[0], nsIContentSecurityPolicy::REPORT_URI_DIRECTIVE)) {
    reportURIList(cspDir);
    return;
  }

  // special case handling for sandbox directive (since it doe4sn't contain
  // a valid source list but rather special sandbox flags)
  if (CSP_IsDirective(mCurDir[0], nsIContentSecurityPolicy::SANDBOX_DIRECTIVE)) {
    sandboxFlagList(cspDir);
    return;
  }

  // make sure to reset cache variables when trying to invalidate unsafe-inline;
  // unsafe-inline might not only appear in script-src, but also in default-src
  mHasHashOrNonce = false;
  mStrictDynamic = false;
  mUnsafeInlineKeywordSrc = nullptr;

  // Try to parse all the srcs by handing the array off to directiveValue
  nsTArray<nsCSPBaseSrc*> srcs;
  directiveValue(srcs);

  // If we can not parse any srcs; we let the source expression be the empty set ('none')
  // see, http://www.w3.org/TR/CSP11/#source-list-parsing
  if (srcs.Length() == 0) {
    nsCSPKeywordSrc *keyword = new nsCSPKeywordSrc(CSP_NONE);
    srcs.AppendElement(keyword);
  }

  // If policy contains 'strict-dynamic' invalidate all srcs within script-src.
  if (mStrictDynamic) {
    MOZ_ASSERT(cspDir->equals(nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE),
               "strict-dynamic only allowed within script-src");
    for (uint32_t i = 0; i < srcs.Length(); i++) {
      // Please note that nsCSPNonceSrc as well as nsCSPHashSrc overwrite invalidate(),
      // so it's fine to just call invalidate() on all srcs. Please also note that
      // nsCSPKeywordSrc() can not be invalidated and always returns false unless the
      // keyword is 'strict-dynamic' in which case we allow the load if the script is
      // not parser created!
      srcs[i]->invalidate();
      // Log a message to the console that src will be ignored.
      nsAutoString srcStr;
      srcs[i]->toString(srcStr);
      // Even though we invalidate all of the srcs internally, we don't want to log
      // messages for the srcs: (1) strict-dynamic, (2) unsafe-inline,
      // (3) nonces, and (4) hashes
      if (!srcStr.EqualsASCII(CSP_EnumToKeyword(CSP_STRICT_DYNAMIC)) &&
          !srcStr.EqualsASCII(CSP_EnumToKeyword(CSP_UNSAFE_EVAL)) &&
          !StringBeginsWith(NS_ConvertUTF16toUTF8(srcStr), NS_LITERAL_CSTRING("'nonce-")) &&
          !StringBeginsWith(NS_ConvertUTF16toUTF8(srcStr), NS_LITERAL_CSTRING("'sha")))
      {
        const char16_t* params[] = { srcStr.get() };
        logWarningErrorToConsole(nsIScriptError::warningFlag, "ignoringSrcForStrictDynamic",
                                 params, ArrayLength(params));
      }
    }
    // Log a warning that all scripts might be blocked because the policy contains
    // 'strict-dynamic' but no valid nonce or hash.
    if (!mHasHashOrNonce) {
      const char16_t* params[] = { mCurDir[0].get() };
      logWarningErrorToConsole(nsIScriptError::warningFlag, "strictDynamicButNoHashOrNonce",
                               params, ArrayLength(params));
    }
  }
  else if (mHasHashOrNonce && mUnsafeInlineKeywordSrc &&
           (cspDir->equals(nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE) ||
            cspDir->equals(nsIContentSecurityPolicy::STYLE_SRC_DIRECTIVE))) {
    mUnsafeInlineKeywordSrc->invalidate();
    // log to the console that unsafe-inline will be ignored
    const char16_t* params[] = { u"'unsafe-inline'" };
    logWarningErrorToConsole(nsIScriptError::warningFlag, "ignoringSrcWithinScriptStyleSrc",
                             params, ArrayLength(params));
  }

  // Add the newly created srcs to the directive and add the directive to the policy
  cspDir->addSrcs(srcs);
  mPolicy->addDirective(cspDir);
}

// policy = [ directive *( ";" [ directive ] ) ]
nsCSPPolicy*
nsCSPParser::policy()
{
  CSPPARSERLOG(("nsCSPParser::policy"));

  mPolicy = new nsCSPPolicy();
  for (uint32_t i = 0; i < mTokens.Length(); i++) {
    // All input is already tokenized; set one tokenized array in the form of
    // [ name, src, src, ... ]
    // to mCurDir and call directive which processes the current directive.
    mCurDir = mTokens[i];
    directive();
  }

  if (mChildSrc && !mFrameSrc) {
    // if we have a child-src, it handles frame-src too, unless frame-src is set
    mChildSrc->setHandleFrameSrc();
  }

  return mPolicy;
}

nsCSPPolicy*
nsCSPParser::parseContentSecurityPolicy(const nsAString& aPolicyString,
                                        nsIURI *aSelfURI,
                                        bool aReportOnly,
                                        nsCSPContext* aCSPContext,
                                        bool aDeliveredViaMetaTag)
{
  if (CSPPARSERLOGENABLED()) {
    CSPPARSERLOG(("nsCSPParser::parseContentSecurityPolicy, policy: %s",
                 NS_ConvertUTF16toUTF8(aPolicyString).get()));
    CSPPARSERLOG(("nsCSPParser::parseContentSecurityPolicy, selfURI: %s",
                 aSelfURI->GetSpecOrDefault().get()));
    CSPPARSERLOG(("nsCSPParser::parseContentSecurityPolicy, reportOnly: %s",
                 (aReportOnly ? "true" : "false")));
    CSPPARSERLOG(("nsCSPParser::parseContentSecurityPolicy, deliveredViaMetaTag: %s",
                 (aDeliveredViaMetaTag ? "true" : "false")));
  }

  NS_ASSERTION(aSelfURI, "Can not parseContentSecurityPolicy without aSelfURI");

  // Separate all input into tokens and store them in the form of:
  // [ [ name, src, src, ... ], [ name, src, src, ... ], ... ]
  // The tokenizer itself can not fail; all eventual errors
  // are detected in the parser itself.

  nsTArray< nsTArray<nsString> > tokens;
  nsCSPTokenizer::tokenizeCSPPolicy(aPolicyString, tokens);

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
      NS_ConvertUTF8toUTF16 unicodePrePath(prePath);
      const char16_t* params[] = { unicodePrePath.get() };
      parser.logWarningErrorToConsole(nsIScriptError::warningFlag, "reportURInotInReportOnlyHeader",
                                      params, ArrayLength(params));
    }
  }

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
