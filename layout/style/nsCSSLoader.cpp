/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: ft=cpp tw=78 sw=2 et ts=2
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/* loading of CSS style sheets using the network APIs */

#include "nsCSSLoader.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIDOMNSDocumentStyle.h"
#include "nsIUnicharInputStream.h"
#include "nsIConverterInputStream.h"
#include "nsICharsetAlias.h"
#include "nsUnicharUtils.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIParser.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsCRT.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentPolicyUtils.h"
#include "nsITimelineService.h"
#include "nsIHttpChannel.h"
#include "nsIScriptError.h"
#include "nsMimeTypes.h"
#include "nsIAtom.h"
#include "nsIDOM3Node.h"
#include "nsICSSStyleSheet.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsICSSLoaderObserver.h"
#include "nsICSSLoader.h"
#include "nsICSSParser.h"
#include "nsICSSImportRule.h"
#include "nsThreadUtils.h"
#include "nsGkAtoms.h"

#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif

#include "nsIMediaList.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSImportRule.h"
#include "nsContentErrors.h"

#ifdef MOZ_LOGGING
// #define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *gLoaderLog = PR_NewLogModule("nsCSSLoader");
#endif /* PR_LOGGING */

#define LOG_FORCE(args) PR_LOG(gLoaderLog, PR_LOG_ALWAYS, args)
#define LOG_ERROR(args) PR_LOG(gLoaderLog, PR_LOG_ERROR, args)
#define LOG_WARN(args) PR_LOG(gLoaderLog, PR_LOG_WARNING, args)
#define LOG_DEBUG(args) PR_LOG(gLoaderLog, PR_LOG_DEBUG, args)
#define LOG(args) LOG_DEBUG(args)

#define LOG_FORCE_ENABLED() PR_LOG_TEST(gLoaderLog, PR_LOG_ALWAYS)
#define LOG_ERROR_ENABLED() PR_LOG_TEST(gLoaderLog, PR_LOG_ERROR)
#define LOG_WARN_ENABLED() PR_LOG_TEST(gLoaderLog, PR_LOG_WARNING)
#define LOG_DEBUG_ENABLED() PR_LOG_TEST(gLoaderLog, PR_LOG_DEBUG)
#define LOG_ENABLED() LOG_DEBUG_ENABLED()

#ifdef PR_LOGGING
#define LOG_URI(format, uri)                        \
  PR_BEGIN_MACRO                                    \
    NS_ASSERTION(uri, "Logging null uri");          \
    if (LOG_ENABLED()) {                            \
      nsCAutoString _logURISpec;                    \
      uri->GetSpec(_logURISpec);                    \
      LOG((format, _logURISpec.get()));             \
    }                                               \
  PR_END_MACRO
#else // PR_LOGGING
#define LOG_URI(format, uri)
#endif // PR_LOGGING

// And some convenience strings...
#ifdef PR_LOGGING
static const char* const gStateStrings[] = {
  "eSheetStateUnknown",
  "eSheetNeedsParser",
  "eSheetPending",
  "eSheetLoading",
  "eSheetComplete"
};
#endif

/********************************
 * SheetLoadData implementation *
 ********************************/
NS_IMPL_ISUPPORTS2(SheetLoadData, nsIUnicharStreamLoaderObserver, nsIRunnable)

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader,
                             const nsSubstring& aTitle,
                             nsIParser* aParserToUnblock,
                             nsIURI* aURI,
                             nsICSSStyleSheet* aSheet,
                             nsIStyleSheetLinkingElement* aOwningElement,
                             PRBool aIsAlternate,
                             nsICSSLoaderObserver* aObserver)
  : mLoader(aLoader),
    mTitle(aTitle),
    mParserToUnblock(aParserToUnblock),
    mURI(aURI),
    mLineNumber(1),
    mSheet(aSheet),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mSyncLoad(PR_FALSE),
    mIsNonDocumentSheet(PR_FALSE),
    mIsLoading(PR_FALSE),
    mIsCancelled(PR_FALSE),
    mMustNotify(PR_FALSE),
    mWasAlternate(aIsAlternate),
    mAllowUnsafeRules(PR_FALSE),
    mOwningElement(aOwningElement),
    mObserver(aObserver)
{

  NS_PRECONDITION(mLoader, "Must have a loader!");
  NS_ADDREF(mLoader);
}

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader,
                             nsIURI* aURI,
                             nsICSSStyleSheet* aSheet,
                             SheetLoadData* aParentData,
                             nsICSSLoaderObserver* aObserver)
  : mLoader(aLoader),
    mParserToUnblock(nsnull),
    mURI(aURI),
    mLineNumber(1),
    mSheet(aSheet),
    mNext(nsnull),
    mParentData(aParentData),
    mPendingChildren(0),
    mSyncLoad(PR_FALSE),
    mIsNonDocumentSheet(PR_FALSE),
    mIsLoading(PR_FALSE),
    mIsCancelled(PR_FALSE),
    mMustNotify(PR_FALSE),
    mWasAlternate(PR_FALSE),
    mAllowUnsafeRules(PR_FALSE),
    mOwningElement(nsnull),
    mObserver(aObserver)
{

  NS_PRECONDITION(mLoader, "Must have a loader!");
  NS_ADDREF(mLoader);
  if (mParentData) {
    NS_ADDREF(mParentData);
    mSyncLoad = mParentData->mSyncLoad;
    mIsNonDocumentSheet = mParentData->mIsNonDocumentSheet;
    mAllowUnsafeRules = mParentData->mAllowUnsafeRules;
    ++(mParentData->mPendingChildren);
  }
}

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader,
                             nsIURI* aURI,
                             nsICSSStyleSheet* aSheet,
                             PRBool aSyncLoad,
                             PRBool aAllowUnsafeRules,
                             nsICSSLoaderObserver* aObserver)
  : mLoader(aLoader),
    mParserToUnblock(nsnull),
    mURI(aURI),
    mLineNumber(1),
    mSheet(aSheet),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mSyncLoad(aSyncLoad),
    mIsNonDocumentSheet(PR_TRUE),
    mIsLoading(PR_FALSE),
    mIsCancelled(PR_FALSE),
    mMustNotify(PR_FALSE),
    mWasAlternate(PR_FALSE),
    mAllowUnsafeRules(aAllowUnsafeRules),
    mOwningElement(nsnull),
    mObserver(aObserver)
{

  NS_PRECONDITION(mLoader, "Must have a loader!");
  NS_ADDREF(mLoader);
}

SheetLoadData::~SheetLoadData()
{
  NS_RELEASE(mLoader);
  NS_IF_RELEASE(mParentData);
  NS_IF_RELEASE(mNext);
}

NS_IMETHODIMP
SheetLoadData::Run()
{
  mLoader->HandleLoadEvent(this);
  return NS_OK;
}

/*************************
 * Loader Implementation *
 *************************/

// static
nsCOMArray<nsICSSParser>* CSSLoaderImpl::gParsers = nsnull;

CSSLoaderImpl::CSSLoaderImpl(void)
  : mDocument(nsnull), 
    mCaseSensitive(PR_FALSE),
    mEnabled(PR_TRUE), 
    mCompatMode(eCompatibility_FullStandards)
{
}

CSSLoaderImpl::~CSSLoaderImpl(void)
{
  NS_ASSERTION((!mLoadingDatas.IsInitialized()) || mLoadingDatas.Count() == 0,
               "How did we get destroyed when there are loading data?");
  NS_ASSERTION((!mPendingDatas.IsInitialized()) || mPendingDatas.Count() == 0,
               "How did we get destroyed when there are pending data?");
  // Note: no real need to revoke our stylesheet loaded events -- they
  // hold strong references to us, so if we're going away that means
  // they're all done.
}

NS_IMPL_ISUPPORTS1(CSSLoaderImpl, nsICSSLoader)

void
CSSLoaderImpl::Shutdown()
{
  delete gParsers;
  gParsers = nsnull;
}

