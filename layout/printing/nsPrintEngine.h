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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * ***** END LICENSE BLOCK ***** */
#ifndef nsPrintEngine_h___
#define nsPrintEngine_h___

#include "nsCOMPtr.h"

#include "nsPrintObject.h"
#include "nsPrintData.h"

// Interfaces
#include "nsIDeviceContext.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIObserver.h"
#include "nsIPrintProgress.h"
#include "nsIPrintProgressParams.h"
#include "nsIPrintOptions.h"
#include "nsIPrintSettings.h"
#include "nsIWebProgressListener.h"
#include "nsISelectionListener.h"

// Other Includes
#include "nsPrintPreviewListener.h"
#include "nsIDocShellTreeNode.h"

// Classes
class nsIPageSequenceFrame;
class nsPagePrintTimer;

// Special Interfaces
#include "nsIWebBrowserPrint.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentViewerPrint.h"

#ifdef MOZ_LAYOUTDEBUG
#include "nsIDebugObject.h"
#endif

//------------------------------------------------------------------------
// nsPrintEngine Class
//
// mPreparingForPrint - indicates that we have started Printing but 
//   have not gone to the timer to start printing the pages. It gets turned 
//   off right before we go to the timer.
//
// mDocWasToBeDestroyed - Gets set when "someone" tries to unload the document
//   while we were prparing to Print. This typically happens if a user starts 
//   to print while a page is still loading. If they start printing and pause 
//   at the print dialog and then the page comes in, we then abort printing 
//   because the document is no longer stable.
// 
//------------------------------------------------------------------------
class nsPrintEngine : public nsIWebBrowserPrint, public nsIObserver
{
public:
  //NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIWebBrowserPrint
  NS_DECL_NSIWEBBROWSERPRINT

  // nsIObserver
  NS_DECL_NSIOBSERVER

  // This enum tells indicates what the default should be for the title
  // if the title from the document is null
  enum eDocTitleDefault {
    eDocTitleDefNone,
    eDocTitleDefBlank,
    eDocTitleDefURLDoc
  };


  nsPrintEngine();
  virtual ~nsPrintEngine();

  void Destroy();
  void DestroyPrintingData();

  nsresult Initialize(nsIDocumentViewer*      aDocViewer, 
                      nsIDocumentViewerPrint* aDocViewerPrint, 
                      nsISupports*            aContainer,
                      nsIDocument*            aDocument,
                      nsIDeviceContext*       aDevContext,
                      nsPresContext*         aPresContext,
                      nsIWidget*              aWindow,
                      nsIWidget*              aParentWidget,
                      FILE*                   aDebugFile);

  nsresult GetSeqFrameAndCountPages(nsIFrame*& aSeqFrame, PRInt32& aCount);
  PRBool   IsOldPrintPreviewPres()
  {
    return mOldPrtPreview != nsnull;
  }
  //
  // The following three methods are used for printing...
  //
  nsresult DocumentReadyForPrinting();
  nsresult GetSelectionDocument(nsIDeviceContextSpec * aDevSpec,
                                nsIDocument ** aNewDoc);

  nsresult SetupToPrintContent(nsIDeviceContext* aDContext,
                               nsIDOMWindow* aCurrentFocusedDOMWin);
  nsresult EnablePOsForPrinting();
  nsPrintObject* FindSmallestSTF();

  PRBool   PrintDocContent(nsPrintObject* aPO, nsresult& aStatus);
  nsresult DoPrint(nsPrintObject * aPO, PRBool aDoSyncPrinting,
                   PRBool& aDonePrinting);
  void SetPrintAsIs(nsPrintObject* aPO, PRBool aAsIs = PR_TRUE);

  enum ePrintFlags {
    eSetPrintFlag = 1U,
    eSetHiddenFlag = 2U
  };
  void SetPrintPO(nsPrintObject* aPO, PRBool aPrint,
                  PRBool aIsHidden = PR_FALSE,
                  PRUint32 aFlags = eSetPrintFlag);

  void TurnScriptingOn(PRBool aDoTurnOn);
  PRBool CheckDocumentForPPCaching();
  void InstallPrintPreviewListener();

  // nsIDocumentViewerPrint Printing Methods
  PRBool   PrintPage(nsPresContext* aPresContext,
                     nsIPrintSettings* aPrintSettings,
                     nsPrintObject* aPOect, PRBool& aInRange);
  PRBool   DonePrintingPages(nsPrintObject* aPO, nsresult aResult);

  //---------------------------------------------------------------------
  void BuildDocTree(nsIDocShellTreeNode * aParentNode,
                    nsVoidArray *         aDocList,
                    nsPrintObject *         aPO);
  nsresult ReflowDocList(nsPrintObject * aPO, PRBool aSetPixelScale,
                         PRBool aDoCalcShrink);
  void SetClipRect(nsPrintObject*  aPO,
                   const nsRect& aClipRect,
                   nscoord       aOffsetX,
                   nscoord       aOffsetY,
                   PRBool        aDoingSetClip);

