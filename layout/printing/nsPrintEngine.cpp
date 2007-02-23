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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#include "nsPrintEngine.h"

#include "nsIStringBundle.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#include "nsISelection.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIURI.h"
#include "nsContentErrors.h"

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIPrintOptions.h"
#include "nsIPrintSession.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsISimpleEnumerator.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"

// PrintOptions is now implemented by PrintSettingsService
static const char sPrintSettingsServiceContractID[] = "@mozilla.org/gfx/printsettings-service;1";
static const char sPrintOptionsContractID[]         = "@mozilla.org/gfx/printsettings-service;1";

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
#include "nsIStringBundle.h"

// Printing Prompts
#include "nsIPrintingPromptService.h"
static const char kPrintingPromptService[] = "@mozilla.org/embedcomp/printingprompt-service;1";

// Printing Timer
#include "nsPagePrintTimer.h"

// FrameSet
#include "nsIDocument.h"

// Focus
#include "nsIDOMEventReceiver.h"
#include "nsIDOMFocusListener.h"
#include "nsISelectionController.h"

// Misc
#include "nsISupportsUtils.h"
#include "nsIFrame.h"
#include "nsIScriptContext.h"
#include "nsILinkHandler.h"
#include "nsIDOMDocument.h"
#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMRange.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsContentUtils.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"

#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIViewManager.h"
#include "nsIView.h"

#include "nsIPageSequenceFrame.h"
#include "nsIURL.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewerFile.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIFrameDebug.h"
#include "nsILayoutHistoryState.h"
#include "nsFrameManager.h"
#include "nsIParser.h"
#include "nsGUIEvent.h"
#include "nsHTMLReflowState.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewerPrint.h"

#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"

#include "nsCDefaultURIFixup.h"
#include "nsIURIFixup.h"

//-----------------------------------------------------
// PR LOGGING
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "prlog.h"

#ifdef PR_LOGGING

#ifdef NS_DEBUG
// PR_LOGGING is force to always be on (even in release builds)
// but we only want some of it on,
//#define EXTENDED_DEBUG_PRINTING 
#endif

#define DUMP_LAYOUT_LEVEL 9 // this turns on the dumping of each doucment's layout info

static PRLogModuleInfo * kPrintingLogMod = PR_NewLogModule("printing");
#define PR_PL(_p1)  PR_LOG(kPrintingLogMod, PR_LOG_DEBUG, _p1);

#ifdef EXTENDED_DEBUG_PRINTING
static PRUint32 gDumpFileNameCnt   = 0;
static PRUint32 gDumpLOFileNameCnt = 0;
#endif

#define PRT_YESNO(_p) ((_p)?"YES":"NO")
static const char * gFrameTypesStr[]       = {"eDoc", "eFrame", "eIFrame", "eFrameSet"};
static const char * gPrintFrameTypeStr[]   = {"kNoFrames", "kFramesAsIs", "kSelectedFrame", "kEachFrameSep"};
static const char * gFrameHowToEnableStr[] = {"kFrameEnableNone", "kFrameEnableAll", "kFrameEnableAsIsAndEach"};
static const char * gPrintRangeStr[]       = {"kRangeAllPages", "kRangeSpecifiedPageRange", "kRangeSelection", "kRangeFocusFrame"};
#else
#define PRT_YESNO(_p)
#define PR_PL(_p1)
#endif

#ifdef EXTENDED_DEBUG_PRINTING
// Forward Declarations
static void DumpPrintObjectsListStart(const char * aStr, nsVoidArray * aDocList);
static void DumpPrintObjectsTree(nsPrintObject * aPO, int aLevel= 0, FILE* aFD = nsnull);
static void DumpPrintObjectsTreeLayout(nsPrintObject * aPO,nsIDeviceContext * aDC, int aLevel= 0, FILE * aFD = nsnull);

#define DUMP_DOC_LIST(_title) DumpPrintObjectsListStart((_title), mPrt->mPrintDocList);
#define DUMP_DOC_TREE DumpPrintObjectsTree(mPrt->mPrintObject);
#define DUMP_DOC_TREELAYOUT DumpPrintObjectsTreeLayout(mPrt->mPrintObject, mPrt->mPrintDC);
#else
#define DUMP_DOC_LIST(_title)
#define DUMP_DOC_TREE
#define DUMP_DOC_TREELAYOUT
#endif


// Class IDs
static NS_DEFINE_CID(kViewManagerCID,       NS_VIEW_MANAGER_CID);
static NS_DEFINE_CID(kWidgetCID,            NS_CHILD_CID);

NS_IMPL_ISUPPORTS1(nsPrintEngine, nsIObserver)

//---------------------------------------------------
//-- nsPrintEngine Class Impl
//---------------------------------------------------
nsPrintEngine::nsPrintEngine() :
  mIsCreatingPrintPreview(PR_FALSE),
  mIsDoingPrinting(PR_FALSE),
  mDocViewerPrint(nsnull),
  mContainer(nsnull),
  mDeviceContext(nsnull),
  mPrt(nsnull),
  mPagePrintTimer(nsnull),
  mPageSeqFrame(nsnull),
  mIsDoingPrintPreview(PR_FALSE),
  mParentWidget(nsnull),
  mPrtPreview(nsnull),
  mOldPrtPreview(nsnull),
  mDebugFile(nsnull),
  mProgressDialogIsShown(PR_FALSE)
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
  if (mPrt) {
    delete mPrt;
    mPrt = nsnull;
  }

#ifdef NS_PRINT_PREVIEW
  if (mPrtPreview) {
    delete mPrtPreview;
    mPrtPreview = nsnull;
  }

  // This is insruance
  if (mOldPrtPreview) {
    delete mOldPrtPreview;
    mOldPrtPreview = nsnull;
  }

#endif

}

//-------------------------------------------------------
void nsPrintEngine::DestroyPrintingData()
{
  if (mPrt) {
    delete mPrt;
    mPrt = nsnull;
  }
}

//---------------------------------------------------------------------------------
//-- Section: Methods needed by the DocViewer
//---------------------------------------------------------------------------------

//--------------------------------------------------------
nsresult nsPrintEngine::Initialize(nsIDocumentViewerPrint* aDocViewerPrint, 
                                   nsISupports*            aContainer,
                                   nsIDocument*            aDocument,
                                   nsIDeviceContext*       aDevContext,
                                   nsIWidget*              aParentWidget,
                                   FILE*                   aDebugFile)
{
  NS_ENSURE_ARG_POINTER(aDocViewerPrint);
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aDevContext);
  NS_ENSURE_ARG_POINTER(aParentWidget);

  mDocViewerPrint = aDocViewerPrint; // weak reference
  mContainer      = aContainer;      // weak reference
  mDocument       = aDocument;
  mDeviceContext  = aDevContext;     // weak reference
  mParentWidget   = aParentWidget;    

  mDebugFile      = aDebugFile;      // ok to be NULL

  return NS_OK;
}

//-------------------------------------------------------
PRBool
nsPrintEngine::CheckBeforeDestroy()
{
  if (mPrt && mPrt->mPreparingForPrint) {
    mPrt->mDocWasToBeDestroyed = PR_TRUE;
    return PR_TRUE;
  }
  return PR_FALSE;
}

//-------------------------------------------------------
nsresult
nsPrintEngine::Cancelled()
{
  if (mPrt && mPrt->mPrintSettings) {
    return mPrt->mPrintSettings->SetIsCancelled(PR_TRUE);
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
    nsCOMPtr<nsPIDOMWindow> win(do_GetInterface(mContainer));
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(win->GetFrameElementInternal()));
    mPrt->mPPEventListeners = new nsPrintPreviewListener(target);

    if (mPrt->mPPEventListeners) {
      mPrt->mPPEventListeners->AddListeners();
    }
  }
}

//----------------------------------------------------------------------
nsresult 
nsPrintEngine::GetSeqFrameAndCountPagesInternal(nsPrintObject*  aPO,
                                                nsIFrame*&    aSeqFrame,
                                                PRInt32&      aCount)
{
  NS_ENSURE_ARG_POINTER(aPO);

  // Finds the SimplePageSequencer frame
  nsIPageSequenceFrame* seqFrame = nsnull;
  aPO->mPresShell->GetPageSequenceFrame(&seqFrame);
  if (seqFrame) {
    CallQueryInterface(seqFrame, &aSeqFrame);
  } else {
    aSeqFrame = nsnull;
  }
  if (aSeqFrame == nsnull) return NS_ERROR_FAILURE;

  // first count the total number of pages
  aCount = 0;
  nsIFrame* pageFrame = aSeqFrame->GetFirstChild(nsnull);
  while (pageFrame != nsnull) {
    aCount++;
    pageFrame = pageFrame->GetNextSibling();
  }

  return NS_OK;

}

//-----------------------------------------------------------------
nsresult nsPrintEngine::GetSeqFrameAndCountPages(nsIFrame*& aSeqFrame, PRInt32& aCount)
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
static void DumpPrintObjectsList(nsVoidArray * aDocList);
static void RootFrameList(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent);
static void DumpViews(nsIDocShell* aDocShell, FILE* out);
static void DumpLayoutData(char* aTitleStr, char* aURLStr,
                           nsPresContext* aPresContext,
                           nsIDeviceContext * aDC, nsIFrame * aRootFrame,
                           nsIDocShell * aDocShell, FILE* aFD);
#endif

//--------------------------------------------------------------------------------

nsresult
nsPrintEngine::CommonPrint(PRBool                  aIsPrintPreview,
                           nsIPrintSettings*       aPrintSettings,
                           nsIWebProgressListener* aWebProgressListener) {
  nsresult rv = DoCommonPrint(aIsPrintPreview, aPrintSettings,
                              aWebProgressListener);
  if (NS_FAILED(rv)) {
    if (aIsPrintPreview) {
      SetIsCreatingPrintPreview(PR_FALSE);
      SetIsPrintPreview(PR_FALSE);
    } else {
      SetIsPrinting(PR_FALSE);
    }
    if (mProgressDialogIsShown)
      CloseProgressDialog(aWebProgressListener);
    if (rv != NS_ERROR_ABORT && rv != NS_ERROR_OUT_OF_MEMORY)
      ShowPrintErrorDialog(rv, !aIsPrintPreview);
    delete mPrt;
    mPrt = nsnull;
  }

  return rv;
}