NS_IMETHODIMP
CSSLoaderImpl::Init(nsIDocument* aDocument)
{
  NS_ASSERTION(! mDocument, "already initialized");

  mDocument = aDocument;

  // We can just use the preferred set, since there are no sheets in the
  // document yet (if there are, how did they get there? _we_ load the sheets!)
  // and hence the selected set makes no sense at this time.
  nsCOMPtr<nsIDOMNSDocumentStyle> domDoc(do_QueryInterface(mDocument));
  if (domDoc) {
    domDoc->GetPreferredStyleSheetSet(mPreferredSheet);
  }
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
StartAlternateLoads(nsIURI *aKey, SheetLoadData* &aData, void* aClosure)
{
  NS_STATIC_CAST(CSSLoaderImpl*,aClosure)->LoadSheet(aData, eSheetNeedsParser);
  return PL_DHASH_REMOVE;
}

NS_IMETHODIMP
CSSLoaderImpl::DropDocumentReference(void)
{
  mDocument = nsnull;
  // Flush out pending datas just so we don't leak by accident.  These
  // loads should short-circuit through the mDocument check in
  // LoadSheet and just end up in SheetComplete immediately
  if (mPendingDatas.IsInitialized())
    mPendingDatas.Enumerate(StartAlternateLoads, this);
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::SetCaseSensitive(PRBool aCaseSensitive)
{
  mCaseSensitive = aCaseSensitive;
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::SetCompatibilityMode(nsCompatibility aCompatMode)
{
  mCompatMode = aCompatMode;
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
StartNonAlternates(nsIURI *aKey, SheetLoadData* &aData, void* aClosure)
{
  NS_PRECONDITION(aData, "Must have a data");
  NS_PRECONDITION(aClosure, "Must have a loader");
  
  CSSLoaderImpl* loader = NS_STATIC_CAST(CSSLoaderImpl*, aClosure);
  // Note that we don't want to affect what the selected style set is,
  // so use PR_TRUE for aHasAlternateRel.
  if (loader->IsAlternate(aData->mTitle, PR_TRUE)) {
    return PL_DHASH_NEXT;
  }

  // Need to start the load
  loader->LoadSheet(aData, eSheetNeedsParser);
  return PL_DHASH_REMOVE;
}

NS_IMETHODIMP
CSSLoaderImpl::SetPreferredSheet(const nsAString& aTitle)
{
#ifdef DEBUG
  nsCOMPtr<nsIDOMNSDocumentStyle> doc(do_QueryInterface(mDocument));
  if (doc) {
    nsAutoString currentPreferred;
    doc->GetLastStyleSheetSet(currentPreferred);
    if (DOMStringIsNull(currentPreferred)) {
      doc->GetPreferredStyleSheetSet(currentPreferred);
    }
    NS_ASSERTION(currentPreferred.Equals(aTitle),
                 "Unexpected argument to SetPreferredSheet");    
  }
#endif
  
  mPreferredSheet = aTitle;

  // start any pending alternates that aren't alternates anymore
  if (mPendingDatas.IsInitialized())
    mPendingDatas.Enumerate(StartNonAlternates, this);
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::GetPreferredSheet(nsAString& aTitle)
{
  aTitle.Assign(mPreferredSheet);
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::GetParserFor(nsICSSStyleSheet* aSheet, 
                            nsICSSParser** aParser)
{
  NS_PRECONDITION(aParser, "Null out param");

  *aParser = nsnull;

  if (!gParsers) {
    gParsers = new nsCOMArray<nsICSSParser>;
    if (!gParsers) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  PRInt32 count = gParsers->Count();
  if (0 < count--) {
    *aParser = gParsers->ObjectAt(count);
    NS_ADDREF(*aParser);
    gParsers->RemoveObjectAt(count);
  }

  nsresult result = NS_OK;
  if (! *aParser) {
    result = NS_NewCSSParser(aParser);
  }
  
  if (*aParser) {
    (*aParser)->SetCaseSensitive(mCaseSensitive);
    (*aParser)->SetQuirkMode(mCompatMode == eCompatibility_NavQuirks);
    if (aSheet) {
      (*aParser)->SetStyleSheet(aSheet);
    }
    (*aParser)->SetChildLoader(this);
  }
  return result;
}

NS_IMETHODIMP
CSSLoaderImpl::RecycleParser(nsICSSParser* aParser)
{
  NS_PRECONDITION(aParser, "Recycle only good parsers, please");
  NS_ENSURE_TRUE(gParsers, NS_ERROR_UNEXPECTED);

  if (!gParsers->AppendObject(aParser)) {
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

static const char kCharsetSym[] = "@charset";

static nsresult GetCharsetFromData(const unsigned char* aStyleSheetData,
                                   PRUint32 aDataLength,
                                   nsACString& aCharset)
{
  aCharset.Truncate();
  if (aDataLength <= sizeof(kCharsetSym) - 1)
    return NS_ERROR_NOT_AVAILABLE;
  PRUint32 step = 1;
  PRUint32 pos = 0;
  // Determine the encoding type.  If we have a BOM, set aCharset to the
  // charset listed for that BOM in http://www.w3.org/TR/REC-xml#sec-guessing;
  // that way even if we don't have a valid @charset rule we can use the BOM to
  // get a reasonable charset.  If we do have an @charset rule, the string from
  // that will override this fallback setting of aCharset.
  if (*aStyleSheetData == 0x40 && *(aStyleSheetData+1) == 0x63 /* '@c' */ ) {
    // 1-byte ASCII-based encoding (ISO-8859-*, UTF-8, etc), no BOM
    step = 1;
    pos = 0;
  }
  else if (aStyleSheetData[0] == 0xEF &&
           aStyleSheetData[1] == 0xBB &&
           aStyleSheetData[2] == 0xBF) {
    // UTF-8 BOM
    step = 1;
    pos = 3;
    aCharset = "UTF-8";
  }
  // Check for a 4-byte encoding BOM before checking for a 2-byte one,
  // since the latter can be a proper subset of the former.
  else if (aStyleSheetData[0] == 0x00 &&
           aStyleSheetData[1] == 0x00 &&
           aStyleSheetData[2] == 0xFE &&
           aStyleSheetData[3] == 0xFF) {
    // big-endian 4-byte encoding BOM
    step = 4;
    pos = 7;
    aCharset = "UTF-32BE";
  }
  else if (aStyleSheetData[0] == 0xFF &&
           aStyleSheetData[1] == 0xFE &&
           aStyleSheetData[2] == 0x00 &&
           aStyleSheetData[3] == 0x00) {
    // little-endian 4-byte encoding BOM
    step = 4;
    pos = 4;
    aCharset = "UTF-32LE";
  }
  else if (aStyleSheetData[0] == 0x00 &&
           aStyleSheetData[1] == 0x00 &&
           aStyleSheetData[2] == 0xFF &&
           aStyleSheetData[3] == 0xFE) {
    // 4-byte encoding BOM in 2143 order
    NS_WARNING("Our unicode decoders aren't likely  to deal with this one");
    step = 4;
    pos = 6;
    aCharset = "UTF-32";
  }
  else if (aStyleSheetData[0] == 0xFE &&
           aStyleSheetData[1] == 0xFF &&
           aStyleSheetData[2] == 0x00 &&
           aStyleSheetData[3] == 0x00) {
    // 4-byte encoding BOM in 3412 order
    NS_WARNING("Our unicode decoders aren't likely  to deal with this one");
    step = 4;
    pos = 5;
    aCharset = "UTF-32";
  }
  else if (aStyleSheetData[0] == 0xFE && aStyleSheetData[1] == 0xFF) {
    // big-endian 2-byte encoding BOM
    step = 2;
    pos = 3;
    aCharset = "UTF-16BE";
  }
  else if (aStyleSheetData[0] == 0xFF && aStyleSheetData[1] == 0xFE) {
    // little-endian 2-byte encoding BOM
    step = 2;
    pos = 2;
    aCharset = "UTF-16LE";
  }
  else if (aStyleSheetData[0] == 0x00 &&
           aStyleSheetData[1] == 0x00 &&
           aStyleSheetData[2] == 0x00 &&
           aStyleSheetData[3] == 0x40) {
    // big-endian 4-byte encoding, no BOM
    step = 4;
    pos = 3;
  }
  else if (aStyleSheetData[0] == 0x40 &&
           aStyleSheetData[1] == 0x00 &&
           aStyleSheetData[2] == 0x00 &&
           aStyleSheetData[3] == 0x00) {
    // little-endian 4-byte encoding, no BOM
    step = 4;
    pos = 0;
  }
  else if (aStyleSheetData[0] == 0x00 &&
           aStyleSheetData[1] == 0x00 &&
           aStyleSheetData[2] == 0x40 &&
           aStyleSheetData[3] == 0x00) {
    // 4-byte encoding in 2143 order, no BOM
    step = 4;
    pos = 2;
  }
  else if (aStyleSheetData[0] == 0x00 &&
           aStyleSheetData[1] == 0x40 &&
           aStyleSheetData[2] == 0x00 &&
           aStyleSheetData[3] == 0x00) {
    // 4-byte encoding in 3412 order, no BOM
    step = 4;
    pos = 1;
  }
  else if (aStyleSheetData[0] == 0x00 &&
           aStyleSheetData[1] == 0x40 &&
           aStyleSheetData[2] == 0x00 &&
           aStyleSheetData[3] == 0x63) {
    // 2-byte big-endian encoding, no BOM
    step = 2;
    pos = 1;
  }
  else if (aStyleSheetData[0] == 0x40 &&
           aStyleSheetData[1] == 0x00 &&
           aStyleSheetData[2] == 0x63 &&
           aStyleSheetData[3] == 0x00) {
    // 2-byte little-endian encoding, no BOM
    step = 2;
    pos = 0;
  }
  else {
    // no clue what this is
    return NS_ERROR_UNEXPECTED;
  }

  PRUint32 index = 0;
  while (pos < aDataLength && index < sizeof(kCharsetSym) - 1) {
    if (aStyleSheetData[pos] != kCharsetSym[index]) {
      // If we have a guess as to the charset based on the BOM, then
      // we can just return NS_OK even if there is no valid @charset
      // rule.
      return aCharset.IsEmpty() ? NS_ERROR_NOT_AVAILABLE : NS_OK;
    }
    ++index;
    pos += step;
  }

  while (pos < aDataLength && nsCRT::IsAsciiSpace(aStyleSheetData[pos])) {
    pos += step;
  }

  if (pos >= aDataLength ||
      (aStyleSheetData[pos] != '"' && aStyleSheetData[pos] != '\'')) {
    return aCharset.IsEmpty() ? NS_ERROR_NOT_AVAILABLE : NS_OK;
  }

  char quote = aStyleSheetData[pos];
  pos += step;
  nsCAutoString charset;
  while (pos < aDataLength) {
    if (aStyleSheetData[pos] == '\\') {
      pos += step;
      if (pos >= aDataLength) {
        break;
      }          
    } else if (aStyleSheetData[pos] == quote) {
      break;
    }
    
    // casting to avoid ambiguities
    charset.Append(char(aStyleSheetData[pos]));
    pos += step;
  }

  // Check for the ending ';'
  pos += step;
  while (pos < aDataLength && nsCRT::IsAsciiSpace(aStyleSheetData[pos])) {
    pos += step;    
  }

  if (pos >= aDataLength || aStyleSheetData[pos] != ';') {
    return aCharset.IsEmpty() ? NS_ERROR_NOT_AVAILABLE : NS_OK;
  }

  aCharset = charset;
  return NS_OK;
}

NS_IMETHODIMP
SheetLoadData::OnDetermineCharset(nsIUnicharStreamLoader* aLoader,
                                  nsISupports* aContext,
                                  const char* aData,
                                  PRUint32 aDataLength,
                                  nsACString& aCharset)
{
  LOG_URI("SheetLoadData::OnDetermineCharset for '%s'", mURI);
  nsCOMPtr<nsIChannel> channel;
  nsresult result = aLoader->GetChannel(getter_AddRefs(channel));
  if (NS_FAILED(result))
    channel = nsnull;

  aCharset.Truncate();

  /*
   * First determine the charset (if one is indicated)
   * 1)  Check nsIChannel::contentCharset
   * 2)  Check @charset rules in the data
   * 3)  Check "charset" attribute of the <LINK> or <?xml-stylesheet?>
   *
   * If all these fail to give us a charset, fall back on our default
   * (parent sheet charset, document charset or ISO-8859-1 in that order)
   */
  if (channel) {
    channel->GetContentCharset(aCharset);
  }

  result = NS_ERROR_NOT_AVAILABLE;

#ifdef PR_LOGGING
  if (! aCharset.IsEmpty()) {
    LOG(("  Setting from HTTP to: %s", PromiseFlatCString(aCharset).get()));
  }
#endif

  if (aCharset.IsEmpty()) {
    //  We have no charset
    //  Try @charset rule and BOM
    result = GetCharsetFromData((const unsigned char*)aData,
                                aDataLength, aCharset);
#ifdef PR_LOGGING
    if (NS_SUCCEEDED(result)) {
      LOG(("  Setting from @charset rule or BOM: %s",
           PromiseFlatCString(aCharset).get()));
    }
#endif
  }

  if (aCharset.IsEmpty()) {
    // Now try the charset on the <link> or processing instruction
    // that loaded us
    if (mOwningElement) {
      nsAutoString elementCharset;
      mOwningElement->GetCharset(elementCharset);
      LossyCopyUTF16toASCII(elementCharset, aCharset);
#ifdef PR_LOGGING
      if (! aCharset.IsEmpty()) {
        LOG(("  Setting from property on element: %s",
             PromiseFlatCString(aCharset).get()));
      }
#endif
    }
  }

  if (aCharset.IsEmpty() && mParentData) {
    aCharset = mParentData->mCharset;
#ifdef PR_LOGGING
    if (! aCharset.IsEmpty()) {
      LOG(("  Setting from parent sheet: %s",
           PromiseFlatCString(aCharset).get()));
    }
#endif
  }
  
  if (aCharset.IsEmpty() && mLoader->mDocument) {
    // no useful data on charset.  Try the document charset.
    aCharset = mLoader->mDocument->GetDocumentCharacterSet();
#ifdef PR_LOGGING
    LOG(("  Set from document: %s", PromiseFlatCString(aCharset).get()));
#endif
  }      

  if (aCharset.IsEmpty()) {
    NS_WARNING("Unable to determine charset for sheet, using ISO-8859-1!");
#ifdef PR_LOGGING
    LOG_WARN(("  Falling back to ISO-8859-1"));
#endif
    aCharset.AssignLiteral("ISO-8859-1");
  }

  mCharset = aCharset;
  return NS_OK;
}

already_AddRefed<nsIURI>
SheetLoadData::GetReferrerURI()
{
  nsIURI* uri = nsnull;
  if (mParentData)
    mParentData->mSheet->GetSheetURI(&uri);
  if (!uri && mLoader->mDocument)
    NS_IF_ADDREF(uri = mLoader->mDocument->GetDocumentURI());
  return uri;
}

/*
 * Here we need to check that the load did not give us an http error
 * page and check the mimetype on the channel to make sure we're not
 * loading non-text/css data in standards mode.
 */
NS_IMETHODIMP
SheetLoadData::OnStreamComplete(nsIUnicharStreamLoader* aLoader,
                                nsISupports* aContext,
                                nsresult aStatus,
                                nsIUnicharInputStream* aDataStream)
{
  LOG(("SheetLoadData::OnStreamComplete"));
  NS_ASSERTION(!mLoader->mSyncCallback, "Synchronous callback from necko");

  if (mIsCancelled) {
    // Just return.  Don't call SheetComplete -- it's already been
    // called and calling it again will lead to an extra NS_RELEASE on
    // this data and a likely crash.
    return NS_OK;
  }
  
  if (!mLoader->mDocument && !mIsNonDocumentSheet) {
    // Sorry, we don't care about this load anymore
    LOG_WARN(("  No document and not non-document sheet; dropping load"));
    mLoader->SheetComplete(this, NS_BINDING_ABORTED);
    return NS_OK;
  }
  
  nsCOMPtr<nsIChannel> channel;
  nsresult result = aLoader->GetChannel(getter_AddRefs(channel));
  if (NS_FAILED(result))
    channel = nsnull;
  
  nsCOMPtr<nsIURI> channelURI;
  if (channel) {
    // If the channel's original URI is "chrome:", we want that, since
    // the observer code in nsXULPrototypeCache depends on chrome stylesheets
    // having a chrome URI.  (Whether or not chrome stylesheets come through
    // this codepath seems nondeterministic.)
    // Otherwise we want the potentially-HTTP-redirected URI.
    channel->GetOriginalURI(getter_AddRefs(channelURI));
    PRBool isChrome;
    if (NS_FAILED(channelURI->SchemeIs("chrome", &isChrome)) || !isChrome)
      channel->GetURI(getter_AddRefs(channelURI));
  }
  
#ifdef MOZ_TIMELINE
  NS_TIMELINE_OUTDENT();
  NS_TIMELINE_MARK_CHANNEL("SheetLoadData::OnStreamComplete(%s)", channel);
#endif // MOZ_TIMELINE
  
  // If it's an HTTP channel, we want to make sure this is not an
  // error document we got.
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    PRBool requestSucceeded;
    result = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(result) && !requestSucceeded) {
      LOG(("  Load returned an error page"));
      mLoader->SheetComplete(this, NS_ERROR_NOT_AVAILABLE);
      return NS_OK;
    }
  }

  if (aDataStream) {
    nsCAutoString contentType;
    if (channel) {
      channel->GetContentType(contentType);
    }
    
    PRBool validType = contentType.EqualsLiteral("text/css") ||
      contentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE) ||
      contentType.IsEmpty();
                                          
    if (!validType) {
      nsCAutoString spec;
      if (channelURI) {
        channelURI->GetSpec(spec);
      }

      const nsAFlatString& specUTF16 = NS_ConvertUTF8toUTF16(spec);
      const nsAFlatString& ctypeUTF16 = NS_ConvertASCIItoUTF16(contentType);
      const PRUnichar *strings[] = { specUTF16.get(), ctypeUTF16.get() };

      const char *errorMessage;
      PRUint32 errorFlag;

      if (mLoader->mCompatMode == eCompatibility_NavQuirks) {
        errorMessage = "MimeNotCssWarn";
        errorFlag = nsIScriptError::warningFlag;
      } else {
        // Drop the data stream so that we do not load it
        aDataStream = nsnull;

        errorMessage = "MimeNotCss";
        errorFlag = nsIScriptError::errorFlag;
      }
      nsCOMPtr<nsIURI> referrer = GetReferrerURI();
      nsContentUtils::ReportToConsole(nsContentUtils::eCSS_PROPERTIES,
                                      errorMessage,
                                      strings, NS_ARRAY_LENGTH(strings),
                                      referrer, EmptyString(), 0, 0, errorFlag,
                                      "CSS Loader");
    }
  }
  
  if (NS_FAILED(aStatus)) {
    LOG_WARN(("  Load failed: status 0x%x", aStatus));
    mLoader->SheetComplete(this, aStatus);
    return NS_OK;
  }

  if (!aDataStream) {
    LOG_WARN(("  No data stream; bailing"));
    mLoader->SheetComplete(this, NS_ERROR_NOT_AVAILABLE);
    return NS_OK;
  }    

  if (channelURI) {
    // Enough to set the URI on mSheet, since any sibling datas we have share
    // the same mInner as mSheet and will thus get the same URI.
    mSheet->SetURIs(channelURI, channelURI);
  }
  
  PRBool completed;
  return mLoader->ParseSheet(aDataStream, this, completed);
}

#ifdef MOZ_XUL
static PRBool IsChromeURI(nsIURI* aURI)
{
  NS_ASSERTION(aURI, "Have to pass in a URI");
  PRBool isChrome = PR_FALSE;
  aURI->SchemeIs("chrome", &isChrome);
  return isChrome;
}
#endif

PRBool
CSSLoaderImpl::IsAlternate(const nsAString& aTitle, PRBool aHasAlternateRel)
{
  // A sheet is alternate if it has a nonempty title that doesn't match the
  // currently selected style set.  But if there _is_ no currently selected
  // style set, the sheet wasn't marked as an alternate explicitly, and aTitle
  // is nonempty, we should select the style set corresponding to aTitle, since
  // that's a preferred sheet.
  if (aTitle.IsEmpty()) {
    return PR_FALSE;
  }
  
  if (!aHasAlternateRel && mDocument && mPreferredSheet.IsEmpty()) {
    // There's no preferred set yet, and we now have a sheet with a title.
    // Make that be the preferred set.
    mDocument->SetHeaderData(nsGkAtoms::headerDefaultStyle, aTitle);
    // We're definitely not an alternate
    return PR_FALSE;
  }

  return !aTitle.Equals(mPreferredSheet);
}

/**
 * CheckLoadAllowed will return success if the load is allowed,
 * failure otherwise. 
 *
 * @param aSourceURI the uri of the document or parent sheet loading the sheet
 * @param aTargetURI the uri of the sheet to be loaded
 * @param aContext the node owning the sheet.  This is the element or document
 *                 owning the stylesheet (possibly indirectly, for child sheets)
 */
nsresult
CSSLoaderImpl::CheckLoadAllowed(nsIURI* aSourceURI,
                                nsIURI* aTargetURI,
                                nsISupports* aContext)
{
  LOG(("CSSLoaderImpl::CheckLoadAllowed"));
  
  // Check with the security manager
  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
  nsresult rv = secMan->CheckLoadURI(aSourceURI, aTargetURI,
                                     nsIScriptSecurityManager::ALLOW_CHROME);
  if (NS_FAILED(rv)) { // failure is normal here; don't warn
    return rv;
  }

  LOG(("  Passed security check"));

  // Check with content policy

  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_STYLESHEET,
                                 aTargetURI,
                                 aSourceURI,
                                 aContext,
                                 NS_LITERAL_CSTRING("text/css"),
                                 nsnull,                        //extra param
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy());

  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    LOG(("  Load blocked by content policy"));
    return NS_ERROR_CONTENT_BLOCKED;
  }

  return rv;
}

/**
 * CreateSheet() creates an nsICSSStyleSheet object for the given URI,
 * if any.  If there is no URI given, we just create a new style sheet
 * object.  Otherwise, we check for an existing style sheet object for
 * that uri in various caches and clone it if we find it.  Cloned
 * sheets will have the title/media/enabled state of the sheet they
 * are clones off; make sure to call PrepareSheet() on the result of
 * CreateSheet().
 */
nsresult
CSSLoaderImpl::CreateSheet(nsIURI* aURI,
                           nsIContent* aLinkingContent,
                           PRBool aSyncLoad,
                           StyleSheetState& aSheetState,
                           nsICSSStyleSheet** aSheet)
{
  LOG(("CSSLoaderImpl::CreateSheet"));
  NS_PRECONDITION(aSheet, "Null out param!");

  NS_ENSURE_TRUE((mCompleteSheets.IsInitialized() || mCompleteSheets.Init()) &&
                   (mLoadingDatas.IsInitialized() || mLoadingDatas.Init()) &&
                   (mPendingDatas.IsInitialized() || mPendingDatas.Init()),
                 NS_ERROR_OUT_OF_MEMORY);
  
  nsresult rv = NS_OK;
  *aSheet = nsnull;
  aSheetState = eSheetStateUnknown;
  
  if (aURI) {
    aSheetState = eSheetComplete;
    nsCOMPtr<nsICSSStyleSheet> sheet;

    // First, the XUL cache
#ifdef MOZ_XUL
    if (IsChromeURI(aURI)) {
      nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
      if (cache) {
        if (cache->IsEnabled()) {
          sheet = cache->GetStyleSheet(aURI);
          LOG(("  From XUL cache: %p", sheet.get()));
        }
      }
    }
#endif

    if (!sheet) {
      // Then complete sheets
      mCompleteSheets.Get(aURI, getter_AddRefs(sheet));
      LOG(("  From completed: %p", sheet.get()));
    
      // Then loading sheets
      if (!sheet && !aSyncLoad) {
        aSheetState = eSheetLoading;
        SheetLoadData* loadData = nsnull;
        mLoadingDatas.Get(aURI, &loadData);
        if (loadData) {
          sheet = loadData->mSheet;
          LOG(("  From loading: %p", sheet.get()));
        }

        // Then alternate sheets
        if (!sheet) {
          aSheetState = eSheetPending;
          SheetLoadData* loadData = nsnull;
          mPendingDatas.Get(aURI, &loadData);
          if (loadData) {
            sheet = loadData->mSheet;
            LOG(("  From pending: %p", sheet.get()));
          }
        }
      }
    }

    if (sheet) {
      // We can use this cached sheet if it's either incomplete or unmodified
      PRBool modified = PR_TRUE;
      sheet->IsModified(&modified);
      PRBool complete = PR_FALSE;
      sheet->GetComplete(complete);
      if (!modified || !complete) {
        // Proceed on failures; at worst we'll try to create one below
        sheet->Clone(nsnull, nsnull, nsnull, nsnull, aSheet);
        NS_ASSERTION(complete || aSheetState != eSheetComplete,
                     "Sheet thinks it's not complete while we think it is");
      }
    }
  }

  if (!*aSheet) {
    aSheetState = eSheetNeedsParser;
    nsIURI *sheetURI = aURI;
    nsCOMPtr<nsIURI> baseURI = aURI;
    if (!aURI) {
      // Inline style.  Use the document's base URL so that @import in
      // the inline sheet picks up the right base.
      NS_ASSERTION(aLinkingContent, "Inline stylesheet without linking content?");
      baseURI = aLinkingContent->GetBaseURI();
      sheetURI = aLinkingContent->GetDocument()->GetDocumentURI();
    }

    rv = NS_NewCSSStyleSheet(aSheet);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aSheet)->SetURIs(sheetURI, baseURI);
  }

  NS_ASSERTION(*aSheet, "We should have a sheet by now!");
  NS_ASSERTION(aSheetState != eSheetStateUnknown, "Have to set a state!");
  LOG(("  State: %s", gStateStrings[aSheetState]));
  
  return NS_OK;
}

/**
 * PrepareSheet() handles setting the media and title on the sheet, as
 * well as setting the enabled state based on the title and whether
 * the sheet had "alternate" in its rel.
 */
nsresult
CSSLoaderImpl::PrepareSheet(nsICSSStyleSheet* aSheet,
                            const nsSubstring& aTitle,
                            const nsSubstring& aMediaString,
                            nsMediaList* aMediaList,
                            PRBool aHasAlternateRel,
                            PRBool *aIsAlternate)
{
  NS_PRECONDITION(aSheet, "Must have a sheet!");

  nsresult rv;
  nsCOMPtr<nsMediaList> mediaList(aMediaList);

  if (!aMediaString.IsEmpty()) {
    NS_ASSERTION(!aMediaList,
                 "must not provide both aMediaString and aMediaList");
    mediaList = new nsMediaList();
    NS_ENSURE_TRUE(mediaList, NS_ERROR_OUT_OF_MEMORY);
    nsCOMPtr<nsICSSParser> mediumParser;
    nsresult rv = GetParserFor(nsnull, getter_AddRefs(mediumParser));
    NS_ENSURE_SUCCESS(rv, rv);
    // We have aMediaString only when linked from link elements, style
    // elements, or PIs, so pass PR_TRUE.
    rv = mediumParser->ParseMediaList(aMediaString, nsnull, 0, mediaList,
                                      PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
    RecycleParser(mediumParser);
  }

  rv = aSheet->SetMedia(mediaList);
  NS_ENSURE_SUCCESS(rv, rv);

  aSheet->SetTitle(aTitle);
  PRBool alternate = IsAlternate(aTitle, aHasAlternateRel);
  aSheet->SetEnabled(! alternate);
  if (aIsAlternate) {
    *aIsAlternate = alternate;
  }
  return NS_OK;    
}

/**
 * InsertSheetInDoc handles ordering of sheets in the document.  Here
 * we have two types of sheets -- those with linking elements and
 * those without.  The latter are loaded by Link: headers.
 * The following constraints are observed:
 * 1) Any sheet with a linking element comes after all sheets without
 *    linking elements
 * 2) Sheets without linking elements are inserted in the order in
 *    which the inserting requests come in, since all of these are
 *    inserted during header data processing in the content sink
 * 3) Sheets with linking elements are ordered based on document order
 *    as determined by CompareDocumentPosition.
 */
nsresult
CSSLoaderImpl::InsertSheetInDoc(nsICSSStyleSheet* aSheet,
                                nsIContent* aLinkingContent,
                                nsIDocument* aDocument)
{
  LOG(("CSSLoaderImpl::InsertSheetInDoc"));
  NS_PRECONDITION(aSheet, "Nothing to insert");
  NS_PRECONDITION(aDocument, "Must have a document to insert into");

  // all nodes that link in sheets should be implementing nsIDOM3Node

  // XXX Need to cancel pending sheet loads for this element, if any

  PRInt32 sheetCount = aDocument->GetNumberOfStyleSheets();

  /*
   * Start the walk at the _end_ of the list, since in the typical
   * case we'll just want to append anyway.  We want to break out of
   * the loop when insertionPoint points to just before the index we
   * want to insert at.  In other words, when we leave the loop
   * insertionPoint is the index of the stylesheet that immediately
   * precedes the one we're inserting.
   */
  PRInt32 insertionPoint;
  for (insertionPoint = sheetCount - 1; insertionPoint >= 0; --insertionPoint) {
    nsIStyleSheet *curSheet = aDocument->GetStyleSheetAt(insertionPoint);
    NS_ASSERTION(curSheet, "There must be a sheet here!");
    nsCOMPtr<nsIDOMStyleSheet> domSheet = do_QueryInterface(curSheet);
    NS_ASSERTION(domSheet, "All the \"normal\" sheets implement nsIDOMStyleSheet");
    nsCOMPtr<nsIDOMNode> sheetOwner;
    domSheet->GetOwnerNode(getter_AddRefs(sheetOwner));
    if (sheetOwner && !aLinkingContent) {
      // Keep moving; all sheets with a sheetOwner come after all
      // sheets without a linkingNode 
      continue;
    }

    if (!sheetOwner) {
      // Aha!  The current sheet has no sheet owner, so we want to
      // insert after it no matter whether we have a linkingNode
      break;
    }

    nsCOMPtr<nsINode> sheetOwnerNode = do_QueryInterface(sheetOwner);
    NS_ASSERTION(aLinkingContent != sheetOwnerNode,
                 "Why do we still have our old sheet?");

    // Have to compare
    if (nsContentUtils::PositionIsBefore(sheetOwnerNode, aLinkingContent)) {
      // The current sheet comes before us, and it better be the first
      // such, because now we break
      break;
    }
  }

  ++insertionPoint; // adjust the index to the spot we want to insert in
  
  // XXX <meta> elements do not implement nsIStyleSheetLinkingElement;
  // need to fix this for them to be ordered correctly.
  nsCOMPtr<nsIStyleSheetLinkingElement>
    linkingElement = do_QueryInterface(aLinkingContent);
  if (linkingElement) {
    linkingElement->SetStyleSheet(aSheet); // This sets the ownerNode on the sheet
  }

  aDocument->BeginUpdate(UPDATE_STYLE);
  aDocument->InsertStyleSheetAt(aSheet, insertionPoint);
  aDocument->EndUpdate(UPDATE_STYLE);
  LOG(("  Inserting into document at position %d", insertionPoint));

  return NS_OK;
}

/**
 * InsertChildSheet handles ordering of @import-ed sheet in their
 * parent sheets.  Here we want to just insert based on order of the
 * @import rules that imported the sheets.  In theory we can't just
 * append to the end because the CSSOM can insert @import rules.  In
 * practice, we get the call to load the child sheet before the CSSOM
 * has finished inserting the @import rule, so we have no idea where
 * to put it anyway.  So just append for now.
 */
nsresult
CSSLoaderImpl::InsertChildSheet(nsICSSStyleSheet* aSheet,
                                nsICSSStyleSheet* aParentSheet,
                                nsICSSImportRule* aParentRule)
{
  LOG(("CSSLoaderImpl::InsertChildSheet"));
  NS_PRECONDITION(aSheet, "Nothing to insert");
  NS_PRECONDITION(aParentSheet, "Need a parent to insert into");
  NS_PRECONDITION(aParentSheet, "How did we get imported?");

  // child sheets should always start out enabled, even if they got
  // cloned off of top-level sheets which were disabled
  aSheet->SetEnabled(PR_TRUE);
  
  aParentSheet->AppendStyleSheet(aSheet);
  aParentRule->SetSheet(aSheet); // This sets the ownerRule on the sheet

  LOG(("  Inserting into parent sheet"));
  //  LOG(("  Inserting into parent sheet at position %d", insertionPoint));

  return NS_OK;
}

/**
 * LoadSheet handles the actual load of a sheet.  If the load is
 * supposed to be synchronous it just opens a channel synchronously
 * using the given uri, wraps the resulting stream in a converter
 * stream and calls ParseSheet.  Otherwise it tries to look for an
 * existing load for this URI and piggyback on it.  Failing all that,
 * a new load is kicked off asynchronously.
 */
nsresult
CSSLoaderImpl::LoadSheet(SheetLoadData* aLoadData, StyleSheetState aSheetState)
{
  LOG(("CSSLoaderImpl::LoadSheet"));
  NS_PRECONDITION(aLoadData, "Need a load data");
  NS_PRECONDITION(aLoadData->mURI, "Need a URI to load");
  NS_PRECONDITION(aLoadData->mSheet, "Need a sheet to load into");
  NS_PRECONDITION(aSheetState != eSheetComplete, "Why bother?");
  NS_ASSERTION(mLoadingDatas.IsInitialized(), "mLoadingDatas should be initialized by now.");

  LOG_URI("  Load from: '%s'", aLoadData->mURI);
  
  nsresult rv = NS_OK;  

  if (!mDocument && !aLoadData->mIsNonDocumentSheet) {
    // No point starting the load; just release all the data and such.
    LOG_WARN(("  No document and not non-document sheet; pre-dropping load"));
    SheetComplete(aLoadData, NS_BINDING_ABORTED);
    return NS_BINDING_ABORTED;
  }

  if (aLoadData->mSyncLoad) {
    LOG(("  Synchronous load"));
    NS_ASSERTION(!aLoadData->mObserver, "Observer for a sync load?");
    NS_ASSERTION(aSheetState == eSheetNeedsParser,
                 "Sync loads can't reuse existing async loads");

    // Just load it
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_OpenURI(getter_AddRefs(stream), aLoadData->mURI);
    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to open URI synchronously"));
      SheetComplete(aLoadData, rv);
      return rv;
    }

    nsCOMPtr<nsIConverterInputStream> converterStream = 
      do_CreateInstance("@mozilla.org/intl/converter-input-stream;1", &rv);
    
    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to create converter stream"));
      SheetComplete(aLoadData, rv);
      return rv;
    }

    // This forces UA sheets to be UTF-8.  We should really look for
    // @charset rules here via ReadSegments on the raw stream...

    // 8192 is a nice magic number that happens to be what a lot of
    // other things use for buffer sizes.
    rv = converterStream->Init(stream, "UTF-8",
                               8192,
                               nsIConverterInputStream::
                                    DEFAULT_REPLACEMENT_CHARACTER);
    
    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to initialize converter stream"));
      SheetComplete(aLoadData, rv);
      return rv;
    }

    PRBool completed;
    rv = ParseSheet(converterStream, aLoadData, completed);
    NS_ASSERTION(completed, "sync load did not complete");
    return rv;
  }

  SheetLoadData* existingData = nsnull;

  if (aSheetState == eSheetLoading) {
    mLoadingDatas.Get(aLoadData->mURI, &existingData);
    NS_ASSERTION(existingData, "CreateSheet lied about the state");
  }
  else if (aSheetState == eSheetPending){
    mPendingDatas.Get(aLoadData->mURI, &existingData);
    NS_ASSERTION(existingData, "CreateSheet lied about the state");
  }
  
  if (existingData) {
    LOG(("  Glomming on to existing load"));
    SheetLoadData* data = existingData;
    while (data->mNext) {
      data = data->mNext;
    }
    data->mNext = aLoadData; // transfer ownership
    if (aSheetState == eSheetPending && !aLoadData->mWasAlternate) {
      // Kick the load off; someone cares about it right away

#ifdef DEBUG
      SheetLoadData* removedData;
      NS_ASSERTION(mPendingDatas.Get(aLoadData->mURI, &removedData) &&
                   removedData == existingData,
                   "Bad pending table.");
#endif

      mPendingDatas.Remove(aLoadData->mURI);

      LOG(("  Forcing load of pending data"));
      return LoadSheet(existingData, eSheetNeedsParser);
    }
    // All done here; once the load completes we'll be marked complete
    // automatically
    return NS_OK;
  }

#ifdef DEBUG
  mSyncCallback = PR_TRUE;
#endif
  nsCOMPtr<nsILoadGroup> loadGroup;
  if (mDocument) {
    loadGroup = mDocument->GetDocumentLoadGroup();
    NS_ASSERTION(loadGroup,
                 "No loadgroup for stylesheet; onload will fire early");
  }

#ifdef MOZ_TIMELINE
  NS_TIMELINE_MARK_URI("Loading style sheet: %s", aLoadData->mURI);
  NS_TIMELINE_INDENT();
#endif
  
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     aLoadData->mURI, nsnull, loadGroup,
                     nsnull, nsIChannel::LOAD_NORMAL);
  
  if (NS_FAILED(rv)) {
    LOG_ERROR(("  Failed to create channel"));
    SheetComplete(aLoadData, rv);
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    // send a minimal Accept header for text/css
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                  NS_LITERAL_CSTRING("text/css,*/*;q=0.1"),
                                  PR_FALSE);
    nsCOMPtr<nsIURI> referrerURI = aLoadData->GetReferrerURI();
    if (referrerURI)
      httpChannel->SetReferrer(referrerURI);
  }

  // Now tell the channel we expect text/css data back....  We do
  // this before opening it, so it's only treated as a hint.
  channel->SetContentType(NS_LITERAL_CSTRING("text/css"));

  // We don't have to hold on to the stream loader.  The ownership
  // model is: Necko owns the stream loader, which owns the load data,
  // which owns us
  nsCOMPtr<nsIUnicharStreamLoader> streamLoader;
  rv = NS_NewUnicharStreamLoader(getter_AddRefs(streamLoader),
                                 aLoadData);

  if (NS_SUCCEEDED(rv))
    rv = channel->AsyncOpen(streamLoader, nsnull);

