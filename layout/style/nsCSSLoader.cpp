/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *       Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#include "nsICSSLoader.h"
#include "nsICSSLoaderObserver.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"

#include "nsIParser.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIUnicharInputStream.h"
#include "nsIConverterInputStream.h"
#include "nsICharsetAlias.h"
#include "nsUnicharUtils.h"
#include "nsHashtable.h"
#include "nsIURL.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIScriptSecurityManager.h"
#include "nsITimelineService.h"
#include "nsIHttpChannel.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsMimeTypes.h"
#include "nsIAtom.h"

#ifdef INCLUDE_XUL
#include "nsIXULPrototypeCache.h"
#endif

#include "nsIDOMMediaList.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSImportRule.h"

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

static NS_DEFINE_CID(kCStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

class CSSLoaderImpl;

/**
 * OVERALL ARCHITECTURE
 *
 * The CSS Loader gets requests to load various sorts of style sheets:
 * inline style from <style> elements, linked style, @import-ed child
 * sheets, agent sheets.  The loader handles the following tasks:
 *
 * 1) Creation of the actual style sheet objects: CreateSheet()
 * 2) setting of the right media, title, enabled state, etc on the
 *    sheet: PrepareSheet()
 * 3) Insertion of the sheet in the proper cascade order:
 *    InsertSheetInDoc() and InsertChildSheet()
 * 4) Load of the sheet: LoadSheet()
 * 5) Parsing of the sheet: ParseSheet()
 * 6) Cleanup: SheetComplete()
 *
 * The detailed documentation for these functions is found with the
 * function implementations.
 *
 * The following helper objects are used:
 * A) URLKey -- a nsHashKey implementation that uses an nsIURI as the key
 * B) SheetLoadData -- a small class that is used to store all the
 *                     information needed for the loading of a sheet;
 *                     this class handles listening for the stream
 *                     loader completion and also handles charset
 *                     determination.
 */

/************************************
 * URLKey class used for hashtables *
 ************************************/
                                                            
MOZ_DECL_CTOR_COUNTER(URLKey)

class URLKey: public nsHashKey {
public:
  URLKey(nsIURI* aURL)
    : mURL(aURL)
  {
    MOZ_COUNT_CTOR(URLKey);
    mHashValue = 0;

    nsCAutoString spec;
    mURL->GetSpec(spec);
    if (!spec.IsEmpty()) {
      mHashValue = nsCRT::HashCode(spec.get());
    }
  }

  URLKey(const URLKey& aKey)
    : mURL(aKey.mURL),
      mHashValue(aKey.mHashValue)
  {
    MOZ_COUNT_CTOR(URLKey);
  }

  virtual ~URLKey(void)
  {
    MOZ_COUNT_DTOR(URLKey);
  }

  virtual PRUint32 HashCode(void) const
  {
    return mHashValue;
  }

  virtual PRBool Equals(const nsHashKey* aKey) const
  {
    URLKey* key = (URLKey*)aKey;
    PRBool equals = PR_FALSE;
    nsresult result = mURL->Equals(key->mURL, &equals);
    return (NS_SUCCEEDED(result) && equals);
  }

  virtual nsHashKey *Clone(void) const
  {
    return new URLKey(*this);
  }

  nsCOMPtr<nsIURI>  mURL;
  PRUint32          mHashValue;
};

/*********************************************
 * Data needed to properly load a stylesheet *
 *********************************************/

class SheetLoadData : public nsIUnicharStreamLoaderObserver
{
public:
  virtual ~SheetLoadData(void);
  // Data for loading a sheet linked from a document
  SheetLoadData(CSSLoaderImpl* aLoader,
                const nsAString& aTitle,
                nsIParser* aParserToUnblock,
                nsIURI* aURI,
                nsICSSStyleSheet* aSheet,
                nsIStyleSheetLinkingElement* aOwningElement,
                nsICSSLoaderObserver* aObserver);                 

  // Data for loading a sheet linked from an @import rule
  SheetLoadData(CSSLoaderImpl* aLoader,
                nsIURI* aURI,
                nsICSSStyleSheet* aSheet,
                SheetLoadData* aParentData,
                nsICSSLoaderObserver* aObserver);                 

  // Data for loading an agent sheet
  SheetLoadData(CSSLoaderImpl* aLoader,
                nsIURI* aURI,
                nsICSSStyleSheet* aSheet,
                nsICSSLoaderObserver* aObserver);
                 
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIUNICHARSTREAMLOADEROBSERVER

  // Hold a ref to the CSSLoader so we can call back to it to let it
  // know the load finished
  CSSLoaderImpl*             mLoader; // strong ref

  // Title needed to pull datas out of the pending datas table when
  // the preferred title is changed
  nsString                   mTitle;

  // Parser to be told to continue parsing once the load completes
  nsCOMPtr<nsIParser>        mParserToUnblock;

  // URI we're loading.  Null for inline sheets
  nsCOMPtr<nsIURI>           mURI;

  // The sheet we're loading data for
  nsCOMPtr<nsICSSStyleSheet> mSheet;

  // Linked list of datas for the same URI as us
  SheetLoadData*             mNext;  // strong ref

  // Load data for the sheet that @import-ed us if we were @import-ed
  // during the parse
  SheetLoadData*             mParentData;  // strong ref

  // Number of sheets we @import-ed that are still loading
  PRUint32                   mPendingChildren;

  // mSyncLoad is true when the load needs to be synchronous -- right
  // now only for LoadAgentSheet with no observer
  PRPackedBool               mSyncLoad;

  // mIsAgent is true if the load was triggered by LoadAgentSheet or
  // an @import from an agent sheet.  Agent loads can proceed even if
  // we have no document.
  PRPackedBool               mIsAgent;

  // mIsLoading is true from the moment we are placed in the loader's
  // "loading datas" table (right after the async channel is opened)
  // to the moment we are removed from said table (due to the load
  // completing or being cancelled).
  PRPackedBool               mIsLoading; // Set once the data is in the "loading" table

  // mIsCancelled is set to true when a sheet load is stopped by
  // Stop() or StopLoadingSheet().  SheetLoadData::OnStreamComplete()
  // checks this to avoid parsing sheets that have been cancelled and
  // such.
  PRPackedBool               mIsCancelled;
  
  // This is the element that imported the sheet.  Needed to get the
  // charset set on it.
  nsCOMPtr<nsIStyleSheetLinkingElement> mOwningElement;

  // The observer that wishes to be notified of load completion
  nsCOMPtr<nsICSSLoaderObserver>        mObserver;
};

NS_IMPL_ISUPPORTS1(SheetLoadData, nsIUnicharStreamLoaderObserver);

/***********************************************************************
 * Enum that describes the state of the sheet returned by CreateSheet. *
 ***********************************************************************/
enum StyleSheetState {
  eSheetStateUnknown = 0,
  eSheetNeedsParser,
  eSheetPending,
  eSheetLoading,
  eSheetComplete
};

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

/**********************
 * Loader Declaration *
 **********************/

class CSSLoaderImpl : public nsICSSLoader
{
public:
  CSSLoaderImpl(void);
  virtual ~CSSLoaderImpl(void);

  NS_DECL_ISUPPORTS

  // nsICSSLoader methods
  NS_IMETHOD Init(nsIDocument* aDocument);
  NS_IMETHOD DropDocumentReference(void);

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive);
  NS_IMETHOD SetCompatibilityMode(nsCompatibility aCompatMode);
  NS_IMETHOD SetPreferredSheet(const nsAString& aTitle);

  NS_IMETHOD GetParserFor(nsICSSStyleSheet* aSheet,
                          nsICSSParser** aParser);
  NS_IMETHOD RecycleParser(nsICSSParser* aParser);

  NS_IMETHOD LoadInlineStyle(nsIContent* aElement,
                             nsIUnicharInputStream* aIn, 
                             const nsAString& aTitle, 
                             const nsAString& aMedia, 
                             PRInt32 aDefaultNameSpaceID,
                             nsIParser* aParserToUnblock,
                             PRBool& aCompleted,
                             nsICSSLoaderObserver* aObserver);

  NS_IMETHOD LoadStyleLink(nsIContent* aElement,
                           nsIURI* aURL, 
                           const nsAString& aTitle, 
                           const nsAString& aMedia, 
                           PRInt32 aDefaultNameSpaceID,
                           nsIParser* aParserToUnblock,
                           PRBool& aCompleted,
                           nsICSSLoaderObserver* aObserver);

  NS_IMETHOD LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                            nsIURI* aURL, 
                            const nsAString& aMedia,
                            PRInt32 aDefaultNameSpaceID,
                            nsICSSImportRule* aRule);

  NS_IMETHOD LoadAgentSheet(nsIURI* aURL, nsICSSStyleSheet** aSheet);

  NS_IMETHOD LoadAgentSheet(nsIURI* aURL, nsICSSLoaderObserver* aObserver);

  // stop loading all sheets
  NS_IMETHOD Stop(void);

  // stop loading one sheet
  NS_IMETHOD StopLoadingSheet(nsIURI* aURL);

  /**
   * Is the loader enabled or not.
   * When disabled, processing of new styles is disabled and an attempt
   * to do so will fail with a return code of
   * NS_ERROR_NOT_AVAILABLE. Note that this DOES NOT disable
   * currently loading styles or already processed styles.
   */
  NS_IMETHOD GetEnabled(PRBool *aEnabled);
  NS_IMETHOD SetEnabled(PRBool aEnabled);

  // local helper methods (some are public for access from statics)
  PRBool IsAlternate(const nsAString& aTitle);

