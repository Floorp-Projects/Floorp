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

#include "nsHashKeys.h"
#include "nsIFrame.h"  // For WeakFrame
#include "nsSize.h"
#include "nsTHashtable.h"
#include "nsWeakReference.h"

// Interfaces
#include "nsIObserver.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"

// Classes
class nsIFrame;
class nsIPrintProgressParams;
class nsIPrintSettings;
class nsPrintData;
class nsPagePrintTimer;
class nsIDocShell;
class nsIDocumentViewerPrint;
class nsIFrame;
class nsPrintObject;
class nsIDocShell;
class nsPageSequenceFrame;
class nsPIDOMWindowOuter;
class nsView;

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
  using Document = mozilla::dom::Document;

 public:
  static void CloseProgressDialog(nsIWebProgressListener* aWebProgressListener);

  nsPrintJob();

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
                      nsIDocShell* aDocShell, Document* aOriginalDoc,
                      float aScreenDPI);

  // Our nsIWebBrowserPrint implementation (nsDocumentViewer) defers to the
  // following methods.

  /**
   * May be called immediately after initialization, or after one or more
   * PrintPreview calls.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  Print(Document* aSourceDoc, nsIPrintSettings* aPrintSettings,
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
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  PrintPreview(Document* aSourceDoc, nsIPrintSettings* aPrintSettings,
               nsIWebProgressListener* aWebProgressListener);

  bool IsDoingPrint() const { return mIsDoingPrinting; }
  bool CreatedForPrintPreview() const { return mCreatedForPrintPreview; }
  bool HasEverPrinted() const { return mHasEverPrinted; }
  /// If the returned value is not greater than zero, an error occurred.
  int32_t GetPrintPreviewNumPages();
  already_AddRefed<nsIPrintSettings> GetCurrentPrintSettings();

  // The setters here also update the DocViewer
  void SetIsPrinting(bool aIsPrinting);
  bool GetIsPrinting() const { return mIsDoingPrinting; }
  void SetIsPrintPreview(bool aIsPrintPreview);
  bool GetIsCreatingPrintPreview() const { return mIsCreatingPrintPreview; }

  std::tuple<nsPageSequenceFrame*, int32_t> GetSeqFrameAndCountPages();

  bool PrePrintPage();
  bool PrintPage(nsPrintObject* aPOect, bool& aInRange);
  bool DonePrintingPages(nsPrintObject* aPO, nsresult aResult);

  nsresult CleanupOnFailure(nsresult aResult, bool aIsPrinting);
  // If FinishPrintPreview() fails, caller may need to reset the state of the
  // object, for example by calling CleanupOnFailure().
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult FinishPrintPreview();
  void FirePrintingErrorEvent(nsresult aPrintError);

  bool CheckBeforeDestroy() const;
  mozilla::PresShell* GetPrintPreviewPresShell();
  nsresult Cancel();
  void Destroy();
  void DestroyPrintingData();

 private:
  nsPrintJob& operator=(const nsPrintJob& aOther) = delete;

  ~nsPrintJob();

  MOZ_CAN_RUN_SCRIPT nsresult DocumentReadyForPrinting();
  MOZ_CAN_RUN_SCRIPT nsresult SetupToPrintContent();
  nsresult EnablePOsForPrinting();
  nsPrintObject* FindSmallestSTF();

  bool PrintDocContent(const mozilla::UniquePtr<nsPrintObject>& aPO,
                       nsresult& aStatus);
  nsresult DoPrint(const mozilla::UniquePtr<nsPrintObject>& aPO);

  /**
   * Filters out certain user events while Print Preview is open to prevent
   * the user from interacting with the Print Preview document and breaking
   * printing invariants.
   */
  void SuppressPrintPreviewUserEvents();

  nsresult ReflowDocList(const mozilla::UniquePtr<nsPrintObject>& aPO,
                         bool aSetPixelScale);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult ReflowPrintObject(const mozilla::UniquePtr<nsPrintObject>& aPO);

  void CalcNumPrintablePages(int32_t& aNumPages);
  void ShowPrintProgress(bool aIsForPrinting, bool& aDoNotify);
  void SetURLAndTitleOnProgressParams(
      const mozilla::UniquePtr<nsPrintObject>& aPO,
      nsIPrintProgressParams* aParams);
  void EllipseLongString(nsAString& aStr, const uint32_t aLen, bool aDoFront);

  nsresult StartPagePrintTimer(const mozilla::UniquePtr<nsPrintObject>& aPO);

  bool IsWindowsInOurSubTree(nsPIDOMWindowOuter* aDOMWindow) const;

  /**
   * @return The document from the focused windows for a document viewer.
   */
  Document* FindFocusedDocument() const;

  /// Customizes the behaviour of GetDisplayTitleAndURL.
  enum class DocTitleDefault : uint32_t { eDocURLElseFallback, eFallback };

  /**
   * Gets the title and URL of the document for display in save-to-PDF dialogs,
   * print spooler lists and page headers/footers.  This will get the title/URL
   * from the PrintSettings, if set, otherwise it will get them from the
   * document.
   *
   * For the title specifically, if a value is not provided by the settings
   * object or the document then, if eDocURLElseFallback is passed, the document
   * URL will be returned as the title if it's non-empty (which should always be
   * the case).  Otherwise a non-empty fallback title will be returned.
   */
  static void GetDisplayTitleAndURL(Document& aDoc, nsIPrintSettings* aSettings,
                                    DocTitleDefault aTitleDefault,
                                    nsAString& aTitle, nsAString& aURLStr);

  MOZ_CAN_RUN_SCRIPT nsresult CommonPrint(
      bool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
      nsIWebProgressListener* aWebProgressListener, Document* aSourceDoc);

  MOZ_CAN_RUN_SCRIPT nsresult DoCommonPrint(
      bool aIsPrintPreview, nsIPrintSettings* aPrintSettings,
      nsIWebProgressListener* aWebProgressListener, Document* aSourceDoc);

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
  MOZ_CAN_RUN_SCRIPT nsresult
  MaybeResumePrintAfterResourcesLoaded(bool aCleanupOnError);

  bool ShouldResumePrint() const;

  nsresult SetRootView(nsPrintObject* aPO, bool& aDoReturn,
                       bool& aDocumentIsTopLevel, nsSize& aAdjSize);
  nsView* GetParentViewForRoot();
  bool DoSetPixelScale();
  void UpdateZoomRatio(nsPrintObject* aPO, bool aSetPixelScale);
  MOZ_CAN_RUN_SCRIPT nsresult ReconstructAndReflow(bool aDoSetPixelScale);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult UpdateSelectionAndShrinkPrintObject(
      nsPrintObject* aPO, bool aDocumentIsTopLevel);
  MOZ_CAN_RUN_SCRIPT nsresult InitPrintDocConstruction(bool aHandleError);
  void FirePrintPreviewUpdateEvent();

  void PageDone(nsresult aResult);

  // The document that we were originally created for in order to print it or
  // create a print preview of it.  This may belong to mDocViewerPrint or may
  // belong to a different docViewer in a different docShell.  In reality, this
  // also may not be the original document that the user selected to print (see
  // the comment documenting Initialize() above).
  RefPtr<Document> mOriginalDoc;

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

  // The nsPrintData for our last print preview (replaced every time the
  // user changes settings in the print preview window).
  // Note: Our new print preview nsPrintData is stored in mPtr until we move it
  // to mPrtPreview once we've finish creating the print preview.
  RefPtr<nsPrintData> mPrtPreview;

  nsPagePrintTimer* mPagePrintTimer = nullptr;

  float mScreenDPI = 115.0f;

  bool mCreatedForPrintPreview = false;
  bool mIsCreatingPrintPreview = false;
  bool mIsDoingPrinting = false;
  bool mHasEverPrinted = false;
  bool mProgressDialogIsShown = false;
  bool mDidLoadDataForPrinting = false;
  bool mIsDestroying = false;
  bool mDisallowSelectionPrint = false;
  bool mIsForModalWindow = false;
};

#endif  // nsPrintJob_h
