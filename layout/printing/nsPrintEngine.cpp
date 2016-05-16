/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintEngine.h"

#include "nsIStringBundle.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/CustomEvent.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIURI.h"
#include "nsITextToSubURI.h"
#include "nsError.h"

#include "nsView.h"
#include <algorithm>

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIPrintOptions.h"
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
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLEmbedElement.h"

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

// Focus
#include "nsISelectionController.h"

// Misc
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "nsISupportsUtils.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"
#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMRange.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsContentUtils.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "mozilla/Preferences.h"

#include "nsWidgetsCID.h"
#include "nsIDeviceContextSpec.h"
#include "nsDeviceContextSpecProxy.h"
#include "nsViewManager.h"
#include "nsView.h"
#include "nsRenderingContext.h"

#include "nsIPageSequenceFrame.h"
#include "nsIURL.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewerFile.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIBaseWindow.h"
#include "nsILayoutHistoryState.h"
#include "nsFrameManager.h"
#include "nsHTMLReflowState.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewerPrint.h"

#include "nsFocusManager.h"
#include "nsRange.h"
#include "nsCDefaultURIFixup.h"
#include "nsIURIFixup.h"
#include "mozilla/dom/Element.h"
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

#ifdef EXTENDED_DEBUG_PRINTING
// Forward Declarations
static void DumpPrintObjectsListStart(const char * aStr, nsTArray<nsPrintObject*> * aDocList);
static void DumpPrintObjectsTree(nsPrintObject * aPO, int aLevel= 0, FILE* aFD = nullptr);
static void DumpPrintObjectsTreeLayout(nsPrintObject * aPO,nsDeviceContext * aDC, int aLevel= 0, FILE * aFD = nullptr);

#define DUMP_DOC_LIST(_title) DumpPrintObjectsListStart((_title), mPrt->mPrintDocList);
#define DUMP_DOC_TREE DumpPrintObjectsTree(mPrt->mPrintObject);
#define DUMP_DOC_TREELAYOUT DumpPrintObjectsTreeLayout(mPrt->mPrintObject, mPrt->mPrintDC);
#else
#define DUMP_DOC_LIST(_title)
#define DUMP_DOC_TREE
#define DUMP_DOC_TREELAYOUT
#endif

class nsScriptSuppressor
{
public:
  explicit nsScriptSuppressor(nsPrintEngine* aPrintEngine)
  : mPrintEngine(aPrintEngine), mSuppressed(false) {}

  ~nsScriptSuppressor() { Unsuppress(); }

  void Suppress()
  {
    if (mPrintEngine) {
      mSuppressed = true;
      mPrintEngine->TurnScriptingOn(false);
    }
  }
  
  void Unsuppress()
  {
    if (mPrintEngine && mSuppressed) {
      mPrintEngine->TurnScriptingOn(true);
    }
    mSuppressed = false;
  }

  void Disconnect() { mPrintEngine = nullptr; }
protected:
  RefPtr<nsPrintEngine> mPrintEngine;
  bool                    mSuppressed;
};

NS_IMPL_ISUPPORTS(nsPrintEngine, nsIWebProgressListener,
                  nsISupportsWeakReference, nsIObserver)

//---------------------------------------------------
//-- nsPrintEngine Class Impl
//---------------------------------------------------
nsPrintEngine::nsPrintEngine() :
  mIsCreatingPrintPreview(false),
  mIsDoingPrinting(false),
  mIsDoingPrintPreview(false),
  mProgressDialogIsShown(false),
  mScreenDPI(115.0f),
  mPrt(nullptr),
  mPagePrintTimer(nullptr),
  mPageSeqFrame(nullptr),
  mPrtPreview(nullptr),
  mOldPrtPreview(nullptr),
  mDebugFile(nullptr),
  mLoadCounter(0),
  mDidLoadDataForPrinting(false),
  mIsDestroying(false),
  mDisallowSelectionPrint(false)
{
}

//-------------------------------------------------------
nsPrintEngine::~nsPrintEngine()
{
  Destroy(); // for insurance
}

//-------------------------------------------------------
void nsPrintEngine::Destroy()
{
  if (mIsDestroying) {
    return;
  }
  mIsDestroying = true;

  if (mPrt) {
    delete mPrt;
    mPrt = nullptr;
  }

#ifdef NS_PRINT_PREVIEW
  if (mPrtPreview) {
    delete mPrtPreview;
    mPrtPreview = nullptr;
  }

  // This is insruance
  if (mOldPrtPreview) {
    delete mOldPrtPreview;
    mOldPrtPreview = nullptr;
  }

#endif
  mDocViewerPrint = nullptr;
}

//-------------------------------------------------------
void nsPrintEngine::DestroyPrintingData()
{
  if (mPrt) {
    nsPrintData* data = mPrt;
    mPrt = nullptr;
    delete data;
  }
}

//---------------------------------------------------------------------------------
//-- Section: Methods needed by the DocViewer
//---------------------------------------------------------------------------------

//--------------------------------------------------------
nsresult nsPrintEngine::Initialize(nsIDocumentViewerPrint* aDocViewerPrint, 
                                   nsIDocShell*            aContainer,
                                   nsIDocument*            aDocument,
                                   float                   aScreenDPI,
                                   FILE*                   aDebugFile)
{
  NS_ENSURE_ARG_POINTER(aDocViewerPrint);
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_ENSURE_ARG_POINTER(aDocument);

  mDocViewerPrint = aDocViewerPrint;
  mContainer      = do_GetWeakReference(aContainer);
  mDocument       = aDocument;
  mScreenDPI      = aScreenDPI;

  mDebugFile      = aDebugFile;      // ok to be nullptr

  return NS_OK;
}

//-------------------------------------------------------
bool
nsPrintEngine::CheckBeforeDestroy()
{
  if (mPrt && mPrt->mPreparingForPrint) {
    mPrt->mDocWasToBeDestroyed = true;
    return true;
  }
  return false;
}