private:
  nsresult CreateSheet(nsIURI* aURI,
                       PRUint32 aDefaultNameSpaceID,
                       StyleSheetState& aSheetState,
                       nsICSSStyleSheet** aSheet);

  nsresult PrepareSheet(nsICSSStyleSheet* aSheet,
                        const nsAString& aTitle,
                        const nsAString& aMedia);

  nsresult InsertSheetInDoc(nsICSSStyleSheet* aSheet,
                            nsIContent* aLinkingElement,
                            nsIDocument* aDocument);

  nsresult InsertChildSheet(nsICSSStyleSheet* aSheet,
                            nsICSSStyleSheet* aParentSheet,
                            nsICSSImportRule* aParentRule);

  nsresult InternalLoadAgentSheet(nsIURI* aURL,
                                  nsICSSStyleSheet** aSheet,
                                  nsICSSLoaderObserver* aObserver);
  
public:
  nsresult LoadSheet(SheetLoadData* aLoadData, StyleSheetState aSheetState);

protected:
  friend class SheetLoadData;

  // Protected functions and members are ones that SheetLoadData needs
  // access to
  nsresult ParseSheet(nsIUnicharInputStream* aStream,
                      SheetLoadData* aLoadData,
                      PRBool& aCompleted);

public:
  void SheetComplete(SheetLoadData* aLoadData, PRBool aSucceeded);

protected:
  // the load data needs access to the document...
  nsIDocument*      mDocument;  // the document we live for

#ifdef DEBUG
  PRPackedBool            mSyncCallback;
#endif

private:
  PRPackedBool      mCaseSensitive; // is document CSS case sensitive
  PRPackedBool      mEnabled; // is enabled to load new styles
  nsCompatibility   mCompatMode;
  nsString          mPreferredSheet;  // title of preferred sheet

  nsCOMArray<nsICSSParser> mParsers;  // array of CSS parsers

  nsSupportsHashtable mCompleteSheets;
  nsHashtable         mLoadingDatas;  // weak refs
  nsHashtable         mPendingDatas;  // weak refs
  
  // We're not likely to have many levels of @import...  But likely to have
  // some.  Allocate some storage, what the hell.
  nsAutoVoidArray   mParsingDatas;

};

/********************************
 * SheetLoadData implementation *
 ********************************/

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader,
                             const nsAString& aTitle,
                             nsIParser* aParserToUnblock,
                             nsIURI* aURI,
                             nsICSSStyleSheet* aSheet,
                             nsIStyleSheetLinkingElement* aOwningElement,
                             nsICSSLoaderObserver* aObserver)
  : mLoader(aLoader),
    mTitle(aTitle),
    mParserToUnblock(aParserToUnblock),
    mURI(aURI),
    mSheet(aSheet),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mSyncLoad(PR_FALSE),
    mIsAgent(PR_FALSE),
    mIsLoading(PR_FALSE),
    mIsCancelled(PR_FALSE),
    mOwningElement(aOwningElement),
    mObserver(aObserver)
{
  NS_INIT_ISUPPORTS();

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
    mSheet(aSheet),
    mNext(nsnull),
    mParentData(aParentData),
    mPendingChildren(0),
    mSyncLoad(PR_FALSE),
    mIsAgent(PR_FALSE),
    mIsLoading(PR_FALSE),
    mIsCancelled(PR_FALSE),
    mOwningElement(nsnull),
    mObserver(aObserver)
{
  NS_INIT_ISUPPORTS();

  NS_PRECONDITION(mLoader, "Must have a loader!");
  NS_ADDREF(mLoader);
  if (mParentData) {
    NS_ADDREF(mParentData);
    mSyncLoad = mParentData->mSyncLoad;
    mIsAgent = mParentData->mIsAgent;
    ++(mParentData->mPendingChildren);
  }
}

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader,
                             nsIURI* aURI,
                             nsICSSStyleSheet* aSheet,
                             nsICSSLoaderObserver* aObserver)
  : mLoader(aLoader),
    mParserToUnblock(nsnull),
    mURI(aURI),
    mSheet(aSheet),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mSyncLoad(nsnull == aObserver),
    mIsAgent(PR_TRUE),
    mIsLoading(PR_FALSE),
    mIsCancelled(PR_FALSE),
    mOwningElement(nsnull),
    mObserver(aObserver)
{
  NS_INIT_ISUPPORTS();

  NS_PRECONDITION(mLoader, "Must have a loader!");
  NS_ADDREF(mLoader);
}

SheetLoadData::~SheetLoadData()
{
  NS_RELEASE(mLoader);
  NS_IF_RELEASE(mParentData);
  NS_IF_RELEASE(mNext);
}

/*************************
 * Loader Implementation *
 *************************/

CSSLoaderImpl::CSSLoaderImpl(void)
  : mDocument(nsnull), 
    mCaseSensitive(PR_FALSE),
    mEnabled(PR_TRUE), 
    mCompatMode(eCompatibility_FullStandards)
{
  NS_INIT_ISUPPORTS();
}

CSSLoaderImpl::~CSSLoaderImpl(void)
{
  NS_ASSERTION(mLoadingDatas.Count() == 0,
               "How did we get destroyed when there are loading data?");
  NS_ASSERTION(mPendingDatas.Count() == 0,
               "How did we get destroyed when there are pending data?");
}

NS_IMPL_ISUPPORTS1(CSSLoaderImpl, nsICSSLoader)

NS_IMETHODIMP
CSSLoaderImpl::Init(nsIDocument* aDocument)
{
  NS_ASSERTION(! mDocument, "already initialized");
  if (! mDocument) {
    mDocument = aDocument;
    return NS_OK;
  }
  return NS_ERROR_ALREADY_INITIALIZED;
}

PR_STATIC_CALLBACK(PRIntn)
StartAlternateLoads(nsHashKey *aKey, void *aData, void* aClosure)
{
  SheetLoadData* data = NS_STATIC_CAST(SheetLoadData*, aData);
  CSSLoaderImpl* loader = NS_STATIC_CAST(CSSLoaderImpl*, aClosure);

  loader->LoadSheet(data, eSheetNeedsParser);
  return kHashEnumerateRemove;
}

