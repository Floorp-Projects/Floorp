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
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsICharsetAlias.h"
#include "nsUnicharUtils.h"
#include "nsHashtable.h"
#include "nsIURL.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsIScriptSecurityManager.h"
#include "nsITimelineService.h"
#include "nsIHttpChannel.h"
#include "nsHTMLAtoms.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"

#ifdef INCLUDE_XUL
#include "nsIXULPrototypeCache.h"
#endif

#include "nsIDOMMediaList.h"
#include "nsIDOMStyleSheet.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kCStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

class CSSLoaderImpl;

MOZ_DECL_CTOR_COUNTER(URLKey)

class URLKey: public nsHashKey {
public:
  URLKey(nsIURI* aURL)
    : mURL(aURL)
  {
    MOZ_COUNT_CTOR(URLKey);
    mHashValue = 0;

    mURL->GetSpec(mSpec);
    if (!mSpec.IsEmpty()) {
      mHashValue = nsCRT::HashCode(mSpec.get());
    }
  }

  URLKey(const URLKey& aKey)
    : mURL(aKey.mURL),
      mHashValue(aKey.mHashValue),
      mSpec(aKey.mSpec)
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
#if 0 // bug 47846
    PRBool equals = PR_FALSE;
    nsresult result = mURL->Equals(key->mURL, &equals);
    return (NS_SUCCEEDED(result) && equals);
#else
    return (nsCRT::strcasecmp(mSpec.get(), key->mSpec.get()) == 0);
#endif
  }

  virtual nsHashKey *Clone(void) const
  {
    return new URLKey(*this);
  }

  nsCOMPtr<nsIURI>  mURL;
  PRUint32          mHashValue;
  nsSharableCString mSpec;
};

class SheetLoadData : public nsIUnicharStreamLoaderObserver
{
public:
  virtual ~SheetLoadData(void);
  SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                const nsString& aTitle, const nsString& aMedia, 
                PRInt32 aDefaultNameSpaceID,
                nsIContent* aOwner, PRInt32 aDocIndex, 
                nsIParser* aParserToUnblock, PRBool aIsInline, 
                nsICSSLoaderObserver* aObserver);
  SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, const nsString& aMedia,
                PRInt32 aDefaultNameSpaceID,
                nsICSSStyleSheet* aParentSheet, PRInt32 aSheetIndex,
                nsICSSImportRule* aParentRule);
  SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                nsICSSLoaderObserver* aObserver);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUNICHARSTREAMLOADEROBSERVER

  CSSLoaderImpl*  mLoader;
  nsIURI*         mURL;
  nsString        mTitle;
  nsString        mMedia;
  PRInt32         mDefaultNameSpaceID;
  PRInt32         mSheetIndex;

  nsIContent*     mOwningElement;
  nsIParser*      mParserToUnblock;

  nsICSSStyleSheet* mParentSheet;
  nsICSSImportRule* mParentRule;

  SheetLoadData*  mNext;
  SheetLoadData*  mParentData;

  PRUint32        mPendingChildren;

  PRPackedBool    mDidBlockParser;

  PRPackedBool    mIsInline;
  PRPackedBool    mIsAgent;
  PRPackedBool    mSyncLoad;

  nsICSSLoaderObserver* mObserver;
};

NS_IMPL_ISUPPORTS1(SheetLoadData, nsIUnicharStreamLoaderObserver);

MOZ_DECL_CTOR_COUNTER(PendingSheetData)

struct PendingSheetData {
  PendingSheetData(nsICSSStyleSheet* aSheet, PRInt32 aDocIndex,
                   nsIContent* aElement, nsICSSLoaderObserver* aObserver)
    : mSheet(aSheet),
      mDocIndex(aDocIndex),
      mOwningElement(aElement),
      mNotify(PR_FALSE),
      mObserver(aObserver)
  {
    MOZ_COUNT_CTOR(PendingSheetData);
    NS_ADDREF(mSheet);
    NS_IF_ADDREF(mOwningElement);
    NS_IF_ADDREF(mObserver);
  }

  ~PendingSheetData(void)
  {
    MOZ_COUNT_DTOR(PendingSheetData);
    NS_RELEASE(mSheet);
    NS_IF_RELEASE(mOwningElement);
    NS_IF_RELEASE(mObserver);
  }

  nsICSSStyleSheet* mSheet;
  PRInt32           mDocIndex;
  nsIContent*       mOwningElement;
  PRBool            mNotify;
  nsICSSLoaderObserver* mObserver;
};

class CSSLoaderImpl : public nsICSSLoader {
public:
  CSSLoaderImpl(void);
  virtual ~CSSLoaderImpl(void);