nsresult
nsPrintEngine::DoCommonPrint(PRBool                  aIsPrintPreview,
                             nsIPrintSettings*       aPrintSettings,
                             nsIWebProgressListener* aWebProgressListener)
{
  nsresult rv;

  if (aIsPrintPreview) {
    // The WebProgressListener can be QI'ed to nsIPrintingPromptService
    // then that means the progress dialog is already being shown.
    nsCOMPtr<nsIPrintingPromptService> pps(do_QueryInterface(aWebProgressListener));
    mProgressDialogIsShown = pps != nsnull;

    if (mIsDoingPrintPreview) {
      mOldPrtPreview = mPrtPreview;
      mPrtPreview = nsnull;
    }
  } else {
    mProgressDialogIsShown = PR_FALSE;
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

  mPrt->mPrintOptions = do_GetService(sPrintOptionsContractID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = CheckForPrinters(mPrt->mPrintOptions, mPrt->mPrintSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  mPrt->mPrintSettings->SetIsCancelled(PR_FALSE);
  mPrt->mPrintSettings->GetShrinkToFit(&mPrt->mShrinkToFit);

  if (aIsPrintPreview) {
    SetIsCreatingPrintPreview(PR_TRUE);
    SetIsPrintPreview(PR_TRUE);
  } else {
    SetIsPrinting(PR_TRUE);
  }

  if (aWebProgressListener != nsnull) {
    mPrt->mPrintProgressListeners.AppendObject(aWebProgressListener);
  }

  // Get the currently focused window and cache it
  // because the Print Dialog will "steal" focus and later when you try
  // to get the currently focused windows it will be NULL
  mPrt->mCurrentFocusWin = FindFocusedDOMWindow();

  // Check to see if there is a "regular" selection
  PRBool isSelection = IsThereARangeSelection(mPrt->mCurrentFocusWin);

  mPrt->mPrintDocList = new nsVoidArray();
  NS_ENSURE_TRUE(mPrt->mPrintDocList, NS_ERROR_OUT_OF_MEMORY);

  // Get the docshell for this documentviewer
  nsCOMPtr<nsIDocShell> webContainer(do_QueryInterface(mContainer, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  mPrt->mPrintObject = new nsPrintObject();
  NS_ENSURE_TRUE(mPrt->mPrintObject, NS_ERROR_OUT_OF_MEMORY);
  rv = mPrt->mPrintObject->Init(webContainer);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(mPrt->mPrintDocList->AppendElement(mPrt->mPrintObject),
                 NS_ERROR_OUT_OF_MEMORY);

  mPrt->mIsParentAFrameSet = IsParentAFrameSet(webContainer);
  mPrt->mPrintObject->mFrameType = mPrt->mIsParentAFrameSet ? eFrameSet : eDoc;

  // Build the "tree" of PrintObjects
  nsCOMPtr<nsIDocShellTreeNode>  parentAsNode(do_QueryInterface(webContainer));
  BuildDocTree(parentAsNode, mPrt->mPrintDocList, mPrt->mPrintObject);

  // XXX This isn't really correct...
  if (!mPrt->mPrintObject->mDocument->GetRootContent())
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
  mPrt->mPrintSettings->SetPrintOptions(nsIPrintSettings::kEnableSelectionRB, isSelection || mPrt->mIsIFrameSelected);

  nsCOMPtr<nsIDeviceContextSpec> devspec
    (do_CreateInstance("@mozilla.org/gfx/devicecontextspec;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aIsPrintPreview) {
#ifdef NS_DEBUG
    mPrt->mDebugFilePtr = mDebugFile;
#endif

    PRBool printSilently;
    mPrt->mPrintSettings->GetPrintSilent(&printSilently);

    // Check prefs for a default setting as to whether we should print silently
    printSilently = nsContentUtils::GetBoolPref("print.always_print_silent",
                                                printSilently);

    // Ask dialog to be Print Shown via the Plugable Printing Dialog Service
    // This service is for the Print Dialog and the Print Progress Dialog
    // If printing silently or you can't get the service continue on
    if (!printSilently) {
      nsCOMPtr<nsIPrintingPromptService> printPromptService(do_GetService(kPrintingPromptService));
      if (printPromptService) {
        nsIDOMWindow *domWin = mDocument->GetWindow(); 
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
        if (rv == NS_ERROR_NOT_IMPLEMENTED) {
          // This means the Dialog service was there, 
          // but they choose not to implement this dialog and 
          // are looking for default behavior from the toolkit
          rv = NS_OK;
        } else if (NS_SUCCEEDED(rv)) {
          // since we got the dialog and it worked then make sure we 
          // are telling GFX we want to print silent
          printSilently = PR_TRUE;
        }
      } else {
        rv = NS_ERROR_GFX_NO_PRINTROMPTSERVICE;
      }
    }
    // Check explicitly for abort because it's expected
    if (rv == NS_ERROR_ABORT) 
      return rv;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = devspec->Init(nsnull, mPrt->mPrintSettings, aIsPrintPreview);
  NS_ENSURE_SUCCESS(rv, rv);

  mPrt->mPrintDC = do_CreateInstance("@mozilla.org/gfx/devicecontext;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
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
    PRInt16 printFrameTypeUsage = nsIPrintSettings::kUseSettingWhenPossible;
    mPrt->mPrintSettings->GetPrintFrameTypeUsage(&printFrameTypeUsage);

    // Ok, see if we are going to use our value and override the default
    if (printFrameTypeUsage == nsIPrintSettings::kUseSettingWhenPossible) {
      // Get the Print Options/Settings PrintFrameType to see what is preferred
      PRInt16 printFrameType = nsIPrintSettings::kEachFrameSep;
      mPrt->mPrintSettings->GetPrintFrameType(&printFrameType);

      // Don't let anybody do something stupid like try to set it to
      // kNoFrames when we are printing a FrameSet
      if (printFrameType == nsIPrintSettings::kNoFrames) {
        mPrt->mPrintFrameType = nsIPrintSettings::kEachFrameSep;
        mPrt->mPrintSettings->SetPrintFrameType(mPrt->mPrintFrameType);
      } else {
        // First find out from the PrinService what options are available
        // to us for Printing FrameSets
        PRInt16 howToEnableFrameUI;
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

  if (aIsPrintPreview) {
    PRBool notifyOnInit = PR_FALSE;
    ShowPrintProgress(PR_FALSE, notifyOnInit);

    // Very important! Turn Off scripting
    TurnScriptingOn(PR_FALSE);

    if (!notifyOnInit) {
      rv = FinishPrintPreview();
    } else {
      rv = NS_OK;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    PRUnichar * docTitleStr;
    PRUnichar * docURLStr;

    GetDisplayTitleAndURL(mPrt->mPrintObject, &docTitleStr, &docURLStr, eDocTitleDefURLDoc); 

    // Nobody ever cared about the file name passed in, as far as I can tell
    rv = mPrt->mPrintDC->PrepareDocument(docTitleStr, nsnull);

    if (docTitleStr) nsMemory::Free(docTitleStr);
    if (docURLStr) nsMemory::Free(docURLStr);

    NS_ENSURE_SUCCESS(rv, rv);

    PRBool doNotify;
    ShowPrintProgress(PR_TRUE, doNotify);
    if (!doNotify) {
      // Print listener setup...
      mPrt->OnStartPrinting();    
      rv = DocumentReadyForPrinting();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

//---------------------------------------------------------------------------------
NS_IMETHODIMP
nsPrintEngine::Print(nsIPrintSettings*       aPrintSettings,
                     nsIWebProgressListener* aWebProgressListener)
{
  return CommonPrint(PR_FALSE, aPrintSettings, aWebProgressListener);
}

/** ---------------------------------------------------
 *  See documentation above in the nsIContentViewerfile class definition
 *	@update 11/01/01 rods
 *
 *  For a full and detailed understanding of the issues with
 *  PrintPreview: See the design spec that is attached to Bug 107562
 */
NS_IMETHODIMP
nsPrintEngine::PrintPreview(nsIPrintSettings* aPrintSettings, 
                                 nsIDOMWindow *aChildDOMWin, 
                                 nsIWebProgressListener* aWebProgressListener)
{
  return CommonPrint(PR_TRUE, aPrintSettings, aWebProgressListener);
}

//----------------------------------------------------------------------------------
/* readonly attribute boolean isFramesetDocument; */
NS_IMETHODIMP
nsPrintEngine::GetIsFramesetDocument(PRBool *aIsFramesetDocument)
{
  nsCOMPtr<nsIDocShell> webContainer(do_QueryInterface(mContainer));
  *aIsFramesetDocument = IsParentAFrameSet(webContainer);
  return NS_OK;
}

//----------------------------------------------------------------------------------
/* readonly attribute boolean isIFrameSelected; */
NS_IMETHODIMP 
nsPrintEngine::GetIsIFrameSelected(PRBool *aIsIFrameSelected)
{
  *aIsIFrameSelected = PR_FALSE;

  // Get the docshell for this documentviewer
  nsCOMPtr<nsIDocShell> webContainer(do_QueryInterface(mContainer));
  // Get the currently focused window
  nsCOMPtr<nsIDOMWindow> currentFocusWin = FindFocusedDOMWindow();
  if (currentFocusWin && webContainer) {
    // Get whether the doc contains a frameset 
    // Also, check to see if the currently focus docshell
    // is a child of this docshell
    PRPackedBool isParentFrameSet;
    *aIsIFrameSelected = IsThereAnIFrameSelected(webContainer, currentFocusWin, isParentFrameSet);
  }
  return NS_OK;
}

//----------------------------------------------------------------------------------
/* readonly attribute boolean isRangeSelection; */
NS_IMETHODIMP 
nsPrintEngine::GetIsRangeSelection(PRBool *aIsRangeSelection)
{
  // Get the currently focused window 
  nsCOMPtr<nsIDOMWindow> currentFocusWin = FindFocusedDOMWindow();
  *aIsRangeSelection = IsThereARangeSelection(currentFocusWin);
  return NS_OK;
}

//----------------------------------------------------------------------------------
/* readonly attribute boolean isFramesetFrameSelected; */
NS_IMETHODIMP 
nsPrintEngine::GetIsFramesetFrameSelected(PRBool *aIsFramesetFrameSelected)
{
  // Get the currently focused window 
  nsCOMPtr<nsIDOMWindow> currentFocusWin = FindFocusedDOMWindow();
  *aIsFramesetFrameSelected = currentFocusWin != nsnull;
  return NS_OK;
}

//----------------------------------------------------------------------------------
/* readonly attribute long printPreviewNumPages; */
NS_IMETHODIMP
nsPrintEngine::GetPrintPreviewNumPages(PRInt32 *aPrintPreviewNumPages)
{
  NS_ENSURE_ARG_POINTER(aPrintPreviewNumPages);

  nsIFrame* seqFrame  = nsnull;
  *aPrintPreviewNumPages = 0;
  if (!mPrtPreview ||
      NS_FAILED(GetSeqFrameAndCountPagesInternal(mPrtPreview->mPrintObject, seqFrame, *aPrintPreviewNumPages))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//----------------------------------------------------------------------------------
// Enumerate all the documents for their titles
NS_IMETHODIMP
nsPrintEngine::EnumerateDocumentNames(PRUint32* aCount,
                                      PRUnichar*** aResult)
{
  NS_ENSURE_ARG(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  *aCount = 0;
  *aResult = nsnull;

  PRInt32     numDocs = mPrt->mPrintDocList->Count();
  PRUnichar** array   = (PRUnichar**) nsMemory::Alloc(numDocs * sizeof(PRUnichar*));
  if (!array)
    return NS_ERROR_OUT_OF_MEMORY;

  for (PRInt32 i=0;i<numDocs;i++) {
    nsPrintObject* po = (nsPrintObject*)mPrt->mPrintDocList->ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    PRUnichar * docTitleStr;
    PRUnichar * docURLStr;
    GetDocumentTitleAndURL(po->mDocument, &docTitleStr, &docURLStr);

    // Use the URL if the doc is empty
    if (!docTitleStr || !*docTitleStr) {
      if (docURLStr && *docURLStr) {
        nsMemory::Free(docTitleStr);
        docTitleStr = docURLStr;
      } else {
        nsMemory::Free(docURLStr);
      }
      docURLStr = nsnull;
      if (!docTitleStr || !*docTitleStr) {
        CleanupDocTitleArray(array, i);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    array[i] = docTitleStr;
    if (docURLStr) nsMemory::Free(docURLStr);
  }
  *aCount  = numDocs;
  *aResult = array;

  return NS_OK;

}

//----------------------------------------------------------------------------------
/* readonly attribute nsIPrintSettings globalPrintSettings; */
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
/* readonly attribute boolean doingPrint; */
NS_IMETHODIMP
nsPrintEngine::GetDoingPrint(PRBool *aDoingPrint)
{
  NS_ENSURE_ARG_POINTER(aDoingPrint);
  *aDoingPrint = mIsDoingPrinting;
  return NS_OK;
}

//----------------------------------------------------------------------------------
/* readonly attribute boolean doingPrintPreview; */
NS_IMETHODIMP
nsPrintEngine::GetDoingPrintPreview(PRBool *aDoingPrintPreview)
{
  NS_ENSURE_ARG_POINTER(aDoingPrintPreview);
  *aDoingPrintPreview = mIsDoingPrintPreview;
  return NS_OK;
}

//----------------------------------------------------------------------------------
/* readonly attribute nsIPrintSettings currentPrintSettings; */
NS_IMETHODIMP
nsPrintEngine::GetCurrentPrintSettings(nsIPrintSettings * *aCurrentPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aCurrentPrintSettings);

  if (mPrt) {
    *aCurrentPrintSettings = mPrt->mPrintSettings;

  } else if (mPrtPreview) {
    *aCurrentPrintSettings = mPrtPreview->mPrintSettings;

  } else {
    *aCurrentPrintSettings = nsnull;
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
nsPrintEngine::CheckForPrinters(nsIPrintOptions*  aPrintOptions,
                                nsIPrintSettings* aPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aPrintOptions);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> simpEnum;
  rv = aPrintOptions->AvailablePrinters(getter_AddRefs(simpEnum));
  if (simpEnum) {
    PRBool fndPrinter = PR_FALSE;
    simpEnum->HasMoreElements(&fndPrinter);
    if (fndPrinter) {
      // For now, it assumes the first item in the list
      // is the default printer, but only set the
      // printer name if there isn't one
      nsCOMPtr<nsISupports> supps;
      simpEnum->GetNext(getter_AddRefs(supps));
      PRUnichar* defPrinterName;
      aPrintSettings->GetPrinterName(&defPrinterName);
      if (!defPrinterName || !*defPrinterName) {
        if (defPrinterName) nsMemory::Free(defPrinterName);
        nsCOMPtr<nsISupportsString> wStr = do_QueryInterface(supps);
        if (wStr) {
          wStr->ToString(&defPrinterName);
          aPrintSettings->SetPrinterName(defPrinterName);
          nsMemory::Free(defPrinterName);
        }
      } else {
        nsMemory::Free(defPrinterName);
      }
      rv = NS_OK;
    }
  } else {
    // this means there were no printers
    // XXX the ifdefs are temporary until they correctly implement Available Printers
#if defined(XP_MAC) || defined(XP_MACOSX)
    rv = NS_OK;
#endif
  }
  return rv;
}

//----------------------------------------------------------------------
// Set up to use the "pluggable" Print Progress Dialog
void
nsPrintEngine::ShowPrintProgress(PRBool aIsForPrinting, PRBool& aDoNotify)
{
  // default to not notifying, that if something here goes wrong
  // or we aren't going to show the progress dialog we can straight into 
  // reflowing the doc for printing.
  aDoNotify = PR_FALSE;

  // Assume we can't do progress and then see if we can
  PRBool showProgresssDialog = PR_FALSE;

  // if it is already being shown then don't bother to find out if it should be
  // so skip this and leave mShowProgressDialog set to FALSE
  if (!mProgressDialogIsShown) {
    showProgresssDialog =
      nsContentUtils::GetBoolPref("print.show_print_progress");
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
      nsPIDOMWindow *domWin = mDocument->GetWindow(); 
      if (!domWin) return;

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
PRBool
nsPrintEngine::IsThereARangeSelection(nsIDOMWindow* aDOMWin)
{
  nsCOMPtr<nsIPresShell> presShell;
  if (aDOMWin) {
    nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aDOMWin));
    window->GetDocShell()->GetPresShell(getter_AddRefs(presShell));
  }

  if (!presShell)
    return PR_FALSE;

  // check here to see if there is a range selection
  // so we know whether to turn on the "Selection" radio button
  nsCOMPtr<nsISelection> selection;
  selection = presShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  if (selection) {
    PRInt32 count;
    selection->GetRangeCount(&count);
    if (count == 1) {
      nsCOMPtr<nsIDOMRange> range;
      if (NS_SUCCEEDED(selection->GetRangeAt(0, getter_AddRefs(range)))) {
        // check to make sure it isn't an insertion selection
        PRBool isCollapsed;
        selection->GetIsCollapsed(&isCollapsed);
        return !isCollapsed;
      }
    }
    if (count > 1) return PR_TRUE;
  }
  return PR_FALSE;
}

//---------------------------------------------------------------------
PRBool
nsPrintEngine::IsParentAFrameSet(nsIDocShell * aParent)
{
  NS_ASSERTION(aParent, "Pointer is null!");

  nsCOMPtr<nsIPresShell> shell;
  aParent->GetPresShell(getter_AddRefs(shell));
  NS_ASSERTION(shell, "shell can't be null");

  // See if the incoming doc is the root document
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(aParent));
  if (!parentAsItem) return PR_FALSE;

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
  PRBool isFrameSet = PR_FALSE;
  // only check to see if there is a frameset if there is
  // NO parent doc for this doc. meaning this parent is the root doc
  if (shell) {
    nsIDocument *doc = shell->GetDocument();
    if (doc) {
      nsIContent *rootContent = doc->GetRootContent();
      if (rootContent) {
        isFrameSet = HasFramesetChild(rootContent);
      }
    }
  }
  return isFrameSet;
}


//---------------------------------------------------------------------
// Recursively build a list of sub documents to be printed
// that mirrors the document tree
void
nsPrintEngine::BuildDocTree(nsIDocShellTreeNode * aParentNode,
                                 nsVoidArray *         aDocList,
                                 nsPrintObject *         aPO)
{
  NS_ASSERTION(aParentNode, "Pointer is null!");
  NS_ASSERTION(aDocList, "Pointer is null!");
  NS_ASSERTION(aPO, "Pointer is null!");

  PRInt32 childWebshellCount;
  aParentNode->GetChildCount(&childWebshellCount);
  if (childWebshellCount > 0) {
    for (PRInt32 i=0;i<childWebshellCount;i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));

      nsCOMPtr<nsIContentViewer>  viewer;
      childAsShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIContentViewerFile> viewerFile(do_QueryInterface(viewer));
        if (viewerFile) {
          nsCOMPtr<nsIDocShell> childDocShell(do_QueryInterface(child));
          nsCOMPtr<nsIDocShellTreeNode> childNode(do_QueryInterface(child));
          nsPrintObject * po = new nsPrintObject();
          nsresult rv = po->Init(childDocShell);
          if (NS_FAILED(rv))
            NS_NOTREACHED("Init failed?");
          po->mParent = aPO;
          aPO->mKids.AppendElement(po);
          aDocList->AppendElement(po);
          BuildDocTree(childNode, aDocList, po);
        }
      }
    }
  }
}

//---------------------------------------------------------------------
void
nsPrintEngine::GetDocumentTitleAndURL(nsIDocument* aDoc,
                                      PRUnichar**  aTitle,
                                      PRUnichar**  aURLStr)
{
  NS_ASSERTION(aDoc,      "Pointer is null!");
  NS_ASSERTION(aTitle,    "Pointer is null!");
  NS_ASSERTION(aURLStr,   "Pointer is null!");

  *aTitle  = nsnull;
  *aURLStr = nsnull;

  const nsAString &docTitle = aDoc->GetDocumentTitle();
  if (!docTitle.IsEmpty()) {
    *aTitle = ToNewUnicode(docTitle);
  }

  nsIURI* url = aDoc->GetDocumentURI();
  if (!url) return;

  nsCOMPtr<nsIURIFixup> urifixup(do_GetService(NS_URIFIXUP_CONTRACTID));
  if (!urifixup) return;

  nsCOMPtr<nsIURI> exposableURI;
  urifixup->CreateExposableURI(url, getter_AddRefs(exposableURI));

  if (!exposableURI) return;

  nsCAutoString urlCStr;
  exposableURI->GetSpec(urlCStr);
  *aURLStr = UTF8ToNewUnicode(urlCStr);
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
  MapContentForPO(aPO, aPO->mDocument->GetRootContent());

  // Continue recursively walking the chilren of this PO
  for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
    MapContentToWebShells(aRootPO, (nsPrintObject*)aPO->mKids[i]);
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
  PRBool hasChildFrames = PR_FALSE;
  for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
    nsPrintObject* po = (nsPrintObject*)aPO->mKids[i];
    if (po->mFrameType == eFrame) {
      hasChildFrames = PR_TRUE;
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

  nsIDocument* doc = aContent->GetDocument();

  NS_ASSERTION(doc, "Content without a document from a document tree?");

  nsIDocument* subDoc = doc->GetSubDocumentFor(aContent);

  if (subDoc) {
    nsCOMPtr<nsISupports> container = subDoc->GetContainer();
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));

    if (docShell) {
      nsPrintObject * po = nsnull;
      PRInt32 cnt = aPO->mKids.Count();
      for (PRInt32 i=0;i<cnt;i++) {
        nsPrintObject* kid = (nsPrintObject*)aPO->mKids.ElementAt(i);
        if (kid->mDocument == subDoc) {
          po = kid;
          break;
        }
      }

      // XXX If a subdocument has no onscreen presentation, there will be no PO
      //     This is even if there should be a print presentation
      if (po) {
        po->mContent  = aContent;

        nsCOMPtr<nsIDOMHTMLFrameElement> frame(do_QueryInterface(aContent));
        if (frame) {
          po->mFrameType = eFrame;
        } else {
          // Assume something iframe-like, i.e. iframe, object, or embed
          po->mFrameType = eIFrame;
          SetPrintAsIs(po, PR_TRUE);
          NS_ASSERTION(po->mParent, "The root must be a parent");
          po->mParent->mPrintAsIs = PR_TRUE;
        }
      }
    }
  }

  // walk children content
  PRUint32 count = aContent->GetChildCount();
  for (PRUint32 i = 0; i < count; ++i) {
    nsIContent *child = aContent->GetChildAt(i);
    MapContentForPO(aPO, child);
  }
}

//---------------------------------------------------------------------
PRBool
nsPrintEngine::IsThereAnIFrameSelected(nsIDocShell* aDocShell,
                                       nsIDOMWindow* aDOMWin,
                                       PRPackedBool& aIsParentFrameSet)
{
  aIsParentFrameSet = IsParentAFrameSet(aDocShell);
  PRBool iFrameIsSelected = PR_FALSE;
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
        nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(aDocShell);
        if (domWin != aDOMWin) {
          iFrameIsSelected = PR_TRUE; // we have a selected IFRAME
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
nsPrintEngine::SetPrintPO(nsPrintObject* aPO, PRBool aPrint)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Set whether to print flag
  aPO->mDontPrint = !aPrint;

  for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
    SetPrintPO((nsPrintObject*)aPO->mKids[i], aPrint);
  } 
}

//---------------------------------------------------------------------
// This will first use a Title and/or URL from the PrintSettings
// if one isn't set then it uses the one from the document
// then if not title is there we will make sure we send something back
// depending on the situation.
void
nsPrintEngine::GetDisplayTitleAndURL(nsPrintObject*    aPO,
                                     PRUnichar**       aTitle, 
                                     PRUnichar**       aURLStr,
                                     eDocTitleDefault  aDefType)
{
  NS_ASSERTION(aPO, "Pointer is null!");
  NS_ASSERTION(aTitle, "Pointer is null!");
  NS_ASSERTION(aURLStr, "Pointer is null!");

  *aTitle  = nsnull;
  *aURLStr = nsnull;

  if (!mPrt)
    return;

  // First check to see if the PrintSettings has defined an alternate title
  // and use that if it did
  PRUnichar * docTitleStrPS = nsnull;
  PRUnichar * docURLStrPS   = nsnull;
  if (mPrt->mPrintSettings) {
    mPrt->mPrintSettings->GetTitle(&docTitleStrPS);
    mPrt->mPrintSettings->GetDocURL(&docURLStrPS);

    if (docTitleStrPS && *docTitleStrPS) {
      *aTitle  = docTitleStrPS;
    }

    if (docURLStrPS && *docURLStrPS) {
      *aURLStr  = docURLStrPS;
    }

    // short circut
    if (docTitleStrPS && docURLStrPS) {
      return;
    }
  }

  PRUnichar* docTitle;
  PRUnichar* docUrl;
  GetDocumentTitleAndURL(aPO->mDocument, &docTitle, &docUrl);

  if (docUrl) {
    if (!docURLStrPS)
      *aURLStr = docUrl;
    else
      nsMemory::Free(docUrl);
  }

  if (docTitle) {
    if (!docTitleStrPS)
      *aTitle = docTitle;
    else
      nsMemory::Free(docTitle);
  } else if (!docTitleStrPS) {
    switch (aDefType) {
      case eDocTitleDefBlank: *aTitle = ToNewUnicode(EmptyString());
        break;

      case eDocTitleDefURLDoc:
        if (*aURLStr) {
          *aTitle = NS_strdup(*aURLStr);
        } else if (mPrt->mBrandName) {
          *aTitle = NS_strdup(mPrt->mBrandName);
        }
        break;
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
    DonePrintingPages(nsnull, rv);
  }
  return rv;
}

/** ---------------------------------------------------
 *  Cleans up when an error occurred
 */
nsresult nsPrintEngine::CleanupOnFailure(nsresult aResult, PRBool aIsPrinting)
{
  PR_PL(("****  Failed %s - rv 0x%X", aIsPrinting?"Printing":"Print Preview", aResult));

  /* cleanup... */
  if (mPagePrintTimer) {
    mPagePrintTimer->Stop();
    NS_RELEASE(mPagePrintTimer);
  }
  
  if (aIsPrinting) {
    SetIsPrinting(PR_FALSE);
  } else {
    SetIsPrintPreview(PR_FALSE);
    SetIsCreatingPrintPreview(PR_FALSE);
  }

  /* cleanup done, let's fire-up an error dialog to notify the user
   * what went wrong... 
   * 
   * When rv == NS_ERROR_ABORT, it means we want out of the 
   * print job without displaying any error messages
   */
  if (aResult != NS_ERROR_ABORT) {
    ShowPrintErrorDialog(aResult, aIsPrinting);
  }

  FirePrintCompletionEvent();

  return aResult;

}

//---------------------------------------------------------------------
void
nsPrintEngine::ShowPrintErrorDialog(nsresult aPrintError, PRBool aIsPrinting)
{

  PR_PL(("nsPrintEngine::ShowPrintErrorDialog(nsresult aPrintError=%lx, PRBool aIsPrinting=%d)\n", (long)aPrintError, (int)aIsPrinting));

  nsCAutoString stringName;

  switch(aPrintError)
  {
#define NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(nserr) case nserr: stringName.AssignLiteral(#nserr); break;
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_CMD_NOT_FOUND)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_CMD_FAILURE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_ACCESS_DENIED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_INVALID_ATTRIBUTE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_PRINTER_NOT_READY)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_OUT_OF_PAPER)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_PRINTER_IO_ERROR)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_FILE_IO_ERROR)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_PRINTPREVIEW)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_UNEXPECTED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_OUT_OF_MEMORY)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_NOT_IMPLEMENTED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_NOT_AVAILABLE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_ABORT)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_STARTDOC)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_ENDDOC)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_STARTPAGE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_ENDPAGE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_PAPER_SIZE_NOT_SUPPORTED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_ORIENTATION_NOT_SUPPORTED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_COLORSPACE_NOT_SUPPORTED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_TOO_MANY_COPIES)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_DRIVER_CONFIGURATION_ERROR)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_XPRINT_BROKEN_XPRT)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_DOC_IS_BUSY_PP)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_DOC_WAS_DESTORYED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_NO_PRINTDIALOG_IN_TOOLKIT)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_NO_PRINTROMPTSERVICE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_NO_XUL)   // Temporary code for Bug 136185 / bug 240490
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_XPRINT_NO_XPRINT_SERVERS_FOUND)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_PLEX_NOT_SUPPORTED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_DOC_IS_BUSY)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_COULD_NOT_LOAD_PRINT_MODULE)
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_RESOLUTION_NOT_SUPPORTED)

    default:
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_FAILURE)
#undef NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG
  }

  PR_PL(("ShowPrintErrorDialog:  stringName='%s'\n", stringName.get()));

  nsXPIDLString msg, title;
  nsresult rv =
    nsContentUtils::GetLocalizedString(nsContentUtils::ePRINTING_PROPERTIES,
                                       stringName.get(), msg);
  if (NS_FAILED(rv)) {
    PR_PL(("GetLocalizedString failed\n"));
    return;
  }

  rv = nsContentUtils::GetLocalizedString(nsContentUtils::ePRINTING_PROPERTIES,
      aIsPrinting ? "print_error_dialog_title"
                  : "printpreview_error_dialog_title",
      title);

  nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    PR_PL(("ShowPrintErrorDialog(): wwatch==nsnull\n"));
    return;
  }

  nsCOMPtr<nsIDOMWindow> active;
  wwatch->GetActiveWindow(getter_AddRefs(active));

  nsCOMPtr<nsIPrompt> dialog;
  /* |GetNewPrompter| allows that |active| is |nsnull| 
   * (see bug 234982 ("nsPrintEngine::ShowPrintErrorDialog() fails in many cases")) */
  wwatch->GetNewPrompter(active, getter_AddRefs(dialog));
  if (!dialog) {
    PR_PL(("ShowPrintErrorDialog(): dialog==nsnull\n"));
    return;
  }

  dialog->Alert(title.get(), msg.get());
  PR_PL(("ShowPrintErrorDialog(): alert displayed successfully.\n"));
}