NS_IMETHODIMP
CSSLoaderImpl::DropDocumentReference(void)
{
  mDocument = nsnull;
  // Flush out pending datas just so we don't leak by accident.  These
  // loads should short-circuit through the mDocument check in
  // LoadSheet and just end up in SheetComplete immediately
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

PR_STATIC_CALLBACK(PRIntn)
StartNonAlternates(nsHashKey *aKey, void *aData, void* aClosure)
{
  NS_PRECONDITION(aData, "Must have a data");
  NS_PRECONDITION(aClosure, "Must have a loader");
  
  SheetLoadData* data = NS_STATIC_CAST(SheetLoadData*, aData);
  CSSLoaderImpl* loader = NS_STATIC_CAST(CSSLoaderImpl*, aClosure);
  if (loader->IsAlternate(data->mTitle)) {
    return kHashEnumerateNext;
  }

  // Need to start the load
  loader->LoadSheet(data, eSheetNeedsParser);
  return kHashEnumerateRemove;
}

NS_IMETHODIMP
CSSLoaderImpl::SetPreferredSheet(const nsAString& aTitle)
{
  mPreferredSheet = aTitle;

  // start any pending alternates that aren't alternates anymore
  mPendingDatas.Enumerate(StartNonAlternates, this);
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::GetParserFor(nsICSSStyleSheet* aSheet, 
                            nsICSSParser** aParser)
{
  NS_ASSERTION(aParser, "null pointer");
  if (! aParser) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult result = NS_OK;

  *aParser = nsnull;
  PRInt32 count = mParsers.Count();
  if (0 < count--) {
    *aParser = mParsers[count];
    NS_ADDREF(*aParser);
    mParsers.RemoveObjectAt(count);
  }

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

  if (!mParsers.AppendObject(aParser)) {
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

// XXX We call this function a good bit.  Consider caching the service
// in a static global or something?
static nsresult ResolveCharset(const nsAString& aCharsetAlias,
                               nsAString& aCharset)
{
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  if (! aCharsetAlias.IsEmpty()) {
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &rv));
    NS_ASSERTION(calias, "cannot find charset alias service");
    if (calias)
    {
      rv = calias->GetPreferred(aCharsetAlias, aCharset);
    }
  }
  return rv;
}

static const char kCharsetSym[] = "@charset";

static nsresult GetCharsetFromData(const unsigned char* aStyleSheetData,
                                   PRUint32 aDataLength,
                                   nsAString& aCharset)
{
  aCharset.Truncate();
  if (aDataLength <= sizeof(kCharsetSym) - 1)
    return NS_ERROR_NOT_AVAILABLE;
  PRUint32 step = 1;
  PRUint32 pos = 0;
  // Determine the encoding type
  if (*aStyleSheetData == 0x40 && *(aStyleSheetData+1) == 0x63 /* '@c' */ ) {
    // 1-byte ASCII-based encoding (ISO-8859-*, UTF-8, etc)
    step = 1;
    pos = 0;
  }
  else if (*aStyleSheetData == 0xEF &&
           *(aStyleSheetData+1) == 0xBB &&
           *(aStyleSheetData+2) == 0xBF) {
    // UTF-8 BOM
    step = 1;
    pos = 3;
  }
  else if (*aStyleSheetData == 0xFE && *(aStyleSheetData+1) == 0xFF) {
    // big-endian 2-byte encoding BOM
    step = 2;
    pos = 3;
  }
  else if (*aStyleSheetData == 0xFF && *(aStyleSheetData+1) == 0xFE) {
    // little-endian 2-byte encoding BOM
    NS_WARNING("Our unicode decoders aren't likely  to deal with this one");
    step = 2;
    pos = 2;
  }
  else if (*aStyleSheetData == 0x00 &&
           *(aStyleSheetData+1) == 0x00 &&
           *(aStyleSheetData+2) == 0xFE &&
           *(aStyleSheetData+3) == 0xFF) {
    // big-endian 4-byte encoding BOM
    step = 4;
    pos = 7;
  }
  else if (*aStyleSheetData == 0xFF &&
           *(aStyleSheetData+1) == 0xFE &&
           *(aStyleSheetData+2) == 0x00 &&
           *(aStyleSheetData+3) == 0x00) {
    // little-endian 4-byte encoding BOM
    NS_WARNING("Our unicode decoders aren't likely to deal with this one");
    step = 4;
    pos = 4;
  }
  else if (*aStyleSheetData == 0x00 &&
           *(aStyleSheetData+1) == 0x00 &&
           *(aStyleSheetData+2) == 0xFF &&
           *(aStyleSheetData+3) == 0xFE) {
    // 4-byte encoding BOM in 2143 order
    NS_WARNING("Our unicode decoders aren't likely to deal with this one");
    step = 4;
    pos = 6;
  }
  else if (*aStyleSheetData == 0xFE &&
           *(aStyleSheetData+1) == 0xFF &&
           *(aStyleSheetData+2) == 0x00 &&
           *(aStyleSheetData+3) == 0x00) {
    // 4-byte encoding BOM in 3412 order
    NS_WARNING("Our unicode decoders aren't likely to deal with this one");
    step = 4;
    pos = 5;
  }
  else if (*aStyleSheetData == 0x00 &&
           *(aStyleSheetData+1) == 0x00 &&
           *(aStyleSheetData+2) == 0x00 &&
           *(aStyleSheetData+3) == 0x40) {
    // big-endian 4-byte encoding, no BOM
    step = 4;
    pos = 3;
  }
  else if (*aStyleSheetData == 0x40 &&
           *(aStyleSheetData+1) == 0x00 &&
           *(aStyleSheetData+2) == 0x00 &&
           *(aStyleSheetData+3) == 0x00) {
    // little-encoding 4-byte encoding, no BOM
    NS_WARNING("Our unicode decoders aren't likely to deal with this one");
    step = 4;
    pos = 0;
  }
  else if (*aStyleSheetData == 0x00 &&
           *(aStyleSheetData+1) == 0x00 &&
           *(aStyleSheetData+2) == 0x40 &&
           *(aStyleSheetData+3) == 0x00) {
    // 4-byte encoding in 2143 order, no BOM
    NS_WARNING("Our unicode decoders aren't likely to deal with this one");
    step = 4;
    pos = 2;
  }
  else if (*aStyleSheetData == 0x00 &&
           *(aStyleSheetData+1) == 0x40 &&
           *(aStyleSheetData+2) == 0x00 &&
           *(aStyleSheetData+3) == 0x00) {
    // 4-byte encoding in 3412 order, no BOM
    NS_WARNING("Our unicode decoders aren't likely to deal with this one");
    step = 4;
    pos = 1;
  }
  else if (*aStyleSheetData == 0x00 &&
           *(aStyleSheetData+1) == 0x40 &&
           *(aStyleSheetData+2) == 0x00 &&
           *(aStyleSheetData+3) == 0x00) {
    // 4-byte encoding in 3412 order, no BOM
    NS_WARNING("Our unicode decoders aren't likely to deal with this one");
    step = 4;
    pos = 1;
  }
  else if (*aStyleSheetData == 0x00 &&
           *(aStyleSheetData+1) == 0x40 &&
           *(aStyleSheetData+2) == 0x00 &&
           *(aStyleSheetData+3) == 0x63) {
    // 2-byte big-endian encoding, no BOM
    step = 2;
    pos = 1;
  }
  else if (*aStyleSheetData == 0x40 &&
           *(aStyleSheetData+1) == 0x00 &&
           *(aStyleSheetData+2) == 0x63 &&
           *(aStyleSheetData+3) == 0x00) {
    // 2-byte big-endian encoding, no BOM
    NS_WARNING("Our unicode decoders aren't likely to deal with this one");
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
      return NS_ERROR_NOT_AVAILABLE;
    }
    ++index;
    pos += step;
  }

  while (pos < aDataLength && nsCRT::IsAsciiSpace(aStyleSheetData[pos])) {
    pos += step;
  }

  if (pos >= aDataLength ||
      (aStyleSheetData[pos] != '"' && aStyleSheetData[pos] != '\'')) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  char quote = aStyleSheetData[pos];
  pos += step;
  while (pos < aDataLength) {
    if (aStyleSheetData[pos] == '\\') {
      pos += step;
      if (pos >= aDataLength) {
        break;
      }          
    } else if (aStyleSheetData[pos] == quote) {
      break;
    }
    
    aCharset.Append(PRUnichar(aStyleSheetData[pos]));
    pos += step;
  }

  // Check for the ending ';'
  pos += step;
  while (pos < aDataLength && nsCRT::IsAsciiSpace(aStyleSheetData[pos])) {
    pos += step;    
  }

  if (pos >= aDataLength || aStyleSheetData[pos] != ';') {
    aCharset.Truncate();
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

NS_IMETHODIMP
SheetLoadData::OnDetermineCharset(nsIUnicharStreamLoader* aLoader,
                                  nsISupports* aContext,
                                  const char* aData,
                                  PRUint32 aDataLength,
                                  nsACString& aCharset)
{
  nsCOMPtr<nsIChannel> channel;
  nsresult result = aLoader->GetChannel(getter_AddRefs(channel));
  if (NS_FAILED(result))
    channel = nsnull;

  /*
   * First determine the charset (if one is indicated)
   * 1)  Check nsIChannel::contentCharset
   * 2)  Check @charset rules in the data
   * 3)  Check "charset" attribute of the <LINK> or <?xml-stylesheet?>
   *
   * If all these fail to give us a charset, fall back on our
   * default (document charset or ISO-8859-1 if we have no document
   * charset)
   */
  nsAutoString charset;
  nsAutoString charsetCandidate;
  if (channel) {
    nsCAutoString charsetVal;
    channel->GetContentCharset(charsetVal);
    CopyASCIItoUCS2(charsetVal, charsetCandidate);
  }

  result = NS_ERROR_NOT_AVAILABLE;
  if (! charsetCandidate.IsEmpty()) {
#ifdef DEBUG_bzbarsky
    fprintf(stderr, "Setting from HTTP to: %s\n", NS_ConvertUCS2toUTF8(charsetCandidate).get());
#endif
    result = ResolveCharset(charsetCandidate, charset);
  }

  if (NS_FAILED(result)) {
    //  We have no charset or the HTTP charset is not recognized.
    //  Try @charset rule
    result = GetCharsetFromData((const unsigned char*)aData,
                                aDataLength, charsetCandidate);
    if (NS_SUCCEEDED(result)) {
#ifdef DEBUG_bzbarsky
      fprintf(stderr, "Setting from @charset rule: %s\n",
              NS_ConvertUCS2toUTF8(charsetCandidate).get());
#endif
      result = ResolveCharset(charsetCandidate, charset);
    }
  }

  if (NS_FAILED(result)) {
    // Now try the charset on the <link> or processing instruction
    // that loaded us
    if (mOwningElement) {
      mOwningElement->GetCharset(charsetCandidate);
      if (! charsetCandidate.IsEmpty()) {
#ifdef DEBUG_bzbarsky
        fprintf(stderr, "Setting from property on element: %s\n",
                NS_ConvertUCS2toUTF8(charsetCandidate).get());
#endif
        result = ResolveCharset(charsetCandidate, charset);
      }
    }
  }

  if (NS_FAILED(result) && mLoader->mDocument) {
    // no useful data on charset.  Try the document charset.
    // That needs no resolution, since it's already fully resolved
    mLoader->mDocument->GetDocumentCharacterSet(charset);
#ifdef DEBUG_bzbarsky
    fprintf(stderr, "Set from document: %s\n",
            NS_ConvertUCS2toUTF8(charset).get());
#endif
  }      

  if (charset.IsEmpty()) {
    NS_WARNING("Unable to determine charset for sheet, using ISO-8859-1!");
    charset = NS_LITERAL_STRING("ISO-8859-1");
  }
  
  aCharset = NS_ConvertUCS2toUTF8(charset);
  return NS_OK;
}

/**
 * Report an error to the error console.
 *  @param aErrorName     The name of a string in css.properties.
 *  @param aParams        The parameters for that string in css.properties.
 *  @param aParamsLength  The length of aParams.
 *  @param aErrorFlags    Error/warning flag to pass to nsIScriptError::Init.
 *
 * XXX This should be a static method on a class called something like
 * nsCSSUtils, since it's a general way of accessing css.properties and
 * will be useful for localizability work on CSS parser error reporting.
 * However, it would need some way of reporting source file name, text,
 * line, and column information.
 */
static nsresult
ReportToConsole(const PRUnichar* aMessageName, const PRUnichar **aParams, 
                PRUint32 aParamsLength, PRUint32 aErrorFlags)
{
  nsresult rv;
  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIStringBundleService> stringBundleService =
    do_GetService(kCStringBundleServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIStringBundle> bundle;
  rv = stringBundleService->CreateBundle(
           "chrome://global/locale/css.properties", getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLString errorText;
  rv = bundle->FormatStringFromName(aMessageName, aParams, aParamsLength,
                                    getter_Copies(errorText));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = errorObject->Init(errorText.get(),
                         NS_LITERAL_STRING("").get(), /* file name */
                         NS_LITERAL_STRING("").get(), /* source line */
                         0, /* line number */
                         0, /* column number */
                         aErrorFlags,
                         "CSS Loader");
  NS_ENSURE_SUCCESS(rv, rv);
  consoleService->LogMessage(errorObject);

  return NS_OK;
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
  
  if (!mLoader->mDocument && !mIsAgent) {
    // Sorry, we don't care about this load anymore
    LOG_WARN(("  No document and not agent sheet; dropping load"));
    mLoader->SheetComplete(this, PR_FALSE);
    return NS_OK;
  }
  
  nsCOMPtr<nsIChannel> channel;
  nsresult result = aLoader->GetChannel(getter_AddRefs(channel));
  if (NS_FAILED(result))
    channel = nsnull;
  
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
      mLoader->SheetComplete(this, PR_FALSE);
      return NS_OK;
    }
  }

  if (aDataStream) {
    nsCAutoString contentType;
    if (channel) {
      channel->GetContentType(contentType);
    }
    
    PRBool validType = contentType.Equals(NS_LITERAL_CSTRING("text/css")) ||
      contentType.Equals(NS_LITERAL_CSTRING(UNKNOWN_CONTENT_TYPE)) ||
      contentType.IsEmpty();
                                          
    if (mLoader->mCompatMode == eCompatibility_NavQuirks || validType) {
      if (!validType) {
        nsCAutoString spec;
        if (channel) {
          nsCOMPtr<nsIURI> uri;
          channel->GetURI(getter_AddRefs(uri));
          if (uri)
            uri->GetSpec(spec);
        }

        const nsAFlatString& specUCS2 = NS_ConvertUTF8toUCS2(spec);
        const nsAFlatString& ctypeUCS2 = NS_ConvertASCIItoUCS2(contentType);
        const PRUnichar *strings[] = { specUCS2.get(), ctypeUCS2.get() };

        ReportToConsole(NS_LITERAL_STRING("MimeNotCssWarn").get(), strings, 2,
                        nsIScriptError::warningFlag);
      }
    } else {
      // Drop the data stream so that we do not load it
      aDataStream = nsnull;
      
      nsCAutoString spec;
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri)
          uri->GetSpec(spec);
      }

      const nsAFlatString& specUCS2 = NS_ConvertUTF8toUCS2(spec);
      const nsAFlatString& ctypeUCS2 = NS_ConvertASCIItoUCS2(contentType);
      const PRUnichar *strings[] = { specUCS2.get(), ctypeUCS2.get() };

      ReportToConsole(NS_LITERAL_STRING("MimeNotCss").get(), strings, 2,
                      nsIScriptError::errorFlag);
    }
  }
  
  if (NS_FAILED(aStatus) || !aDataStream) {
    LOG_WARN(("  Load failed: status %d, data stream %p",
              aStatus, aDataStream));
    mLoader->SheetComplete(this, PR_FALSE);
    return NS_OK;
  }
    
  PRBool completed;
  return mLoader->ParseSheet(aDataStream, this, completed);
}

#ifdef INCLUDE_XUL
static PRBool IsChromeURI(nsIURI* aURI)
{
  NS_ASSERTION(aURI, "Have to pass in a URI");
  PRBool isChrome = PR_FALSE;
  aURI->SchemeIs("chrome", &isChrome);
  return isChrome;
}
#endif

typedef nsresult (*nsStringEnumFunc)(const nsAString& aSubString, void *aData);

static nsresult EnumerateMediaString(const nsAString& aStringList, nsStringEnumFunc aFunc, void* aData)
{
  nsresult status = NS_OK;

  nsAutoString  stringList(aStringList); // copy to work buffer
  nsAutoString  subStr;

  stringList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)stringList.get();
  PRUnichar* end   = start;

  while (NS_SUCCEEDED(status) && (kNullCh != *start)) {
    PRBool  quoted = PR_FALSE;

    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }

    if ((kApostrophe == *start) || (kQuote == *start)) { // quoted string
      PRUnichar quote = *start++;
      quoted = PR_TRUE;
      end = start;
      while (kNullCh != *end) {
        if (quote == *end) {  // found closing quote
          *end++ = kNullCh;     // end string here
          while ((kNullCh != *end) && (kComma != *end)) { // keep going until comma
            end++;
          }
          break;
        }
        end++;
      }
    }
    else {  // non-quoted string or ended
      end = start;

      while ((kNullCh != *end) && (kComma != *end)) { // look for comma
        end++;
      }
      *end = kNullCh; // end string here
    }

    // truncate at first non letter, digit or hyphen
    PRUnichar* test = start;
    while (test <= end) {
      if ((PR_FALSE == nsCRT::IsAsciiAlpha(*test)) && 
          (PR_FALSE == nsCRT::IsAsciiDigit(*test)) && (kMinus != *test)) {
        *test = kNullCh;
        break;
      }
      test++;
    }
    subStr = start;

    if (PR_FALSE == quoted) {
      subStr.CompressWhitespace(PR_FALSE, PR_TRUE);
    }

    if (!subStr.IsEmpty()) {
      status = (*aFunc)(subStr, aData);
    }

    start = ++end;
  }

  return status;
}

