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
 *   Boris Zbarsky <bzbarsky@mit.edu>
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

#ifndef nsCSSLoader_h__
#define nsCSSLoader_h__

class CSSLoaderImpl;
class nsIURI;
class nsICSSStyleSheet;
class nsIStyleSheetLinkingElement;
class nsICSSLoaderObserver;
class nsICSSParser;
class nsICSSImportRule;
class nsMediaList;

#include "nsICSSLoader.h"
#include "nsIRunnable.h"
#include "nsIUnicharStreamLoader.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsURIHashKey.h"
#include "nsInterfaceHashtable.h"
#include "nsDataHashtable.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

/**
 * OVERALL ARCHITECTURE
 *
 * The CSS Loader gets requests to load various sorts of style sheets:
 * inline style from <style> elements, linked style, @import-ed child
 * sheets, non-document sheets.  The loader handles the following tasks:
 *
 * 1) Checking whether the load is allowed: CheckLoadAllowed()
 * 2) Creation of the actual style sheet objects: CreateSheet()
 * 3) setting of the right media, title, enabled state, etc on the
 *    sheet: PrepareSheet()
 * 4) Insertion of the sheet in the proper cascade order:
 *    InsertSheetInDoc() and InsertChildSheet()
 * 5) Load of the sheet: LoadSheet()
 * 6) Parsing of the sheet: ParseSheet()
 * 7) Cleanup: SheetComplete()
 *
 * The detailed documentation for these functions is found with the
 * function implementations.
 *
 * The following helper object is used:
 *    SheetLoadData -- a small class that is used to store all the
 *                     information needed for the loading of a sheet;
 *                     this class handles listening for the stream
 *                     loader completion and also handles charset
 *                     determination.
 */

/*********************************************
 * Data needed to properly load a stylesheet *
 *********************************************/

class SheetLoadData : public nsIRunnable,
                      public nsIUnicharStreamLoaderObserver
{
public:
  virtual ~SheetLoadData(void);
  // Data for loading a sheet linked from a document
  SheetLoadData(CSSLoaderImpl* aLoader,
                const nsSubstring& aTitle,
                nsIURI* aURI,
                nsICSSStyleSheet* aSheet,
                nsIStyleSheetLinkingElement* aOwningElement,
                PRBool aIsAlternate,
                nsICSSLoaderObserver* aObserver);                 

  // Data for loading a sheet linked from an @import rule
  SheetLoadData(CSSLoaderImpl* aLoader,
                nsIURI* aURI,
                nsICSSStyleSheet* aSheet,
                SheetLoadData* aParentData,
                nsICSSLoaderObserver* aObserver);                 

  // Data for loading a non-document sheet
  SheetLoadData(CSSLoaderImpl* aLoader,
                nsIURI* aURI,
                nsICSSStyleSheet* aSheet,
                PRBool aSyncLoad,
                PRBool aAllowUnsafeRules,
                nsICSSLoaderObserver* aObserver);

  already_AddRefed<nsIURI> GetReferrerURI();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIUNICHARSTREAMLOADEROBSERVER

  // Hold a ref to the CSSLoader so we can call back to it to let it
  // know the load finished
  CSSLoaderImpl*             mLoader; // strong ref

  // Title needed to pull datas out of the pending datas table when
  // the preferred title is changed
  nsString                   mTitle;

  // Charset we decided to use for the sheet
  nsCString                  mCharset;

  // URI we're loading.  Null for inline sheets
  nsCOMPtr<nsIURI>           mURI;

  // Should be 1 for non-inline sheets.
  PRUint32                   mLineNumber;

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
  // now only for LoadSheetSync and children of sync loads.
  PRPackedBool               mSyncLoad : 1;

  // mIsNonDocumentSheet is true if the load was triggered by LoadSheetSync or
  // LoadSheet or an @import from such a sheet.  Non-document sheet loads can
  // proceed even if we have no document.
  PRPackedBool               mIsNonDocumentSheet : 1;

  // mIsLoading is true from the moment we are placed in the loader's
  // "loading datas" table (right after the async channel is opened)
  // to the moment we are removed from said table (due to the load
  // completing or being cancelled).
  PRPackedBool               mIsLoading : 1;

  // mIsCancelled is set to true when a sheet load is stopped by
  // Stop() or StopLoadingSheet().  SheetLoadData::OnStreamComplete()
  // checks this to avoid parsing sheets that have been cancelled and
  // such.
  PRPackedBool               mIsCancelled : 1;

  // mMustNotify is true if the load data is being loaded async and
  // the original function call that started the load has returned.
  // XXXbz sort our relationship with load/error events!
  PRPackedBool               mMustNotify : 1;
  
  // mWasAlternate is true if the sheet was an alternate when the load data was
  // created.
  PRPackedBool               mWasAlternate : 1;
  
  // mAllowUnsafeRules is true if we should allow unsafe rules to be parsed
  // in the loaded sheet.
  PRPackedBool               mAllowUnsafeRules : 1;
  
  // This is the element that imported the sheet.  Needed to get the
  // charset set on it.
  nsCOMPtr<nsIStyleSheetLinkingElement> mOwningElement;

  // The observer that wishes to be notified of load completion
  nsCOMPtr<nsICSSLoaderObserver>        mObserver;
};


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