#ifdef DEBUG
  mSyncCallback = PR_FALSE;
#endif

  if (NS_FAILED(rv)) {
    LOG_ERROR(("  Failed to create stream loader"));
    SheetComplete(aLoadData, rv);
    return rv;
  }

  if (!mLoadingDatas.Put(aLoadData->mURI, aLoadData)) {
    LOG_ERROR(("  Failed to put data in loading table"));
    aLoadData->mIsCancelled = PR_TRUE;
    channel->Cancel(NS_ERROR_OUT_OF_MEMORY);
    SheetComplete(aLoadData, NS_ERROR_OUT_OF_MEMORY);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  aLoadData->mIsLoading = PR_TRUE;
  
  return NS_OK;
}

/**
 * ParseSheet handles parsing the data stream.  The main idea here is
 * to push the current load data onto the parse stack before letting
 * the CSS parser at the data stream.  That lets us handle @import
 * correctly.
 */
nsresult
CSSLoaderImpl::ParseSheet(nsIUnicharInputStream* aStream,
                          SheetLoadData* aLoadData,
                          PRBool& aCompleted)
{
  LOG(("CSSLoaderImpl::ParseSheet"));
  NS_PRECONDITION(aStream, "Must have data to parse");
  NS_PRECONDITION(aLoadData, "Must have load data");
  NS_PRECONDITION(aLoadData->mSheet, "Must have sheet to parse into");

  aCompleted = PR_FALSE;

  nsCOMPtr<nsICSSParser> parser;
  nsresult rv = GetParserFor(aLoadData->mSheet, getter_AddRefs(parser));
  if (NS_FAILED(rv)) {
    LOG_ERROR(("  Failed to get CSS parser"));
    SheetComplete(aLoadData, rv);
    return rv;
  }

  // The parser insists on passing out a strong ref to the sheet it
  // parsed.  We don't care.
  nsCOMPtr<nsICSSStyleSheet> dummySheet;
  // Push our load data on the stack so any kids can pick it up
  mParsingDatas.AppendElement(aLoadData);
  nsCOMPtr<nsIURI> sheetURI, baseURI;
  aLoadData->mSheet->GetSheetURI(getter_AddRefs(sheetURI));
  aLoadData->mSheet->GetBaseURI(getter_AddRefs(baseURI));
  rv = parser->Parse(aStream, sheetURI, baseURI, aLoadData->mLineNumber,
                     aLoadData->mAllowUnsafeRules,
                     *getter_AddRefs(dummySheet));
  mParsingDatas.RemoveElementAt(mParsingDatas.Count() - 1);
  RecycleParser(parser);

  NS_ASSERTION(aLoadData->mPendingChildren == 0 || !aLoadData->mSyncLoad,
               "Sync load has leftover pending children!");
  
  if (aLoadData->mPendingChildren == 0) {
    LOG(("  No pending kids from parse"));
    aCompleted = PR_TRUE;
    SheetComplete(aLoadData, NS_OK);
  }
  // Otherwise, the children are holding strong refs to the data and
  // will call SheetComplete() on it when they complete.
  
  return NS_OK;
}

