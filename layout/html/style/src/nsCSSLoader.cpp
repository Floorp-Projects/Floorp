/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsICSSLoader.h"

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

#include "nsHashtable.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNeckoUtil.h"
#else
#include "nsIURLGroup.h"
#endif // NECKO
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"

#include <iostream.h>

static NS_DEFINE_IID(kICSSLoaderIID, NS_ICSS_LOADER_IID);
//static NS_DEFINE_IID(kICSSParserIID, NS_ICSS_PARSER_IID);
//static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
//static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIStyleSheetLinkingElementIID, NS_ISTYLESHEETLINKINGELEMENT_IID);

class CSSLoaderImpl;

class URLKey: public nsHashKey {
public:
  URLKey(nsIURI* aURL)
    : nsHashKey(),
      mURL(aURL)
  {
    NS_ADDREF(mURL);
    mHashValue = 0;

#ifdef NECKO
    char* urlStr;
    mURL->GetSpec(&urlStr);
#else
    PRUnichar*  urlStr;
    mURL->ToString(&urlStr);
#endif
    if (urlStr) {
      mHashValue = nsCRT::HashValue(urlStr);
#ifdef NECKO
      nsCRT::free(urlStr);
#else
      delete [] urlStr;
#endif
    }
  }

  URLKey(const URLKey& aKey)
    : nsHashKey(),
      mURL(aKey.mURL),
      mHashValue(aKey.mHashValue)
  {
    NS_ADDREF(mURL);
  }

  virtual ~URLKey(void)
  {
    NS_RELEASE(mURL);
  }

  virtual PRUint32 HashValue(void) const
  {
    return mHashValue;
  }

  virtual PRBool Equals(const nsHashKey* aKey) const
  {
    URLKey* key = (URLKey*)aKey;
#ifdef NECKO
    PRBool equals = PR_FALSE;
    nsresult result = mURL->Equals(key->mURL, &equals);
    return (NS_SUCCEEDED(result) && equals);
#else
    return mURL->Equals(key->mURL);
#endif
  }

  virtual nsHashKey *Clone(void) const
  {
    return new URLKey(*this);
  }

  nsIURI*   mURL;
  PRUint32  mHashValue;
};

class SupportsKey: public nsHashKey {
public:
  SupportsKey(nsISupports* aSupports)
    : nsHashKey(),
      mSupports(aSupports)
  { // note: does not hold reference on supports pointer
  }

  SupportsKey(const SupportsKey& aKey)
    : nsHashKey(),
      mSupports(aKey.mSupports)
  {
  }

  virtual ~SupportsKey(void)
  {
  }

  virtual PRUint32 HashValue(void) const
  {
    return (PRUint32)mSupports;
  }

  virtual PRBool Equals(const nsHashKey* aKey) const
  {
    SupportsKey* key = (SupportsKey*)aKey;
    return PRBool(mSupports == key->mSupports);
  }

  virtual nsHashKey *Clone(void) const
  {
    return new SupportsKey(*this);
  }

  nsISupports*  mSupports;
};

struct SheetLoadData {
  SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                const nsString& aTitle, const nsString& aMedia, 
                PRInt32 aDefaultNameSpaceID,
                nsIContent* aOwner, PRInt32 aDocIndex, 
                nsIParser* aParserToUnblock, PRBool aIsInline);
  SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, const nsString& aMedia,
                PRInt32 aDefaultNameSpaceID,
                nsICSSStyleSheet* aParentSheet, PRInt32 aSheetIndex);
  SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, nsCSSLoaderCallbackFunc aCallback,
                void* aData);
  ~SheetLoadData(void);

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

  SheetLoadData*  mNext;
  SheetLoadData*  mParentData;

  PRUint32        mPendingChildren;

  PRBool          mIsInline;
  PRBool          mIsAgent;

  nsCSSLoaderCallbackFunc mCallback;
  void*                   mCallbackData;
};

