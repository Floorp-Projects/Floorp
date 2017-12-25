/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintJob.h"

#include "nsIStringBundle.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/CustomEvent.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIURI.h"
#include "nsITextToSubURI.h"
#include "nsError.h"

#include "nsView.h"
#include <algorithm>

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIPrintSession.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"

static const char sPrintSettingsServiceContractID[] = "@mozilla.org/gfx/printsettings-service;1";

// Printing Events
#include "nsPrintPreviewListener.h"
#include "nsThreadUtils.h"

// Printing
#include "nsIWebBrowserPrint.h"

// Print Preview
#include "imgIContainer.h" // image animation mode constants
#include "nsIWebBrowserPrint.h" // needed for PrintPreview Navigation constants

// Print Progress
#include "nsIPrintProgress.h"
#include "nsIPrintProgressParams.h"
#include "nsIObserver.h"

// Print error dialog
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"

// Printing Prompts
#include "nsIPrintingPromptService.h"
static const char kPrintingPromptService[] = "@mozilla.org/embedcomp/printingprompt-service;1";

// Printing Timer
#include "nsPagePrintTimer.h"

// FrameSet
#include "nsIDocument.h"
#include "nsIDocumentInlines.h"

// Focus
#include "nsISelectionController.h"

// Misc
#include "gfxContext.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "nsISupportsUtils.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentObserver.h"
#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMRange.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsContentUtils.h"
#include "nsIPresShell.h"
#include "nsLayoutStylesheetCache.h"
#include "nsLayoutUtils.h"
#include "mozilla/Preferences.h"
#include "Text.h"

#include "nsWidgetsCID.h"
#include "nsIDeviceContextSpec.h"
#include "nsDeviceContextSpecProxy.h"
#include "nsViewManager.h"
#include "nsView.h"

#include "nsIPageSequenceFrame.h"
#include "nsIURL.h"
#include "nsIContentViewerEdit.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIBaseWindow.h"
#include "nsILayoutHistoryState.h"
#include "nsFrameManager.h"
#include "mozilla/ReflowInput.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewerPrint.h"

#include "nsFocusManager.h"
#include "nsRange.h"
#include "nsCDefaultURIFixup.h"
#include "nsIURIFixup.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLFrameElement.h"
#include "nsContentList.h"
#include "nsIChannel.h"
#include "xpcpublic.h"
#include "nsVariant.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

//-----------------------------------------------------
// PR LOGGING
#include "mozilla/Logging.h"

#ifdef DEBUG
// PR_LOGGING is force to always be on (even in release builds)
// but we only want some of it on,
//#define EXTENDED_DEBUG_PRINTING
#endif

#define DUMP_LAYOUT_LEVEL 9 // this turns on the dumping of each doucment's layout info

#ifndef PR_PL
static mozilla::LazyLogModule gPrintingLog("printing")

#define PR_PL(_p1)  MOZ_LOG(gPrintingLog, mozilla::LogLevel::Debug, _p1);
#endif

#ifdef EXTENDED_DEBUG_PRINTING
static uint32_t gDumpFileNameCnt   = 0;
static uint32_t gDumpLOFileNameCnt = 0;
#endif

#define PRT_YESNO(_p) ((_p)?"YES":"NO")
static const char * gFrameTypesStr[]       = {"eDoc", "eFrame", "eIFrame", "eFrameSet"};
static const char * gPrintFrameTypeStr[]   = {"kNoFrames", "kFramesAsIs", "kSelectedFrame", "kEachFrameSep"};
static const char * gFrameHowToEnableStr[] = {"kFrameEnableNone", "kFrameEnableAll", "kFrameEnableAsIsAndEach"};
static const char * gPrintRangeStr[]       = {"kRangeAllPages", "kRangeSpecifiedPageRange", "kRangeSelection", "kRangeFocusFrame"};

// This processes the selection on aOrigDoc and creates an inverted selection on
// aDoc, which it then deletes. If the start or end of the inverted selection
// ranges occur in text nodes then an ellipsis is added.
static nsresult DeleteUnselectedNodes(nsIDocument* aOrigDoc, nsIDocument* aDoc);

#ifdef EXTENDED_DEBUG_PRINTING
// Forward Declarations
static void DumpPrintObjectsListStart(const char * aStr, nsTArray<nsPrintObject*> * aDocList);
static void DumpPrintObjectsTree(nsPrintObject * aPO, int aLevel= 0, FILE* aFD = nullptr);
static void DumpPrintObjectsTreeLayout(nsPrintObject * aPO,nsDeviceContext * aDC, int aLevel= 0, FILE * aFD = nullptr);

#define DUMP_DOC_LIST(_title) DumpPrintObjectsListStart((_title), mPrt->mPrintDocList);
#define DUMP_DOC_TREE DumpPrintObjectsTree(mPrt->mPrintObject.get());
#define DUMP_DOC_TREELAYOUT DumpPrintObjectsTreeLayout(mPrt->mPrintObject.get(), mPrt->mPrintDC);
#else
#define DUMP_DOC_LIST(_title)
#define DUMP_DOC_TREE
#define DUMP_DOC_TREELAYOUT
#endif

class nsScriptSuppressor
{
public:
  explicit nsScriptSuppressor(nsPrintJob* aPrintJob)
    : mPrintJob(aPrintJob)
    , mSuppressed(false)
  {}

  ~nsScriptSuppressor() { Unsuppress(); }

  void Suppress()
  {
    if (mPrintJob) {
      mSuppressed = true;
      mPrintJob->TurnScriptingOn(false);
    }
  }

  void Unsuppress()
  {
    if (mPrintJob && mSuppressed) {
      mPrintJob->TurnScriptingOn(true);
    }
    mSuppressed = false;
  }

  void Disconnect() { mPrintJob = nullptr; }
protected:
  RefPtr<nsPrintJob>      mPrintJob;
  bool                    mSuppressed;
};

NS_IMPL_ISUPPORTS(nsPrintJob, nsIWebProgressListener,
                  nsISupportsWeakReference, nsIObserver)

//---------------------------------------------------
//-- nsPrintJob Class Impl
//---------------------------------------------------
nsPrintJob::nsPrintJob()
  : mIsCreatingPrintPreview(false)
  , mIsDoingPrinting(false)
  , mIsDoingPrintPreview(false)
  , mProgressDialogIsShown(false)
  , mScreenDPI(115.0f)
  , mPagePrintTimer(nullptr)
  , mLoadCounter(0)
  , mDidLoadDataForPrinting(false)
  , mIsDestroying(false)
  , mDisallowSelectionPrint(false)
{
}

//-------------------------------------------------------
nsPrintJob::~nsPrintJob()
{
  Destroy(); // for insurance
  DisconnectPagePrintTimer();
}

//-------------------------------------------------------
void
nsPrintJob::Destroy()
{
  if (mIsDestroying) {
    return;
  }
  mIsDestroying = true;

  mPrt = nullptr;

#ifdef NS_PRINT_PREVIEW
  mPrtPreview = nullptr;
  mOldPrtPreview = nullptr;
#endif
  mDocViewerPrint = nullptr;
}

//-------------------------------------------------------
void
nsPrintJob::DestroyPrintingData()
{
  mPrt = nullptr;
}

//---------------------------------------------------------------------------------
//-- Section: Methods needed by the DocViewer
//---------------------------------------------------------------------------------

//--------------------------------------------------------
nsresult
nsPrintJob::Initialize(nsIDocumentViewerPrint* aDocViewerPrint,
                       nsIDocShell*            aContainer,
                       nsIDocument*            aDocument,
                       float                   aScreenDPI)
{
  NS_ENSURE_ARG_POINTER(aDocViewerPrint);
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_ENSURE_ARG_POINTER(aDocument);

  mDocViewerPrint = aDocViewerPrint;
  mContainer      = do_GetWeakReference(aContainer);
  mDocument       = aDocument;
  mScreenDPI      = aScreenDPI;

  return NS_OK;
}

//-------------------------------------------------------
bool
nsPrintJob::CheckBeforeDestroy()
{
  if (mPrt && mPrt->mPreparingForPrint) {
    mPrt->mDocWasToBeDestroyed = true;
    return true;
  }
  return false;
}

//-------------------------------------------------------
nsresult
nsPrintJob::Cancelled()
{
  if (mPrt && mPrt->mPrintSettings) {
    return mPrt->mPrintSettings->SetIsCancelled(true);
  }
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------
// Install our event listeners on the document to prevent
// some events from being processed while in PrintPreview
//
// No return code - if this fails, there isn't much we can do
void
nsPrintJob::InstallPrintPreviewListener()
{
  if (!mPrt->mPPEventListeners) {
    nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mContainer);
    if (!docShell) {
      return;
    }

    if (nsPIDOMWindowOuter* win = docShell->GetWindow()) {
      nsCOMPtr<EventTarget> target = win->GetFrameElementInternal();
      mPrt->mPPEventListeners = new nsPrintPreviewListener(target);
      mPrt->mPPEventListeners->AddListeners();
    }
  }
}

//----------------------------------------------------------------------
nsresult
nsPrintJob::GetSeqFrameAndCountPagesInternal(const UniquePtr<nsPrintObject>& aPO,
                                             nsIFrame*&    aSeqFrame,
                                             int32_t&      aCount)
{
  NS_ENSURE_ARG_POINTER(aPO);

  // This is sometimes incorrectly called before the pres shell has been created
  // (bug 1141756). MOZ_DIAGNOSTIC_ASSERT so we'll still see the crash in
  // Nightly/Aurora in case the other patch fixes this.
  if (!aPO->mPresShell) {
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "GetSeqFrameAndCountPages needs a non-null pres shell");
    return NS_ERROR_FAILURE;
  }

  // Finds the SimplePageSequencer frame
  nsIPageSequenceFrame* seqFrame = aPO->mPresShell->GetPageSequenceFrame();
  aSeqFrame = do_QueryFrame(seqFrame);
  if (!aSeqFrame) {
    return NS_ERROR_FAILURE;
  }

  // count the total number of pages
  aCount = aSeqFrame->PrincipalChildList().GetLength();

  return NS_OK;
}

//-----------------------------------------------------------------
nsresult
nsPrintJob::GetSeqFrameAndCountPages(nsIFrame*& aSeqFrame, int32_t& aCount)
{
  MOZ_ASSERT(mPrtPreview);
  // Guarantee that mPrintPreview->mPrintObject won't be deleted during a call
  // of GetSeqFrameAndCountPagesInternal().
  RefPtr<nsPrintData> printDataForPrintPreview = mPrtPreview;
  return GetSeqFrameAndCountPagesInternal(
           printDataForPrintPreview->mPrintObject, aSeqFrame, aCount);
}
//---------------------------------------------------------------------------------
//-- Done: Methods needed by the DocViewer
//---------------------------------------------------------------------------------


//---------------------------------------------------------------------------------
//-- Section: nsIWebBrowserPrint
//---------------------------------------------------------------------------------

// Foward decl for Debug Helper Functions
#ifdef EXTENDED_DEBUG_PRINTING
static int RemoveFilesInDir(const char * aDir);
static void GetDocTitleAndURL(nsPrintObject* aPO, char *& aDocStr, char *& aURLStr);
static void DumpPrintObjectsTree(nsPrintObject * aPO, int aLevel, FILE* aFD);
static void DumpPrintObjectsList(nsTArray<nsPrintObject*> * aDocList);
static void RootFrameList(nsPresContext* aPresContext, FILE* out, int32_t aIndent);
static void DumpViews(nsIDocShell* aDocShell, FILE* out);
static void DumpLayoutData(char* aTitleStr, char* aURLStr,
                           nsPresContext* aPresContext,
                           nsDeviceContext * aDC, nsIFrame * aRootFrame,
                           nsIDocShell * aDocShell, FILE* aFD);
#endif

//--------------------------------------------------------------------------------

nsresult
nsPrintJob::CommonPrint(bool                    aIsPrintPreview,
                        nsIPrintSettings*       aPrintSettings,
                        nsIWebProgressListener* aWebProgressListener,
                        nsIDOMDocument* aDoc)
{
  // Callers must hold a strong reference to |this| to ensure that we stay
  // alive for the duration of this method, because our main owning reference
  // (on nsDocumentViewer) might be cleared during this function (if we cause
  // script to run and it cancels the print operation).

  nsresult rv = DoCommonPrint(aIsPrintPreview, aPrintSettings,
                              aWebProgressListener, aDoc);
  if (NS_FAILED(rv)) {
    if (aIsPrintPreview) {
      SetIsCreatingPrintPreview(false);
      SetIsPrintPreview(false);
    } else {
      SetIsPrinting(false);
    }
    if (mProgressDialogIsShown)
      CloseProgressDialog(aWebProgressListener);
    if (rv != NS_ERROR_ABORT && rv != NS_ERROR_OUT_OF_MEMORY) {
      FirePrintingErrorEvent(rv);
    }
    mPrt = nullptr;
  }

  return rv;
}