static nsresult MediumEnumFunc(const nsAString& aSubString, void* aData)
{
  nsCOMPtr<nsIAtom> medium = do_GetAtom(aSubString);
  NS_ENSURE_TRUE(medium, NS_ERROR_OUT_OF_MEMORY);
  ((nsICSSStyleSheet*)aData)->AppendMedium(medium);
  return NS_OK;
}

PRBool
CSSLoaderImpl::IsAlternate(const nsAString& aTitle)
{
  if (!aTitle.IsEmpty()) {
    return PRBool(! aTitle.Equals(mPreferredSheet, nsCaseInsensitiveStringComparator()));
  }
  return PR_FALSE;
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
                           PRUint32 aDefaultNameSpaceID,
                           StyleSheetState& aSheetState,
                           nsICSSStyleSheet** aSheet)
{
  LOG(("CSSLoaderImpl::CreateSheet"));
  NS_PRECONDITION(aSheet, "Null out param!");
  
  nsresult rv = NS_OK;
  *aSheet = nsnull;
  aSheetState = eSheetStateUnknown;
  
  if (aURI) {
    // First, complete sheets
    aSheetState = eSheetComplete;

    nsCOMPtr<nsICSSStyleSheet> sheet;
    URLKey key(aURI);
    sheet = dont_AddRef(NS_STATIC_CAST(nsICSSStyleSheet*,
                                       mCompleteSheets.Get(&key)));
    LOG(("  From completed: %p", sheet.get()));

    // Then the XUL cache
#ifdef INCLUDE_XUL
    if (!sheet && IsChromeURI(aURI)) {
      nsCOMPtr<nsIXULPrototypeCache> cache(do_GetService("@mozilla.org/xul/xul-prototype-cache;1"));
      if (cache) {
        PRBool enabled;
        cache->GetEnabled(&enabled);
        if (enabled) {
          cache->GetStyleSheet(aURI, getter_AddRefs(sheet));
        }
      }
    }
    LOG(("  From XUL cache: %p", sheet.get()));
#endif

    // Then loading sheets
    if (!sheet) {
      aSheetState = eSheetLoading;
      SheetLoadData* loadData =
        NS_STATIC_CAST(SheetLoadData*, mLoadingDatas.Get(&key));
      if (loadData) {
        sheet = loadData->mSheet;
      }
    }
    LOG(("  From loading: %p", sheet.get()));

    // Then alternate sheets
    if (!sheet) {
      aSheetState = eSheetPending;
      SheetLoadData* loadData =
        NS_STATIC_CAST(SheetLoadData*, mPendingDatas.Get(&key));
      if (loadData) {
        sheet = loadData->mSheet;
      }
    }
    LOG(("  From pending: %p", sheet.get()));

    if (sheet) {
      // We can use this cached sheet if it's either incomplete or unmodified
      PRBool modified = PR_TRUE;
      sheet->IsModified(&modified);
      PRBool complete = PR_FALSE;
      sheet->GetComplete(complete);
      if (!modified || !complete) {
        // Proceed on failures; at worst we'll try to create one below
        sheet->Clone(*aSheet);
        NS_ASSERTION(complete || aSheetState != eSheetComplete,
                     "Sheet thinks it's not complete while we think it is");
      }
    }
  }

  if (!*aSheet) {
    aSheetState = eSheetNeedsParser;
    nsCOMPtr<nsIURI> sheetURI = aURI;
    // Need to create one now
    if (!sheetURI) {
      // Inline style.  Use the document's base URL so that @import in
      // the inline sheet picks up the right base.
      mDocument->GetBaseURL(*getter_AddRefs(sheetURI));
    }

    rv = NS_NewCSSStyleSheet(aSheet, sheetURI);
    NS_ENSURE_SUCCESS(rv, rv);
    
    (*aSheet)->SetDefaultNameSpaceID(aDefaultNameSpaceID);
  }

  NS_ASSERTION(*aSheet, "We should have a sheet by now!");
  NS_ASSERTION(aSheetState != eSheetStateUnknown, "Have to set a state!");
  LOG(("  State: %s", gStateStrings[aSheetState]));
  
  return NS_OK;
}