struct PendingSheetData {
  PendingSheetData(nsICSSStyleSheet* aSheet, PRInt32 aDocIndex,
                   nsIContent* aElement)
    : mSheet(aSheet),
      mDocIndex(aDocIndex),
      mOwningElement(aElement),
      mNotify(PR_FALSE)
  {
    NS_ADDREF(mSheet);
    NS_IF_ADDREF(mOwningElement);
  }

  ~PendingSheetData(void)
  {
    NS_RELEASE(mSheet);
    NS_IF_RELEASE(mOwningElement);
  }

  nsICSSStyleSheet* mSheet;
  PRInt32           mDocIndex;
  nsIContent*       mOwningElement;
  PRBool            mNotify;
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
  NS_IMETHOD SetPreferredSheet(const nsString& aTitle);

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
                             PRBool& aCompleted);

  NS_IMETHOD LoadStyleLink(nsIContent* aElement,
                           nsIURI* aURL, 
                           const nsString& aTitle, 
                           const nsString& aMedia, 
                           PRInt32 aDefaultNameSpaceID,
                           PRInt32 aIndex,
                           nsIParser* aParserToUnblock,
                           PRBool& aCompleted);

  NS_IMETHOD LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                            nsIURI* aURL, 
                            const nsString& aMedia,
                            PRInt32 aDefaultNameSpaceID,
                            PRInt32 aIndex);

  NS_IMETHOD LoadAgentSheet(nsIURI* aURL, 
                          nsICSSStyleSheet*& aSheet,
                          PRBool& aCompleted,
                          nsCSSLoaderCallbackFunc aCallback,
                          void *aData);

  // local helper methods (public for access from statics)
  void Cleanup(URLKey& aKey, SheetLoadData* aLoadData);
  nsresult SheetComplete(nsICSSStyleSheet* aSheet, SheetLoadData* aLoadData);

  nsresult ParseSheet(nsIUnicharInputStream* aIn, SheetLoadData* aLoadData,
                      PRBool& aCompleted, nsICSSStyleSheet*& aSheet);

  void DidLoadStyle(nsIUnicharStreamLoader* aLoader,
                    nsString& aStyleData,
                    SheetLoadData* aLoadData,
                    nsresult aStatus);

  nsresult SetMedia(nsICSSStyleSheet* aSheet, const nsString& aMedia);

  nsresult PrepareSheet(nsICSSStyleSheet* aSheet, const nsString& aTitle, 
                        const nsString& aMedia);

  nsresult AddPendingSheet(nsICSSStyleSheet* aSheet, PRInt32 aDocIndex, 
                           nsIContent* aElement);

  PRBool IsAlternate(const nsString& aTitle);

  nsresult InsertSheetInDoc(nsICSSStyleSheet* aSheet, PRInt32 aDocIndex, 
                            nsIContent* aElement, PRBool aNotify);

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

  nsVoidArray   mParsingData; // array of data for sheets currently parsing

  nsVoidArray   mPendingDocSheets;  // loaded sheet waiting for doc insertion
  nsVoidArray   mPendingAlternateSheets;  // alternates waiting for load to start

  nsHashtable   mSheetMapTable;  // map to insertion index arrays

#ifdef NS_DEBUG
  PRBool  mSyncCallback;
#endif
};

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                             const nsString& aTitle, const nsString& aMedia,
                             PRInt32 aDefaultNameSpaceID,
                             nsIContent* aOwner, PRInt32 aDocIndex, 
                             nsIParser* aParserToUnblock, PRBool aIsInline)
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
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mIsInline(aIsInline),
    mIsAgent(PR_FALSE),
    mCallback(nsnull),
    mCallbackData(nsnull)
{
  NS_ADDREF(mLoader);
  NS_ADDREF(mURL);
  NS_IF_ADDREF(mOwningElement);
  NS_IF_ADDREF(mParserToUnblock);
}

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                             const nsString& aMedia, 
                             PRInt32 aDefaultNameSpaceID,
                             nsICSSStyleSheet* aParentSheet, PRInt32 aSheetIndex)
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
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mIsInline(PR_FALSE),
    mIsAgent(PR_FALSE),
    mCallback(nsnull),
    mCallbackData(nsnull)
{
  NS_ADDREF(mLoader);
  NS_ADDREF(mURL);
  NS_ADDREF(mParentSheet);
}

