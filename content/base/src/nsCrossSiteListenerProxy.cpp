/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCrossSiteListenerProxy.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsICharsetAlias.h"
#include "nsMimeTypes.h"
#include "nsIStreamConverterService.h"
#include "nsStringStream.h"
#include "nsParserUtils.h"
#include "nsGkAtoms.h"
#include "nsWhitespaceTokenizer.h"

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

NS_IMPL_ISUPPORTS5(nsCrossSiteListenerProxy, nsIStreamListener,
                   nsIRequestObserver, nsIContentSink, nsIXMLContentSink,
                   nsIExpatSink)

nsCrossSiteListenerProxy::nsCrossSiteListenerProxy(nsIStreamListener* aOuter,
                                                   nsIPrincipal* aRequestingPrincipal)
  : mOuter(aOuter), mAcceptState(eNotSet), mHasForwardedRequest(PR_FALSE)
{
  aRequestingPrincipal->GetURI(getter_AddRefs(mRequestingURI));
}

nsresult
nsCrossSiteListenerProxy::ForwardRequest(PRBool aFromStop)
{
  if (mHasForwardedRequest) {
    return NS_OK;
  }

  mHasForwardedRequest = PR_TRUE;

  if (mParser) {
    mParser->Terminate();
    mParser = nsnull;
    mParserListener = nsnull;
  }

  if (mAcceptState != eAccept) {
    mOuterRequest->Cancel(NS_ERROR_DOM_BAD_URI);
    mOuter->OnStartRequest(mOuterRequest, mOuterContext);

    // Only call OnStopRequest here if we were called from OnStopRequest.
    // Otherwise the call to Cancel will make us get an OnStopRequest later
    // so we'll forward OnStopRequest then.
    if (aFromStop) {
      mOuter->OnStopRequest(mOuterRequest, mOuterContext, NS_ERROR_DOM_BAD_URI);
    }

    return NS_ERROR_DOM_BAD_URI;
  }

  nsresult rv = mOuter->OnStartRequest(mOuterRequest, mOuterContext);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mStoredData.IsEmpty()) {
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewCStringInputStream(getter_AddRefs(stream), mStoredData);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mOuter->OnDataAvailable(mOuterRequest, mOuterContext, stream, 0,
                                 mStoredData.Length());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnStartRequest(nsIRequest* aRequest,
                                         nsISupports* aContext)
{
  mOuterRequest = aRequest;
  mOuterContext = aContext;

  // Check if the request failed
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_FAILED(status)) {
    mAcceptState = eDeny;
    return ForwardRequest(PR_FALSE);
  }

  // Check if this was actually a cross domain request
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) {
    return NS_ERROR_DOM_BAD_URI;
  }
  nsCOMPtr<nsIURI> finalURI;
  channel->GetURI(getter_AddRefs(finalURI));
  rv = nsContentUtils::GetSecurityManager()->
    CheckSameOriginURI(mRequestingURI, finalURI);
  if (NS_SUCCEEDED(rv)) {
    mAcceptState = eAccept;
    return ForwardRequest(PR_FALSE);
  }

  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(channel);
  if (http) {
    PRBool succeeded;
    rv = http->GetRequestSucceeded(&succeeded);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!succeeded) {
      mAcceptState = eDeny;
      return ForwardRequest(PR_FALSE);
    }
  }

  // Get the list of subdomains out of mRequestingURI
  nsCString host;
  rv = mRequestingURI->GetAsciiHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 nextDot, currDot = 0;
  while ((nextDot = host.FindChar('.', currDot)) != -1) {
    mReqSubdomains.AppendElement(Substring(host, currDot, nextDot - currDot));
    currDot = nextDot + 1;
  }
  mReqSubdomains.AppendElement(Substring(host, currDot));

  // Check the Access-Control header
  if (http) {
    nsCAutoString ac;
    rv = http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control"), ac);
    
    if (NS_SUCCEEDED(rv)) {
      CheckHeader(ac);
    }
  }

  if (mAcceptState == eDeny) {
    return ForwardRequest(PR_FALSE);
  }

  // Set up a parser with us as a sink to look for PIs
  mParser = do_CreateInstance(kCParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mParserListener = do_QueryInterface(mParser);

  mParser->SetCommand(kLoadAsData);
  mParser->SetContentSink(this);
  mParser->Parse(finalURI);

  // check channel's charset...
  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8"));
  PRInt32 charsetSource = kCharsetFromDocTypeDefault;
  nsCAutoString charsetVal;
  rv = channel->GetContentCharset(charsetVal);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsICharsetAlias> calias =
      do_GetService(NS_CHARSETALIAS_CONTRACTID);

    if (calias) {
      nsCAutoString preferred;
      rv = calias->GetPreferred(charsetVal, preferred);
      if (NS_SUCCEEDED(rv)) {            
        charset = preferred;
        charsetSource = kCharsetFromChannel;
      }
    }
  }

  mParser->SetDocumentCharset(charset, charsetSource);

  nsCAutoString contentType;
  channel->GetContentType(contentType);

  // Time to sniff! Note: this should go away once file channels do
  // sniffing themselves.
  PRBool sniff;
  if (NS_SUCCEEDED(finalURI->SchemeIs("file", &sniff)) && sniff &&
    contentType.Equals(UNKNOWN_CONTENT_TYPE)) {
    nsCOMPtr<nsIStreamConverterService> serv =
      do_GetService("@mozilla.org/streamConverters;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIStreamListener> converter;
      rv = serv->AsyncConvertData(UNKNOWN_CONTENT_TYPE,
                                  "*/*",
                                  mParserListener,
                                  aContext,
                                  getter_AddRefs(converter));
      if (NS_SUCCEEDED(rv)) {
        mParserListener = converter;
      }
    }
  }

  // Hold a local reference to make sure the parser doesn't go away
  nsCOMPtr<nsIStreamListener> stackedListener = mParserListener;
  return stackedListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnStopRequest(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsresult aStatusCode)
{
  if (mHasForwardedRequest) {
    return mOuter->OnStopRequest(aRequest, aContext, aStatusCode);
  }

  mAcceptState = eDeny;
  return ForwardRequest(PR_TRUE);
}

