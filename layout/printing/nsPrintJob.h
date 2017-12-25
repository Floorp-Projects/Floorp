/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPrintJob_h
#define nsPrintJob_h

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

#include "nsCOMPtr.h"

#include "nsPrintObject.h"
#include "nsPrintData.h"
#include "nsFrameList.h"
#include "nsIFrame.h"
#include "nsIWebProgress.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

// Interfaces
#include "nsIObserver.h"

// Classes
class nsPagePrintTimer;
class nsIDocShell;
class nsIDocument;
class nsIDocumentViewerPrint;
class nsPrintObject;
class nsIDocShell;
class nsIPageSequenceFrame;

/**
 * A print job may be instantiated either for printing to an actual physical
 * printer, or for creating a print preview.
 */
class nsPrintJob final : public nsIObserver
                       , public nsIWebProgressListener
                       , public nsSupportsWeakReference
{
public:
  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

  NS_DECL_NSIWEBPROGRESSLISTENER

  // Old nsIWebBrowserPrint methods; not cleaned up yet
  NS_IMETHOD Print(nsIPrintSettings*       aPrintSettings,
                   nsIWebProgressListener* aWebProgressListener);
  NS_IMETHOD PrintPreview(nsIPrintSettings* aPrintSettings,
                          mozIDOMWindowProxy* aChildDOMWin,
                          nsIWebProgressListener* aWebProgressListener);
  NS_IMETHOD GetIsFramesetDocument(bool *aIsFramesetDocument);
  NS_IMETHOD GetIsIFrameSelected(bool *aIsIFrameSelected);
  NS_IMETHOD GetIsRangeSelection(bool *aIsRangeSelection);
  NS_IMETHOD GetIsFramesetFrameSelected(bool *aIsFramesetFrameSelected);
  NS_IMETHOD GetPrintPreviewNumPages(int32_t *aPrintPreviewNumPages);
  NS_IMETHOD EnumerateDocumentNames(uint32_t* aCount, char16_t*** aResult);
  static nsresult GetGlobalPrintSettings(nsIPrintSettings** aPrintSettings);
  NS_IMETHOD GetDoingPrint(bool *aDoingPrint);
  NS_IMETHOD GetDoingPrintPreview(bool *aDoingPrintPreview);
  NS_IMETHOD GetCurrentPrintSettings(nsIPrintSettings **aCurrentPrintSettings);


  // This enum tells indicates what the default should be for the title
  // if the title from the document is null
  enum eDocTitleDefault {
    eDocTitleDefBlank,
    eDocTitleDefURLDoc
  };

  nsPrintJob();

  void Destroy();
  void DestroyPrintingData();

  nsresult Initialize(nsIDocumentViewerPrint* aDocViewerPrint,
                      nsIDocShell*            aContainer,
                      nsIDocument*            aDocument,
                      float                   aScreenDPI);

  nsresult GetSeqFrameAndCountPages(nsIFrame*& aSeqFrame, int32_t& aCount);

  //
  // The following three methods are used for printing...
  //
  nsresult DocumentReadyForPrinting();
  nsresult GetSelectionDocument(nsIDeviceContextSpec * aDevSpec,
                                nsIDocument ** aNewDoc);

  nsresult SetupToPrintContent();
  nsresult EnablePOsForPrinting();
  nsPrintObject* FindSmallestSTF();

  bool PrintDocContent(const mozilla::UniquePtr<nsPrintObject>& aPO,
                       nsresult& aStatus);
  nsresult DoPrint(const mozilla::UniquePtr<nsPrintObject>& aPO);

  void SetPrintPO(nsPrintObject* aPO, bool aPrint);

  void TurnScriptingOn(bool aDoTurnOn);
  bool CheckDocumentForPPCaching();
  void InstallPrintPreviewListener();

  // nsIDocumentViewerPrint Printing Methods
  bool     HasPrintCallbackCanvas();
  bool     PrePrintPage();
  bool     PrintPage(nsPrintObject* aPOect, bool& aInRange);
  bool     DonePrintingPages(nsPrintObject* aPO, nsresult aResult);

  //---------------------------------------------------------------------
  void BuildDocTree(nsIDocShell *      aParentNode,
                    nsTArray<nsPrintObject*> * aDocList,
                    const mozilla::UniquePtr<nsPrintObject>& aPO);
  nsresult ReflowDocList(const mozilla::UniquePtr<nsPrintObject>& aPO,
                         bool aSetPixelScale);

  nsresult ReflowPrintObject(const mozilla::UniquePtr<nsPrintObject>& aPO);

  void CheckForChildFrameSets(const mozilla::UniquePtr<nsPrintObject>& aPO);

  void CalcNumPrintablePages(int32_t& aNumPages);
  void ShowPrintProgress(bool aIsForPrinting, bool& aDoNotify);
  nsresult CleanupOnFailure(nsresult aResult, bool aIsPrinting);
  // If FinishPrintPreview() fails, caller may need to reset the state of the
  // object, for example by calling CleanupOnFailure().
  nsresult FinishPrintPreview();
  static void CloseProgressDialog(nsIWebProgressListener* aWebProgressListener);
  void SetDocAndURLIntoProgress(const mozilla::UniquePtr<nsPrintObject>& aPO,
                                nsIPrintProgressParams* aParams);
  void EllipseLongString(nsAString& aStr, const uint32_t aLen, bool aDoFront);
  nsresult CheckForPrinters(nsIPrintSettings* aPrintSettings);
  void CleanupDocTitleArray(char16_t**& aArray, int32_t& aCount);

  bool IsThereARangeSelection(nsPIDOMWindowOuter* aDOMWin);

  void FirePrintingErrorEvent(nsresult aPrintError);
  //---------------------------------------------------------------------


  // Timer Methods
  nsresult StartPagePrintTimer(const mozilla::UniquePtr<nsPrintObject>& aPO);

  bool IsWindowsInOurSubTree(nsPIDOMWindowOuter* aDOMWindow);
  static bool IsParentAFrameSet(nsIDocShell * aParent);
  bool IsThereAnIFrameSelected(nsIDocShell* aDocShell,
                               nsPIDOMWindowOuter* aDOMWin,
                               bool& aIsParentFrameSet);

  static nsPrintObject* FindPrintObjectByDOMWin(nsPrintObject* aParentObject,
                                                nsPIDOMWindowOuter* aDOMWin);

  // get the currently infocus frame for the document viewer
  already_AddRefed<nsPIDOMWindowOuter> FindFocusedDOMWindow();

  static void GetDocumentTitleAndURL(nsIDocument* aDoc,
                                     nsAString&   aTitle,
                                     nsAString&   aURLStr);
  void GetDisplayTitleAndURL(const mozilla::UniquePtr<nsPrintObject>& aPO,
                             nsAString&       aTitle,
                             nsAString&       aURLStr,
                             eDocTitleDefault aDefType);

  static bool HasFramesetChild(nsIContent* aContent);

  bool     CheckBeforeDestroy();
  nsresult Cancelled();

  nsIPresShell* GetPrintPreviewPresShell() {return mPrtPreview->mPrintObject->mPresShell;}

  float GetPrintPreviewScale() { return mPrtPreview->mPrintObject->
                                        mPresContext->GetPrintPreviewScale(); }

  static nsIPresShell* GetPresShellFor(nsIDocShell* aDocShell);

  // These calls also update the DocViewer
  void SetIsPrinting(bool aIsPrinting);
  bool GetIsPrinting()
  {
    return mIsDoingPrinting;
  }
  void SetIsPrintPreview(bool aIsPrintPreview);
  bool GetIsPrintPreview()
  {
    return mIsDoingPrintPreview;
  }
  void SetIsCreatingPrintPreview(bool aIsCreatingPrintPreview)
  {
    mIsCreatingPrintPreview = aIsCreatingPrintPreview;
  }
  bool GetIsCreatingPrintPreview()
  {
    return mIsCreatingPrintPreview;
  }

  void SetDisallowSelectionPrint(bool aDisallowSelectionPrint)
  {
    mDisallowSelectionPrint = aDisallowSelectionPrint;
  }

private:
  nsPrintJob& operator=(const nsPrintJob& aOther) = delete;

  ~nsPrintJob();

  nsresult CommonPrint(bool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
                       nsIWebProgressListener* aWebProgressListener,
                       nsIDOMDocument* aDoc);

  nsresult DoCommonPrint(bool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
                         nsIWebProgressListener* aWebProgressListener,
                         nsIDOMDocument* aDoc);

  void FirePrintCompletionEvent();
  static nsresult GetSeqFrameAndCountPagesInternal(const mozilla::UniquePtr<nsPrintObject>& aPO,
                                                   nsIFrame*&      aSeqFrame,
                                                   int32_t&        aCount);

  static void MapContentForPO(const mozilla::UniquePtr<nsPrintObject>& aPO,
                              nsIContent* aContent);

  static void MapContentToWebShells(const mozilla::UniquePtr<nsPrintObject>& aRootPO,
                                    const mozilla::UniquePtr<nsPrintObject>& aPO);

  static void SetPrintAsIs(nsPrintObject* aPO, bool aAsIs = true);

  void DisconnectPagePrintTimer();

  nsresult AfterNetworkPrint(bool aHandleError);

  nsresult SetRootView(nsPrintObject* aPO,
                       bool& aDoReturn,
                       bool& aDocumentIsTopLevel,
                       nsSize& aAdjSize);
  nsView* GetParentViewForRoot();
  bool DoSetPixelScale();
  void UpdateZoomRatio(nsPrintObject* aPO, bool aSetPixelScale);
  nsresult ReconstructAndReflow(bool aDoSetPixelScale);
  nsresult UpdateSelectionAndShrinkPrintObject(nsPrintObject* aPO,
                                               bool aDocumentIsTopLevel);
  nsresult InitPrintDocConstruction(bool aHandleError);
  void FirePrintPreviewUpdateEvent();

  void PageDone(nsresult aResult);


  bool mIsCreatingPrintPreview;
  bool mIsDoingPrinting;
  bool mIsDoingPrintPreview;
  bool mProgressDialogIsShown;

  nsCOMPtr<nsIDocumentViewerPrint> mDocViewerPrint;
  nsWeakPtr               mContainer;
  float                   mScreenDPI;

  // We are the primary owner of our nsPrintData member vars.  These vars
  // are refcounted so that functions (e.g. nsPrintData methods) can create
  // temporary owning references when they need to fire a callback that
  // could conceivably destroy this nsPrintJob owner object and all its
  // member-data.
  RefPtr<nsPrintData> mPrt;

  nsPagePrintTimer*       mPagePrintTimer;
  WeakFrame               mPageSeqFrame;

  // Print Preview
  RefPtr<nsPrintData> mPrtPreview;
  RefPtr<nsPrintData> mOldPrtPreview;

  nsCOMPtr<nsIDocument>   mDocument;

  int32_t mLoadCounter;
  bool mDidLoadDataForPrinting;
  bool mIsDestroying;
  bool mDisallowSelectionPrint;
};

#endif // nsPrintJob_h