//-----------------------------------------------------------------
//-- Section: Reflow Methods
//-----------------------------------------------------------------

//-------------------------------------------------------
nsresult
nsPrintEngine::SetupToPrintContent()
{
  // In this step we figure out which documents should be printed
  // i.e. if we are printing the selection then only enable that nsPrintObject
  // for printing
  if (NS_FAILED(EnablePOsForPrinting())) {
    return NS_ERROR_FAILURE;
  }
  DUMP_DOC_LIST("\nAfter Enable------------------------------------------");

  // This is an Optimization
  // If we are in PP then we already know all the shrinkage information
  // so just transfer it to the PrintData and we will skip the extra shrinkage reflow
  //
  // doSetPixelScale tells Reflow whether to set the shrinkage value into the DC
  // The first time we do not want to do this, the second time through we do
  PRBool doSetPixelScale = PR_FALSE;
  PRBool ppIsShrinkToFit = mPrtPreview && mPrtPreview->mShrinkToFit;
  if (ppIsShrinkToFit) {
    mPrt->mShrinkRatio = mPrtPreview->mShrinkRatio;
    doSetPixelScale = PR_TRUE;
  }

  // Here we reflow all the PrintObjects
  nsresult rv = ReflowDocList(mPrt->mPrintObject, doSetPixelScale);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // Here is where we do the extra reflow for shrinking the content
  // But skip this step if we are in PrintPreview
  if (mPrt->mShrinkToFit && !ppIsShrinkToFit) {
    // Now look for the PO that has the smallest percent for shrink to fit
    if (mPrt->mPrintDocList->Count() > 1 && mPrt->mPrintObject->mFrameType == eFrameSet) {
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

    // Only Shrink if we are smaller
    if (mPrt->mShrinkRatio < 0.998f) {
      // Clamp Shrink to Fit to 60%
      mPrt->mShrinkRatio = PR_MAX(mPrt->mShrinkRatio, 0.60f);

      for (PRInt32 i=0;i<mPrt->mPrintDocList->Count();i++) {
        nsPrintObject* po = (nsPrintObject*)mPrt->mPrintDocList->ElementAt(i);
        NS_ASSERTION(po, "nsPrintObject can't be null!");
        // Wipe out the presentation before we reflow
        po->DestroyPresentation();
      }

#if (defined(XP_WIN) || defined(XP_OS2)) && defined(EXTENDED_DEBUG_PRINTING)
      // We need to clear all the output files here
      // because they will be re-created with second reflow of the docs
      if (kPrintingLogMod && kPrintingLogMod->level == DUMP_LAYOUT_LEVEL) {
        RemoveFilesInDir(".\\");
        gDumpFileNameCnt   = 0;
        gDumpLOFileNameCnt = 0;
      }
#endif

      // Here we reflow all the PrintObjects a second time
      // this time using the shrinkage values
      // The last param here tells reflow to NOT calc the shrinkage values
      if (NS_FAILED(ReflowDocList(mPrt->mPrintObject, PR_TRUE))) {
        return NS_ERROR_FAILURE;
      }
    }

#ifdef PR_LOGGING
    {
      float calcRatio = 0.0f;
      if (mPrt->mPrintDocList->Count() > 1 && mPrt->mPrintObject->mFrameType == eFrameSet) {
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
#endif
  }

  DUMP_DOC_LIST(("\nAfter Reflow------------------------------------------"));
  PR_PL(("\n"));
  PR_PL(("-------------------------------------------------------\n"));
  PR_PL(("\n"));

  CalcNumPrintablePages(mPrt->mNumPrintablePages);

  PR_PL(("--- Printing %d pages\n", mPrt->mNumPrintablePages));
  DUMP_DOC_TREELAYOUT;

  // Print listener setup...
  if (mPrt != nsnull) {
    mPrt->OnStartPrinting();    
  }

  PRUnichar* fileName = nsnull;
  // check to see if we are printing to a file
  PRBool isPrintToFile = PR_FALSE;
  mPrt->mPrintSettings->GetPrintToFile(&isPrintToFile);
  if (isPrintToFile) {
  // On some platforms The BeginDocument needs to know the name of the file
  // and it uses the PrintService to get it, so we need to set it into the PrintService here
    mPrt->mPrintSettings->GetToFileName(&fileName);
  }

  PRUnichar * docTitleStr;
  PRUnichar * docURLStr;
  GetDisplayTitleAndURL(mPrt->mPrintObject, &docTitleStr, &docURLStr, eDocTitleDefURLDoc); 

  PRInt32 startPage = 1;
  PRInt32 endPage   = mPrt->mNumPrintablePages;

  PRInt16 printRangeType = nsIPrintSettings::kRangeAllPages;
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
    rv = mPrt->mPrintDC->BeginDocument(docTitleStr, fileName, startPage, endPage);
  }

  PR_PL(("****************** Begin Document ************************\n"));

  if (docTitleStr) nsMemory::Free(docTitleStr);
  if (docURLStr) nsMemory::Free(docURLStr);

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
nsPrintEngine::ReflowDocList(nsPrintObject* aPO, PRBool aSetPixelScale)
{
  NS_ENSURE_ARG_POINTER(aPO);

  // Check to see if the subdocument's element has been hidden by the parent document
  if (aPO->mParent && aPO->mParent->mPresShell) {
    nsIFrame * frame =
      aPO->mParent->mPresShell->GetPrimaryFrameFor(aPO->mContent);
    if (frame) {
      if (!frame->GetStyleVisibility()->IsVisible()) {
        aPO->mDontPrint = PR_TRUE;
        aPO->mInvisible = PR_TRUE;
        return NS_OK;
      }
    }
  }

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

  nsresult rv;
  // Reflow the PO
  rv = ReflowPrintObject(aPO);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i=0;i<cnt;i++) {
    rv = ReflowDocList((nsPrintObject *)aPO->mKids[i], aSetPixelScale);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

//-------------------------------------------------------
// Reflow a nsPrintObject
nsresult
nsPrintEngine::ReflowPrintObject(nsPrintObject * aPO)
{
  NS_ASSERTION(aPO, "Pointer is null!");
  if (!aPO) return NS_ERROR_FAILURE;

  nsSize adjSize;
  PRBool documentIsTopLevel;
  nsIFrame* frame = nsnull;
  if (!aPO->IsPrintable())
    return NS_OK;

  if (aPO->mParent && aPO->mParent->IsPrintable()) {
    if (aPO->mParent->mPresShell) {
      frame = aPO->mParent->mPresShell->FrameManager()->
        GetPrimaryFrameFor(aPO->mContent, -1);
    }
    // Without a frame, this document can't be displayed; therefore, there is no
    // point to reflowing it
    if (!frame)
      return NS_OK;

    adjSize = frame->GetContentRect().Size();
    documentIsTopLevel = PR_FALSE;
    // presshell exists because parent is printable
  } else {
    PRInt32 pageWidth, pageHeight;
    mPrt->mPrintDC->GetDeviceSurfaceDimensions(pageWidth, pageHeight);
    adjSize = nsSize(pageWidth, pageHeight);
    documentIsTopLevel = PR_TRUE;
  }

  // create the PresContext
  aPO->mPresContext = new nsPresContext(aPO->mDocument,
                                        mIsCreatingPrintPreview ?
                                         nsPresContext::eContext_PrintPreview:
                                         nsPresContext::eContext_Print);
  NS_ENSURE_TRUE(aPO->mPresContext, NS_ERROR_OUT_OF_MEMORY);
  aPO->mPresContext->SetPrintSettings(mPrt->mPrintSettings);

  // set the presentation context to the value in the print settings
  PRBool printBGColors;
  mPrt->mPrintSettings->GetPrintBGColors(&printBGColors);
  aPO->mPresContext->SetBackgroundColorDraw(printBGColors);
  mPrt->mPrintSettings->GetPrintBGImages(&printBGColors);
  aPO->mPresContext->SetBackgroundImageDraw(printBGColors);

  // init it with the DC
  nsresult rv = aPO->mPresContext->Init(mPrt->mPrintDC);
  NS_ENSURE_SUCCESS(rv, rv);

  aPO->mViewManager = do_CreateInstance(kViewManagerCID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = aPO->mViewManager->Init(mPrt->mPrintDC);
  NS_ENSURE_SUCCESS(rv,rv);

  nsStyleSet* styleSet;
  rv = mDocViewerPrint->CreateStyleSet(aPO->mDocument, &styleSet);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aPO->mDocument->CreateShell(aPO->mPresContext, aPO->mViewManager,
                                   styleSet, getter_AddRefs(aPO->mPresShell));
  if (NS_FAILED(rv)) {
    delete styleSet;
    return rv;
  }

  styleSet->EndUpdate();
  
  // The pres shell now owns the style set object.

  PR_PL(("In DV::ReflowPrintObject PO: %p (%9s) Setting w,h to %d,%d\n", aPO,
         gFrameTypesStr[aPO->mFrameType], adjSize.width, adjSize.height));

  // XXX - Hack Alert
  // OK, so there is a selection, we will print the entire selection
  // on one page and then crop the page.
  // This means you can never print any selection that is longer than
  // one page put it keeps it from page breaking in the middle of your
  // print of the selection (see also nsSimplePageSequence.cpp)
  PRInt16 printRangeType = nsIPrintSettings::kRangeAllPages;
  mPrt->mPrintSettings->GetPrintRange(&printRangeType);

  if (printRangeType == nsIPrintSettings::kRangeSelection &&
      IsThereARangeSelection(mPrt->mCurrentFocusWin)) {
    adjSize.height = NS_UNCONSTRAINEDSIZE;
  }

  // Here we decide whether we need scrollbars and
  // what the parent will be of the widget
  // How this logic presently works: Print Preview is always as-is (as far
  // as I can tell; not sure how it would work in other cases); only the root 
  // is not eIFrame or eFrame.  The child documents get a parent widget from
  // logic in nsFrameFrame.  In any case, a child widget is created for the root
  // view of the document.
  PRBool canCreateScrollbars = PR_FALSE;
  nsIView* parentView;
  // the top nsPrintObject's widget will always have scrollbars
  if (frame) {
    nsIView* view = frame->GetView();
    NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
    view = view->GetFirstChild();
    NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
    parentView = view;
  } else {
    canCreateScrollbars = PR_TRUE;
    parentView = nsnull;
  }

  // Create a child window of the parent that is our "root view/window"
  nsRect tbounds = nsRect(nsPoint(0, 0), adjSize);
  nsIView* rootView = aPO->mViewManager->CreateView(tbounds, parentView);
  NS_ENSURE_TRUE(rootView, NS_ERROR_OUT_OF_MEMORY);

  // Only create a widget for print preview; when printing, a widget is
  // unnecessary and unexpected
  // Also, no widget should be needed except for the top-level document
  if (mIsCreatingPrintPreview && documentIsTopLevel) {
    nsNativeWidget widget = nsnull;
    if (!frame)
      widget = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
    rv = rootView->CreateWidget(kWidgetCID, nsnull,
                                widget, PR_TRUE, PR_TRUE,
                                eContentTypeContent);
    NS_ENSURE_SUCCESS(rv, rv);
    aPO->mWindow = rootView->GetWidget();
    aPO->mPresContext->SetPaginatedScrolling(canCreateScrollbars);
  }

  // Setup hierarchical relationship in view manager
  aPO->mViewManager->SetRootView(rootView);

  // This docshell stuff is weird; will go away when we stop having multiple
  // presentations per document
  nsCOMPtr<nsISupports> supps(do_QueryInterface(aPO->mDocShell));
  aPO->mPresContext->SetContainer(supps);

  aPO->mPresShell->BeginObservingDocument();

  aPO->mPresContext->SetPageSize(adjSize);
  aPO->mPresContext->SetIsRootPaginatedDocument(documentIsTopLevel);
  aPO->mPresContext->SetPageScale(aPO->mZoomRatio);
  // Calculate scale factor from printer to screen
  PRInt32 printDPI = mPrt->mPrintDC->AppUnitsPerInch() /
                     mPrt->mPrintDC->AppUnitsPerDevPixel();
  PRInt32 screenDPI = mDeviceContext->AppUnitsPerInch() /
                      mDeviceContext->AppUnitsPerDevPixel();
  aPO->mPresContext->SetPrintPreviewScale(float(screenDPI) / float(printDPI));

  rv = aPO->mPresShell->InitialReflow(adjSize.width, adjSize.height);

  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPresShell> displayShell;
  aPO->mDocShell->GetPresShell(getter_AddRefs(displayShell));
  // Transfer Selection Ranges to the new Print PresShell
  nsCOMPtr<nsISelection> selection, selectionPS;
  // It's okay if there is no display shell, just skip copying the selection
  if (displayShell) {
    selection = displayShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  }
  selectionPS = aPO->mPresShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  if (selection && selectionPS) {
    PRInt32 cnt;
    selection->GetRangeCount(&cnt);
    PRInt32 inx;
    for (inx=0;inx<cnt;inx++) {
      nsCOMPtr<nsIDOMRange> range;
      if (NS_SUCCEEDED(selection->GetRangeAt(inx, getter_AddRefs(range))))
        selectionPS->AddRange(range);
    }
  }

  // If we are trying to shrink the contents to fit on the page
  // we must first locate the "pageContent" frame
  // Then we walk the frame tree and look for the "xmost" frame
  // this is the frame where the right-hand side of the frame extends
  // the furthest
  if (mPrt->mShrinkToFit && documentIsTopLevel) {
    nsIPageSequenceFrame* pageSequence;
    aPO->mPresShell->GetPageSequenceFrame(&pageSequence);
    pageSequence->GetSTFPercent(aPO->mShrinkRatio);
  }

#ifdef EXTENDED_DEBUG_PRINTING
    if (kPrintingLogMod && kPrintingLogMod->level == DUMP_LAYOUT_LEVEL) {
      char * docStr;
      char * urlStr;
      GetDocTitleAndURL(aPO, docStr, urlStr);
      char filename[256];
      sprintf(filename, "print_dump_%d.txt", gDumpFileNameCnt++);
      // Dump all the frames and view to a a file
      FILE * fd = fopen(filename, "w");
      if (fd) {
        nsIFrame *theRootFrame =
          aPO->mPresShell->FrameManager()->GetRootFrame();
        fprintf(fd, "Title: %s\n", docStr?docStr:"");
        fprintf(fd, "URL:   %s\n", urlStr?urlStr:"");
        fprintf(fd, "--------------- Frames ----------------\n");
        nsCOMPtr<nsIRenderingContext> renderingContext;
        mPrt->mPrintDocDC->CreateRenderingContext(*getter_AddRefs(renderingContext));
        RootFrameList(aPO->mPresContext, fd, 0);
        //DumpFrames(fd, aPO->mPresContext, renderingContext, theRootFrame, 0);
        fprintf(fd, "---------------------------------------\n\n");
        fprintf(fd, "--------------- Views From Root Frame----------------\n");
        nsIView* v = theRootFrame->GetView();
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
      if (docStr) nsMemory::Free(docStr);
      if (urlStr) nsMemory::Free(urlStr);
    }
#endif

  return NS_OK;
}

//-------------------------------------------------------
// Figure out how many documents and how many total pages we are printing
void
nsPrintEngine::CalcNumPrintablePages(PRInt32& aNumPages)
{
  aNumPages = 0;
  // Count the number of printable documents
  // and printable pages
  PRInt32 i;
  for (i=0; i<mPrt->mPrintDocList->Count(); i++) {
    nsPrintObject* po = (nsPrintObject*)mPrt->mPrintDocList->ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    if (po->mPresContext && po->mPresContext->IsRootPaginatedDocument()) {
      nsIPageSequenceFrame* pageSequence;
      po->mPresShell->GetPageSequenceFrame(&pageSequence);
      nsIFrame * seqFrame;
      if (NS_SUCCEEDED(CallQueryInterface(pageSequence, &seqFrame))) {
        nsIFrame* frame = seqFrame->GetFirstChild(nsnull);
        while (frame) {
          aNumPages++;
          frame = frame->GetNextSibling();
        }
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
PRBool
nsPrintEngine::PrintDocContent(nsPrintObject* aPO, nsresult& aStatus)
{
  NS_ASSERTION(aPO, "Pointer is null!");
  aStatus = NS_OK;

  if (!aPO->mHasBeenPrinted && aPO->IsPrintable()) {
    aStatus = DoPrint(aPO);
    return PR_TRUE;
  }

  // If |aPO->mPrintAsIs| and |aPO->mHasBeenPrinted| are true,
  // the kids frames are already processed in |PrintPage|.
  if (!aPO->mInvisible && !(aPO->mPrintAsIs && aPO->mHasBeenPrinted)) {
    for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
      nsPrintObject* po = (nsPrintObject*)aPO->mKids[i];
      PRBool printed = PrintDocContent(po, aStatus);
      if (printed || NS_FAILED(aStatus)) {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
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
    PRInt16 printRangeType = nsIPrintSettings::kRangeAllPages;
    nsresult rv;
    if (mPrt->mPrintSettings != nsnull) {
      mPrt->mPrintSettings->GetPrintRange(&printRangeType);
    }

    // Ask the page sequence frame to print all the pages
    nsIPageSequenceFrame* pageSequence;
    poPresShell->GetPageSequenceFrame(&pageSequence);
    NS_ASSERTION(nsnull != pageSequence, "no page sequence frame");

    // We are done preparing for printing, so we can turn this off
    mPrt->mPreparingForPrint = PR_FALSE;

    // mPrt->mDebugFilePtr this is onlu non-null when compiled for debugging
    if (nsnull != mPrt->mDebugFilePtr) {
#ifdef NS_DEBUG
      // output the regression test
      nsIFrameDebug* fdbg;
      nsIFrame* root = poPresShell->FrameManager()->GetRootFrame();

      if (NS_SUCCEEDED(CallQueryInterface(root, &fdbg))) {
        fdbg->DumpRegressionData(poPresContext, mPrt->mDebugFilePtr, 0, PR_TRUE);
      }
      fclose(mPrt->mDebugFilePtr);
      SetIsPrinting(PR_FALSE);
#endif
    } else {
#ifdef EXTENDED_DEBUG_PRINTING
      nsIFrame* rootFrame = poPresShell->FrameManager()->GetRootFrame();
      if (aPO->IsPrintable()) {
        char * docStr;
        char * urlStr;
        GetDocTitleAndURL(aPO, docStr, urlStr);
        DumpLayoutData(docStr, urlStr, poPresContext, mPrt->mPrintDocDC, rootFrame, docShell, nsnull);
        if (docStr) nsMemory::Free(docStr);
        if (urlStr) nsMemory::Free(urlStr);
      }
#endif

      if (mPrt->mPrintSettings) {
        PRUnichar * docTitleStr = nsnull;
        PRUnichar * docURLStr   = nsnull;

        GetDisplayTitleAndURL(aPO, &docTitleStr, &docURLStr, eDocTitleDefBlank); 

        if (nsIPrintSettings::kRangeSelection == printRangeType) {
          poPresContext->SetIsRenderingOnlySelection(PR_TRUE);
          // temporarily creating rendering context
          // which is needed to dinf the selection frames
          nsCOMPtr<nsIRenderingContext> rc;
          mPrt->mPrintDC->CreateRenderingContext(*getter_AddRefs(rc));

          // find the starting and ending page numbers
          // via the selection
          nsIFrame* startFrame;
          nsIFrame* endFrame;
          PRInt32   startPageNum;
          PRInt32   endPageNum;
          nsRect    startRect;
          nsRect    endRect;

          nsCOMPtr<nsISelection> selectionPS;
          selectionPS = poPresShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);

          rv = GetPageRangeForSelection(poPresShell, poPresContext, *rc, selectionPS, pageSequence,
                                        &startFrame, startPageNum, startRect,
                                        &endFrame, endPageNum, endRect);
          if (NS_SUCCEEDED(rv)) {
            mPrt->mPrintSettings->SetStartPageRange(startPageNum);
            mPrt->mPrintSettings->SetEndPageRange(endPageNum);
            nsMargin margin(0,0,0,0);
            mPrt->mPrintSettings->GetMarginInTwips(margin);

            if (startPageNum == endPageNum) {
              {
                nsPresContext* presContext = poPresShell->GetPresContext();
                startRect.y -= presContext->TwipsToAppUnits(margin.top);
                endRect.y   -= presContext->TwipsToAppUnits(margin.top);
                // XXX This is temporary fix for printing more than one page of a selection
                pageSequence->SetSelectionHeight(startRect.y, endRect.y+endRect.height-startRect.y);

                // calc total pages by getting calculating the selection's height
                // and then dividing it by how page content frames will fit.
                nscoord selectionHgt = endRect.y + endRect.height - startRect.y;
                PRInt32 pageWidth, pageHeight;
                mPrt->mPrintDC->GetDeviceSurfaceDimensions(pageWidth, pageHeight);
                pageHeight -= margin.top + margin.bottom;
                PRInt32 totalPages = PRInt32((float(selectionHgt) / float(pageHeight))+0.99);
                pageSequence->SetTotalNumPages(totalPages);
              }
            }
          }
        }

        nsIFrame * seqFrame;
        if (NS_FAILED(CallQueryInterface(pageSequence, &seqFrame))) {
          SetIsPrinting(PR_FALSE);
          return NS_ERROR_FAILURE;
        }

        mPageSeqFrame = pageSequence;
        mPageSeqFrame->StartPrint(poPresContext, mPrt->mPrintSettings, docTitleStr, docURLStr);

        // Schedule Page to Print
        PR_PL(("Scheduling Print of PO: %p (%s) \n", aPO, gFrameTypesStr[aPO->mFrameType]));
        StartPagePrintTimer(aPO);
      } else {
        // not sure what to do here!
        SetIsPrinting(PR_FALSE);
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

//---------------------------------------------------------------------
void
nsPrintEngine::SetDocAndURLIntoProgress(nsPrintObject* aPO,
                                        nsIPrintProgressParams* aParams)
{
  NS_ASSERTION(aPO, "Must have vaild nsPrintObject");
  NS_ASSERTION(aParams, "Must have vaild nsIPrintProgressParams");

  if (!aPO || !aPO->mDocShell || !aParams) {
    return;
  }
  const PRUint32 kTitleLength = 64;

  PRUnichar * docTitleStr;
  PRUnichar * docURLStr;
  GetDisplayTitleAndURL(aPO, &docTitleStr, &docURLStr, eDocTitleDefURLDoc);

  // Make sure the Titles & URLS don't get too long for the progress dialog
  ElipseLongString(docTitleStr, kTitleLength, PR_FALSE);
  ElipseLongString(docURLStr, kTitleLength, PR_TRUE);

  aParams->SetDocTitle(docTitleStr);
  aParams->SetDocURL(docURLStr);

  if (docTitleStr != nsnull) nsMemory::Free(docTitleStr);
  if (docURLStr != nsnull) nsMemory::Free(docURLStr);
}

//---------------------------------------------------------------------
void
nsPrintEngine::ElipseLongString(PRUnichar *& aStr, const PRUint32 aLen, PRBool aDoFront)
{
  // Make sure the URLS don't get too long for the progress dialog
  if (aStr && nsCRT::strlen(aStr) > aLen) {
    if (aDoFront) {
      PRUnichar * ptr = &aStr[nsCRT::strlen(aStr)-aLen+3];
      nsAutoString newStr;
      newStr.AppendLiteral("...");
      newStr += ptr;
      nsMemory::Free(aStr);
      aStr = ToNewUnicode(newStr);
    } else {
      nsAutoString newStr(aStr);
      newStr.SetLength(aLen-3);
      newStr.AppendLiteral("...");
      nsMemory::Free(aStr);
      aStr = ToNewUnicode(newStr);
    }
  }
}

//-------------------------------------------------------
PRBool
nsPrintEngine::PrintPage(nsPrintObject*    aPO,
                         PRBool&           aInRange)
{
  NS_ASSERTION(aPO,            "aPO is null!");
  NS_ASSERTION(mPageSeqFrame,  "mPageSeqFrame is null!");
  NS_ASSERTION(mPrt,           "mPrt is null!");

  // Although these should NEVER be NULL
  // This is added insurance, to make sure we don't crash in optimized builds
  if (!mPrt || !aPO || !mPageSeqFrame) {
    ShowPrintErrorDialog(NS_ERROR_FAILURE);
    return PR_TRUE; // means we are done printing
  }

  PR_PL(("-----------------------------------\n"));
  PR_PL(("------ In DV::PrintPage PO: %p (%s)\n", aPO, gFrameTypesStr[aPO->mFrameType]));

  // Check setting to see if someone request it be cancelled
  PRBool isCancelled = PR_FALSE;
  mPrt->mPrintSettings->GetIsCancelled(&isCancelled);
  if (isCancelled)
    return PR_TRUE;

  PRInt32 pageNum, numPages, endPage;
  mPageSeqFrame->GetCurrentPageNum(&pageNum);
  mPageSeqFrame->GetNumPages(&numPages);

  PRBool donePrinting;
  PRBool isDoingPrintRange;
  mPageSeqFrame->IsDoingPrintRange(&isDoingPrintRange);
  if (isDoingPrintRange) {
    PRInt32 fromPage;
    PRInt32 toPage;
    mPageSeqFrame->GetPrintRange(&fromPage, &toPage);

    if (fromPage > numPages) {
      return PR_TRUE;
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
    aInRange = PR_TRUE;
  }

  // XXX This is wrong, but the actual behavior in the presence of a print
  // range sucks.
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep)
    endPage = mPrt->mNumPrintablePages;
  
  mPrt->DoOnProgressChange(++mPrt->mNumPagesPrinted, endPage, PR_FALSE, 0);

  // Print the Page
  // if a print job was cancelled externally, an EndPage or BeginPage may
  // fail and the failure is passed back here.
  // Returning PR_TRUE means we are done printing.
  //
  // When rv == NS_ERROR_ABORT, it means we want out of the
  // print job without displaying any error messages
  nsresult rv = mPageSeqFrame->PrintNextPage();
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_ABORT) {
      ShowPrintErrorDialog(rv);
      mPrt->mIsAborted = PR_TRUE;
    }
    return PR_TRUE;
  }

  mPageSeqFrame->DoPageEnd();

  return donePrinting;
}

/** ---------------------------------------------------
 *  Find by checking frames type
 */
nsresult 
nsPrintEngine::FindSelectionBoundsWithList(nsPresContext* aPresContext,
                                           nsIRenderingContext& aRC,
                                           nsIAtom*        aList,
                                           nsIFrame *      aParentFrame,
                                           nsRect&         aRect,
                                           nsIFrame *&     aStartFrame,
                                           nsRect&         aStartRect,
                                           nsIFrame *&     aEndFrame,
                                           nsRect&         aEndRect)
{
  NS_ASSERTION(aPresContext, "Pointer is null!");
  NS_ASSERTION(aParentFrame, "Pointer is null!");

  nsIFrame* child = aParentFrame->GetFirstChild(aList);
  aRect += aParentFrame->GetPosition();
  while (child) {
    // only leaf frames have this bit flipped
    // then check the hard way
    PRBool isSelected = (child->GetStateBits() & NS_FRAME_SELECTED_CONTENT)
      == NS_FRAME_SELECTED_CONTENT;
    if (isSelected) {
      isSelected = child->IsVisibleForPainting();
    }

    if (isSelected) {
      nsRect r = child->GetRect();
      if (aStartFrame == nsnull) {
        aStartFrame = child;
        aStartRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
      } else {
        aEndFrame = child;
        aEndRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
      }
    }
    FindSelectionBounds(aPresContext, aRC, child, aRect, aStartFrame, aStartRect, aEndFrame, aEndRect);
    child = child->GetNextSibling();
  }
  aRect -= aParentFrame->GetPosition();
  return NS_OK;
}

//-------------------------------------------------------
// Find the Frame that is XMost
nsresult 
nsPrintEngine::FindSelectionBounds(nsPresContext* aPresContext,
                                   nsIRenderingContext& aRC,
                                   nsIFrame *      aParentFrame,
                                   nsRect&         aRect,
                                   nsIFrame *&     aStartFrame,
                                   nsRect&         aStartRect,
                                   nsIFrame *&     aEndFrame,
                                   nsRect&         aEndRect)
{
  NS_ASSERTION(aPresContext, "Pointer is null!");
  NS_ASSERTION(aParentFrame, "Pointer is null!");

  // loop through named child lists
  nsIAtom* childListName = nsnull;
  PRInt32  childListIndex = 0;
  do {
    nsresult rv = FindSelectionBoundsWithList(aPresContext, aRC, childListName, aParentFrame, aRect, aStartFrame, aStartRect, aEndFrame, aEndRect);
    NS_ENSURE_SUCCESS(rv, rv);
    childListName = aParentFrame->GetAdditionalChildListName(childListIndex++);
  } while (childListName);
  return NS_OK;
}

/** ---------------------------------------------------
 *  This method finds the starting and ending page numbers
 *  of the selection and also returns rect for each where
 *  the x,y of the rect is relative to the very top of the
 *  frame tree (absolutely positioned)
 */
nsresult 
nsPrintEngine::GetPageRangeForSelection(nsIPresShell *        aPresShell,
                                        nsPresContext*       aPresContext,
                                        nsIRenderingContext&  aRC,
                                        nsISelection*         aSelection,
                                        nsIPageSequenceFrame* aPageSeqFrame,
                                        nsIFrame**            aStartFrame,
                                        PRInt32&              aStartPageNum,
                                        nsRect&               aStartRect,
                                        nsIFrame**            aEndFrame,
                                        PRInt32&              aEndPageNum,
                                        nsRect&               aEndRect)
{
  NS_ASSERTION(aPresShell, "Pointer is null!");
  NS_ASSERTION(aPresContext, "Pointer is null!");
  NS_ASSERTION(aSelection, "Pointer is null!");
  NS_ASSERTION(aPageSeqFrame, "Pointer is null!");
  NS_ASSERTION(aStartFrame, "Pointer is null!");
  NS_ASSERTION(aEndFrame, "Pointer is null!");

  nsIFrame * seqFrame;
  if (NS_FAILED(CallQueryInterface(aPageSeqFrame, &seqFrame))) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame * startFrame = nsnull;
  nsIFrame * endFrame   = nsnull;

  // start out with the sequence frame and search the entire frame tree
  // capturing the starting and ending child frames of the selection
  // and their rects
  nsRect r = seqFrame->GetRect();
  FindSelectionBounds(aPresContext, aRC, seqFrame, r,
                      startFrame, aStartRect, endFrame, aEndRect);

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
  if (startFrame != nsnull) {
    // Now search up the tree to find what page the
    // start/ending selections frames are on
    //
    // Check to see if start should be same as end if
    // the end frame comes back null
    if (endFrame == nsnull) {
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
  PRInt32 pageNum = 1;
  nsIFrame* child = seqFrame->GetFirstChild(nsnull);
  while (child != nsnull) {
    printf("Page: %d - %p\n", pageNum, child);
    pageNum++;
    child = child->GetNextSibling();
  }
  }
#endif

  // Now that we have the page frames
  // find out what the page numbers are for each frame
  PRInt32 pageNum = 1;
  nsIFrame* page = seqFrame->GetFirstChild(nsnull);
  while (page != nsnull) {
    if (page == startPageFrame) {
      aStartPageNum = pageNum;
    }
    if (page == endPageFrame) {
      aEndPageNum = pageNum;
    }
    pageNum++;
    page = page->GetNextSibling();
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
void nsPrintEngine::SetIsPrinting(PRBool aIsPrinting)
{ 
  mIsDoingPrinting = aIsPrinting;
  if (mDocViewerPrint) {
    mDocViewerPrint->SetIsPrinting(aIsPrinting);
  }
}

//---------------------------------------------------------------------
void nsPrintEngine::SetIsPrintPreview(PRBool aIsPrintPreview) 
{ 
  mIsDoingPrintPreview = aIsPrintPreview; 

  if (mDocViewerPrint) {
    mDocViewerPrint->SetIsPrintPreview(aIsPrintPreview);
  }
}

//---------------------------------------------------------------------
void
nsPrintEngine::CleanupDocTitleArray(PRUnichar**& aArray, PRInt32& aCount)
{
  for (PRInt32 i = aCount - 1; i >= 0; i--) {
    nsMemory::Free(aArray[i]);
  }
  nsMemory::Free(aArray);
  aArray = NULL;
  aCount = 0;
}

//---------------------------------------------------------------------
// static
PRBool nsPrintEngine::HasFramesetChild(nsIContent* aContent)
{
  if (!aContent) {
    return PR_FALSE;
  }

  PRUint32 numChildren = aContent->GetChildCount();

  // do a breadth search across all siblings
  for (PRUint32 i = 0; i < numChildren; ++i) {
    nsIContent *child = aContent->GetChildAt(i);
    if (child->Tag() == nsGkAtoms::frameset &&
        child->IsNodeOfType(nsINode::eHTML)) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}
 


/** ---------------------------------------------------
 *  Get the Focused Frame for a documentviewer
 */
already_AddRefed<nsIDOMWindow>
nsPrintEngine::FindFocusedDOMWindow()
{
  nsIDOMWindow * domWin = nsnull;

  nsPIDOMWindow *theDOMWindow = mDocument->GetWindow();
  if(theDOMWindow){
    nsIFocusController *focusController =
      theDOMWindow->GetRootFocusController();
    if (focusController) {
      nsCOMPtr<nsIDOMWindowInternal> theDOMWin;
      focusController->GetFocusedWindow(getter_AddRefs(theDOMWin));
      if(theDOMWin && IsWindowsInOurSubTree(theDOMWin)){
        NS_ADDREF(domWin = theDOMWin);
      }
    }
  }

  return domWin;
}

//---------------------------------------------------------------------
PRBool
nsPrintEngine::IsWindowsInOurSubTree(nsIDOMWindow * aDOMWindow)
{
  PRBool found = PR_FALSE;

  // now check to make sure it is in "our" tree of docshells
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aDOMWindow));
  if (window) {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
      do_QueryInterface(window->GetDocShell());

    if (docShellAsItem) {
      // get this DocViewer docshell
      nsCOMPtr<nsIDocShell> thisDVDocShell(do_QueryInterface(mContainer));
      while (!found) {
        nsCOMPtr<nsIDocShell> parentDocshell(do_QueryInterface(docShellAsItem));
        if (parentDocshell) {
          if (parentDocshell == thisDVDocShell) {
            found = PR_TRUE;
            break;
          }
        } else {
          break; // at top of tree
        }
        nsCOMPtr<nsIDocShellTreeItem> docShellParent;
        docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));
        docShellAsItem = docShellParent;
      } // while
    }
  } // scriptobj

  return found;
}

//-------------------------------------------------------
PRBool
nsPrintEngine::DonePrintingPages(nsPrintObject* aPO, nsresult aResult)
{
  //NS_ASSERTION(aPO, "Pointer is null!");
  PR_PL(("****** In DV::DonePrintingPages PO: %p (%s)\n", aPO, aPO?gFrameTypesStr[aPO->mFrameType]:""));

  if (aPO != nsnull) {
    aPO->mHasBeenPrinted = PR_TRUE;
    nsresult rv;
    PRBool didPrint = PrintDocContent(mPrt->mPrintObject, rv);
    if (NS_SUCCEEDED(rv) && didPrint) {
      PR_PL(("****** In DV::DonePrintingPages PO: %p (%s) didPrint:%s (Not Done Printing)\n", aPO, gFrameTypesStr[aPO->mFrameType], PRT_YESNO(didPrint)));
      return PR_FALSE;
    }
  }

  if (NS_SUCCEEDED(aResult)) {
    FirePrintCompletionEvent();
  }

  SetIsPrinting(PR_FALSE);

  // Release reference to mPagePrintTimer; the timer object destroys itself
  // after this returns true
  NS_IF_RELEASE(mPagePrintTimer);

  return PR_TRUE;
}

//-------------------------------------------------------
// Recursively sets the PO items to be printed "As Is"
// from the given item down into the tree
void
nsPrintEngine::SetPrintAsIs(nsPrintObject* aPO, PRBool aAsIs)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  aPO->mPrintAsIs = aAsIs;
  for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
    SetPrintAsIs((nsPrintObject*)aPO->mKids[i], aAsIs);
  }
}

//-------------------------------------------------------
// Given a DOMWindow it recursively finds the PO object that matches
nsPrintObject*
nsPrintEngine::FindPrintObjectByDOMWin(nsPrintObject* aPO,
                                       nsIDOMWindow* aDOMWin)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Often the CurFocused DOMWindow is passed in
  // andit is valid for it to be null, so short circut
  if (!aDOMWin) {
    return nsnull;
  }

  nsCOMPtr<nsIDOMWindow> domWin(do_GetInterface(aPO->mDocShell));
  if (domWin && domWin == aDOMWin) {
    return aPO;
  }

  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i = 0; i < cnt; ++i) {
    nsPrintObject* po = FindPrintObjectByDOMWin((nsPrintObject*)aPO->mKids[i],
                                                aDOMWin);
    if (po) {
      return po;
    }
  }

  return nsnull;
}

//-------------------------------------------------------
nsresult
nsPrintEngine::EnablePOsForPrinting()
{
  // NOTE: All POs have been "turned off" for printing
  // this is where we decided which POs get printed.
  mPrt->mSelectedPO = nsnull;

  if (mPrt->mPrintSettings == nsnull) {
    return NS_ERROR_FAILURE;
  }

  mPrt->mPrintFrameType = nsIPrintSettings::kNoFrames;
  mPrt->mPrintSettings->GetPrintFrameType(&mPrt->mPrintFrameType);

  PRInt16 printHowEnable = nsIPrintSettings::kFrameEnableNone;
  mPrt->mPrintSettings->GetHowToEnableFrameUI(&printHowEnable);

  PRInt16 printRangeType = nsIPrintSettings::kRangeAllPages;
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
      SetPrintPO(mPrt->mPrintObject, PR_TRUE);

      // Set the children so they are PrinAsIs
      // In this case, the children are probably IFrames
      if (mPrt->mPrintObject->mKids.Count() > 0) {
        for (PRInt32 i=0;i<mPrt->mPrintObject->mKids.Count();i++) {
          nsPrintObject* po = (nsPrintObject*)mPrt->mPrintObject->mKids[i];
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
        if (po != nsnull) {
          mPrt->mSelectedPO = po;
          // Makes sure all of its children are be printed "AsIs"
          SetPrintAsIs(po);

          // Now, only enable this POs (the selected PO) and all of its children
          SetPrintPO(po, PR_TRUE);

          // check to see if we have a range selection,
          // as oppose to a insert selection
          // this means if the user just clicked on the IFrame then
          // there will not be a selection so we want the entire page to print
          //
          // XXX this is sort of a hack right here to make the page
          // not try to reposition itself when printing selection
          nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(po->mDocShell);
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
        for (PRInt32 i=0;i<mPrt->mPrintDocList->Count();i++) {
          nsPrintObject* po = (nsPrintObject*)mPrt->mPrintDocList->ElementAt(i);
          NS_ASSERTION(po, "nsPrintObject can't be null!");
          nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(po->mDocShell);
          if (IsThereARangeSelection(domWin)) {
            mPrt->mCurrentFocusWin = domWin;
            SetPrintPO(po, PR_TRUE);
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
      if (po != nsnull) {
        mPrt->mSelectedPO = po;
        // Makes sure all of its children are be printed "AsIs"
        SetPrintAsIs(po);

        // Now, only enable this POs (the selected PO) and all of its children
        SetPrintPO(po, PR_TRUE);

        // check to see if we have a range selection,
        // as oppose to a insert selection
        // this means if the user just clicked on the IFrame then
        // there will not be a selection so we want the entire page to print
        //
        // XXX this is sort of a hack right here to make the page
        // not try to reposition itself when printing selection
        nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(po->mDocShell);
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
    SetPrintPO(mPrt->mPrintObject, PR_TRUE);
    return NS_OK;
  }

  // If we are printing the selected Frame then
  // find that PO for that selected DOMWin and set it all of its
  // children to be printed
  if (mPrt->mPrintFrameType == nsIPrintSettings::kSelectedFrame) {

    if ((mPrt->mIsParentAFrameSet && mPrt->mCurrentFocusWin) || mPrt->mIsIFrameSelected) {
      nsPrintObject * po = FindPrintObjectByDOMWin(mPrt->mPrintObject, mPrt->mCurrentFocusWin);
      if (po != nsnull) {
        mPrt->mSelectedPO = po;
        // NOTE: Calling this sets the "po" and
        // we don't want to do this for documents that have no children,
        // because then the "DoEndPage" gets called and it shouldn't
        if (po->mKids.Count() > 0) {
          // Makes sure that itself, and all of its children are printed "AsIs"
          SetPrintAsIs(po);
        }

        // Now, only enable this POs (the selected PO) and all of its children
        SetPrintPO(po, PR_TRUE);
      }
    }
    return NS_OK;
  }

  // If we are print each subdoc separately,
  // then don't print any of the FraneSet Docs
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    SetPrintPO(mPrt->mPrintObject, PR_TRUE);
    PRInt32 cnt = mPrt->mPrintDocList->Count();
    for (PRInt32 i=0;i<cnt;i++) {
      nsPrintObject* po = (nsPrintObject*)mPrt->mPrintDocList->ElementAt(i);
      NS_ASSERTION(po, "nsPrintObject can't be null!");
      if (po->mFrameType == eFrameSet) {
        po->mDontPrint = PR_TRUE;
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
  nsPrintObject* smallestPO = nsnull;

  for (PRInt32 i=0;i<mPrt->mPrintDocList->Count();i++) {
    nsPrintObject* po = (nsPrintObject*)mPrt->mPrintDocList->ElementAt(i);
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
nsPrintEngine::TurnScriptingOn(PRBool aDoTurnOn)
{
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

  for (PRInt32 i=0;i<prt->mPrintDocList->Count();i++) {
    nsPrintObject* po = (nsPrintObject*)prt->mPrintDocList->ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");

    nsIDocument* doc = po->mDocument;
    
    // get the script global object
    nsIScriptGlobalObject *scriptGlobalObj = doc->GetScriptGlobalObject();

    if (scriptGlobalObj) {
      nsIScriptContext *scx = scriptGlobalObj->GetContext();
      NS_ASSERTION(scx, "Can't get nsIScriptContext");
      if (aDoTurnOn) {
        doc->DeleteProperty(nsGkAtoms::scriptEnabledBeforePrintPreview);
      } else {
        // Have to be careful, because people call us over and over again with
        // aDoTurnOn == PR_FALSE.  So don't set the property if it's already
        // set, since in that case we'd set it to the wrong value.
        nsresult propThere;
        doc->GetProperty(nsGkAtoms::scriptEnabledBeforePrintPreview,
                         &propThere);
        if (propThere == NS_PROPTABLE_PROP_NOT_THERE) {
          // Stash the current value of IsScriptEnabled on the document, so
          // that layout code running in print preview doesn't get confused.
          doc->SetProperty(nsGkAtoms::scriptEnabledBeforePrintPreview,
                           NS_INT32_TO_PTR(doc->IsScriptEnabled()));
        }
      }
      scx->SetScriptsEnabled(aDoTurnOn, PR_TRUE);
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
    aWebProgressListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_DOCUMENT, nsnull);
  }
}

//-----------------------------------------------------------------
nsresult
nsPrintEngine::FinishPrintPreview()
{
  nsresult rv = NS_OK;

#ifdef NS_PRINT_PREVIEW

  rv = DocumentReadyForPrinting();

  SetIsCreatingPrintPreview(PR_FALSE);

  /* cleaup on failure + notify user */
  if (NS_FAILED(rv)) {
    /* cleanup done, let's fire-up an error dialog to notify the user
     * what went wrong...
     */
    mPrt->OnEndPrinting();
    TurnScriptingOn(PR_TRUE);

    return CleanupOnFailure(rv, PR_FALSE); // ignore return value here
  }

  // At this point we are done preparing everything
  // before it is to be created


  if (mIsDoingPrintPreview && mOldPrtPreview) {
    delete mOldPrtPreview;
    mOldPrtPreview = nsnull;
  }

  InstallPrintPreviewListener();

  mPrt->OnEndPrinting();

  // PrintPreview was built using the mPrt (code reuse)
  // then we assign it over
  mPrtPreview = mPrt;
  mPrt        = nsnull;

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
    nsresult rv = NS_NewPagePrintTimer(&mPagePrintTimer);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the delay time in between the printing of each page
    // this gives the user more time to press cancel
    PRInt32 printPageDelay = 500;
    mPrt->mPrintSettings->GetPrintPageDelay(&printPageDelay);

    mPagePrintTimer->Init(this, mDocViewerPrint, printPageDelay);
  }

  return mPagePrintTimer->Start(aPO);
}

/*=============== nsIObserver Interface ======================*/
NS_IMETHODIMP 
nsPrintEngine::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mIsDoingPrinting) {
    rv = DocumentReadyForPrinting();
 
    /* cleaup on failure + notify user */
    if (NS_FAILED(rv)) {
      CleanupOnFailure(rv, PR_TRUE);
    }
  } else {
    rv = FinishPrintPreview();
    if (mPrtPreview) {
      mPrtPreview->OnEndPrinting();
    }
    rv = NS_OK;
  }

  return rv;

}

//---------------------------------------------------------------
//-- PLEvent Notification
//---------------------------------------------------------------
class nsPrintCompletionEvent : public nsRunnable {
public:
  nsPrintCompletionEvent(nsIDocumentViewerPrint *docViewerPrint)
    : mDocViewerPrint(docViewerPrint) {
    NS_ASSERTION(mDocViewerPrint, "mDocViewerPrint is null.");
  }

  NS_IMETHOD Run() {
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
#if (defined(XP_WIN) || defined(XP_OS2)) && defined(EXTENDED_DEBUG_PRINTING)
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
static void RootFrameList(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  if (!aPresContext || !out)
    return;

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    nsIFrame* frame = shell->FrameManager()->GetRootFrame();
    if (frame) {
      nsIFrameDebug* debugFrame;
      nsresult rv = CallQueryInterface(frame, &debugFrame);
      if (NS_SUCCEEDED(rv))
        debugFrame->List(aPresContext, out, aIndent);
    }
  }
}

/** ---------------------------------------------------
 *  Dumps Frames for Printing
 */
static void DumpFrames(FILE*                 out,
                       nsPresContext*       aPresContext,
                       nsIRenderingContext * aRendContext,
                       nsIFrame *            aFrame,
                       PRInt32               aLevel)
{
  NS_ASSERTION(out, "Pointer is null!");
  NS_ASSERTION(aPresContext, "Pointer is null!");
  NS_ASSERTION(aRendContext, "Pointer is null!");
  NS_ASSERTION(aFrame, "Pointer is null!");

  nsIFrame* child = aFrame->GetFirstChild(nsnull);
  while (child != nsnull) {
    for (PRInt32 i=0;i<aLevel;i++) {
     fprintf(out, "  ");
    }
    nsAutoString tmp;
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(CallQueryInterface(child, &frameDebug))) {
      frameDebug->GetFrameName(tmp);
    }
    fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
    PRBool isSelected;
    if (NS_SUCCEEDED(child->IsVisibleForPainting(aPresContext, *aRendContext, PR_TRUE, &isSelected))) {
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

  if (nsnull != aDocShell) {
    fprintf(out, "docshell=%p \n", aDocShell);
    nsIPresShell* shell = nsPrintEngine::GetPresShellFor(aDocShell);
    if (shell) {
      nsIViewManager* vm = shell->GetViewManager();
      if (vm) {
        nsIView* root;
        vm->GetRootView(root);
        if (nsnull != root) {
          root->List(out);
        }
      }
    }
    else {
      fputs("null pres shell\n", out);
    }

    // dump the views of the sub documents
    PRInt32 i, n;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    docShellAsNode->GetChildCount(&n);
    for (i = 0; i < n; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
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
                    nsIDeviceContext * aDC,
                    nsIFrame *         aRootFrame,
                    nsIDocShekk *      aDocShell,
                    FILE*              aFD = nsnull)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  if (aPresContext == nsnull || aDC == nsnull) {
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
    nsCOMPtr<nsIRenderingContext> renderingContext;
    aDC->CreateRenderingContext(*getter_AddRefs(renderingContext));
    RootFrameList(aPresContext, fd, 0);
    //DumpFrames(fd, aPresContext, renderingContext, aRootFrame, 0);
    fprintf(fd, "---------------------------------------\n\n");
    fprintf(fd, "--------------- Views From Root Frame----------------\n");
    nsIView* v = aRootFrame->GetView();
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
    if (aFD == nsnull) {
      fclose(fd);
    }
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsList(nsVoidArray * aDocList)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  NS_ASSERTION(aDocList, "Pointer is null!");

  const char types[][3] = {"DC", "FR", "IF", "FS"};
  PR_PL(("Doc List\n***************************************************\n"));
  PR_PL(("T  P A H    PO    DocShell   Seq     Page      Root     Page#    Rect\n"));
  PRInt32 cnt = aDocList->Count();
  for (PRInt32 i=0;i<cnt;i++) {
    nsPrintObject* po = (nsPrintObject*)aDocList->ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    nsIFrame* rootFrame = nsnull;
    if (po->mPresShell) {
      rootFrame = po->mPresShell->FrameManager()->GetRootFrame();
      while (rootFrame != nsnull) {
        nsIPageSequenceFrame * sqf = nsnull;
        if (NS_SUCCEEDED(CallQueryInterface(rootFrame, &sqf))) {
          break;
        }
        rootFrame = rootFrame->GetFirstChild(nsnull);
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
  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i=0;i<cnt;i++) {
    nsPrintObject* po = (nsPrintObject*)aPO->mKids.ElementAt(i);
    NS_ASSERTION(po, "nsPrintObject can't be null!");
    for (PRInt32 k=0;k<aLevel;k++) fprintf(fd, "  ");
    fprintf(fd, "%s %p %p %p %p %d %d,%d,%d,%d\n", types[po->mFrameType], po, po->mDocShell.get(), po->mSeqFrame,
           po->mPageFrame, po->mPageNum, po->mRect.x, po->mRect.y, po->mRect.width, po->mRect.height);
  }
}

//-------------------------------------------------------------
static void GetDocTitleAndURL(nsPrintObject* aPO, char *& aDocStr, char *& aURLStr)
{
  aDocStr = nsnull;
  aURLStr = nsnull;

  PRUnichar * docTitleStr;
  PRUnichar * docURLStr;
  nsPrintEngine::GetDisplayTitleAndURL(aPO,
                                            &docTitleStr, &docURLStr,
                                            nsPrintEngine::eDocTitleDefURLDoc); 

  if (docTitleStr) {
    nsAutoString strDocTitle(docTitleStr);
    aDocStr = ToNewCString(strDocTitle);
    nsMemory::Free(docTitleStr);
  }

  if (docURLStr) {
    nsAutoString strURL(docURLStr);
    aURLStr = ToNewCString(strURL);
    nsMemory::Free(docURLStr);
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsTreeLayout(nsPrintObject * aPO,
                                       nsIDeviceContext * aDC,
                                       int aLevel, FILE * aFD)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  NS_ASSERTION(aPO, "Pointer is null!");
  NS_ASSERTION(aDC, "Pointer is null!");

  const char types[][3] = {"DC", "FR", "IF", "FS"};
  FILE * fd = nsnull;
  if (aLevel == 0) {
    fd = fopen("tree_layout.txt", "w");
    fprintf(fd, "DocTree\n***************************************************\n");
    fprintf(fd, "***************************************************\n");
    fprintf(fd, "T     PO    DocShell   Seq      Page     Page#    Rect\n");
  } else {
    fd = aFD;
  }
  if (fd) {
    nsIFrame* rootFrame = nsnull;
    if (aPO->mPresShell) {
      rootFrame = aPO->mPresShell->FrameManager()->GetRootFrame();
    }
    for (PRInt32 k=0;k<aLevel;k++) fprintf(fd, "  ");
    fprintf(fd, "%s %p %p %p %p %d %d,%d,%d,%d\n", types[aPO->mFrameType], aPO, aPO->mDocShell.get(), aPO->mSeqFrame,
           aPO->mPageFrame, aPO->mPageNum, aPO->mRect.x, aPO->mRect.y, aPO->mRect.width, aPO->mRect.height);
    if (aPO->IsPrintable()) {
      char * docStr;
      char * urlStr;
      GetDocTitleAndURL(aPO, docStr, urlStr);
      DumpLayoutData(docStr, urlStr, aPO->mPresContext, aDC, rootFrame, aPO->mDocShell, fd);
      if (docStr) nsMemory::Free(docStr);
      if (urlStr) nsMemory::Free(urlStr);
    }
    fprintf(fd, "<***************************************************>\n");

    PRInt32 cnt = aPO->mKids.Count();
    for (PRInt32 i=0;i<cnt;i++) {
      nsPrintObject* po = (nsPrintObject*)aPO->mKids.ElementAt(i);
      NS_ASSERTION(po, "nsPrintObject can't be null!");
      DumpPrintObjectsTreeLayout(po, aDC, aLevel+1, fd);
    }
  }
  if (aLevel == 0 && fd) {
    fclose(fd);
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsListStart(const char * aStr, nsVoidArray * aDocList)
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
