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
#include "nsIStreamLoader.h"
#include "nsIUnicharInputStream.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsICharsetAlias.h"
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

#ifdef INCLUDE_XUL
#include "nsIXULPrototypeCache.h"
#endif

#include "nsIDOMMediaList.h"
#include "nsIDOMStyleSheet.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

class CSSLoaderImpl;

MOZ_DECL_CTOR_COUNTER(URLKey)

class URLKey: public nsHashKey {
public:
  URLKey(nsIURI* aURL)
    : nsHashKey(),
      mURL(aURL),
      mSpec(nsnull)
  {
    MOZ_COUNT_CTOR(URLKey);
    NS_ADDREF(mURL);
    mHashValue = 0;

    mURL->GetSpec((char **)&mSpec);
    if (mSpec) {
      mHashValue = nsCRT::HashCode(mSpec);
    }
  }

  URLKey(const URLKey& aKey)
    : nsHashKey(),
      mURL(aKey.mURL),
      mHashValue(aKey.mHashValue),
      mSpec(nsnull)
  {
    MOZ_COUNT_CTOR(URLKey);
    NS_ADDREF(mURL);
    if (aKey.mSpec)
      mSpec = nsCRT::strdup(aKey.mSpec);
  }

  virtual ~URLKey(void)
  {
    MOZ_COUNT_DTOR(URLKey);
    NS_RELEASE(mURL);
    if (mSpec)
      nsCRT::free((char *)mSpec);
    mSpec = nsnull;
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
    return (nsCRT::strcasecmp(mSpec, key->mSpec) == 0);
#endif
  }

  virtual nsHashKey *Clone(void) const
  {
    return new URLKey(*this);
  }

  nsIURI*   mURL;
  PRUint32  mHashValue;
  const char* mSpec;
};

class SheetLoadData : public nsIStreamLoaderObserver
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
  NS_DECL_NSISTREAMLOADEROBSERVER

  CSSLoaderImpl*  mLoader;
  nsIURI*         mURL;
  nsString        mTitle;
  nsString        mMedia;
  PRInt32         mDefaultNameSpaceID;
  PRInt32         mSheetIndex;

  nsIContent*     mOwningElement;
  nsIParser*      mParserToUnblock;
  PRBool          mDidBlockParser;

  nsICSSStyleSheet* mParentSheet;
  nsICSSImportRule* mParentRule;

  SheetLoadData*  mNext;
  SheetLoadData*  mParentData;

  PRUint32        mPendingChildren;

  PRBool          mIsInline;
  PRBool          mIsAgent;
  PRBool          mSyncLoad;

  nsICSSLoaderObserver* mObserver;
};

NS_IMPL_ISUPPORTS1(SheetLoadData, nsIStreamLoaderObserver);

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