SheetLoadData::SheetLoadData(CSSLoaderImpl* aLoader, nsIURI* aURL, 
                             nsCSSLoaderCallbackFunc aCallback, void* aData)
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
    mNext(nsnull),
    mParentData(nsnull),
    mPendingChildren(0),
    mIsInline(PR_FALSE),
    mIsAgent(PR_TRUE),
    mCallback(aCallback),
    mCallbackData(nsnull)
{
  NS_ADDREF(mLoader);
  NS_ADDREF(mURL);
}


SheetLoadData::~SheetLoadData(void)
{
  NS_RELEASE(mLoader);
  NS_RELEASE(mURL);
  NS_IF_RELEASE(mOwningElement);
  NS_IF_RELEASE(mParserToUnblock);
  NS_IF_RELEASE(mParentSheet);
  if (mNext) {
    delete mNext;
  }
}

CSSLoaderImpl::CSSLoaderImpl(void)
{
  NS_INIT_REFCNT();
  mDocument = nsnull;
  mCaseSensitive = PR_FALSE;
  mNavQuirkMode = PR_FALSE;
  mParsers = nsnull;
}

static PRBool ReleaseSheet(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsICSSStyleSheet* sheet = (nsICSSStyleSheet*)aData;
  NS_RELEASE(sheet);
  return PR_TRUE;
}

static PRBool DeleteHashLoadData(nsHashKey* aKey, void* aData, void* aClosure)
{
  SheetLoadData* data = (SheetLoadData*)aData;
  delete data;
  return PR_TRUE;
}

static PRBool DeletePendingData(void* aData, void* aClosure)
{
  PendingSheetData* data = (PendingSheetData*)aData;
  delete data;
  return PR_TRUE;
}

static PRBool DeleteLoadData(void* aData, void* aClosure)
{
  SheetLoadData* data = (SheetLoadData*)aData;
  delete data;
  return PR_TRUE;
}

static PRBool DeleteSheetMap(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsVoidArray* map = (nsVoidArray*)aData;
  delete map;
  return PR_TRUE;
}

CSSLoaderImpl::~CSSLoaderImpl(void)
{
  NS_ASSERTION(0 == mLoadingSheets.Count(), "destructing loader while sheets loading");
  NS_IF_RELEASE(mParsers);
  mLoadedSheets.Enumerate(ReleaseSheet);
  mLoadingSheets.Enumerate(DeleteHashLoadData);
  mPendingDocSheets.EnumerateForwards(DeletePendingData, nsnull);
  mPendingAlternateSheets.EnumerateForwards(DeleteLoadData, nsnull);
  mSheetMapTable.Enumerate(DeleteSheetMap);
}

NS_IMPL_ISUPPORTS(CSSLoaderImpl, kICSSLoaderIID)

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
CSSLoaderImpl::SetPreferredSheet(const nsString& aTitle)
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
    NS_RELEASE(aParser);
  }
  return result;
}

static void
DoneLoadingStyle(nsIUnicharStreamLoader* aLoader,
                 nsString& aStyleData,
                 void* aLoadData,
                 nsresult aStatus)
{
  SheetLoadData* data = (SheetLoadData*)aLoadData;
  data->mLoader->DidLoadStyle(aLoader, aStyleData, data, aStatus);
  // We added a reference when the loader was created. This
  // release should destroy it.
  NS_RELEASE(aLoader);
}

static PRBool
InsertPendingSheet(void* aPendingData, void* aLoader)
{
  PendingSheetData* data = (PendingSheetData*)aPendingData;
  CSSLoaderImpl*  loader = (CSSLoaderImpl*)aLoader;
  loader->InsertSheetInDoc(data->mSheet, data->mDocIndex, 
                           data->mOwningElement, data->mNotify);
  delete data;
  return PR_TRUE;
}