/**********************
 * Loader Declaration *
 **********************/

class CSSLoaderImpl : public nsICSSLoader
{
public:
  CSSLoaderImpl(void);
  virtual ~CSSLoaderImpl(void);

  NS_DECL_ISUPPORTS

  static void Shutdown(); // called at app shutdown
  
  // nsICSSLoader methods
  NS_IMETHOD Init(nsIDocument* aDocument);
  NS_IMETHOD DropDocumentReference(void);

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive);
  NS_IMETHOD SetCompatibilityMode(nsCompatibility aCompatMode);
  NS_IMETHOD SetPreferredSheet(const nsAString& aTitle);
  NS_IMETHOD GetPreferredSheet(nsAString& aTitle);

  NS_IMETHOD GetParserFor(nsICSSStyleSheet* aSheet,
                          nsICSSParser** aParser);
  NS_IMETHOD RecycleParser(nsICSSParser* aParser);

  NS_IMETHOD LoadInlineStyle(nsIContent* aElement,
                             nsIUnicharInputStream* aStream, 
                             PRUint32 aLineNumber,
                             const nsSubstring& aTitle,
                             const nsSubstring& aMedia,
                             nsICSSLoaderObserver* aObserver,
                             PRBool* aCompleted,
                             PRBool* aIsAlternate);

  NS_IMETHOD LoadStyleLink(nsIContent* aElement,
                           nsIURI* aURL, 
                           const nsSubstring& aTitle,
                           const nsSubstring& aMedia,
                           PRBool aHasAlternateRel,
                           nsICSSLoaderObserver* aObserver,
                           PRBool* aIsAlternate);

  NS_IMETHOD LoadChildSheet(nsICSSStyleSheet* aParentSheet,
                            nsIURI* aURL, 
                            nsMediaList* aMedia,
                            nsICSSImportRule* aRule);

  NS_IMETHOD LoadSheetSync(nsIURI* aURL, PRBool aAllowUnsafeRules,
                           nsICSSStyleSheet** aSheet);

  NS_IMETHOD LoadSheet(nsIURI* aURL, nsICSSLoaderObserver* aObserver,
                       nsICSSStyleSheet** aSheet);

  NS_IMETHOD LoadSheet(nsIURI* aURL, nsICSSLoaderObserver* aObserver);

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

  // IsAlternate can change our currently selected style set if none
  // is selected and aHasAlternateRel is false.
  PRBool IsAlternate(const nsAString& aTitle, PRBool aHasAlternateRel);