NS_METHOD
StringSegmentWriter(nsIInputStream *aInStream,
                    void *aClosure,
                    const char *aFromSegment,
                    PRUint32 aToOffset,
                    PRUint32 aCount,
                    PRUint32 *aWriteCount)
{
  nsCString* dest = static_cast<nsCString*>(aClosure);

  dest->Append(aFromSegment, aCount);
  *aWriteCount = aCount;
  
  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnDataAvailable(nsIRequest* aRequest,
                                          nsISupports* aContext, 
                                          nsIInputStream* aInputStream,
                                          PRUint32 aOffset,
                                          PRUint32 aCount)
{
  if (mHasForwardedRequest) {
    return mOuter->OnDataAvailable(aRequest, aContext, aInputStream, aOffset,
                                   aCount);
  }

  NS_ASSERTION(mStoredData.Length() == aOffset,
               "Stored wrong amount of data");

  PRUint32 read;
  nsresult rv = aInputStream->ReadSegments(StringSegmentWriter, &mStoredData,
                                           aCount, &read);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(read == aCount, "didn't store all of the stream");

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewCStringInputStream(getter_AddRefs(stream),
                                Substring(mStoredData, aOffset));
  NS_ENSURE_SUCCESS(rv, rv);

  // Hold a local reference to make sure the parser doesn't go away
  nsCOMPtr<nsIStreamListener> stackedListener = mParserListener;
  rv = stackedListener->OnDataAvailable(aRequest, aContext, stream, aOffset,
                                        aCount);
  // When we forward the request we also terminate the parsing which will
  // result in an error bubbling up to here. We want to ignore the error
  // in that case.
  if (mHasForwardedRequest) {
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::HandleStartElement(const PRUnichar *aName,
                                             const PRUnichar **aAtts,
                                             PRUint32 aAttsCount,
                                             PRInt32 aIndex,
                                             PRUint32 aLineNumber)
{
  // We're done processing the prolog.
  ForwardRequest(PR_FALSE);

  // Block the parser since we don't want to spend more cycles on parsing
  // stuff.
  return NS_ERROR_HTMLPARSER_BLOCK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::HandleEndElement(const PRUnichar *aName)
{
  NS_ASSERTION(mHasForwardedRequest, "Should have forwarded request");

  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::HandleComment(const PRUnichar *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::HandleCDataSection(const PRUnichar *aData,
                                             PRUint32 aLength)
{
  NS_ASSERTION(mHasForwardedRequest, "Should have forwarded request");

  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::HandleDoctypeDecl(const nsAString & aSubset,
                                            const nsAString & aName,
                                            const nsAString & aSystemId,
                                            const nsAString & aPublicId,
                                            nsISupports *aCatalogData)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::HandleCharacterData(const PRUnichar *aData,
                                              PRUint32 aLength)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::HandleProcessingInstruction(const PRUnichar *aTarget,
                                                      const PRUnichar *aData)
{
  if (mHasForwardedRequest ||
      !NS_LITERAL_STRING("access-control").Equals(aTarget)) {
    return NS_OK;
  }

  nsDependentString data(aData);

  PRBool seenType = PR_FALSE, seenExclude = PR_FALSE;
  PRBool ruleIsAllow = PR_FALSE;
  nsAutoString itemList, excludeList;

  PRUint32 i;
  for (i = 0;; ++i) {
    nsAutoString attrName;
    if (nsParserUtils::GetQuotedAttrNameAt(data, i, attrName) &&
        attrName.IsEmpty()) {
      break;
    }

    nsCOMPtr<nsIAtom> attr = do_GetAtom(attrName);

    PRBool res;
    if (!seenType && attrName.EqualsLiteral("allow")) {
      seenType = PR_TRUE;
      ruleIsAllow = PR_TRUE;

      res = nsParserUtils::GetQuotedAttributeValue(data, attr, itemList);
    }
    else if (!seenType && attrName.EqualsLiteral("deny")) {
      seenType = PR_TRUE;
      ruleIsAllow = PR_FALSE;

      res = nsParserUtils::GetQuotedAttributeValue(data, attr, itemList);
    }
    else if (!seenExclude && attrName.EqualsLiteral("exclude")) {
      seenExclude = PR_TRUE;

      res = nsParserUtils::GetQuotedAttributeValue(data, attr, excludeList);
    }
    else {
      res = PR_FALSE;
    }
    
    if (!res) {
      // parsing attribute value failed or unknown/duplicated attribute
      mAcceptState = eDeny;
      return ForwardRequest(PR_FALSE);
    }
  }

  PRBool matchesRule = PR_FALSE;

  nsWhitespaceTokenizer itemTok(itemList);

  if (!itemTok.hasMoreTokens()) {
    mAcceptState = eDeny;

    return ForwardRequest(PR_FALSE);
  }

  while (itemTok.hasMoreTokens()) {
    // Order is important here since we always want to call the function
    matchesRule = VerifyAndMatchDomainPattern(
      NS_ConvertUTF16toUTF8(itemTok.nextToken())) || matchesRule;
  }

  nsWhitespaceTokenizer excludeTok(excludeList);
  while (excludeTok.hasMoreTokens()) {
    // Order is important here since we always want to call the function
    matchesRule = !VerifyAndMatchDomainPattern(
      NS_ConvertUTF16toUTF8(excludeTok.nextToken())) && matchesRule;
  }

  if (matchesRule && mAcceptState != eDeny) {
    mAcceptState = ruleIsAllow ? eAccept : eDeny;
  }

  if (mAcceptState == eDeny) {
    return ForwardRequest(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::HandleXMLDeclaration(const PRUnichar *aVersion,
                                               const PRUnichar *aEncoding,
                                               PRInt32 aStandalone)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::ReportError(const PRUnichar *aErrorText,
                                      const PRUnichar *aSourceText,
                                      nsIScriptError *aError,
                                      PRBool *_retval)
{
  if (!mHasForwardedRequest) {
    mAcceptState = eDeny;

    return ForwardRequest(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::WillBuildModel()
{
  nsCOMPtr<nsIDTD> dtd;
  mParser->GetDTD(getter_AddRefs(dtd));
  NS_ASSERTION(dtd, "missing dtd in WillBuildModel");
  if (dtd && !(dtd->GetType() & NS_IPARSER_FLAG_XML)) {
    ForwardRequest(PR_FALSE);
    
    // Block the parser since we don't want to spend more cycles on parsing
    // stuff.
    return NS_ERROR_HTMLPARSER_BLOCK;
  }

  return NS_OK;
}

// Moves aIter past the LWS (RFC2616) directly following it.
// Returns PR_TRUE and updates aIter if there was an LWS there,
//         PR_FALSE otherwise
static PRBool
EatLWS(const char*& aIter, const char* aEnd)
{
  if (aIter + 1 < aEnd && *aIter == '\r' && *(aIter + 1) == '\n') {
    aIter += 2;
  }

  PRBool res = PR_FALSE;
  while (aIter < aEnd && (*aIter == '\t' || *aIter == ' ')) {
    ++aIter;
    res = PR_TRUE;
  }
  
  return res;
}

// Moves aIter past the string, given by aString, directly following it.
// Returns PR_TRUE and updates aIter if the string was there,
//         PR_FALSE otherwise
static PRBool
EatString(const char*& aIter, const char* aEnd, const char* aString)
{
  const char* local = aIter;
  while (*aString && local < aEnd && *local == *aString) {
    ++local;
    ++aString;
  }
  if (*aString) {
    return PR_FALSE;
  }

  aIter = local;
  
  return PR_TRUE;
}

// Moves aIter to the first aChar following it.
// Returns The string between the aIters initial position and the
// found character if one was found.
// Returns an empty string otherwise.
static nsDependentCSubstring
EatToChar(const char*& aIter, const char* aEnd, char aChar)
{
  const char* start = aIter;
  while (aIter < aEnd) {
    if (*aIter == aChar) {
      return Substring(start, aIter);
    }
    ++aIter;
  }

  static char emptyStatic[] = { '\0' };

  aIter = start;
  return Substring(emptyStatic, emptyStatic);
}

PRBool
nsCrossSiteListenerProxy::MatchPatternList(const char*& aIter, const char* aEnd)
{
  PRBool matchesList = PR_FALSE;
  PRBool hasItems = PR_FALSE;

  for (;;) {
    const char* start = aIter;
    if (!EatLWS(aIter, aEnd)) {
      break;
    }

    if (!EatString(aIter, aEnd, "<")) {
      // restore iterator to before LWS since it wasn't part of the list
      aIter = start;
      break;
    }

    const nsACString& accessItem = EatToChar(aIter, aEnd, '>');
    if (!EatString(aIter, aEnd, ">")) {
      mAcceptState = eDeny;
      break;
    }

    hasItems = PR_TRUE;

    // Order is important here since we always want to call the function
    matchesList = VerifyAndMatchDomainPattern(accessItem) || matchesList;
  }

  if (!hasItems) {
    mAcceptState = eDeny;
  }

  return matchesList;
}

#define DENY_AND_RETURN \
      mAcceptState = eDeny; \
      return

void
nsCrossSiteListenerProxy::CheckHeader(const nsCString& aHeader)
{
  const char* iter = aHeader.BeginReading();
  const char* end = aHeader.EndReading();

  // ruleset        ::= LWS? rule LWS? ("," LWS? rule LWS?)*
  while (iter < end) {
    // eat LWS?
    EatLWS(iter, end);

    // rule ::= rule-type (LWS pattern)+ (LWS "exclude" (LWS pattern)+)?
    // eat rule-type
    PRBool ruleIsAllow;
    if (EatString(iter, end, "deny")) {
      ruleIsAllow = PR_FALSE;
    }
    else if (EatString(iter, end, "allow")) {
      ruleIsAllow = PR_TRUE;
    }
    else {
      DENY_AND_RETURN;
    }

    // eat (LWS pattern)+
    PRBool matchesRule = MatchPatternList(iter, end);

    PRBool ateLWS = EatLWS(iter, end);

    // eat (LWS "exclude" (LWS pattern)+)?
    if (ateLWS && EatString(iter, end, "exclude")) {
      ateLWS = PR_FALSE;

      // Order is important here since we always want to call the function
      matchesRule = !MatchPatternList(iter, end) && matchesRule;
    }
    
    if (matchesRule && mAcceptState != eDeny) {
      mAcceptState = ruleIsAllow ? eAccept : eDeny;
    }

    // eat LWS?
    if (!ateLWS) {
      EatLWS(iter, end);
    }
    
    if (iter != end) {
      if (!EatString(iter, end, ",")) {
        DENY_AND_RETURN;
      }
    }
  }
}

// Moves aIter forward one character if the character at aIter is in [a-zA-Z]
// Returns PR_TRUE and updates aIter if such a character was found.
//         PR_FALSE otherwise.
static PRBool
EatAlpha(nsACString::const_iterator& aIter, nsACString::const_iterator& aEnd)
{
  if (aIter != aEnd && ((*aIter >= 'A' && *aIter <= 'Z') ||
                        (*aIter >= 'a' && *aIter <= 'z'))) {
    ++aIter;

    return PR_TRUE;
  }

  return PR_FALSE;
}

// Moves aIter forward one character if the character at aIter is in [0-9]
// Returns PR_TRUE and updates aIter if such a character was found.
//         PR_FALSE otherwise.
static PRBool
EatDigit(nsACString::const_iterator& aIter, nsACString::const_iterator& aEnd)
{
  if (aIter != aEnd && *aIter >= '0' && *aIter <= '9') {
    ++aIter;

    return PR_TRUE;
  }

  return PR_FALSE;
}

// Moves aIter forward one character if the character at aIter is aChar
// Returns PR_TRUE and updates aIter if aChar was found.
//         PR_FALSE otherwise.
static PRBool
EatChar(nsACString::const_iterator& aIter, nsACString::const_iterator& aEnd,
        char aChar)
{
  if (aIter != aEnd && *aIter == aChar) {
    ++aIter;

    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsCrossSiteListenerProxy::VerifyAndMatchDomainPattern(const nsACString& aPattern)
{
  if (aPattern.EqualsLiteral("*")) {
    return PR_TRUE;
  }

  // access-item ::= (scheme "://")? domain-pattern (":" port)? | "*"

  nsACString::const_iterator start, iter, end;
  aPattern.BeginReading(start);
  aPattern.EndReading(end);

  // (scheme "://")?
  nsCString patternScheme;
  nsACString::const_iterator schemeStart = start, schemeEnd = end;
  if (FindInReadable(NS_LITERAL_CSTRING("://"), schemeStart, schemeEnd)) {
    // There is a '://' in the string which means that it must start with
    // a scheme.
    
    iter = start;
    
    if (!EatAlpha(iter, end)) {
      DENY_AND_RETURN PR_FALSE;
    }
    
    while(EatAlpha(iter, end) ||
          EatDigit(iter, end) ||
          EatChar(iter, end, '+') ||
          EatChar(iter, end, '-') ||
          EatChar(iter, end, '.')) {}
    
    if (iter != schemeStart) {
      DENY_AND_RETURN PR_FALSE;
    }

    // Set the scheme
    patternScheme = Substring(start, iter);

    start = iter.advance(3);
  }

  // domain-pattern ::= subdomain | "*." subdomain
  PRBool patternHasWild = PR_FALSE;
  if (EatChar(start, end, '*')) {
    if (!EatChar(start, end, '.')) {
      DENY_AND_RETURN PR_FALSE;
    }
    patternHasWild = PR_TRUE;
  }

  nsTArray<nsCString> patternSubdomains;

  // subdomain ::= label | subdomain "." label
  do {
    iter = start;
    if (!EatAlpha(iter, end)) {
      DENY_AND_RETURN PR_FALSE;
    }

    while (EatAlpha(iter, end) ||
           EatDigit(iter, end) ||
           EatChar(iter, end, '-')) {}
    
    const nsDependentCSubstring& label = Substring(start, iter);
    if (label.Last() == '-') {
      DENY_AND_RETURN PR_FALSE;
    }

    start = iter;
    
    // Save the label
    patternSubdomains.AppendElement(label);
  } while (EatChar(start, end, '.'));

  // (":" port)?
  PRInt32 patternPort = -1;
  if (EatChar(start, end, ':')) {
    iter = start;
    while (EatDigit(iter, end)) {}
    
    if (iter != start) {
      PRInt32 ec;
      patternPort = PromiseFlatCString(Substring(start, iter)).ToInteger(&ec);
      NS_ASSERTION(NS_SUCCEEDED(ec), "ToInteger failed");
    }
    
    start = iter;
  }

  // Did we consume the whole pattern?
  if (start != end) {
    DENY_AND_RETURN PR_FALSE;
  }

  // Do checks at the end so that we make sure that the whole pattern is
  // checked for syntax correctness first.

  // Check scheme
  PRBool res;
  if (!patternScheme.IsEmpty() &&
      (NS_FAILED(mRequestingURI->SchemeIs(patternScheme.get(), &res)) ||
       !res)) {
    return PR_FALSE;
  }

  // Check port
  if (patternPort != -1 &&
      patternPort != NS_GetRealPort(mRequestingURI)) {
    return PR_FALSE;
  }

  // Check subdomain
  PRUint32 patternPos = patternSubdomains.Length();
  PRUint32 reqPos = mReqSubdomains.Length();
  do {
    --patternPos;
    --reqPos;
    if (!patternSubdomains[patternPos].LowerCaseEqualsASCII(
          mReqSubdomains[reqPos].get(), mReqSubdomains[reqPos].Length())) {
      return PR_FALSE;
    }
  } while (patternPos > 0 && reqPos > 0);

  // Only matches if we've matched all of pattern and either matched all of
  // mRequestingURI and there's no wildcard, or there's a wildcard and there
  // are still elements of mRequestingURI left.
  
  return patternPos == 0 &&
         ((reqPos == 0 && !patternHasWild) ||
          (reqPos != 0 && patternHasWild));
}