class CSSLoaderImpl: public nsICSSLoader {
public:
  CSSLoaderImpl(void);
  virtual ~CSSLoaderImpl(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDocument* aDocument);
  NS_IMETHOD DropDocumentReference(void);

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive);
  NS_IMETHOD SetQuirkMode(PRBool aQuirkMode);
  NS_IMETHOD SetPreferredSheet(const nsAReadableString& aTitle);

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

  void DidLoadStyle(nsIStreamLoader* aLoader,
                    nsString* aStyleData,     // takes ownership, will delete when done
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

  nsIDocument*  mDocument;  // the document we live for

  PRBool        mCaseSensitive; // is document CSS case sensitive
  PRBool        mNavQuirkMode;  // should CSS be in quirk mode
  nsString      mPreferredSheet;    // title of preferred sheet

  nsISupportsArray* mParsers;     // array of CSS parsers

  nsHashtable   mLoadedSheets;  // url to first sheet fully loaded for URL
  nsHashtable   mLoadingSheets; // all current loads

  // mParsingData is (almost?) always needed, so create with storage
  nsAutoVoidArray   mParsingData; // array of data for sheets currently parsing

  nsVoidArray   mPendingDocSheets;  // loaded sheet waiting for doc insertion
  nsVoidArray   mPendingAlternateSheets;  // alternates waiting for load to start

  nsHashtable   mSheetMapTable;  // map to insertion index arrays

  // @charset support
  nsString      mCharset;        // the charset we are using

  NS_IMETHOD GetCharset(/*out*/nsString &aCharsetDest) const; // PUBLIC
  NS_IMETHOD SetCharset(/*in*/ const nsString &aCharsetSrc);  // PUBLIC
    // public methods for clients to set the charset if they know it
    //  NOTE: the SetCharset method will always get the preferred charset from the charset passed in
    //        unless it is the emptystring, which causes the default charset to be set

  nsresult SetCharset(/*in*/ const nsString &aHTTPHeader, /*in*/ const nsString &aStyleSheetData);
    // sets the charset based upon the data passed in
    //  - if HTTPHeader is not empty and it has a charset, use that
    //  - otherwise, if the StyleSheetData is not empty and it has '@charset' as the first substring,
    //    then use that
    //  - othersise set the default charset

  // stop loading all sheets
  NS_IMETHOD Stop(void);

  // stop loading one sheet
  NS_IMETHOD StopLoadingSheet(nsIURI* aURL);

#ifdef NS_DEBUG
  PRBool  mSyncCallback;
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
    mDidBlockParser(PR_FALSE),
    mParentSheet(nsnull),
    mParentRule(nsnull),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mIsInline(aIsInline),
    mIsAgent(PR_FALSE),
    mSyncLoad(PR_FALSE),
    mObserver(aObserver)
{
  NS_INIT_REFCNT();
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
    mDidBlockParser(PR_FALSE),
    mParentSheet(aParentSheet),
    mParentRule(aParentRule),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mIsInline(PR_FALSE),
    mIsAgent(PR_FALSE),
    mSyncLoad(PR_FALSE),
    mObserver(nsnull)
{
  NS_INIT_REFCNT();
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
    mDidBlockParser(PR_FALSE),
    mParentSheet(nsnull),
    mParentRule(nsnull),
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mIsInline(PR_FALSE),
    mIsAgent(PR_TRUE),
    mSyncLoad(PRBool(nsnull == aObserver)),
    mObserver(aObserver)
{
  NS_INIT_REFCNT();
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
{
  NS_INIT_REFCNT();
  mDocument = nsnull;
  mCaseSensitive = PR_FALSE;
  mNavQuirkMode = PR_FALSE;
  mParsers = nsnull;
  SetCharset(nsAutoString());
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
CSSLoaderImpl::SetQuirkMode(PRBool aQuirkMode)
{
  mNavQuirkMode = aQuirkMode;
  return NS_OK;
}

NS_IMETHODIMP
CSSLoaderImpl::SetPreferredSheet(const nsAReadableString& aTitle)
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
    (*aParser)->SetQuirkMode(mNavQuirkMode);
    (*aParser)->SetCharset(mCharset);
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
    result = NS_OK;
    if (! mParsers) {
      result = NS_NewISupportsArray(&mParsers);
    }
    if (mParsers) {
      mParsers->AppendElement(aParser);
    }
  }
  return result;
}

NS_IMETHODIMP
SheetLoadData::OnStreamComplete(nsIStreamLoader* aLoader,
                                nsISupports* context,
                                nsresult aStatus,
                                PRUint32 stringLen,
                                const char* string)
{
  NS_TIMELINE_OUTDENT();
  NS_TIMELINE_MARK_LOADER("SheetLoadData::OnStreamComplete(%s)", aLoader);
  nsresult result = NS_OK;
  nsString *strUnicodeBuffer = nsnull;

  if (string && stringLen>0) {

    // First determine the charset (if one is indicated) and set the data member.
    // XXX use the HTTP header data too
    nsString strStyleDataUndecoded;
    nsString strHTTPHeaderData;
    strStyleDataUndecoded.AssignWithConversion(string,stringLen);
    (void)mLoader->SetCharset(strHTTPHeaderData, strStyleDataUndecoded); // bug 66190: ignore errors
    {
      // now get the decoder
      nsCOMPtr<nsICharsetConverterManager> ccm = 
               do_GetService(kCharsetConverterManagerCID, &result);
      if (NS_SUCCEEDED(result) && ccm) {
        nsString charset;
        mLoader->GetCharset(charset);
        nsIUnicodeDecoder *decoder = nsnull;
        ccm->GetUnicodeDecoder(&charset,&decoder);
        if (decoder) {
          PRInt32 unicodeLength=0;
          if (NS_SUCCEEDED(decoder->GetMaxLength(string,stringLen,&unicodeLength))) {
            PRUnichar *unicodeString = nsnull;
            strUnicodeBuffer = new nsString;
            if (nsnull == strUnicodeBuffer) {
              result = NS_ERROR_OUT_OF_MEMORY;
            } else {
              // make space for the decoding
              strUnicodeBuffer->SetCapacity(unicodeLength);
              unicodeString = (PRUnichar *) strUnicodeBuffer->get();
              result = decoder->Convert(string, (PRInt32 *) &stringLen, unicodeString, &unicodeLength);
              if (NS_SUCCEEDED(result)) {
                strUnicodeBuffer->SetLength(unicodeLength);
              } else {
                strUnicodeBuffer->SetLength(0);
              }
            }
          }
          NS_RELEASE(decoder);
        }
      }
    }
  }

  mLoader->DidLoadStyle(aLoader, strUnicodeBuffer, this, aStatus);
  // NOTE: passed ownership of strUnicodeBuffer to mLoader in the call, 
  //       so nulling it out for clarity / safety
  strUnicodeBuffer = nsnull;

  // We added a reference when the loader was created. This
  // release should destroy it.
  NS_RELEASE(aLoader);
  return result;
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
        NS_ASSERTION(data->mParentSheet, "bug");
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
CSSLoaderImpl::DidLoadStyle(nsIStreamLoader* aLoader,
                            nsString* aStyleData,
                            SheetLoadData* aLoadData,
                            nsresult aStatus)
{
#ifdef NS_DEBUG
  NS_ASSERTION(! mSyncCallback, "getting synchronous callback from netlib");
#endif

  if (NS_SUCCEEDED(aStatus) && (aStyleData) && (0 < aStyleData->Length()) && (mDocument)) {
    nsresult result;
    nsIUnicharInputStream* uin = nsnull;

    // wrap the string with the CSS data up in a unicode input stream.
    result = NS_NewStringUnicharInputStream(&uin, aStyleData);

    if (NS_SUCCEEDED(result)) {
      // XXX We have no way of indicating failure. Silently fail?
      PRBool completed;
      nsICSSStyleSheet* sheet;
      result = ParseSheet(uin, aLoadData, completed, sheet);
      NS_IF_RELEASE(sheet);
      NS_IF_RELEASE(uin);
    }
    else {
      URLKey  key(aLoadData->mURL);
      Cleanup(key, aLoadData);
    }
  }
  else {  // load failed or document now gone, cleanup    
#ifdef DEBUG
    if (mDocument && NS_FAILED(aStatus)) {  // still have doc, must have failed
      // Dump error message to console.
      nsXPIDLCString url;
      aLoadData->mURL->GetSpec(getter_Copies(url));
      nsCAutoString errorMessage(NS_LITERAL_CSTRING("CSSLoaderImpl::DidLoadStyle: Load of URL '") +
                                 nsDependentCString(url) +
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

    if (0 < subStr.Length()) {
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
  if (0 < aMedia.Length()) {
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
  if (0 < aTitle.Length()) {
    return PRBool(! aTitle.EqualsIgnoreCase(mPreferredSheet));
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
          result = NS_NewConverterStream(&uin, nsnull, in);
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
          nsXPIDLCString url;
          aKey.mURL->GetSpec(getter_Copies(url));
          nsCAutoString errorMessage(NS_LITERAL_CSTRING("CSSLoaderImpl::LoadSheet: Load of URL '") +
                                     nsDependentCString(url) +
                                     NS_LITERAL_CSTRING("' failed.  Error code: "));
          errorMessage.AppendInt(NS_ERROR_GET_CODE(result));
          NS_WARNING(errorMessage.get());
        }
#endif
      }
    }
    else if (mDocument || aData->mIsAgent) {  // we're still live, start an async load
      nsIStreamLoader* loader;
      nsIURI* urlClone;
      result = aKey.mURL->Clone(&urlClone); // dont give key URL to netlib, it gets munged
      if (NS_SUCCEEDED(result)) {
#ifdef NS_DEBUG
        mSyncCallback = PR_TRUE;
#endif
        nsCOMPtr<nsILoadGroup> loadGroup;
        mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));

        NS_ASSERTION(loadGroup, "A stylesheet was unable to locate a load group. This means the onload is going to fire too early!");

#ifdef MOZ_TIMELINE
        NS_TIMELINE_MARK_URI("Loading style sheet: %s", urlClone);
        NS_TIMELINE_INDENT();
#endif
        result = NS_NewStreamLoader(&loader, urlClone, aData, nsnull, loadGroup,
                                    nsnull, nsIChannel::LOAD_NORMAL);
#ifdef NS_DEBUG
        mSyncCallback = PR_FALSE;
#endif
        NS_RELEASE(urlClone);
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
  NS_ASSERTION(mDocument, "not initialized");
  if (! mDocument) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  //-- Make sure this page is allowed to load this URL
  // If we are doing view-source, no need to check...
  PRBool isForViewSource = PR_FALSE;
  if (aParserToUnblock) {
    nsAutoString command;
    aParserToUnblock->GetCommand(command);
    isForViewSource = command.Equals(NS_LITERAL_STRING("view-source"));
  }
  if (!isForViewSource) {
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsIURI* docURI;
    rv = mDocument->GetBaseURL(docURI);
    if (NS_FAILED(rv) || !docURI) return NS_ERROR_FAILURE;
    rv = secMan->CheckLoadURI(docURI, aURL, nsIScriptSecurityManager::ALLOW_CHROME);
    NS_IF_RELEASE(docURI);
    if (NS_FAILED(rv)) return rv;
  }

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
      nsICSSStyleSheet* clone = nsnull;
      result = sheet->Clone(clone);
      if (NS_SUCCEEDED(result)) {
        result = SetMedia(clone, aMedia);
        if (NS_SUCCEEDED(result)) {
          result = InsertChildSheet(clone, aParentSheet, aIndex);
          if (NS_SUCCEEDED(result) && aParentRule) {
            aParentRule->SetSheet(clone);
          }
        }
        NS_RELEASE(clone);
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
        
          (data->mParentData->mPendingChildren)++;
        }
        result = LoadSheet(key, data);
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
        nsIUnicharInputStream* uin;
        result = NS_NewConverterStream(&uin, nsnull, in, 0, &mCharset);
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
          NS_RELEASE(uin);
        }
        else {
          fputs("CSSLoader::LoadAgentSheet - failed to get converter stream\n", stderr);
        }
        NS_RELEASE(in);
      }
#ifdef DEBUG
      else {
        // Dump an error message to the console
        nsXPIDLCString url;
        aURL->GetSpec(getter_Copies(url));
        nsCAutoString errorMessage(NS_LITERAL_CSTRING("CSSLoaderImpl::LoadAgentSheet: Load of URL '") +
                                   nsDependentCString(url) +
                                   NS_LITERAL_CSTRING("' failed.  Error code: "));
        errorMessage.AppendInt(NS_ERROR_GET_CODE(result));
        NS_WARNING(errorMessage.get());
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



NS_IMETHODIMP CSSLoaderImpl::GetCharset(/*out*/nsString &aCharsetDest) const
{
  NS_ASSERTION(mCharset.Length() > 0, "CSSLoader charset should be set in ctor" );
  nsresult rv = NS_OK;
  aCharsetDest = mCharset;
  return rv;
}

NS_IMETHODIMP CSSLoaderImpl::SetCharset(/*in*/ const nsString &aCharsetSrc)
  // public methods for clients to set the charset if they know it
  //  NOTE: the SetCharset method will always get the preferred charset from the charset passed in
  //        unless it is the emptystring, which causes the default charset to be set
{
  nsresult rv = NS_OK;
  if (aCharsetSrc.Length() == 0) {
    mCharset.AssignWithConversion("ISO-8859-1");
  } else {
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &rv));
    NS_ASSERTION(calias, "cannot find charset alias");
    nsAutoString charsetName(aCharsetSrc);
    if( NS_SUCCEEDED(rv) && (nsnull != calias))
    {
      PRBool same = PR_FALSE;
      rv = calias->Equals(aCharsetSrc, mCharset, &same);
      if(NS_SUCCEEDED(rv) && same)
      {
        return NS_OK; // no difference, don't change it
      }
      // different, need to change it
      rv = calias->GetPreferred(aCharsetSrc, charsetName);
      if(NS_FAILED(rv))
      {
         // failed - unknown alias , fallback to ISO-8859-1
        charsetName.AssignWithConversion("ISO-8859-1");
      }
      mCharset = charsetName;
    }
  }

  return rv;
}

nsresult CSSLoaderImpl::SetCharset(/*in*/ const nsString &aHTTPHeader, 
                                   /*in*/ const nsString &aStyleSheetData)
  // sets the charset based upon the data passed in
  //  - if HTTPHeader is not empty and it has a charset, use that
  //  - otherwise, if the StyleSheetData is not empty and it has '@charset' as the first substring,
  //    then use that
  //  - othersise set the default charset
{
  nsresult rv = NS_OK;
  nsString str;
  PRBool setCharset = PR_FALSE;

  PRInt32 charsetOffset;
  if (aHTTPHeader.Length() > 0) {
    // check if it has the charset= parameter
    static const char charsetStr[] = "charset=";
    if ((charsetOffset = aHTTPHeader.Find(charsetStr,PR_TRUE)) > 0) {
      aHTTPHeader.Right(str, aHTTPHeader.Length() -
                             (charsetOffset + sizeof(charsetStr)-1));
      setCharset = PR_TRUE;
    }
  } else if (aStyleSheetData.Length() > 0) {
    static const char atCharsetStr[] = "@charset";
    if ((charsetOffset = aStyleSheetData.Find(atCharsetStr)) > -1) {
      nsString strValue;
      // skip past the ident
      aStyleSheetData.Right(str, aStyleSheetData.Length() -
                                 (sizeof(atCharsetStr)-1));
      // strip any whitespace
      str.StripWhitespace();
      // truncate everything past the delimiter (semicolon)
      PRInt32 pos = str.Find(";");
      if (pos > -1) {
        str.Left(strValue,pos);
      }
      // strip any quotes
      strValue.Trim("\"\'");

      // that's the charset!
      str = strValue;

      setCharset = PR_TRUE;
    }
  }
  if (PR_TRUE == setCharset) {
    rv = SetCharset(str);
  }
  return rv;
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