/**
 * PrepareSheet() handles setting the media and title on the sheet, as
 * well as setting the enabled state based on the title
 */
nsresult
CSSLoaderImpl::PrepareSheet(nsICSSStyleSheet* aSheet,
                            const nsAString& aTitle,
                            const nsAString& aMedia)
{
  NS_PRECONDITION(aSheet, "Must have a sheet!");

  nsresult rv = NS_OK;
  aSheet->ClearMedia();
  if (!aMedia.IsEmpty()) {
    rv = EnumerateMediaString(aMedia, MediumEnumFunc, aSheet);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  aSheet->SetTitle(aTitle);
  aSheet->SetEnabled(! IsAlternate(aTitle));
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
                                nsIContent* aLinkingElement,
                                nsIDocument* aDocument)
{
  LOG(("CSSLoaderImpl::InsertSheetInDoc"));
  NS_PRECONDITION(aSheet, "Nothing to insert");
  NS_PRECONDITION(aDocument, "Must have a document to insert into");

  // all nodes that link in sheets should be implementing nsIDOM3Node
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> linkingNode = do_QueryInterface(aLinkingElement);
  NS_ASSERTION(linkingNode || !aLinkingElement,
               "Need to implement nsIDOM3Node to get insertion order right");

  // XXX Need to cancel pending sheet loads for this element, if any

  PRInt32 sheetCount;
  aDocument->GetNumberOfStyleSheets(PR_FALSE, &sheetCount);

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
    nsCOMPtr<nsIStyleSheet> curSheet;
    aDocument->GetStyleSheetAt(insertionPoint, PR_FALSE,
                               getter_AddRefs(curSheet));
    NS_ASSERTION(curSheet, "There must be a sheet here!");
    nsCOMPtr<nsIDOMStyleSheet> domSheet = do_QueryInterface(curSheet);
    NS_ASSERTION(domSheet, "All the \"normal\" sheets implement nsIDOMStyleSheet");
    nsCOMPtr<nsIDOMNode> sheetOwner;
    domSheet->GetOwnerNode(getter_AddRefs(sheetOwner));
    if (sheetOwner && !linkingNode) {
      // Keep moving; all sheets with a sheetOwner come after all
      // sheets without a linkingNode 
      continue;
    }

    if (!sheetOwner) {
      // Aha!  The current sheet has no sheet owner, so we want to
      // insert after it no matter whether we have a linkingNode
      break;
    }

    // Have to compare
    PRUint16 comparisonFlags = 0;
    rv = linkingNode->CompareDocumentPosition(sheetOwner, &comparisonFlags);
    // If we can't get the order right, just bail...
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(!(comparisonFlags & nsIDOMNode::DOCUMENT_POSITION_DISCONNECTED),
                 "Why are these elements in different documents?");
#ifdef DEBUG
    {
      PRBool sameNode = PR_FALSE;
      linkingNode->IsSameNode(sheetOwner, &sameNode);
      NS_ASSERTION(!sameNode, "Why do we still have our old sheet?");
    }
#endif // DEBUG
    if (comparisonFlags & nsIDOMNode::DOCUMENT_POSITION_PRECEDING) {
      // The current sheet comes before us, and it better be the first
      // such, because now we break
      break;
    }
  }

  ++insertionPoint; // adjust the index to the spot we want to insert in
  
  // XXX <meta> elements do not implement nsIStyleSheetLinkingElement;
  // need to fix this for them to be ordered correctly.
  nsCOMPtr<nsIStyleSheetLinkingElement>
    linkingElement = do_QueryInterface(aLinkingElement);
  if (linkingElement) {
    linkingElement->SetStyleSheet(aSheet); // This sets the ownerNode on the sheet
  }

  aDocument->InsertStyleSheetAt(aSheet, insertionPoint);
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

  LOG_URI("  Load from: '%s'", aLoadData->mURI);
  
  nsresult rv = NS_OK;  

  if (!mDocument && !aLoadData->mIsAgent) {
    // No point starting the load; just release all the data and such.
    LOG_WARN(("  No document and not agent sheet; pre-dropping load"));
    SheetComplete(aLoadData, PR_FALSE);
    return NS_OK;
  }

  // We have to clone the URI because we're using it as the hash key and necko
  // will munge it.  For example, "chrome://communicator/skin/" will become
  // "chrome://communicator/skin/communicator.css" if you open a channel with
  // it..  :(
  nsCOMPtr<nsIURI> uriClone;
  rv = aLoadData->mURI->Clone(getter_AddRefs(uriClone));
  if (!uriClone) {
    LOG_ERROR(("  Failed to clone URI"));
    SheetComplete(aLoadData, PR_FALSE);
    return rv;
  }
  
  if (aLoadData->mSyncLoad) {
    LOG(("  Synchronous load"));
    // Just load it; if we aren't sharing it by now it's too late.  We could
    // look at the deferred load list, but the chances of anything being
    // loaded as both an alternate and a UA sheet are slim, so don't bother.
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_OpenURI(getter_AddRefs(stream), uriClone);
    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to open URI synchronously"));
      SheetComplete(aLoadData, PR_FALSE);
      return rv;
    }

    nsCOMPtr<nsIConverterInputStream> converterStream = 
      do_CreateInstance("@mozilla.org/intl/converter-input-stream;1", &rv);
    
    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to create converter stream"));
      SheetComplete(aLoadData, PR_FALSE);
      return rv;
    }

    // This forces UA sheets to be UTF-8.  We should really look for
    // @charset rules here via ReadSegments on the raw stream...

    // 8192 is a nice magic number that happens to be what a lot of
    // other things use for buffer sizes.
    rv = converterStream->Init(stream, NS_LITERAL_STRING("UTF-8").get(),
                               8192, PR_TRUE);
    
    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to initialize converter stream"));
      SheetComplete(aLoadData, PR_FALSE);
      return rv;
    }

    PRBool completed;
    rv = ParseSheet(converterStream, aLoadData, completed);
    NS_ASSERTION(completed, "sync load did not complete");
    return rv;
  }

  URLKey key(aLoadData->mURI);
  SheetLoadData* existingData = nsnull;

  if (aSheetState == eSheetLoading) {
    existingData = NS_STATIC_CAST(SheetLoadData*, mLoadingDatas.Get(&key));
    NS_ASSERTION(existingData, "CreateSheet lied about the state");
  }
  else if (aSheetState == eSheetPending){
    existingData = NS_STATIC_CAST(SheetLoadData*, mPendingDatas.Get(&key));
    NS_ASSERTION(existingData, "CreateSheet lied about the state");
  }
  
  if (existingData) {
    LOG(("  Glomming on to existing load"));
    SheetLoadData* data = existingData;
    while (data->mNext) {
      data = data->mNext;
    }
    data->mNext = aLoadData; // transfer ownership
    if (aSheetState == eSheetPending && !IsAlternate(aLoadData->mTitle)) {
      // Kick the load off; someone cares about it right away
#ifdef DEBUG
      SheetLoadData* removedData =
#endif
        NS_STATIC_CAST(SheetLoadData*, mPendingDatas.Remove(&key));
      NS_ASSERTION(removedData == existingData, "Broken pending table");

      LOG(("  Forcing load of pending data"));
      LoadSheet(existingData, eSheetNeedsParser);
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
    mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
    NS_ASSERTION(loadGroup,
                 "No loadgroup for stylesheet; onload will fire early");
  }

#ifdef MOZ_TIMELINE
  NS_TIMELINE_MARK_URI("Loading style sheet: %s", uriClone);
  NS_TIMELINE_INDENT();
#endif
  
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     uriClone, nsnull, loadGroup,
                     nsnull, nsIChannel::LOAD_NORMAL);
  
  if (NS_FAILED(rv)) {
    LOG_ERROR(("  Failed to create channel"));
    SheetComplete(aLoadData, PR_FALSE);
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    // send a minimal Accept header for text/css
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                  NS_LITERAL_CSTRING("text/css,*/*;q=0.1"),
                                  PR_FALSE);
    if (mDocument) {
      nsCOMPtr<nsIURI> documentURI;
      mDocument->GetDocumentURL(getter_AddRefs(documentURI));
      NS_ASSERTION(documentURI, "Null document uri is bad!");
      if (documentURI) {
        httpChannel->SetReferrer(documentURI);
      }
    }
  }

  // We don't have to hold on to the stream loader.  The ownership
  // model is: Necko owns the stream loader, which owns the load data,
  // which owns us
  nsCOMPtr<nsIUnicharStreamLoader> streamLoader;
  rv = NS_NewUnicharStreamLoader(getter_AddRefs(streamLoader),
                                 channel, aLoadData);