private:
  nsresult CheckLoadAllowed(nsIURI* aSourceURI,
                            nsIURI* aTargetURI,
                            nsISupports* aContext);


  // For inline style, the aURI param is null, but the aLinkingContent
  // must be non-null then.
  nsresult CreateSheet(nsIURI* aURI,
                       nsIContent* aLinkingContent,
                       PRBool aSyncLoad,
                       StyleSheetState& aSheetState,
                       nsICSSStyleSheet** aSheet);

  // Pass in either a media string or the nsMediaList from the
  // CSSParser.  Don't pass both.
  // If aIsAlternate is non-null, this method will set *aIsAlternate to
  // correspond to the sheet's enabled state (which it will set no matter what)
  nsresult PrepareSheet(nsICSSStyleSheet* aSheet,
                        const nsSubstring& aTitle,
                        const nsSubstring& aMediaString,
                        nsMediaList* aMediaList,
                        PRBool aHasAlternateRel = PR_FALSE,
                        PRBool *aIsAlternate = nsnull);

  nsresult InsertSheetInDoc(nsICSSStyleSheet* aSheet,
                            nsIContent* aLinkingContent,
                            nsIDocument* aDocument);

  nsresult InsertChildSheet(nsICSSStyleSheet* aSheet,
                            nsICSSStyleSheet* aParentSheet,
                            nsICSSImportRule* aParentRule);

  nsresult InternalLoadNonDocumentSheet(nsIURI* aURL,
                                        PRBool aAllowUnsafeRules,
                                        nsICSSStyleSheet** aSheet,
                                        nsICSSLoaderObserver* aObserver);

  // Post a load event for aObserver to be notified about aSheet.  The
  // notification will be sent with status NS_OK unless the load event is
  // canceled at some point (in which case it will be sent with
  // NS_BINDING_ABORTED).  aWasAlternate indicates the state when the load was
  // initiated, not the state at some later time.  aURI should be the URI the
  // sheet was loaded from (may be null for inline sheets).
  nsresult PostLoadEvent(nsIURI* aURI,
                         nsICSSStyleSheet* aSheet,
                         nsICSSLoaderObserver* aObserver,
                         PRBool aWasAlternate);
public:
  // Handle an event posted by PostLoadEvent
  void HandleLoadEvent(SheetLoadData* aEvent);

  // Note: LoadSheet is responsible for releasing aLoadData and setting the
  // sheet to complete on failure.
  nsresult LoadSheet(SheetLoadData* aLoadData, StyleSheetState aSheetState);

protected:
  friend class SheetLoadData;

  // Protected functions and members are ones that SheetLoadData needs
  // access to.

  // Parse the stylesheet in aLoadData.  The sheet data comes from aStream.
  // Set aCompleted to true if the parse finished, false otherwise (e.g. if the
  // sheet had an @import).  If aCompleted is true when this returns, then
  // ParseSheet also called SheetComplete on aLoadData
  nsresult ParseSheet(nsIUnicharInputStream* aStream,
                      SheetLoadData* aLoadData,
                      PRBool& aCompleted);

public:
  // The load of the sheet in aLoadData is done, one way or another.  Do final
  // cleanup, including releasing aLoadData.
  void SheetComplete(SheetLoadData* aLoadData, nsresult aStatus);

private:
  typedef nsTArray<nsRefPtr<SheetLoadData> > LoadDataArray;
  
  // The guts of SheetComplete.  This may be called recursively on parent datas
  // or datas that had glommed on to a single load.  The array is there so load
  // datas whose observers need to be notified can be added to it.
  void DoSheetComplete(SheetLoadData* aLoadData, nsresult aStatus,
                       LoadDataArray& aDatasToNotify);

  static nsCOMArray<nsICSSParser>* gParsers;  // array of idle CSS parsers

  // the load data needs access to the document...
  nsIDocument*      mDocument;  // the document we live for

#ifdef DEBUG
  PRPackedBool            mSyncCallback;
#endif

  PRPackedBool      mCaseSensitive; // is document CSS case sensitive
  PRPackedBool      mEnabled; // is enabled to load new styles
  nsCompatibility   mCompatMode;
  nsString          mPreferredSheet;  // title of preferred sheet

  nsInterfaceHashtable<nsURIHashKey,nsICSSStyleSheet> mCompleteSheets;
  nsDataHashtable<nsURIHashKey,SheetLoadData*> mLoadingDatas; // weak refs
  nsDataHashtable<nsURIHashKey,SheetLoadData*> mPendingDatas; // weak refs
  
  // We're not likely to have many levels of @import...  But likely to have
  // some.  Allocate some storage, what the hell.
  nsAutoVoidArray   mParsingDatas;

  // The array of posted stylesheet loaded events (SheetLoadDatas) we have.
  // Note that these are rare.
  LoadDataArray mPostedEvents;
};

#endif // nsCSSLoader_h__