/**
 * SheetComplete is the do-it-all cleanup function.  It removes the
 * load data from the "loading" hashtable, adds the sheet to the
 * "completed" hashtable, massages the XUL cache, handles siblings of
 * the load data (other loads for the same URI), handles unblocking
 * blocked parent loads as needed, and most importantly calls
 * NS_RELEASE on the load data to destroy the whole mess.
 */
void
CSSLoaderImpl::SheetComplete(SheetLoadData* aLoadData, nsresult aStatus)
{
  LOG(("CSSLoaderImpl::SheetComplete"));
  NS_PRECONDITION(aLoadData, "Must have a load data!");
  NS_PRECONDITION(aLoadData->mSheet, "Must have a sheet");
  NS_ASSERTION(mLoadingDatas.IsInitialized(),"mLoadingDatas should be initialized by now.");

  LOG(("Load completed, status: 0x%x", aStatus));

  // Twiddle the hashtables
  if (aLoadData->mURI) {
    LOG_URI("  Finished loading: '%s'", aLoadData->mURI);
    // Remove the data from the list of loading datas
    if (aLoadData->mIsLoading) {
#ifdef DEBUG
      SheetLoadData *loadingData;
      NS_ASSERTION(mLoadingDatas.Get(aLoadData->mURI, &loadingData) &&
                   loadingData == aLoadData,
                   "Bad loading table");
#endif

      mLoadingDatas.Remove(aLoadData->mURI);
      aLoadData->mIsLoading = PR_FALSE;
    }
  }
  

  // This is a mess.  If we have a document.write() that writes out
  // two <link> elements pointing to the same url, we will actually
  // end up blocking the same parser twice.  This seems very wrong --
  // if we blocked it the first time, why is more stuff getting
  // written??  In any case, we only want to unblock it once.
  // Otherwise we get icky things like crashes in layout...  We need
  // to stop blocking the parser.  We really do.
  PRBool seenParser = PR_FALSE;

  // Go through and deal with the whole linked list.
  SheetLoadData* data = aLoadData;
  while (data) {

    data->mSheet->SetModified(PR_FALSE); // it's clean
    data->mSheet->SetComplete();
    if (data->mMustNotify && data->mObserver) {
      data->mObserver->StyleSheetLoaded(data->mSheet, data->mWasAlternate,
                                        aStatus);
    }

    // Only unblock the parser if mMustNotify is true (so we're not being
    // called synchronously from LoadSheet) and mWasAlternate is false.
    if (data->mParserToUnblock) {
      LOG(("Parser to unblock: %p", data->mParserToUnblock.get()));
      if (!seenParser && data->mMustNotify && !data->mWasAlternate) {
        LOG(("Unblocking parser: %p", data->mParserToUnblock.get()));
        seenParser = PR_TRUE;
        data->mParserToUnblock->ContinueParsing();
      }
      data->mParserToUnblock = nsnull; // drop the ref, just in case
    }

    NS_ASSERTION(!data->mParentData ||
                 data->mParentData->mPendingChildren != 0,
                 "Broken pending child count on our parent");

    // If we have a parent, our parent is no longer being parsed, and
    // we are the last pending child, then our load completion
    // completes the parent too.  Note that the parent _can_ still be
    // being parsed (eg if the child (us) failed to open the channel
    // or some such).
    if (data->mParentData &&
        --(data->mParentData->mPendingChildren) == 0 &&
        mParsingDatas.IndexOf(data->mParentData) == -1) {
      SheetComplete(data->mParentData, aStatus);
    }
    
    data = data->mNext;
  }

  // Now that it's marked complete, put the sheet in our cache
  if (NS_SUCCEEDED(aStatus) && aLoadData->mURI) {
#ifdef MOZ_XUL
    if (IsChromeURI(aLoadData->mURI)) {
      nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
      if (cache && cache->IsEnabled()) {
        if (!cache->GetStyleSheet(aLoadData->mURI)) {
          LOG(("  Putting sheet in XUL prototype cache"));
          cache->PutStyleSheet(aLoadData->mSheet);
        }
      }
    }
    else {
#endif
      mCompleteSheets.Put(aLoadData->mURI, aLoadData->mSheet);
#ifdef MOZ_XUL
    }
#endif
  }

  NS_RELEASE(aLoadData);  // this will release parents and siblings and all that
  if (mLoadingDatas.Count() == 0 && mPendingDatas.Count() > 0) {
    LOG(("  No more loading sheets; starting alternates"));
    mPendingDatas.Enumerate(StartAlternateLoads, this);
  }
}

