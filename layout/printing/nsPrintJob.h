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

  /**
   * Initialize for printing, or for creating a print preview document.
   *
   * aDocViewerPrint owns us.
   *
   * When called in preparation for printing, aOriginalDoc is aDocViewerPrint's
   * document.  The document/viewer may be for a sub-document (an iframe).
   *
   * When called in preparation for print preview, aOriginalDoc belongs to a
   * different docViewer, in a different docShell, in a different TabGroup.
   * In this case our aDocViewerPrint is the docViewer for the about:blank
   * document in a new tab that the Firefox frontend code has created in
   * preparation for PrintPreview to generate a print preview document in it.
   *
   * NOTE: In the case we're called for print preview, aOriginalDoc actually
   * may not be the original document that the user selected to print.  It
   * is not the actual original document in the case when the user chooses to
   * display a simplified version of a print preview document.  In that
   * instance the Firefox frontend code creates a second print preview tab,
   * with a new docViewer and nsPrintJob, and passes the previous print preview
   * document as aOriginalDoc (it doesn't want to pass the actual original
   * document since it may have mutated)!
   */
  nsresult Initialize(nsIDocumentViewerPrint* aDocViewerPrint,
                      nsIDocShell* aDocShell,
                      mozilla::dom::Document* aOriginalDoc, float aScreenDPI);

  // Our nsIWebBrowserPrint implementation (nsDocumentViewer) defers to the
  // following methods.

  /**
   * May be called immediately after initialization, or after one or more
   * PrintPreview calls.
   */
  nsresult Print(nsIPrintSettings* aPrintSettings,
                 nsIWebProgressListener* aWebProgressListener);

  /**
   * Generates a new print preview document and replaces our docViewer's
   * document with it.  (Note that this breaks the normal invariant that a
   * Document and its nsDocumentViewer have an unchanging 1:1 relationship.)
   *
   * This may be called multiple times on the same instance in order to
   * recreate the print preview document to take account of settings that the
   * user has changed in the print preview interface.  In this case aSourceDoc
   * is actually our docViewer's current document!
   */
  nsresult PrintPreview(mozilla::dom::Document* aSourceDoc,
                        nsIPrintSettings* aPrintSettings,
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
                       mozilla::dom::Document* aSourceDoc);

  nsresult DoCommonPrint(bool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
                         nsIWebProgressListener* aWebProgressListener,
                         mozilla::dom::Document* aSourceDoc);

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

  // The document that we were originally created for in order to print it or
  // create a print preview of it.  This may belong to mDocViewerPrint or may
  // belong to a different docViewer in a different docShell.  In reality, this
  // also may not be the original document that the user selected to print (see
  // the comment documenting Initialize() above).
  RefPtr<mozilla::dom::Document> mOriginalDoc;

  // The docViewer that owns us, and its docShell.
  nsCOMPtr<nsIDocumentViewerPrint> mDocViewerPrint;
  nsWeakPtr mDocShell;

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