  NS_DECL_ISUPPORTS

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
                             const nsString& aTitle, 
                             const nsString& aMedia, 
                             PRInt32 aDefaultNameSpaceID,
                             PRInt32 aIndex,
                             nsIParser* aParserToUnblock,
                             PRBool& aCompleted,
                             nsICSSLoaderObserver* aObserver);

  NS_IMETHOD LoadStyleLink(nsIContent* aElement,
                           nsIURI* aURL, 
                           const nsString& aTitle, 
                           const nsString& aMedia, 
                           PRInt32 aDefaultNameSpaceID,
                           PRInt32 aIndex,
                           nsIParser* aParserToUnblock,
                           PRBool& aCompleted,
                           nsICSSLoaderObserver* aObserver);

  NS_IMETHOD LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                            nsIURI* aURL, 
                            const nsString& aMedia,
                            PRInt32 aDefaultNameSpaceID,
                            PRInt32 aIndex,
                            nsICSSImportRule* aRule);

  NS_IMETHOD LoadAgentSheet(nsIURI* aURL, 
                            nsICSSStyleSheet*& aSheet,
                            PRBool& aCompleted,
                            nsICSSLoaderObserver* aObserver);

  // local helper methods (public for access from statics)
  void Cleanup(URLKey& aKey, SheetLoadData* aLoadData);
  nsresult SheetComplete(nsICSSStyleSheet* aSheet, SheetLoadData* aLoadData);

  nsresult ParseSheet(nsIUnicharInputStream* aIn, SheetLoadData* aLoadData,
                      PRBool& aCompleted, nsICSSStyleSheet*& aSheet);

  void DidLoadStyle(nsIUnicharStreamLoader* aLoader,
                    nsIUnicharInputStream* aStyleDataStream,
                    SheetLoadData* aLoadData,
                    nsresult aStatus);

  nsresult SetMedia(nsICSSStyleSheet* aSheet, const nsString& aMedia);

  nsresult PrepareSheet(nsICSSStyleSheet* aSheet, const nsString& aTitle, 
                        const nsString& aMedia);

  nsresult AddPendingSheet(nsICSSStyleSheet* aSheet, PRInt32 aDocIndex, 
                           nsIContent* aElement, 
                           nsICSSLoaderObserver* aObserver);

  PRBool IsAlternate(const nsString& aTitle);

  nsresult InsertSheetInDoc(nsICSSStyleSheet* aSheet, PRInt32 aDocIndex, 
                            nsIContent* aElement, PRBool aNotify,
                            nsICSSLoaderObserver* aObserver);

  nsresult InsertChildSheet(nsICSSStyleSheet* aSheet, nsICSSStyleSheet* aParentSheet, 
                            PRInt32 aIndex);

  nsresult LoadSheet(URLKey& aKey, SheetLoadData* aData);

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

  nsIDocument*      mDocument;  // the document we live for

  PRPackedBool      mCaseSensitive; // is document CSS case sensitive
  PRPackedBool      mEnabled; // is enabled to load new styles
  nsCompatibility   mCompatMode;
  nsString          mPreferredSheet;    // title of preferred sheet

  nsISupportsArray* mParsers;     // array of CSS parsers

  nsHashtable       mLoadedSheets;  // url to first sheet fully loaded for URL
  nsHashtable       mLoadingSheets; // all current loads

  // mParsingData is (almost?) always needed, so create with storage
  nsAutoVoidArray   mParsingData; // array of data for sheets currently parsing

  nsVoidArray       mPendingDocSheets;  // loaded sheet waiting for doc insertion
  nsVoidArray       mPendingAlternateSheets;  // alternates waiting for load to start

  nsHashtable       mSheetMapTable;  // map to insertion index arrays

#ifdef NS_DEBUG
  PRBool            mSyncCallback;
#endif
};

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                             const nsString& aTitle, const nsString& aMedia,
                             PRInt32 aDefaultNameSpaceID,
                             nsIContent* aOwner, PRInt32 aDocIndex, 
                             nsIParser* aParserToUnblock, PRBool aIsInline, 
                             nsICSSLoaderObserver* aObserver)
  : mLoader(aLoader),
    mURL(aURL),
    mTitle(aTitle),
    mMedia(aMedia),
    mDefaultNameSpaceID(aDefaultNameSpaceID),
    mSheetIndex(aDocIndex),
    mOwningElement(aOwner),
    mParserToUnblock(aParserToUnblock),
    mParentSheet(nsnull),
    mParentRule(nsnull),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mDidBlockParser(PR_FALSE),
    mIsInline(aIsInline),
    mIsAgent(PR_FALSE),
    mSyncLoad(PR_FALSE),
    mObserver(aObserver)
{
  NS_INIT_ISUPPORTS();
  NS_ADDREF(mLoader);
  NS_ADDREF(mURL);
  NS_IF_ADDREF(mOwningElement);
  NS_IF_ADDREF(mParserToUnblock);
  NS_IF_ADDREF(mObserver);
}

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                             const nsString& aMedia, 
                             PRInt32 aDefaultNameSpaceID,
                             nsICSSStyleSheet* aParentSheet,
                             PRInt32 aSheetIndex,
                             nsICSSImportRule* aParentRule)
  : mLoader(aLoader),
    mURL(aURL),
    mTitle(),
    mMedia(aMedia),
    mDefaultNameSpaceID(aDefaultNameSpaceID),
    mSheetIndex(aSheetIndex),
    mOwningElement(nsnull),
    mParserToUnblock(nsnull),
    mParentSheet(aParentSheet),
    mParentRule(aParentRule),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mDidBlockParser(PR_FALSE),
    mIsInline(PR_FALSE),
    mIsAgent(PR_FALSE),
    mSyncLoad(PR_FALSE),
    mObserver(nsnull)
{
  NS_INIT_ISUPPORTS();
  NS_ADDREF(mLoader);
  NS_ADDREF(mURL);
  NS_ADDREF(mParentSheet);
  NS_ADDREF(mParentRule);
}

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                             nsICSSLoaderObserver* aObserver)
  : mLoader(aLoader),
    mURL(aURL),
    mTitle(),
    mMedia(),
    mDefaultNameSpaceID(kNameSpaceID_Unknown),
    mSheetIndex(-1),
    mOwningElement(nsnull),
    mParserToUnblock(nsnull),
    mParentSheet(nsnull),
    mParentRule(nsnull),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mDidBlockParser(PR_FALSE),
    mIsInline(PR_FALSE),
    mIsAgent(PR_TRUE),
    mSyncLoad(PRBool(nsnull == aObserver)),
    mObserver(aObserver)
{
  NS_INIT_ISUPPORTS();
  NS_ADDREF(mLoader);
  NS_ADDREF(mURL);
  NS_IF_ADDREF(mObserver);
}


SheetLoadData::~SheetLoadData(void)
{
  NS_RELEASE(mLoader);
  NS_RELEASE(mURL);
  NS_IF_RELEASE(mOwningElement);
  NS_IF_RELEASE(mParserToUnblock);
  NS_IF_RELEASE(mParentSheet);
  NS_IF_RELEASE(mParentRule);
  NS_IF_RELEASE(mObserver);
  NS_IF_RELEASE(mNext);
}

CSSLoaderImpl::CSSLoaderImpl(void)
  : mDocument(nsnull), 
    mCaseSensitive(PR_FALSE),
    mEnabled(PR_TRUE), 
    mCompatMode(eCompatibility_FullStandards),
    mParsers(nsnull)
{
  NS_INIT_ISUPPORTS();
}

static PRBool PR_CALLBACK ReleaseSheet(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsICSSStyleSheet* sheet = (nsICSSStyleSheet*)aData;
  NS_RELEASE(sheet);
  return PR_TRUE;
}

static PRBool PR_CALLBACK DeleteHashLoadData(nsHashKey* aKey, void* aData, void* aClosure)
{
  SheetLoadData* data = (SheetLoadData*)aData;
  NS_RELEASE(data);
  return PR_TRUE;
}

