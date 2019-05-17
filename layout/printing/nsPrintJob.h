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
class nsIDocumentViewerPrint;
class nsPrintObject;
class nsIDocShell;
class nsIPageSequenceFrame;

namespace mozilla {
class PresShell;
namespace dom {
class Document;
}
}  // namespace mozilla

/**
 * A print job may be instantiated either for printing to an actual physical
 * printer, or for creating a print preview.
 */
class nsPrintJob final : public nsIObserver,
                         public nsIWebProgressListener,
                         public nsSupportsWeakReference {
 public:
  static nsresult GetGlobalPrintSettings(nsIPrintSettings** aPrintSettings);
  static void CloseProgressDialog(nsIWebProgressListener* aWebProgressListener);

  nsPrintJob() = default;

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

  NS_DECL_NSIWEBPROGRESSLISTENER

  // Our nsIWebBrowserPrint implementation defers to these methods.
  nsresult Print(nsIPrintSettings* aPrintSettings,
                 nsIWebProgressListener* aWebProgressListener);
  nsresult PrintPreview(nsIPrintSettings* aPrintSettings,
                        mozIDOMWindowProxy* aChildDOMWin,
                        nsIWebProgressListener* aWebProgressListener);
  bool IsDoingPrint() const { return mIsDoingPrinting; }
  bool IsDoingPrintPreview() const { return mIsDoingPrintPreview; }
  bool IsFramesetDocument() const;
  bool IsIFrameSelected();
  bool IsRangeSelection();
  bool IsFramesetFrameSelected() const;
  /// If the returned value is not greater than zero, an error occurred.
  int32_t GetPrintPreviewNumPages();
  /// Callers are responsible for free'ing aResult.
  nsresult EnumerateDocumentNames(uint32_t* aCount, char16_t*** aResult);
  already_AddRefed<nsIPrintSettings> GetCurrentPrintSettings();

  // This enum tells indicates what the default should be for the title
  // if the title from the document is null
  enum eDocTitleDefault { eDocTitleDefBlank, eDocTitleDefURLDoc };

  void Destroy();
  void DestroyPrintingData();

  nsresult Initialize(nsIDocumentViewerPrint* aDocViewerPrint,
                      nsIDocShell* aContainer,
                      mozilla::dom::Document* aDocument, float aScreenDPI);

  nsresult GetSeqFrameAndCountPages(nsIFrame*& aSeqFrame, int32_t& aCount);

  //
  // The following three methods are used for printing...
  //
  nsresult DocumentReadyForPrinting();
  nsresult GetSelectionDocument(nsIDeviceContextSpec* aDevSpec,
                                mozilla::dom::Document** aNewDoc);

  nsresult SetupToPrintContent();
  nsresult EnablePOsForPrinting();
  nsPrintObject* FindSmallestSTF();

  bool PrintDocContent(const mozilla::UniquePtr<nsPrintObject>& aPO,
                       nsresult& aStatus);
  nsresult DoPrint(const mozilla::UniquePtr<nsPrintObject>& aPO);

  void SetPrintPO(nsPrintObject* aPO, bool aPrint);

  void TurnScriptingOn(bool aDoTurnOn);
  bool CheckDocumentForPPCaching();

  /**
   * Filters out certain user events while Print Preview is open to prevent
   * the user from interacting with the Print Preview document and breaking
   * printing invariants.
   */
  void SuppressPrintPreviewUserEvents();

  // nsIDocumentViewerPrint Printing Methods
  bool HasPrintCallbackCanvas();
  bool PrePrintPage();
  bool PrintPage(nsPrintObject* aPOect, bool& aInRange);
  bool DonePrintingPages(nsPrintObject* aPO, nsresult aResult);

  //---------------------------------------------------------------------
  void BuildDocTree(nsIDocShell* aParentNode,
                    nsTArray<nsPrintObject*>* aDocList,
                    const mozilla::UniquePtr<nsPrintObject>& aPO);
  nsresult ReflowDocList(const mozilla::UniquePtr<nsPrintObject>& aPO,
                         bool aSetPixelScale);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult ReflowPrintObject(const mozilla::UniquePtr<nsPrintObject>& aPO);

  void CheckForChildFrameSets(const mozilla::UniquePtr<nsPrintObject>& aPO);

  void CalcNumPrintablePages(int32_t& aNumPages);
  void ShowPrintProgress(bool aIsForPrinting, bool& aDoNotify);
  nsresult CleanupOnFailure(nsresult aResult, bool aIsPrinting);
  // If FinishPrintPreview() fails, caller may need to reset the state of the
  // object, for example by calling CleanupOnFailure().
  nsresult FinishPrintPreview();
  void SetURLAndTitleOnProgressParams(
      const mozilla::UniquePtr<nsPrintObject>& aPO,
      nsIPrintProgressParams* aParams);
  void EllipseLongString(nsAString& aStr, const uint32_t aLen, bool aDoFront);
  void CleanupDocTitleArray(char16_t**& aArray, int32_t& aCount);

  bool IsThereARangeSelection(nsPIDOMWindowOuter* aDOMWin);

  void FirePrintingErrorEvent(nsresult aPrintError);
  //---------------------------------------------------------------------

  // Timer Methods
  nsresult StartPagePrintTimer(const mozilla::UniquePtr<nsPrintObject>& aPO);

  bool IsWindowsInOurSubTree(nsPIDOMWindowOuter* aDOMWindow) const;
  bool IsThereAnIFrameSelected(nsIDocShell* aDocShell,
                               nsPIDOMWindowOuter* aDOMWin,
                               bool& aIsParentFrameSet);

  // get the currently infocus frame for the document viewer
  already_AddRefed<nsPIDOMWindowOuter> FindFocusedDOMWindow() const;

  void GetDisplayTitleAndURL(const mozilla::UniquePtr<nsPrintObject>& aPO,
                             nsAString& aTitle, nsAString& aURLStr,
                             eDocTitleDefault aDefType);

  bool CheckBeforeDestroy();
  nsresult Cancelled();

  mozilla::PresShell* GetPrintPreviewPresShell() {
    return mPrtPreview->mPrintObject->mPresShell;
  }

  float GetPrintPreviewScale() {
    return mPrtPreview->mPrintObject->mPresContext->GetPrintPreviewScale();
  }

  // These calls also update the DocViewer
  void SetIsPrinting(bool aIsPrinting);
  bool GetIsPrinting() { return mIsDoingPrinting; }
  void SetIsPrintPreview(bool aIsPrintPreview);
  bool GetIsPrintPreview() { return mIsDoingPrintPreview; }
  bool GetIsCreatingPrintPreview() { return mIsCreatingPrintPreview; }

  void SetDisallowSelectionPrint(bool aDisallowSelectionPrint) {
    mDisallowSelectionPrint = aDisallowSelectionPrint;
  }

 private:
  nsPrintJob& operator=(const nsPrintJob& aOther) = delete;

  ~nsPrintJob();

  nsresult CommonPrint(bool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
                       nsIWebProgressListener* aWebProgressListener,
                       mozilla::dom::Document* aDoc);

  nsresult DoCommonPrint(bool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
                         nsIWebProgressListener* aWebProgressListener,
                         mozilla::dom::Document* aDoc);

  void FirePrintCompletionEvent();

  void DisconnectPagePrintTimer();

  /**
   * This method is called to resume printing after all outstanding resources
   * referenced by the static clone have finished loading.  (It is possibly
   * called synchronously if there are no resources to load.)  While a static
   * clone will generally just be able to reference the (already loaded)
   * resources that the original document references, the static clone may
   * reference additional resources that have not previously been loaded
   * (if it has a 'print' style sheet, for example).
   */
  nsresult ResumePrintAfterResourcesLoaded(bool aCleanupOnError);

  nsresult SetRootView(nsPrintObject* aPO, bool& aDoReturn,
                       bool& aDocumentIsTopLevel, nsSize& aAdjSize);
  nsView* GetParentViewForRoot();
  bool DoSetPixelScale();
  void UpdateZoomRatio(nsPrintObject* aPO, bool aSetPixelScale);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult ReconstructAndReflow(bool aDoSetPixelScale);
  nsresult UpdateSelectionAndShrinkPrintObject(nsPrintObject* aPO,
                                               bool aDocumentIsTopLevel);
  nsresult InitPrintDocConstruction(bool aHandleError);
  void FirePrintPreviewUpdateEvent();

  void PageDone(nsresult aResult);

  RefPtr<mozilla::dom::Document> mDocument;
  nsCOMPtr<nsIDocumentViewerPrint> mDocViewerPrint;

  nsWeakPtr mContainer;
  WeakFrame mPageSeqFrame;

  // We are the primary owner of our nsPrintData member vars.  These vars
  // are refcounted so that functions (e.g. nsPrintData methods) can create
  // temporary owning references when they need to fire a callback that
  // could conceivably destroy this nsPrintJob owner object and all its
  // member-data.
  RefPtr<nsPrintData> mPrt;

  // Print Preview
  RefPtr<nsPrintData> mPrtPreview;
  RefPtr<nsPrintData> mOldPrtPreview;

  nsPagePrintTimer* mPagePrintTimer = nullptr;

  float mScreenDPI = 115.0f;
  int32_t mLoadCounter = 0;

  bool mIsCreatingPrintPreview = false;
  bool mIsDoingPrinting = false;
  bool mIsDoingPrintPreview = false;
  bool mProgressDialogIsShown = false;
  bool mDidLoadDataForPrinting = false;
  bool mIsDestroying = false;
  bool mDisallowSelectionPrint = false;
};

#endif  // nsPrintJob_h