#ifdef DEBUG
  mSyncCallback = PR_FALSE;
#endif

  if (NS_FAILED(rv)) {
    LOG_ERROR(("  Failed to create stream loader"));
    SheetComplete(aLoadData, PR_FALSE);
    return rv;
  }

  mLoadingDatas.Put(&key, aLoadData);
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
  
  nsresult rv;
  nsCOMPtr<nsICSSParser> parser;
  rv = GetParserFor(aLoadData->mSheet, getter_AddRefs(parser));
  if (NS_FAILED(rv)) {
    LOG_ERROR(("  Failed to get CSS parser"));
    SheetComplete(aLoadData, PR_FALSE);
    return rv;
  }

  // The parser insists on passing out a strong ref to the sheet it
  // parsed.  We don't care.
  nsCOMPtr<nsICSSStyleSheet> dummySheet;
  // Push our load data on the stack so any kids can pick it up
  mParsingDatas.AppendElement(aLoadData);
  nsCOMPtr<nsIURI> uri = aLoadData->mURI;
  if (!uri) {
    aLoadData->mSheet->GetURL(*getter_AddRefs(uri));
  }
  rv = parser->Parse(aStream, uri, *getter_AddRefs(dummySheet));
  mParsingDatas.RemoveElementAt(mParsingDatas.Count() - 1);
  RecycleParser(parser);

  NS_ASSERTION(aLoadData->mPendingChildren >= 0,
               "Negatively many kids?");
  
  if (aLoadData->mPendingChildren == 0) {
    LOG(("  No pending kids from parse"));
    aCompleted = PR_TRUE;
    if (!aLoadData->mURI) {
      // inline sheet and we're all done with no kids, so we will not
      // be blocking the parser
      LOG(("  Inline sheet with no pending kids; nulling out parser"));
      aLoadData->mParserToUnblock = nsnull;
    }
    SheetComplete(aLoadData, PR_TRUE);
  }
  // Otherwise, the children are holding strong refs to the data and
  // will call SheetComplete() on it when they complete.
  
  return NS_OK;
}

