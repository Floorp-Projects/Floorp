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
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIObserver.h"

// Classes
class nsPagePrintTimer;
class nsIDocShellTreeNode;
class nsIDeviceContext;
class nsIDocumentViewerPrint;
class nsPrintObject;
class nsIDocShell;
class nsIPageSequenceFrame;

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
class nsPrintEngine : public nsIObserver
{
public:
  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

  // Old nsIWebBrowserPrint methods; not cleaned up yet
  NS_IMETHOD Print(nsIPrintSettings*       aPrintSettings,
                   nsIWebProgressListener* aWebProgressListener);
  NS_IMETHOD PrintPreview(nsIPrintSettings* aPrintSettings,
                          nsIDOMWindow *aChildDOMWin,
                          nsIWebProgressListener* aWebProgressListener);
  NS_IMETHOD GetIsFramesetDocument(PRBool *aIsFramesetDocument);
  NS_IMETHOD GetIsIFrameSelected(PRBool *aIsIFrameSelected);
  NS_IMETHOD GetIsRangeSelection(PRBool *aIsRangeSelection);
  NS_IMETHOD GetIsFramesetFrameSelected(PRBool *aIsFramesetFrameSelected);
  NS_IMETHOD GetPrintPreviewNumPages(PRInt32 *aPrintPreviewNumPages);
  NS_IMETHOD EnumerateDocumentNames(PRUint32* aCount, PRUnichar*** aResult);
  static nsresult GetGlobalPrintSettings(nsIPrintSettings** aPrintSettings);
  NS_IMETHOD GetDoingPrint(PRBool *aDoingPrint);
  NS_IMETHOD GetDoingPrintPreview(PRBool *aDoingPrintPreview);
  NS_IMETHOD GetCurrentPrintSettings(nsIPrintSettings **aCurrentPrintSettings);


  // This enum tells indicates what the default should be for the title
  // if the title from the document is null
  enum eDocTitleDefault {
    eDocTitleDefNone,
    eDocTitleDefBlank,
    eDocTitleDefURLDoc
  };

  nsPrintEngine();
  ~nsPrintEngine();

  void Destroy();
  void DestroyPrintingData();

  nsresult Initialize(nsIDocumentViewerPrint* aDocViewerPrint, 
                      nsISupports*            aContainer,
                      nsIDocument*            aDocument,
                      nsIDeviceContext*       aDevContext,
                      nsIWidget*              aParentWidget,
                      FILE*                   aDebugFile);

  nsresult GetSeqFrameAndCountPages(nsIFrame*& aSeqFrame, PRInt32& aCount);

  //
  // The following three methods are used for printing...
  //
  nsresult DocumentReadyForPrinting();
  nsresult GetSelectionDocument(nsIDeviceContextSpec * aDevSpec,
                                nsIDocument ** aNewDoc);

  nsresult SetupToPrintContent();
  nsresult EnablePOsForPrinting();
  nsPrintObject* FindSmallestSTF();

  PRBool   PrintDocContent(nsPrintObject* aPO, nsresult& aStatus);
  nsresult DoPrint(nsPrintObject * aPO);

  void SetPrintPO(nsPrintObject* aPO, PRBool aPrint);

  void TurnScriptingOn(PRBool aDoTurnOn);
  PRBool CheckDocumentForPPCaching();
  void InstallPrintPreviewListener();

  // nsIDocumentViewerPrint Printing Methods
  PRBool   PrintPage(nsPrintObject* aPOect, PRBool& aInRange);
  PRBool   DonePrintingPages(nsPrintObject* aPO, nsresult aResult);

  //---------------------------------------------------------------------
  void BuildDocTree(nsIDocShellTreeNode * aParentNode,
                    nsVoidArray *         aDocList,
                    nsPrintObject *         aPO);
  nsresult ReflowDocList(nsPrintObject * aPO, PRBool aSetPixelScale);