nsresult
nsPrintJob::DoCommonPrint(bool                    aIsPrintPreview,
                          nsIPrintSettings*       aPrintSettings,
                          nsIWebProgressListener* aWebProgressListener,
                          nsIDOMDocument*         aDoc)
{
  nsresult rv;

  if (aIsPrintPreview) {
    // The WebProgressListener can be QI'ed to nsIPrintingPromptService
    // then that means the progress dialog is already being shown.
    nsCOMPtr<nsIPrintingPromptService> pps(do_QueryInterface(aWebProgressListener));
    mProgressDialogIsShown = pps != nullptr;

    if (mIsDoingPrintPreview) {
      mOldPrtPreview = Move(mPrtPreview);
    }
  } else {
    mProgressDialogIsShown = false;
  }

  // Grab the new instance with local variable to guarantee that it won't be
  // deleted during this method.
  mPrt = new nsPrintData(aIsPrintPreview ? nsPrintData::eIsPrintPreview :
                                           nsPrintData::eIsPrinting);
  RefPtr<nsPrintData> printData = mPrt;

  // if they don't pass in a PrintSettings, then get the Global PS
  printData->mPrintSettings = aPrintSettings;
  if (!printData->mPrintSettings) {
    rv = GetGlobalPrintSettings(getter_AddRefs(printData->mPrintSettings));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = CheckForPrinters(printData->mPrintSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  printData->mPrintSettings->SetIsCancelled(false);
  printData->mPrintSettings->GetShrinkToFit(&printData->mShrinkToFit);

  if (aIsPrintPreview) {
    SetIsCreatingPrintPreview(true);
    SetIsPrintPreview(true);
    nsCOMPtr<nsIContentViewer> viewer =
      do_QueryInterface(mDocViewerPrint);
    if (viewer) {
      viewer->SetTextZoom(1.0f);
      viewer->SetFullZoom(1.0f);
      viewer->SetMinFontSize(0);
    }
  }

  // Create a print session and let the print settings know about it.
  // Don't overwrite an existing print session.
  // The print settings hold an nsWeakPtr to the session so it does not
  // need to be cleared from the settings at the end of the job.
  // XXX What lifetime does the printSession need to have?
  nsCOMPtr<nsIPrintSession> printSession;
  bool remotePrintJobListening = false;
  if (!aIsPrintPreview) {
    rv = printData->mPrintSettings->GetPrintSession(
                                      getter_AddRefs(printSession));
    if (NS_FAILED(rv) || !printSession) {
      printSession = do_CreateInstance("@mozilla.org/gfx/printsession;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      printData->mPrintSettings->SetPrintSession(printSession);
    } else {
      RefPtr<mozilla::layout::RemotePrintJobChild> remotePrintJob;
      printSession->GetRemotePrintJob(getter_AddRefs(remotePrintJob));
      if (NS_SUCCEEDED(rv) && remotePrintJob) {
        // If we have a RemotePrintJob add it to the print progress listeners,
        // so it can forward to the parent.
        printData->mPrintProgressListeners.AppendElement(remotePrintJob);
        remotePrintJobListening = true;
      }
    }

  }

  if (aWebProgressListener != nullptr) {
    printData->mPrintProgressListeners.AppendObject(aWebProgressListener);
  }

  // Get the currently focused window and cache it
  // because the Print Dialog will "steal" focus and later when you try
  // to get the currently focused windows it will be nullptr
  printData->mCurrentFocusWin = FindFocusedDOMWindow();

  // Check to see if there is a "regular" selection
  bool isSelection = IsThereARangeSelection(printData->mCurrentFocusWin);

  // Get the docshell for this documentviewer
  nsCOMPtr<nsIDocShell> webContainer(do_QueryReferent(mContainer, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  {
    if (aIsPrintPreview) {
      nsCOMPtr<nsIContentViewer> viewer;
      webContainer->GetContentViewer(getter_AddRefs(viewer));
      if (viewer && viewer->GetDocument() && viewer->GetDocument()->IsShowing()) {
        viewer->GetDocument()->OnPageHide(false, nullptr);
      }
    }

    nsAutoScriptBlocker scriptBlocker;
    printData->mPrintObject = MakeUnique<nsPrintObject>();
    rv = printData->mPrintObject->Init(webContainer, aDoc, aIsPrintPreview);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(printData->mPrintDocList.AppendElement(
                                              printData->mPrintObject.get()),
                   NS_ERROR_OUT_OF_MEMORY);

    printData->mIsParentAFrameSet = IsParentAFrameSet(webContainer);
    printData->mPrintObject->mFrameType =
      printData->mIsParentAFrameSet ? eFrameSet : eDoc;

    // Build the "tree" of PrintObjects
    BuildDocTree(printData->mPrintObject->mDocShell, &printData->mPrintDocList,
                 printData->mPrintObject);
  }

  // The nsAutoScriptBlocker above will now have been destroyed, which may
  // cause our print/print-preview operation to finish. In this case, we
  // should immediately return an error code so that the root caller knows
  // it shouldn't continue to do anything with this instance.
  if (mIsDestroying || (aIsPrintPreview && !GetIsCreatingPrintPreview())) {
    return NS_ERROR_FAILURE;
  }

  if (!aIsPrintPreview) {
    SetIsPrinting(true);
  }

  // XXX This isn't really correct...
  if (!printData->mPrintObject->mDocument ||
      !printData->mPrintObject->mDocument->GetRootElement())
    return NS_ERROR_GFX_PRINTER_STARTDOC;

  // Create the linkage from the sub-docs back to the content element
  // in the parent document
  MapContentToWebShells(printData->mPrintObject, printData->mPrintObject);

  printData->mIsIFrameSelected =
    IsThereAnIFrameSelected(webContainer, printData->mCurrentFocusWin,
                            printData->mIsParentAFrameSet);

  // Setup print options for UI
  if (printData->mIsParentAFrameSet) {
    if (printData->mCurrentFocusWin) {
      printData->mPrintSettings->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableAll);
    } else {
      printData->mPrintSettings->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableAsIsAndEach);
    }
  } else {
    printData->mPrintSettings->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableNone);
  }
  // Now determine how to set up the Frame print UI
  printData->mPrintSettings->SetPrintOptions(
                               nsIPrintSettings::kEnableSelectionRB,
                               isSelection || printData->mIsIFrameSelected);

  bool printingViaParent = XRE_IsContentProcess() &&
                           Preferences::GetBool("print.print_via_parent");
  nsCOMPtr<nsIDeviceContextSpec> devspec;
  if (printingViaParent) {
    devspec = new nsDeviceContextSpecProxy();
  } else {
    devspec = do_CreateInstance("@mozilla.org/gfx/devicecontextspec;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsScriptSuppressor scriptSuppressor(this);
  // If printing via parent we still call ShowPrintDialog even for print preview
  // because we use that to retrieve the print settings from the printer.
  // The dialog is not shown, but this means we don't need to access the printer
  // driver from the child, which causes sandboxing issues.
  if (!aIsPrintPreview || printingViaParent) {
    scriptSuppressor.Suppress();
    bool printSilently;
    printData->mPrintSettings->GetPrintSilent(&printSilently);

    // Check prefs for a default setting as to whether we should print silently
    printSilently =
      Preferences::GetBool("print.always_print_silent", printSilently);

    // Ask dialog to be Print Shown via the Plugable Printing Dialog Service
    // This service is for the Print Dialog and the Print Progress Dialog
    // If printing silently or you can't get the service continue on
    // If printing via the parent then we need to confirm that the pref is set
    // and get a remote print job, but the parent won't display a prompt.
    if (!printSilently || printingViaParent) {
      nsCOMPtr<nsIPrintingPromptService> printPromptService(do_GetService(kPrintingPromptService));
      if (printPromptService) {
        nsPIDOMWindowOuter* domWin = nullptr;
        // We leave domWin as nullptr to indicate a call for print preview.
        if (!aIsPrintPreview) {
          domWin = mDocument->GetWindow();
          NS_ENSURE_TRUE(domWin, NS_ERROR_FAILURE);
        }

        // Platforms not implementing a given dialog for the service may
        // return NS_ERROR_NOT_IMPLEMENTED or an error code.
        //
        // NS_ERROR_NOT_IMPLEMENTED indicates they want default behavior
        // Any other error code means we must bail out
        //
        nsCOMPtr<nsIWebBrowserPrint> wbp(do_QueryInterface(mDocViewerPrint));
        rv = printPromptService->ShowPrintDialog(domWin, wbp,
                                                 printData->mPrintSettings);
        //
        // ShowPrintDialog triggers an event loop which means we can't assume
        // that the state of this->{anything} matches the state we've checked
        // above. Including that a given {thing} is non null.
        if (NS_WARN_IF(mPrt != printData)) {
          return NS_ERROR_FAILURE;
        }

        if (NS_SUCCEEDED(rv)) {
          // since we got the dialog and it worked then make sure we
          // are telling GFX we want to print silent
          printSilently = true;

          if (printData->mPrintSettings && !aIsPrintPreview) {
            // The user might have changed shrink-to-fit in the print dialog, so update our copy of its state
            printData->mPrintSettings->GetShrinkToFit(&printData->mShrinkToFit);

            // If we haven't already added the RemotePrintJob as a listener,
            // add it now if there is one.
            if (!remotePrintJobListening) {
              RefPtr<mozilla::layout::RemotePrintJobChild> remotePrintJob;
              printSession->GetRemotePrintJob(getter_AddRefs(remotePrintJob));
              if (NS_SUCCEEDED(rv) && remotePrintJob) {
                printData->mPrintProgressListeners.AppendElement(
                                                     remotePrintJob);
                remotePrintJobListening = true;
              }
            }
          }
        } else if (rv == NS_ERROR_NOT_IMPLEMENTED) {
          // This means the Dialog service was there,
          // but they choose not to implement this dialog and
          // are looking for default behavior from the toolkit
          rv = NS_OK;
        }
      } else {
        // No dialog service available
        rv = NS_ERROR_NOT_IMPLEMENTED;
      }
    } else {
      // Call any code that requires a run of the event loop.
      rv = printData->mPrintSettings->SetupSilentPrinting();
    }
    // Check explicitly for abort because it's expected
    if (rv == NS_ERROR_ABORT)
      return rv;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = devspec->Init(nullptr, printData->mPrintSettings, aIsPrintPreview);
  NS_ENSURE_SUCCESS(rv, rv);

  printData->mPrintDC = new nsDeviceContext();
  rv = printData->mPrintDC->InitForPrinting(devspec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (XRE_IsParentProcess() && !printData->mPrintDC->IsSyncPagePrinting()) {
    RefPtr<nsPrintJob> self(this);
    printData->mPrintDC->RegisterPageDoneCallback([self](nsresult aResult) { self->PageDone(aResult); });
  }

  if (aIsPrintPreview) {
    printData->mPrintSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);

    // override any UI that wants to PrintPreview any selection or page range
    // we want to view every page in PrintPreview each time
    printData->mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
  } else {
    // Always check and set the print settings first and then fall back
    // onto the PrintService if there isn't a PrintSettings
    //
    // Posiible Usage values:
    //   nsIPrintSettings::kUseInternalDefault
    //   nsIPrintSettings::kUseSettingWhenPossible
    //
    // NOTE: The consts are the same for PrintSettings and PrintSettings
    int16_t printFrameTypeUsage = nsIPrintSettings::kUseSettingWhenPossible;
    printData->mPrintSettings->GetPrintFrameTypeUsage(&printFrameTypeUsage);

    // Ok, see if we are going to use our value and override the default
    if (printFrameTypeUsage == nsIPrintSettings::kUseSettingWhenPossible) {
      // Get the Print Options/Settings PrintFrameType to see what is preferred
      int16_t printFrameType = nsIPrintSettings::kEachFrameSep;
      printData->mPrintSettings->GetPrintFrameType(&printFrameType);

      // Don't let anybody do something stupid like try to set it to
      // kNoFrames when we are printing a FrameSet
      if (printFrameType == nsIPrintSettings::kNoFrames) {
        printData->mPrintFrameType = nsIPrintSettings::kEachFrameSep;
        printData->mPrintSettings->SetPrintFrameType(
                                     printData->mPrintFrameType);
      } else {
        // First find out from the PrinService what options are available
        // to us for Printing FrameSets
        int16_t howToEnableFrameUI;
        printData->mPrintSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);
        if (howToEnableFrameUI != nsIPrintSettings::kFrameEnableNone) {
          switch (howToEnableFrameUI) {
          case nsIPrintSettings::kFrameEnableAll:
            printData->mPrintFrameType = printFrameType;
            break;

          case nsIPrintSettings::kFrameEnableAsIsAndEach:
            if (printFrameType != nsIPrintSettings::kSelectedFrame) {
              printData->mPrintFrameType = printFrameType;
            } else { // revert back to a good value
              printData->mPrintFrameType = nsIPrintSettings::kEachFrameSep;
            }
            break;
          } // switch
          printData->mPrintSettings->SetPrintFrameType(
                                       printData->mPrintFrameType);
        }
      }
    } else {
      printData->mPrintSettings->GetPrintFrameType(&printData->mPrintFrameType);
    }
  }

  if (printData->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    CheckForChildFrameSets(printData->mPrintObject);
  }

  if (NS_FAILED(EnablePOsForPrinting())) {
    return NS_ERROR_FAILURE;
  }

  // Attach progressListener to catch network requests.
  nsCOMPtr<nsIWebProgress> webProgress =
    do_QueryInterface(printData->mPrintObject->mDocShell);
  webProgress->AddProgressListener(
    static_cast<nsIWebProgressListener*>(this),
    nsIWebProgress::NOTIFY_STATE_REQUEST);

  mLoadCounter = 0;
  mDidLoadDataForPrinting = false;

  if (aIsPrintPreview) {
    bool notifyOnInit = false;
    ShowPrintProgress(false, notifyOnInit);

    // Very important! Turn Off scripting
    TurnScriptingOn(false);

    if (!notifyOnInit) {
      InstallPrintPreviewListener();
      rv = InitPrintDocConstruction(false);
    } else {
      rv = NS_OK;
    }
  } else {
    bool doNotify;
    ShowPrintProgress(true, doNotify);
    if (!doNotify) {
      // Print listener setup...
      printData->OnStartPrinting();

      rv = InitPrintDocConstruction(false);
    }
  }

  // We will enable scripting later after printing has finished.
  scriptSuppressor.Disconnect();

  return NS_OK;
}

//---------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::Print(nsIPrintSettings*       aPrintSettings,
                  nsIWebProgressListener* aWebProgressListener)
{
  // If we have a print preview document, use that instead of the original
  // mDocument. That way animated images etc. get printed using the same state
  // as in print preview.
  nsCOMPtr<nsIDOMDocument> doc =
    do_QueryInterface(mPrtPreview && mPrtPreview->mPrintObject ?
                        mPrtPreview->mPrintObject->mDocument : mDocument);

  return CommonPrint(false, aPrintSettings, aWebProgressListener, doc);
}

NS_IMETHODIMP
nsPrintJob::PrintPreview(nsIPrintSettings* aPrintSettings,
                         mozIDOMWindowProxy* aChildDOMWin,
                         nsIWebProgressListener* aWebProgressListener)
{
  // Get the DocShell and see if it is busy
  // (We can't Print Preview this document if it is still busy)
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
  NS_ENSURE_STATE(docShell);

  uint32_t busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
  if (NS_FAILED(docShell->GetBusyFlags(&busyFlags)) ||
      busyFlags != nsIDocShell::BUSY_FLAGS_NONE) {
    CloseProgressDialog(aWebProgressListener);
    FirePrintingErrorEvent(NS_ERROR_GFX_PRINTER_DOC_IS_BUSY);
    return NS_ERROR_FAILURE;
  }

  auto* window = nsPIDOMWindowOuter::From(aChildDOMWin);
  NS_ENSURE_STATE(window);
  nsCOMPtr<nsIDocument> doc = window->GetDoc();
  NS_ENSURE_STATE(doc);
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
  MOZ_ASSERT(domDoc);

  // Document is not busy -- go ahead with the Print Preview
  return CommonPrint(true, aPrintSettings, aWebProgressListener, domDoc);
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::GetIsFramesetDocument(bool* aIsFramesetDocument)
{
  nsCOMPtr<nsIDocShell> webContainer(do_QueryReferent(mContainer));
  *aIsFramesetDocument = IsParentAFrameSet(webContainer);
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::GetIsIFrameSelected(bool* aIsIFrameSelected)
{
  *aIsIFrameSelected = false;

  // Get the docshell for this documentviewer
  nsCOMPtr<nsIDocShell> webContainer(do_QueryReferent(mContainer));
  // Get the currently focused window
  nsCOMPtr<nsPIDOMWindowOuter> currentFocusWin = FindFocusedDOMWindow();
  if (currentFocusWin && webContainer) {
    // Get whether the doc contains a frameset
    // Also, check to see if the currently focus docshell
    // is a child of this docshell
    bool isParentFrameSet;
    *aIsIFrameSelected = IsThereAnIFrameSelected(webContainer, currentFocusWin, isParentFrameSet);
  }
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::GetIsRangeSelection(bool* aIsRangeSelection)
{
  // Get the currently focused window
  nsCOMPtr<nsPIDOMWindowOuter> currentFocusWin = FindFocusedDOMWindow();
  *aIsRangeSelection = IsThereARangeSelection(currentFocusWin);
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::GetIsFramesetFrameSelected(bool* aIsFramesetFrameSelected)
{
  // Get the currently focused window
  nsCOMPtr<nsPIDOMWindowOuter> currentFocusWin = FindFocusedDOMWindow();
  *aIsFramesetFrameSelected = currentFocusWin != nullptr;
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::GetPrintPreviewNumPages(int32_t* aPrintPreviewNumPages)
{
  NS_ENSURE_ARG_POINTER(aPrintPreviewNumPages);

  nsIFrame* seqFrame  = nullptr;
  *aPrintPreviewNumPages = 0;

  // When calling this function, the FinishPrintPreview() function might not
  // been called as there are still some
  RefPtr<nsPrintData> printData = mPrtPreview ? mPrtPreview : mPrt;
  if (NS_WARN_IF(!printData)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
    GetSeqFrameAndCountPagesInternal(printData->mPrintObject, seqFrame,
                                     *aPrintPreviewNumPages);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//----------------------------------------------------------------------------------
// Enumerate all the documents for their titles
NS_IMETHODIMP
nsPrintJob::EnumerateDocumentNames(uint32_t* aCount,
                                   char16_t*** aResult)
{
  NS_ENSURE_ARG(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  *aCount = 0;
  *aResult = nullptr;

  int32_t     numDocs = mPrt->mPrintDocList.Length();
  char16_t** array   = (char16_t**) moz_xmalloc(numDocs * sizeof(char16_t*));
  if (!array)
    return NS_ERROR_OUT_OF_MEMORY;

  for (int32_t i=0;i<numDocs;i++) {
    nsPrintObject* po = mPrt->mPrintDocList.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    nsAutoString docTitleStr;
    nsAutoString docURLStr;
    GetDocumentTitleAndURL(po->mDocument, docTitleStr, docURLStr);

    // Use the URL if the doc is empty
    if (docTitleStr.IsEmpty() && !docURLStr.IsEmpty()) {
      docTitleStr = docURLStr;
    }
    array[i] = ToNewUnicode(docTitleStr);
  }
  *aCount  = numDocs;
  *aResult = array;

  return NS_OK;

}

//----------------------------------------------------------------------------------
nsresult
nsPrintJob::GetGlobalPrintSettings(nsIPrintSettings** aGlobalPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aGlobalPrintSettings);

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPrintSettingsService> printSettingsService =
    do_GetService(sPrintSettingsServiceContractID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = printSettingsService->GetGlobalPrintSettings(aGlobalPrintSettings);
  }
  return rv;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::GetDoingPrint(bool* aDoingPrint)
{
  NS_ENSURE_ARG_POINTER(aDoingPrint);
  *aDoingPrint = mIsDoingPrinting;
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::GetDoingPrintPreview(bool* aDoingPrintPreview)
{
  NS_ENSURE_ARG_POINTER(aDoingPrintPreview);
  *aDoingPrintPreview = mIsDoingPrintPreview;
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintJob::GetCurrentPrintSettings(nsIPrintSettings** aCurrentPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aCurrentPrintSettings);

  if (mPrt) {
    *aCurrentPrintSettings = mPrt->mPrintSettings;

  } else if (mPrtPreview) {
    *aCurrentPrintSettings = mPrtPreview->mPrintSettings;

  } else {
    *aCurrentPrintSettings = nullptr;
  }
  NS_IF_ADDREF(*aCurrentPrintSettings);
  return NS_OK;
}

//-----------------------------------------------------------------
//-- Section: Pre-Reflow Methods
//-----------------------------------------------------------------

//---------------------------------------------------------------------
// This method checks to see if there is at least one printer defined
// and if so, it sets the first printer in the list as the default name
// in the PrintSettings which is then used for Printer Preview
nsresult
nsPrintJob::CheckForPrinters(nsIPrintSettings* aPrintSettings)
{
#if defined(XP_MACOSX) || defined(ANDROID)
  // Mac doesn't support retrieving a printer list.
  return NS_OK;
#else
#if defined(MOZ_X11)
  // On Linux, default printer name should be requested on the parent side.
  // Unless we are in the parent, we ignore this function
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }
#endif
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  // See if aPrintSettings already has a printer
  nsString printerName;
  nsresult rv = aPrintSettings->GetPrinterName(printerName);
  if (NS_SUCCEEDED(rv) && !printerName.IsEmpty()) {
    return NS_OK;
  }

  // aPrintSettings doesn't have a printer set. Try to fetch the default.
  nsCOMPtr<nsIPrintSettingsService> printSettingsService =
    do_GetService(sPrintSettingsServiceContractID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = printSettingsService->GetDefaultPrinterName(printerName);
  if (NS_SUCCEEDED(rv) && !printerName.IsEmpty()) {
    rv = aPrintSettings->SetPrinterName(printerName);
  }
  return rv;
#endif
}

//----------------------------------------------------------------------
// Set up to use the "pluggable" Print Progress Dialog
void
nsPrintJob::ShowPrintProgress(bool aIsForPrinting, bool& aDoNotify)
{
  // default to not notifying, that if something here goes wrong
  // or we aren't going to show the progress dialog we can straight into
  // reflowing the doc for printing.
  aDoNotify = false;

  // Assume we can't do progress and then see if we can
  bool showProgresssDialog = false;

  // if it is already being shown then don't bother to find out if it should be
  // so skip this and leave mShowProgressDialog set to FALSE
  if (!mProgressDialogIsShown) {
    showProgresssDialog = Preferences::GetBool("print.show_print_progress");
  }

  // Guarantee that mPrt and the objects it owns won't be deleted.  If this
  // method shows a progress dialog and spins the event loop.  So, mPrt may be
  // cleared or recreated.
  RefPtr<nsPrintData> printData = mPrt;

  // Turning off the showing of Print Progress in Prefs overrides
  // whether the calling PS desire to have it on or off, so only check PS if
  // prefs says it's ok to be on.
  if (showProgresssDialog) {
    printData->mPrintSettings->GetShowPrintProgress(&showProgresssDialog);
  }

  // Now open the service to get the progress dialog
  // If we don't get a service, that's ok, then just don't show progress
  if (showProgresssDialog) {
    nsCOMPtr<nsIPrintingPromptService> printPromptService(do_GetService(kPrintingPromptService));
    if (printPromptService) {
      nsPIDOMWindowOuter* domWin = mDocument->GetWindow();
      if (!domWin) return;

      nsCOMPtr<nsIDocShell> docShell = domWin->GetDocShell();
      if (!docShell) return;
      nsCOMPtr<nsIDocShellTreeOwner> owner;
      docShell->GetTreeOwner(getter_AddRefs(owner));
      nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_GetInterface(owner);
      if (!browserChrome) return;
      bool isModal = true;
      browserChrome->IsWindowModal(&isModal);
      if (isModal) {
        // Showing a print progress dialog when printing a modal window
        // isn't supported. See bug 301560.
        return;
      }

      nsCOMPtr<nsIWebProgressListener> printProgressListener;

      nsCOMPtr<nsIWebBrowserPrint> wbp(do_QueryInterface(mDocViewerPrint));
      nsresult rv =
        printPromptService->ShowProgress(
                              domWin, wbp, printData->mPrintSettings, this,
                              aIsForPrinting,
                              getter_AddRefs(printProgressListener),
                              getter_AddRefs(printData->mPrintProgressParams),
                              &aDoNotify);
      if (NS_SUCCEEDED(rv)) {
        if (printProgressListener) {
          printData->mPrintProgressListeners.AppendObject(
                                               printProgressListener);
        }

        if (printData->mPrintProgressParams) {
          SetDocAndURLIntoProgress(printData->mPrintObject,
                                   printData->mPrintProgressParams);
        }
      }
    }
  }
}

//---------------------------------------------------------------------
bool
nsPrintJob::IsThereARangeSelection(nsPIDOMWindowOuter* aDOMWin)
{
  if (mDisallowSelectionPrint)
    return false;

  nsCOMPtr<nsIPresShell> presShell;
  if (aDOMWin) {
    presShell = aDOMWin->GetDocShell()->GetPresShell();
  }

  if (!presShell)
    return false;

  // check here to see if there is a range selection
  // so we know whether to turn on the "Selection" radio button
  Selection* selection = presShell->GetCurrentSelection(SelectionType::eNormal);
  if (!selection) {
    return false;
  }

  int32_t rangeCount = selection->RangeCount();
  if (!rangeCount) {
    return false;
  }

  if (rangeCount > 1) {
    return true;
  }

  // check to make sure it isn't an insertion selection
  return selection->GetRangeAt(0) && !selection->IsCollapsed();
}

//---------------------------------------------------------------------
bool
nsPrintJob::IsParentAFrameSet(nsIDocShell* aParent)
{
  // See if the incoming doc is the root document
  if (!aParent) return false;

  // When it is the top level document we need to check
  // to see if it contains a frameset. If it does, then
  // we only want to print the doc's children and not the document itself
  // For anything else we always print all the children and the document
  // for example, if the doc contains an IFRAME we eant to print the child
  // document (the IFRAME) and then the rest of the document.
  //
  // XXX we really need to search the frame tree, and not the content
  // but there is no way to distinguish between IFRAMEs and FRAMEs
  // with the GetFrameType call.
  // Bug 53459 has been files so we can eventually distinguish
  // between IFRAME frames and FRAME frames
  bool isFrameSet = false;
  // only check to see if there is a frameset if there is
  // NO parent doc for this doc. meaning this parent is the root doc
  nsCOMPtr<nsIDocument> doc = aParent->GetDocument();
  if (doc) {
    nsIContent *rootElement = doc->GetRootElement();
    if (rootElement) {
      isFrameSet = HasFramesetChild(rootElement);
    }
  }
  return isFrameSet;
}


//---------------------------------------------------------------------
// Recursively build a list of sub documents to be printed
// that mirrors the document tree
void
nsPrintJob::BuildDocTree(nsIDocShell*      aParentNode,
                         nsTArray<nsPrintObject*>* aDocList,
                         const UniquePtr<nsPrintObject>& aPO)
{
  NS_ASSERTION(aParentNode, "Pointer is null!");
  NS_ASSERTION(aDocList, "Pointer is null!");
  NS_ASSERTION(aPO, "Pointer is null!");

  int32_t childWebshellCount;
  aParentNode->GetChildCount(&childWebshellCount);
  if (childWebshellCount > 0) {
    for (int32_t i=0;i<childWebshellCount;i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));

      nsCOMPtr<nsIContentViewer>  viewer;
      childAsShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIDOMDocument> doc = do_GetInterface(childAsShell);
        auto po = MakeUnique<nsPrintObject>();
        po->mParent = aPO.get();
        nsresult rv = po->Init(childAsShell, doc, aPO->mPrintPreview);
        if (NS_FAILED(rv))
          NS_NOTREACHED("Init failed?");
        aPO->mKids.AppendElement(Move(po));
        aDocList->AppendElement(aPO->mKids.LastElement().get());
        BuildDocTree(childAsShell, aDocList, aPO->mKids.LastElement());
      }
    }
  }
}

//---------------------------------------------------------------------
void
nsPrintJob::GetDocumentTitleAndURL(nsIDocument* aDoc,
                                   nsAString&   aTitle,
                                   nsAString&   aURLStr)
{
  NS_ASSERTION(aDoc, "Pointer is null!");

  aTitle.Truncate();
  aURLStr.Truncate();

  nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aDoc);
  doc->GetTitle(aTitle);

  nsIURI* url = aDoc->GetDocumentURI();
  if (!url) return;

  nsCOMPtr<nsIURIFixup> urifixup(do_GetService(NS_URIFIXUP_CONTRACTID));
  if (!urifixup) return;

  nsCOMPtr<nsIURI> exposableURI;
  urifixup->CreateExposableURI(url, getter_AddRefs(exposableURI));

  if (!exposableURI) return;

  nsAutoCString urlCStr;
  nsresult rv = exposableURI->GetSpec(urlCStr);
  if (NS_FAILED(rv)) return;

  nsCOMPtr<nsITextToSubURI> textToSubURI =
    do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return;

  textToSubURI->UnEscapeURIForUI(NS_LITERAL_CSTRING("UTF-8"),
                                 urlCStr, aURLStr);
}

//---------------------------------------------------------------------
// The walks the PO tree and for each document it walks the content
// tree looking for any content that are sub-shells
//
// It then sets the mContent pointer in the "found" PO object back to the
// the document that contained it.
void
nsPrintJob::MapContentToWebShells(const UniquePtr<nsPrintObject>& aRootPO,
                                  const UniquePtr<nsPrintObject>& aPO)
{
  NS_ASSERTION(aRootPO, "Pointer is null!");
  NS_ASSERTION(aPO, "Pointer is null!");

  // Recursively walk the content from the root item
  // XXX Would be faster to enumerate the subdocuments, although right now
  //     nsIDocument doesn't expose quite what would be needed.
  nsCOMPtr<nsIContentViewer> viewer;
  aPO->mDocShell->GetContentViewer(getter_AddRefs(viewer));
  if (!viewer) return;

  nsCOMPtr<nsIDOMDocument> domDoc;
  viewer->GetDOMDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (!doc) return;

  Element* rootElement = doc->GetRootElement();
  if (rootElement) {
    MapContentForPO(aPO, rootElement);
  } else {
    NS_WARNING("Null root content on (sub)document.");
  }

  // Continue recursively walking the chilren of this PO
  for (const UniquePtr<nsPrintObject>& kid : aPO->mKids) {
    MapContentToWebShells(aRootPO, kid);
  }

}

//-------------------------------------------------------
// A Frame's sub-doc may contain content or a FrameSet
// When it contains a FrameSet the mFrameType for the PrintObject
// is always set to an eFrame. Which is fine when printing "AsIs"
// but is incorrect when when printing "Each Frame Separately".
// When printing "Each Frame Separately" the Frame really acts like
// a frameset.
//
// This method walks the PO tree and checks to see if the PrintObject is
// an eFrame and has children that are eFrames (meaning it's a Frame containing a FrameSet)
// If so, then the mFrameType need to be changed to eFrameSet
//
// Also note: We only want to call this we are printing "Each Frame Separately"
//            when printing "As Is" leave it as an eFrame
void
nsPrintJob::CheckForChildFrameSets(const UniquePtr<nsPrintObject>& aPO)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Continue recursively walking the chilren of this PO
  bool hasChildFrames = false;
  for (const UniquePtr<nsPrintObject>& po : aPO->mKids) {
    if (po->mFrameType == eFrame) {
      hasChildFrames = true;
      CheckForChildFrameSets(po);
    }
  }

  if (hasChildFrames && aPO->mFrameType == eFrame) {
    aPO->mFrameType = eFrameSet;
  }
}

//---------------------------------------------------------------------
// This method is key to the entire print mechanism.
//
// This "maps" or figures out which sub-doc represents a
// given Frame or IFrame in its parent sub-doc.
//
// So the Mcontent pointer in the child sub-doc points to the
// content in the its parent document, that caused it to be printed.
// This is used later to (after reflow) to find the absolute location
// of the sub-doc on its parent's page frame so it can be
// printed in the correct location.
//
// This method recursvely "walks" the content for a document finding
// all the Frames and IFrames, then sets the "mFrameType" data member
// which tells us what type of PO we have
void
nsPrintJob::MapContentForPO(const UniquePtr<nsPrintObject>& aPO,
                            nsIContent* aContent)
{
  NS_PRECONDITION(aPO && aContent, "Null argument");

  nsIDocument* doc = aContent->GetComposedDoc();

  NS_ASSERTION(doc, "Content without a document from a document tree?");

  nsIDocument* subDoc = doc->GetSubDocumentFor(aContent);

  if (subDoc) {
    nsCOMPtr<nsIDocShell> docShell(subDoc->GetDocShell());

    if (docShell) {
      nsPrintObject * po = nullptr;
      for (const UniquePtr<nsPrintObject>& kid : aPO->mKids) {
        if (kid->mDocument == subDoc) {
          po = kid.get();
          break;
        }
      }

      // XXX If a subdocument has no onscreen presentation, there will be no PO
      //     This is even if there should be a print presentation
      if (po) {
        // "frame" elements not in a frameset context should be treated
        // as iframes
        if (aContent->IsHTMLElement(nsGkAtoms::frame) && po->mParent->mFrameType == eFrameSet) {
          po->mFrameType = eFrame;
        } else {
          // Assume something iframe-like, i.e. iframe, object, or embed
          po->mFrameType = eIFrame;
          SetPrintAsIs(po, true);
          NS_ASSERTION(po->mParent, "The root must be a parent");
          po->mParent->mPrintAsIs = true;
        }
      }
    }
  }

  // walk children content
  for (nsIContent* child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    MapContentForPO(aPO, child);
  }
}

//---------------------------------------------------------------------
bool
nsPrintJob::IsThereAnIFrameSelected(nsIDocShell* aDocShell,
                                    nsPIDOMWindowOuter* aDOMWin,
                                    bool& aIsParentFrameSet)
{
  aIsParentFrameSet = IsParentAFrameSet(aDocShell);
  bool iFrameIsSelected = false;
  if (mPrt && mPrt->mPrintObject) {
    nsPrintObject* po = FindPrintObjectByDOMWin(mPrt->mPrintObject.get(), aDOMWin);
    iFrameIsSelected = po && po->mFrameType == eIFrame;
  } else {
    // First, check to see if we are a frameset
    if (!aIsParentFrameSet) {
      // Check to see if there is a currenlt focused frame
      // if so, it means the selected frame is either the main docshell
      // or an IFRAME
      if (aDOMWin) {
        // Get the main docshell's DOMWin to see if it matches
        // the frame that is selected
        nsPIDOMWindowOuter* domWin = aDocShell ? aDocShell->GetWindow() : nullptr;
        if (domWin != aDOMWin) {
          iFrameIsSelected = true; // we have a selected IFRAME
        }
      }
    }
  }

  return iFrameIsSelected;
}

//---------------------------------------------------------------------
// Recursively sets all the PO items to be printed
// from the given item down into the tree
void
nsPrintJob::SetPrintPO(nsPrintObject* aPO, bool aPrint)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Set whether to print flag
  aPO->mDontPrint = !aPrint;

  for (const UniquePtr<nsPrintObject>& kid : aPO->mKids) {
    SetPrintPO(kid.get(), aPrint);
  }
}

//---------------------------------------------------------------------
// This will first use a Title and/or URL from the PrintSettings
// if one isn't set then it uses the one from the document
// then if not title is there we will make sure we send something back
// depending on the situation.
void
nsPrintJob::GetDisplayTitleAndURL(const UniquePtr<nsPrintObject>& aPO,
                                  nsAString& aTitle,
                                  nsAString& aURLStr,
                                  eDocTitleDefault aDefType)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  if (!mPrt)
    return;

  aTitle.Truncate();
  aURLStr.Truncate();

  // First check to see if the PrintSettings has defined an alternate title
  // and use that if it did
  if (mPrt->mPrintSettings) {
    mPrt->mPrintSettings->GetTitle(aTitle);
    mPrt->mPrintSettings->GetDocURL(aURLStr);
  }

  nsAutoString docTitle;
  nsAutoString docUrl;
  GetDocumentTitleAndURL(aPO->mDocument, docTitle, docUrl);

  if (aURLStr.IsEmpty() && !docUrl.IsEmpty()) {
    aURLStr = docUrl;
  }

  if (aTitle.IsEmpty()) {
    if (!docTitle.IsEmpty()) {
      aTitle = docTitle;
    } else {
      if (aDefType == eDocTitleDefURLDoc) {
        if (!aURLStr.IsEmpty()) {
          aTitle = aURLStr;
        } else if (!mPrt->mBrandName.IsEmpty()) {
          aTitle = mPrt->mBrandName;
        }
      }
    }
  }
}

//---------------------------------------------------------------------
nsresult
nsPrintJob::DocumentReadyForPrinting()
{
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    // Guarantee that mPrt->mPrintObject won't be deleted during a call of
    // CheckForChildFrameSets().
    RefPtr<nsPrintData> printData = mPrt;
    CheckForChildFrameSets(printData->mPrintObject);
  }

  //
  // Send the document to the printer...
  //
  nsresult rv = SetupToPrintContent();
  if (NS_FAILED(rv)) {
    // The print job was canceled or there was a problem
    // So remove all other documents from the print list
    DonePrintingPages(nullptr, rv);
  }
  return rv;
}

/** ---------------------------------------------------
 *  Cleans up when an error occurred
 */
nsresult
nsPrintJob::CleanupOnFailure(nsresult aResult, bool aIsPrinting)
{
  PR_PL(("****  Failed %s - rv 0x%" PRIX32, aIsPrinting?"Printing":"Print Preview",
         static_cast<uint32_t>(aResult)));

  /* cleanup... */
  if (mPagePrintTimer) {
    mPagePrintTimer->Stop();
    DisconnectPagePrintTimer();
  }

  if (aIsPrinting) {
    SetIsPrinting(false);
  } else {
    SetIsPrintPreview(false);
    SetIsCreatingPrintPreview(false);
  }

  /* cleanup done, let's fire-up an error dialog to notify the user
   * what went wrong...
   *
   * When rv == NS_ERROR_ABORT, it means we want out of the
   * print job without displaying any error messages
   */
  if (aResult != NS_ERROR_ABORT) {
    FirePrintingErrorEvent(aResult);
  }

  FirePrintCompletionEvent();

  return aResult;

}

//---------------------------------------------------------------------
void
nsPrintJob::FirePrintingErrorEvent(nsresult aPrintError)
{
  nsCOMPtr<nsIContentViewer> cv = do_QueryInterface(mDocViewerPrint);
  if (NS_WARN_IF(!cv)) {
    return;
  }

  nsCOMPtr<nsIDocument> doc = cv->GetDocument();
  RefPtr<CustomEvent> event =
    NS_NewDOMCustomEvent(doc, nullptr, nullptr);

  MOZ_ASSERT(event);
  nsCOMPtr<nsIWritableVariant> resultVariant = new nsVariant();
  // nsresults are Uint32_t's, but XPConnect will interpret it as a double
  // when any JS attempts to access it, and will therefore interpret it
  // incorrectly. We preempt this by casting and setting as a double.
  resultVariant->SetAsDouble(static_cast<double>(aPrintError));

  event->InitCustomEvent(NS_LITERAL_STRING("PrintingError"), false, false,
                         resultVariant);
  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(doc, event);
  asyncDispatcher->mOnlyChromeDispatch = true;
  asyncDispatcher->RunDOMEventWhenSafe();

  // Inform any progress listeners of the Error.
  if (mPrt) {
    // Note that nsPrintData::DoOnStatusChange() will call some listeners.
    // So, mPrt can be cleared or recreated.
    RefPtr<nsPrintData> printData = mPrt;
    printData->DoOnStatusChange(aPrintError);
  }
}

//-----------------------------------------------------------------
//-- Section: Reflow Methods
//-----------------------------------------------------------------

nsresult
nsPrintJob::ReconstructAndReflow(bool doSetPixelScale)
{
  if (NS_WARN_IF(!mPrt)) {
    return NS_ERROR_FAILURE;
  }

#if defined(XP_WIN) && defined(EXTENDED_DEBUG_PRINTING)
  // We need to clear all the output files here
  // because they will be re-created with second reflow of the docs
  if (kPrintingLogMod && kPrintingLogMod->level == DUMP_LAYOUT_LEVEL) {
    RemoveFilesInDir(".\\");
    gDumpFileNameCnt   = 0;
    gDumpLOFileNameCnt = 0;
  }
#endif

  // In this loop, it's conceivable that one of our helpers might clear mPrt,
  // while we're using it & its members!  So we capture it in an owning local
  // reference & use that instead of using mPrt directly.
  RefPtr<nsPrintData> printData = mPrt;
  for (uint32_t i = 0; i < printData->mPrintDocList.Length(); ++i) {
    nsPrintObject* po = printData->mPrintDocList.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");

    if (po->mDontPrint || po->mInvisible) {
      continue;
    }

    // When the print object has been marked as "print the document" (i.e,
    // po->mDontPrint is false), mPresContext and mPresShell should be
    // non-nullptr (i.e., should've been created for the print) since they
    // are necessary to print the document.
    MOZ_ASSERT(po->mPresContext && po->mPresShell,
      "mPresContext and mPresShell shouldn't be nullptr when the print object "
      "has been marked as \"print the document\"");

    UpdateZoomRatio(po, doSetPixelScale);

    po->mPresContext->SetPageScale(po->mZoomRatio);

    // Calculate scale factor from printer to screen
    float printDPI = float(printData->mPrintDC->AppUnitsPerCSSInch()) /
                       float(printData->mPrintDC->AppUnitsPerDevPixel());
    po->mPresContext->SetPrintPreviewScale(mScreenDPI / printDPI);

    po->mPresShell->ReconstructFrames();

    // If the printing was canceled or restarted with different data,
    // let's stop doing this printing.
    if (NS_WARN_IF(mPrt != printData)) {
      return NS_ERROR_FAILURE;
    }

    // For all views except the first one, setup the root view.
    // ??? Can there be multiple po for the top-level-document?
    bool documentIsTopLevel = true;
    if (i != 0) {
      nsSize adjSize;
      bool doReturn;
      nsresult rv = SetRootView(po, doReturn, documentIsTopLevel, adjSize);

      MOZ_ASSERT(!documentIsTopLevel, "How could this happen?");

      if (NS_FAILED(rv) || doReturn) {
        return rv;
      }
    }

    po->mPresShell->FlushPendingNotifications(FlushType::Layout);

    // If the printing was canceled or restarted with different data,
    // let's stop doing this printing.
    if (NS_WARN_IF(mPrt != printData)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = UpdateSelectionAndShrinkPrintObject(po, documentIsTopLevel);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

//-------------------------------------------------------
nsresult
nsPrintJob::SetupToPrintContent()
{
  // This method may be called while DoCommonPrint() initializes the instance
  // when its script blocker goes out of scope.  In such case, this cannot do
  // its job as expected because some objects in mPrt have not been initialized
  // yet but they are necessary.
  // Note: it shouldn't be possible for mPrt->mPrintObject to be null; we check
  // it for good measure (after we check its owner) before we start
  // dereferencing it below.
  if (NS_WARN_IF(!mPrt) ||
      NS_WARN_IF(!mPrt->mPrintObject)) {
    return NS_ERROR_FAILURE;
  }

  // If this is creating print preview, mPrt->mPrintObject->mPresContext and
  // mPrt->mPrintObject->mPresShell need to be non-nullptr because this cannot
  // initialize page sequence frame without them at end of this method since
  // page sequence frame has already been destroyed or not been created yet.
  if (mIsCreatingPrintPreview &&
      (NS_WARN_IF(!mPrt->mPrintObject->mPresContext) ||
       NS_WARN_IF(!mPrt->mPrintObject->mPresShell))) {
    return NS_ERROR_FAILURE;
  }

  // If this is printing some documents (not print-previewing the documents),
  // mPrt->mPrintObject->mPresContext and mPrt->mPrintObject->mPresShell can be
  // nullptr only when mPrt->mPrintObject->mDontPrint is set to true.  E.g., if
  // the document has a <frameset> element and it's printing only content in a
  // <frame> element or all <frame> elements separately.
  MOZ_ASSERT(
    (!mIsCreatingPrintPreview && !mPrt->mPrintObject->IsPrintable()) ||
    (mPrt->mPrintObject->mPresContext && mPrt->mPrintObject->mPresShell),
    "mPresContext and mPresShell shouldn't be nullptr when printing the "
    "document or creating print-preview");

  bool didReconstruction = false;

  // This method works with mPrt->mPrintObject.  So, we need to guarantee that
  // it won't be deleted in this method.  We achieve this by holding a strong
  // local reference to mPrt, which in turn keeps mPrintObject alive.
  RefPtr<nsPrintData> printData = mPrt;

  // If some new content got loaded since the initial reflow rebuild
  // everything.
  if (mDidLoadDataForPrinting) {
    nsresult rv = ReconstructAndReflow(DoSetPixelScale());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // If the printing was canceled or restarted with different data,
    // let's stop doing this printing.
    if (NS_WARN_IF(mPrt != printData)) {
      return NS_ERROR_FAILURE;
    }
    didReconstruction = true;
  }

  // Here is where we figure out if extra reflow for shrinking the content
  // is required.
  // But skip this step if we are in PrintPreview
  bool ppIsShrinkToFit = mPrtPreview && mPrtPreview->mShrinkToFit;
  if (printData->mShrinkToFit && !ppIsShrinkToFit) {
    // Now look for the PO that has the smallest percent for shrink to fit
    if (printData->mPrintDocList.Length() > 1 &&
        printData->mPrintObject->mFrameType == eFrameSet) {
      nsPrintObject* smallestPO = FindSmallestSTF();
      NS_ASSERTION(smallestPO, "There must always be an XMost PO!");
      if (smallestPO) {
        // Calc the shrinkage based on the entire content area
        printData->mShrinkRatio = smallestPO->mShrinkRatio;
      }
    } else {
      // Single document so use the Shrink as calculated for the PO
      printData->mShrinkRatio = printData->mPrintObject->mShrinkRatio;
    }

    if (printData->mShrinkRatio < 0.998f) {
      nsresult rv = ReconstructAndReflow(true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // If the printing was canceled or restarted with different data,
      // let's stop doing this printing.
      if (NS_WARN_IF(mPrt != printData)) {
        return NS_ERROR_FAILURE;
      }
      didReconstruction = true;
    }

    if (MOZ_LOG_TEST(gPrintingLog, LogLevel::Debug)) {
      float calcRatio = 0.0f;
      if (printData->mPrintDocList.Length() > 1 &&
          printData->mPrintObject->mFrameType == eFrameSet) {
        nsPrintObject* smallestPO = FindSmallestSTF();
        NS_ASSERTION(smallestPO, "There must always be an XMost PO!");
        if (smallestPO) {
          // Calc the shrinkage based on the entire content area
          calcRatio = smallestPO->mShrinkRatio;
        }
      } else {
        // Single document so use the Shrink as calculated for the PO
        calcRatio = printData->mPrintObject->mShrinkRatio;
      }
      PR_PL(("**************************************************************************\n"));
      PR_PL(("STF Ratio is: %8.5f Effective Ratio: %8.5f Diff: %8.5f\n",
             printData->mShrinkRatio, calcRatio,
             printData->mShrinkRatio-calcRatio));
      PR_PL(("**************************************************************************\n"));
    }
  }

  // If the frames got reconstructed and reflowed the number of pages might
  // has changed.
  if (didReconstruction) {
    FirePrintPreviewUpdateEvent();
    // If the printing was canceled or restarted with different data,
    // let's stop doing this printing.
    if (NS_WARN_IF(mPrt != printData)) {
      return NS_ERROR_FAILURE;
    }
  }

  DUMP_DOC_LIST(("\nAfter Reflow------------------------------------------"));
  PR_PL(("\n"));
  PR_PL(("-------------------------------------------------------\n"));
  PR_PL(("\n"));

  CalcNumPrintablePages(printData->mNumPrintablePages);

  PR_PL(("--- Printing %d pages\n", printData->mNumPrintablePages));
  DUMP_DOC_TREELAYOUT;

  // Print listener setup...
  printData->OnStartPrinting();

  // If the printing was canceled or restarted with different data,
  // let's stop doing this printing.
  if (NS_WARN_IF(mPrt != printData)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString fileNameStr;
  // check to see if we are printing to a file
  bool isPrintToFile = false;
  printData->mPrintSettings->GetPrintToFile(&isPrintToFile);
  if (isPrintToFile) {
    // On some platforms The BeginDocument needs to know the name of the file.
    printData->mPrintSettings->GetToFileName(fileNameStr);
  }

  nsAutoString docTitleStr;
  nsAutoString docURLStr;
  GetDisplayTitleAndURL(printData->mPrintObject, docTitleStr, docURLStr,
                        eDocTitleDefURLDoc);

  int32_t startPage = 1;
  int32_t endPage = printData->mNumPrintablePages;

  int16_t printRangeType = nsIPrintSettings::kRangeAllPages;
  printData->mPrintSettings->GetPrintRange(&printRangeType);
  if (printRangeType == nsIPrintSettings::kRangeSpecifiedPageRange) {
    printData->mPrintSettings->GetStartPageRange(&startPage);
    printData->mPrintSettings->GetEndPageRange(&endPage);
    if (endPage > printData->mNumPrintablePages) {
      endPage = printData->mNumPrintablePages;
    }
  }

  nsresult rv = NS_OK;
  // BeginDocument may pass back a FAILURE code
  // i.e. On Windows, if you are printing to a file and hit "Cancel"
  //      to the "File Name" dialog, this comes back as an error
  // Don't start printing when regression test are executed
  if (mIsDoingPrinting) {
    rv = printData->mPrintDC->BeginDocument(docTitleStr, fileNameStr, startPage,
                                            endPage);
  }

  if (mIsCreatingPrintPreview) {
    // Copy docTitleStr and docURLStr to the pageSequenceFrame, to be displayed
    // in the header
    nsIPageSequenceFrame* seqFrame =
      printData->mPrintObject->mPresShell->GetPageSequenceFrame();
    if (seqFrame) {
      seqFrame->StartPrint(printData->mPrintObject->mPresContext,
                           printData->mPrintSettings, docTitleStr, docURLStr);
    }
  }

  PR_PL(("****************** Begin Document ************************\n"));

  NS_ENSURE_SUCCESS(rv, rv);

  // This will print the docshell document
  // when it completes asynchronously in the DonePrintingPages method
  // it will check to see if there are more docshells to be printed and
  // then PrintDocContent will be called again.

  if (mIsDoingPrinting) {
    PrintDocContent(printData->mPrintObject, rv); // ignore return value
  }

  return rv;
}

//-------------------------------------------------------
// Recursively reflow each sub-doc and then calc
// all the frame locations of the sub-docs
nsresult
nsPrintJob::ReflowDocList(const UniquePtr<nsPrintObject>& aPO,
                          bool aSetPixelScale)
{
  NS_ENSURE_ARG_POINTER(aPO);

  // Check to see if the subdocument's element has been hidden by the parent document
  if (aPO->mParent && aPO->mParent->mPresShell) {
    nsIFrame* frame = aPO->mContent ? aPO->mContent->GetPrimaryFrame() : nullptr;
    if (!frame || !frame->StyleVisibility()->IsVisible()) {
      SetPrintPO(aPO.get(), false);
      aPO->mInvisible = true;
      return NS_OK;
    }
  }

  UpdateZoomRatio(aPO.get(), aSetPixelScale);

  nsresult rv;
  // Reflow the PO
  rv = ReflowPrintObject(aPO);
  NS_ENSURE_SUCCESS(rv, rv);

  for (const UniquePtr<nsPrintObject>& kid : aPO->mKids) {
    rv = ReflowDocList(kid, aSetPixelScale);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

void
nsPrintJob::FirePrintPreviewUpdateEvent()
{
  // Dispatch the event only while in PrintPreview. When printing, there is no
  // listener bound to this event and therefore no need to dispatch it.
  if (mIsDoingPrintPreview && !mIsDoingPrinting) {
    nsCOMPtr<nsIContentViewer> cv = do_QueryInterface(mDocViewerPrint);
    (new AsyncEventDispatcher(
       cv->GetDocument(), NS_LITERAL_STRING("printPreviewUpdate"), true, true)
    )->RunDOMEventWhenSafe();
  }
}

nsresult
nsPrintJob::InitPrintDocConstruction(bool aHandleError)
{
  nsresult rv;
  // Guarantee that mPrt->mPrintObject won't be deleted.  It's owned by mPrt.
  // So, we should grab it with local variable.
  RefPtr<nsPrintData> printData = mPrt;
  rv = ReflowDocList(printData->mPrintObject, DoSetPixelScale());
  NS_ENSURE_SUCCESS(rv, rv);

  FirePrintPreviewUpdateEvent();

  if (mLoadCounter == 0) {
    AfterNetworkPrint(aHandleError);
  }
  return rv;
}

nsresult
nsPrintJob::AfterNetworkPrint(bool aHandleError)
{
  // If Destroy() has already been called, mPtr is nullptr.  Then, the instance
  // needs to do nothing anymore in this method.
  // Note: it shouldn't be possible for mPrt->mPrintObject to be null; we
  // just check it for good measure, as we check its owner.
  // Note: it shouldn't be possible for mPrt->mPrintObject->mDocShell to be
  // null; we just check it for good measure, as we check its owner.
  if (!mPrt ||
      NS_WARN_IF(!mPrt->mPrintObject) ||
      NS_WARN_IF(!mPrt->mPrintObject->mDocShell)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWebProgress> webProgress = do_QueryInterface(mPrt->mPrintObject->mDocShell);

  webProgress->RemoveProgressListener(
    static_cast<nsIWebProgressListener*>(this));

  nsresult rv;
  if (mIsDoingPrinting) {
    rv = DocumentReadyForPrinting();
  } else {
    rv = FinishPrintPreview();
  }

  /* cleaup on failure + notify user */
  if (aHandleError && NS_FAILED(rv)) {
    NS_WARNING("nsPrintJob::AfterNetworkPrint failed");
    CleanupOnFailure(rv, !mIsDoingPrinting);
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
nsPrintJob::OnStateChange(nsIWebProgress* aWebProgress,
                          nsIRequest* aRequest,
                          uint32_t aStateFlags,
                          nsresult aStatus)
{
  nsAutoCString name;
  aRequest->GetName(name);
  if (name.EqualsLiteral("about:document-onload-blocker")) {
    return NS_OK;
  }
  if (aStateFlags & STATE_START) {
    ++mLoadCounter;
  } else if (aStateFlags & STATE_STOP) {
    mDidLoadDataForPrinting = true;
    --mLoadCounter;

    // If all resources are loaded, then do a small timeout and if there
    // are still no new requests, then another reflow.
    if (mLoadCounter == 0) {
      AfterNetworkPrint(true);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsPrintJob::OnProgressChange(nsIWebProgress* aWebProgress,
                             nsIRequest* aRequest,
                             int32_t aCurSelfProgress,
                             int32_t aMaxSelfProgress,
                             int32_t aCurTotalProgress,
                             int32_t aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrintJob::OnLocationChange(nsIWebProgress* aWebProgress,
                             nsIRequest* aRequest,
                             nsIURI* aLocation,
                             uint32_t aFlags)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrintJob::OnStatusChange(nsIWebProgress* aWebProgress,
                           nsIRequest* aRequest,
                           nsresult aStatus,
                           const char16_t* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrintJob::OnSecurityChange(nsIWebProgress* aWebProgress,
                             nsIRequest* aRequest,
                             uint32_t aState)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

//-------------------------------------------------------

void
nsPrintJob::UpdateZoomRatio(nsPrintObject* aPO, bool aSetPixelScale)
{
  // Here is where we set the shrinkage value into the DC
  // and this is what actually makes it shrink
  if (aSetPixelScale && aPO->mFrameType != eIFrame) {
    float ratio;
    if (mPrt->mPrintFrameType == nsIPrintSettings::kFramesAsIs || mPrt->mPrintFrameType == nsIPrintSettings::kNoFrames) {
      ratio = mPrt->mShrinkRatio - 0.005f; // round down
    } else {
      ratio = aPO->mShrinkRatio - 0.005f; // round down
    }
    aPO->mZoomRatio = ratio;
  } else if (!mPrt->mShrinkToFit) {
    double scaling;
    mPrt->mPrintSettings->GetScaling(&scaling);
    aPO->mZoomRatio = float(scaling);
  }
}

nsresult
nsPrintJob::UpdateSelectionAndShrinkPrintObject(nsPrintObject* aPO,
                                                bool aDocumentIsTopLevel)
{
  nsCOMPtr<nsIPresShell> displayShell = aPO->mDocShell->GetPresShell();
  // Transfer Selection Ranges to the new Print PresShell
  RefPtr<Selection> selection, selectionPS;
  // It's okay if there is no display shell, just skip copying the selection
  if (displayShell) {
    selection = displayShell->GetCurrentSelection(SelectionType::eNormal);
  }
  selectionPS = aPO->mPresShell->GetCurrentSelection(SelectionType::eNormal);

  // Reset all existing selection ranges that might have been added by calling
  // this function before.
  if (selectionPS) {
    selectionPS->RemoveAllRanges();
  }
  if (selection && selectionPS) {
    int32_t cnt = selection->RangeCount();
    int32_t inx;
    for (inx = 0; inx < cnt; ++inx) {
        selectionPS->AddRange(selection->GetRangeAt(inx));
    }
  }

  // If we are trying to shrink the contents to fit on the page
  // we must first locate the "pageContent" frame
  // Then we walk the frame tree and look for the "xmost" frame
  // this is the frame where the right-hand side of the frame extends
  // the furthest
  if (mPrt->mShrinkToFit && aDocumentIsTopLevel) {
    nsIPageSequenceFrame* pageSequence = aPO->mPresShell->GetPageSequenceFrame();
    NS_ENSURE_STATE(pageSequence);
    pageSequence->GetSTFPercent(aPO->mShrinkRatio);
    // Limit the shrink-to-fit scaling for some text-ish type of documents.
    nsAutoString contentType;
    aPO->mPresShell->GetDocument()->GetContentType(contentType);
    if (contentType.EqualsLiteral("application/xhtml+xml") ||
        StringBeginsWith(contentType, NS_LITERAL_STRING("text/"))) {
      int32_t limitPercent =
        Preferences::GetInt("print.shrink-to-fit.scale-limit-percent", 20);
      limitPercent = std::max(0, limitPercent);
      limitPercent = std::min(100, limitPercent);
      float minShrinkRatio = float(limitPercent) / 100;
      aPO->mShrinkRatio = std::max(aPO->mShrinkRatio, minShrinkRatio);
    }
  }
  return NS_OK;
}

bool
nsPrintJob::DoSetPixelScale()
{
  // This is an Optimization
  // If we are in PP then we already know all the shrinkage information
  // so just transfer it to the PrintData and we will skip the extra shrinkage reflow
  //
  // doSetPixelScale tells Reflow whether to set the shrinkage value into the DC
  // The first time we do not want to do this, the second time through we do
  bool doSetPixelScale = false;
  bool ppIsShrinkToFit = mPrtPreview && mPrtPreview->mShrinkToFit;
  if (ppIsShrinkToFit) {
    mPrt->mShrinkRatio = mPrtPreview->mShrinkRatio;
    doSetPixelScale = true;
  }
  return doSetPixelScale;
}

nsView*
nsPrintJob::GetParentViewForRoot()
{
  if (mIsCreatingPrintPreview) {
    nsCOMPtr<nsIContentViewer> cv = do_QueryInterface(mDocViewerPrint);
    if (cv) {
      return cv->FindContainerView();
    }
  }
  return nullptr;
}

nsresult
nsPrintJob::SetRootView(nsPrintObject* aPO,
                        bool& doReturn,
                        bool& documentIsTopLevel,
                        nsSize& adjSize)
{
  bool canCreateScrollbars = true;

  nsView* rootView;
  nsView* parentView = nullptr;

  doReturn = false;

  if (aPO->mParent && aPO->mParent->IsPrintable()) {
    nsIFrame* frame = aPO->mContent ? aPO->mContent->GetPrimaryFrame() : nullptr;
    // Without a frame, this document can't be displayed; therefore, there is no
    // point to reflowing it
    if (!frame) {
      SetPrintPO(aPO, false);
      doReturn = true;
      return NS_OK;
    }

    //XXX If printing supported printing document hierarchies with non-constant
    // zoom this would be wrong as we use the same mPrt->mPrintDC for all
    // subdocuments.
    adjSize = frame->GetContentRect().Size();
    documentIsTopLevel = false;
    // presshell exists because parent is printable

    // the top nsPrintObject's widget will always have scrollbars
    if (frame && frame->IsSubDocumentFrame()) {
      nsView* view = frame->GetView();
      NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
      view = view->GetFirstChild();
      NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
      parentView = view;
      canCreateScrollbars = false;
    }
  } else {
    nscoord pageWidth, pageHeight;
    mPrt->mPrintDC->GetDeviceSurfaceDimensions(pageWidth, pageHeight);
    adjSize = nsSize(pageWidth, pageHeight);
    documentIsTopLevel = true;
    parentView = GetParentViewForRoot();
  }

  if (aPO->mViewManager->GetRootView()) {
    // Reuse the root view that is already on the root frame.
    rootView = aPO->mViewManager->GetRootView();
    // Remove it from its existing parent if necessary
    aPO->mViewManager->RemoveChild(rootView);
    rootView->SetParent(parentView);
  } else {
    // Create a child window of the parent that is our "root view/window"
    nsRect tbounds = nsRect(nsPoint(0, 0), adjSize);
    rootView = aPO->mViewManager->CreateView(tbounds, parentView);
    NS_ENSURE_TRUE(rootView, NS_ERROR_OUT_OF_MEMORY);
  }

  if (mIsCreatingPrintPreview && documentIsTopLevel) {
    aPO->mPresContext->SetPaginatedScrolling(canCreateScrollbars);
  }

  // Setup hierarchical relationship in view manager
  aPO->mViewManager->SetRootView(rootView);

  return NS_OK;
}

// Reflow a nsPrintObject
nsresult
nsPrintJob::ReflowPrintObject(const UniquePtr<nsPrintObject>& aPO)
{
  NS_ENSURE_STATE(aPO);

  if (!aPO->IsPrintable()) {
    return NS_OK;
  }

  NS_ASSERTION(!aPO->mPresContext, "Recreating prescontext");

  // Guarantee that mPrt and the objects it owns won't be deleted in this method
  // because it might be cleared if other modules called from here may fire
  // events, notifying observers and/or listeners.
  RefPtr<nsPrintData> printData = mPrt;

  // create the PresContext
  nsPresContext::nsPresContextType type =
      mIsCreatingPrintPreview ? nsPresContext::eContext_PrintPreview:
                                nsPresContext::eContext_Print;
  nsView* parentView =
    aPO->mParent && aPO->mParent->IsPrintable() ? nullptr : GetParentViewForRoot();
  aPO->mPresContext = parentView ?
      new nsPresContext(aPO->mDocument, type) :
      new nsRootPresContext(aPO->mDocument, type);
  NS_ENSURE_TRUE(aPO->mPresContext, NS_ERROR_OUT_OF_MEMORY);
  aPO->mPresContext->SetPrintSettings(printData->mPrintSettings);

  // set the presentation context to the value in the print settings
  bool printBGColors;
  printData->mPrintSettings->GetPrintBGColors(&printBGColors);
  aPO->mPresContext->SetBackgroundColorDraw(printBGColors);
  printData->mPrintSettings->GetPrintBGImages(&printBGColors);
  aPO->mPresContext->SetBackgroundImageDraw(printBGColors);

  // init it with the DC
  nsresult rv = aPO->mPresContext->Init(printData->mPrintDC);
  NS_ENSURE_SUCCESS(rv, rv);

  aPO->mViewManager = new nsViewManager();

  rv = aPO->mViewManager->Init(printData->mPrintDC);
  NS_ENSURE_SUCCESS(rv,rv);

  StyleSetHandle styleSet = mDocViewerPrint->CreateStyleSet(aPO->mDocument);

  if (aPO->mDocument->IsSVGDocument()) {
    // The SVG document only loads minimal-xul.css, so it doesn't apply other
    // styles. We should add ua.css for applying style which related to print.
    auto cache = nsLayoutStylesheetCache::For(aPO->mDocument->GetStyleBackendType());
    styleSet->PrependStyleSheet(SheetType::Agent, cache->UASheet());
  }

  aPO->mPresShell = aPO->mDocument->CreateShell(aPO->mPresContext,
                                                aPO->mViewManager, styleSet);
  if (!aPO->mPresShell) {
    styleSet->Delete();
    return NS_ERROR_FAILURE;
  }

  // If we're printing selection then remove the unselected nodes from our
  // cloned document.
  int16_t printRangeType = nsIPrintSettings::kRangeAllPages;
  printData->mPrintSettings->GetPrintRange(&printRangeType);
  if (printRangeType == nsIPrintSettings::kRangeSelection) {
    DeleteUnselectedNodes(aPO->mDocument->GetOriginalDocument(), aPO->mDocument);
  }

  styleSet->EndUpdate();

  // The pres shell now owns the style set object.


  bool doReturn = false;;
  bool documentIsTopLevel = false;
  nsSize adjSize;

  rv = SetRootView(aPO.get(), doReturn, documentIsTopLevel, adjSize);

  if (NS_FAILED(rv) || doReturn) {
    return rv;
  }

  PR_PL(("In DV::ReflowPrintObject PO: %p pS: %p (%9s) Setting w,h to %d,%d\n",
         aPO.get(), aPO->mPresShell.get(),
         gFrameTypesStr[aPO->mFrameType], adjSize.width, adjSize.height));


  // This docshell stuff is weird; will go away when we stop having multiple
  // presentations per document
  aPO->mPresContext->SetContainer(aPO->mDocShell);

  aPO->mPresShell->BeginObservingDocument();

  aPO->mPresContext->SetPageSize(adjSize);
  aPO->mPresContext->SetIsRootPaginatedDocument(documentIsTopLevel);
  aPO->mPresContext->SetPageScale(aPO->mZoomRatio);
  // Calculate scale factor from printer to screen
  float printDPI = float(printData->mPrintDC->AppUnitsPerCSSInch()) /
                     float(printData->mPrintDC->AppUnitsPerDevPixel());
  aPO->mPresContext->SetPrintPreviewScale(mScreenDPI / printDPI);

  if (mIsCreatingPrintPreview && documentIsTopLevel) {
    mDocViewerPrint->SetPrintPreviewPresentation(aPO->mViewManager,
                                                 aPO->mPresContext,
                                                 aPO->mPresShell);
  }

  rv = aPO->mPresShell->Initialize(adjSize.width, adjSize.height);

  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(aPO->mPresShell, "Presshell should still be here");

  // Process the reflow event Initialize posted
  aPO->mPresShell->FlushPendingNotifications(FlushType::Layout);

  rv = UpdateSelectionAndShrinkPrintObject(aPO.get(), documentIsTopLevel);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef EXTENDED_DEBUG_PRINTING
    if (kPrintingLogMod && kPrintingLogMod->level == DUMP_LAYOUT_LEVEL) {
      nsAutoCString docStr;
      nsAutoCString urlStr;
      GetDocTitleAndURL(aPO, docStr, urlStr);
      char filename[256];
      sprintf(filename, "print_dump_%d.txt", gDumpFileNameCnt++);
      // Dump all the frames and view to a a file
      FILE * fd = fopen(filename, "w");
      if (fd) {
        nsIFrame *theRootFrame =
          aPO->mPresShell->FrameManager()->GetRootFrame();
        fprintf(fd, "Title: %s\n", docStr.get());
        fprintf(fd, "URL:   %s\n", urlStr.get());
        fprintf(fd, "--------------- Frames ----------------\n");
        //RefPtr<gfxContext> renderingContext =
        //  printData->mPrintDocDC->CreateRenderingContext();
        RootFrameList(aPO->mPresContext, fd, 0);
        //DumpFrames(fd, aPO->mPresContext, renderingContext, theRootFrame, 0);
        fprintf(fd, "---------------------------------------\n\n");
        fprintf(fd, "--------------- Views From Root Frame----------------\n");
        nsView* v = theRootFrame->GetView();
        if (v) {
          v->List(fd);
        } else {
          printf("View is null!\n");
        }
        if (docShell) {
          fprintf(fd, "--------------- All Views ----------------\n");
          DumpViews(docShell, fd);
          fprintf(fd, "---------------------------------------\n\n");
        }
        fclose(fd);
      }
    }
#endif

  return NS_OK;
}

//-------------------------------------------------------
// Figure out how many documents and how many total pages we are printing
void
nsPrintJob::CalcNumPrintablePages(int32_t& aNumPages)
{
  aNumPages = 0;
  // Count the number of printable documents
  // and printable pages
  for (uint32_t i=0; i<mPrt->mPrintDocList.Length(); i++) {
    nsPrintObject* po = mPrt->mPrintDocList.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    // Note: The po->mPresContext null-check below is necessary, because it's
    // possible po->mPresContext might never have been set.  (e.g., if
    // IsPrintable() returns false, ReflowPrintObject bails before setting
    // mPresContext)
    if (po->mPresContext && po->mPresContext->IsRootPaginatedDocument()) {
      nsIPageSequenceFrame* pageSequence = po->mPresShell->GetPageSequenceFrame();
      nsIFrame * seqFrame = do_QueryFrame(pageSequence);
      if (seqFrame) {
        aNumPages += seqFrame->PrincipalChildList().GetLength();
      }
    }
  }
}

//-----------------------------------------------------------------
//-- Done: Reflow Methods
//-----------------------------------------------------------------

//-----------------------------------------------------------------
//-- Section: Printing Methods
//-----------------------------------------------------------------

//-------------------------------------------------------
// Called for each DocShell that needs to be printed
bool
nsPrintJob::PrintDocContent(const UniquePtr<nsPrintObject>& aPO,
                            nsresult& aStatus)
{
  NS_ASSERTION(aPO, "Pointer is null!");
  aStatus = NS_OK;

  if (!aPO->mHasBeenPrinted && aPO->IsPrintable()) {
    aStatus = DoPrint(aPO);
    return true;
  }

  // If |aPO->mPrintAsIs| and |aPO->mHasBeenPrinted| are true,
  // the kids frames are already processed in |PrintPage|.
  if (!aPO->mInvisible && !(aPO->mPrintAsIs && aPO->mHasBeenPrinted)) {
    for (const UniquePtr<nsPrintObject>& po : aPO->mKids) {
      bool printed = PrintDocContent(po, aStatus);
      if (printed || NS_FAILED(aStatus)) {
        return true;
      }
    }
  }
  return false;
}

static nsINode*
GetCorrespondingNodeInDocument(const nsINode* aNode, nsIDocument* aDoc)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aDoc);

  // Selections in anonymous subtrees aren't supported.
  if (aNode->IsInAnonymousSubtree()) {
    return nullptr;
  }

  nsTArray<int32_t> indexArray;
  const nsINode* child = aNode;
  while (const nsINode* parent = child->GetParentNode()) {
    int32_t index = parent->IndexOf(child);
    MOZ_ASSERT(index >= 0);
    indexArray.AppendElement(index);
    child = parent;
  }
  MOZ_ASSERT(child->IsNodeOfType(nsINode::eDOCUMENT));

  nsINode* correspondingNode = aDoc;
  for (int32_t i = indexArray.Length() - 1; i >= 0; --i) {
    correspondingNode = correspondingNode->GetChildAt(indexArray[i]);
    NS_ENSURE_TRUE(correspondingNode, nullptr);
  }

  return correspondingNode;
}

static NS_NAMED_LITERAL_STRING(kEllipsis, u"\x2026");

static nsresult
DeleteUnselectedNodes(nsIDocument* aOrigDoc, nsIDocument* aDoc)
{
  nsIPresShell* origShell = aOrigDoc->GetShell();
  nsIPresShell* shell = aDoc->GetShell();
  NS_ENSURE_STATE(origShell && shell);

  RefPtr<Selection> origSelection =
    origShell->GetCurrentSelection(SelectionType::eNormal);
  RefPtr<Selection> selection =
    shell->GetCurrentSelection(SelectionType::eNormal);
  NS_ENSURE_STATE(origSelection && selection);

  nsINode* bodyNode = aDoc->GetBodyElement();
  nsINode* startNode = bodyNode;
  uint32_t startOffset = 0;
  uint32_t ellipsisOffset = 0;

  int32_t rangeCount = origSelection->RangeCount();
  for (int32_t i = 0; i < rangeCount; ++i) {
    nsRange* origRange = origSelection->GetRangeAt(i);

    // New end is start of original range.
    nsINode* endNode =
      GetCorrespondingNodeInDocument(origRange->GetStartContainer(), aDoc);

    // If we're no longer in the same text node reset the ellipsis offset.
    if (endNode != startNode) {
      ellipsisOffset = 0;
    }
    uint32_t endOffset = origRange->StartOffset() + ellipsisOffset;

    // Create the range that we want to remove. Note that if startNode or
    // endNode are null CreateRange will fail and we won't remove that section.
    RefPtr<nsRange> range;
    nsresult rv = nsRange::CreateRange(startNode, startOffset, endNode,
                                       endOffset, getter_AddRefs(range));

    if (NS_SUCCEEDED(rv) && !range->Collapsed()) {
      selection->AddRange(range);

      // Unless we've already added an ellipsis at the start, if we ended mid
      // text node then add ellipsis.
      Text* text = endNode->GetAsText();
      if (!ellipsisOffset && text && endOffset && endOffset < text->Length()) {
        text->InsertData(endOffset, kEllipsis);
        ellipsisOffset += kEllipsis.Length();
      }
    }

    // Next new start is end of original range.
    startNode =
      GetCorrespondingNodeInDocument(origRange->GetEndContainer(), aDoc);

    // If we're no longer in the same text node reset the ellipsis offset.
    if (startNode != endNode) {
      ellipsisOffset = 0;
    }
    startOffset = origRange->EndOffset() + ellipsisOffset;

    // If the next node will start mid text node then add ellipsis.
    Text* text = startNode ? startNode->GetAsText() : nullptr;
    if (text && startOffset && startOffset < text->Length()) {
      text->InsertData(startOffset, kEllipsis);
      startOffset += kEllipsis.Length();
      ellipsisOffset += kEllipsis.Length();
    }
  }

  // Add in the last range to the end of the body.
  RefPtr<nsRange> lastRange;
  nsresult rv = nsRange::CreateRange(startNode, startOffset, bodyNode,
                                     bodyNode->GetChildCount(),
                                     getter_AddRefs(lastRange));
  if (NS_SUCCEEDED(rv) && !lastRange->Collapsed()) {
    selection->AddRange(lastRange);
  }

  selection->DeleteFromDocument();
  return NS_OK;
}

//-------------------------------------------------------
nsresult
nsPrintJob::DoPrint(const UniquePtr<nsPrintObject>& aPO)
{
  PR_PL(("\n"));
  PR_PL(("**************************** %s ****************************\n", gFrameTypesStr[aPO->mFrameType]));
  PR_PL(("****** In DV::DoPrint   PO: %p \n", aPO.get()));

  nsIPresShell*   poPresShell   = aPO->mPresShell;
  nsPresContext*  poPresContext = aPO->mPresContext;

  NS_ASSERTION(poPresContext, "PrintObject has not been reflowed");
  NS_ASSERTION(poPresContext->Type() != nsPresContext::eContext_PrintPreview,
               "How did this context end up here?");

  // Guarantee that mPrt and the objects it owns won't be deleted in this method
  // because it might be cleared if other modules called from here may fire
  // events, notifying observers and/or listeners.
  RefPtr<nsPrintData> printData = mPrt;

  if (printData->mPrintProgressParams) {
    SetDocAndURLIntoProgress(aPO, printData->mPrintProgressParams);
  }

  {
    // Ask the page sequence frame to print all the pages
    nsIPageSequenceFrame* pageSequence = poPresShell->GetPageSequenceFrame();
    NS_ASSERTION(nullptr != pageSequence, "no page sequence frame");

    // We are done preparing for printing, so we can turn this off
    printData->mPreparingForPrint = false;

#ifdef EXTENDED_DEBUG_PRINTING
    nsIFrame* rootFrame = poPresShell->FrameManager()->GetRootFrame();
    if (aPO->IsPrintable()) {
      nsAutoCString docStr;
      nsAutoCString urlStr;
      GetDocTitleAndURL(aPO, docStr, urlStr);
      DumpLayoutData(docStr.get(), urlStr.get(), poPresContext,
                     printData->mPrintDocDC, rootFrame, docShell, nullptr);
    }
#endif

    if (!printData->mPrintSettings) {
      // not sure what to do here!
      SetIsPrinting(false);
      return NS_ERROR_FAILURE;
    }

    nsAutoString docTitleStr;
    nsAutoString docURLStr;
    GetDisplayTitleAndURL(aPO, docTitleStr, docURLStr, eDocTitleDefBlank);

    nsIFrame * seqFrame = do_QueryFrame(pageSequence);
    if (!seqFrame) {
      SetIsPrinting(false);
      return NS_ERROR_FAILURE;
    }

    mPageSeqFrame = seqFrame;
    pageSequence->StartPrint(poPresContext, printData->mPrintSettings,
                             docTitleStr, docURLStr);

    // Schedule Page to Print
    PR_PL(("Scheduling Print of PO: %p (%s) \n", aPO.get(), gFrameTypesStr[aPO->mFrameType]));
    StartPagePrintTimer(aPO);
  }

  return NS_OK;
}

//---------------------------------------------------------------------
void
nsPrintJob::SetDocAndURLIntoProgress(const UniquePtr<nsPrintObject>& aPO,
                                     nsIPrintProgressParams* aParams)
{
  NS_ASSERTION(aPO, "Must have valid nsPrintObject");
  NS_ASSERTION(aParams, "Must have valid nsIPrintProgressParams");

  if (!aPO || !aPO->mDocShell || !aParams) {
    return;
  }
  const uint32_t kTitleLength = 64;

  nsAutoString docTitleStr;
  nsAutoString docURLStr;
  GetDisplayTitleAndURL(aPO, docTitleStr, docURLStr, eDocTitleDefURLDoc);

  // Make sure the Titles & URLS don't get too long for the progress dialog
  EllipseLongString(docTitleStr, kTitleLength, false);
  EllipseLongString(docURLStr, kTitleLength, true);

  aParams->SetDocTitle(docTitleStr);
  aParams->SetDocURL(docURLStr);
}

//---------------------------------------------------------------------
void
nsPrintJob::EllipseLongString(nsAString& aStr, const uint32_t aLen, bool aDoFront)
{
  // Make sure the URLS don't get too long for the progress dialog
  if (aLen >= 3 && aStr.Length() > aLen) {
    if (aDoFront) {
      nsAutoString newStr;
      newStr.AppendLiteral("...");
      newStr += Substring(aStr, aStr.Length() - (aLen - 3), aLen - 3);
      aStr = newStr;
    } else {
      aStr.SetLength(aLen - 3);
      aStr.AppendLiteral("...");
    }
  }
}

static bool
DocHasPrintCallbackCanvas(nsIDocument* aDoc, void* aData)
{
  if (!aDoc) {
    return true;
  }
  Element* root = aDoc->GetRootElement();
  if (!root) {
    return true;
  }
  RefPtr<nsContentList> canvases = NS_GetContentList(root,
                                                       kNameSpaceID_XHTML,
                                                       NS_LITERAL_STRING("canvas"));
  uint32_t canvasCount = canvases->Length(true);
  for (uint32_t i = 0; i < canvasCount; ++i) {
    HTMLCanvasElement* canvas = HTMLCanvasElement::FromContentOrNull(canvases->Item(i, false));
    if (canvas && canvas->GetMozPrintCallback()) {
      // This subdocument has a print callback. Set result and return false to
      // stop iteration.
      *static_cast<bool*>(aData) = true;
      return false;
    }
  }
  return true;
}

static bool
DocHasPrintCallbackCanvas(nsIDocument* aDoc)
{
  bool result = false;
  aDoc->EnumerateSubDocuments(&DocHasPrintCallbackCanvas, static_cast<void*>(&result));
  return result;
}

/**
 * Checks to see if the document this print engine is associated with has any
 * canvases that have a mozPrintCallback.
 * https://developer.mozilla.org/en-US/docs/Web/API/HTMLCanvasElement#Properties
 */
bool
nsPrintJob::HasPrintCallbackCanvas()
{
  if (!mDocument) {
    return false;
  }
  // First check this mDocument.
  bool result = false;
  DocHasPrintCallbackCanvas(mDocument, static_cast<void*>(&result));
  // Also check the sub documents.
  return result || DocHasPrintCallbackCanvas(mDocument);
}

//-------------------------------------------------------
bool
nsPrintJob::PrePrintPage()
{
  NS_ASSERTION(mPageSeqFrame.IsAlive(), "mPageSeqFrame is not alive!");
  NS_ASSERTION(mPrt,           "mPrt is null!");

  // Although these should NEVER be nullptr
  // This is added insurance, to make sure we don't crash in optimized builds
  if (!mPrt || !mPageSeqFrame.IsAlive()) {
    return true; // means we are done preparing the page.
  }

  // Guarantee that mPrt won't be deleted during a call of
  // FirePrintingErrorEvent().
  RefPtr<nsPrintData> printData = mPrt;

  // Check setting to see if someone request it be cancelled
  bool isCancelled = false;
  printData->mPrintSettings->GetIsCancelled(&isCancelled);
  if (isCancelled)
    return true;

  // Ask mPageSeqFrame if the page is ready to be printed.
  // If the page doesn't get printed at all, the |done| will be |true|.
  bool done = false;
  nsIPageSequenceFrame* pageSeqFrame = do_QueryFrame(mPageSeqFrame.GetFrame());
  nsresult rv = pageSeqFrame->PrePrintNextPage(mPagePrintTimer, &done);
  if (NS_FAILED(rv)) {
    // ??? ::PrintPage doesn't set |printData->mIsAborted = true| if
    // rv != NS_ERROR_ABORT, but I don't really understand why this should be
    // the right thing to do?  Shouldn't |printData->mIsAborted| set to true
    // all the time if something went wrong?
    if (rv != NS_ERROR_ABORT) {
      FirePrintingErrorEvent(rv);
      printData->mIsAborted = true;
    }
    done = true;
  }
  return done;
}

bool
nsPrintJob::PrintPage(nsPrintObject* aPO,
                      bool& aInRange)
{
  NS_ASSERTION(aPO,            "aPO is null!");
  NS_ASSERTION(mPageSeqFrame.IsAlive(), "mPageSeqFrame is not alive!");
  NS_ASSERTION(mPrt,           "mPrt is null!");

  // Although these should NEVER be nullptr
  // This is added insurance, to make sure we don't crash in optimized builds
  if (!mPrt || !aPO || !mPageSeqFrame.IsAlive()) {
    FirePrintingErrorEvent(NS_ERROR_FAILURE);
    return true; // means we are done printing
  }

  // Guarantee that mPrt won't be deleted during a call of
  // nsPrintData::DoOnProgressChange() which runs some listeners,
  // which may clear (& might otherwise destroy).
  RefPtr<nsPrintData> printData = mPrt;

  PR_PL(("-----------------------------------\n"));
  PR_PL(("------ In DV::PrintPage PO: %p (%s)\n", aPO, gFrameTypesStr[aPO->mFrameType]));

  // Check setting to see if someone request it be cancelled
  bool isCancelled = false;
  printData->mPrintSettings->GetIsCancelled(&isCancelled);
  if (isCancelled || printData->mIsAborted) {
    return true;
  }

  int32_t pageNum, numPages, endPage;
  nsIPageSequenceFrame* pageSeqFrame = do_QueryFrame(mPageSeqFrame.GetFrame());
  pageSeqFrame->GetCurrentPageNum(&pageNum);
  pageSeqFrame->GetNumPages(&numPages);

  bool donePrinting;
  bool isDoingPrintRange;
  pageSeqFrame->IsDoingPrintRange(&isDoingPrintRange);
  if (isDoingPrintRange) {
    int32_t fromPage;
    int32_t toPage;
    pageSeqFrame->GetPrintRange(&fromPage, &toPage);

    if (fromPage > numPages) {
      return true;
    }
    if (toPage > numPages) {
      toPage = numPages;
    }

    PR_PL(("****** Printing Page %d printing from %d to page %d\n", pageNum, fromPage, toPage));

    donePrinting = pageNum >= toPage;
    aInRange = pageNum >= fromPage && pageNum <= toPage;
    endPage = (toPage - fromPage)+1;
  } else {
    PR_PL(("****** Printing Page %d of %d page(s)\n", pageNum, numPages));

    donePrinting = pageNum >= numPages;
    endPage = numPages;
    aInRange = true;
  }

  // XXX This is wrong, but the actual behavior in the presence of a print
  // range sucks.
  if (printData->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    endPage = printData->mNumPrintablePages;
  }

  printData->DoOnProgressChange(++printData->mNumPagesPrinted,
                                endPage, false, 0);
  if (NS_WARN_IF(mPrt != printData)) {
    // If current printing is canceled or new print is started, let's return
    // true to notify the caller of current printing is done.
    return true;
  }

  if (XRE_IsParentProcess() && !printData->mPrintDC->IsSyncPagePrinting()) {
    mPagePrintTimer->WaitForRemotePrint();
  }

  // Print the Page
  // if a print job was cancelled externally, an EndPage or BeginPage may
  // fail and the failure is passed back here.
  // Returning true means we are done printing.
  //
  // When rv == NS_ERROR_ABORT, it means we want out of the
  // print job without displaying any error messages
  nsresult rv = pageSeqFrame->PrintNextPage();
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_ABORT) {
      FirePrintingErrorEvent(rv);
      printData->mIsAborted = true;
    }
    return true;
  }

  pageSeqFrame->DoPageEnd();

  return donePrinting;
}

void
nsPrintJob::PageDone(nsresult aResult)
{
  MOZ_ASSERT(mIsDoingPrinting);

  // mPagePrintTimer might be released during RemotePrintFinished, keep a
  // reference here to make sure it lives long enough.
  RefPtr<nsPagePrintTimer> timer = mPagePrintTimer;
  timer->RemotePrintFinished();
}

//-----------------------------------------------------------------
//-- Done: Printing Methods
//-----------------------------------------------------------------


//-----------------------------------------------------------------
//-- Section: Misc Support Methods
//-----------------------------------------------------------------

//---------------------------------------------------------------------
void
nsPrintJob::SetIsPrinting(bool aIsPrinting)
{
  mIsDoingPrinting = aIsPrinting;
  // Calling SetIsPrinting while in print preview confuses the document viewer
  // This is safe because we prevent exiting print preview while printing
  if (!mIsDoingPrintPreview && mDocViewerPrint) {
    mDocViewerPrint->SetIsPrinting(aIsPrinting);
  }
  if (mPrt && aIsPrinting) {
    mPrt->mPreparingForPrint = true;
  }
}

//---------------------------------------------------------------------
void
nsPrintJob::SetIsPrintPreview(bool aIsPrintPreview)
{
  mIsDoingPrintPreview = aIsPrintPreview;

  if (mDocViewerPrint) {
    mDocViewerPrint->SetIsPrintPreview(aIsPrintPreview);
  }
}

//---------------------------------------------------------------------
void
nsPrintJob::CleanupDocTitleArray(char16_t**& aArray, int32_t& aCount)
{
  for (int32_t i = aCount - 1; i >= 0; i--) {
    free(aArray[i]);
  }
  free(aArray);
  aArray = nullptr;
  aCount = 0;
}

//---------------------------------------------------------------------
// static
bool
nsPrintJob::HasFramesetChild(nsIContent* aContent)
{
  if (!aContent) {
    return false;
  }

  // do a breadth search across all siblings
  for (nsIContent* child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::frameset)) {
      return true;
    }
  }

  return false;
}



/** ---------------------------------------------------
 *  Get the Focused Frame for a documentviewer
 */
already_AddRefed<nsPIDOMWindowOuter>
nsPrintJob::FindFocusedDOMWindow()
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsPIDOMWindowOuter* window = mDocument->GetWindow();
  NS_ENSURE_TRUE(window, nullptr);

  nsCOMPtr<nsPIDOMWindowOuter> rootWindow = window->GetPrivateRoot();
  NS_ENSURE_TRUE(rootWindow, nullptr);

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsFocusManager::GetFocusedDescendant(rootWindow,
                                       nsFocusManager::eIncludeAllDescendants,
                                       getter_AddRefs(focusedWindow));
  NS_ENSURE_TRUE(focusedWindow, nullptr);

  if (IsWindowsInOurSubTree(focusedWindow)) {
    return focusedWindow.forget();
  }

  return nullptr;
}

//---------------------------------------------------------------------
bool
nsPrintJob::IsWindowsInOurSubTree(nsPIDOMWindowOuter* window)
{
  bool found = false;

  // now check to make sure it is in "our" tree of docshells
  if (window) {
    nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();

    if (docShell) {
      // get this DocViewer docshell
      nsCOMPtr<nsIDocShell> thisDVDocShell(do_QueryReferent(mContainer));
      while (!found) {
        if (docShell) {
          if (docShell == thisDVDocShell) {
            found = true;
            break;
          }
        } else {
          break; // at top of tree
        }
        nsCOMPtr<nsIDocShellTreeItem> docShellItemParent;
        docShell->GetSameTypeParent(getter_AddRefs(docShellItemParent));
        docShell = do_QueryInterface(docShellItemParent);
      } // while
    }
  } // scriptobj

  return found;
}

//-------------------------------------------------------
bool
nsPrintJob::DonePrintingPages(nsPrintObject* aPO, nsresult aResult)
{
  //NS_ASSERTION(aPO, "Pointer is null!");
  PR_PL(("****** In DV::DonePrintingPages PO: %p (%s)\n", aPO, aPO?gFrameTypesStr[aPO->mFrameType]:""));

  // If there is a pageSeqFrame, make sure there are no more printCanvas active
  // that might call |Notify| on the pagePrintTimer after things are cleaned up
  // and printing was marked as being done.
  if (mPageSeqFrame.IsAlive()) {
    nsIPageSequenceFrame* pageSeqFrame = do_QueryFrame(mPageSeqFrame.GetFrame());
    pageSeqFrame->ResetPrintCanvasList();
  }

  // Guarantee that mPrt and mPrt->mPrintObject won't be deleted during a
  // call of PrintDocContent() and FirePrintCompletionEvent().
  RefPtr<nsPrintData> printData = mPrt;

  if (aPO && !printData->mIsAborted) {
    aPO->mHasBeenPrinted = true;
    nsresult rv;
    bool didPrint = PrintDocContent(printData->mPrintObject, rv);
    if (NS_SUCCEEDED(rv) && didPrint) {
      PR_PL(("****** In DV::DonePrintingPages PO: %p (%s) didPrint:%s (Not Done Printing)\n", aPO, gFrameTypesStr[aPO->mFrameType], PRT_YESNO(didPrint)));
      return false;
    }
  }

  printData->mPrintDC->UnregisterPageDoneCallback();

  if (NS_SUCCEEDED(aResult)) {
    FirePrintCompletionEvent();
    // XXX mPrt may be cleared or replaced with new instance here.
    //     However, the following methods will clean up with new mPrt or will
    //     do nothing due to no proper nsPrintData instance.
  }

  TurnScriptingOn(true);
  SetIsPrinting(false);

  // Release reference to mPagePrintTimer; the timer object destroys itself
  // after this returns true
  DisconnectPagePrintTimer();

  return true;
}

//-------------------------------------------------------
// Recursively sets the PO items to be printed "As Is"
// from the given item down into the tree
void
nsPrintJob::SetPrintAsIs(nsPrintObject* aPO, bool aAsIs)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  aPO->mPrintAsIs = aAsIs;
  for (const UniquePtr<nsPrintObject>& kid : aPO->mKids) {
    SetPrintAsIs(kid.get(), aAsIs);
  }
}

//-------------------------------------------------------
// Given a DOMWindow it recursively finds the PO object that matches
nsPrintObject*
nsPrintJob::FindPrintObjectByDOMWin(nsPrintObject* aPO,
                                    nsPIDOMWindowOuter* aDOMWin)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Often the CurFocused DOMWindow is passed in
  // andit is valid for it to be null, so short circut
  if (!aDOMWin) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = aDOMWin->GetDoc();
  if (aPO->mDocument && aPO->mDocument->GetOriginalDocument() == doc) {
    return aPO;
  }

  for (const UniquePtr<nsPrintObject>& kid : aPO->mKids) {
    nsPrintObject* po = FindPrintObjectByDOMWin(kid.get(), aDOMWin);
    if (po) {
      return po;
    }
  }

  return nullptr;
}

//-------------------------------------------------------
nsresult
nsPrintJob::EnablePOsForPrinting()
{
  // Guarantee that mPrt and the objects it owns won't be deleted.
  RefPtr<nsPrintData> printData = mPrt;

  // NOTE: All POs have been "turned off" for printing
  // this is where we decided which POs get printed.

  if (!printData->mPrintSettings) {
    return NS_ERROR_FAILURE;
  }

  printData->mPrintFrameType = nsIPrintSettings::kNoFrames;
  printData->mPrintSettings->GetPrintFrameType(&printData->mPrintFrameType);

  int16_t printHowEnable = nsIPrintSettings::kFrameEnableNone;
  printData->mPrintSettings->GetHowToEnableFrameUI(&printHowEnable);

  int16_t printRangeType = nsIPrintSettings::kRangeAllPages;
  printData->mPrintSettings->GetPrintRange(&printRangeType);

  PR_PL(("\n"));
  PR_PL(("********* nsPrintJob::EnablePOsForPrinting *********\n"));
  PR_PL(("PrintFrameType:     %s \n",
         gPrintFrameTypeStr[printData->mPrintFrameType]));
  PR_PL(("HowToEnableFrameUI: %s \n", gFrameHowToEnableStr[printHowEnable]));
  PR_PL(("PrintRange:         %s \n", gPrintRangeStr[printRangeType]));
  PR_PL(("----\n"));

  // ***** This is the ultimate override *****
  // if we are printing the selection (either an IFrame or selection range)
  // then set the mPrintFrameType as if it were the selected frame
  if (printRangeType == nsIPrintSettings::kRangeSelection) {
    printData->mPrintFrameType = nsIPrintSettings::kSelectedFrame;
    printHowEnable = nsIPrintSettings::kFrameEnableNone;
  }

  // This tells us that the "Frame" UI has turned off,
  // so therefore there are no FrameSets/Frames/IFrames to be printed
  //
  // This means there are not FrameSets,
  // but the document could contain an IFrame
  if (printHowEnable == nsIPrintSettings::kFrameEnableNone) {
    // Print all the pages or a sub range of pages
    if (printRangeType == nsIPrintSettings::kRangeAllPages ||
        printRangeType == nsIPrintSettings::kRangeSpecifiedPageRange) {
      SetPrintPO(printData->mPrintObject.get(), true);

      // Set the children so they are PrinAsIs
      // In this case, the children are probably IFrames
      if (printData->mPrintObject->mKids.Length() > 0) {
        for (const UniquePtr<nsPrintObject>& po :
               printData->mPrintObject->mKids) {
          NS_ASSERTION(po, "nsPrintObject can't be null!");
          SetPrintAsIs(po.get());
        }

        // ***** Another override *****
        printData->mPrintFrameType = nsIPrintSettings::kFramesAsIs;
      }
      PR_PL(("PrintFrameType:     %s \n",
             gPrintFrameTypeStr[printData->mPrintFrameType]));
      PR_PL(("HowToEnableFrameUI: %s \n",
             gFrameHowToEnableStr[printHowEnable]));
      PR_PL(("PrintRange:         %s \n", gPrintRangeStr[printRangeType]));
      return NS_OK;
    }

    // This means we are either printed a selected IFrame or
    // we are printing the current selection
    if (printRangeType == nsIPrintSettings::kRangeSelection) {
      // If the currentFocusDOMWin can'r be null if something is selected
      if (printData->mCurrentFocusWin) {
        // Find the selected IFrame
        nsPrintObject* po =
          FindPrintObjectByDOMWin(printData->mPrintObject.get(),
                                  printData->mCurrentFocusWin);
        if (po) {
          // Makes sure all of its children are be printed "AsIs"
          SetPrintAsIs(po);

          // Now, only enable this POs (the selected PO) and all of its children
          SetPrintPO(po, true);

          // check to see if we have a range selection,
          // as oppose to a insert selection
          // this means if the user just clicked on the IFrame then
          // there will not be a selection so we want the entire page to print
          //
          // XXX this is sort of a hack right here to make the page
          // not try to reposition itself when printing selection
          nsPIDOMWindowOuter* domWin =
            po->mDocument->GetOriginalDocument()->GetWindow();
          if (!IsThereARangeSelection(domWin)) {
            printRangeType = nsIPrintSettings::kRangeAllPages;
            printData->mPrintSettings->SetPrintRange(printRangeType);
          }
          PR_PL(("PrintFrameType:     %s \n",
                 gPrintFrameTypeStr[printData->mPrintFrameType]));
          PR_PL(("HowToEnableFrameUI: %s \n",
                 gFrameHowToEnableStr[printHowEnable]));
          PR_PL(("PrintRange:         %s \n",
                 gPrintRangeStr[printRangeType]));
          return NS_OK;
        }
      } else {
        for (uint32_t i = 0; i < printData->mPrintDocList.Length(); i++) {
          nsPrintObject* po = printData->mPrintDocList.ElementAt(i);
          NS_ASSERTION(po, "nsPrintObject can't be null!");
          nsCOMPtr<nsPIDOMWindowOuter> domWin = po->mDocShell->GetWindow();
          if (IsThereARangeSelection(domWin)) {
            printData->mCurrentFocusWin = domWin.forget();
            SetPrintPO(po, true);
            break;
          }
        }
        return NS_OK;
      }
    }
  }

  // check to see if there is a selection when a FrameSet is present
  if (printRangeType == nsIPrintSettings::kRangeSelection) {
    // If the currentFocusDOMWin can'r be null if something is selected
    if (printData->mCurrentFocusWin) {
      // Find the selected IFrame
      nsPrintObject* po =
        FindPrintObjectByDOMWin(printData->mPrintObject.get(),
                                printData->mCurrentFocusWin);
      if (po) {
        // Makes sure all of its children are be printed "AsIs"
        SetPrintAsIs(po);

        // Now, only enable this POs (the selected PO) and all of its children
        SetPrintPO(po, true);

        // check to see if we have a range selection,
        // as oppose to a insert selection
        // this means if the user just clicked on the IFrame then
        // there will not be a selection so we want the entire page to print
        //
        // XXX this is sort of a hack right here to make the page
        // not try to reposition itself when printing selection
        nsCOMPtr<nsPIDOMWindowOuter> domWin = po->mDocument->GetOriginalDocument()->GetWindow();
        if (!IsThereARangeSelection(domWin)) {
          printRangeType = nsIPrintSettings::kRangeAllPages;
          printData->mPrintSettings->SetPrintRange(printRangeType);
        }
        PR_PL(("PrintFrameType:     %s \n",
               gPrintFrameTypeStr[printData->mPrintFrameType]));
        PR_PL(("HowToEnableFrameUI: %s \n",
               gFrameHowToEnableStr[printHowEnable]));
        PR_PL(("PrintRange:         %s \n", gPrintRangeStr[printRangeType]));
        return NS_OK;
      }
    }
  }

  // If we are printing "AsIs" then sets all the POs to be printed as is
  if (printData->mPrintFrameType == nsIPrintSettings::kFramesAsIs) {
    SetPrintAsIs(printData->mPrintObject.get());
    SetPrintPO(printData->mPrintObject.get(), true);
    return NS_OK;
  }

  // If we are printing the selected Frame then
  // find that PO for that selected DOMWin and set it all of its
  // children to be printed
  if (printData->mPrintFrameType == nsIPrintSettings::kSelectedFrame) {
    if ((printData->mIsParentAFrameSet && printData->mCurrentFocusWin) ||
        printData->mIsIFrameSelected) {
      nsPrintObject* po =
        FindPrintObjectByDOMWin(printData->mPrintObject.get(),
                                printData->mCurrentFocusWin);
      if (po) {
        // NOTE: Calling this sets the "po" and
        // we don't want to do this for documents that have no children,
        // because then the "DoEndPage" gets called and it shouldn't
        if (po->mKids.Length() > 0) {
          // Makes sure that itself, and all of its children are printed "AsIs"
          SetPrintAsIs(po);
        }

        // Now, only enable this POs (the selected PO) and all of its children
        SetPrintPO(po, true);
      }
    }
    return NS_OK;
  }

  // If we are print each subdoc separately,
  // then don't print any of the FraneSet Docs
  if (printData->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    SetPrintPO(printData->mPrintObject.get(), true);
    int32_t cnt = printData->mPrintDocList.Length();
    for (int32_t i=0;i<cnt;i++) {
      nsPrintObject* po = printData->mPrintDocList.ElementAt(i);
      NS_ASSERTION(po, "nsPrintObject can't be null!");
      if (po->mFrameType == eFrameSet) {
        po->mDontPrint = true;
      }
    }
  }

  return NS_OK;
}

//-------------------------------------------------------
// Return the nsPrintObject with that is XMost (The widest frameset frame) AND
// contains the XMost (widest) layout frame
nsPrintObject*
nsPrintJob::FindSmallestSTF()
{
  float smallestRatio = 1.0f;
  nsPrintObject* smallestPO = nullptr;

  for (uint32_t i=0;i<mPrt->mPrintDocList.Length();i++) {
    nsPrintObject* po = mPrt->mPrintDocList.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    if (po->mFrameType != eFrameSet && po->mFrameType != eIFrame) {
      if (po->mShrinkRatio < smallestRatio) {
        smallestRatio = po->mShrinkRatio;
        smallestPO    = po;
      }
    }
  }

#ifdef EXTENDED_DEBUG_PRINTING
  if (smallestPO) printf("*PO: %p  Type: %d  %10.3f\n", smallestPO, smallestPO->mFrameType, smallestPO->mShrinkRatio);
#endif
  return smallestPO;
}

//-------------------------------------------------------
void
nsPrintJob::TurnScriptingOn(bool aDoTurnOn)
{
  if (mIsDoingPrinting && aDoTurnOn && mDocViewerPrint &&
      mDocViewerPrint->GetIsPrintPreview()) {
    // We don't want to turn scripting on if print preview is shown still after
    // printing.
    return;
  }

  // The following for loop uses nsPrintObject instances that are owned by
  // mPrt or mPrtPreview.  Therefore, this method needs to guarantee that
  // they won't be deleted in this method.
  RefPtr<nsPrintData> printData = mPrt ? mPrt : mPrtPreview;
  if (!printData) {
    return;
  }

  NS_ASSERTION(mDocument, "We MUST have a document.");
  // First, get the script global object from the document...

  for (uint32_t i = 0; i < printData->mPrintDocList.Length(); i++) {
    nsPrintObject* po = printData->mPrintDocList.ElementAt(i);
    MOZ_ASSERT(po);

    nsIDocument* doc = po->mDocument;
    if (!doc) {
      continue;
    }

    if (nsCOMPtr<nsPIDOMWindowInner> window = doc->GetInnerWindow()) {
      nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(window);
      NS_WARNING_ASSERTION(go && go->GetGlobalJSObject(), "Can't get global");
      nsresult propThere = NS_PROPTABLE_PROP_NOT_THERE;
      doc->GetProperty(nsGkAtoms::scriptEnabledBeforePrintOrPreview,
                       &propThere);
      if (aDoTurnOn) {
        if (propThere != NS_PROPTABLE_PROP_NOT_THERE) {
          doc->DeleteProperty(nsGkAtoms::scriptEnabledBeforePrintOrPreview);
          if (go && go->GetGlobalJSObject()) {
            xpc::Scriptability::Get(go->GetGlobalJSObject()).Unblock();
          }
          window->Resume();
        }
      } else {
        // Have to be careful, because people call us over and over again with
        // aDoTurnOn == false.  So don't set the property if it's already
        // set, since in that case we'd set it to the wrong value.
        if (propThere == NS_PROPTABLE_PROP_NOT_THERE) {
          // Stash the current value of IsScriptEnabled on the document, so
          // that layout code running in print preview doesn't get confused.
          doc->SetProperty(nsGkAtoms::scriptEnabledBeforePrintOrPreview,
                           NS_INT32_TO_PTR(doc->IsScriptEnabled()));
          if (go && go->GetGlobalJSObject()) {
            xpc::Scriptability::Get(go->GetGlobalJSObject()).Block();
          }
          window->Suspend();
        }
      }
    }
  }
}

//-----------------------------------------------------------------
//-- Done: Misc Support Methods
//-----------------------------------------------------------------


//-----------------------------------------------------------------
//-- Section: Finishing up or Cleaning up
//-----------------------------------------------------------------

//-----------------------------------------------------------------
void
nsPrintJob::CloseProgressDialog(nsIWebProgressListener* aWebProgressListener)
{
  if (aWebProgressListener) {
    aWebProgressListener->OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_DOCUMENT, NS_OK);
  }
}

//-----------------------------------------------------------------
nsresult
nsPrintJob::FinishPrintPreview()
{
  nsresult rv = NS_OK;

#ifdef NS_PRINT_PREVIEW

  if (!mPrt) {
    /* we're already finished with print preview */
    return rv;
  }

  rv = DocumentReadyForPrinting();

  // Note that this method may be called while the instance is being
  // initialized.  Some methods which initialize the instance (e.g.,
  // DoCommonPrint) may need to stop initializing and return error if
  // this is called.  Therefore it's important to set IsCreatingPrintPreview
  // state to false here.  If you need to remove this call of
  // SetIsCreatingPrintPreview here, you need to keep them being able to
  // check whether the owner stopped using this instance.
  SetIsCreatingPrintPreview(false);

  // mPrt may be cleared during a call of nsPrintData::OnEndPrinting()
  // because that method invokes some arbitrary listeners.
  RefPtr<nsPrintData> printData = mPrt;
  if (NS_FAILED(rv)) {
    /* cleanup done, let's fire-up an error dialog to notify the user
     * what went wrong...
     */
    printData->OnEndPrinting();
    // XXX mPrt may be nullptr here.  So, Shouldn't TurnScriptingOn() take
    //     nsPrintData as an argument?
    TurnScriptingOn(true);

    return rv;
  }

  // At this point we are done preparing everything
  // before it is to be created

  if (mIsDoingPrintPreview && mOldPrtPreview) {
    mOldPrtPreview = nullptr;
  }

  printData->OnEndPrinting();
  // XXX If mPrt becomes nullptr or different instance here, what should we
  //     do?

  // PrintPreview was built using the mPrt (code reuse)
  // then we assign it over
  mPrtPreview = Move(mPrt);

#endif // NS_PRINT_PREVIEW

  return NS_OK;
}

//-----------------------------------------------------------------
//-- Done: Finishing up or Cleaning up
//-----------------------------------------------------------------


/*=============== Timer Related Code ======================*/
nsresult
nsPrintJob::StartPagePrintTimer(const UniquePtr<nsPrintObject>& aPO)
{
  if (!mPagePrintTimer) {
    // Get the delay time in between the printing of each page
    // this gives the user more time to press cancel
    int32_t printPageDelay = 50;
    mPrt->mPrintSettings->GetPrintPageDelay(&printPageDelay);

    nsCOMPtr<nsIContentViewer> cv = do_QueryInterface(mDocViewerPrint);
    NS_ENSURE_TRUE(cv, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDocument> doc = cv->GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    RefPtr<nsPagePrintTimer> timer =
      new nsPagePrintTimer(this, mDocViewerPrint, doc, printPageDelay);
    timer.forget(&mPagePrintTimer);

    nsCOMPtr<nsIPrintSession> printSession;
    nsresult rv = mPrt->mPrintSettings->GetPrintSession(getter_AddRefs(printSession));
    if (NS_SUCCEEDED(rv) && printSession) {
      RefPtr<mozilla::layout::RemotePrintJobChild> remotePrintJob;
      printSession->GetRemotePrintJob(getter_AddRefs(remotePrintJob));
      if (NS_SUCCEEDED(rv) && remotePrintJob) {
        remotePrintJob->SetPagePrintTimer(mPagePrintTimer);
        remotePrintJob->SetPrintJob(this);
      }
    }
  }

  return mPagePrintTimer->Start(aPO.get());
}

/*=============== nsIObserver Interface ======================*/
NS_IMETHODIMP
nsPrintJob::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  nsresult rv = NS_ERROR_FAILURE;

  rv = InitPrintDocConstruction(true);
  if (!mIsDoingPrinting && mPrtPreview) {
    RefPtr<nsPrintData> printDataOfPrintPreview = mPrtPreview;
    printDataOfPrintPreview->OnEndPrinting();
  }

  return rv;

}

//---------------------------------------------------------------
//-- PLEvent Notification
//---------------------------------------------------------------
class nsPrintCompletionEvent : public Runnable {
public:
  explicit nsPrintCompletionEvent(nsIDocumentViewerPrint* docViewerPrint)
    : mozilla::Runnable("nsPrintCompletionEvent")
    , mDocViewerPrint(docViewerPrint)
  {
    NS_ASSERTION(mDocViewerPrint, "mDocViewerPrint is null.");
  }

  NS_IMETHOD Run() override {
    if (mDocViewerPrint)
      mDocViewerPrint->OnDonePrinting();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDocumentViewerPrint> mDocViewerPrint;
};

//-----------------------------------------------------------
void
nsPrintJob::FirePrintCompletionEvent()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> event = new nsPrintCompletionEvent(mDocViewerPrint);
  nsCOMPtr<nsIContentViewer> cv = do_QueryInterface(mDocViewerPrint);
  NS_ENSURE_TRUE_VOID(cv);
  nsCOMPtr<nsIDocument> doc = cv->GetDocument();
  NS_ENSURE_TRUE_VOID(doc);

  NS_ENSURE_SUCCESS_VOID(doc->Dispatch(TaskCategory::Other, event.forget()));
}

void
nsPrintJob::DisconnectPagePrintTimer()
{
  if (mPagePrintTimer) {
    mPagePrintTimer->Disconnect();
    NS_RELEASE(mPagePrintTimer);
  }
}

//---------------------------------------------------------------
//---------------------------------------------------------------
//-- Debug helper routines
//---------------------------------------------------------------
//---------------------------------------------------------------
#if defined(XP_WIN) && defined(EXTENDED_DEBUG_PRINTING)
#include "windows.h"
#include "process.h"
#include "direct.h"

#define MY_FINDFIRST(a,b) FindFirstFile(a,b)
#define MY_FINDNEXT(a,b) FindNextFile(a,b)
#define ISDIR(a) (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
#define MY_FINDCLOSE(a) FindClose(a)
#define MY_FILENAME(a) a.cFileName
#define MY_FILESIZE(a) (a.nFileSizeHigh * MAXDWORD) + a.nFileSizeLow

int RemoveFilesInDir(const char * aDir)
{
	WIN32_FIND_DATA data_ptr;
	HANDLE find_handle;

  char path[MAX_PATH];

  strcpy(path, aDir);

	// Append slash to the end of the directory names if not there
	if (path[strlen(path)-1] != '\\')
    strcat(path, "\\");

  char findPath[MAX_PATH];
  strcpy(findPath, path);
  strcat(findPath, "*.*");

	find_handle = MY_FINDFIRST(findPath, &data_ptr);

	if (find_handle != INVALID_HANDLE_VALUE) {
		do  {
			if (ISDIR(data_ptr)
				&& (stricmp(MY_FILENAME(data_ptr),"."))
				&& (stricmp(MY_FILENAME(data_ptr),".."))) {
					// skip
			}
			else if (!ISDIR(data_ptr)) {
        if (!strncmp(MY_FILENAME(data_ptr), "print_dump", 10)) {
          char fileName[MAX_PATH];
          strcpy(fileName, aDir);
          strcat(fileName, "\\");
          strcat(fileName, MY_FILENAME(data_ptr));
				  printf("Removing %s\n", fileName);
          remove(fileName);
        }
			}
		} while(MY_FINDNEXT(find_handle,&data_ptr));
		MY_FINDCLOSE(find_handle);
	}
	return TRUE;
}
#endif

#ifdef EXTENDED_DEBUG_PRINTING

/** ---------------------------------------------------
 *  Dumps Frames for Printing
 */
static void RootFrameList(nsPresContext* aPresContext, FILE* out, int32_t aIndent)
{
  if (!aPresContext || !out)
    return;

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    nsIFrame* frame = shell->FrameManager()->GetRootFrame();
    if (frame) {
      frame->List(aPresContext, out, aIndent);
    }
  }
}

/** ---------------------------------------------------
 *  Dumps Frames for Printing
 */
static void DumpFrames(FILE*                 out,
                       nsPresContext*       aPresContext,
                       gfxContext * aRendContext,
                       nsIFrame *            aFrame,
                       int32_t               aLevel)
{
  NS_ASSERTION(out, "Pointer is null!");
  NS_ASSERTION(aPresContext, "Pointer is null!");
  NS_ASSERTION(aRendContext, "Pointer is null!");
  NS_ASSERTION(aFrame, "Pointer is null!");

  nsIFrame* child = aFrame->PrincipalChildList().FirstChild();
  while (child != nullptr) {
    for (int32_t i=0;i<aLevel;i++) {
     fprintf(out, "  ");
    }
    nsAutoString tmp;
    child->GetFrameName(tmp);
    fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
    bool isSelected;
    if (NS_SUCCEEDED(child->IsVisibleForPainting(aPresContext, *aRendContext, true, &isSelected))) {
      fprintf(out, " %p %s", child, isSelected?"VIS":"UVS");
      nsRect rect = child->GetRect();
      fprintf(out, "[%d,%d,%d,%d] ", rect.x, rect.y, rect.width, rect.height);
      fprintf(out, "v: %p ", (void*)child->GetView());
      fprintf(out, "\n");
      DumpFrames(out, aPresContext, aRendContext, child, aLevel+1);
      child = child->GetNextSibling();
    }
  }
}


/** ---------------------------------------------------
 *  Dumps the Views from the DocShell
 */
static void
DumpViews(nsIDocShell* aDocShell, FILE* out)
{
  NS_ASSERTION(aDocShell, "Pointer is null!");
  NS_ASSERTION(out, "Pointer is null!");

  if (nullptr != aDocShell) {
    fprintf(out, "docshell=%p \n", aDocShell);
    nsIPresShell* shell = nsPrintJob::GetPresShellFor(aDocShell);
    if (shell) {
      nsViewManager* vm = shell->GetViewManager();
      if (vm) {
        nsView* root = vm->GetRootView();
        if (root) {
          root->List(out);
        }
      }
    }
    else {
      fputs("null pres shell\n", out);
    }

    // dump the views of the sub documents
    int32_t i, n;
    aDocShell->GetChildCount(&n);
    for (i = 0; i < n; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aDocShell->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (childAsShell) {
        DumpViews(childAsShell, out);
      }
    }
  }
}

/** ---------------------------------------------------
 *  Dumps the Views and Frames
 */
void DumpLayoutData(char*              aTitleStr,
                    char*              aURLStr,
                    nsPresContext*    aPresContext,
                    nsDeviceContext * aDC,
                    nsIFrame *         aRootFrame,
                    nsIDocShekk *      aDocShell,
                    FILE*              aFD = nullptr)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  if (aPresContext == nullptr || aDC == nullptr) {
    return;
  }

#ifdef NS_PRINT_PREVIEW
  if (aPresContext->Type() == nsPresContext::eContext_PrintPreview) {
    return;
  }
#endif

  NS_ASSERTION(aRootFrame, "Pointer is null!");
  NS_ASSERTION(aDocShell, "Pointer is null!");

  // Dump all the frames and view to a a file
  char filename[256];
  sprintf(filename, "print_dump_layout_%d.txt", gDumpLOFileNameCnt++);
  FILE * fd = aFD?aFD:fopen(filename, "w");
  if (fd) {
    fprintf(fd, "Title: %s\n", aTitleStr?aTitleStr:"");
    fprintf(fd, "URL:   %s\n", aURLStr?aURLStr:"");
    fprintf(fd, "--------------- Frames ----------------\n");
    fprintf(fd, "--------------- Frames ----------------\n");
    //RefPtr<gfxContext> renderingContext =
    //  aDC->CreateRenderingContext();
    RootFrameList(aPresContext, fd, 0);
    //DumpFrames(fd, aPresContext, renderingContext, aRootFrame, 0);
    fprintf(fd, "---------------------------------------\n\n");
    fprintf(fd, "--------------- Views From Root Frame----------------\n");
    nsView* v = aRootFrame->GetView();
    if (v) {
      v->List(fd);
    } else {
      printf("View is null!\n");
    }
    if (aDocShell) {
      fprintf(fd, "--------------- All Views ----------------\n");
      DumpViews(aDocShell, fd);
      fprintf(fd, "---------------------------------------\n\n");
    }
    if (aFD == nullptr) {
      fclose(fd);
    }
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsList(nsTArray<nsPrintObject*> * aDocList)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  NS_ASSERTION(aDocList, "Pointer is null!");

  const char types[][3] = {"DC", "FR", "IF", "FS"};
  PR_PL(("Doc List\n***************************************************\n"));
  PR_PL(("T  P A H    PO    DocShell   Seq     Page      Root     Page#    Rect\n"));
  int32_t cnt = aDocList->Length();
  for (int32_t i=0;i<cnt;i++) {
    nsPrintObject* po = aDocList->ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    nsIFrame* rootFrame = nullptr;
    if (po->mPresShell) {
      rootFrame = po->mPresShell->FrameManager()->GetRootFrame();
      while (rootFrame != nullptr) {
        nsIPageSequenceFrame * sqf = do_QueryFrame(rootFrame);
        if (sqf) {
          break;
        }
        rootFrame = rootFrame->PrincipalChildList().FirstChild();
      }
    }

    PR_PL(("%s %d %d %d %p %p %p %p %p   %d   %d,%d,%d,%d\n", types[po->mFrameType],
            po->IsPrintable(), po->mPrintAsIs, po->mHasBeenPrinted, po, po->mDocShell.get(), po->mSeqFrame,
            po->mPageFrame, rootFrame, po->mPageNum, po->mRect.x, po->mRect.y, po->mRect.width, po->mRect.height));
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsTree(nsPrintObject * aPO, int aLevel, FILE* aFD)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  NS_ASSERTION(aPO, "Pointer is null!");

  FILE * fd = aFD?aFD:stdout;
  const char types[][3] = {"DC", "FR", "IF", "FS"};
  if (aLevel == 0) {
    fprintf(fd, "DocTree\n***************************************************\n");
    fprintf(fd, "T     PO    DocShell   Seq      Page     Page#    Rect\n");
  }
  int32_t cnt = aPO->mKids.Length();
  for (int32_t i=0;i<cnt;i++) {
    nsPrintObject* po = aPO->mKids.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    for (int32_t k=0;k<aLevel;k++) fprintf(fd, "  ");
    fprintf(fd, "%s %p %p %p %p %d %d,%d,%d,%d\n", types[po->mFrameType], po, po->mDocShell.get(), po->mSeqFrame,
           po->mPageFrame, po->mPageNum, po->mRect.x, po->mRect.y, po->mRect.width, po->mRect.height);
  }
}

//-------------------------------------------------------------
static void GetDocTitleAndURL(const UniquePtr<nsPrintObject>& aPO,
                              nsACString& aDocStr,
                              nsACString& aURLStr)
{
  nsAutoString docTitleStr;
  nsAutoString docURLStr;
  nsPrintJob::GetDisplayTitleAndURL(aPO,
                                    docTitleStr, docURLStr,
                                    nsPrintJob::eDocTitleDefURLDoc);
  aDocStr = NS_ConvertUTF16toUTF8(docTitleStr);
  aURLStr = NS_ConvertUTF16toUTF8(docURLStr);
}

//-------------------------------------------------------------
static void DumpPrintObjectsTreeLayout(nsPrintObject * aPO,
                                       nsDeviceContext * aDC,
                                       int aLevel, FILE * aFD)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  NS_ASSERTION(aPO, "Pointer is null!");
  NS_ASSERTION(aDC, "Pointer is null!");

  const char types[][3] = {"DC", "FR", "IF", "FS"};
  FILE * fd = nullptr;
  if (aLevel == 0) {
    fd = fopen("tree_layout.txt", "w");
    fprintf(fd, "DocTree\n***************************************************\n");
    fprintf(fd, "***************************************************\n");
    fprintf(fd, "T     PO    DocShell   Seq      Page     Page#    Rect\n");
  } else {
    fd = aFD;
  }
  if (fd) {
    nsIFrame* rootFrame = nullptr;
    if (aPO->mPresShell) {
      rootFrame = aPO->mPresShell->FrameManager()->GetRootFrame();
    }
    for (int32_t k=0;k<aLevel;k++) fprintf(fd, "  ");
    fprintf(fd, "%s %p %p %p %p %d %d,%d,%d,%d\n", types[aPO->mFrameType], aPO, aPO->mDocShell.get(), aPO->mSeqFrame,
           aPO->mPageFrame, aPO->mPageNum, aPO->mRect.x, aPO->mRect.y, aPO->mRect.width, aPO->mRect.height);
    if (aPO->IsPrintable()) {
      nsAutoCString docStr;
      nsAutoCString urlStr;
      GetDocTitleAndURL(aPO, docStr, urlStr);
      DumpLayoutData(docStr.get(), urlStr.get(), aPO->mPresContext, aDC, rootFrame, aPO->mDocShell, fd);
    }
    fprintf(fd, "<***************************************************>\n");

    int32_t cnt = aPO->mKids.Length();
    for (int32_t i=0;i<cnt;i++) {
      nsPrintObject* po = aPO->mKids.ElementAt(i);
      NS_ASSERTION(po, "nsPrintObject can't be null!");
      DumpPrintObjectsTreeLayout(po, aDC, aLevel+1, fd);
    }
  }
  if (aLevel == 0 && fd) {
    fclose(fd);
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsListStart(const char * aStr, nsTArray<nsPrintObject*> * aDocList)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  NS_ASSERTION(aStr, "Pointer is null!");
  NS_ASSERTION(aDocList, "Pointer is null!");

  PR_PL(("%s\n", aStr));
  DumpPrintObjectsList(aDocList);
}

#define DUMP_DOC_LIST(_title) DumpPrintObjectsListStart((_title), mPrt->mPrintDocList);
#define DUMP_DOC_TREE DumpPrintObjectsTree(mPrt->mPrintObject.get());
#define DUMP_DOC_TREELAYOUT DumpPrintObjectsTreeLayout(mPrt->mPrintObject.get(), mPrt->mPrintDC);

#else
#define DUMP_DOC_LIST(_title)
#define DUMP_DOC_TREE
#define DUMP_DOC_TREELAYOUT
#endif

//---------------------------------------------------------------
//---------------------------------------------------------------
//-- End of debug helper routines
//---------------------------------------------------------------