  nsresult ReflowPrintObject(nsPrintObject * aPO, PRBool aDoCalcShrink);
  nsresult CalcPageFrameLocation(nsIPresShell * aPresShell,
                                  nsPrintObject*   aPO);
  nsPrintObject * FindPrintObjectByWS(nsPrintObject* aPO, nsIWebShell * aWebShell);
  void MapContentForPO(nsPrintObject*   aRootObject,
                       nsIPresShell*  aPresShell,
                       nsIContent*    aContent);
  void MapContentToWebShells(nsPrintObject* aRootPO, nsPrintObject* aPO);
  void CheckForChildFrameSets(nsPrintObject* aPO);
  nsresult MapSubDocFrameLocations(nsPrintObject* aPO);

  void CalcNumPrintableDocsAndPages(PRInt32& aNumDocs, PRInt32& aNumPages);
  void DoProgressForAsIsFrames();
  void DoProgressForSeparateFrames();
  void ShowPrintProgress(PRBool aIsForPrinting, PRBool& aDoNotify);
  nsresult CleanupOnFailure(nsresult aResult, PRBool aIsPrinting);
  nsresult FinishPrintPreview();
  static void CloseProgressDialog(nsIWebProgressListener* aWebProgressListener);

  void SetDocAndURLIntoProgress(nsPrintObject* aPO,
                                nsIPrintProgressParams* aParams);
  void ElipseLongString(PRUnichar *& aStr, const PRUint32 aLen, PRBool aDoFront);
  nsresult CheckForPrinters(nsIPrintOptions*  aPrintOptions,
                            nsIPrintSettings* aPrintSettings);
  void CleanupDocTitleArray(PRUnichar**& aArray, PRInt32& aCount);
  void CheckForHiddenFrameSetFrames();

  PRBool IsThereARangeSelection(nsIDOMWindow * aDOMWin);

  //---------------------------------------------------------------------


  // Timer Methods
  nsresult StartPagePrintTimer(nsPresContext * aPresContext,
                               nsIPrintSettings* aPrintSettings,
                               nsPrintObject*     aPO,
                               PRUint32         aDelay);

  PRBool IsWindowsInOurSubTree(nsIDOMWindow * aDOMWindow);
  PRBool IsParentAFrameSet(nsIWebShell * aParent);
  PRBool IsThereAnIFrameSelected(nsIWebShell* aWebShell,
                                 nsIDOMWindow* aDOMWin,
                                 PRPackedBool& aIsParentFrameSet);

  nsPrintObject* FindPrintObjectByDOMWin(nsPrintObject* aParentObject,
                                         nsIDOMWindow* aDOMWin);

  // get the currently infocus frame for the document viewer
  already_AddRefed<nsIDOMWindow> FindFocusedDOMWindow();

  //---------------------------------------------------------------------
  // Static Methods
  //---------------------------------------------------------------------
  static void GetDocumentTitleAndURL(nsIDocument* aDoc,
                                     PRUnichar** aTitle,
                                     PRUnichar** aURLStr);
  static void GetDisplayTitleAndURL(nsPrintObject*      aPO, 
                                    nsIPrintSettings* aPrintSettings, 
                                    const PRUnichar*  aBrandName,
                                    PRUnichar**       aTitle, 
                                    PRUnichar**       aURLStr,
                                    eDocTitleDefault  aDefType = eDocTitleDefNone);
  static void ShowPrintErrorDialog(nsresult printerror,
                                   PRBool aIsPrinting = PR_TRUE);
  static void GetPresShellAndRootContent(nsIDocShell *  aDocShell,
                                         nsIPresShell** aPresShell,
                                         nsIContent**   aContent);

  static PRBool HasFramesetChild(nsIContent* aContent);

  PRBool   CheckBeforeDestroy();
  nsresult Cancelled();

  nsresult ShowDocList(PRBool aShow);

  void GetNewPresentation(nsCOMPtr<nsIPresShell>& aShell, 
                          nsCOMPtr<nsPresContext>& aPC, 
                          nsCOMPtr<nsIViewManager>& aVM, 
                          nsCOMPtr<nsIWidget>& aW);

  // CachedPresentationObj is used to cache the presentation
  // so we can bring it back later
  PRBool HasCachedPres()
  {
    return mIsCachingPresentation && mCachedPresObj;
  }
  PRBool IsCachingPres()
  {
    return mIsCachingPresentation;
  }
  void SetCacheOldPres(PRBool aDoCache)
  {
    mIsCachingPresentation = aDoCache;
  }
  void CachePresentation(nsIPresShell* aShell, nsPresContext* aPC,
                         nsIViewManager* aVM, nsIWidget* aW);
  void GetCachedPresentation(nsCOMPtr<nsIPresShell>& aShell, 
                             nsCOMPtr<nsPresContext>& aPC, 
                             nsCOMPtr<nsIViewManager>& aVM, 
                             nsCOMPtr<nsIWidget>& aW);

  static nsIPresShell* GetPresShellFor(nsIDocShell* aDocShell);

  void SetDialogParent(nsIDOMWindow* aDOMWin)
  {
    mDialogParentWin = aDOMWin;
  }