static PRBool PR_CALLBACK DeletePendingData(void* aData, void* aClosure)
{
  PendingSheetData* data = (PendingSheetData*)aData;
  delete data;
  return PR_TRUE;
}

static PRBool PR_CALLBACK DeleteLoadData(void* aData, void* aClosure)
{
  SheetLoadData* data = (SheetLoadData*)aData;
  NS_RELEASE(data);
  return PR_TRUE;
}

static PRBool PR_CALLBACK DeleteSheetMap(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsAutoVoidArray* map = (nsAutoVoidArray*)aData;
  delete map;
  return PR_TRUE;
}

CSSLoaderImpl::~CSSLoaderImpl(void)
{
  if (mLoadingSheets.Count() > 0) {
    Stop();
  }
  NS_IF_RELEASE(mParsers);
  mLoadedSheets.Enumerate(ReleaseSheet);
  mLoadingSheets.Enumerate(DeleteHashLoadData);
  mPendingDocSheets.EnumerateForwards(DeletePendingData, nsnull);
  mPendingAlternateSheets.EnumerateForwards(DeleteLoadData, nsnull);
  mSheetMapTable.Enumerate(DeleteSheetMap);
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

NS_IMETHODIMP
CSSLoaderImpl::DropDocumentReference(void)
{
  mDocument = nsnull;
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

NS_IMETHODIMP
CSSLoaderImpl::SetPreferredSheet(const nsAString& aTitle)
{
  mPreferredSheet = aTitle;

  // start any pending alternates that aren't alternates anymore
  PRInt32 index = 0;
  while (index < mPendingAlternateSheets.Count()) { // count will change during loop
    SheetLoadData* data = (SheetLoadData*)mPendingAlternateSheets.ElementAt(index);
    if (! IsAlternate(data->mTitle)) {
      mPendingAlternateSheets.RemoveElementAt(index);
      URLKey key(data->mURL);
      LoadSheet(key, data); // this may steal pending alternates too
    }
    else {
      index++;
    }
  }
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
  if (mParsers) {
    PRUint32 count = 0;
    mParsers->Count(&count);
    if (0 < count--) {
      *aParser = (nsICSSParser*)mParsers->ElementAt(count);
      mParsers->RemoveElementAt(count);
    }
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
  nsresult result = NS_ERROR_NULL_POINTER;

  if (aParser) {
    if (! mParsers) {
      result = NS_NewISupportsArray(&mParsers);
    }
    if (mParsers) {
      result = mParsers->AppendElement(aParser);
    }
  }
  return result;
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
    nsCOMPtr<nsIStyleSheetLinkingElement>
      element(do_QueryInterface(mOwningElement));
    if (element) {
      element->GetCharset(charsetCandidate);
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

NS_IMETHODIMP
SheetLoadData::OnStreamComplete(nsIUnicharStreamLoader* aLoader,
                                nsISupports* aContext,
                                nsresult aStatus,
                                nsIUnicharInputStream* aDataStream)
{
  nsCOMPtr<nsIChannel> channel;
  nsresult result = aLoader->GetChannel(getter_AddRefs(channel));
  if (NS_FAILED(result))
    channel = nsnull;
  NS_TIMELINE_OUTDENT();
  NS_TIMELINE_MARK_CHANNEL("SheetLoadData::OnStreamComplete(%s)", channel);

  // If it's an HTTP channel, we want to make sure this is not an
  // error document we got.
  PRBool realDocument = PR_TRUE;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    PRBool requestSucceeded;
    result = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(result) && !requestSucceeded) {
      realDocument = PR_FALSE;
    }
  }

  if (realDocument && aDataStream) {
    nsCAutoString contentType;
    if (channel) {
      channel->GetContentType(contentType);
    }
    if (mLoader->mCompatMode == eCompatibility_NavQuirks ||
        contentType.Equals(NS_LITERAL_CSTRING("text/css")) ||
        contentType.IsEmpty()) {

      if (!contentType.IsEmpty() &&
          contentType != NS_LITERAL_CSTRING("text/css")) {
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
  } else {
    // Drop the data stream so that we do not load it
    aDataStream = nsnull;
  }
  
  mLoader->DidLoadStyle(aLoader, aDataStream, this, aStatus);
  // We added a reference when the loader was created. This
  // release should destroy it.
  NS_RELEASE(aLoader);
  return NS_OK;
}

static PRBool PR_CALLBACK
InsertPendingSheet(void* aPendingData, void* aLoader)
{
  PendingSheetData* data = (PendingSheetData*)aPendingData;
  CSSLoaderImpl*  loader = (CSSLoaderImpl*)aLoader;
  loader->InsertSheetInDoc(data->mSheet, data->mDocIndex, 
                           data->mOwningElement, data->mNotify,
                           data->mObserver);
  delete data;
  return PR_TRUE;
}

static PRBool PR_CALLBACK
AreAllPendingAlternateSheets(void* aPendingData, void* aLoader)
{
  PendingSheetData* data = (PendingSheetData*)aPendingData;
  CSSLoaderImpl*  loader = (CSSLoaderImpl*)aLoader;
  nsAutoString  title;
  data->mSheet->GetTitle(title);
  if (loader->IsAlternate(title)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
CSSLoaderImpl::Cleanup(URLKey& aKey, SheetLoadData* aLoadData)
{
  // notify any parents that child is done
  SheetLoadData* data = aLoadData;
  do {
    if (data->mParentData) {
      if (0 == --(data->mParentData->mPendingChildren)) {  // all children are done, handle parent
        NS_ASSERTION(data->mParentSheet, "Parent load data exists, but parent sheet does not");
        if (! data->mSyncLoad) {
          SheetComplete(data->mParentSheet, data->mParentData);
        }
      }
    }
    data = data->mNext;
  } while (data);
    
  if (! aLoadData->mIsInline) { // inline sheets don't go in loading table
    SheetLoadData* oldData = (SheetLoadData*)mLoadingSheets.Remove(&aKey);
    NS_ASSERTION(oldData == aLoadData, "broken loading table");
  }

  // release all parsers, but we only need to unblock one
  data = aLoadData;
  PRBool done = PR_FALSE;
  do {
    if (data->mParserToUnblock) {
      if (!done && data->mDidBlockParser) {
        done = PR_TRUE;
        data->mParserToUnblock->ContinueParsing();  // this may result in re-entrant calls to loader
      }
      // To reduce the risk of leaking a parser if we leak a SheetLoadData,
      // release the parser here.
      NS_RELEASE(data->mParserToUnblock); // assigns nsnull
    }
    data = data->mNext;
  } while (data);
    
  // if all loads complete, put pending sheets into doc
  if (0 == mLoadingSheets.Count()) {
    PRInt32  count = mPendingDocSheets.Count();
    if (count) {
      if (! mPendingDocSheets.EnumerateForwards(AreAllPendingAlternateSheets, this)) {
        PendingSheetData* last = (PendingSheetData*)mPendingDocSheets.ElementAt(count - 1);
        last->mNotify = PR_TRUE;
      }
      mPendingDocSheets.EnumerateForwards(InsertPendingSheet, this);
      mPendingDocSheets.Clear();
    }
    // start pending alternate loads
    while (mPendingAlternateSheets.Count()) {
      data = (SheetLoadData*)mPendingAlternateSheets.ElementAt(0);
      mPendingAlternateSheets.RemoveElementAt(0);
      URLKey key(data->mURL);
      LoadSheet(key, data); // this may pull other pending alternates (with same URL)
    }
  }

  NS_RELEASE(aLoadData); // delete data last, it may have last ref on loader...
}

#ifdef INCLUDE_XUL
static PRBool IsChromeURI(nsIURI* aURI)
{
  NS_ASSERTION(aURI, "bad caller");
  PRBool isChrome = PR_FALSE;
  aURI->SchemeIs("chrome", &isChrome);
  return isChrome;
}
#endif

nsresult
CSSLoaderImpl::SheetComplete(nsICSSStyleSheet* aSheet, SheetLoadData* aLoadData)
{
#ifdef INCLUDE_XUL
  if (IsChromeURI(aLoadData->mURL)) {
    nsCOMPtr<nsIXULPrototypeCache> cache(do_GetService("@mozilla.org/xul/xul-prototype-cache;1"));
    if (cache) {
      PRBool enabled;
      cache->GetEnabled(&enabled);
      if (enabled) {
        nsCOMPtr<nsICSSStyleSheet> sheet;
        cache->GetStyleSheet(aLoadData->mURL, getter_AddRefs(sheet));
        if (!sheet) {
          cache->PutStyleSheet(aSheet);
          aSheet->SetModified(PR_FALSE); // We're clean.
        }
      }
    }
  }
#endif

  nsresult result = NS_OK;

  URLKey  key(aLoadData->mURL);

  if (! aLoadData->mIsInline) { // don't remember inline sheets 
    NS_ADDREF(aSheet);  // add ref for table
    nsICSSStyleSheet* oldSheet = (nsICSSStyleSheet*)mLoadedSheets.Put(&key, aSheet);
    NS_IF_RELEASE(oldSheet);  // relase predecessor (was dirty)
  }

  SheetLoadData* data = aLoadData;
  do {  // add to parent sheet, parent doc or pending doc sheet list
    PrepareSheet(aSheet, data->mTitle, data->mMedia);
    if (data->mParentSheet) { // is child sheet
      InsertChildSheet(aSheet, data->mParentSheet, data->mSheetIndex);
      if (data->mParentRule) { // we have the @import rule that loaded this sheet
        data->mParentRule->SetSheet(aSheet);
        if (!data->mParentData) {
          // No parent load data; this must be an @import rule being
          // inserted via the DOM.  Let the parent sheet know that
          // it's done.
          nsCOMPtr<nsICSSLoaderObserver> loaderObserver(do_QueryInterface(data->mParentSheet));
          if (loaderObserver) {
            loaderObserver->StyleSheetLoaded(aSheet, PR_TRUE);
          }
        }
      }
    }
    else if (data->mIsAgent) {  // is agent sheet
      if (data->mObserver) {
        data->mObserver->StyleSheetLoaded(aSheet, PR_FALSE);
      }
    }
    else {  // doc sheet
      if (data->mParserToUnblock) { // if blocking, insert it immediately
        InsertSheetInDoc(aSheet, data->mSheetIndex, data->mOwningElement, PR_TRUE, data->mObserver);
      }
      else { // otherwise wait until all are loaded (even inlines)
        AddPendingSheet(aSheet, data->mSheetIndex, data->mOwningElement, data->mObserver);
      }
    }

    data = data->mNext;
    if (data) { // clone sheet for next insertion
      nsICSSStyleSheet* clone = nsnull;
      result = aSheet->Clone(clone);
      NS_RELEASE(aSheet);
      if (NS_SUCCEEDED(result)) {
        aSheet = clone;
      }
    }
    else {
      NS_RELEASE(aSheet);
    }
  } while (data && aSheet);

  Cleanup(key, aLoadData);
  return result;
}

nsresult
CSSLoaderImpl::ParseSheet(nsIUnicharInputStream* aIn,
                          SheetLoadData* aLoadData,
                          PRBool& aCompleted,
                          nsICSSStyleSheet*& aSheet)
{
  nsresult result;
  PRBool  failed = PR_TRUE;

  aCompleted = PR_TRUE;
  aSheet = nsnull;
  result = NS_NewCSSStyleSheet(&aSheet, aLoadData->mURL);
  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsICSSParser> parser;
    result = GetParserFor(aSheet, getter_AddRefs(parser));
    if (NS_SUCCEEDED(result)) {
      mParsingData.AppendElement(aLoadData);
      if (kNameSpaceID_Unknown != aLoadData->mDefaultNameSpaceID) {
        aSheet->SetDefaultNameSpaceID(aLoadData->mDefaultNameSpaceID);
      }
      result = parser->Parse(aIn, aLoadData->mURL, aSheet);  // this may result in re-entrant load child sheet calls
      mParsingData.RemoveElementAt(mParsingData.Count() - 1);

      if (NS_SUCCEEDED(result)) {
        aSheet->SetModified(PR_FALSE);  // make it clean from the load
        failed = PR_FALSE;
        if (0 == aLoadData->mPendingChildren) { // sheet isn't still loading children
          if (aLoadData->mIsInline) {
            aLoadData->mDidBlockParser = PR_FALSE;  // don't need to unblock, we're done and won't block
          }
          SheetComplete(aSheet, aLoadData);
        }
        else {  // else sheet is still waiting for children to load, last child will complete it
          aCompleted = PR_FALSE;
        }
      }
      RecycleParser(parser);
    }
  }
  if (failed) {
    URLKey  key(aLoadData->mURL);
    Cleanup(key, aLoadData);
  }
  return result;
}

void
CSSLoaderImpl::DidLoadStyle(nsIUnicharStreamLoader* aLoader,
                            nsIUnicharInputStream* aStyleDataStream,
                            SheetLoadData* aLoadData,
                            nsresult aStatus)
{
#ifdef NS_DEBUG
  NS_ASSERTION(! mSyncCallback, "getting synchronous callback from netlib");
#endif

  if (NS_SUCCEEDED(aStatus) && aStyleDataStream && mDocument) {
    // XXX We have no way of indicating failure. Silently fail?
    PRBool completed;
    nsCOMPtr<nsICSSStyleSheet> sheet;
    ParseSheet(aStyleDataStream, aLoadData, completed, *getter_AddRefs(sheet));
    // XXX clean up if failure or something?
  }
  else {  // load failed or document now gone, cleanup    
#ifdef DEBUG
    if (mDocument && NS_FAILED(aStatus)) {  // still have doc, must have failed
      // Dump error message to console.
      nsCAutoString url;
      aLoadData->mURL->GetSpec(url);
      nsCAutoString errorMessage(NS_LITERAL_CSTRING("CSSLoaderImpl::DidLoadStyle: Load of URL '") +
                                 url +
                                 NS_LITERAL_CSTRING("' failed.  Error code: "));
      errorMessage.AppendInt(NS_ERROR_GET_CODE(aStatus));
      NS_WARNING(errorMessage.get());
    }
#endif

    URLKey  key(aLoadData->mURL);
    Cleanup(key, aLoadData);
  }

}

typedef nsresult (*nsStringEnumFunc)(const nsString& aSubString, void *aData);

static nsresult EnumerateMediaString(const nsString& aStringList, nsStringEnumFunc aFunc, void* aData)
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

static nsresult MediumEnumFunc(const nsString& aSubString, void* aData)
{
  nsIAtom*  medium = NS_NewAtom(aSubString);
  if (!medium) return NS_ERROR_OUT_OF_MEMORY;
  ((nsICSSStyleSheet*)aData)->AppendMedium(medium);
  NS_RELEASE(medium);
  return NS_OK;
}


nsresult
CSSLoaderImpl::SetMedia(nsICSSStyleSheet* aSheet, const nsString& aMedia)
{
  nsresult rv = NS_OK;
  aSheet->ClearMedia();
  if (!aMedia.IsEmpty()) {
    rv = EnumerateMediaString(aMedia, MediumEnumFunc, aSheet);
  }
  return rv;
}

nsresult
CSSLoaderImpl::PrepareSheet(nsICSSStyleSheet* aSheet, const nsString& aTitle, 
                            const nsString& aMedia)
{
  nsresult result = SetMedia(aSheet, aMedia);
  
  aSheet->SetTitle(aTitle);
  return result;
}

nsresult
CSSLoaderImpl::AddPendingSheet(nsICSSStyleSheet* aSheet, PRInt32 aDocIndex,
                               nsIContent* aElement,
                               nsICSSLoaderObserver* aObserver)
{
  PendingSheetData* data = new PendingSheetData(aSheet, 
                                                aDocIndex, 
                                                aElement,
                                                aObserver);
  if (data) {
    mPendingDocSheets.AppendElement(data);
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

PRBool
CSSLoaderImpl::IsAlternate(const nsString& aTitle)
{
  if (!aTitle.IsEmpty()) {
    return PRBool(! aTitle.Equals(mPreferredSheet, nsCaseInsensitiveStringComparator()));
  }
  return PR_FALSE;
}

nsresult
CSSLoaderImpl::InsertSheetInDoc(nsICSSStyleSheet* aSheet, PRInt32 aDocIndex, 
                                nsIContent* aElement, PRBool aNotify,
                                nsICSSLoaderObserver* aObserver)
{
  if ((! mDocument) || (! aSheet)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (nsnull != aElement) {
    nsIDOMNode* domNode = nsnull;
    if (NS_SUCCEEDED(aElement->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)&domNode))) {
      aSheet->SetOwningNode(domNode);
      NS_RELEASE(domNode);
    }

    nsIStyleSheetLinkingElement* element;
    if (NS_SUCCEEDED(aElement->QueryInterface(NS_GET_IID(nsIStyleSheetLinkingElement),
                                              (void**)&element))) {
      element->SetStyleSheet(aSheet);
      NS_RELEASE(element);
    }
  }

  nsAutoString  title;
  aSheet->GetTitle(title);
  aSheet->SetEnabled(! IsAlternate(title));

  nsVoidKey key(mDocument);
  nsAutoVoidArray*  sheetMap = (nsAutoVoidArray*)mSheetMapTable.Get(&key);
  if (! sheetMap) {
    sheetMap = new nsAutoVoidArray();
    if (sheetMap) {
      mSheetMapTable.Put(&key, sheetMap);
    }
  }

  if (sheetMap) {
    PRInt32 insertIndex = sheetMap->Count();
    PRBool insertedSheet = PR_FALSE;
    while (0 <= --insertIndex) {
      PRInt32 targetIndex = NS_PTR_TO_INT32(sheetMap->ElementAt(insertIndex));
      if (targetIndex < aDocIndex) {
        mDocument->InsertStyleSheetAt(aSheet, insertIndex + 1, aNotify);
        sheetMap->InsertElementAt((void*)aDocIndex, insertIndex + 1);
        insertedSheet = PR_TRUE;
        break;
      }
    }
    if (!insertedSheet) { // didn't insert yet
      mDocument->InsertStyleSheetAt(aSheet, 0, aNotify);
      sheetMap->InsertElementAt((void*)aDocIndex, 0);
    }
    if (nsnull != aObserver) {
      aObserver->StyleSheetLoaded(aSheet, aNotify);
    }
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
CSSLoaderImpl::InsertChildSheet(nsICSSStyleSheet* aSheet, nsICSSStyleSheet* aParentSheet, 
                                PRInt32 aIndex)
{
  if ((! aParentSheet) || (! aSheet)) {
    return NS_ERROR_NULL_POINTER;
  }

  // child sheets should always start out enabled, even if they got
  // cloned off of top-level sheets which were disabled
  aSheet->SetEnabled(PR_TRUE);
  
  nsVoidKey key(aParentSheet);
  nsAutoVoidArray*  sheetMap = (nsAutoVoidArray*)mSheetMapTable.Get(&key);
  if (! sheetMap) {
    sheetMap = new nsAutoVoidArray();
    if (sheetMap) {
      mSheetMapTable.Put(&key, sheetMap);
    }
  }

  if (sheetMap) {
    PRInt32 insertIndex = sheetMap->Count();
    while (0 <= --insertIndex) {
      PRInt32 targetIndex = NS_PTR_TO_INT32(sheetMap->ElementAt(insertIndex));
      if (targetIndex < aIndex) {
        aParentSheet->InsertStyleSheetAt(aSheet, insertIndex + 1);
        sheetMap->InsertElementAt((void*)aIndex, insertIndex + 1);
        aSheet = nsnull;
        break;
      }
    }
    if (nsnull != aSheet) { // didn't insert yet
      aParentSheet->InsertStyleSheetAt(aSheet, 0);
      sheetMap->InsertElementAt((void*)aIndex, 0);
    }
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
CSSLoaderImpl::LoadSheet(URLKey& aKey, SheetLoadData* aData)
{
  nsresult result = NS_OK;

  SheetLoadData* loadingData = (SheetLoadData*)mLoadingSheets.Get(&aKey);
  if (loadingData) {  // already loading this sheet, glom on to the load
    while (loadingData->mNext) {
      loadingData = loadingData->mNext;
    }
    loadingData->mNext = aData;
  }
  else {  // not loading, go load it
    if (aData->mSyncLoad) { // handle agent sheets synchronously
      nsIURI* urlClone;
      result = aKey.mURL->Clone(&urlClone); // dont give key URL to netlib, it gets munged
      if (NS_SUCCEEDED(result)) {
        nsIInputStream* in;
        result = NS_OpenURI(&in, urlClone);
        NS_RELEASE(urlClone);
        if (NS_SUCCEEDED(result)) {
          // Translate the input using the argument character set id into unicode
          nsIUnicharInputStream* uin;
          result = NS_NewUTF8ConverterStream(&uin, in, 0);
          if (NS_SUCCEEDED(result)) {
            mLoadingSheets.Put(&aKey, aData);
            PRBool completed;
            nsICSSStyleSheet* sheet;
            result = ParseSheet(uin, aData, completed, sheet);
            NS_ASSERTION(completed, "sync sheet load failed to complete");
            NS_IF_RELEASE(sheet);
            NS_RELEASE(uin);
          }
          else {
            fputs("CSSLoader::LoadSheet - failed to get converter stream\n", stderr);
          }
          NS_RELEASE(in);
        }
#ifdef DEBUG
        else {
          // Dump an error message to the console
          nsCAutoString url;
          aKey.mURL->GetSpec(url);
          nsCAutoString errorMessage(NS_LITERAL_CSTRING("CSSLoaderImpl::LoadSheet: Load of URL '") +
                                     url +
                                     NS_LITERAL_CSTRING("' failed.  Error code: "));
          errorMessage.AppendInt(NS_ERROR_GET_CODE(result));
          NS_WARNING(errorMessage.get());
        }
#endif
      }
    }
    else if (mDocument || aData->mIsAgent) {  // we're still live, start an async load
      nsIUnicharStreamLoader* loader;
      nsCOMPtr<nsIURI> urlClone;
      result = aKey.mURL->Clone(getter_AddRefs(urlClone)); // dont give key URL to netlib, it gets munged
      if (NS_SUCCEEDED(result)) {
#ifdef NS_DEBUG
        mSyncCallback = PR_TRUE;
#endif
        nsCOMPtr<nsILoadGroup> loadGroup;
        mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));

        NS_ASSERTION(loadGroup, "A stylesheet was unable to locate a load group. This means the onload is going to fire too early!");

        nsCOMPtr<nsIURI> document_uri;
        mDocument->GetDocumentURL(getter_AddRefs(document_uri));

#ifdef MOZ_TIMELINE
        NS_TIMELINE_MARK_URI("Loading style sheet: %s", urlClone);
        NS_TIMELINE_INDENT();
#endif
        nsCOMPtr<nsIChannel> channel;
        result = NS_NewChannel(getter_AddRefs(channel),
                               urlClone, nsnull, loadGroup,
                               nsnull, nsIChannel::LOAD_NORMAL);
        if (NS_SUCCEEDED(result)) {
          if (document_uri) {
            nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
            if (httpChannel) {
              // send a minimal Accept header for text/css
              httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                            NS_LITERAL_CSTRING(""));
              httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                            NS_LITERAL_CSTRING("text/css,*/*;q=0.1"));
              result = httpChannel->SetReferrer(document_uri);
            }
          }

          if (NS_SUCCEEDED(result)) {
            result = NS_NewUnicharStreamLoader(&loader, channel, aData);
          }
        }

#ifdef NS_DEBUG
        mSyncCallback = PR_FALSE;
#endif
        if (NS_SUCCEEDED(result)) {
          mLoadingSheets.Put(&aKey, aData);
          // grab any pending alternates that have this URL
          loadingData = aData;
          PRInt32 index = 0;
          while (index < mPendingAlternateSheets.Count()) {
            SheetLoadData* data = (SheetLoadData*)mPendingAlternateSheets.ElementAt(index);
            PRBool equals = PR_FALSE;
            result = aKey.mURL->Equals(data->mURL, &equals);
            if (NS_SUCCEEDED(result) && equals)
            {
              mPendingAlternateSheets.RemoveElementAt(index);
              NS_IF_RELEASE(loadingData->mNext);
              loadingData->mNext = data;
              loadingData = data;
            }
            else {
              index++;
            }
          }
        }
      }
    }
    else { // document gone,  no point in starting the load
      NS_RELEASE(aData);
    }
  }
  return result;
}

NS_IMETHODIMP
CSSLoaderImpl::LoadInlineStyle(nsIContent* aElement,
                               nsIUnicharInputStream* aIn, 
                               const nsString& aTitle, 
                               const nsString& aMedia, 
                               PRInt32 aDefaultNameSpaceID,
                               PRInt32 aDocIndex,
                               nsIParser* aParserToUnblock,
                               PRBool& aCompleted,
                               nsICSSLoaderObserver* aObserver)
{
  if (!mEnabled)
    return NS_ERROR_NOT_AVAILABLE;

  NS_ASSERTION(mDocument, "not initialized");
  if (! mDocument) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // XXX need to add code to cancel any pending sheets for element
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aIn) {
    nsIURI* docURL;
    mDocument->GetBaseURL(docURL);
    SheetLoadData* data = new SheetLoadData(this, docURL, aTitle, aMedia, aDefaultNameSpaceID,
                                            aElement,
                                            aDocIndex, aParserToUnblock,
                                            PR_TRUE, aObserver);
    if (data == nsnull) {
      result = NS_ERROR_OUT_OF_MEMORY;
    }
    else {
      NS_ADDREF(data);
      nsICSSStyleSheet* sheet;
      result = ParseSheet(aIn, data, aCompleted, sheet);
      NS_IF_RELEASE(sheet);
      if ((! aCompleted) && (aParserToUnblock)) {
        data->mDidBlockParser = PR_TRUE;
      }
    }
    NS_RELEASE(docURL);
  }
  return result;
}


NS_IMETHODIMP
CSSLoaderImpl::LoadStyleLink(nsIContent* aElement,
                             nsIURI* aURL, 
                             const nsString& aTitle, 
                             const nsString& aMedia, 
                             PRInt32 aDefaultNameSpaceID,
                             PRInt32 aDocIndex,
                             nsIParser* aParserToUnblock,
                             PRBool& aCompleted,
                             nsICSSLoaderObserver* aObserver)
{
  if (!mEnabled)
    return NS_ERROR_NOT_AVAILABLE;

  NS_ASSERTION(mDocument, "not initialized");
  if (! mDocument) {
    return NS_ERROR_NOT_INITIALIZED;
  }

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

  // XXX need to add code to cancel any pending sheets for element
  nsresult result = NS_ERROR_NULL_POINTER;

  aCompleted = PR_TRUE;
  if (aURL) {
    URLKey  key(aURL);
    PRBool  sheetModified = PR_FALSE;

    nsICSSStyleSheet* sheet = (nsICSSStyleSheet*)mLoadedSheets.Get(&key);
#ifdef INCLUDE_XUL
    if (!sheet && IsChromeURI(aURL)) {
      nsCOMPtr<nsIXULPrototypeCache> cache(do_GetService("@mozilla.org/xul/xul-prototype-cache;1"));
      if (cache) {
        PRBool enabled;
        cache->GetEnabled(&enabled);
        if (enabled) {
          nsCOMPtr<nsICSSStyleSheet> cachedSheet;
          cache->GetStyleSheet(aURL, getter_AddRefs(cachedSheet));
          if (cachedSheet)
            sheet = cachedSheet;
        }
      }
    }
#endif

    if (sheet && (NS_OK == sheet->IsModified(&sheetModified)) && sheetModified) {  // if dirty, forget it
      sheet = nsnull;
    }

    if (sheet) {  // already have one fully loaded and unmodified
      nsICSSStyleSheet* clone = nsnull;
      result = sheet->Clone(clone);
      if (NS_SUCCEEDED(result)) {
        PrepareSheet(clone, aTitle, aMedia);
        if (aParserToUnblock || (0 == mLoadingSheets.Count())) { 
          // we either have a parser that needs the sheet now, or we aren't currently
          // busy.  go ahead and put the sheet right into the doc.
          result = InsertSheetInDoc(clone, aDocIndex, aElement, PR_TRUE, aObserver);
        }
        else {  // add to pending list
          result = AddPendingSheet(clone, aDocIndex, aElement, aObserver);
        }
        NS_RELEASE(clone);
      }
    }
    else {  // need to load it
      SheetLoadData* data = new SheetLoadData(this, aURL, aTitle, aMedia, aDefaultNameSpaceID,
                                              aElement, aDocIndex, 
                                              aParserToUnblock, PR_FALSE,
                                              aObserver);
      if (data == nsnull) {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      else {
        NS_ADDREF(data);
        if (IsAlternate(aTitle) && mLoadingSheets.Count() &&
            (! mLoadingSheets.Get(&key)) && (! aParserToUnblock)) {
          // this is an alternate, and we're already loading others, but not loading this, defer it
          mPendingAlternateSheets.AppendElement(data);
          result = NS_OK;
        }
        else {
          if (aParserToUnblock) {
            data->mDidBlockParser = PR_TRUE;
          }
          result = LoadSheet(key, data);
        }
      }
      aCompleted = PR_FALSE;
    }
  }
  return result;
}

NS_IMETHODIMP
CSSLoaderImpl::LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                              nsIURI* aURL, 
                              const nsString& aMedia,
                              PRInt32 aDefaultNameSpaceID,
                              PRInt32 aIndex,
                              nsICSSImportRule* aParentRule)
{
  if (!mEnabled)
    return NS_ERROR_NOT_AVAILABLE;

  nsresult result = NS_ERROR_NULL_POINTER;

  if (aURL) {
    URLKey  key(aURL);
    PRBool  sheetModified = PR_FALSE;

    nsICSSStyleSheet* sheet = (nsICSSStyleSheet*)mLoadedSheets.Get(&key);
#ifdef INCLUDE_XUL
    if (!sheet && IsChromeURI(aURL)) {
      nsCOMPtr<nsIXULPrototypeCache> cache(do_GetService("@mozilla.org/xul/xul-prototype-cache;1"));
      if (cache) {
        PRBool enabled;
        cache->GetEnabled(&enabled);
        if (enabled) {
          nsCOMPtr<nsICSSStyleSheet> cachedSheet;
          cache->GetStyleSheet(aURL, getter_AddRefs(cachedSheet));
          if (cachedSheet)
            sheet = cachedSheet;
        }
      }
    }
#endif

    if (sheet && (NS_OK == sheet->IsModified(&sheetModified)) && sheetModified) {  // if dirty, forget it
      sheet = nsnull;
    }

    if (sheet) {  // already have one loaded and unmodified
      nsCOMPtr<nsICSSStyleSheet> clone;
      result = sheet->Clone(*getter_AddRefs(clone));
      if (NS_SUCCEEDED(result)) {
        result = SetMedia(clone, aMedia);
        if (NS_SUCCEEDED(result)) {
          result = InsertChildSheet(clone, aParentSheet, aIndex);
          if (NS_SUCCEEDED(result) && aParentRule) {
            aParentRule->SetSheet(clone);
          }
        }
      }
    }
    else {
      SheetLoadData* data = new SheetLoadData(this, aURL, aMedia, aDefaultNameSpaceID,
                                              aParentSheet, aIndex, aParentRule);
      if (data == nsnull) {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      else {
        NS_ADDREF(data);
        PRInt32 count = mParsingData.Count();
        if (count) { // still parsing the parent (expected for @import)
          // XXX assert that last parsing == parent sheet
          SheetLoadData* parentData = (SheetLoadData*)mParsingData.ElementAt(count - 1);
          data->mParentData = parentData;
          data->mIsAgent = parentData->mIsAgent;
          data->mSyncLoad = parentData->mSyncLoad;

          // verify that sheet doesn't have new child as a parent 
          do {
            PRBool equals;
            result = parentData->mURL->Equals(aURL, &equals);
            if (NS_SUCCEEDED(result) && equals) { // houston, we have a loop, blow off this child
              data->mParentData = nsnull;
              NS_RELEASE(data);
              return NS_OK;
            }
            parentData = parentData->mParentData;
          } while (parentData);

          ++(data->mParentData->mPendingChildren);
        }
        result = LoadSheet(key, data);
        if (count && NS_FAILED(result)) {
          // If the load failed, undo the addition of the pending child.
          --(data->mParentData->mPendingChildren);
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP
CSSLoaderImpl::LoadAgentSheet(nsIURI* aURL, 
                              nsICSSStyleSheet*& aSheet,
                              PRBool& aCompleted,
                              nsICSSLoaderObserver* aObserver)
{
  if (!mEnabled)
    return NS_ERROR_NOT_AVAILABLE;
  
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aURL) {
    // Get an input stream from the url
    nsIInputStream* in;
    nsIURI* urlClone;
    result = aURL->Clone(&urlClone); // dont give key URL to netlib, it gets munged
    if (NS_SUCCEEDED(result)) {
      result = NS_OpenURI(&in, urlClone);
      NS_RELEASE(urlClone);
      if (NS_SUCCEEDED(result)) {
        // Translate the input, using our characterset, into unicode
        nsCOMPtr<nsIConverterInputStream> uin =
          do_CreateInstance("@mozilla.org/intl/converter-input-stream;1",
                            &result);
        if (NS_SUCCEEDED(result))
          result = uin->Init(in, NS_LITERAL_STRING("ISO-8859-1").get(),
                             0, PR_TRUE);
        if (NS_SUCCEEDED(result)) {
          SheetLoadData* data = new SheetLoadData(this, aURL, aObserver);
          if (data == nsnull) {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
          else {
            NS_ADDREF(data);
            URLKey  key(aURL);
						if (aObserver == nsnull) {
	            mLoadingSheets.Put(&key, data);
	            result = ParseSheet(uin, data, aCompleted, aSheet);
	          }
						else {
	      			result = LoadSheet(key, data); // this may steal pending alternates too
	      			aCompleted = PR_FALSE;
	      		}
          }
        }
        else {
          fputs("CSSLoader::LoadAgentSheet - failed to get converter stream\n", stderr);
        }
        NS_RELEASE(in);
      }
#ifdef DEBUG
      else {
        // Dump an error message to the console
        PRBool ignoreError = PR_FALSE;
        nsCAutoString url;
        aURL->GetSpec(url);
        // Ignore userChrome.css and userContent.css failures
#define USERCHROMECSS "userChrome.css"
#define USERCONTENTCSS "userContent.css"
        if (url.Length() > sizeof(USERCHROMECSS) &&
            Substring(url, url.Length()-sizeof(USERCHROMECSS)+1, sizeof(USERCHROMECSS)).Equals(NS_LITERAL_CSTRING(USERCHROMECSS)))
          ignoreError = PR_TRUE;

        if (!ignoreError &&
            url.Length() > sizeof(USERCONTENTCSS) &&
            Substring(url, url.Length()-sizeof(USERCONTENTCSS)+1, sizeof(USERCONTENTCSS)).Equals(NS_LITERAL_CSTRING(USERCONTENTCSS)))
          ignoreError = PR_TRUE;

        if (!ignoreError) {
          nsCAutoString errorMessage(NS_LITERAL_CSTRING("CSSLoaderImpl::LoadAgentSheet: Load of URL '") +
                                     url +
                                     NS_LITERAL_CSTRING("' failed.  Error code: "));
          errorMessage.AppendInt(NS_ERROR_GET_CODE(result));
          NS_WARNING(errorMessage.get());
        }
      }
#endif
    }
  }
  return result;
}


nsresult NS_NewCSSLoader(nsIDocument* aDocument, nsICSSLoader** aLoader)
{
  CSSLoaderImpl* it = new CSSLoaderImpl();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->Init(aDocument);
  return it->QueryInterface(NS_GET_IID(nsICSSLoader), (void **)aLoader);
}

nsresult NS_NewCSSLoader(nsICSSLoader** aLoader)
{
  CSSLoaderImpl* it = new CSSLoaderImpl();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsICSSLoader), (void **)aLoader);
}


static PRBool PR_CALLBACK StopLoadingSheetCallback(nsHashKey* aKey, void* aData, void* aClosure)
{
  NS_ENSURE_TRUE(aData, NS_ERROR_NULL_POINTER);

  SheetLoadData* data = (SheetLoadData*)aData;
  NS_ENSURE_TRUE(data->mLoader, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(data->mURL, NS_ERROR_NULL_POINTER);

  data->mLoader->StopLoadingSheet(data->mURL);

  return PR_TRUE;
}

NS_IMETHODIMP
CSSLoaderImpl::Stop()
{
  if (mLoadingSheets.Count() > 0) {
    mLoadingSheets.Enumerate(StopLoadingSheetCallback);
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::StopLoadingSheet(nsIURI* aURL)
{
  NS_ENSURE_TRUE(aURL, NS_ERROR_NULL_POINTER);
  if (mLoadingSheets.Count() > 0) {
    URLKey key(aURL);
    SheetLoadData* loadData = (SheetLoadData*)mLoadingSheets.Get(&key);
    if (loadData) {
      Cleanup(key, loadData);
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