NS_IMETHODIMP
CSSLoaderImpl::LoadInlineStyle(nsIContent* aElement,
                               nsIUnicharInputStream* aStream, 
                               PRUint32 aLineNumber,
                               const nsSubstring& aTitle,
                               const nsSubstring& aMedia,
                               nsIParser* aParserToUnblock,
                               nsICSSLoaderObserver* aObserver,
                               PRBool* aCompleted,
                               PRBool* aIsAlternate)

{
  LOG(("CSSLoaderImpl::LoadInlineStyle"));
  NS_PRECONDITION(aStream, "Must have a stream to parse!");
  NS_ASSERTION(mParsingDatas.Count() == 0, "We're in the middle of a parse?");

  *aCompleted = PR_TRUE;

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIStyleSheetLinkingElement> owningElement(do_QueryInterface(aElement));
  NS_ASSERTION(owningElement, "Element is not a style linking element!");
  
  StyleSheetState state;
  nsCOMPtr<nsICSSStyleSheet> sheet;
  nsresult rv = CreateSheet(nsnull, aElement, PR_FALSE, state,
                            getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(state == eSheetNeedsParser,
               "Inline sheets should not be cached");

  rv = PrepareSheet(sheet, aTitle, aMedia, nsnull, PR_FALSE,
                    aIsAlternate);
  NS_ENSURE_SUCCESS(rv, rv);
  
  LOG(("  Sheet is alternate: %d", *aIsAlternate));
  
  rv = InsertSheetInDoc(sheet, aElement, mDocument);
  NS_ENSURE_SUCCESS(rv, rv);
  
  SheetLoadData* data = new SheetLoadData(this, aTitle, aParserToUnblock,
                                          nsnull, sheet, owningElement,
                                          *aIsAlternate, aObserver);

  if (!data) {
    sheet->SetComplete();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(data);
  data->mLineNumber = aLineNumber;
  // Parse completion releases the load data
  rv = ParseSheet(aStream, data, *aCompleted);
  NS_ENSURE_SUCCESS(rv, rv);

  // If aCompleted is true, |data| may well be deleted by now.
  if (!*aCompleted) {
    data->mMustNotify = PR_TRUE;
  }
  return rv;
}        

NS_IMETHODIMP
CSSLoaderImpl::LoadStyleLink(nsIContent* aElement,
                             nsIURI* aURL, 
                             const nsSubstring& aTitle,
                             const nsSubstring& aMedia,
                             PRBool aHasAlternateRel,
                             nsIParser* aParserToUnblock,
                             nsICSSLoaderObserver* aObserver,
                             PRBool* aIsAlternate)
{
  LOG(("CSSLoaderImpl::LoadStyleLink"));
  NS_PRECONDITION(aURL, "Must have URL to load");
  NS_ASSERTION(mParsingDatas.Count() == 0, "We're in the middle of a parse?");

  LOG_URI("  Link uri: '%s'", aURL);
  LOG(("  Link title: '%s'", NS_ConvertUTF16toUTF8(aTitle).get()));
  LOG(("  Link media: '%s'", NS_ConvertUTF16toUTF8(aMedia).get()));
  LOG(("  Link alternate rel: %d", aHasAlternateRel));

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_INITIALIZED);

  // Check whether we should even load
  nsIURI *docURI = mDocument->GetDocumentURI();
  if (!docURI) return NS_ERROR_FAILURE;

  nsISupports* context = aElement;
  if (!context) {
    context = mDocument;
  }
  nsresult rv = CheckLoadAllowed(docURI, aURL, context);
  if (NS_FAILED(rv)) return rv;

  LOG(("  Passed load check"));
  
  StyleSheetState state;
  nsCOMPtr<nsICSSStyleSheet> sheet;
  rv = CreateSheet(aURL, aElement, PR_FALSE, state,
                   getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = PrepareSheet(sheet, aTitle, aMedia, nsnull, aHasAlternateRel,
                    aIsAlternate);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("  Sheet is alternate: %d", *aIsAlternate));
  
  rv = InsertSheetInDoc(sheet, aElement, mDocument);
  NS_ENSURE_SUCCESS(rv, rv);

  if (state == eSheetComplete) {
    LOG(("  Sheet already complete: 0x%p",
         NS_STATIC_CAST(void*, sheet.get())));
    return PostLoadEvent(aURL, sheet, aObserver, aParserToUnblock,
                         *aIsAlternate);
  }

  nsCOMPtr<nsIStyleSheetLinkingElement> owningElement(do_QueryInterface(aElement));

  // Now we need to actually load it
  SheetLoadData* data = new SheetLoadData(this, aTitle, aParserToUnblock, aURL,
                                          sheet, owningElement, *aIsAlternate,
                                          aObserver);
  if (!data) {
    sheet->SetComplete();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(data);

  // If we have to parse and it's an alternate non-inline, defer it
  if (aURL && state == eSheetNeedsParser && mLoadingDatas.Count() != 0 &&
      *aIsAlternate) {
    LOG(("  Deferring alternate sheet load"));
    if (!mPendingDatas.Put(aURL, data)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    data->mMustNotify = PR_TRUE;
    return NS_OK;
  }

  // Load completion will free the data
  rv = LoadSheet(data, state);
  NS_ENSURE_SUCCESS(rv, rv);

  data->mMustNotify = PR_TRUE;
  return rv;
}

NS_IMETHODIMP
CSSLoaderImpl::LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                              nsIURI* aURL, 
                              nsMediaList* aMedia,
                              nsICSSImportRule* aParentRule)
{
  LOG(("CSSLoaderImpl::LoadChildSheet"));
  NS_PRECONDITION(aURL, "Must have a URI to load");

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  LOG_URI("  Child uri: '%s'", aURL);

  // Check whether we should even load
  nsCOMPtr<nsIURI> sheetURI;
  nsresult rv = aParentSheet->GetSheetURI(getter_AddRefs(sheetURI));
  if (NS_FAILED(rv) || !sheetURI) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> owningNode;

  // check for an owning document: if none, don't bother walking up the parent
  // sheets
  nsCOMPtr<nsIDocument> owningDoc;
  rv = aParentSheet->GetOwningDocument(*getter_AddRefs(owningDoc));
  if (NS_SUCCEEDED(rv) && owningDoc) {
    nsCOMPtr<nsIDOMStyleSheet> nextParentSheet(do_QueryInterface(aParentSheet));
    NS_ENSURE_TRUE(nextParentSheet, NS_ERROR_FAILURE); //Not a stylesheet!?

    nsCOMPtr<nsIDOMStyleSheet> topSheet;
    //traverse our way to the top-most sheet
    do {
      topSheet.swap(nextParentSheet);
      topSheet->GetParentStyleSheet(getter_AddRefs(nextParentSheet));
    } while (nextParentSheet);

    topSheet->GetOwnerNode(getter_AddRefs(owningNode));
  }

  nsISupports* context = owningNode;
  if (!context) {
    context = mDocument;
  }
  
  rv = CheckLoadAllowed(sheetURI, aURL, context);
  if (NS_FAILED(rv)) return rv;

  LOG(("  Passed load check"));
  
  SheetLoadData* parentData = nsnull;
  nsCOMPtr<nsICSSLoaderObserver> observer;

  PRInt32 count = mParsingDatas.Count();
  if (count > 0) {
    LOG(("  Have a parent load"));
    parentData = NS_STATIC_CAST(SheetLoadData*,
                                mParsingDatas.ElementAt(count - 1));
    // Check for cycles
    SheetLoadData* data = parentData;
    while (data && data->mURI) {
      PRBool equal;
      if (NS_SUCCEEDED(data->mURI->Equals(aURL, &equal)) && equal) {
        // Houston, we have a loop, blow off this child and pretend this never
        // happened
        LOG_ERROR(("  @import cycle detected, dropping load"));
        return NS_OK;
      }
      data = data->mParentData;
    }

    NS_ASSERTION(parentData->mSheet == aParentSheet,
                 "Unexpected call to LoadChildSheet");
  } else {
    LOG(("  No parent load; must be CSSOM"));
    // No parent load data, so the sheet will need to be notified when
    // we finish, if it can be, if we do the load asynchronously.
    observer = do_QueryInterface(aParentSheet);
  }

  // Now that we know it's safe to load this (passes security check and not a
  // loop) do so
  nsCOMPtr<nsICSSStyleSheet> sheet;
  StyleSheetState state;
  rv = CreateSheet(aURL, nsnull,
                   parentData ? parentData->mSyncLoad : PR_FALSE,
                   state, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  const nsSubstring& empty = EmptyString();
  rv = PrepareSheet(sheet, empty, empty, aMedia);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = InsertChildSheet(sheet, aParentSheet, aParentRule);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (state == eSheetComplete) {
    LOG(("  Sheet already complete"));
    // We're completely done.  No need to notify, even, since the
    // @import rule addition/modification will trigger the right style
    // changes automatically.
    return NS_OK;
  }

  
  SheetLoadData* data = new SheetLoadData(this, aURL, sheet, parentData,
                                          observer);

  if (!data) {
    sheet->SetComplete();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(data);
  PRBool syncLoad = data->mSyncLoad;

  // Load completion will release the data
  rv = LoadSheet(data, state);
  NS_ENSURE_SUCCESS(rv, rv);

  // If syncLoad is true, |data| will be deleted by now.
  if (!syncLoad) {
    data->mMustNotify = PR_TRUE;
  }
  return rv;  
}

NS_IMETHODIMP
CSSLoaderImpl::LoadSheetSync(nsIURI* aURL, PRBool aAllowUnsafeRules,
                             nsICSSStyleSheet** aSheet)
{
  LOG(("CSSLoaderImpl::LoadSheetSync"));
  return InternalLoadNonDocumentSheet(aURL, aAllowUnsafeRules, aSheet, nsnull);
}

NS_IMETHODIMP
CSSLoaderImpl::LoadSheet(nsIURI* aURL, nsICSSLoaderObserver* aObserver,
                         nsICSSStyleSheet** aSheet)
{
  LOG(("CSSLoaderImpl::LoadSheet(aURL, aObserver, aSheet) api call"));
  NS_PRECONDITION(aSheet, "aSheet is null");
  return InternalLoadNonDocumentSheet(aURL, PR_FALSE, aSheet, aObserver);
}

NS_IMETHODIMP
CSSLoaderImpl::LoadSheet(nsIURI* aURL, nsICSSLoaderObserver* aObserver)
{
  LOG(("CSSLoaderImpl::LoadSheet(aURL, aObserver) api call"));
  return InternalLoadNonDocumentSheet(aURL, PR_FALSE, nsnull, aObserver);
}

nsresult
CSSLoaderImpl::InternalLoadNonDocumentSheet(nsIURI* aURL, 
                                            PRBool aAllowUnsafeRules,
                                            nsICSSStyleSheet** aSheet,
                                            nsICSSLoaderObserver* aObserver)
{
  NS_PRECONDITION(aURL, "Must have a URI to load");
  NS_PRECONDITION(aSheet || aObserver, "Sheet and observer can't both be null");
  NS_ASSERTION(mParsingDatas.Count() == 0, "We're in the middle of a parse?");

  LOG_URI("  Non-document sheet uri: '%s'", aURL);
  
  if (aSheet) {
    *aSheet = nsnull;
  }
  
  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  StyleSheetState state;
  nsCOMPtr<nsICSSStyleSheet> sheet;
  PRBool syncLoad = (aObserver == nsnull);
  
  nsresult rv = CreateSheet(aURL, nsnull, syncLoad, state,
                            getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  const nsSubstring& empty = EmptyString();
  rv = PrepareSheet(sheet, empty, empty, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (state == eSheetComplete) {
    LOG(("  Sheet already complete"));
    if (aObserver) {
      rv = PostLoadEvent(aURL, sheet, aObserver, nsnull, PR_FALSE);
    }
    if (aSheet) {
      sheet.swap(*aSheet);
    }
    return rv;
  }

  SheetLoadData* data =
    new SheetLoadData(this, aURL, sheet, syncLoad, aAllowUnsafeRules, aObserver);

  if (!data) {
    sheet->SetComplete();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ADDREF(data);
  rv = LoadSheet(data, state);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSheet) {
    sheet.swap(*aSheet);
  }
  if (aObserver) {
    data->mMustNotify = PR_TRUE;
  }

  return rv;
}

nsresult
CSSLoaderImpl::PostLoadEvent(nsIURI* aURI,
                             nsICSSStyleSheet* aSheet,
                             nsICSSLoaderObserver* aObserver,
                             nsIParser* aParserToUnblock,
                             PRBool aWasAlternate)
{
  LOG(("nsCSSLoader::PostLoadEvent"));
  NS_PRECONDITION(aSheet, "Must have sheet");
  // XXXbz can't assert this yet; have to post even with a null
  // observer, since we may need to unblock the parser
  // NS_PRECONDITION(aObserver, "Must have observer");

  nsRefPtr<SheetLoadData> evt =
    new SheetLoadData(this, EmptyString(), // title doesn't matter here
                      aParserToUnblock,
                      aURI,
                      aSheet,
                      nsnull,  // owning element doesn't matter here
                      aWasAlternate,
                      aObserver);
  NS_ENSURE_TRUE(evt, NS_ERROR_OUT_OF_MEMORY);

  if (!mPostedEvents.AppendElement(evt)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_DispatchToCurrentThread(evt);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch stylesheet load event");
    mPostedEvents.RemoveElement(evt);
  } else {
    // We'll unblock onload when we handle the event.
    if (mDocument) {
      mDocument->BlockOnload();
    }

    // We want to notify the observer for this data.
    evt->mMustNotify = PR_TRUE;
  }

  return rv;
}

void
CSSLoaderImpl::HandleLoadEvent(SheetLoadData* aEvent)
{
  // XXXbz can't assert this yet.... May not have an observer because
  // we're unblocking the parser
  // NS_ASSERTION(aEvent->mObserver, "Must have observer");
  NS_ASSERTION(aEvent->mSheet, "Must have sheet");
  if (!aEvent->mIsCancelled) {
    // SheetComplete will call Release(), so give it a reference to do
    // that with.
    NS_ADDREF(aEvent);
    SheetComplete(aEvent, NS_OK);
  }

  mPostedEvents.RemoveElement(aEvent);

  if (mDocument) {
    mDocument->UnblockOnload(PR_TRUE);
  }
}

nsresult NS_NewCSSLoader(nsIDocument* aDocument, nsICSSLoader** aLoader)
{
  CSSLoaderImpl* it = new CSSLoaderImpl();

  NS_ENSURE_TRUE(it, NS_ERROR_OUT_OF_MEMORY);

  it->Init(aDocument);
  return CallQueryInterface(it, aLoader);
}

nsresult NS_NewCSSLoader(nsICSSLoader** aLoader)
{
  CSSLoaderImpl* it = new CSSLoaderImpl();

  NS_ENSURE_TRUE(it, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(it, aLoader);
}

PR_STATIC_CALLBACK(PLDHashOperator)
StopLoadingSheetCallback(nsIURI* aKey, SheetLoadData*& aData, void* aClosure)
{
  NS_PRECONDITION(aData, "Must have a data!");
  NS_PRECONDITION(aClosure, "Must have a loader");

  aData->mIsLoading = PR_FALSE; // we will handle the removal right here
  aData->mIsCancelled = PR_TRUE;
  
  NS_STATIC_CAST(CSSLoaderImpl*,aClosure)->SheetComplete(aData,
                                                         NS_BINDING_ABORTED);

  return PL_DHASH_REMOVE;
}

NS_IMETHODIMP
CSSLoaderImpl::Stop()
{
  if (mLoadingDatas.IsInitialized() && mLoadingDatas.Count() > 0) {
    mLoadingDatas.Enumerate(StopLoadingSheetCallback, this);
  }
  if (mPendingDatas.IsInitialized() && mPendingDatas.Count() > 0) {
    mPendingDatas.Enumerate(StopLoadingSheetCallback, this);
  }
  for (PRUint32 i = 0; i < mPostedEvents.Length(); ++i) {
    SheetLoadData* data = mPostedEvents[i];
    data->mIsCancelled = PR_TRUE;
    // SheetComplete() calls Release(), so give this an extra ref.
    NS_ADDREF(data);
    data->mLoader->SheetComplete(data, NS_BINDING_ABORTED);
  }
  mPostedEvents.Clear();
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::StopLoadingSheet(nsIURI* aURL)
{
  NS_ENSURE_TRUE(aURL, NS_ERROR_NULL_POINTER);

  SheetLoadData* loadData = nsnull;
  if (mLoadingDatas.IsInitialized()) {
    mLoadingDatas.Get(aURL, &loadData);
  }
  if (!loadData && mPendingDatas.IsInitialized()) {
    mPendingDatas.Get(aURL, &loadData);
    if (loadData) {
      // have to remove from mPendingDatas ourselves, since
      // SheetComplete won't do that.
      mPendingDatas.Remove(aURL);
    }
  }
    
  if (!loadData) {
    for (PRUint32 i = 0; i < mPostedEvents.Length(); ++i) {
      SheetLoadData* curData = mPostedEvents[i];
      PRBool equal;
      if (curData->mURI && NS_SUCCEEDED(curData->mURI->Equals(aURL, &equal)) &&
          equal) {
        loadData = curData;
        // SheetComplete() calls Release(), so give it an extra ref.
        NS_ADDREF(curData);
        mPostedEvents.RemoveElementAt(i);
        break;
      }
    }
  }
          
  if (loadData) {
    loadData->mIsCancelled = PR_TRUE;
    SheetComplete(loadData, NS_BINDING_ABORTED);
  }
           
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::GetEnabled(PRBool *aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = mEnabled;
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::SetEnabled(PRBool aEnabled)
{
  mEnabled = aEnabled;
  return NS_OK;
}