/**
 * SheetComplete is the do-it-all cleanup function.  It removes the
 * load data from the "loading" hashtable, adds the sheet to the
 * "completed" hashtable, massages the XUL cache, handles siblings of
 * the load data (other loads for the same URI, handles unblocking
 * blocked parent loads as needed, and most importantly calls
 * NS_RELEASE on the load data to destroy the whole mess.
 */
void
CSSLoaderImpl::SheetComplete(SheetLoadData* aLoadData, PRBool aSucceeded)
{
  LOG(("CSSLoaderImpl::SheetComplete"));
  NS_PRECONDITION(aLoadData, "Must have a load data!");
  NS_PRECONDITION(aLoadData->mSheet, "Must have a sheet");

  LOG(("Load completed: %d", aSucceeded));

  // Twiddle the hashtables
  if (aLoadData->mURI) {
    LOG_URI("  Finished loading: '%s'", aLoadData->mURI);
    // Remove the data from the list of loading datas
    URLKey key(aLoadData->mURI);
    if (aLoadData->mIsLoading) {
      
#ifdef DEBUG
      SheetLoadData* loadingData =
#endif
        NS_STATIC_CAST(SheetLoadData*, mLoadingDatas.Remove(&key));
      NS_ASSERTION(loadingData == aLoadData, "Broken loading table");
      aLoadData->mIsLoading = PR_FALSE;
    }
    
    if (aSucceeded) {
      mCompleteSheets.Put(&key, aLoadData->mSheet);
    }
  }
  

#ifdef DEBUG
  PRBool seenParser = PR_FALSE;
#endif
  // Go through and deal with the whole linked list.
  SheetLoadData* data = aLoadData;
  while (data) {

    data->mSheet->SetModified(PR_FALSE); // it's clean
    data->mSheet->SetComplete();
    if (data->mObserver) {
      data->mObserver->StyleSheetLoaded(data->mSheet, PR_TRUE);
    }
                           
    if (data->mParserToUnblock) {
#ifdef DEBUG
      NS_ASSERTION(!seenParser, "Should not have multiple parsers involved!");
      seenParser = PR_TRUE;
#endif
      data->mParserToUnblock->ContinueParsing();
      data->mParserToUnblock = nsnull; // drop the ref, just in case
    }

    if (data->mParentData &&
        --(data->mParentData->mPendingChildren) == 0 &&
        !data->mSyncLoad) {
      // We finished the parent too
      SheetComplete(data->mParentData, aSucceeded);
    }
    
    data = data->mNext;
  }

#ifdef INCLUDE_XUL
  // Now that it's marked complete, put the sheet in our cache
  if (aSucceeded && aLoadData->mURI && IsChromeURI(aLoadData->mURI)) {
    nsCOMPtr<nsIXULPrototypeCache> cache(do_GetService("@mozilla.org/xul/xul-prototype-cache;1"));
    if (cache) {
      PRBool enabled;
      cache->GetEnabled(&enabled);
      if (enabled) {
        nsCOMPtr<nsICSSStyleSheet> sheet;
        cache->GetStyleSheet(aLoadData->mURI, getter_AddRefs(sheet));
        if (!sheet) {
          cache->PutStyleSheet(aLoadData->mSheet);
        }
      }
    }
  }
#endif

  NS_RELEASE(aLoadData);  // this will release parents and siblings and all that
  if (mLoadingDatas.Count() == 0 && mPendingDatas.Count() > 0) {
    LOG(("  No more loading sheets; starting alternates"));
    mPendingDatas.Enumerate(StartAlternateLoads, this);
  }
}