  // These calls also update the DocViewer
  void SetIsPrinting(PRBool aIsPrinting);
  PRBool GetIsPrinting()
  {
    return mIsDoingPrinting;
  }
  void SetIsPrintPreview(PRBool aIsPrintPreview);
  PRBool GetIsPrintPreview()
  {
    return mIsDoingPrintPreview;
  }
  void SetIsCreatingPrintPreview(PRBool aIsCreatingPrintPreview)
  {
    mIsCreatingPrintPreview = aIsCreatingPrintPreview;
  }
  PRBool GetIsCreatingPrintPreview()
  {
    return mIsCreatingPrintPreview;
  }

#ifdef MOZ_LAYOUTDEBUG
  static nsresult TestRuntimeErrorCondition(PRInt16  aRuntimeID, 
                                            nsresult aCurrentErrorCode, 
                                            nsresult aNewErrorCode);

  static PRBool IsDoingRuntimeTesting();
  static void InitializeTestRuntimeError();
protected:
  static PRBool mIsDoingRuntimeTesting;

  static nsCOMPtr<nsIDebugObject> mLayoutDebugObj; // always de-referenced with the destructor
#endif

protected:
  void FirePrintCompletionEvent();
  nsresult ShowDocListInternal(nsPrintObject* aPO, PRBool aShow);
  nsresult GetSeqFrameAndCountPagesInternal(nsPrintObject*  aPO,
                                            nsIFrame*&      aSeqFrame,
                                            PRInt32&        aCount);

  static nsresult FindSelectionBoundsWithList(nsPresContext* aPresContext,
                                              nsIRenderingContext& aRC,
                                              nsIAtom*        aList,
                                              nsIFrame *      aParentFrame,
                                              nsRect&         aRect,
                                              nsIFrame *&     aStartFrame,
                                              nsRect&         aStartRect,
                                              nsIFrame *&     aEndFrame,
                                              nsRect&         aEndRect);

  static nsresult FindSelectionBounds(nsPresContext* aPresContext,
                                      nsIRenderingContext& aRC,
                                      nsIFrame *      aParentFrame,
                                      nsRect&         aRect,
                                      nsIFrame *&     aStartFrame,
                                      nsRect&         aStartRect,
                                      nsIFrame *&     aEndFrame,
                                      nsRect&         aEndRect);

  static nsresult GetPageRangeForSelection(nsIPresShell *        aPresShell,
                                           nsPresContext*       aPresContext,
                                           nsIRenderingContext&  aRC,
                                           nsISelection*         aSelection,
                                           nsIPageSequenceFrame* aPageSeqFrame,
                                           nsIFrame**            aStartFrame,
                                           PRInt32&              aStartPageNum,
                                           nsRect&               aStartRect,
                                           nsIFrame**            aEndFrame,
                                           PRInt32&              aEndPageNum,
                                           nsRect&               aEndRect);

  static nsIFrame * FindFrameByType(nsPresContext* aPresContext,
                                    nsIFrame *      aParentFrame,
                                    nsIAtom *       aType,
                                    nsRect&         aRect,
                                    nsRect&         aChildRect);

  // Static memeber variables
  PRBool mIsCreatingPrintPreview;
  PRBool mIsDoingPrinting;

  nsIDocumentViewerPrint* mDocViewerPrint; // [WEAK] it owns me!
  nsIDocumentViewer*      mDocViewer;      // [WEAK] it owns me!
  nsISupports*            mContainer;      // [WEAK] it owns me!
  nsIDeviceContext*       mDeviceContext;  // not ref counted
  nsPresContext*         mPresContext;    // not ref counted
  nsCOMPtr<nsIWidget>     mWindow;      
  
  nsPrintData*            mPrt;
  nsPagePrintTimer*       mPagePrintTimer;
  nsIPageSequenceFrame*   mPageSeqFrame;

  // Print Preview
  PRBool                  mIsDoingPrintPreview; // per DocumentViewer
  nsCOMPtr<nsIWidget>     mParentWidget;        
  nsPrintData*            mPrtPreview;
  nsPrintData*            mOldPrtPreview;

  nsCOMPtr<nsIDocument>   mDocument;
  nsCOMPtr<nsIDOMWindow>  mDialogParentWin;

  PRBool                      mIsCachingPresentation;
  CachedPresentationObj*      mCachedPresObj;
  FILE* mDebugFile;

private:
  nsPrintEngine& operator=(const nsPrintEngine& aOther); // not implemented

};

#ifdef MOZ_LAYOUTDEBUG
#define INIT_RUNTIME_ERROR_CHECKING() nsPrintEngine::InitializeTestRuntimeError();
#define CHECK_RUNTIME_ERROR_CONDITION(_name, _currerr, _newerr) \
  if (nsPrintEngine::IsDoingRuntimeTesting()) { \
    _currerr = nsPrintEngine::TestRuntimeErrorCondition(_name, _currerr, _newerr); \
  }
#else
#define CHECK_RUNTIME_ERROR_CONDITION(_name, _currerr, _newerr)
#define INIT_RUNTIME_ERROR_CHECKING()
#endif

#endif /* nsPrintEngine_h___ */