//-------------------------------------------------------
nsresult
nsPrintEngine::Cancelled()
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
nsPrintEngine::InstallPrintPreviewListener()
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
nsPrintEngine::GetSeqFrameAndCountPagesInternal(nsPrintObject*  aPO,
                                                nsIFrame*&    aSeqFrame,
                                                int32_t&      aCount)
{
  NS_ENSURE_ARG_POINTER(aPO);

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
nsresult nsPrintEngine::GetSeqFrameAndCountPages(nsIFrame*& aSeqFrame, int32_t& aCount)
{
  NS_ASSERTION(mPrtPreview, "mPrtPreview can't be null!");
  return GetSeqFrameAndCountPagesInternal(mPrtPreview->mPrintObject, aSeqFrame, aCount);
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
nsPrintEngine::CommonPrint(bool                    aIsPrintPreview,
                           nsIPrintSettings*       aPrintSettings,
                           nsIWebProgressListener* aWebProgressListener,
                           nsIDOMDocument* aDoc) {
  RefPtr<nsPrintEngine> kungfuDeathGrip = this;
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
    delete mPrt;
    mPrt = nullptr;
  }

  return rv;
}

nsresult
nsPrintEngine::DoCommonPrint(bool                    aIsPrintPreview,
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
      mOldPrtPreview = mPrtPreview;
      mPrtPreview = nullptr;
    }
  } else {
    mProgressDialogIsShown = false;
  }

  mPrt = new nsPrintData(aIsPrintPreview ? nsPrintData::eIsPrintPreview :
                                           nsPrintData::eIsPrinting);
  NS_ENSURE_TRUE(mPrt, NS_ERROR_OUT_OF_MEMORY);

  // if they don't pass in a PrintSettings, then get the Global PS
  mPrt->mPrintSettings = aPrintSettings;
  if (!mPrt->mPrintSettings) {
    rv = GetGlobalPrintSettings(getter_AddRefs(mPrt->mPrintSettings));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = CheckForPrinters(mPrt->mPrintSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  mPrt->mPrintSettings->SetIsCancelled(false);
  mPrt->mPrintSettings->GetShrinkToFit(&mPrt->mShrinkToFit);

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
  // The print settings hold an nsWeakPtr to the session so it does not
  // need to be cleared from the settings at the end of the job.
  // XXX What lifetime does the printSession need to have?
  nsCOMPtr<nsIPrintSession> printSession;
  if (!aIsPrintPreview) {
    printSession = do_CreateInstance("@mozilla.org/gfx/printsession;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    mPrt->mPrintSettings->SetPrintSession(printSession);
  }

  if (aWebProgressListener != nullptr) {
    mPrt->mPrintProgressListeners.AppendObject(aWebProgressListener);
  }

  // Get the currently focused window and cache it
  // because the Print Dialog will "steal" focus and later when you try
  // to get the currently focused windows it will be nullptr
  mPrt->mCurrentFocusWin = FindFocusedDOMWindow();

  // Check to see if there is a "regular" selection
  bool isSelection = IsThereARangeSelection(mPrt->mCurrentFocusWin);

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
    mPrt->mPrintObject = new nsPrintObject();
    NS_ENSURE_TRUE(mPrt->mPrintObject, NS_ERROR_OUT_OF_MEMORY);
    rv = mPrt->mPrintObject->Init(webContainer, aDoc, aIsPrintPreview);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(mPrt->mPrintDocList.AppendElement(mPrt->mPrintObject),
                   NS_ERROR_OUT_OF_MEMORY);

    mPrt->mIsParentAFrameSet = IsParentAFrameSet(webContainer);
    mPrt->mPrintObject->mFrameType = mPrt->mIsParentAFrameSet ? eFrameSet : eDoc;

    // Build the "tree" of PrintObjects
    BuildDocTree(mPrt->mPrintObject->mDocShell, &mPrt->mPrintDocList,
                 mPrt->mPrintObject);
  }

  if (!aIsPrintPreview) {
    SetIsPrinting(true);
  }

  // XXX This isn't really correct...
  if (!mPrt->mPrintObject->mDocument ||
      !mPrt->mPrintObject->mDocument->GetRootElement())
    return NS_ERROR_GFX_PRINTER_STARTDOC;

  // Create the linkage from the sub-docs back to the content element
  // in the parent document
  MapContentToWebShells(mPrt->mPrintObject, mPrt->mPrintObject);

  mPrt->mIsIFrameSelected = IsThereAnIFrameSelected(webContainer, mPrt->mCurrentFocusWin, mPrt->mIsParentAFrameSet);

  // Setup print options for UI
  if (mPrt->mIsParentAFrameSet) {
    if (mPrt->mCurrentFocusWin) {
      mPrt->mPrintSettings->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableAll);
    } else {
      mPrt->mPrintSettings->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableAsIsAndEach);
    }
  } else {
    mPrt->mPrintSettings->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableNone);
  }
  // Now determine how to set up the Frame print UI
  mPrt->mPrintSettings->SetPrintOptions(nsIPrintSettings::kEnableSelectionRB,
                                        isSelection || mPrt->mIsIFrameSelected);

  nsCOMPtr<nsIDeviceContextSpec> devspec;
  if (XRE_IsContentProcess() && Preferences::GetBool("print.print_via_parent")) {
    devspec = new nsDeviceContextSpecProxy();
  } else {
    devspec = do_CreateInstance("@mozilla.org/gfx/devicecontextspec;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsScriptSuppressor scriptSuppressor(this);
  if (!aIsPrintPreview) {
#ifdef DEBUG
    mPrt->mDebugFilePtr = mDebugFile;
#endif

    scriptSuppressor.Suppress();
    bool printSilently;
    mPrt->mPrintSettings->GetPrintSilent(&printSilently);

    // Check prefs for a default setting as to whether we should print silently
    printSilently =
      Preferences::GetBool("print.always_print_silent", printSilently);

    // Ask dialog to be Print Shown via the Plugable Printing Dialog Service
    // This service is for the Print Dialog and the Print Progress Dialog
    // If printing silently or you can't get the service continue on
    if (!printSilently) {
      nsCOMPtr<nsIPrintingPromptService> printPromptService(do_GetService(kPrintingPromptService));
      if (printPromptService) {
        nsPIDOMWindowOuter* domWin = mDocument->GetWindow(); 
        NS_ENSURE_TRUE(domWin, NS_ERROR_FAILURE);

        // Platforms not implementing a given dialog for the service may
        // return NS_ERROR_NOT_IMPLEMENTED or an error code.
        //
        // NS_ERROR_NOT_IMPLEMENTED indicates they want default behavior
        // Any other error code means we must bail out
        //
        nsCOMPtr<nsIWebBrowserPrint> wbp(do_QueryInterface(mDocViewerPrint));
        rv = printPromptService->ShowPrintDialog(domWin, wbp,
                                                 mPrt->mPrintSettings);
        //
        // ShowPrintDialog triggers an event loop which means we can't assume
        // that the state of this->{anything} matches the state we've checked
        // above. Including that a given {thing} is non null.
        if (!mPrt) {
          return NS_ERROR_FAILURE;
        }

        if (NS_SUCCEEDED(rv)) {
          // since we got the dialog and it worked then make sure we 
          // are telling GFX we want to print silent
          printSilently = true;

          if (mPrt->mPrintSettings) {
            // The user might have changed shrink-to-fit in the print dialog, so update our copy of its state
            mPrt->mPrintSettings->GetShrinkToFit(&mPrt->mShrinkToFit);

            // Add thee RemotePrintJob as a listener if there is one.
            RefPtr<mozilla::layout::RemotePrintJobChild> remotePrintJob;
            printSession->GetRemotePrintJob(getter_AddRefs(remotePrintJob));
            if (NS_SUCCEEDED(rv) && remotePrintJob) {
              mPrt->mPrintProgressListeners.AppendElement(remotePrintJob);
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
      rv = mPrt->mPrintSettings->SetupSilentPrinting();
    }
    // Check explicitly for abort because it's expected
    if (rv == NS_ERROR_ABORT) 
      return rv;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = devspec->Init(nullptr, mPrt->mPrintSettings, aIsPrintPreview);
  NS_ENSURE_SUCCESS(rv, rv);

  mPrt->mPrintDC = new nsDeviceContext();
  rv = mPrt->mPrintDC->InitForPrinting(devspec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aIsPrintPreview) {
    mPrt->mPrintSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);

    // override any UI that wants to PrintPreview any selection or page range
    // we want to view every page in PrintPreview each time
    mPrt->mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
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
    mPrt->mPrintSettings->GetPrintFrameTypeUsage(&printFrameTypeUsage);

    // Ok, see if we are going to use our value and override the default
    if (printFrameTypeUsage == nsIPrintSettings::kUseSettingWhenPossible) {
      // Get the Print Options/Settings PrintFrameType to see what is preferred
      int16_t printFrameType = nsIPrintSettings::kEachFrameSep;
      mPrt->mPrintSettings->GetPrintFrameType(&printFrameType);

      // Don't let anybody do something stupid like try to set it to
      // kNoFrames when we are printing a FrameSet
      if (printFrameType == nsIPrintSettings::kNoFrames) {
        mPrt->mPrintFrameType = nsIPrintSettings::kEachFrameSep;
        mPrt->mPrintSettings->SetPrintFrameType(mPrt->mPrintFrameType);
      } else {
        // First find out from the PrinService what options are available
        // to us for Printing FrameSets
        int16_t howToEnableFrameUI;
        mPrt->mPrintSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);
        if (howToEnableFrameUI != nsIPrintSettings::kFrameEnableNone) {
          switch (howToEnableFrameUI) {
          case nsIPrintSettings::kFrameEnableAll:
            mPrt->mPrintFrameType = printFrameType;
            break;

          case nsIPrintSettings::kFrameEnableAsIsAndEach:
            if (printFrameType != nsIPrintSettings::kSelectedFrame) {
              mPrt->mPrintFrameType = printFrameType;
            } else { // revert back to a good value
              mPrt->mPrintFrameType = nsIPrintSettings::kEachFrameSep;
            }
            break;
          } // switch
          mPrt->mPrintSettings->SetPrintFrameType(mPrt->mPrintFrameType);
        }
      }
    } else {
      mPrt->mPrintSettings->GetPrintFrameType(&mPrt->mPrintFrameType);
    }
  }

  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    CheckForChildFrameSets(mPrt->mPrintObject);
  }

  if (NS_FAILED(EnablePOsForPrinting())) {
    return NS_ERROR_FAILURE;
  }

  // Attach progressListener to catch network requests.
  nsCOMPtr<nsIWebProgress> webProgress = do_QueryInterface(mPrt->mPrintObject->mDocShell);
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
      mPrt->OnStartPrinting();

      rv = InitPrintDocConstruction(false);
    }
  }

  // We will enable scripting later after printing has finished.
  scriptSuppressor.Disconnect();

  return NS_OK;
}

//---------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintEngine::Print(nsIPrintSettings*       aPrintSettings,
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
nsPrintEngine::PrintPreview(nsIPrintSettings* aPrintSettings, 
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
nsPrintEngine::GetIsFramesetDocument(bool *aIsFramesetDocument)
{
  nsCOMPtr<nsIDocShell> webContainer(do_QueryReferent(mContainer));
  *aIsFramesetDocument = IsParentAFrameSet(webContainer);
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP 
nsPrintEngine::GetIsIFrameSelected(bool *aIsIFrameSelected)
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
nsPrintEngine::GetIsRangeSelection(bool *aIsRangeSelection)
{
  // Get the currently focused window 
  nsCOMPtr<nsPIDOMWindowOuter> currentFocusWin = FindFocusedDOMWindow();
  *aIsRangeSelection = IsThereARangeSelection(currentFocusWin);
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP 
nsPrintEngine::GetIsFramesetFrameSelected(bool *aIsFramesetFrameSelected)
{
  // Get the currently focused window 
  nsCOMPtr<nsPIDOMWindowOuter> currentFocusWin = FindFocusedDOMWindow();
  *aIsFramesetFrameSelected = currentFocusWin != nullptr;
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintEngine::GetPrintPreviewNumPages(int32_t *aPrintPreviewNumPages)
{
  NS_ENSURE_ARG_POINTER(aPrintPreviewNumPages);

  nsPrintData* prt = nullptr;
  nsIFrame* seqFrame  = nullptr;
  *aPrintPreviewNumPages = 0;

  // When calling this function, the FinishPrintPreview() function might not
  // been called as there are still some 
  if (mPrtPreview) {
    prt = mPrtPreview;
  } else {
    prt = mPrt;
  }
  if ((!prt) ||
      NS_FAILED(GetSeqFrameAndCountPagesInternal(prt->mPrintObject, seqFrame, *aPrintPreviewNumPages))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//----------------------------------------------------------------------------------
// Enumerate all the documents for their titles
NS_IMETHODIMP
nsPrintEngine::EnumerateDocumentNames(uint32_t* aCount,
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
nsPrintEngine::GetGlobalPrintSettings(nsIPrintSettings **aGlobalPrintSettings)
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
nsPrintEngine::GetDoingPrint(bool *aDoingPrint)
{
  NS_ENSURE_ARG_POINTER(aDoingPrint);
  *aDoingPrint = mIsDoingPrinting;
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintEngine::GetDoingPrintPreview(bool *aDoingPrintPreview)
{
  NS_ENSURE_ARG_POINTER(aDoingPrintPreview);
  *aDoingPrintPreview = mIsDoingPrintPreview;
  return NS_OK;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintEngine::GetCurrentPrintSettings(nsIPrintSettings * *aCurrentPrintSettings)
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
nsPrintEngine::CheckForPrinters(nsIPrintSettings* aPrintSettings)
{
#if defined(XP_MACOSX) || defined(ANDROID)
  // Mac doesn't support retrieving a printer list.
  return NS_OK;
#else
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  // See if aPrintSettings already has a printer
  nsXPIDLString printerName;
  nsresult rv = aPrintSettings->GetPrinterName(getter_Copies(printerName));
  if (NS_SUCCEEDED(rv) && !printerName.IsEmpty()) {
    return NS_OK;
  }

  // aPrintSettings doesn't have a printer set. Try to fetch the default.
  nsCOMPtr<nsIPrintSettingsService> printSettingsService =
    do_GetService(sPrintSettingsServiceContractID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = printSettingsService->GetDefaultPrinterName(getter_Copies(printerName));
  if (NS_SUCCEEDED(rv) && !printerName.IsEmpty()) {
    rv = aPrintSettings->SetPrinterName(printerName.get());
  }
  return rv;
#endif
}

//----------------------------------------------------------------------
// Set up to use the "pluggable" Print Progress Dialog
void
nsPrintEngine::ShowPrintProgress(bool aIsForPrinting, bool& aDoNotify)
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

  // Turning off the showing of Print Progress in Prefs overrides
  // whether the calling PS desire to have it on or off, so only check PS if 
  // prefs says it's ok to be on.
  if (showProgresssDialog) {
    mPrt->mPrintSettings->GetShowPrintProgress(&showProgresssDialog);
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
      nsresult rv = printPromptService->ShowProgress(domWin, wbp, mPrt->mPrintSettings, this, aIsForPrinting,
                                                     getter_AddRefs(printProgressListener), 
                                                     getter_AddRefs(mPrt->mPrintProgressParams), 
                                                     &aDoNotify);
      if (NS_SUCCEEDED(rv)) {
        if (printProgressListener && mPrt->mPrintProgressParams) {
          mPrt->mPrintProgressListeners.AppendObject(printProgressListener);
          SetDocAndURLIntoProgress(mPrt->mPrintObject, mPrt->mPrintProgressParams);
        }
      }
    }
  }
}

//---------------------------------------------------------------------
bool
nsPrintEngine::IsThereARangeSelection(nsPIDOMWindowOuter* aDOMWin)
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
  Selection* selection =
    presShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
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
nsPrintEngine::IsParentAFrameSet(nsIDocShell * aParent)
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
nsPrintEngine::BuildDocTree(nsIDocShell *      aParentNode,
                            nsTArray<nsPrintObject*> * aDocList,
                            nsPrintObject *            aPO)
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
        nsCOMPtr<nsIContentViewerFile> viewerFile(do_QueryInterface(viewer));
        if (viewerFile) {
          nsCOMPtr<nsIDOMDocument> doc = do_GetInterface(childAsShell);
          nsPrintObject * po = new nsPrintObject();
          po->mParent = aPO;
          nsresult rv = po->Init(childAsShell, doc, aPO->mPrintPreview);
          if (NS_FAILED(rv))
            NS_NOTREACHED("Init failed?");
          aPO->mKids.AppendElement(po);
          aDocList->AppendElement(po);
          BuildDocTree(childAsShell, aDocList, po);
        }
      }
    }
  }
}

//---------------------------------------------------------------------
void
nsPrintEngine::GetDocumentTitleAndURL(nsIDocument* aDoc,
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
  exposableURI->GetSpec(urlCStr);

  nsresult rv;
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
nsPrintEngine::MapContentToWebShells(nsPrintObject* aRootPO,
                                     nsPrintObject* aPO)
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
  for (uint32_t i=0;i<aPO->mKids.Length();i++) {
    MapContentToWebShells(aRootPO, aPO->mKids[i]);
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
nsPrintEngine::CheckForChildFrameSets(nsPrintObject* aPO)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Continue recursively walking the chilren of this PO
  bool hasChildFrames = false;
  for (uint32_t i=0;i<aPO->mKids.Length();i++) {
    nsPrintObject* po = aPO->mKids[i];
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
nsPrintEngine::MapContentForPO(nsPrintObject*   aPO,
                               nsIContent*      aContent)
{
  NS_PRECONDITION(aPO && aContent, "Null argument");

  nsIDocument* doc = aContent->GetComposedDoc();

  NS_ASSERTION(doc, "Content without a document from a document tree?");

  nsIDocument* subDoc = doc->GetSubDocumentFor(aContent);

  if (subDoc) {
    nsCOMPtr<nsIDocShell> docShell(subDoc->GetDocShell());

    if (docShell) {
      nsPrintObject * po = nullptr;
      int32_t cnt = aPO->mKids.Length();
      for (int32_t i=0;i<cnt;i++) {
        nsPrintObject* kid = aPO->mKids.ElementAt(i);
        if (kid->mDocument == subDoc) {
          po = kid;
          break;
        }
      }

      // XXX If a subdocument has no onscreen presentation, there will be no PO
      //     This is even if there should be a print presentation
      if (po) {

        nsCOMPtr<nsIDOMHTMLFrameElement> frame(do_QueryInterface(aContent));
        // "frame" elements not in a frameset context should be treated
        // as iframes
        if (frame && po->mParent->mFrameType == eFrameSet) {
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
nsPrintEngine::IsThereAnIFrameSelected(nsIDocShell* aDocShell,
                                       nsPIDOMWindowOuter* aDOMWin,
                                       bool& aIsParentFrameSet)
{
  aIsParentFrameSet = IsParentAFrameSet(aDocShell);
  bool iFrameIsSelected = false;
  if (mPrt && mPrt->mPrintObject) {
    nsPrintObject* po = FindPrintObjectByDOMWin(mPrt->mPrintObject, aDOMWin);
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
nsPrintEngine::SetPrintPO(nsPrintObject* aPO, bool aPrint)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Set whether to print flag
  aPO->mDontPrint = !aPrint;

  for (uint32_t i=0;i<aPO->mKids.Length();i++) {
    SetPrintPO(aPO->mKids[i], aPrint);
  } 
}

//---------------------------------------------------------------------
// This will first use a Title and/or URL from the PrintSettings
// if one isn't set then it uses the one from the document
// then if not title is there we will make sure we send something back
// depending on the situation.
void
nsPrintEngine::GetDisplayTitleAndURL(nsPrintObject*   aPO,
                                     nsAString&       aTitle,
                                     nsAString&       aURLStr,
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
    char16_t * docTitleStrPS = nullptr;
    char16_t * docURLStrPS   = nullptr;
    mPrt->mPrintSettings->GetTitle(&docTitleStrPS);
    mPrt->mPrintSettings->GetDocURL(&docURLStrPS);

    if (docTitleStrPS) {
      aTitle = docTitleStrPS;
    }

    if (docURLStrPS) {
      aURLStr = docURLStrPS;
    }

    free(docTitleStrPS);
    free(docURLStrPS);
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
        } else if (mPrt->mBrandName) {
          aTitle = mPrt->mBrandName;
        }
      }
    }
  }
}

//---------------------------------------------------------------------
nsresult nsPrintEngine::DocumentReadyForPrinting()
{
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    CheckForChildFrameSets(mPrt->mPrintObject);
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
nsresult nsPrintEngine::CleanupOnFailure(nsresult aResult, bool aIsPrinting)
{
  PR_PL(("****  Failed %s - rv 0x%X", aIsPrinting?"Printing":"Print Preview", aResult));

  /* cleanup... */
  if (mPagePrintTimer) {
    mPagePrintTimer->Stop();
    NS_RELEASE(mPagePrintTimer);
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
nsPrintEngine::FirePrintingErrorEvent(nsresult aPrintError)
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
  mPrt->DoOnStatusChange(aPrintError);
}

//-----------------------------------------------------------------
//-- Section: Reflow Methods
//-----------------------------------------------------------------

nsresult
nsPrintEngine::ReconstructAndReflow(bool doSetPixelScale)
{
#if defined(XP_WIN) && defined(EXTENDED_DEBUG_PRINTING)
  // We need to clear all the output files here
  // because they will be re-created with second reflow of the docs
  if (kPrintingLogMod && kPrintingLogMod->level == DUMP_LAYOUT_LEVEL) {
    RemoveFilesInDir(".\\");
    gDumpFileNameCnt   = 0;
    gDumpLOFileNameCnt = 0;
  }
#endif

  for (uint32_t i = 0; i < mPrt->mPrintDocList.Length(); ++i) {
    nsPrintObject* po = mPrt->mPrintDocList.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");

    if (po->mDontPrint || po->mInvisible) {
      continue;
    }

    UpdateZoomRatio(po, doSetPixelScale);

    po->mPresContext->SetPageScale(po->mZoomRatio);

    // Calculate scale factor from printer to screen
    float printDPI = float(mPrt->mPrintDC->AppUnitsPerCSSInch()) /
                     float(mPrt->mPrintDC->AppUnitsPerDevPixel());
    po->mPresContext->SetPrintPreviewScale(mScreenDPI / printDPI);

    po->mPresShell->ReconstructFrames();

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

    po->mPresShell->FlushPendingNotifications(Flush_Layout);

    nsresult rv = UpdateSelectionAndShrinkPrintObject(po, documentIsTopLevel);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

//-------------------------------------------------------
nsresult
nsPrintEngine::SetupToPrintContent()
{
  nsresult rv;

  bool didReconstruction = false;
  
  // If some new content got loaded since the initial reflow rebuild
  // everything.
  if (mDidLoadDataForPrinting) {
    rv = ReconstructAndReflow(DoSetPixelScale());
    didReconstruction = true;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Here is where we figure out if extra reflow for shrinking the content
  // is required.
  // But skip this step if we are in PrintPreview
  bool ppIsShrinkToFit = mPrtPreview && mPrtPreview->mShrinkToFit;
  if (mPrt->mShrinkToFit && !ppIsShrinkToFit) {
    // Now look for the PO that has the smallest percent for shrink to fit
    if (mPrt->mPrintDocList.Length() > 1 && mPrt->mPrintObject->mFrameType == eFrameSet) {
      nsPrintObject* smallestPO = FindSmallestSTF();
      NS_ASSERTION(smallestPO, "There must always be an XMost PO!");
      if (smallestPO) {
        // Calc the shrinkage based on the entire content area
        mPrt->mShrinkRatio = smallestPO->mShrinkRatio;
      }
    } else {
      // Single document so use the Shrink as calculated for the PO
      mPrt->mShrinkRatio = mPrt->mPrintObject->mShrinkRatio;
    }

    if (mPrt->mShrinkRatio < 0.998f) {
      rv = ReconstructAndReflow(true);
      didReconstruction = true;
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (MOZ_LOG_TEST(gPrintingLog, LogLevel::Debug)) {
      float calcRatio = 0.0f;
      if (mPrt->mPrintDocList.Length() > 1 && mPrt->mPrintObject->mFrameType == eFrameSet) {
        nsPrintObject* smallestPO = FindSmallestSTF();
        NS_ASSERTION(smallestPO, "There must always be an XMost PO!");
        if (smallestPO) {
          // Calc the shrinkage based on the entire content area
          calcRatio = smallestPO->mShrinkRatio;
        }
      } else {
        // Single document so use the Shrink as calculated for the PO
        calcRatio = mPrt->mPrintObject->mShrinkRatio;
      }
      PR_PL(("**************************************************************************\n"));
      PR_PL(("STF Ratio is: %8.5f Effective Ratio: %8.5f Diff: %8.5f\n", mPrt->mShrinkRatio, calcRatio,  mPrt->mShrinkRatio-calcRatio));
      PR_PL(("**************************************************************************\n"));
    }
  }
  
  // If the frames got reconstructed and reflowed the number of pages might
  // has changed.
  if (didReconstruction) {
    FirePrintPreviewUpdateEvent();
  }
  
  DUMP_DOC_LIST(("\nAfter Reflow------------------------------------------"));
  PR_PL(("\n"));
  PR_PL(("-------------------------------------------------------\n"));
  PR_PL(("\n"));

  CalcNumPrintablePages(mPrt->mNumPrintablePages);

  PR_PL(("--- Printing %d pages\n", mPrt->mNumPrintablePages));
  DUMP_DOC_TREELAYOUT;

  // Print listener setup...
  if (mPrt != nullptr) {
    mPrt->OnStartPrinting();    
  }

  nsAutoString fileNameStr;
  // check to see if we are printing to a file
  bool isPrintToFile = false;
  mPrt->mPrintSettings->GetPrintToFile(&isPrintToFile);
  if (isPrintToFile) {
    // On some platforms The BeginDocument needs to know the name of the file.
    char16_t* fileName = nullptr;
    mPrt->mPrintSettings->GetToFileName(&fileName);
    fileNameStr = fileName;
  }

  nsAutoString docTitleStr;
  nsAutoString docURLStr;
  GetDisplayTitleAndURL(mPrt->mPrintObject, docTitleStr, docURLStr, eDocTitleDefURLDoc);

  int32_t startPage = 1;
  int32_t endPage   = mPrt->mNumPrintablePages;

  int16_t printRangeType = nsIPrintSettings::kRangeAllPages;
  mPrt->mPrintSettings->GetPrintRange(&printRangeType);
  if (printRangeType == nsIPrintSettings::kRangeSpecifiedPageRange) {
    mPrt->mPrintSettings->GetStartPageRange(&startPage);
    mPrt->mPrintSettings->GetEndPageRange(&endPage);
    if (endPage > mPrt->mNumPrintablePages) {
      endPage = mPrt->mNumPrintablePages;
    }
  }

  rv = NS_OK;
  // BeginDocument may pass back a FAILURE code
  // i.e. On Windows, if you are printing to a file and hit "Cancel" 
  //      to the "File Name" dialog, this comes back as an error
  // Don't start printing when regression test are executed  
  if (!mPrt->mDebugFilePtr && mIsDoingPrinting) {
    rv = mPrt->mPrintDC->BeginDocument(docTitleStr, fileNameStr, startPage,
                                       endPage);
  } 

  if (mIsCreatingPrintPreview) {
    // Copy docTitleStr and docURLStr to the pageSequenceFrame, to be displayed
    // in the header
    nsIPageSequenceFrame *seqFrame = mPrt->mPrintObject->mPresShell->GetPageSequenceFrame();
    if (seqFrame) {
      seqFrame->StartPrint(mPrt->mPrintObject->mPresContext, 
                           mPrt->mPrintSettings, docTitleStr, docURLStr);
    }
  }

  PR_PL(("****************** Begin Document ************************\n"));

  NS_ENSURE_SUCCESS(rv, rv);

  // This will print the docshell document
  // when it completes asynchronously in the DonePrintingPages method
  // it will check to see if there are more docshells to be printed and
  // then PrintDocContent will be called again.

  if (mIsDoingPrinting) {
    PrintDocContent(mPrt->mPrintObject, rv); // ignore return value
  }

  return rv;
}

//-------------------------------------------------------
// Recursively reflow each sub-doc and then calc
// all the frame locations of the sub-docs
nsresult
nsPrintEngine::ReflowDocList(nsPrintObject* aPO, bool aSetPixelScale)
{
  NS_ENSURE_ARG_POINTER(aPO);

  // Check to see if the subdocument's element has been hidden by the parent document
  if (aPO->mParent && aPO->mParent->mPresShell) {
    nsIFrame* frame = aPO->mContent ? aPO->mContent->GetPrimaryFrame() : nullptr;
    if (!frame || !frame->StyleVisibility()->IsVisible()) {
      SetPrintPO(aPO, false);
      aPO->mInvisible = true;
      return NS_OK;
    }
  }

  UpdateZoomRatio(aPO, aSetPixelScale);

  nsresult rv;
  // Reflow the PO
  rv = ReflowPrintObject(aPO);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t cnt = aPO->mKids.Length();
  for (int32_t i=0;i<cnt;i++) {
    rv = ReflowDocList(aPO->mKids[i], aSetPixelScale);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

void
nsPrintEngine::FirePrintPreviewUpdateEvent()
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
nsPrintEngine::InitPrintDocConstruction(bool aHandleError)
{
  nsresult rv;
  rv = ReflowDocList(mPrt->mPrintObject, DoSetPixelScale());
  NS_ENSURE_SUCCESS(rv, rv);

  FirePrintPreviewUpdateEvent();

  if (mLoadCounter == 0) {
    AfterNetworkPrint(aHandleError);
  }
  return rv;
}

nsresult
nsPrintEngine::AfterNetworkPrint(bool aHandleError)
{
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
    CleanupOnFailure(rv, !mIsDoingPrinting);
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
nsPrintEngine::OnStateChange(nsIWebProgress* aWebProgress,
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
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

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
nsPrintEngine::OnProgressChange(nsIWebProgress* aWebProgress,
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
nsPrintEngine::OnLocationChange(nsIWebProgress* aWebProgress,
                                nsIRequest* aRequest,
                                nsIURI* aLocation,
                                uint32_t aFlags)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrintEngine::OnStatusChange(nsIWebProgress *aWebProgress,
                              nsIRequest *aRequest,
                              nsresult aStatus,
                              const char16_t *aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrintEngine::OnSecurityChange(nsIWebProgress *aWebProgress,
                                  nsIRequest *aRequest,
                                  uint32_t aState)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

//-------------------------------------------------------

void
nsPrintEngine::UpdateZoomRatio(nsPrintObject* aPO, bool aSetPixelScale)
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
nsPrintEngine::UpdateSelectionAndShrinkPrintObject(nsPrintObject* aPO,
                                                   bool aDocumentIsTopLevel)
{
  nsCOMPtr<nsIPresShell> displayShell = aPO->mDocShell->GetPresShell();
  // Transfer Selection Ranges to the new Print PresShell
  RefPtr<Selection> selection, selectionPS;
  // It's okay if there is no display shell, just skip copying the selection
  if (displayShell) {
    selection = displayShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  }
  selectionPS = aPO->mPresShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);

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
nsPrintEngine::DoSetPixelScale()
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
nsPrintEngine::GetParentViewForRoot()
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
nsPrintEngine::SetRootView(
    nsPrintObject* aPO, 
    bool& doReturn, 
    bool& documentIsTopLevel, 
    nsSize& adjSize
)
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
    if (frame && frame->GetType() == nsGkAtoms::subDocumentFrame) {
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
nsPrintEngine::ReflowPrintObject(nsPrintObject * aPO)
{
  NS_ENSURE_STATE(aPO);

  if (!aPO->IsPrintable()) {
    return NS_OK;
  }
  
  NS_ASSERTION(!aPO->mPresContext, "Recreating prescontext");

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
  aPO->mPresContext->SetPrintSettings(mPrt->mPrintSettings);

  // set the presentation context to the value in the print settings
  bool printBGColors;
  mPrt->mPrintSettings->GetPrintBGColors(&printBGColors);
  aPO->mPresContext->SetBackgroundColorDraw(printBGColors);
  mPrt->mPrintSettings->GetPrintBGImages(&printBGColors);
  aPO->mPresContext->SetBackgroundImageDraw(printBGColors);

  // init it with the DC
  nsresult rv = aPO->mPresContext->Init(mPrt->mPrintDC);
  NS_ENSURE_SUCCESS(rv, rv);

  aPO->mViewManager = new nsViewManager();

  rv = aPO->mViewManager->Init(mPrt->mPrintDC);
  NS_ENSURE_SUCCESS(rv,rv);

  StyleSetHandle styleSet = mDocViewerPrint->CreateStyleSet(aPO->mDocument);

  aPO->mPresShell = aPO->mDocument->CreateShell(aPO->mPresContext,
                                                aPO->mViewManager, styleSet);
  if (!aPO->mPresShell) {
    styleSet->Delete();
    return NS_ERROR_FAILURE;
  }

  styleSet->EndUpdate();
  
  // The pres shell now owns the style set object.


  bool doReturn = false;;
  bool documentIsTopLevel = false;
  nsSize adjSize; 

  rv = SetRootView(aPO, doReturn, documentIsTopLevel, adjSize);

  if (NS_FAILED(rv) || doReturn) {
    return rv; 
  }

  PR_PL(("In DV::ReflowPrintObject PO: %p pS: %p (%9s) Setting w,h to %d,%d\n", aPO, aPO->mPresShell.get(),
         gFrameTypesStr[aPO->mFrameType], adjSize.width, adjSize.height));


  // This docshell stuff is weird; will go away when we stop having multiple
  // presentations per document
  aPO->mPresContext->SetContainer(aPO->mDocShell);

  aPO->mPresShell->BeginObservingDocument();

  aPO->mPresContext->SetPageSize(adjSize);
  aPO->mPresContext->SetIsRootPaginatedDocument(documentIsTopLevel);
  aPO->mPresContext->SetPageScale(aPO->mZoomRatio);
  // Calculate scale factor from printer to screen
  float printDPI = float(mPrt->mPrintDC->AppUnitsPerCSSInch()) /
                   float(mPrt->mPrintDC->AppUnitsPerDevPixel());
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
  aPO->mPresShell->FlushPendingNotifications(Flush_Layout);

  rv = UpdateSelectionAndShrinkPrintObject(aPO, documentIsTopLevel);
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
        //  mPrt->mPrintDocDC->CreateRenderingContext();
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
nsPrintEngine::CalcNumPrintablePages(int32_t& aNumPages)
{
  aNumPages = 0;
  // Count the number of printable documents
  // and printable pages
  for (uint32_t i=0; i<mPrt->mPrintDocList.Length(); i++) {
    nsPrintObject* po = mPrt->mPrintDocList.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
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
nsPrintEngine::PrintDocContent(nsPrintObject* aPO, nsresult& aStatus)
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
    for (uint32_t i=0;i<aPO->mKids.Length();i++) {
      nsPrintObject* po = aPO->mKids[i];
      bool printed = PrintDocContent(po, aStatus);
      if (printed || NS_FAILED(aStatus)) {
        return true;
      }
    }
  }
  return false;
}

static already_AddRefed<nsIDOMNode>
GetEqualNodeInCloneTree(nsIDOMNode* aNode, nsIDocument* aDoc)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  // Selections in anonymous subtrees aren't supported.
  if (content && content->IsInAnonymousSubtree()) {
    return nullptr;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, nullptr);

  nsTArray<int32_t> indexArray;
  nsINode* current = node;
  NS_ENSURE_TRUE(current, nullptr);
  while (current) {
    nsINode* parent = current->GetParentNode();
    if (!parent) {
     break;
    }
    int32_t index = parent->IndexOf(current);
    NS_ENSURE_TRUE(index >= 0, nullptr);
    indexArray.AppendElement(index);
    current = parent;
  }
  NS_ENSURE_TRUE(current->IsNodeOfType(nsINode::eDOCUMENT), nullptr);

  current = aDoc;
  for (int32_t i = indexArray.Length() - 1; i >= 0; --i) {
    current = current->GetChildAt(indexArray[i]);
    NS_ENSURE_TRUE(current, nullptr);
  }
  nsCOMPtr<nsIDOMNode> result = do_QueryInterface(current);
  return result.forget();
}

static void
CloneRangeToSelection(nsRange* aRange, nsIDocument* aDoc,
                      Selection* aSelection)
{
  if (aRange->Collapsed()) {
    return;
  }

  nsCOMPtr<nsIDOMNode> startContainer, endContainer;
  aRange->GetStartContainer(getter_AddRefs(startContainer));
  int32_t startOffset = aRange->StartOffset();
  aRange->GetEndContainer(getter_AddRefs(endContainer));
  int32_t endOffset = aRange->EndOffset();
  NS_ENSURE_TRUE_VOID(startContainer && endContainer);

  nsCOMPtr<nsIDOMNode> newStart = GetEqualNodeInCloneTree(startContainer, aDoc);
  nsCOMPtr<nsIDOMNode> newEnd = GetEqualNodeInCloneTree(endContainer, aDoc);
  NS_ENSURE_TRUE_VOID(newStart && newEnd);

  nsCOMPtr<nsINode> newStartNode = do_QueryInterface(newStart);
  NS_ENSURE_TRUE_VOID(newStartNode);

  RefPtr<nsRange> range = new nsRange(newStartNode);
  nsresult rv = range->SetStart(newStartNode, startOffset);
  NS_ENSURE_SUCCESS_VOID(rv);
  rv = range->SetEnd(newEnd, endOffset);
  NS_ENSURE_SUCCESS_VOID(rv);

  aSelection->AddRange(range);
}

static nsresult CloneSelection(nsIDocument* aOrigDoc, nsIDocument* aDoc)
{
  nsIPresShell* origShell = aOrigDoc->GetShell();
  nsIPresShell* shell = aDoc->GetShell();
  NS_ENSURE_STATE(origShell && shell);

  RefPtr<Selection> origSelection =
    origShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  RefPtr<Selection> selection =
    shell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  NS_ENSURE_STATE(origSelection && selection);

  int32_t rangeCount = origSelection->RangeCount();
  for (int32_t i = 0; i < rangeCount; ++i) {
      CloneRangeToSelection(origSelection->GetRangeAt(i), aDoc, selection);
  }
  return NS_OK;
}

//-------------------------------------------------------
nsresult
nsPrintEngine::DoPrint(nsPrintObject * aPO)
{
  PR_PL(("\n"));
  PR_PL(("**************************** %s ****************************\n", gFrameTypesStr[aPO->mFrameType]));
  PR_PL(("****** In DV::DoPrint   PO: %p \n", aPO));

  nsIPresShell*   poPresShell   = aPO->mPresShell;
  nsPresContext*  poPresContext = aPO->mPresContext;

  NS_ASSERTION(poPresContext, "PrintObject has not been reflowed");
  NS_ASSERTION(poPresContext->Type() != nsPresContext::eContext_PrintPreview,
               "How did this context end up here?");

  if (mPrt->mPrintProgressParams) {
    SetDocAndURLIntoProgress(aPO, mPrt->mPrintProgressParams);
  }

  {
    int16_t printRangeType = nsIPrintSettings::kRangeAllPages;
    nsresult rv;
    if (mPrt->mPrintSettings != nullptr) {
      mPrt->mPrintSettings->GetPrintRange(&printRangeType);
    }

    // Ask the page sequence frame to print all the pages
    nsIPageSequenceFrame* pageSequence = poPresShell->GetPageSequenceFrame();
    NS_ASSERTION(nullptr != pageSequence, "no page sequence frame");

    // We are done preparing for printing, so we can turn this off
    mPrt->mPreparingForPrint = false;

    // mPrt->mDebugFilePtr this is onlu non-null when compiled for debugging
    if (nullptr != mPrt->mDebugFilePtr) {
#ifdef DEBUG
      // output the regression test
      nsIFrame* root = poPresShell->FrameManager()->GetRootFrame();
      root->DumpRegressionData(poPresContext, mPrt->mDebugFilePtr, 0);
      fclose(mPrt->mDebugFilePtr);
      SetIsPrinting(false);
#endif
    } else {
#ifdef EXTENDED_DEBUG_PRINTING
      nsIFrame* rootFrame = poPresShell->FrameManager()->GetRootFrame();
      if (aPO->IsPrintable()) {
        nsAutoCString docStr;
        nsAutoCString urlStr;
        GetDocTitleAndURL(aPO, docStr, urlStr);
        DumpLayoutData(docStr.get(), urlStr.get(), poPresContext, mPrt->mPrintDocDC, rootFrame, docShell, nullptr);
      }
#endif

      if (!mPrt->mPrintSettings) {
        // not sure what to do here!
        SetIsPrinting(false);
        return NS_ERROR_FAILURE;
      }

      nsAutoString docTitleStr;
      nsAutoString docURLStr;
      GetDisplayTitleAndURL(aPO, docTitleStr, docURLStr, eDocTitleDefBlank);

      if (nsIPrintSettings::kRangeSelection == printRangeType) {
        CloneSelection(aPO->mDocument->GetOriginalDocument(), aPO->mDocument);

        poPresContext->SetIsRenderingOnlySelection(true);
        // temporarily creating rendering context
        // which is needed to find the selection frames
        // mPrintDC must have positive width and height for this call

        // find the starting and ending page numbers
        // via the selection
        nsIFrame* startFrame;
        nsIFrame* endFrame;
        int32_t   startPageNum;
        int32_t   endPageNum;
        nsRect    startRect;
        nsRect    endRect;

        rv = GetPageRangeForSelection(pageSequence,
                                      &startFrame, startPageNum, startRect,
                                      &endFrame, endPageNum, endRect);
        if (NS_SUCCEEDED(rv)) {
          mPrt->mPrintSettings->SetStartPageRange(startPageNum);
          mPrt->mPrintSettings->SetEndPageRange(endPageNum);
          nsIntMargin marginTwips(0,0,0,0);
          nsIntMargin unwrtMarginTwips(0,0,0,0);
          mPrt->mPrintSettings->GetMarginInTwips(marginTwips);
          mPrt->mPrintSettings->GetUnwriteableMarginInTwips(unwrtMarginTwips);
          nsMargin totalMargin = poPresContext->CSSTwipsToAppUnits(marginTwips +
                                                                   unwrtMarginTwips);
          if (startPageNum == endPageNum) {
            startRect.y -= totalMargin.top;
            endRect.y   -= totalMargin.top;

            // Clip out selection regions above the top of the first page
            if (startRect.y < 0) {
              // Reduce height to be the height of the positive-territory
              // region of original rect
              startRect.height = std::max(0, startRect.YMost());
              startRect.y = 0;
            }
            if (endRect.y < 0) {
              // Reduce height to be the height of the positive-territory
              // region of original rect
              endRect.height = std::max(0, endRect.YMost());
              endRect.y = 0;
            }
            NS_ASSERTION(endRect.y >= startRect.y,
                         "Selection end point should be after start point");
            NS_ASSERTION(startRect.height >= 0,
                         "rect should have non-negative height.");
            NS_ASSERTION(endRect.height >= 0,
                         "rect should have non-negative height.");

            nscoord selectionHgt = endRect.y + endRect.height - startRect.y;
            // XXX This is temporary fix for printing more than one page of a selection
            pageSequence->SetSelectionHeight(startRect.y * aPO->mZoomRatio,
                                             selectionHgt * aPO->mZoomRatio);

            // calc total pages by getting calculating the selection's height
            // and then dividing it by how page content frames will fit.
            nscoord pageWidth, pageHeight;
            mPrt->mPrintDC->GetDeviceSurfaceDimensions(pageWidth, pageHeight);
            pageHeight -= totalMargin.top + totalMargin.bottom;
            int32_t totalPages = NSToIntCeil(float(selectionHgt) * aPO->mZoomRatio / float(pageHeight));
            pageSequence->SetTotalNumPages(totalPages);
          }
        }
      }

      nsIFrame * seqFrame = do_QueryFrame(pageSequence);
      if (!seqFrame) {
        SetIsPrinting(false);
        return NS_ERROR_FAILURE;
      }

      mPageSeqFrame = pageSequence;
      mPageSeqFrame->StartPrint(poPresContext, mPrt->mPrintSettings, docTitleStr, docURLStr);

      // Schedule Page to Print
      PR_PL(("Scheduling Print of PO: %p (%s) \n", aPO, gFrameTypesStr[aPO->mFrameType]));
      StartPagePrintTimer(aPO);
    }
  }

  return NS_OK;
}

//---------------------------------------------------------------------
void
nsPrintEngine::SetDocAndURLIntoProgress(nsPrintObject* aPO,
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

  aParams->SetDocTitle(docTitleStr.get());
  aParams->SetDocURL(docURLStr.get());
}

//---------------------------------------------------------------------
void
nsPrintEngine::EllipseLongString(nsAString& aStr, const uint32_t aLen, bool aDoFront)
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
 */
bool
nsPrintEngine::HasPrintCallbackCanvas()
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
nsPrintEngine::PrePrintPage()
{
  NS_ASSERTION(mPageSeqFrame,  "mPageSeqFrame is null!");
  NS_ASSERTION(mPrt,           "mPrt is null!");

  // Although these should NEVER be nullptr
  // This is added insurance, to make sure we don't crash in optimized builds
  if (!mPrt || !mPageSeqFrame) {
    return true; // means we are done preparing the page.
  }

  // Check setting to see if someone request it be cancelled
  bool isCancelled = false;
  mPrt->mPrintSettings->GetIsCancelled(&isCancelled);
  if (isCancelled)
    return true;

  // Ask mPageSeqFrame if the page is ready to be printed.
  // If the page doesn't get printed at all, the |done| will be |true|.
  bool done = false;
  nsresult rv = mPageSeqFrame->PrePrintNextPage(mPagePrintTimer, &done);
  if (NS_FAILED(rv)) {
    // ??? ::PrintPage doesn't set |mPrt->mIsAborted = true| if rv != NS_ERROR_ABORT,
    // but I don't really understand why this should be the right thing to do?
    // Shouldn't |mPrt->mIsAborted| set to true all the time if something
    // wents wrong?
    if (rv != NS_ERROR_ABORT) {
      FirePrintingErrorEvent(rv);
      mPrt->mIsAborted = true;
    }
    done = true;
  }
  return done;
}

bool
nsPrintEngine::PrintPage(nsPrintObject*    aPO,
                         bool&           aInRange)
{
  NS_ASSERTION(aPO,            "aPO is null!");
  NS_ASSERTION(mPageSeqFrame,  "mPageSeqFrame is null!");
  NS_ASSERTION(mPrt,           "mPrt is null!");

  // Although these should NEVER be nullptr
  // This is added insurance, to make sure we don't crash in optimized builds
  if (!mPrt || !aPO || !mPageSeqFrame) {
    FirePrintingErrorEvent(NS_ERROR_FAILURE);
    return true; // means we are done printing
  }

  PR_PL(("-----------------------------------\n"));
  PR_PL(("------ In DV::PrintPage PO: %p (%s)\n", aPO, gFrameTypesStr[aPO->mFrameType]));

  // Check setting to see if someone request it be cancelled
  bool isCancelled = false;
  mPrt->mPrintSettings->GetIsCancelled(&isCancelled);
  if (isCancelled || mPrt->mIsAborted)
    return true;

  int32_t pageNum, numPages, endPage;
  mPageSeqFrame->GetCurrentPageNum(&pageNum);
  mPageSeqFrame->GetNumPages(&numPages);

  bool donePrinting;
  bool isDoingPrintRange;
  mPageSeqFrame->IsDoingPrintRange(&isDoingPrintRange);
  if (isDoingPrintRange) {
    int32_t fromPage;
    int32_t toPage;
    mPageSeqFrame->GetPrintRange(&fromPage, &toPage);

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
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep)
    endPage = mPrt->mNumPrintablePages;
  
  mPrt->DoOnProgressChange(++mPrt->mNumPagesPrinted, endPage, false, 0);

  // Print the Page
  // if a print job was cancelled externally, an EndPage or BeginPage may
  // fail and the failure is passed back here.
  // Returning true means we are done printing.
  //
  // When rv == NS_ERROR_ABORT, it means we want out of the
  // print job without displaying any error messages
  nsresult rv = mPageSeqFrame->PrintNextPage();
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_ABORT) {
      FirePrintingErrorEvent(rv);
      mPrt->mIsAborted = true;
    }
    return true;
  }

  mPageSeqFrame->DoPageEnd();

  return donePrinting;
}

/** ---------------------------------------------------
 *  Find by checking frames type
 */
nsresult
nsPrintEngine::FindSelectionBoundsWithList(nsFrameList::Enumerator& aChildFrames,
                                           nsIFrame *      aParentFrame,
                                           nsRect&         aRect,
                                           nsIFrame *&     aStartFrame,
                                           nsRect&         aStartRect,
                                           nsIFrame *&     aEndFrame,
                                           nsRect&         aEndRect)
{
  NS_ASSERTION(aParentFrame, "Pointer is null!");

  aRect += aParentFrame->GetPosition();
  for (; !aChildFrames.AtEnd(); aChildFrames.Next()) {
    nsIFrame* child = aChildFrames.get();
    if (child->IsSelected() && child->IsVisibleForPainting()) {
      nsRect r = child->GetRect();
      if (aStartFrame == nullptr) {
        aStartFrame = child;
        aStartRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
      } else {
        aEndFrame = child;
        aEndRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
      }
    }
    FindSelectionBounds(child, aRect, aStartFrame, aStartRect, aEndFrame, aEndRect);
    child = child->GetNextSibling();
  }
  aRect -= aParentFrame->GetPosition();
  return NS_OK;
}

//-------------------------------------------------------
// Find the Frame that is XMost
nsresult
nsPrintEngine::FindSelectionBounds(nsIFrame *      aParentFrame,
                                   nsRect&         aRect,
                                   nsIFrame *&     aStartFrame,
                                   nsRect&         aStartRect,
                                   nsIFrame *&     aEndFrame,
                                   nsRect&         aEndRect)
{
  NS_ASSERTION(aParentFrame, "Pointer is null!");

  // loop through named child lists
  nsIFrame::ChildListIterator lists(aParentFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    nsresult rv = FindSelectionBoundsWithList(childFrames, aParentFrame, aRect,
      aStartFrame, aStartRect, aEndFrame, aEndRect);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  This method finds the starting and ending page numbers
 *  of the selection and also returns rect for each where
 *  the x,y of the rect is relative to the very top of the
 *  frame tree (absolutely positioned)
 */
nsresult
nsPrintEngine::GetPageRangeForSelection(nsIPageSequenceFrame* aPageSeqFrame,
                                        nsIFrame**            aStartFrame,
                                        int32_t&              aStartPageNum,
                                        nsRect&               aStartRect,
                                        nsIFrame**            aEndFrame,
                                        int32_t&              aEndPageNum,
                                        nsRect&               aEndRect)
{
  NS_ASSERTION(aPageSeqFrame, "Pointer is null!");
  NS_ASSERTION(aStartFrame, "Pointer is null!");
  NS_ASSERTION(aEndFrame, "Pointer is null!");

  nsIFrame * seqFrame = do_QueryFrame(aPageSeqFrame);
  if (!seqFrame) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame * startFrame = nullptr;
  nsIFrame * endFrame   = nullptr;

  // start out with the sequence frame and search the entire frame tree
  // capturing the starting and ending child frames of the selection
  // and their rects
  nsRect r = seqFrame->GetRect();
  FindSelectionBounds(seqFrame, r, startFrame, aStartRect, endFrame, aEndRect);

#ifdef DEBUG_rodsX
  printf("Start Frame: %p\n", startFrame);
  printf("End Frame:   %p\n", endFrame);
#endif

  // initial the page numbers here
  // in case we don't find and frames
  aStartPageNum = -1;
  aEndPageNum   = -1;

  nsIFrame * startPageFrame;
  nsIFrame * endPageFrame;

  // check to make sure we found a starting frame
  if (startFrame != nullptr) {
    // Now search up the tree to find what page the
    // start/ending selections frames are on
    //
    // Check to see if start should be same as end if
    // the end frame comes back null
    if (endFrame == nullptr) {
      // XXX the "GetPageFrame" step could be integrated into
      // the FindSelectionBounds step, but walking up to find
      // the parent of a child frame isn't expensive and it makes
      // FindSelectionBounds a little easier to understand
      startPageFrame = nsLayoutUtils::GetPageFrame(startFrame);
      endPageFrame   = startPageFrame;
      aEndRect       = aStartRect;
    } else {
      startPageFrame = nsLayoutUtils::GetPageFrame(startFrame);
      endPageFrame   = nsLayoutUtils::GetPageFrame(endFrame);
    }
  } else {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG_rodsX
  printf("Start Page: %p\n", startPageFrame);
  printf("End Page:   %p\n", endPageFrame);

  // dump all the pages and their pointers
  {
  int32_t pageNum = 1;
  nsIFrame* child = seqFrame->PrincipalChildList().FirstChild();
  while (child != nullptr) {
    printf("Page: %d - %p\n", pageNum, child);
    pageNum++;
    child = child->GetNextSibling();
  }
  }
#endif

  // Now that we have the page frames
  // find out what the page numbers are for each frame
  int32_t pageNum = 1;
  for (nsIFrame* page : seqFrame->PrincipalChildList()) {
    if (page == startPageFrame) {
      aStartPageNum = pageNum;
    }
    if (page == endPageFrame) {
      aEndPageNum = pageNum;
    }
    pageNum++;
  }

#ifdef DEBUG_rodsX
  printf("Start Page No: %d\n", aStartPageNum);
  printf("End Page No:   %d\n", aEndPageNum);
#endif

  *aStartFrame = startPageFrame;
  *aEndFrame   = endPageFrame;

  return NS_OK;
}

//-----------------------------------------------------------------
//-- Done: Printing Methods
//-----------------------------------------------------------------


//-----------------------------------------------------------------
//-- Section: Misc Support Methods
//-----------------------------------------------------------------

//---------------------------------------------------------------------
void nsPrintEngine::SetIsPrinting(bool aIsPrinting)
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
void nsPrintEngine::SetIsPrintPreview(bool aIsPrintPreview) 
{ 
  mIsDoingPrintPreview = aIsPrintPreview; 

  if (mDocViewerPrint) {
    mDocViewerPrint->SetIsPrintPreview(aIsPrintPreview);
  }
}

//---------------------------------------------------------------------
void
nsPrintEngine::CleanupDocTitleArray(char16_t**& aArray, int32_t& aCount)
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
bool nsPrintEngine::HasFramesetChild(nsIContent* aContent)
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
nsPrintEngine::FindFocusedDOMWindow()
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsPIDOMWindowOuter* window = mDocument->GetWindow();
  NS_ENSURE_TRUE(window, nullptr);

  nsCOMPtr<nsPIDOMWindowOuter> rootWindow = window->GetPrivateRoot();
  NS_ENSURE_TRUE(rootWindow, nullptr);

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsFocusManager::GetFocusedDescendant(rootWindow, true,
                                       getter_AddRefs(focusedWindow));
  NS_ENSURE_TRUE(focusedWindow, nullptr);

  if (IsWindowsInOurSubTree(focusedWindow)) {
    return focusedWindow.forget();
  }

  return nullptr;
}

//---------------------------------------------------------------------
bool
nsPrintEngine::IsWindowsInOurSubTree(nsPIDOMWindowOuter* window)
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
nsPrintEngine::DonePrintingPages(nsPrintObject* aPO, nsresult aResult)
{
  //NS_ASSERTION(aPO, "Pointer is null!");
  PR_PL(("****** In DV::DonePrintingPages PO: %p (%s)\n", aPO, aPO?gFrameTypesStr[aPO->mFrameType]:""));

  // If there is a pageSeqFrame, make sure there are no more printCanvas active
  // that might call |Notify| on the pagePrintTimer after things are cleaned up
  // and printing was marked as being done.
  if (mPageSeqFrame) {
    mPageSeqFrame->ResetPrintCanvasList();
  }

  if (aPO && !mPrt->mIsAborted) {
    aPO->mHasBeenPrinted = true;
    nsresult rv;
    bool didPrint = PrintDocContent(mPrt->mPrintObject, rv);
    if (NS_SUCCEEDED(rv) && didPrint) {
      PR_PL(("****** In DV::DonePrintingPages PO: %p (%s) didPrint:%s (Not Done Printing)\n", aPO, gFrameTypesStr[aPO->mFrameType], PRT_YESNO(didPrint)));
      return false;
    }
  }

  if (NS_SUCCEEDED(aResult)) {
    FirePrintCompletionEvent();
  }

  TurnScriptingOn(true);
  SetIsPrinting(false);

  // Release reference to mPagePrintTimer; the timer object destroys itself
  // after this returns true
  NS_IF_RELEASE(mPagePrintTimer);

  return true;
}

//-------------------------------------------------------
// Recursively sets the PO items to be printed "As Is"
// from the given item down into the tree
void
nsPrintEngine::SetPrintAsIs(nsPrintObject* aPO, bool aAsIs)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  aPO->mPrintAsIs = aAsIs;
  for (uint32_t i=0;i<aPO->mKids.Length();i++) {
    SetPrintAsIs(aPO->mKids[i], aAsIs);
  }
}

//-------------------------------------------------------
// Given a DOMWindow it recursively finds the PO object that matches
nsPrintObject*
nsPrintEngine::FindPrintObjectByDOMWin(nsPrintObject* aPO,
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

  int32_t cnt = aPO->mKids.Length();
  for (int32_t i = 0; i < cnt; ++i) {
    nsPrintObject* po = FindPrintObjectByDOMWin(aPO->mKids[i], aDOMWin);
    if (po) {
      return po;
    }
  }

  return nullptr;
}

//-------------------------------------------------------
nsresult
nsPrintEngine::EnablePOsForPrinting()
{
  // NOTE: All POs have been "turned off" for printing
  // this is where we decided which POs get printed.
  mPrt->mSelectedPO = nullptr;

  if (mPrt->mPrintSettings == nullptr) {
    return NS_ERROR_FAILURE;
  }

  mPrt->mPrintFrameType = nsIPrintSettings::kNoFrames;
  mPrt->mPrintSettings->GetPrintFrameType(&mPrt->mPrintFrameType);

  int16_t printHowEnable = nsIPrintSettings::kFrameEnableNone;
  mPrt->mPrintSettings->GetHowToEnableFrameUI(&printHowEnable);

  int16_t printRangeType = nsIPrintSettings::kRangeAllPages;
  mPrt->mPrintSettings->GetPrintRange(&printRangeType);

  PR_PL(("\n"));
  PR_PL(("********* nsPrintEngine::EnablePOsForPrinting *********\n"));
  PR_PL(("PrintFrameType:     %s \n", gPrintFrameTypeStr[mPrt->mPrintFrameType]));
  PR_PL(("HowToEnableFrameUI: %s \n", gFrameHowToEnableStr[printHowEnable]));
  PR_PL(("PrintRange:         %s \n", gPrintRangeStr[printRangeType]));
  PR_PL(("----\n"));

  // ***** This is the ultimate override *****
  // if we are printing the selection (either an IFrame or selection range)
  // then set the mPrintFrameType as if it were the selected frame
  if (printRangeType == nsIPrintSettings::kRangeSelection) {
    mPrt->mPrintFrameType = nsIPrintSettings::kSelectedFrame;
    printHowEnable        = nsIPrintSettings::kFrameEnableNone;
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
      SetPrintPO(mPrt->mPrintObject, true);

      // Set the children so they are PrinAsIs
      // In this case, the children are probably IFrames
      if (mPrt->mPrintObject->mKids.Length() > 0) {
        for (uint32_t i=0;i<mPrt->mPrintObject->mKids.Length();i++) {
          nsPrintObject* po = mPrt->mPrintObject->mKids[i];
          NS_ASSERTION(po, "nsPrintObject can't be null!");
          SetPrintAsIs(po);
        }

        // ***** Another override *****
        mPrt->mPrintFrameType = nsIPrintSettings::kFramesAsIs;
      }
      PR_PL(("PrintFrameType:     %s \n", gPrintFrameTypeStr[mPrt->mPrintFrameType]));
      PR_PL(("HowToEnableFrameUI: %s \n", gFrameHowToEnableStr[printHowEnable]));
      PR_PL(("PrintRange:         %s \n", gPrintRangeStr[printRangeType]));
      return NS_OK;
    }

    // This means we are either printed a selected IFrame or
    // we are printing the current selection
    if (printRangeType == nsIPrintSettings::kRangeSelection) {

      // If the currentFocusDOMWin can'r be null if something is selected
      if (mPrt->mCurrentFocusWin) {
        // Find the selected IFrame
        nsPrintObject * po = FindPrintObjectByDOMWin(mPrt->mPrintObject, mPrt->mCurrentFocusWin);
        if (po != nullptr) {
          mPrt->mSelectedPO = po;
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
            mPrt->mPrintSettings->SetPrintRange(printRangeType);
          }
          PR_PL(("PrintFrameType:     %s \n", gPrintFrameTypeStr[mPrt->mPrintFrameType]));
          PR_PL(("HowToEnableFrameUI: %s \n", gFrameHowToEnableStr[printHowEnable]));
          PR_PL(("PrintRange:         %s \n", gPrintRangeStr[printRangeType]));
          return NS_OK;
        }
      } else {
        for (uint32_t i=0;i<mPrt->mPrintDocList.Length();i++) {
          nsPrintObject* po = mPrt->mPrintDocList.ElementAt(i);
          NS_ASSERTION(po, "nsPrintObject can't be null!");
          nsCOMPtr<nsPIDOMWindowOuter> domWin = po->mDocShell->GetWindow();
          if (IsThereARangeSelection(domWin)) {
            mPrt->mCurrentFocusWin = domWin.forget();
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
    if (mPrt->mCurrentFocusWin) {
      // Find the selected IFrame
      nsPrintObject * po = FindPrintObjectByDOMWin(mPrt->mPrintObject, mPrt->mCurrentFocusWin);
      if (po != nullptr) {
        mPrt->mSelectedPO = po;
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
          mPrt->mPrintSettings->SetPrintRange(printRangeType);
        }
        PR_PL(("PrintFrameType:     %s \n", gPrintFrameTypeStr[mPrt->mPrintFrameType]));
        PR_PL(("HowToEnableFrameUI: %s \n", gFrameHowToEnableStr[printHowEnable]));
        PR_PL(("PrintRange:         %s \n", gPrintRangeStr[printRangeType]));
        return NS_OK;
      }
    }
  }

  // If we are printing "AsIs" then sets all the POs to be printed as is
  if (mPrt->mPrintFrameType == nsIPrintSettings::kFramesAsIs) {
    SetPrintAsIs(mPrt->mPrintObject);
    SetPrintPO(mPrt->mPrintObject, true);
    return NS_OK;
  }

  // If we are printing the selected Frame then
  // find that PO for that selected DOMWin and set it all of its
  // children to be printed
  if (mPrt->mPrintFrameType == nsIPrintSettings::kSelectedFrame) {

    if ((mPrt->mIsParentAFrameSet && mPrt->mCurrentFocusWin) || mPrt->mIsIFrameSelected) {
      nsPrintObject * po = FindPrintObjectByDOMWin(mPrt->mPrintObject, mPrt->mCurrentFocusWin);
      if (po != nullptr) {
        mPrt->mSelectedPO = po;
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
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    SetPrintPO(mPrt->mPrintObject, true);
    int32_t cnt = mPrt->mPrintDocList.Length();
    for (int32_t i=0;i<cnt;i++) {
      nsPrintObject* po = mPrt->mPrintDocList.ElementAt(i);
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
nsPrintEngine::FindSmallestSTF()
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
nsPrintEngine::TurnScriptingOn(bool aDoTurnOn)
{
  if (mIsDoingPrinting && aDoTurnOn && mDocViewerPrint &&
      mDocViewerPrint->GetIsPrintPreview()) {
    // We don't want to turn scripting on if print preview is shown still after
    // printing.
    return;
  }

  nsPrintData* prt = mPrt;
#ifdef NS_PRINT_PREVIEW
  if (!prt) {
    prt = mPrtPreview;
  }
#endif
  if (!prt) {
    return;
  }

  NS_ASSERTION(mDocument, "We MUST have a document.");
  // First, get the script global object from the document...

  for (uint32_t i=0;i<prt->mPrintDocList.Length();i++) {
    nsPrintObject* po = prt->mPrintDocList.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");

    nsIDocument* doc = po->mDocument;
    if (!doc) {
      continue;
    }

    if (nsCOMPtr<nsPIDOMWindowInner> window = doc->GetInnerWindow()) {
      nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(window);
      NS_WARN_IF_FALSE(go && go->GetGlobalJSObject(), "Can't get global");
      nsresult propThere = NS_PROPTABLE_PROP_NOT_THERE;
      doc->GetProperty(nsGkAtoms::scriptEnabledBeforePrintOrPreview,
                       &propThere);
      if (aDoTurnOn) {
        if (propThere != NS_PROPTABLE_PROP_NOT_THERE) {
          doc->DeleteProperty(nsGkAtoms::scriptEnabledBeforePrintOrPreview);
          if (go && go->GetGlobalJSObject()) {
            xpc::Scriptability::Get(go->GetGlobalJSObject()).Unblock();
          }
          window->ResumeTimeouts(false);
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
          window->SuspendTimeouts(1, false);
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
nsPrintEngine::CloseProgressDialog(nsIWebProgressListener* aWebProgressListener)
{
  if (aWebProgressListener) {
    aWebProgressListener->OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_DOCUMENT, NS_OK);
  }
}

//-----------------------------------------------------------------
nsresult
nsPrintEngine::FinishPrintPreview()
{
  nsresult rv = NS_OK;

#ifdef NS_PRINT_PREVIEW

  if (!mPrt) {
    /* we're already finished with print preview */
    return rv;
  }

  rv = DocumentReadyForPrinting();

  SetIsCreatingPrintPreview(false);

  /* cleaup on failure + notify user */
  if (NS_FAILED(rv)) {
    /* cleanup done, let's fire-up an error dialog to notify the user
     * what went wrong...
     */
    mPrt->OnEndPrinting();
    TurnScriptingOn(true);

    return rv;
  }

  // At this point we are done preparing everything
  // before it is to be created


  if (mIsDoingPrintPreview && mOldPrtPreview) {
    delete mOldPrtPreview;
    mOldPrtPreview = nullptr;
  }


  mPrt->OnEndPrinting();

  // PrintPreview was built using the mPrt (code reuse)
  // then we assign it over
  mPrtPreview = mPrt;
  mPrt        = nullptr;

#endif // NS_PRINT_PREVIEW

  return NS_OK;
}

//-----------------------------------------------------------------
//-- Done: Finishing up or Cleaning up
//-----------------------------------------------------------------


/*=============== Timer Related Code ======================*/
nsresult
nsPrintEngine::StartPagePrintTimer(nsPrintObject* aPO)
{
  if (!mPagePrintTimer) {
    // Get the delay time in between the printing of each page
    // this gives the user more time to press cancel
    int32_t printPageDelay = 50;
    mPrt->mPrintSettings->GetPrintPageDelay(&printPageDelay);

    RefPtr<nsPagePrintTimer> timer =
      new nsPagePrintTimer(this, mDocViewerPrint, printPageDelay);
    timer.forget(&mPagePrintTimer);

    nsCOMPtr<nsIPrintSession> printSession;
    nsresult rv = mPrt->mPrintSettings->GetPrintSession(getter_AddRefs(printSession));
    if (NS_SUCCEEDED(rv) && printSession) {
      RefPtr<mozilla::layout::RemotePrintJobChild> remotePrintJob;
      printSession->GetRemotePrintJob(getter_AddRefs(remotePrintJob));
      if (NS_SUCCEEDED(rv) && remotePrintJob) {
        remotePrintJob->SetPagePrintTimer(mPagePrintTimer);
        remotePrintJob->SetPrintEngine(this);
      }
    }
  }

  return mPagePrintTimer->Start(aPO);
}

/*=============== nsIObserver Interface ======================*/
NS_IMETHODIMP 
nsPrintEngine::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData)
{
  nsresult rv = NS_ERROR_FAILURE;

  rv = InitPrintDocConstruction(true);
  if (!mIsDoingPrinting && mPrtPreview) {
      mPrtPreview->OnEndPrinting();
  }

  return rv;

}

//---------------------------------------------------------------
//-- PLEvent Notification
//---------------------------------------------------------------
class nsPrintCompletionEvent : public Runnable {
public:
  explicit nsPrintCompletionEvent(nsIDocumentViewerPrint *docViewerPrint)
    : mDocViewerPrint(docViewerPrint) {
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
nsPrintEngine::FirePrintCompletionEvent()
{
  nsCOMPtr<nsIRunnable> event = new nsPrintCompletionEvent(mDocViewerPrint);
  if (NS_FAILED(NS_DispatchToCurrentThread(event)))
    NS_WARNING("failed to dispatch print completion event");
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
                       nsRenderingContext * aRendContext,
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
    nsIPresShell* shell = nsPrintEngine::GetPresShellFor(aDocShell);
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
static void GetDocTitleAndURL(nsPrintObject* aPO, nsACString& aDocStr, nsACString& aURLStr)
{
  nsAutoString docTitleStr;
  nsAutoString docURLStr;
  nsPrintEngine::GetDisplayTitleAndURL(aPO,
                                       docTitleStr, docURLStr,
                                       nsPrintEngine::eDocTitleDefURLDoc);
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
#define DUMP_DOC_TREE DumpPrintObjectsTree(mPrt->mPrintObject);
#define DUMP_DOC_TREELAYOUT DumpPrintObjectsTreeLayout(mPrt->mPrintObject, mPrt->mPrintDC);

#else
#define DUMP_DOC_LIST(_title)
#define DUMP_DOC_TREE
#define DUMP_DOC_TREELAYOUT
#endif

//---------------------------------------------------------------
//---------------------------------------------------------------
//-- End of debug helper routines
//---------------------------------------------------------------