NS_IMETHODIMP
CSSLoaderImpl::LoadInlineStyle(nsIContent* aElement,
                               nsIUnicharInputStream* aStream, 
                               const nsAString& aTitle, 
                               const nsAString& aMedia, 
                               PRInt32 aDefaultNameSpaceID,
                               nsIParser* aParserToUnblock,
                               PRBool& aCompleted,
                               nsICSSLoaderObserver* aObserver)
{
  LOG(("CSSLoaderImpl::LoadInlineStyle"));
  NS_PRECONDITION(aStream, "Must have a stream to parse!");
  NS_ASSERTION(mParsingDatas.Count() == 0, "We're in the middle of a parse?");

  aCompleted = PR_TRUE;

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIStyleSheetLinkingElement> owningElement(do_QueryInterface(aElement));
  NS_ASSERTION(owningElement, "Element is not a style linking element!");
  
  StyleSheetState state;
  nsCOMPtr<nsICSSStyleSheet> sheet;
  nsresult rv = CreateSheet(nsnull, aDefaultNameSpaceID, state,
                            getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(state == eSheetNeedsParser,
               "Inline sheets should not be cached");

  rv = PrepareSheet(sheet, aTitle, aMedia);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = InsertSheetInDoc(sheet, aElement, mDocument);
  NS_ENSURE_SUCCESS(rv, rv);
  
  SheetLoadData* data = new SheetLoadData(this, aTitle, aParserToUnblock,
                                          nsnull, sheet, owningElement,
                                          aObserver);

  if (!data) {
    sheet->SetComplete();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(data);
  // Parse completion releases the load data
  return ParseSheet(aStream, data, aCompleted);
}        

NS_IMETHODIMP
CSSLoaderImpl::LoadStyleLink(nsIContent* aElement,
                             nsIURI* aURL, 
                             const nsAString& aTitle, 
                             const nsAString& aMedia, 
                             PRInt32 aDefaultNameSpaceID,
                             nsIParser* aParserToUnblock,
                             PRBool& aCompleted,
                             nsICSSLoaderObserver* aObserver)
{
  LOG(("CSSLoaderImpl::LoadStyleLink"));
  NS_PRECONDITION(aURL, "Must have URL to load");
  NS_ASSERTION(mParsingDatas.Count() == 0, "We're in the middle of a parse?");

  LOG_URI("  Link uri: '%s'", aURL);
  
  aCompleted = PR_TRUE;

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_INITIALIZED);

  //-- Make sure this page is allowed to load this URL
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> secMan = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIURI> docURI;
  rv = mDocument->GetDocumentURL(getter_AddRefs(docURI));
  if (NS_FAILED(rv) || !docURI) return NS_ERROR_FAILURE;
  rv = secMan->CheckLoadURI(docURI, aURL, nsIScriptSecurityManager::ALLOW_CHROME);
  if (NS_FAILED(rv)) return rv;

  LOG(("  Passed security check"));
  
  StyleSheetState state;
  nsCOMPtr<nsICSSStyleSheet> sheet;
  rv = CreateSheet(aURL, aDefaultNameSpaceID, state, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = PrepareSheet(sheet, aTitle, aMedia);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = InsertSheetInDoc(sheet, aElement, mDocument);
  NS_ENSURE_SUCCESS(rv, rv);

  if (state == eSheetComplete) {
    LOG(("  Sheet already complete"));
    // We're completely done; this sheet is fully loaded, clean, and so forth
    if (aObserver) {
      aObserver->StyleSheetLoaded(sheet, PR_TRUE);
    }
    return NS_OK;
  }

  nsCOMPtr<nsIStyleSheetLinkingElement> owningElement(do_QueryInterface(aElement));

  // Now we need to actually load it
  SheetLoadData* data = new SheetLoadData(this, aTitle, aParserToUnblock, aURL,
                                          sheet, owningElement, aObserver);
  if (!data) {
    sheet->SetComplete();
    if (aObserver) {
      aObserver->StyleSheetLoaded(sheet, PR_TRUE);
    }
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(data);

  // Caller gets to wait for the sheet to load.
  aCompleted = PR_FALSE;

  // If we have to parse and it's an alternate non-inline, defer it
  if (aURL && state == eSheetNeedsParser && mLoadingDatas.Count() != 0 &&
      IsAlternate(aTitle)) {
    LOG(("  Deferring alternate sheet load"));
    URLKey key(aURL);
    mPendingDatas.Put(&key, data);
    return NS_OK;
  }

  // Load completion will free the data
  return LoadSheet(data, state);
}

NS_IMETHODIMP
CSSLoaderImpl::LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                              nsIURI* aURL, 
                              const nsAString& aMedia,
                              PRInt32 aDefaultNameSpaceID,
                              nsICSSImportRule* aParentRule)
{
  LOG(("CSSLoaderImpl::LoadChildSheet"));
  NS_PRECONDITION(aURL, "Must have a URI to load");

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  LOG_URI("  Child uri: '%s'", aURL);

  //-- Make sure this page is allowed to load this URL
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> secMan = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIURI> sheetURI;
  rv = aParentSheet->GetURL(*getter_AddRefs(sheetURI));
  if (NS_FAILED(rv) || !sheetURI) return NS_ERROR_FAILURE;
  rv = secMan->CheckLoadURI(sheetURI, aURL, nsIScriptSecurityManager::ALLOW_CHROME);
  if (NS_FAILED(rv)) return rv;

  LOG(("  Passed security check"));
  
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

    NS_ASSERTION(parentData->mSheet = aParentSheet,
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
  rv = CreateSheet(aURL, aDefaultNameSpaceID, state, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = PrepareSheet(sheet, NS_LITERAL_STRING(""), aMedia);
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

  // Load completion will release the data
  return LoadSheet(data, state);
  
}

NS_IMETHODIMP
CSSLoaderImpl::LoadAgentSheet(nsIURI* aURL, 
                              nsICSSStyleSheet** aSheet)
{
  LOG(("CSSLoaderImpl::LoadAgentSheet synchronous"));
  return InternalLoadAgentSheet(aURL, aSheet, nsnull);
}

NS_IMETHODIMP
CSSLoaderImpl::LoadAgentSheet(nsIURI* aURL, 
                              nsICSSLoaderObserver* aObserver)
{
  LOG(("CSSLoaderImpl::LoadAgentSheet asynchronous"));
  return InternalLoadAgentSheet(aURL, nsnull, aObserver);
}

nsresult
CSSLoaderImpl::InternalLoadAgentSheet(nsIURI* aURL, 
                                      nsICSSStyleSheet** aSheet,
                                      nsICSSLoaderObserver* aObserver)
{
  NS_PRECONDITION(aURL, "Must have a URI to load");
  NS_PRECONDITION((!aSheet || !aObserver) && (aSheet || aObserver),
                  "One or the other please, at most one");
  NS_ASSERTION(mParsingDatas.Count() == 0, "We're in the middle of a parse?");

  LOG_URI("  Agent uri: '%s'", aURL);
  
  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  StyleSheetState state;
  nsCOMPtr<nsICSSStyleSheet> sheet;
  nsresult rv = CreateSheet(aURL, kNameSpaceID_Unknown, state,
                            getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = PrepareSheet(sheet, NS_LITERAL_STRING(""), NS_LITERAL_STRING(""));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (aSheet) {
    *aSheet = nsnull;
  }
  
  if (state == eSheetComplete) {
    LOG(("  Sheet already complete"));
    if (aSheet) {
      *aSheet = sheet;
      NS_ADDREF(*aSheet);
    } else {
      aObserver->StyleSheetLoaded(sheet, PR_TRUE);
    }
    return NS_OK;
  }

  SheetLoadData* data = new SheetLoadData(this, aURL, sheet, aObserver);

  if (!data) {
    sheet->SetComplete();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ADDREF(data);
  rv = LoadSheet(data, state);

  if (NS_SUCCEEDED(rv) && aSheet) {
    *aSheet = sheet;
    NS_ADDREF(*aSheet);
  }

  return rv;
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

PR_STATIC_CALLBACK(PRIntn)
StopLoadingSheetCallback(nsHashKey* aKey, void* aData, void* aClosure)
{
  NS_PRECONDITION(aData, "Must have a data!");
  NS_PRECONDITION(aClosure, "Must have a loader");

  SheetLoadData* data = NS_STATIC_CAST(SheetLoadData*, aData);
  CSSLoaderImpl* loader = NS_STATIC_CAST(CSSLoaderImpl*, aClosure);

  data->mIsLoading = PR_FALSE; // we will handle the removal right here
  data->mIsCancelled = PR_TRUE;
  
  loader->SheetComplete(data, PR_FALSE);

  return kHashEnumerateRemove;
}

NS_IMETHODIMP
CSSLoaderImpl::Stop()
{
  if (mLoadingDatas.Count() > 0) {
    mLoadingDatas.Enumerate(StopLoadingSheetCallback, this);
  }
  if (mPendingDatas.Count() > 0) {
    mPendingDatas.Enumerate(StopLoadingSheetCallback, this);
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::StopLoadingSheet(nsIURI* aURL)
{
  NS_ENSURE_TRUE(aURL, NS_ERROR_NULL_POINTER);
  if (mLoadingDatas.Count() > 0 || mPendingDatas.Count() > 0) {
    URLKey key(aURL);
    SheetLoadData* loadData =
      NS_STATIC_CAST(SheetLoadData*, mLoadingDatas.Get(&key));
    if (!loadData) {
      loadData = NS_STATIC_CAST(SheetLoadData*, mPendingDatas.Get(&key));
      if (loadData) {
        // have to remove from mPendingDatas ourselves, since
        // SheetComplete won't do that.
        mPendingDatas.Remove(&key);
      }
    }
    
    if (loadData) {
      loadData->mIsCancelled = PR_TRUE;
      SheetComplete(loadData, PR_FALSE);
    }
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