  nsresult ReflowPrintObject(nsPrintObject * aPO);

  void CheckForChildFrameSets(nsPrintObject* aPO);

  void CalcNumPrintablePages(PRInt32& aNumPages);
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

  PRBool IsThereARangeSelection(nsIDOMWindow * aDOMWin);

  //---------------------------------------------------------------------


  // Timer Methods
  nsresult StartPagePrintTimer(nsPrintObject* aPO);

  PRBool IsWindowsInOurSubTree(nsIDOMWindow * aDOMWindow);
  static PRBool IsParentAFrameSet(nsIDocShell * aParent);
  PRBool IsThereAnIFrameSelected(nsIDocShell* aDocShell,
                                 nsIDOMWindow* aDOMWin,
                                 PRPackedBool& aIsParentFrameSet);

  static nsPrintObject* FindPrintObjectByDOMWin(nsPrintObject* aParentObject,
                                                nsIDOMWindow* aDOMWin);

  // get the currently infocus frame for the document viewer
  already_AddRefed<nsIDOMWindow> FindFocusedDOMWindow();

  //---------------------------------------------------------------------
  // Static Methods
  //---------------------------------------------------------------------
  static void GetDocumentTitleAndURL(nsIDocument* aDoc,
                                     PRUnichar** aTitle,
                                     PRUnichar** aURLStr);
  void GetDisplayTitleAndURL(nsPrintObject*    aPO,
                             PRUnichar**       aTitle,
                             PRUnichar**       aURLStr,
                             eDocTitleDefault  aDefType);
  static void ShowPrintErrorDialog(nsresult printerror,
                                   PRBool aIsPrinting = PR_TRUE);

  static PRBool HasFramesetChild(nsIContent* aContent);

  PRBool   CheckBeforeDestroy();
  nsresult Cancelled();

  nsIWidget* GetPrintPreviewWindow() {return mPrtPreview->mPrintObject->mWindow;}

  nsIViewManager* GetPrintPreviewViewManager() {return mPrtPreview->mPrintObject->mViewManager;}

  static nsIPresShell* GetPresShellFor(nsIDocShell* aDocShell);

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

protected:

  nsresult CommonPrint(PRBool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
              nsIWebProgressListener* aWebProgressListener);

  nsresult DoCommonPrint(PRBool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
                         nsIWebProgressListener* aWebProgressListener);

  void FirePrintCompletionEvent();
  static nsresult GetSeqFrameAndCountPagesInternal(nsPrintObject*  aPO,
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

  static void MapContentForPO(nsPrintObject* aPO, nsIContent* aContent);

  static void MapContentToWebShells(nsPrintObject* aRootPO, nsPrintObject* aPO);

  static void SetPrintAsIs(nsPrintObject* aPO, PRBool aAsIs = PR_TRUE);

  // Static member variables
  PRPackedBool mIsCreatingPrintPreview;
  PRPackedBool mIsDoingPrinting;
  PRPackedBool mIsDoingPrintPreview; // per DocumentViewer
  PRPackedBool mProgressDialogIsShown;

  nsIDocumentViewerPrint* mDocViewerPrint; // [WEAK] it owns me!
  nsISupports*            mContainer;      // [WEAK] it owns me!
  nsIDeviceContext*       mDeviceContext;  // not ref counted
  
  nsPrintData*            mPrt;
  nsPagePrintTimer*       mPagePrintTimer;
  nsIPageSequenceFrame*   mPageSeqFrame;

  // Print Preview
  nsCOMPtr<nsIWidget>     mParentWidget;        
  nsPrintData*            mPrtPreview;
  nsPrintData*            mOldPrtPreview;

  nsCOMPtr<nsIDocument>   mDocument;

  FILE* mDebugFile;

private:
  nsPrintEngine& operator=(const nsPrintEngine& aOther); // not implemented

};

#endif /* nsPrintEngine_h___ */