static PRBool
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
        SheetComplete(data->mParentSheet, data->mParentData);
      }
    }
    data = data->mNext;
  } while (data);
    
  if (! aLoadData->mIsInline) { // inline sheets don't go in loading table
    SheetLoadData* oldData = (SheetLoadData*)mLoadingSheets.Remove(&aKey);
    NS_ASSERTION(oldData == aLoadData, "broken loading table");
  }

  // unblock parser
  data = aLoadData;
  do {
    if (data->mParserToUnblock && data->mDidBlockParser) {
      data->mParserToUnblock->EnableParser(PR_TRUE);  // this may result in re-entrant calls to loader
      break;
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

  delete aLoadData; // delete data last, it may have last ref on loader...
}

nsresult
CSSLoaderImpl::SheetComplete(nsICSSStyleSheet* aSheet, SheetLoadData* aLoadData)
{
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
    }
    else if (data->mIsAgent) {  // is agent sheet
      if (data->mCallback) {
        (*(data->mCallback))(aSheet, data->mCallbackData);
      }
    }
    else {  // doc sheet
      if (data->mParserToUnblock) { // if blocking, insert it immediately
        InsertSheetInDoc(aSheet, data->mSheetIndex, data->mOwningElement, PR_TRUE);
      }
      else { // otherwise wait until all are loaded (even inlines)
        AddPendingSheet(aSheet, data->mSheetIndex, data->mOwningElement);
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
    nsICSSParser* parser;
    result = GetParserFor(aSheet, &parser);
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
                            nsString& aStyleData,
                            SheetLoadData* aLoadData,
                            nsresult aStatus)
{
#ifdef NS_DEBUG
  NS_ASSERTION(! mSyncCallback, "getting synchronous callback from netlib");
#endif

  if (NS_SUCCEEDED(aStatus) && (0 < aStyleData.Length()) && (mDocument)) {
    nsresult result;
    nsIUnicharInputStream* uin = nsnull;
    // wrap the string with the CSS data up in a unicode input stream.
    result = NS_NewStringUnicharInputStream(&uin, new nsString(aStyleData));
    if (NS_SUCCEEDED(result)) {
      // XXX We have no way of indicating failure. Silently fail?
      PRBool completed;
      nsICSSStyleSheet* sheet;
      result = ParseSheet(uin, aLoadData, completed, sheet);
      NS_IF_RELEASE(sheet);

      NS_RELEASE(uin);
    }
    else {
      URLKey  key(aLoadData->mURL);
      Cleanup(key, aLoadData);
    }
  }
  else {  // load failed or document now gone, cleanup    
    if (mDocument && NS_FAILED(aStatus)) {  // still have doc, must have failed
      // Dump error message to console.
#ifdef NECKO
      char *url;
#else
      const char *url;
#endif
      if (nsnull != aLoadData->mURL) {
        aLoadData->mURL->GetSpec(&url);
      }
      else {
        url = "";      
      }
      cerr << "CSSLoaderImpl::DidLoadStyle: Load of URL '" << url 
           << "' failed.  Error code: " << NS_ERROR_GET_CODE(aStatus) << "\n";
#ifdef NECKO
      nsCRT::free(url);
#endif
    }

    URLKey  key(aLoadData->mURL);
    Cleanup(key, aLoadData);
  }

}

typedef PRBool (*nsStringEnumFunc)(const nsString& aSubString, void *aData);

static PRBool EnumerateMediaString(const nsString& aStringList, nsStringEnumFunc aFunc, void* aData)
{
  PRBool    running = PR_TRUE;

  nsAutoString  stringList(aStringList); // copy to work buffer
  nsAutoString  subStr;

  stringList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)stringList.GetUnicode();
  PRUnichar* end   = start;

  while (running && (kNullCh != *start)) {
    PRBool  quoted = PR_FALSE;

    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
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
      if ((PR_FALSE == nsString::IsAlpha(*test)) && 
          (PR_FALSE == nsString::IsDigit(*test)) && (kMinus != *test)) {
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
      running = (*aFunc)(subStr, aData);
    }

    start = ++end;
  }

  return running;
}

static PRBool MediumEnumFunc(const nsString& aSubString, void* aData)
{
  nsIAtom*  medium = NS_NewAtom(aSubString);
  ((nsICSSStyleSheet*)aData)->AppendMedium(medium);
  return PR_TRUE;
}


nsresult
CSSLoaderImpl::SetMedia(nsICSSStyleSheet* aSheet, const nsString& aMedia)
{
  aSheet->ClearMedia();
  if (0 < aMedia.Length()) {
    EnumerateMediaString(aMedia, MediumEnumFunc, aSheet);
  }
  return NS_OK;
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
                               nsIContent* aElement) 
{
  PendingSheetData* data = new PendingSheetData(aSheet, aDocIndex, aElement);
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
                                nsIContent* aElement, PRBool aNotify)
{
  if ((! mDocument) || (! aSheet)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (nsnull != aElement) {
    nsIDOMNode* domNode = nsnull;
    if (NS_SUCCEEDED(aElement->QueryInterface(kIDOMNodeIID, (void**)&domNode))) {
      aSheet->SetOwningNode(domNode);
      NS_RELEASE(domNode);
    }

    nsIStyleSheetLinkingElement* element;
    if (NS_SUCCEEDED(aElement->QueryInterface(kIStyleSheetLinkingElementIID,
                                              (void**)&element))) {
      element->SetStyleSheet(aSheet);
      NS_RELEASE(element);
    }
  }

  nsAutoString  title;
  aSheet->GetTitle(title);
  aSheet->SetEnabled(! IsAlternate(title));

  SupportsKey key(mDocument);
  nsVoidArray*  sheetMap = (nsVoidArray*)mSheetMapTable.Get(&key);
  if (! sheetMap) {
    sheetMap = new nsVoidArray();
    if (sheetMap) {
      mSheetMapTable.Put(&key, sheetMap);
    }
  }

  if (sheetMap) {
    PRInt32 insertIndex = sheetMap->Count();
    while (0 <= --insertIndex) {
      PRInt32 targetIndex = (PRInt32)sheetMap->ElementAt(insertIndex);
      if (targetIndex < aDocIndex) {
        mDocument->InsertStyleSheetAt(aSheet, insertIndex + 1, aNotify);
        sheetMap->InsertElementAt((void*)aDocIndex, insertIndex + 1);
        aSheet = nsnull;
        break;
      }
    }
    if (nsnull != aSheet) { // didn't insert yet
      mDocument->InsertStyleSheetAt(aSheet, 0, aNotify);
      sheetMap->InsertElementAt((void*)aDocIndex, 0);
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

  SupportsKey key(aParentSheet);
  nsVoidArray*  sheetMap = (nsVoidArray*)mSheetMapTable.Get(&key);
  if (! sheetMap) {
    sheetMap = new nsVoidArray();
    if (sheetMap) {
      mSheetMapTable.Put(&key, sheetMap);
    }
  }

  if (sheetMap) {
    PRInt32 insertIndex = sheetMap->Count();
    while (0 <= --insertIndex) {
      PRInt32 targetIndex = (PRInt32)sheetMap->ElementAt(insertIndex);
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

#ifndef NECKO
static nsIURI* CloneURL(nsIURI* aURL)
{
  nsIURI* result = nsnull;

  PRUnichar*  urlStr;
  aURL->ToString(&urlStr);
  if (urlStr) {
    nsAutoString buffer(urlStr);
    delete [] urlStr;
    nsILoadGroup* LoadGroup = nsnull;
    aURL->GetLoadGroup(&LoadGroup);
    if (LoadGroup) {
      LoadGroup->CreateURL(&result, aURL, buffer, nsnull);
      NS_RELEASE(LoadGroup);
    }
    else {
      NS_NewURL(&result, buffer, aURL);
    }
  }
  return result;
}
#endif

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
    nsIUnicharStreamLoader* loader;
#ifdef NECKO
    nsIURI* urlClone;
    result = aKey.mURL->Clone(&urlClone); // dont give key URL to netlib, it gets munged
    if (NS_SUCCEEDED(result)) {
#else
    nsIURI* urlClone = CloneURL(aKey.mURL); // don't give the key to netlib, it munges it
    if (urlClone) {
#endif
#ifdef NS_DEBUG
      mSyncCallback = PR_TRUE;
#endif
      result = NS_NewUnicharStreamLoader(&loader, urlClone, 
#ifdef NECKO
                                         nsCOMPtr<nsILoadGroup>(mDocument->GetDocumentLoadGroup()),
#endif
                                         DoneLoadingStyle, aData);
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
#ifdef NECKO
          PRBool equals = PR_FALSE;
          result = aKey.mURL->Equals(data->mURL, &equals);
          if (NS_SUCCEEDED(result) && equals)
#else
          if (aKey.mURL->Equals(data->mURL))
#endif
          {
            mPendingAlternateSheets.RemoveElementAt(index);
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
                               PRBool& aCompleted)
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
                                            PR_TRUE);
    nsICSSStyleSheet* sheet;
    result = ParseSheet(aIn, data, aCompleted, sheet);
    NS_IF_RELEASE(sheet);
    if ((! aCompleted) && (aParserToUnblock)) {
      data->mDidBlockParser = PR_TRUE;
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
                             PRBool& aCompleted)
{
  NS_ASSERTION(mDocument, "not initialized");
  if (! mDocument) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // XXX need to add code to cancel any pending sheets for element
  nsresult result = NS_ERROR_NULL_POINTER;

  aCompleted = PR_TRUE;
  if (aURL) {
    URLKey  key(aURL);

    nsICSSStyleSheet* sheet = (nsICSSStyleSheet*)mLoadedSheets.Get(&key);
    if (sheet && (NS_COMFALSE == sheet->IsUnmodified())) {  // if dirty, forget it
      sheet = nsnull;
    }

    if (sheet) {  // already have one fully loaded and unmodified
      nsICSSStyleSheet* clone = nsnull;
      result = sheet->Clone(clone);
      if (NS_SUCCEEDED(result)) {
        PrepareSheet(clone, aTitle, aMedia);
        if (aParserToUnblock) { // stick it in now, parser is waiting for it
          result = InsertSheetInDoc(clone, aDocIndex, aElement, PR_TRUE);
        }
        else {  // add to pending list
          result = AddPendingSheet(clone, aDocIndex, aElement);
        }
        NS_RELEASE(clone);
      }
    }
    else {  // need to load it
      SheetLoadData* data = new SheetLoadData(this, aURL, aTitle, aMedia, aDefaultNameSpaceID,
                                              aElement, aDocIndex, 
                                              aParserToUnblock, PR_FALSE);
      if (IsAlternate(aTitle) && mLoadingSheets.Count() &&
          (! mLoadingSheets.Get(&key)) && (! aParserToUnblock)) {
        // this is an alternate, and we're already loading others, but not loading this, defer it
        mPendingAlternateSheets.AppendElement(data);
        result = NS_OK;
      }
      else {
        result = LoadSheet(key, data);
      }
      aCompleted = PR_FALSE;
      if (aParserToUnblock) {
        data->mDidBlockParser = PR_TRUE;
      }
    }
  }
  return result;
}

NS_IMETHODIMP
CSSLoaderImpl::LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                              nsIURI* aURL, 
                              const nsString& aMedia,
                              PRInt32 aDefaultNameSpaceID,
                              PRInt32 aIndex)
{
  nsresult result = NS_ERROR_NULL_POINTER;

  if (aURL) {
    URLKey  key(aURL);

    nsICSSStyleSheet* sheet = (nsICSSStyleSheet*)mLoadedSheets.Get(&key);
    if (sheet && (NS_COMFALSE == sheet->IsUnmodified())) {  // if dirty, forget it
      sheet = nsnull;
    }

    if (sheet) {  // already have one loaded and unmodified
      nsICSSStyleSheet* clone = nsnull;
      result = sheet->Clone(clone);
      if (NS_SUCCEEDED(result)) {
        SetMedia(clone, aMedia);
        result = InsertChildSheet(clone, aParentSheet, aIndex);
        NS_RELEASE(clone);
      }
    }
    else {
      SheetLoadData* data = new SheetLoadData(this, aURL, aMedia, aDefaultNameSpaceID,
                                              aParentSheet, aIndex);

      PRInt32 count = mParsingData.Count();
      if (count) { // still parsing the parent (expected for @import)
        // XXX assert that last parsing == parent sheet
        SheetLoadData* parentData = (SheetLoadData*)mParsingData.ElementAt(count - 1);
        data->mParentData = parentData;

        // verify that sheet doesn't have new child as a parent 
        do {
#ifdef NECKO
          PRBool equals;
          result = parentData->mURL->Equals(aURL, &equals);
          if (NS_SUCCEEDED(result) && equals) { // houston, we have a loop, blow off this child
#else
          if (parentData->mURL->Equals(aURL)) { // houston, we have a loop, blow off this child
#endif
            data->mParentData = nsnull;
            delete data;
            return NS_OK;
          }
          parentData = parentData->mParentData;
        } while (parentData);
        
        (data->mParentData->mPendingChildren)++;
      }
      result = LoadSheet(key, data);
    }
  }
  return result;
}

NS_IMETHODIMP
CSSLoaderImpl::LoadAgentSheet(nsIURI* aURL, 
                              nsICSSStyleSheet*& aSheet,
                              PRBool& aCompleted,
                              nsCSSLoaderCallbackFunc aCallback,
                              void *aData)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aURL) {
    // Get an input stream from the url
    nsIInputStream* in;
#ifdef NECKO
    nsIURI* urlClone;
    result = aURL->Clone(&urlClone); // dont give key URL to netlib, it gets munged
    if (NS_SUCCEEDED(result)) {
#else
    nsIURI* urlClone = CloneURL(aURL);  // dont give key URL to netlib, it gets munged
    if (urlClone) {
#endif
#ifdef NECKO
      result = NS_OpenURI(&in, urlClone);
#else
      result = NS_OpenURL(urlClone, &in);
#endif
      NS_RELEASE(urlClone);
      if (NS_SUCCEEDED(result)) {
        // Translate the input using the argument character set id into unicode
        nsIUnicharInputStream* uin;
        result = NS_NewConverterStream(&uin, nsnull, in);
        if (NS_SUCCEEDED(result)) {
          SheetLoadData* data = new SheetLoadData(this, aURL, aCallback, aData);
          URLKey  key(aURL);
          mLoadingSheets.Put(&key, data);
          result = ParseSheet(uin, data, aCompleted, aSheet);
          NS_RELEASE(uin);
        }
        else {
          fputs("CSSLoader::LoadAgentSheet - failed to get converter stream\n", stderr);
        }
        NS_RELEASE(in);
      }
      else {
        // Dump an error message to the console
#ifdef NECKO
        char *url;
#else
        const char *url;
#endif
        if (nsnull != aURL) 
          aURL->GetSpec(&url);
        else
          url = "";      
        cerr << "CSSLoaderImpl::LoadAgentSheet: Load of URL '" << url 
             << "' failed.  Error code: " << NS_ERROR_GET_CODE(result)  << "\n";
#ifdef NECKO
        nsCRT::free(url);
#endif
      }
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
  return it->QueryInterface(kICSSLoaderIID, (void **)aLoader);
}

nsresult NS_NewCSSLoader(nsICSSLoader** aLoader)
{
  CSSLoaderImpl* it = new CSSLoaderImpl();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSLoaderIID, (void **)aLoader);
}



