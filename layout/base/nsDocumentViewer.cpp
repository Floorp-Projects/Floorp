/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Rosen <dr@netscape.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMWindowInternal.h"

#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsICSSStyleSheet.h"
#include "nsIFrame.h"

#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptGlobalObject.h"
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
#include "nsGenericHTMLElement.h"

#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIDeviceContextSpecFactory.h"
#include "nsIViewManager.h"
#include "nsIView.h"

#include "nsIPref.h"
#include "nsIPageSequenceFrame.h"
#include "nsIURL.h"
#include "nsIWebShell.h"
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
#include "nsLayoutAtoms.h"
#include "nsIFrameManager.h"
#include "nsIParser.h"
#include "nsIPrintContext.h"
#include "nsGUIEvent.h"
#include "nsHTMLReflowState.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIXULDocument.h"  // Temporary code for Bug 136185

#include "nsIChromeRegistry.h"

#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

// Timer Includes
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsITimelineService.h"

#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h" // XXX until atoms get factored into nsLayoutAtoms
#include "nsISimpleEnumerator.h"
#include "nsISupportsPrimitives.h"
static NS_DEFINE_IID(kPrinterEnumeratorCID, NS_PRINTER_ENUMERATOR_CID);

// PrintOptions is now implemented by PrintSettingsService
static const char sPrintSettingsServiceContractID[] = "@mozilla.org/gfx/printsettings-service;1";
static const char sPrintOptionsContractID[]         = "@mozilla.org/gfx/printsettings-service;1";

// Printing Events
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsPrintPreviewListener.h"

// Printing
#include "nsIWebBrowserPrint.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLObjectElement.h"

// Print Preview
#include "nsIPrintPreviewContext.h"
#include "imgIContainer.h" // image animation mode constants
#include "nsIScrollableView.h"
#include "nsIScrollable.h"
#include "nsIWebBrowserPrint.h" // needed for PrintPreview Navigation constants

// Print Progress
#include "nsIPrintProgress.h"
#include "nsIPrintProgressParams.h"


// Print error dialog
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIStringBundle.h"

// Printing Prompts
#include "nsIPrintingPromptService.h"
const char* kPrintingPromptService = "@mozilla.org/embedcomp/printingprompt-service;1";

#define NS_ERROR_GFX_PRINTER_BUNDLE_URL "chrome://global/locale/printing.properties"

// FrameSet
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsINameSpaceManager.h"
#include "nsIWebShell.h"

//focus
#include "nsIDOMEventReceiver.h"
#include "nsIDOMFocusListener.h"
#include "nsISelectionController.h"

#ifdef IBMBIDI
#include "nsBidiUtils.h"
#endif

static NS_DEFINE_CID(kPresShellCID, NS_PRESSHELL_CID);
static NS_DEFINE_CID(kGalleyContextCID,  NS_GALLEYCONTEXT_CID);
static NS_DEFINE_CID(kPrintContextCID,  NS_PRINTCONTEXT_CID);
static NS_DEFINE_CID(kStyleSetCID,  NS_STYLESET_CID);

#ifdef NS_DEBUG

#undef NOISY_VIEWER
#else
#undef NOISY_VIEWER
#endif

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
//-----------------------------------------------------

// PrintObject Document Type
enum PrintObjectType  {eDoc = 0, eFrame = 1, eIFrame = 2, eFrameSet = 3};

class DocumentViewerImpl;
class nsPagePrintTimer;

// New PrintPreview
static NS_DEFINE_CID(kPrintPreviewContextCID,  NS_PRINT_PREVIEW_CONTEXT_CID);

// a small delegate class used to avoid circular references

#ifdef XP_MAC
#pragma mark ** nsDocViewerSelectionListener **
#endif

class nsDocViewerSelectionListener : public nsISelectionListener
{
public:

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsISelectionListerner interface
  NS_DECL_NSISELECTIONLISTENER

                       nsDocViewerSelectionListener()
                       : mDocViewer(NULL)
                       , mGotSelectionState(PR_FALSE)
                       , mSelectionWasCollapsed(PR_FALSE)
                       {
                         NS_INIT_REFCNT();
                       }

  virtual              ~nsDocViewerSelectionListener() {}

  nsresult             Init(DocumentViewerImpl *aDocViewer);

protected:

  DocumentViewerImpl*  mDocViewer;
  PRPackedBool         mGotSelectionState;
  PRPackedBool         mSelectionWasCollapsed;

};


/** editor Implementation of the FocusListener interface
 */
class nsDocViewerFocusListener : public nsIDOMFocusListener
{
public:
  /** default constructor
   */
  nsDocViewerFocusListener();
  /** default destructor
   */
  virtual ~nsDocViewerFocusListener();


/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of focus event handler interface*/
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);
/*END implementations of focus event handler interface*/
  nsresult             Init(DocumentViewerImpl *aDocViewer);

private:
    DocumentViewerImpl*  mDocViewer;
};



#ifdef XP_MAC
#pragma mark ** DocumentViewerImpl **
#endif

//---------------------------------------------------
//-- Object for Caching the Presentation
//---------------------------------------------------
class CachedPresentationObj
{
public:
  CachedPresentationObj(nsIPresShell* aShell, nsIPresContext* aPC,
                        nsIViewManager* aVM, nsIWidget* aW):
    mWindow(aW), mViewManager(aVM), mPresShell(aShell), mPresContext(aPC)
  {
  }

  // The order here is important because the order of destruction is the
  // reverse of the order listed here, and the view manager must outlive
  // the pres shell.
  nsCOMPtr<nsIWidget>      mWindow;
  nsCOMPtr<nsIViewManager> mViewManager;
  nsCOMPtr<nsIPresShell>   mPresShell;
  nsCOMPtr<nsIPresContext> mPresContext;
};

//---------------------------------------------------
//-- PrintObject Class
//---------------------------------------------------
struct PrintObject
{

public:
  PrintObject();
  ~PrintObject(); // non-virtual

  // Methods
  PRBool IsPrintable()  { return !mDontPrint; }
  void   DestroyPresentation();

  // Data Members
  nsCOMPtr<nsIWebShell>    mWebShell;
  PrintObjectType mFrameType;
  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIStyleSet>    mStyleSet;
  nsCOMPtr<nsIPresShell>   mPresShell;
  nsCOMPtr<nsIViewManager> mViewManager;
  nsCOMPtr<nsIWidget>      mWindow;
  nsIView         *mRootView;

  nsIContent      *mContent;
  nsIFrame        *mSeqFrame;
  nsIFrame        *mPageFrame;
  PRInt32          mPageNum;
  nsRect           mRect;
  nsRect           mReflowRect;

  nsVoidArray      mKids;
  PrintObject*     mParent;
  PRPackedBool     mHasBeenPrinted;
  PRPackedBool     mDontPrint;
  PRPackedBool     mPrintAsIs;
  PRPackedBool     mSkippedPageEject;
  PRPackedBool     mSharedPresShell;
  PRPackedBool     mIsHidden;         // Indicates PO is hidden, not reflowed, not shown

  nsRect           mClipRect;

  PRUint16         mImgAnimationMode;
  PRUnichar*       mDocTitle;
  PRUnichar*       mDocURL;
  float            mShrinkRatio;
  nscoord          mXMost;

private:
  PrintObject& operator=(const PrintObject& aOther); // not implemented

};

//------------------------------------------------------------------------
// PrintData Class
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
class PrintData {
public:

  typedef enum {eIsPrinting, eIsPrintPreview } ePrintDataType;

  PrintData(ePrintDataType aType);
  ~PrintData(); // non-virtual

  // Listener Helper Methods
  void OnEndPrinting();
  void OnStartPrinting();
  static void DoOnProgressChange(nsVoidArray& aListeners,
                                 PRInt32      aProgess,
                                 PRInt32      aMaxProgress,
                                 PRBool       aDoStartStop = PR_FALSE,
                                 PRInt32      aFlag = 0);

  ePrintDataType               mType;            // the type of data this is (Printing or Print Preview)
  nsCOMPtr<nsIDeviceContext>   mPrintDC;
  nsIView                     *mPrintView;
  FILE                        *mDebugFilePtr;    // a file where information can go to when printing

  PrintObject *                mPrintObject;
  PrintObject *                mSelectedPO;

  nsVoidArray                      mPrintProgressListeners;
  nsCOMPtr<nsIWebProgressListener> mPrintProgressListener;
  nsCOMPtr<nsIPrintProgress>       mPrintProgress;
  nsCOMPtr<nsIPrintProgressParams> mPrintProgressParams;
  PRBool                           mShowProgressDialog;

  nsCOMPtr<nsIDOMWindowInternal> mCurrentFocusWin; // cache a pointer to the currently focused window

  nsVoidArray*                mPrintDocList;
  nsCOMPtr<nsIDeviceContext>  mPrintDocDC;
  nsCOMPtr<nsIDOMWindow>      mPrintDocDW;
  PRPackedBool                mIsIFrameSelected;
  PRPackedBool                mIsParentAFrameSet;
  PRPackedBool                mPrintingAsIsSubDoc;
  PRPackedBool                mOnStartSent;
  PRPackedBool                mIsAborted;           // tells us the document is being aborted
  PRPackedBool                mPreparingForPrint;   // see comments above
  PRPackedBool                mDocWasToBeDestroyed; // see comments above
  PRBool                      mShrinkToFit;
  PRInt16                     mPrintFrameType;
  PRInt32                     mNumPrintableDocs;
  PRInt32                     mNumDocsPrinted;
  PRInt32                     mNumPrintablePages;
  PRInt32                     mNumPagesPrinted;
  float                       mShrinkRatio;
  float                       mOrigDCScale;
  float                       mOrigTextZoom;
  float                       mOrigZoom;

  nsCOMPtr<nsIPrintSettings>  mPrintSettings;
  nsCOMPtr<nsIPrintOptions>   mPrintOptions;
  nsPrintPreviewListener*     mPPEventListeners;

  // CachedPresentationObj is used to cache the presentation
  // so we can bring it back later
  PRBool HasCachedPres()                  { return mIsCachingPresentation && mCachedPresObj; }
  PRBool IsCachingPres()                  { return mIsCachingPresentation;                   }
  void   SetCacheOldPres(PRBool aDoCache) { mIsCachingPresentation = aDoCache;               }

  PRBool                      mIsCachingPresentation;
  CachedPresentationObj*      mCachedPresObj;

  PRUnichar*            mBrandName; //  needed as a substitute name for a document

private:
  PrintData() {}
  PrintData& operator=(const PrintData& aOther); // not implemented

};

//-------------------------------------------------------------
class DocumentViewerImpl : public nsIDocumentViewer,
                           public nsIContentViewerEdit,
                           public nsIContentViewerFile,
                           public nsIMarkupDocumentViewer,
                           public nsIWebBrowserPrint
{
  friend class nsDocViewerSelectionListener;
  friend class nsPagePrintTimer;
  friend class PrintData;

public:
  DocumentViewerImpl();
  DocumentViewerImpl(nsIPresContext* aPresContext);

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIContentViewer interface...
  NS_DECL_NSICONTENTVIEWER

  // nsIDocumentViewer interface...
  NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet);
  NS_IMETHOD GetDocument(nsIDocument*& aResult);
  NS_IMETHOD GetPresShell(nsIPresShell*& aResult);
  NS_IMETHOD GetPresContext(nsIPresContext*& aResult);
  NS_IMETHOD CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                       nsIDocumentViewer*& aResult);

  // nsIContentViewerEdit
  NS_DECL_NSICONTENTVIEWEREDIT

  // nsIContentViewerFile
  NS_DECL_NSICONTENTVIEWERFILE

  // nsIMarkupDocumentViewer
  NS_DECL_NSIMARKUPDOCUMENTVIEWER

  // nsIWebBrowserPrint
  NS_DECL_NSIWEBBROWSERPRINT

  typedef void (*CallChildFunc)(nsIMarkupDocumentViewer* aViewer,
                                void* aClosure);
  nsresult CallChildren(CallChildFunc aFunc, void* aClosure);

  // Printing Methods
  PRBool   PrintPage(nsIPresContext* aPresContext,
                     nsIPrintSettings* aPrintSettings,
                     PrintObject* aPOect, PRBool& aInRange);
  PRBool   DonePrintingPages(PrintObject* aPO);

  // helper method
  static void GetWebShellTitleAndURL(nsIWebShell* aWebShell,
                                     PRUnichar** aTitle, PRUnichar** aURLStr);

  // This enum tells indicates what the default should be for the title
  // if the title from the document is null
  enum eDocTitleDefault {
    eDocTitleDefNone,
    eDocTitleDefBlank,
    eDocTitleDefURLDoc
  };

  static void GetDisplayTitleAndURL(PrintObject*      aPO, 
                                    nsIPrintSettings* aPrintSettings, 
                                    const PRUnichar*  aBrandName,
                                    PRUnichar**       aTitle, 
                                    PRUnichar**       aURLStr,
                                    eDocTitleDefault  aDefType = eDocTitleDefNone);

protected:
  virtual ~DocumentViewerImpl();

private:
  void ForceRefresh(void);
  nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
  nsresult MakeWindow(nsIWidget* aParentWidget,
                      const nsRect& aBounds);
  nsresult InitInternal(nsIWidget* aParentWidget,
                        nsIDeviceContext* aDeviceContext,
                        const nsRect& aBounds,
                        PRBool aDoCreation);
  nsresult InitPresentationStuff(PRBool aDoInitialReflow);

  nsresult GetDocumentSelection(nsISelection **aSelection,
                                nsIPresShell * aPresShell = nsnull);
  nsresult FindFrameSetWithIID(nsIContent * aParentContent, const nsIID& aIID);
  PRBool   IsThereARangeSelection(nsIDOMWindowInternal * aDOMWin);
  PRBool   IsParentAFrameSet(nsIWebShell * aParent);
  PRBool   IsWebShellAFrameSet(nsIWebShell * aParent);

  PRBool   IsThereAnIFrameSelected(nsIWebShell* aWebShell,
                                   nsIDOMWindowInternal * aDOMWin,
                                   PRPackedBool& aDoesContainFrameset);
  PRBool   IsWindowsInOurSubTree(nsIDOMWindowInternal * aDOMWindow);


  nsresult GetPopupNode(nsIDOMNode** aNode);
  nsresult GetPopupLinkNode(nsIDOMNode** aNode);
  nsresult GetPopupImageNode(nsIDOMNode** aNode);

  //---------------------------------------------------------------------
  void BuildDocTree(nsIDocShellTreeNode * aParentNode,
                    nsVoidArray *         aDocList,
                    PrintObject *         aPO);
  nsresult ReflowDocList(PrintObject * aPO, PRBool aSetPixelScale,
                         PRBool aDoCalcShrink);
  void SetClipRect(PrintObject*  aPO,
                   const nsRect& aClipRect,
                   nscoord       aOffsetX,
                   nscoord       aOffsetY,
                   PRBool        aDoingSetClip);

  nsresult ReflowPrintObject(PrintObject * aPO, PRBool aDoCalcShrink);
  nsresult CalcPageFrameLocation(nsIPresShell * aPresShell,
                                  PrintObject*   aPO);
  PrintObject * FindPrintObjectByWS(PrintObject* aPO, nsIWebShell * aWebShell);
  void MapContentForPO(PrintObject*   aRootObject,
                       nsIPresShell*  aPresShell,
                       nsIContent*    aContent);
  void MapContentToWebShells(PrintObject* aRootPO, PrintObject* aPO);
  nsresult MapSubDocFrameLocations(PrintObject* aPO);
  PrintObject* FindPrintObjectByDOMWin(PrintObject* aParentObject,
                                       nsIDOMWindowInternal * aDOMWin);
  void GetPresShellAndRootContent(nsIWebShell * aWebShell,
                                  nsIPresShell** aPresShell,
                                  nsIContent** aContent);

  void CalcNumPrintableDocsAndPages(PRInt32& aNumDocs, PRInt32& aNumPages);
  void DoProgressForAsIsFrames();
  void DoProgressForSeparateFrames();
  void DoPrintProgress(PRBool aIsForPrinting);
  void SetDocAndURLIntoProgress(PrintObject* aPO,
                                nsIPrintProgressParams* aParams);
  nsresult CheckForPrinters(nsIPrintOptions*  aPrintOptions,
                            nsIPrintSettings* aPrintSettings,
                            PRUint32          aErrorCode,
                            PRBool            aIsPrinting);
  void CleanupDocTitleArray(PRUnichar**& aArray, PRInt32& aCount);
  void CheckForHiddenFrameSetFrames();

  // get the currently infocus frame for the document viewer
  nsIDOMWindowInternal * FindFocusedDOMWindowInternal();

  // get the DOMWindow for a given WebShell
  nsIDOMWindowInternal * GetDOMWinForWebShell(nsIWebShell* aWebShell);

  //
  // The following three methods are used for printing...
  //
  nsresult DocumentReadyForPrinting();
  //nsresult PrintSelection(nsIDeviceContextSpec * aDevSpec);
  nsresult GetSelectionDocument(nsIDeviceContextSpec * aDevSpec,
                                nsIDocument ** aNewDoc);

  nsresult SetupToPrintContent(nsIWebShell*          aParent,
                               nsIDeviceContext*     aDContext,
                               nsIDOMWindowInternal* aCurrentFocusedDOMWin);
  nsresult EnablePOsForPrinting();
  PrintObject* FindXMostPO();
  void FindXMostFrameSize(nsIPresContext* aPresContext,
                          nsIRenderingContext* aRC, nsIFrame* aFrame,
                          nscoord aX, nscoord aY, PRInt32& aMaxWidth);
  void FindXMostFrameInList(nsIPresContext* aPresContext,
                            nsIRenderingContext* aRC, nsIAtom* aList,
                            nsIFrame* aFrame, nscoord aX, nscoord aY,
                            PRInt32& aMaxWidth);

  PRBool   PrintDocContent(PrintObject* aPO, nsresult& aStatus);
  nsresult DoPrint(PrintObject * aPO, PRBool aDoSyncPrinting,
                   PRBool& aDonePrinting);
  void SetPrintAsIs(PrintObject* aPO, PRBool aAsIs = PR_TRUE);

  enum ePrintFlags {eSetPrintFlag = 1U, eSetHiddenFlag = 2U };
  void SetPrintPO(PrintObject* aPO, PRBool aPrint, PRBool aIsHidden = PR_FALSE, PRUint32 aFlags = eSetPrintFlag);

#ifdef NS_PRINT_PREVIEW
  nsresult ShowDocList(PrintObject* aPO, PRBool aShow);
  void InstallNewPresentation();
  void ReturnToGalleyPresentation();
  void TurnScriptingOn(PRBool aDoTurnOn);
  PRBool CheckDocumentForPPCaching();
  void InstallPrintPreviewListener();
#endif


  // Timer Methods
  nsresult StartPagePrintTimer(nsIPresContext * aPresContext,
                               nsIPrintSettings* aPrintSettings,
                               PrintObject*     aPO,
                               PRUint32         aDelay);

  void PrepareToStartLoad(void);

  nsresult SyncParentSubDocMap();

  // Misc
  static void ShowPrintErrorDialog(nsresult printerror,
                                   PRBool aIsPrinting = PR_TRUE);

protected:
  // IMPORTANT: The ownership implicit in the following member
  // variables has been explicitly checked and set using nsCOMPtr
  // for owning pointers and raw COM interface pointers for weak
  // (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).

  nsISupports* mContainer; // [WEAK] it owns me!
  nsCOMPtr<nsIDeviceContext> mDeviceContext;   // ??? can't hurt, but...

  // the following six items are explicitly in this order
  // so they will be destroyed in the reverse order (pinkerton, scc)
  nsCOMPtr<nsIDocument>    mDocument;
  nsCOMPtr<nsIWidget>      mWindow;      // ??? should we really own it?
  nsCOMPtr<nsIViewManager> mViewManager;
  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;

  nsCOMPtr<nsIStyleSheet>  mUAStyleSheet;

  nsCOMPtr<nsISelectionListener> mSelectionListener;
  nsCOMPtr<nsIDOMFocusListener> mFocusListener;

  nsCOMPtr<nsIContentViewer> mPreviousViewer;

  PRBool  mEnableRendering;
  PRBool  mStopped;
  PRBool  mLoaded;
  PRInt16 mNumURLStarts;
  PRInt16 mDestroyRefCount;     // a second "refcount" for the document viewer's "destroy"
  nsIPageSequenceFrame* mPageSeqFrame;


  PrintData*        mPrt;
  nsPagePrintTimer* mPagePrintTimer;

  // These data member support delayed printing when the document is loading
  nsCOMPtr<nsIPrintSettings>       mCachedPrintSettings;
  nsCOMPtr<nsIWebProgressListener> mCachedPrintWebProgressListner;
  PRPackedBool                     mPrintIsPending;
  PRPackedBool                     mPrintDocIsFullyLoaded;

#ifdef NS_PRINT_PREVIEW
  PRBool            mIsDoingPrintPreview; // per DocumentViewer
  nsIWidget*        mParentWidget; // purposely won't be ref counted
  PrintData*        mPrtPreview;
  PrintData*        mOldPrtPreview;
#endif

#ifdef NS_DEBUG
  FILE* mDebugFile;
#endif

  // static memeber variables
  static PRBool mIsCreatingPrintPreview;
  static PRBool mIsDoingPrinting;

  // document management data
  //   these items are specific to markup documents (html and xml)
  //   may consider splitting these out into a subclass
  PRPackedBool mAllowPlugins;
  PRPackedBool mIsSticky;

  /* character set member data */
  nsString mDefaultCharacterSet;
  nsString mHintCharset;
  PRInt32 mHintCharsetSource;
  nsString mForceCharacterSet;
};

//---------------------------------------------------
//-- Page Timer Class
//---------------------------------------------------
class nsPagePrintTimer : public nsITimerCallback
{
public:

  NS_DECL_ISUPPORTS

  nsPagePrintTimer()
      : mDocViewer(nsnull), mPresContext(nsnull), mPrintSettings(nsnull), mDelay(0)
  {
    NS_INIT_ISUPPORTS();
  }

  virtual ~nsPagePrintTimer()
  {
    if (mTimer) {
      mTimer->Cancel();
    }
    DocumentViewerImpl::mIsDoingPrinting = PR_FALSE;
    mDocViewer->Destroy();
    NS_RELEASE(mDocViewer);
  }


  nsresult StartTimer(PRBool aUseDelay = PR_TRUE)
  {
    nsresult result;
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &result);
    if (NS_FAILED(result)) {
      NS_WARNING("unable to start the timer");
    } else {
      mTimer->Init(this, aUseDelay?mDelay:0, PR_TRUE, NS_TYPE_ONE_SHOT);
    }
    return result;
  }



  // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer)
  {
    if (mPresContext && mDocViewer) {
      PRPackedBool initNewTimer = PR_TRUE;
      // Check to see if we are done
      // donePrinting will be true if it completed successfully or
      // if the printing was cancelled
      PRBool inRange;
      PRBool donePrinting = mDocViewer->PrintPage(mPresContext, mPrintSettings, mPrintObj, inRange);
      if (donePrinting) {
        // now clean up print or print the next webshell
        if (mDocViewer->DonePrintingPages(mPrintObj)) {
          initNewTimer = PR_FALSE;
        }
      }

      Stop();
      if (initNewTimer) {
        nsresult result = StartTimer(inRange);
        if (NS_FAILED(result)) {
          donePrinting = PR_TRUE;     // had a failure.. we are finished..
          DocumentViewerImpl::mIsDoingPrinting = PR_FALSE;
        }
      }
    }
  }

  void Init(DocumentViewerImpl* aDocViewerImpl,
            nsIPresContext*     aPresContext,
            nsIPrintSettings*   aPrintSettings,
            PrintObject*        aPO,
            PRUint32            aDelay)
  {
    NS_IF_RELEASE(mDocViewer);
    mDocViewer       = aDocViewerImpl;
    NS_ADDREF(mDocViewer);

    mPresContext     = aPresContext;
    mPrintSettings   = aPrintSettings;
    mPrintObj        = aPO;
    mDelay           = aDelay;
  }

  nsresult Start(DocumentViewerImpl* aDocViewerImpl,
                 nsIPresContext*     aPresContext,
                 nsIPrintSettings*   aPrintSettings,
                 PrintObject*        aPO,
                 PRUint32            aDelay)
  {
    Init(aDocViewerImpl, aPresContext, aPrintSettings, aPO, aDelay);
    return StartTimer(PR_FALSE);
  }


  void Stop()
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nsnull;
    }
  }

private:
  DocumentViewerImpl*        mDocViewer;
  nsIPresContext*            mPresContext;
  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  nsCOMPtr<nsITimer>         mTimer;
  PRUint32                   mDelay;
  PrintObject *              mPrintObj;
};

NS_IMPL_ISUPPORTS1(nsPagePrintTimer, nsITimerCallback)

static nsresult NS_NewUpdateTimer(nsPagePrintTimer **aResult)
{

  NS_PRECONDITION(aResult, "null param");

  nsPagePrintTimer* result = new nsPagePrintTimer;

  if (!result) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(result);
  *aResult = result;

  return NS_OK;
}

//---------------------------------------------------
//-- PrintData Class Impl
//---------------------------------------------------
PrintData::PrintData(ePrintDataType aType) :
  mType(aType), mPrintView(nsnull), mDebugFilePtr(nsnull), mPrintObject(nsnull), mSelectedPO(nsnull),
  mShowProgressDialog(PR_TRUE), mPrintDocList(nsnull), mIsIFrameSelected(PR_FALSE),
  mIsParentAFrameSet(PR_FALSE), mPrintingAsIsSubDoc(PR_FALSE), mOnStartSent(PR_FALSE),
  mIsAborted(PR_FALSE), mPreparingForPrint(PR_FALSE), mDocWasToBeDestroyed(PR_FALSE),
  mShrinkToFit(PR_FALSE), mPrintFrameType(nsIPrintSettings::kFramesAsIs), 
  mNumPrintableDocs(0), mNumDocsPrinted(0), mNumPrintablePages(0), mNumPagesPrinted(0),
  mShrinkRatio(1.0), mOrigDCScale(1.0), mOrigTextZoom(1.0), mOrigZoom(1.0), mPPEventListeners(NULL), 
  mIsCachingPresentation(PR_FALSE), mCachedPresObj(nsnull), mBrandName(nsnull)
{

  nsCOMPtr<nsIStringBundle> brandBundle;
  nsCOMPtr<nsIStringBundleService> svc( do_GetService( NS_STRINGBUNDLE_CONTRACTID ) );
  if (svc) {
    svc->CreateBundle( "chrome://global/locale/brand.properties", getter_AddRefs( brandBundle ) );
    if (brandBundle) {
      brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(), &mBrandName );
    }
  }

  if (!mBrandName) {
    mBrandName = ToNewUnicode(NS_LITERAL_STRING("Mozilla Document"));
  }

}

PrintData::~PrintData()
{

  // Set the cached Zoom value back into the DC
  if (mPrintDC) {
    mPrintDC->SetTextZoom(mOrigTextZoom);
    mPrintDC->SetZoom(mOrigZoom);
  }

  // removed any cached
  if (mCachedPresObj) {
    delete mCachedPresObj;
  }

  // remove the event listeners
  if (mPPEventListeners) {
    mPPEventListeners->RemoveListeners();
    NS_RELEASE(mPPEventListeners);
  }

  // Only Send an OnEndPrinting if we have started printing
  if (mOnStartSent) {
    OnEndPrinting();
  }

  if (mPrintDC && !mDebugFilePtr) {
    PR_PL(("****************** End Document ************************\n"));
    PR_PL(("\n"));
    PRBool isCancelled = PR_FALSE;
    mPrintSettings->GetIsCancelled(&isCancelled);

    nsresult rv = NS_OK;
    if (mType == eIsPrinting) {
      if (!isCancelled && !mIsAborted) {
        rv = mPrintDC->EndDocument();
      } else {
        rv = mPrintDC->AbortDocument();  
      }
      if (NS_FAILED(rv)) {
        DocumentViewerImpl::ShowPrintErrorDialog(rv);
      }
    }
  }

  delete mPrintObject;

  if (mPrintDocList != nsnull) {
    mPrintDocList->Clear();
    delete mPrintDocList;
  }

  if (mBrandName) {
    nsCRT::free(mBrandName);
  }

  DocumentViewerImpl::mIsDoingPrinting = PR_FALSE;

  for (PRInt32 i=0;i<mPrintProgressListeners.Count();i++) {
    nsIWebProgressListener* wpl = NS_STATIC_CAST(nsIWebProgressListener*, mPrintProgressListeners.ElementAt(i));
    NS_ASSERTION(wpl, "nsIWebProgressListener is NULL!");
    NS_RELEASE(wpl);
  }

}

void PrintData::OnStartPrinting()
{
  if (!mOnStartSent) {
    DoOnProgressChange(mPrintProgressListeners, 100, 100, PR_TRUE, nsIWebProgressListener::STATE_START|nsIWebProgressListener::STATE_IS_DOCUMENT);
    mOnStartSent = PR_TRUE;
  }
}

void PrintData::OnEndPrinting()
{
  DoOnProgressChange(mPrintProgressListeners, 100, 100, PR_TRUE, nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_DOCUMENT);
  if (mPrintProgress && mShowProgressDialog) {
    mPrintProgress->CloseProgressDialog(PR_TRUE);
  }
}

void
PrintData::DoOnProgressChange(nsVoidArray& aListeners,
                              PRInt32      aProgess,
                              PRInt32      aMaxProgress,
                              PRBool       aDoStartStop,
                              PRInt32      aFlag)
{
  if (aProgess == 0) return;

  for (PRInt32 i=0;i<aListeners.Count();i++) {
    nsIWebProgressListener* wpl = NS_STATIC_CAST(nsIWebProgressListener*, aListeners.ElementAt(i));
    NS_ASSERTION(wpl, "nsIWebProgressListener is NULL!");
    wpl->OnProgressChange(nsnull, nsnull, aProgess, aMaxProgress, aProgess, aMaxProgress);
    if (aDoStartStop) {
      wpl->OnStateChange(nsnull, nsnull, aFlag, 0);
    }
  }
}


//---------------------------------------------------
//-- PrintObject Class Impl
//---------------------------------------------------
PrintObject::PrintObject() :
  mFrameType(eFrame),
  mRootView(nsnull), mContent(nsnull),
  mSeqFrame(nsnull), mPageFrame(nsnull), mPageNum(-1),
  mRect(0,0,0,0), mReflowRect(0,0,0,0),
  mParent(nsnull), mHasBeenPrinted(PR_FALSE), mDontPrint(PR_TRUE),
  mPrintAsIs(PR_FALSE), mSkippedPageEject(PR_FALSE), mSharedPresShell(PR_FALSE), mIsHidden(PR_FALSE),
  mClipRect(-1,-1, -1, -1),
  mImgAnimationMode(imgIContainer::kNormalAnimMode),
  mDocTitle(nsnull), mDocURL(nsnull), mShrinkRatio(1.0), mXMost(0)
{
}

PrintObject::~PrintObject()
{
  if (mPresContext) {
    mPresContext->SetImageAnimationMode(mImgAnimationMode);
  }

  for (PRInt32 i=0;i<mKids.Count();i++) {
    PrintObject* po = (PrintObject*)mKids[i];
    NS_ASSERTION(po, "PrintObject can't be null!");
    delete po;
  }

  if (mPresShell && !mSharedPresShell) {
    mPresShell->EndObservingDocument();
    mPresShell->Destroy();
  }

  if (mDocTitle) nsMemory::Free(mDocTitle);
  if (mDocURL) nsMemory::Free(mDocURL);

}

//------------------------------------------------------------------
// Resets PO by destroying the presentation
void PrintObject::DestroyPresentation()
{
  mWindow      = nsnull;
  mPresContext = nsnull;
  if (mPresShell) mPresShell->Destroy();
  mPresShell   = nsnull;
  mViewManager = nsnull;
  mStyleSet    = nsnull;
}

//------------------------------------------------------------------
// DocumentViewerImpl
//------------------------------------------------------------------
// Class IDs
static NS_DEFINE_CID(kViewManagerCID,       NS_VIEW_MANAGER_CID);
static NS_DEFINE_CID(kWidgetCID,            NS_CHILD_CID);
static NS_DEFINE_CID(kViewCID,              NS_VIEW_CID);

// Data members
PRBool DocumentViewerImpl::mIsCreatingPrintPreview = PR_FALSE;
PRBool DocumentViewerImpl::mIsDoingPrinting        = PR_FALSE;

//------------------------------------------------------------------
nsresult
NS_NewDocumentViewer(nsIDocumentViewer** aResult)
{
  *aResult = new DocumentViewerImpl();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

// Note: operator new zeros our memory
DocumentViewerImpl::DocumentViewerImpl()
{
  NS_INIT_ISUPPORTS();
  PrepareToStartLoad();

#ifdef NS_PRINT_PREVIEW
  mParentWidget = nsnull;
#endif
}

void DocumentViewerImpl::PrepareToStartLoad()
{
  mEnableRendering  = PR_TRUE;
  mStopped          = PR_FALSE;
  mLoaded           = PR_FALSE;
  mPrt              = nsnull;
  mPrintIsPending   = PR_FALSE;
  mPrintDocIsFullyLoaded = PR_FALSE;

#ifdef NS_PRINT_PREVIEW
  mIsDoingPrintPreview = PR_FALSE;
  mPrtPreview          = nsnull;
  mOldPrtPreview       = nsnull;
#endif

#ifdef NS_DEBUG
  mDebugFile = nsnull;
#endif
}

DocumentViewerImpl::DocumentViewerImpl(nsIPresContext* aPresContext)
  : mPresContext(aPresContext), mAllowPlugins(PR_TRUE), mIsSticky(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  mHintCharsetSource = kCharsetUninitialized;
  PrepareToStartLoad();
}

NS_IMPL_ISUPPORTS6(DocumentViewerImpl,
                   nsIContentViewer,
                   nsIDocumentViewer,
                   nsIMarkupDocumentViewer,
                   nsIContentViewerFile,
                   nsIContentViewerEdit,
                   nsIWebBrowserPrint)

DocumentViewerImpl::~DocumentViewerImpl()
{
  NS_ASSERTION(!mDocument, "User did not call nsIContentViewer::Close");
  if (mDocument) {
    Close();
  }

  NS_ASSERTION(!mPresShell, "User did not call nsIContentViewer::Destroy");
  if (mPresShell) {
    Destroy();
  }

  // XXX(?) Revoke pending invalidate events

  // clear weak references before we go away
  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
  }
}

/*
 * This method is called by the Document Loader once a document has
 * been created for a particular data stream...  The content viewer
 * must cache this document for later use when Init(...) is called.
 *
 * This method is also called when an out of band document.write() happens.
 * In that case, the document passed in is the same as the previous document.
 */
NS_IMETHODIMP
DocumentViewerImpl::LoadStart(nsISupports *aDoc)
{
#ifdef NOISY_VIEWER
  printf("DocumentViewerImpl::LoadStart\n");
#endif

  nsresult rv;
  if (!mDocument) {
    mDocument = do_QueryInterface(aDoc, &rv);
  }
  else if (mDocument == aDoc) {
    // Reset the document viewer's state back to what it was
    // when the document load started.
    PrepareToStartLoad();
  }

  return rv;
}

nsresult
DocumentViewerImpl::SyncParentSubDocMap()
{
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(mContainer));
  nsCOMPtr<nsIDOMWindowInternal> win(do_GetInterface(item));
  nsCOMPtr<nsPIDOMWindow> pwin(do_QueryInterface(win));
  nsCOMPtr<nsIContent> content;

  if (mDocument && pwin) {
    nsCOMPtr<nsIDOMElement> frame_element;
    pwin->GetFrameElementInternal(getter_AddRefs(frame_element));

    content = do_QueryInterface(frame_element);
  }

  if (content) {
    nsCOMPtr<nsIDocShellTreeItem> parent;
    item->GetParent(getter_AddRefs(parent));

    nsCOMPtr<nsIDOMWindow> parent_win(do_GetInterface(parent));

    if (parent_win) {
      nsCOMPtr<nsIDOMDocument> dom_doc;
      parent_win->GetDocument(getter_AddRefs(dom_doc));

      nsCOMPtr<nsIDocument> parent_doc(do_QueryInterface(dom_doc));

      if (parent_doc) {
        return parent_doc->SetSubDocumentFor(content, mDocument);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetContainer(nsISupports* aContainer)
{
  mContainer = aContainer;
  if (mPresContext) {
    mPresContext->SetContainer(aContainer);
  }

  // We're loading a new document into the window where this document
  // viewer lives, sync the parent document's frame element -> sub
  // document map

  return SyncParentSubDocMap();
}

NS_IMETHODIMP
DocumentViewerImpl::GetContainer(nsISupports** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mContainer;
   NS_IF_ADDREF(*aResult);

   return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Init(nsIWidget* aParentWidget,
                         nsIDeviceContext* aDeviceContext,
                         const nsRect& aBounds)
{
  return InitInternal(aParentWidget, aDeviceContext, aBounds, PR_TRUE);
}

nsresult
DocumentViewerImpl::InitPresentationStuff(PRBool aDoInitialReflow)
{
  // Create the style set...
  nsCOMPtr<nsIStyleSet> styleSet;
  nsresult rv = CreateStyleSet(mDocument, getter_AddRefs(styleSet));
  NS_ENSURE_SUCCESS(rv, rv);

  // Now make the shell for the document
  rv = mDocument->CreateShell(mPresContext, mViewManager, styleSet,
                              getter_AddRefs(mPresShell));

  NS_ENSURE_SUCCESS(rv, rv);

  mPresShell->BeginObservingDocument();

  // Initialize our view manager
  nsRect bounds;
  mWindow->GetBounds(bounds);

  float p2t;

  mPresContext->GetPixelsToTwips(&p2t);

  nscoord width = NSIntPixelsToTwips(bounds.width, p2t);
  nscoord height = NSIntPixelsToTwips(bounds.height, p2t);

  mViewManager->DisableRefresh();
  mViewManager->SetWindowDimensions(width, height);

  // Setup default view manager background color

  // This may be overridden by the docshell with the background color
  // for the last document loaded into the docshell
  nscolor bgcolor = NS_RGB(0, 0, 0);
  mPresContext->GetDefaultBackgroundColor(&bgcolor);
  mViewManager->SetDefaultBackgroundColor(bgcolor);

  if (aDoInitialReflow) {
    nsCOMPtr<nsIScrollable> sc = do_QueryInterface(mContainer);

    if (sc) {
      nsCOMPtr<nsIContent> root;
      mDocument->GetRootContent(getter_AddRefs(root));

      nsCOMPtr<nsIDOMHTMLFrameSetElement> frameset(do_QueryInterface(root));

      if (frameset) {
        // If this is a frameset (i.e. not a frame) then we never want
        // scrollbars on it, the scrollbars go inside the frames
        // inside the frameset...

        sc->SetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                           NS_STYLE_OVERFLOW_HIDDEN);
        sc->SetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                           NS_STYLE_OVERFLOW_HIDDEN);
      } else {
        sc->ResetScrollbarPreferences();
      }
    }

    // Initial reflow
    mPresShell->InitialReflow(width, height);

    // Now trigger a refresh
    if (mEnableRendering) {
      mViewManager->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
    }
  }

  // now register ourselves as a selection listener, so that we get
  // called when the selection changes in the window
  nsDocViewerSelectionListener *selectionListener =
    new nsDocViewerSelectionListener();
  NS_ENSURE_TRUE(selectionListener, NS_ERROR_OUT_OF_MEMORY);

  selectionListener->Init(this);

  // mSelectionListener is a owning reference
  mSelectionListener = selectionListener;

  nsCOMPtr<nsISelection> selection;
  rv = GetDocumentSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
  rv = selPrivate->AddSelectionListener(mSelectionListener);
  if (NS_FAILED(rv))
    return rv;

  // Save old listener so we can unregister it
  nsCOMPtr<nsIDOMFocusListener> mOldFocusListener = mFocusListener;

  // focus listener
  //
  // now register ourselves as a focus listener, so that we get called
  // when the focus changes in the window
  nsDocViewerFocusListener *focusListener;
  NS_NEWXPCOM(focusListener, nsDocViewerFocusListener);
  NS_ENSURE_TRUE(focusListener, NS_ERROR_OUT_OF_MEMORY);

  focusListener->Init(this);

  // mFocusListener is a strong reference
  mFocusListener = focusListener;

  // get the DOM event receiver
  nsCOMPtr<nsIDOMEventReceiver> erP(do_QueryInterface(mDocument));
  NS_WARN_IF_FALSE(erP, "No event receiver in document!");

  if (erP) {
    rv = erP->AddEventListenerByIID(mFocusListener,
                                    NS_GET_IID(nsIDOMFocusListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
    if (mOldFocusListener) {
      rv = erP->RemoveEventListenerByIID(mOldFocusListener,
                                      NS_GET_IID(nsIDOMFocusListener));
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remove focus listener");
    }
  }

  return NS_OK;
}

//-----------------------------------------------
// This method can be used to initial the "presentation"
// The aDoCreation indicates whether it should create
// all the new objects or just initialize the existing ones
nsresult
DocumentViewerImpl::InitInternal(nsIWidget* aParentWidget,
                                 nsIDeviceContext* aDeviceContext,
                                 const nsRect& aBounds,
                                 PRBool aDoCreation)
{
#ifdef NS_PRINT_PREVIEW
  mParentWidget = aParentWidget; // not ref counted
#endif

  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  mDeviceContext = dont_QueryInterface(aDeviceContext);

#ifdef NS_PRINT_PREVIEW
  // Clear PrintPreview Alternate Device
  if (mDeviceContext) {
    mDeviceContext->SetAltDevice(nsnull);
    mDeviceContext->SetCanonicalPixelScale(1.0);
  }
#endif

  PRBool makeCX = PR_FALSE;
  if (aDoCreation) {
    if (aParentWidget && !mPresContext) {
      // Create presentation context
      if (mIsCreatingPrintPreview) {
        mPresContext = do_CreateInstance(kPrintPreviewContextCID, &rv);
      } else {
        mPresContext = do_CreateInstance(kGalleyContextCID, &rv);
      }
      if (NS_FAILED(rv))
        return rv;

      mPresContext->Init(aDeviceContext); 

#ifdef NS_PRINT_PREVIEW
      makeCX = !mIsDoingPrintPreview; // needs to be true except when we are already in PP
#else
      makeCX = PR_TRUE;
#endif
    }
  }

  if (aDoCreation && mPresContext) {
    // Create the ViewManager and Root View...

    // We must do this before we tell the script global object about
    // this new document since doing that will cause us to re-enter
    // into nsHTMLFrameInnerFrame code through reflows caused by
    // FlushPendingNotifications() calls down the road...

    rv = MakeWindow(aParentWidget, aBounds);
    NS_ENSURE_SUCCESS(rv, rv);
    Hide();
  }

  nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(mContainer));
  if (requestor) {
    if (mPresContext) {
      nsCOMPtr<nsILinkHandler> linkHandler;
      requestor->GetInterface(NS_GET_IID(nsILinkHandler),
                              getter_AddRefs(linkHandler));
      mPresContext->SetContainer(mContainer);
      mPresContext->SetLinkHandler(linkHandler);
    }

    if (!mIsDoingPrintPreview) {
      // Set script-context-owner in the document

      nsCOMPtr<nsIScriptGlobalObject> global;
      requestor->GetInterface(NS_GET_IID(nsIScriptGlobalObject),
                              getter_AddRefs(global));

      if (global) {
        mDocument->SetScriptGlobalObject(global);
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(mDocument));

        if (domdoc) {
          global->SetNewDocument(domdoc, PR_TRUE);
        }
      }
    }
  }

  if (aDoCreation && mPresContext) {
    // The ViewManager and Root View was created above (in
    // MakeWindow())...

    rv = InitPresentationStuff(!makeCX);
  }

  return rv;
}

//
// LoadComplete(aStatus)
//
//   aStatus - The status returned from loading the document.
//
// This method is called by the container when the document has been
// completely loaded.
//
NS_IMETHODIMP
DocumentViewerImpl::LoadComplete(nsresult aStatus)
{
  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIScriptGlobalObject> global;

  // First, get the script global object from the document...
  rv = mDocument->GetScriptGlobalObject(getter_AddRefs(global));

  // Fail if no ScriptGlobalObject is available...
  NS_ENSURE_TRUE(global, NS_ERROR_NULL_POINTER);

  mLoaded = PR_TRUE;

  /* We need to protect ourself against auto-destruction in case the
     window is closed while processing the OnLoad event.  See bug
     http://bugzilla.mozilla.org/show_bug.cgi?id=78445 for more
     explanation.
  */
  NS_ADDREF_THIS();

  // Now, fire either an OnLoad or OnError event to the document...
  if(NS_SUCCEEDED(aStatus)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;

    event.eventStructType = NS_EVENT;
    event.message = NS_PAGE_LOAD;
    rv = global->HandleDOMEvent(mPresContext, &event, nsnull,
                                NS_EVENT_FLAG_INIT, &status);
#ifdef MOZ_TIMELINE
    // if navigator.xul's load is complete, the main nav window is visible
    // mark that point.

    if (mDocument) {
      //printf("DEBUG: getting uri from document (%p)\n", mDocument.get());

      nsCOMPtr<nsIURI> uri;
      mDocument->GetDocumentURL(getter_AddRefs(uri));

      if (uri) {
        //printf("DEBUG: getting spec fro uri (%p)\n", uri.get());
        nsCAutoString spec;
        uri->GetSpec(spec);
        if (!strcmp(spec.get(), "chrome://navigator/content/navigator.xul")) {
          NS_TIMELINE_MARK("Navigator Window visible now");
        }
      }
    }
#endif /* MOZ_TIMELINE */
  } else {
    // XXX: Should fire error event to the document...
  }

  // Now that the document has loaded, we can tell the presshell
  // to unsuppress painting.
  if (mPresShell && !mStopped) {
    mPresShell->UnsuppressPainting();
  }

  NS_RELEASE_THIS();

  // Check to see if someone tried to print during the load
  if (mPrintIsPending) {
    mPrintIsPending        = PR_FALSE;
    mPrintDocIsFullyLoaded = PR_TRUE;
    Print(mCachedPrintSettings, mCachedPrintWebProgressListner);
    mCachedPrintSettings           = nsnull;
    mCachedPrintWebProgressListner = nsnull;
  }

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::Unload()
{
  nsresult rv;

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  // First, get the script global object from the document...
  nsCOMPtr<nsIScriptGlobalObject> global;

  rv = mDocument->GetScriptGlobalObject(getter_AddRefs(global));
  if (!global) {
    // Fail if no ScriptGlobalObject is available...
    NS_ASSERTION(0, "nsIScriptGlobalObject not set for document!");
    return NS_ERROR_NULL_POINTER;
  }

  // Now, fire an Unload event to the document...
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event;

  event.eventStructType = NS_EVENT;
  event.message = NS_PAGE_UNLOAD;
  rv = global->HandleDOMEvent(mPresContext, &event, nsnull,
                              NS_EVENT_FLAG_INIT, &status);

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::Close()
{
  // All callers are supposed to call close to break circular
  // references.  If we do this stuff in the destructor, the
  // destructor might never be called (especially if we're being
  // used from JS.

  // Close is also needed to disable scripts during paint suppression,
  // since we transfer the existing global object to the new document
  // that is loaded.  In the future, the global object may become a proxy
  // for an object that can be switched in and out so that we don't need
  // to disable scripts during paint suppression.

  if (mDocument) {
#ifdef NS_PRINT_PREVIEW
    // Turn scripting back on
    // after PrintPreview had turned it off
    if (mPrtPreview) {
      TurnScriptingOn(PR_TRUE);
    }
#endif

    // Before we clear the script global object, clear the undisplayed
    // content map, since XBL content can be destroyed by the
    // |SetDocument(null, ...)| triggered by calling
    // |SetScriptGlobalObject(null)|.
    if (mPresShell) {
      nsCOMPtr<nsIFrameManager> frameManager;
      mPresShell->GetFrameManager(getter_AddRefs(frameManager));
      if (frameManager)
        frameManager->ClearUndisplayedContentMap();
    }

    // Break global object circular reference on the document created
    // in the DocViewer Init
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));

    if (globalObject) {
      globalObject->SetNewDocument(nsnull, PR_TRUE);
    }

    // out of band cleanup of webshell
    mDocument->SetScriptGlobalObject(nsnull);

    if (mFocusListener) {
      // get the DOM event receiver
      nsCOMPtr<nsIDOMEventReceiver> erP(do_QueryInterface(mDocument));
      NS_WARN_IF_FALSE(erP, "No event receiver in document!");

      if (erP) {
        erP->RemoveEventListenerByIID(mFocusListener,
                                      NS_GET_IID(nsIDOMFocusListener));
      }
    }
  }

  mDocument = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Destroy()
{
  // Here is where we check to see if the docment was still being prepared 
  // for printing when it was asked to be destroy from someone externally
  // This usually happens if the document is unloaded while the user is in the Print Dialog
  //
  // So we flip the bool to remember that the document is going away
  // and we can clean up and abort later after returning from the Print Dialog
  if (mPrt && mPrt->mPreparingForPrint) {
    mPrt->mDocWasToBeDestroyed = PR_TRUE;
    return NS_OK;
  }

  // Don't let the document get unloaded while we are printing
  // this could happen if we hit the back button during printing
  if (mDestroyRefCount != 0) {
    --mDestroyRefCount;
    return NS_OK;
  }

  // All callers are supposed to call destroy to break circular
  // references.  If we do this stuff in the destructor, the
  // destructor might never be called (especially if we're being
  // used from JS.

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

  // Avoid leaking the old viewer.
  if (mPreviousViewer) {
    mPreviousViewer->Destroy();
    mPreviousViewer = nsnull;
  }

  if (mDeviceContext) {
    mDeviceContext->FlushFontCache();
    mDeviceContext = nsnull;
  }

  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();

    nsCOMPtr<nsISelection> selection;
    GetDocumentSelection(getter_AddRefs(selection));

    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));

    if (selPrivate && mSelectionListener)
      selPrivate->RemoveSelectionListener(mSelectionListener);

    mPresShell->Destroy();
    mPresShell = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Stop(void)
{
  NS_ASSERTION(mDocument, "Stop called too early or too late");
  if (mDocument) {
    mDocument->StopDocumentLoad();
  }

  mStopped = PR_TRUE;

  if (!mLoaded && mPresShell) {
    // Well, we might as well paint what we have so far.
    mPresShell->UnsuppressPainting();
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetDOMDocument(nsIDOMDocument **aResult)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  return CallQueryInterface(mDocument.get(), aResult);
}

NS_IMETHODIMP
DocumentViewerImpl::SetDOMDocument(nsIDOMDocument *aDocument)
{
  // Assumptions:
  //
  // 1) this document viewer has been initialized with a call to Init().
  // 2) the stylesheets associated with the document have been added
  // to the document.

  // XXX Right now, this method assumes that the layout of the current
  // document hasn't started yet.  More cleanup will probably be
  // necessary to make this method work for the case when layout *has*
  // occurred for the current document.
  // That work can happen when and if it is needed.

  nsresult rv;
  if (!aDocument)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDocument> newDoc = do_QueryInterface(aDocument, &rv);
  if (NS_FAILED(rv)) return rv;

  // 0) Replace the old document with the new one
  mDocument = newDoc;

  // 1) Set the script global object on the new document
  nsCOMPtr<nsIScriptGlobalObject> global(do_GetInterface(mContainer));

  if (global) {
    mDocument->SetScriptGlobalObject(global);
    global->SetNewDocument(aDocument, PR_TRUE);

    rv = SyncParentSubDocMap();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // 2) Replace the current pres shell with a new shell for the new document

  if (mPresShell) {
    mPresShell->EndObservingDocument();
    mPresShell->Destroy();

    mPresShell = nsnull;
  }

  // And if we're already given a prescontext...
  if (mPresContext) {
    // 3) Create a new style set for the document

    nsCOMPtr<nsIStyleSet> styleSet;
    rv = CreateStyleSet(mDocument, getter_AddRefs(styleSet));
    if (NS_FAILED(rv))
      return rv;

    rv = newDoc->CreateShell(mPresContext, mViewManager, styleSet,
                             getter_AddRefs(mPresShell));
    NS_ENSURE_SUCCESS(rv, rv);

    mPresShell->BeginObservingDocument();

    // 4) Register the focus listener on the new document

    nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(mDocument, &rv);
    NS_WARN_IF_FALSE(erP, "No event receiver in document!");

    if (erP) {
      rv = erP->AddEventListenerByIID(mFocusListener,
                                      NS_GET_IID(nsIDOMFocusListener));
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
    }
  }

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet)
{
  NS_ASSERTION(aUAStyleSheet, "unexpected null pointer");
  if (aUAStyleSheet) {
    nsCOMPtr<nsICSSStyleSheet> sheet(do_QueryInterface(aUAStyleSheet));
    nsCOMPtr<nsICSSStyleSheet> newSheet;
    sheet->Clone(*getter_AddRefs(newSheet));
    mUAStyleSheet = newSheet;
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetDocument(nsIDocument*& aResult)
{
  aResult = mDocument;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetPresShell(nsIPresShell*& aResult)
{
  aResult = mPresShell;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetPresContext(nsIPresContext*& aResult)
{
  aResult = mPresContext;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetBounds(nsRect& aResult)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->GetBounds(aResult);
  }
  else {
    aResult.SetRect(0, 0, 0, 0);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetPreviousViewer(nsIContentViewer** aViewer)
{
  *aViewer = mPreviousViewer;
  NS_IF_ADDREF(*aViewer);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetPreviousViewer(nsIContentViewer* aViewer)
{
  // NOTE:  |Show| sets |mPreviousViewer| to null without calling this
  // function.

  if (aViewer) {
    NS_ASSERTION(!mPreviousViewer,
                 "can't set previous viewer when there already is one");

    // In a multiple chaining situation (which occurs when running a thrashing
    // test like i-bench or jrgm's tests with no delay), we can build up a
    // whole chain of viewers.  In order to avoid this, we always set our previous
    // viewer to the MOST previous viewer in the chain, and then dump the intermediate
    // link from the chain.  This ensures that at most only 2 documents are alive
    // and undestroyed at any given time (the one that is showing and the one that
    // is loading with painting suppressed).
    nsCOMPtr<nsIContentViewer> prevViewer;
    aViewer->GetPreviousViewer(getter_AddRefs(prevViewer));
    if (prevViewer) {
      aViewer->SetPreviousViewer(nsnull);
      aViewer->Destroy();
      return SetPreviousViewer(prevViewer);
    }
  }

  mPreviousViewer = aViewer;
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetBounds(const nsRect& aBounds)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  if (mWindow) {
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height,
                    PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Move(PRInt32 aX, PRInt32 aY)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Move(aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Show(void)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  // We don't need the previous viewer anymore since we're not
  // displaying it.
  if (mPreviousViewer) {
    // This little dance *may* only be to keep
    // PresShell::EndObservingDocument happy, but I'm not sure.
    nsCOMPtr<nsIContentViewer> prevViewer(mPreviousViewer);
    mPreviousViewer = nsnull;
    prevViewer->Destroy();
  }

  if (mWindow) {
    mWindow->Show(PR_TRUE);
  }

  if (mDocument && !mPresShell && !mWindow) {
    nsresult rv;

    nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mContainer));
    NS_ENSURE_TRUE(base_win, NS_ERROR_UNEXPECTED);

    base_win->GetParentWidget(&mParentWidget);
    NS_ENSURE_TRUE(mParentWidget, NS_ERROR_UNEXPECTED);

    mDeviceContext = dont_AddRef(mParentWidget->GetDeviceContext());

#ifdef NS_PRINT_PREVIEW
    // Clear PrintPreview Alternate Device
    if (mDeviceContext) {
      mDeviceContext->SetAltDevice(nsnull);
    }
#endif

    // Create presentation context
    if (mIsCreatingPrintPreview) {
      NS_ERROR("Whoa, we should not get here!");

      return NS_ERROR_UNEXPECTED;
    }

    mPresContext = do_CreateInstance(kGalleyContextCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mPresContext->Init(mDeviceContext);

    nsRect tbounds;
    mParentWidget->GetBounds(tbounds);

    float p2t;
    mPresContext->GetPixelsToTwips(&p2t);
    tbounds *= p2t;

    // Create the view manager
    mViewManager = do_CreateInstance(kViewManagerCID);
    NS_ENSURE_TRUE(mViewManager, NS_ERROR_NOT_AVAILABLE);

    // Initialize the view manager with an offset. This allows the
    // viewmanager to manage a coordinate space offset from (0,0)
    rv = mViewManager->Init(mDeviceContext);
    if (NS_FAILED(rv))
      return rv;

    rv = mViewManager->SetWindowOffset(tbounds.x, tbounds.y);
    if (NS_FAILED(rv))
      return rv;

    // Reset the bounds offset so the root view is set to 0,0. The
    // offset is specified in nsIViewManager::Init above.
    // Besides, layout will reset the root view to (0,0) during reflow,
    // so changing it to 0,0 eliminates placing the root view in the
    // wrong place initially.
    tbounds.x = 0;
    tbounds.y = 0;

    // Create a child window of the parent that is our "root
    // view/window" Create a view

    nsIView *view = nsnull;
    rv = CallCreateInstance(kViewCID, &view);
    if (NS_FAILED(rv))
      return rv;
    rv = view->Init(mViewManager, tbounds, nsnull);
    if (NS_FAILED(rv))
      return rv;

    rv = view->CreateWidget(kWidgetCID, nsnull,
                            mParentWidget->GetNativeData(NS_NATIVE_WIDGET),
                            PR_TRUE, PR_FALSE);

    if (NS_FAILED(rv))
      return rv;

    // Setup hierarchical relationship in view manager
    mViewManager->SetRootView(view);

    view->GetWidget(*getter_AddRefs(mWindow));

    if (mPresContext && mContainer) {
      nsCOMPtr<nsILinkHandler> linkHandler(do_GetInterface(mContainer));

      if (linkHandler) {
        mPresContext->SetLinkHandler(linkHandler);
      }

      mPresContext->SetContainer(mContainer);
    }

    if (mPresContext) {
      Hide();

      rv = InitPresentationStuff(PR_TRUE);
    }

    // If we get here the document load has already started and the
    // window is shown because some JS on the page caused it to be
    // shown...

    mPresShell->UnsuppressPainting();
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Hide(void)
{
  PRBool is_in_print_mode = PR_FALSE;

  GetDoingPrint(&is_in_print_mode);

  if (is_in_print_mode) {
    // If we, or one of our parents, is in print mode it means we're
    // right now returning from print and the layout frame that was
    // created for this document is being destroyed. In such a case we
    // ignore the Hide() call.

    return NS_OK;
  }

  GetDoingPrintPreview(&is_in_print_mode);

  if (is_in_print_mode) {
    // If we, or one of our parents, is in print preview mode it means
    // we're right now returning from print preview and the layout
    // frame that was created for this document is being destroyed. In
    // such a case we ignore the Hide() call.

    return NS_OK;
  }

  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Show(PR_FALSE);
  }

  if (!mPresShell || mIsDoingPrintPreview) {
    return NS_OK;
  }

  // Avoid leaking the old viewer.
  if (mPreviousViewer) {
    mPreviousViewer->Destroy();
    mPreviousViewer = nsnull;
  }

  if (mIsSticky) {
    // This window is sticky, that means that it might be shown again
    // and we don't want the presshell n' all that to be thrown away
    // just because the window is hidden.

    return NS_OK;
  }

  if (mDeviceContext) {
    mDeviceContext->FlushFontCache();
  }

  // Break circular reference (or something)
  mPresShell->EndObservingDocument();
  nsCOMPtr<nsISelection> selection;

  GetDocumentSelection(getter_AddRefs(selection));

  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));

  if (selPrivate && mSelectionListener) {
    selPrivate->RemoveSelectionListener(mSelectionListener);
  }

  nsCOMPtr<nsIXULDocument> xul_doc(do_QueryInterface(mDocument));

  if (xul_doc) {
    xul_doc->OnHide();
  }

  mPresShell->Destroy();

  mPresShell     = nsnull;
  mPresContext   = nsnull;
  mViewManager   = nsnull;
  mWindow        = nsnull;
  mDeviceContext = nsnull;
  mParentWidget  = nsnull;

  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mContainer));

  if (base_win) {
    base_win->SetParentWidget(nsnull);
  }

  return NS_OK;
}

nsresult
DocumentViewerImpl::FindFrameSetWithIID(nsIContent * aParentContent,
                                        const nsIID& aIID)
{
  PRInt32 numChildren;
  aParentContent->ChildCount(numChildren);

  // do a breadth search across all siblings
  PRInt32 inx;
  for (inx=0;inx<numChildren;inx++) {
    nsCOMPtr<nsIContent> child;
    if (NS_SUCCEEDED(aParentContent->ChildAt(inx, *getter_AddRefs(child))) && child) {
      nsCOMPtr<nsISupports> temp;
      if (NS_SUCCEEDED(child->QueryInterface(aIID, (void**)getter_AddRefs(temp)))) {
        return NS_OK;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  Helper function
 */
static nsIPresShell*
GetPresShellFor(nsIDocShell* aDocShell)
{
  nsCOMPtr<nsIDOMDocument> domDoc(do_GetInterface(aDocShell));
  if (!domDoc) return nsnull;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc) return nsnull;

  nsIPresShell* shell = nsnull;
  doc->GetShellAt(0, &shell);

  return shell;
}

//---------------------------------------------------------------
//---------------------------------------------------------------
//-- Debug helper routines
//---------------------------------------------------------------
//---------------------------------------------------------------
#if defined(XP_PC) && defined(EXTENDED_DEBUG_PRINTING)
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
static void RootFrameList(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  if((nsnull == aPresContext) || (nsnull == out))
    return;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (nsnull != shell) {
    nsIFrame* frame;
    shell->GetRootFrame(&frame);
    if(nsnull != frame) {
      nsIFrameDebug* debugFrame;
      nsresult rv;
      rv = frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&debugFrame);
      if(NS_SUCCEEDED(rv))
        debugFrame->List(aPresContext, out, aIndent);
    }
  }
}

/** ---------------------------------------------------
 *  Dumps Frames for Printing
 */
static void DumpFrames(FILE*                 out,
                       nsIPresContext*       aPresContext,
                       nsIRenderingContext * aRendContext,
                       nsIFrame *            aFrame,
                       PRInt32               aLevel)
{
  NS_ASSERTION(out, "Pointer is null!");
  NS_ASSERTION(aPresContext, "Pointer is null!");
  NS_ASSERTION(aRendContext, "Pointer is null!");
  NS_ASSERTION(aFrame, "Pointer is null!");

  nsIFrame * child;
  aFrame->FirstChild(aPresContext, nsnull, &child);
  while (child != nsnull) {
    for (PRInt32 i=0;i<aLevel;i++) {
     fprintf(out, "  ");
    }
    nsAutoString tmp;
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(CallQueryInterface(child, &frameDebug))) {
      frameDebug->GetFrameName(tmp);
    }
    fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
    nsFrameState state;
    child->GetFrameState(&state);
    PRBool isSelected;
    if (NS_SUCCEEDED(child->IsVisibleForPainting(aPresContext, *aRendContext, PR_TRUE, &isSelected))) {
      fprintf(out, " %p %s", child, isSelected?"VIS":"UVS");
      nsRect rect;
      child->GetRect(rect);
      fprintf(out, "[%d,%d,%d,%d] ", rect.x, rect.y, rect.width, rect.height);
      nsIView * view;
      child->GetView(aPresContext, &view);
      fprintf(out, "v: %p ", view);
      fprintf(out, "\n");
      DumpFrames(out, aPresContext, aRendContext, child, aLevel+1);
      child->GetNextSibling(&child);
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
    nsIPresShell* shell = GetPresShellFor(aDocShell);
    if (nsnull != shell) {
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsIView* root;
        vm->GetRootView(root);
        if (nsnull != root) {
          root->List(out);
        }
      }
      NS_RELEASE(shell);
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
                    nsIPresContext*    aPresContext,
                    nsIDeviceContext * aDC,
                    nsIFrame *         aRootFrame,
                    nsIWebShell *      aWebShell,
                    FILE*              aFD = nsnull)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  if (aPresContext == nsnull || aDC == nsnull) {
    return;
  }

#ifdef NS_PRINT_PREVIEW
  nsCOMPtr<nsIPrintPreviewContext> ppContext = do_QueryInterface(aPresContext);
  if (ppContext) {
    return;
  }
#endif

  NS_ASSERTION(aRootFrame, "Pointer is null!");
  NS_ASSERTION(aWebShell, "Pointer is null!");

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
    nsIView * v;
    aRootFrame->GetView(aPresContext, &v);
    if (v) {
      v->List(fd);
    } else {
      printf("View is null!\n");
    }
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aWebShell));
    if (docShell) {
      fprintf(fd, "--------------- All Views ----------------\n");
      DumpViews(docShell, fd);
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

  char * types[] = {"DC", "FR", "IF", "FS"};
  PR_PL(("Doc List\n***************************************************\n"));
  PR_PL(("T  P A H    PO    WebShell   Seq     Page      Root     Page#    Rect\n"));
  PRInt32 cnt = aDocList->Count();
  for (PRInt32 i=0;i<cnt;i++) {
    PrintObject* po = (PrintObject*)aDocList->ElementAt(i);
    NS_ASSERTION(po, "PrintObject can't be null!");
    nsIFrame* rootFrame = nsnull;
    if (po->mPresShell) {
      po->mPresShell->GetRootFrame(&rootFrame);
      while (rootFrame != nsnull) {
        nsIPageSequenceFrame * sqf = nsnull;
        if (NS_SUCCEEDED(CallQueryInterface(rootFrame, &sqf))) {
          break;
        }
        rootFrame->FirstChild(po->mPresContext, nsnull, &rootFrame);
      }
    }

    PR_PL(("%s %d %d %d %p %p %p %p %p   %d   %d,%d,%d,%d\n", types[po->mFrameType],
            po->IsPrintable(), po->mPrintAsIs, po->mHasBeenPrinted, po, po->mWebShell, po->mSeqFrame,
            po->mPageFrame, rootFrame, po->mPageNum, po->mRect.x, po->mRect.y, po->mRect.width, po->mRect.height));
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsTree(PrintObject * aPO, int aLevel= 0, FILE* aFD = nsnull)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  NS_ASSERTION(aPO, "Pointer is null!");

  FILE * fd = aFD?aFD:stdout;
  char * types[] = {"DC", "FR", "IF", "FS"};
  if (aLevel == 0) {
    fprintf(fd, "DocTree\n***************************************************\n");
    fprintf(fd, "T     PO    WebShell   Seq      Page     Page#    Rect\n");
  }
  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i=0;i<cnt;i++) {
    PrintObject* po = (PrintObject*)aPO->mKids.ElementAt(i);
    NS_ASSERTION(po, "PrintObject can't be null!");
    for (PRInt32 k=0;k<aLevel;k++) fprintf(fd, "  ");
    fprintf(fd, "%s %p %p %p %p %d %d,%d,%d,%d\n", types[po->mFrameType], po, po->mWebShell, po->mSeqFrame,
           po->mPageFrame, po->mPageNum, po->mRect.x, po->mRect.y, po->mRect.width, po->mRect.height);
  }
}

//-------------------------------------------------------------
static void GetDocTitleAndURL(PrintObject* aPO, char *& aDocStr, char *& aURLStr)
{
  aDocStr = nsnull;
  aURLStr = nsnull;

  PRUnichar * mozillaDoc = ToNewUnicode(NS_LITERAL_STRING("Mozilla Document"));
  PRUnichar * docTitleStr;
  PRUnichar * docURLStr;
  DocumentViewerImpl::GetDisplayTitleAndURL(aPO, nsnull, mozillaDoc,
                                            &docTitleStr, &docURLStr,
                                            DocumentViewerImpl::eDocTitleDefURLDoc); 

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

  if (mozillaDoc) {
    nsMemory::Free(mozillaDoc);
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsTreeLayout(PrintObject * aPO,
                                       nsIDeviceContext * aDC,
                                       int aLevel= 0, FILE * aFD = nsnull)
{
  if (!kPrintingLogMod || kPrintingLogMod->level != DUMP_LAYOUT_LEVEL) return;

  NS_ASSERTION(aPO, "Pointer is null!");
  NS_ASSERTION(aDC, "Pointer is null!");

  char * types[] = {"DC", "FR", "IF", "FS"};
  FILE * fd = nsnull;
  if (aLevel == 0) {
    fd = fopen("tree_layout.txt", "w");
    fprintf(fd, "DocTree\n***************************************************\n");
    fprintf(fd, "***************************************************\n");
    fprintf(fd, "T     PO    WebShell   Seq      Page     Page#    Rect\n");
  } else {
    fd = aFD;
  }
  if (fd) {
    nsIFrame* rootFrame = nsnull;
    if (aPO->mPresShell) {
      aPO->mPresShell->GetRootFrame(&rootFrame);
    }
    for (PRInt32 k=0;k<aLevel;k++) fprintf(fd, "  ");
    fprintf(fd, "%s %p %p %p %p %d %d,%d,%d,%d\n", types[aPO->mFrameType], aPO, aPO->mWebShell, aPO->mSeqFrame,
           aPO->mPageFrame, aPO->mPageNum, aPO->mRect.x, aPO->mRect.y, aPO->mRect.width, aPO->mRect.height);
    if (aPO->IsPrintable()) {
      char * docStr;
      char * urlStr;
      GetDocTitleAndURL(aPO, docStr, urlStr);
      DumpLayoutData(docStr, urlStr, aPO->mPresContext, aDC, rootFrame, aPO->mWebShell, fd);
      if (docStr) nsMemory::Free(docStr);
      if (urlStr) nsMemory::Free(urlStr);
    }
    fprintf(fd, "<***************************************************>\n");

    PRInt32 cnt = aPO->mKids.Count();
    for (PRInt32 i=0;i<cnt;i++) {
      PrintObject* po = (PrintObject*)aPO->mKids.ElementAt(i);
      NS_ASSERTION(po, "PrintObject can't be null!");
      DumpPrintObjectsTreeLayout(po, aDC, aLevel+1, fd);
    }
  }
  if (aLevel == 0 && fd) {
    fclose(fd);
  }
}

//-------------------------------------------------------------
static void DumpPrintObjectsListStart(char * aStr, nsVoidArray * aDocList)
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
//---------------------------------------------------------------

/** ---------------------------------------------------
 *  Giving a child frame it searches "up" the tree until it
 *  finds a "Page" frame.
 */
static nsIFrame * GetPageFrame(nsIFrame * aFrame)
{
  nsIFrame * frame = aFrame;
  while (frame != nsnull) {
    nsCOMPtr<nsIAtom> type;
    frame->GetFrameType(getter_AddRefs(type));
    if (type.get() == nsLayoutAtoms::pageFrame) {
      return frame;
    }
    frame->GetParent(&frame);
  }
  return nsnull;
}


/** ---------------------------------------------------
 *  Find by checking content's tag type
 */
static nsIFrame * FindFrameByType(nsIPresContext* aPresContext,
                                  nsIFrame *      aParentFrame,
                                  nsIAtom *       aType,
                                  nsRect&         aRect,
                                  nsRect&         aChildRect)
{
  NS_ASSERTION(aPresContext, "Pointer is null!");
  NS_ASSERTION(aParentFrame, "Pointer is null!");
  NS_ASSERTION(aType, "Pointer is null!");

  nsIFrame * child;
  nsRect rect;
  aParentFrame->GetRect(rect);
  aRect.x += rect.x;
  aRect.y += rect.y;
  aParentFrame->FirstChild(aPresContext, nsnull, &child);
  while (child != nsnull) {
    nsCOMPtr<nsIContent> content;
    child->GetContent(getter_AddRefs(content));
    if (content) {
      nsCOMPtr<nsIAtom> type;
      content->GetTag(*getter_AddRefs(type));
      if (type.get() == aType) {
        nsRect r;
        child->GetRect(r);
        aChildRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
        aRect.x -= rect.x;
        aRect.y -= rect.y;
        return child;
      }
    }
    nsIFrame * fndFrame = FindFrameByType(aPresContext, child, aType, aRect, aChildRect);
    if (fndFrame != nsnull) {
      return fndFrame;
    }
    child->GetNextSibling(&child);
  }
  aRect.x -= rect.x;
  aRect.y -= rect.y;
  return nsnull;
}

/** ---------------------------------------------------
 *  Find by checking frames type
 */
static nsresult FindSelectionBounds(nsIPresContext* aPresContext,
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

  nsIFrame * child;
  aParentFrame->FirstChild(aPresContext, nsnull, &child);
  nsRect rect;
  aParentFrame->GetRect(rect);
  aRect.x += rect.x;
  aRect.y += rect.y;
  while (child != nsnull) {
    nsFrameState state;
    child->GetFrameState(&state);
    // only leaf frames have this bit flipped
    // then check the hard way
    PRBool isSelected = (state & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
    if (isSelected) {
      if (NS_FAILED(child->IsVisibleForPainting(aPresContext, aRC, PR_TRUE, &isSelected))) {
        return NS_ERROR_FAILURE;
      }
    }

    if (isSelected) {
      nsRect r;
      child->GetRect(r);
      if (aStartFrame == nsnull) {
        aStartFrame = child;
        aStartRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
      } else {
        child->GetRect(r);
        aEndFrame = child;
        aEndRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
      }
    }
    FindSelectionBounds(aPresContext, aRC, child, aRect, aStartFrame, aStartRect, aEndFrame, aEndRect);
    child->GetNextSibling(&child);
  }
  aRect.x -= rect.x;
  aRect.y -= rect.y;
  return NS_OK;
}

/** ---------------------------------------------------
 *  This method finds the starting and ending page numbers
 *  of the selection and also returns rect for each where
 *  the x,y of the rect is relative to the very top of the
 *  frame tree (absolutely positioned)
 */
static nsresult GetPageRangeForSelection(nsIPresShell *        aPresShell,
                                         nsIPresContext*       aPresContext,
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
  nsRect rect;
  seqFrame->GetRect(rect);

  // start out with the sequence frame and search the entire frame tree
  // capturing the the starting and ending child frames of the selection
  // and their rects
  FindSelectionBounds(aPresContext, aRC, seqFrame, rect, startFrame, aStartRect, endFrame, aEndRect);

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
      startPageFrame = GetPageFrame(startFrame);
      endPageFrame   = startPageFrame;
      aEndRect       = aStartRect;
    } else {
      startPageFrame = GetPageFrame(startFrame);
      endPageFrame   = GetPageFrame(endFrame);
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
  nsIFrame * child;
  seqFrame->FirstChild(aPresContext, nsnull, &child);
  while (child != nsnull) {
    printf("Page: %d - %p\n", pageNum, child);
    pageNum++;
    child->GetNextSibling(&child);
  }
  }
#endif

  // Now that we have the page frames
  // find out what the page numbers are for each frame
  PRInt32 pageNum = 1;
  nsIFrame * page;
  seqFrame->FirstChild(aPresContext, nsnull, &page);
  while (page != nsnull) {
    if (page == startPageFrame) {
      aStartPageNum = pageNum;
    }
    if (page == endPageFrame) {
      aEndPageNum = pageNum;
    }
    pageNum++;
    page->GetNextSibling(&page);
  }

#ifdef DEBUG_rodsX
  printf("Start Page No: %d\n", aStartPageNum);
  printf("End Page No:   %d\n", aEndPageNum);
#endif

  *aStartFrame = startPageFrame;
  *aEndFrame   = endPageFrame;

  return NS_OK;
}

//-------------------------------------------------------
PRBool
DocumentViewerImpl::IsParentAFrameSet(nsIWebShell * aParent)
{
  NS_ASSERTION(aParent, "Pointer is null!");

  // See if if the incoming doc is the root document
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
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  if (shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIContent> rootContent;
      doc->GetRootContent(getter_AddRefs(rootContent));
      if (rootContent) {
        if (NS_SUCCEEDED(FindFrameSetWithIID(rootContent, NS_GET_IID(nsIDOMHTMLFrameSetElement)))) {
          isFrameSet = PR_TRUE;
        }
      }
    }
  }
  return isFrameSet;
}

//-------------------------------------------------------
PRBool
DocumentViewerImpl::IsWebShellAFrameSet(nsIWebShell * aWebShell)
{
  NS_ASSERTION(aWebShell, "Pointer is null!");

  PRBool doesContainFrameSet = PR_FALSE;
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsIContent>   rootContent;
  GetPresShellAndRootContent(aWebShell, getter_AddRefs(presShell), getter_AddRefs(rootContent));
  if (rootContent) {
    if (NS_SUCCEEDED(FindFrameSetWithIID(rootContent, NS_GET_IID(nsIDOMHTMLFrameSetElement)))) {
      doesContainFrameSet = PR_TRUE;
    }
  }
  return doesContainFrameSet;
}

//-------------------------------------------------------
void
DocumentViewerImpl::GetWebShellTitleAndURL(nsIWebShell * aWebShell,
                                           PRUnichar**   aTitle,
                                           PRUnichar**   aURLStr)
{
  NS_ASSERTION(aWebShell, "Pointer is null!");
  NS_ASSERTION(aTitle, "Pointer is null!");
  NS_ASSERTION(aURLStr, "Pointer is null!");

  *aTitle  = nsnull;
  *aURLStr = nsnull;

  // now get the actual values if the PrintSettings didn't have any
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aWebShell));
  if (!docShell) return;

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell) return;

  nsCOMPtr<nsIDocument> doc;
  presShell->GetDocument(getter_AddRefs(doc));
  if (!doc) return;

  const nsString* docTitle = doc->GetDocumentTitle();
  if (docTitle && !docTitle->IsEmpty()) {
    *aTitle = ToNewUnicode(*docTitle);
  }

  nsCOMPtr<nsIURI> url;
  doc->GetDocumentURL(getter_AddRefs(url));
  if (!url) return;

  nsCAutoString urlCStr;
  url->GetSpec(urlCStr);
  *aURLStr = ToNewUnicode(NS_ConvertUTF8toUCS2(urlCStr));
}

//-------------------------------------------------------
// This will first use a Title and/or URL from the PrintSettings
// if one isn't set then it uses the one from the document
// then if not title is there we will make sure we send something back
// depending on the situation.
void
DocumentViewerImpl::GetDisplayTitleAndURL(PrintObject*      aPO,
                                          nsIPrintSettings* aPrintSettings,
                                          const PRUnichar*  aBrandName,
                                          PRUnichar**       aTitle, 
                                          PRUnichar**       aURLStr,
                                          eDocTitleDefault  aDefType)
{
  NS_ASSERTION(aBrandName, "Pointer is null!");
  NS_ASSERTION(aPO, "Pointer is null!");
  NS_ASSERTION(aTitle, "Pointer is null!");
  NS_ASSERTION(aURLStr, "Pointer is null!");

  *aTitle  = nsnull;
  *aURLStr = nsnull;

  // First check to see if the PrintSettings has defined an alternate title
  // and use that if it did
  PRUnichar * docTitleStrPS = nsnull;
  PRUnichar * docURLStrPS   = nsnull;
  if (aPrintSettings) {
    aPrintSettings->GetTitle(&docTitleStrPS);
    aPrintSettings->GetDocURL(&docURLStrPS);

    if (docTitleStrPS && nsCRT::strlen(docTitleStrPS) > 0) {
      *aTitle  = docTitleStrPS;
    }

    if (docURLStrPS && nsCRT::strlen(docURLStrPS) > 0) {
      *aURLStr  = docURLStrPS;
    }

    // short circut
    if (docTitleStrPS && docURLStrPS) {
      return;
    }
  }

  if (!docURLStrPS) {
    if (aPO->mDocURL) {
      *aURLStr = nsCRT::strdup(aPO->mDocURL);
    }
  }

  if (!docTitleStrPS) {
    if (aPO->mDocTitle) {
      *aTitle = nsCRT::strdup(aPO->mDocTitle);
    } else {
      switch (aDefType) {
        case eDocTitleDefBlank: *aTitle = ToNewUnicode(NS_LITERAL_STRING(""));
          break;

        case eDocTitleDefURLDoc:
          if (*aURLStr) {
            *aTitle = nsCRT::strdup(*aURLStr);
          } else {
            if (aBrandName) *aTitle = nsCRT::strdup(aBrandName);
          }
          break;

        default:
          break;
      } // switch
    }
  }

}


//-------------------------------------------------------
PRBool
DocumentViewerImpl::DonePrintingPages(PrintObject* aPO)
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

  DoProgressForAsIsFrames();
  DoProgressForSeparateFrames();

  delete mPrt;
  mPrt = nsnull;
  mIsDoingPrinting = PR_FALSE;

  NS_IF_RELEASE(mPagePrintTimer);

  return PR_TRUE;
}

//-------------------------------------------------------
PRBool
DocumentViewerImpl::PrintPage(nsIPresContext*   aPresContext,
                              nsIPrintSettings* aPrintSettings,
                              PrintObject*      aPO,
                              PRBool&           aInRange)
{
  NS_ASSERTION(aPresContext,   "aPresContext is null!");
  NS_ASSERTION(aPrintSettings, "aPrintSettings is null!");
  NS_ASSERTION(aPO,            "aPO is null!");
  NS_ASSERTION(mPageSeqFrame,  "mPageSeqFrame is null!");
  NS_ASSERTION(mPrt,           "mPrt is null!");

  // Although these should NEVER be NULL
  // This is added insurance, to make sure we don't crash in optimized builds
  if (!mPrt || !aPresContext || !aPrintSettings || !aPO || !mPageSeqFrame) {
    ShowPrintErrorDialog(NS_ERROR_FAILURE);
    return PR_TRUE; // means we are done printing
  }

  PR_PL(("-----------------------------------\n"));
  PR_PL(("------ In DV::PrintPage PO: %p (%s)\n", aPO, gFrameTypesStr[aPO->mFrameType]));

  PRBool isCancelled = PR_FALSE;

  // Check setting to see if someone request it be cancelled (programatically)
  aPrintSettings->GetIsCancelled(&isCancelled);
  if (!isCancelled) {
    // If not, see if the user has cancelled it
    if (mPrt->mPrintProgress) {
      mPrt->mPrintProgress->GetProcessCanceledByUser(&isCancelled);
    }
  }

  // DO NOT allow the print job to be cancelled if it is Print FrameAsIs
  // because it is only printing one page.
  if (isCancelled) {
    if (mPrt->mPrintFrameType == nsIPrintSettings::kFramesAsIs) {
      aPrintSettings->SetIsCancelled(PR_FALSE);
    } else {
      aPrintSettings->SetIsCancelled(PR_TRUE);
      return PR_TRUE;
    }
  }

  PRInt32 pageNum;
  PRInt32 curPage;
  PRInt32 endPage;
  mPageSeqFrame->GetCurrentPageNum(&pageNum);

  PRBool donePrinting = PR_FALSE;
  PRBool isDoingPrintRange;
  mPageSeqFrame->IsDoingPrintRange(&isDoingPrintRange);
  if (isDoingPrintRange) {
    PRInt32 fromPage;
    PRInt32 toPage;
    PRInt32 numPages;
    mPageSeqFrame->GetPrintRange(&fromPage, &toPage);
    mPageSeqFrame->GetNumPages(&numPages);
    if (fromPage > numPages) {
      return PR_TRUE;
    }
    if (toPage > numPages) {
      toPage = numPages;
    }

    PR_PL(("****** Printing Page %d printing from %d to page %d\n", pageNum, fromPage, toPage));

    donePrinting = pageNum >= toPage;
    aInRange = pageNum >= fromPage && pageNum <= toPage;
    PRInt32 pageInc = pageNum - fromPage + 1;
    curPage = pageInc >= 0?pageInc+1:0;
    endPage = (toPage - fromPage)+1;
  } else {
    PRInt32 numPages;
    mPageSeqFrame->GetNumPages(&numPages);

    PR_PL(("****** Printing Page %d of %d page(s)\n", pageNum, numPages));

    donePrinting = pageNum >= numPages;
    curPage = pageNum+1;
    endPage = numPages;
    aInRange = PR_TRUE;
  }

  // NOTE: mPrt->mPrintFrameType gets set to  "kFramesAsIs" when a
  // plain document contains IFrames, so we need to override that case here
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    DoProgressForSeparateFrames();

  } else if (mPrt->mPrintFrameType != nsIPrintSettings::kFramesAsIs ||
             mPrt->mPrintObject->mFrameType == eDoc && aPO == mPrt->mPrintObject) {
    PrintData::DoOnProgressChange(mPrt->mPrintProgressListeners, curPage, endPage);
  }

  // Set Clip when Printing "AsIs" or
  // when printing an IFrame for SelectedFrame or EachFrame
  PRBool setClip = PR_FALSE;
  switch (mPrt->mPrintFrameType) {

    case nsIPrintSettings::kFramesAsIs:
      setClip = PR_TRUE;
      break;

    case nsIPrintSettings::kSelectedFrame:
      if (aPO->mPrintAsIs) {
        if (aPO->mFrameType == eIFrame) {
          setClip = aPO != mPrt->mSelectedPO;
        }
      }
      break;

    case nsIPrintSettings::kEachFrameSep:
      if (aPO->mPrintAsIs) {
        if (aPO->mFrameType == eIFrame) {
          setClip = PR_TRUE;
        }
      }
      break;

  } //switch

  if (setClip) {
    // Always set the clip x,y to zero because it isn't going to have any margins
    aPO->mClipRect.x = 0;
    aPO->mClipRect.y = 0;
    mPageSeqFrame->SetClipRect(aPO->mPresContext, &aPO->mClipRect);
  }

  // Print the Page
  // if a print job was cancelled externally, an EndPage or BeginPage may
  // fail and the failure is passed back here.
  // Returning PR_TRUE means we are done printing.
  //
  // When rv == NS_ERROR_ABORT, it means we want out of the
  // print job without displaying any error messages
  nsresult rv = mPageSeqFrame->PrintNextPage(aPresContext);
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_ABORT) {
      ShowPrintErrorDialog(rv);
      mPrt->mIsAborted = PR_TRUE;
    }
    return PR_TRUE;
  }

  // Now see if any of the SubDocs are on this page
  if (aPO->mPrintAsIs) {
    nsIPageSequenceFrame * curPageSeq = mPageSeqFrame;
    aPO->mHasBeenPrinted = PR_TRUE;
    PRInt32 cnt = aPO->mKids.Count();
    for (PRInt32 i=0;i<cnt;i++) {
      PrintObject* po = (PrintObject*)aPO->mKids[i];
      NS_ASSERTION(po, "PrintObject can't be null!");
      if (po->IsPrintable()) {
        // Now verify that SubDoc's PageNum matches the
        // page num of it's parent doc
        curPageSeq->GetCurrentPageNum(&pageNum);
        nsIFrame* fr;
        CallQueryInterface(curPageSeq, &fr);

        if (fr == po->mSeqFrame && pageNum == po->mPageNum) {
          PRBool donePrintingSubDoc;
          DoPrint(po, PR_TRUE, donePrintingSubDoc); // synchronous printing, it changes the value mPageSeqFrame
          po->mHasBeenPrinted = PR_TRUE;
        }
      }
    } // while
    mPageSeqFrame = curPageSeq;

    if (aPO->mParent == nsnull ||
        (aPO->mParent != nsnull && !aPO->mParent->mPrintAsIs && aPO->mPrintAsIs)) {
      mPageSeqFrame->DoPageEnd(aPresContext);
    }

    // XXX this is because PrintAsIs for FrameSets reflows to two pages
    // not sure why, but this needs to be fixed.
    if (aPO->mFrameType == eFrameSet && mPrt->mPrintFrameType == nsIPrintSettings::kFramesAsIs) {
      return PR_TRUE;
    }
  }

  return donePrinting;
}

//-------------------------------------------------------
// Recursively finds a PrintObject that has the aWebShell
PrintObject * DocumentViewerImpl::FindPrintObjectByWS(PrintObject* aPO, nsIWebShell * aWebShell)
{
  NS_ASSERTION(aPO, "Pointer is null!");
  NS_ASSERTION(aWebShell, "Pointer is null!");

  if (aPO->mWebShell == aWebShell) {
    return aPO;
  }
  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i=0;i<cnt;i++) {
    PrintObject* kid = (PrintObject*)aPO->mKids.ElementAt(i);
    NS_ASSERTION(kid, "PrintObject can't be null!");
    PrintObject* po = FindPrintObjectByWS(kid, aWebShell);
    if (po != nsnull) {
      return po;
    }
  }
  return nsnull;
}

//-------------------------------------------------------
// Recursively build a list of of sub documents to be printed
// that mirrors the document tree
void
DocumentViewerImpl::BuildDocTree(nsIDocShellTreeNode * aParentNode,
                                 nsVoidArray *         aDocList,
                                 PrintObject *         aPO)
{
  NS_ASSERTION(aParentNode, "Pointer is null!");
  NS_ASSERTION(aDocList, "Pointer is null!");
  NS_ASSERTION(aPO, "Pointer is null!");

  // Get the Doc and Title String
  GetWebShellTitleAndURL(aPO->mWebShell, &aPO->mDocTitle, &aPO->mDocURL);

  PRInt32 childWebshellCount;
  aParentNode->GetChildCount(&childWebshellCount);
  if (childWebshellCount > 0) {
    for (PRInt32 i=0;i<childWebshellCount;i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));

      nsCOMPtr<nsIPresShell> presShell;
      childAsShell->GetPresShell(getter_AddRefs(presShell));

      if (!presShell) {
        continue;
      }

      nsCOMPtr<nsIContentViewer>  viewer;
      childAsShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIContentViewerFile> viewerFile(do_QueryInterface(viewer));
        if (viewerFile) {
          nsCOMPtr<nsIWebShell> childWebShell(do_QueryInterface(child));
          nsCOMPtr<nsIDocShellTreeNode> childNode(do_QueryInterface(child));
          PrintObject * po = new PrintObject;
          po->mWebShell = childWebShell;
          po->mParent   = aPO;
          aPO->mKids.AppendElement(po);
          aDocList->AppendElement(po);
          BuildDocTree(childNode, aDocList, po);
        }
      }
    }
  }
}

//-------------------------------------------------------
// Recursively sets the PO items to be printed "As Is"
// from the given item down into the tree
void
DocumentViewerImpl::SetPrintAsIs(PrintObject* aPO, PRBool aAsIs)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  aPO->mPrintAsIs = aAsIs;
  for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
    SetPrintAsIs((PrintObject*)aPO->mKids[i], aAsIs);
  }
}

//-------------------------------------------------------
// Recursively sets all the PO items to be printed
// from the given item down into the tree
void
DocumentViewerImpl::SetPrintPO(PrintObject* aPO, PRBool aPrint,
                               PRBool aIsHidden, PRUint32 aFlags)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Set whether to print flag
  // If it is hidden dont' allow ANY changes to the mDontPrint
  // because mDontPrint has already been turned off
  if ((aFlags & eSetPrintFlag) && !aPO->mIsHidden) aPO->mDontPrint = !aPrint;

  // Set hidden flag
  if (aFlags & eSetHiddenFlag) aPO->mIsHidden = aIsHidden;

  for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
    SetPrintPO((PrintObject*)aPO->mKids[i], aPrint, aIsHidden, aFlags);
  } 
}

//-------------------------------------------------------
// Finds the Page Frame and the absolute location on the page
// for a Sub document.
//
// NOTE: This MUST be done after the sub-doc has been laid out
// This is called by "MapSubDocFrameLocations"
//
nsresult
DocumentViewerImpl::CalcPageFrameLocation(nsIPresShell * aPresShell,
                                          PrintObject*   aPO)
{
  NS_ASSERTION(aPresShell, "Pointer is null!");
  NS_ASSERTION(aPO, "Pointer is null!");

  if (aPO != nsnull && aPO->mContent != nsnull) {

    // Find that frame for the sub-doc's content element
    // in the parent document
    // if it comes back null it probably has the style
    // set to "display:none"
    nsIFrame * frame;
    aPresShell->GetPrimaryFrameFor(aPO->mContent, &frame);
    if (frame == nsnull) {
      aPO->mDontPrint = PR_TRUE;
      return NS_OK;
    }

    nsMargin borderPadding(0, 0, 0, 0);
    frame->CalcBorderPadding(borderPadding);

    // Calc absolute position of the frame all the way up
    // to the SimpleSeq frame
    nsRect rect;
    frame->GetRect(rect);
    rect.Deflate(borderPadding);

    rect.x = 0;
    rect.y = 0;
    nsIFrame * parent    = frame;
    nsIFrame * pageFrame = nsnull;
    nsIFrame * seqFrame  = nsnull;
    while (parent != nsnull) {
      nsRect rr;
      parent->GetRect(rr);
      rect.x += rr.x;
      rect.y += rr.y;
      nsIFrame * temp = parent;
      temp->GetParent(&parent);
      // Keep a pointer to the Seq and Page frames
      nsIPageSequenceFrame * sqf = nsnull;
      if (parent != nsnull &&
          NS_SUCCEEDED(CallQueryInterface(parent, &sqf)) && sqf) {
        pageFrame = temp;
        seqFrame  = parent;
      }
    }
    NS_ASSERTION(seqFrame, "The sequencer frame can't be null!");
    NS_ASSERTION(pageFrame, "The page frame can't be null!");
    if (seqFrame == nsnull || pageFrame == nsnull) return NS_ERROR_FAILURE;

    // Remember the Frame location information for later
    aPO->mRect      = rect;
    aPO->mSeqFrame  = seqFrame;
    aPO->mPageFrame = pageFrame;

    // Calc the Page No it is on
    PRInt32 pageNum = 1;
    nsIFrame * child;
    seqFrame->FirstChild(aPO->mPresContext, nsnull, &child);
    while (child != nsnull) {
      if (pageFrame == child) {
        aPO->mPageNum = pageNum;
        break;
      }
      pageNum++;
      child->GetNextSibling(&child);
    } // while
  }
  return NS_OK;
}

//-------------------------------------------------------
// This recusively walks the PO tree calculating the
// the page location and the absolute frame location for
// a sub-doc.
//
// NOTE: This MUST be done after the sub-doc has been laid out
// This is called by "ReflowDocList"
//
nsresult
DocumentViewerImpl::MapSubDocFrameLocations(PrintObject* aPO)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  if (aPO->mParent != nsnull && aPO->mParent->mPresShell) {
    nsresult rv = CalcPageFrameLocation(aPO->mParent->mPresShell, aPO);
    if (NS_FAILED(rv)) return rv;
  }

  if (aPO->mPresShell) {
    for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
      nsresult rv = MapSubDocFrameLocations((PrintObject*)aPO->mKids[i]);
      if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}


//-------------------------------------------------------
// This method is key to the entire print mechanism.
//
// This "maps" or figures out which sub-doc represents a
// given Frame or IFrame in it's parent sub-doc.
//
// So the Mcontent pointer in the child sub-doc points to the
// content in the it's parent document, that caused it to be printed.
// This is used later to (after reflow) to find the absolute location
// of the sub-doc on it's parent's page frame so it can be
// printed in the correct location.
//
// This method recursvely "walks" the content for a document finding
// all the Frames and IFrames, then sets the "mFrameType" data member
// which tells us what type of PO we have
void
DocumentViewerImpl::MapContentForPO(PrintObject*   aRootObject,
                                    nsIPresShell*  aPresShell,
                                    nsIContent*    aContent)
{
  NS_ASSERTION(aRootObject, "Pointer is null!");
  NS_ASSERTION(aPresShell, "Pointer is null!");
  NS_ASSERTION(aContent, "Pointer is null!");

  nsCOMPtr<nsIDocument> doc;
  aContent->GetDocument(*getter_AddRefs(doc));

  if (!doc) {
    NS_ERROR("No document!");

    return;
  }

  nsCOMPtr<nsIDocument> subDoc;
  doc->GetSubDocumentFor(aContent, getter_AddRefs(subDoc));

  if (subDoc) {
    nsCOMPtr<nsISupports> container;
    subDoc->GetContainer(getter_AddRefs(container));

    nsCOMPtr<nsIPresShell> presShell;
    subDoc->GetShellAt(0, getter_AddRefs(presShell));

    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(container));

    if (presShell && webShell) {
      PrintObject * po = FindPrintObjectByWS(aRootObject, webShell);
      NS_ASSERTION(po, "PO can't be null!");

      if (po) {
        po->mContent  = aContent;

        // Now, "type" the PO
        nsCOMPtr<nsIDOMHTMLFrameSetElement> frameSet =
          do_QueryInterface(aContent);

        if (frameSet) {
          po->mFrameType = eFrameSet;
        } else {
          nsCOMPtr<nsIDOMHTMLFrameElement> frame(do_QueryInterface(aContent));
          if (frame) {
            po->mFrameType = eFrame;
          } else {
            nsCOMPtr<nsIDOMHTMLObjectElement> objElement =
              do_QueryInterface(aContent);
            nsCOMPtr<nsIDOMHTMLIFrameElement> iFrame =
              do_QueryInterface(aContent);

            if (iFrame || objElement) {
              po->mFrameType = eIFrame;
              po->mPrintAsIs = PR_TRUE;
              if (po->mParent) {
                po->mParent->mPrintAsIs = PR_TRUE;
              }
            }
          }
        }
      }
    }
  }

  // walk children content
  PRInt32 count;
  aContent->ChildCount(count);
  for (PRInt32 i=0;i<count;i++) {
    nsIContent* child;
    aContent->ChildAt(i, child);
    MapContentForPO(aRootObject, aPresShell, child);
  }
}

//-------------------------------------------------------
// The walks the PO tree and for each document it walks the content
// tree looking for any content that are sub-shells
//
// It then sets the mContent pointer in the "found" PO object back to the
// the document that contained it.
void
DocumentViewerImpl::MapContentToWebShells(PrintObject* aRootPO,
                                          PrintObject* aPO)
{
  NS_ASSERTION(aRootPO, "Pointer is null!");
  NS_ASSERTION(aPO, "Pointer is null!");

  // Recursively walk the content from the root item
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsIContent> rootContent;
  GetPresShellAndRootContent(aPO->mWebShell, getter_AddRefs(presShell), getter_AddRefs(rootContent));
  if (presShell && rootContent) {
    MapContentForPO(aRootPO, presShell, rootContent);
  }

  // Continue recursively walking the chilren of this PO
  for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
    MapContentToWebShells(aRootPO, (PrintObject*)aPO->mKids[i]);
  }

}

//-------------------------------------------------------
// This gets ref counted copies of the PresShell and Root Content
// for a given nsIWebShell
void
DocumentViewerImpl::GetPresShellAndRootContent(nsIWebShell *  aWebShell,
                                               nsIPresShell** aPresShell,
                                               nsIContent**   aContent)
{
  NS_ASSERTION(aWebShell, "Pointer is null!");
  NS_ASSERTION(aPresShell, "Pointer is null!");
  NS_ASSERTION(aContent, "Pointer is null!");

  *aContent   = nsnull;
  *aPresShell = nsnull;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aWebShell));

  nsCOMPtr<nsIPresShell> presShell(getter_AddRefs(GetPresShellFor(docShell)));
  if (!presShell) return;

  nsCOMPtr<nsIDocument> doc;
  presShell->GetDocument(getter_AddRefs(doc));
  if (!doc) return;

  doc->GetRootContent(aContent); // this addrefs
  *aPresShell = presShell.get();
  NS_ADDREF(*aPresShell);
}

//-------------------------------------------------------
// Recursively sets the clip rect on all thchildren
void
DocumentViewerImpl::SetClipRect(PrintObject*  aPO,
                                const nsRect& aClipRect,
                                nscoord       aOffsetX,
                                nscoord       aOffsetY,
                                PRBool        aDoingSetClip)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  nsRect clipRect = aClipRect;
  if (aDoingSetClip) {
    nscoord width  = (aPO->mRect.x+aPO->mRect.width) > aClipRect.width?aClipRect.width-aPO->mRect.x:aPO->mRect.width;
    nscoord height = (aPO->mRect.y+aPO->mRect.height) > aClipRect.height?aClipRect.height-aPO->mRect.y:aPO->mRect.height;
    aPO->mClipRect.SetRect(aPO->mRect.x, aPO->mRect.y, width, height);

  }

  PRBool doClip = aDoingSetClip;

  if (aPO->mFrameType == eFrame) {
    if (aDoingSetClip) {
      aPO->mClipRect.SetRect(aOffsetX, aOffsetY, aPO->mClipRect.width, aPO->mClipRect.height);
      clipRect = aPO->mClipRect;
    } else if (mPrt->mPrintFrameType == nsIPrintSettings::kFramesAsIs) {
      aPO->mClipRect.SetRect(aOffsetX, aOffsetY, aPO->mRect.width, aPO->mRect.height);
      clipRect = aPO->mClipRect;
      doClip = PR_TRUE;
    }

  } else if (aPO->mFrameType == eIFrame) {

    if (aDoingSetClip) {
      aPO->mClipRect.SetRect(aOffsetX, aOffsetY, aPO->mClipRect.width, aPO->mClipRect.height);
      clipRect = aPO->mClipRect;
    } else {

      if (mPrt->mPrintFrameType == nsIPrintSettings::kSelectedFrame) {
        if (aPO->mParent && aPO->mParent == mPrt->mSelectedPO) {
          aPO->mClipRect.SetRect(aOffsetX, aOffsetY, aPO->mRect.width, aPO->mRect.height);
          clipRect = aPO->mClipRect;
          doClip = PR_TRUE;
        }
      } else {
        aPO->mClipRect.SetRect(aOffsetX, aOffsetY, aPO->mRect.width, aPO->mRect.height);
        clipRect = aPO->mClipRect;
        doClip = PR_TRUE;
      }
    }

  }


  PR_PL(("In DV::SetClipRect PO: %p (%9s) ", aPO, gFrameTypesStr[aPO->mFrameType]));
  PR_PL(("%5d,%5d,%5d,%5d\n", aPO->mClipRect.x, aPO->mClipRect.y,aPO->mClipRect.width, aPO->mClipRect.height));

  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i=0;i<cnt;i++) {
    SetClipRect((PrintObject *)aPO->mKids[i], clipRect,
                aOffsetX+aPO->mRect.x, aOffsetY+aPO->mRect.y, doClip);
  }
}

//-------------------------------------------------------
// Recursively reflow each sub-doc and then calc
// all the frame locations of the sub-docs
nsresult
DocumentViewerImpl::ReflowDocList(PrintObject* aPO, PRBool aSetPixelScale, PRBool aDoCalcShrink)
{
  NS_ASSERTION(aPO, "Pointer is null!");
  if (!aPO) return NS_ERROR_FAILURE;

  // Don't reflow hidden POs
  if (aPO->mIsHidden) return NS_OK;

  // Here is where we set the shrinkage value into the DC
  // and this is what actually makes it shrink
  if (aSetPixelScale && aPO->mFrameType != eIFrame) {
    float ratio;
    if (mPrt->mPrintFrameType == nsIPrintSettings::kFramesAsIs || mPrt->mPrintFrameType == nsIPrintSettings::kNoFrames) {
      ratio = mPrt->mShrinkRatio - 0.005f; // round down
    } else {
      ratio = aPO->mShrinkRatio - 0.005f; // round down
    }
    mPrt->mPrintDC->SetCanonicalPixelScale(ratio*mPrt->mOrigDCScale);
  }

  // Reflow the PO
  if (NS_FAILED(ReflowPrintObject(aPO, aDoCalcShrink))) {
    return NS_ERROR_FAILURE;
  }

  // Calc the absolute poistion of the frames
  if (NS_FAILED(MapSubDocFrameLocations(aPO))) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i=0;i<cnt;i++) {
    if (NS_FAILED(ReflowDocList((PrintObject *)aPO->mKids[i], aSetPixelScale, aDoCalcShrink))) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

//-------------------------------------------------------
// Reflow a PrintObject
nsresult
DocumentViewerImpl::ReflowPrintObject(PrintObject * aPO, PRBool aDoCalcShrink)
{
  NS_ASSERTION(aPO, "Pointer is null!");
  if (!aPO) return NS_ERROR_FAILURE;

  // If it is hidden don't bother reflow it or any of it's children
  if (aPO->mIsHidden) return NS_OK;

  // Now locate the nsIDocument for the WebShell
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aPO->mWebShell));
  NS_ASSERTION(docShell, "The DocShell can't be NULL!");

  nsCOMPtr<nsIPresShell>  wsPresShell;
  docShell->GetPresShell(getter_AddRefs(wsPresShell));
  NS_ASSERTION(wsPresShell, "The PresShell can't be NULL!");

  nsCOMPtr<nsIDocument> document;
  wsPresShell->GetDocument(getter_AddRefs(document));

  // create the PresContext
  PRBool containerIsSet = PR_FALSE;
  nsresult rv;
  if (mIsCreatingPrintPreview) {
    nsCOMPtr<nsIPrintPreviewContext> printPreviewCon(do_CreateInstance(kPrintPreviewContextCID, &rv));
    if (NS_FAILED(rv)) {
      return rv;
    }
    aPO->mPresContext = do_QueryInterface(printPreviewCon);
    printPreviewCon->SetPrintSettings(mPrt->mPrintSettings);
  } else {
    nsCOMPtr<nsIPrintContext> printcon(do_CreateInstance(kPrintContextCID, &rv));
    if (NS_FAILED(rv)) {
      return rv;
    }
    aPO->mPresContext = do_QueryInterface(printcon);
    printcon->SetPrintSettings(mPrt->mPrintSettings);
  }


  // set the presentation context to the value in the print settings
  PRBool printBGColors;
  mPrt->mPrintSettings->GetPrintBGColors(&printBGColors);
  aPO->mPresContext->SetBackgroundColorDraw(printBGColors);
  mPrt->mPrintSettings->GetPrintBGImages(&printBGColors);
  aPO->mPresContext->SetBackgroundImageDraw(printBGColors);


  // init it with the DC
  (aPO->mPresContext)->Init(mPrt->mPrintDocDC);

  CreateStyleSet(document, getter_AddRefs(aPO->mStyleSet));

  aPO->mViewManager = do_CreateInstance(kViewManagerCID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aPO->mViewManager->Init(mPrt->mPrintDocDC);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = document->CreateShell(aPO->mPresContext, aPO->mViewManager,
                             aPO->mStyleSet, getter_AddRefs(aPO->mPresShell));
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 pageWidth, pageHeight;
  mPrt->mPrintDocDC->GetDeviceSurfaceDimensions(pageWidth, pageHeight);

  PRInt32 width, height;
  if (aPO->mContent == nsnull || !aPO->mPrintAsIs ||
      (aPO->mPrintAsIs && aPO->mParent && !aPO->mParent->mPrintAsIs) ||
      (aPO->mFrameType == eIFrame && aPO == mPrt->mSelectedPO)) {
    width  = pageWidth;
    height = pageHeight;
  } else {
    width  = aPO->mRect.width;
    height = aPO->mRect.height;
  }

  PR_PL(("In DV::ReflowPrintObject PO: %p (%9s) Setting w,h to %d,%d\n", aPO, gFrameTypesStr[aPO->mFrameType], width, height));

  nsCOMPtr<nsIDOMWindowInternal> domWinIntl =
    do_QueryInterface(mPrt->mPrintDocDW);
  // XXX - Hack Alert
  // OK, so ther eis a selection, we will print the entire selection
  // on one page and then crop the page.
  // This means you can never print any selection that is longer than
  // one page put it keeps it from page breaking in the middle of your
  // print of the selection (see also nsSimplePageSequence.cpp)
  PRInt16 printRangeType = nsIPrintSettings::kRangeAllPages;
  if (mPrt->mPrintSettings != nsnull) {
    mPrt->mPrintSettings->GetPrintRange(&printRangeType);
  }


  if (printRangeType == nsIPrintSettings::kRangeSelection &&
      IsThereARangeSelection(domWinIntl)) {
    height = NS_UNCONSTRAINEDSIZE;
  }

  nsRect tbounds = nsRect(0, 0, width, height);

  // Create a child window of the parent that is our "root view/window"
  rv = CallCreateInstance(kViewCID, &aPO->mRootView);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = (aPO->mRootView)->Init(aPO->mViewManager, tbounds, nsnull);
  if (NS_FAILED(rv)) {
    return rv;
  }

#ifdef NS_PRINT_PREVIEW
  // Here we decide whether we need scrollbars and
  // what the parent will be of the widget
  if (mIsCreatingPrintPreview) {
    PRBool canCreateScrollbars = PR_FALSE;
    nsCOMPtr<nsIWidget> widget = mParentWidget;
    // the top PrintObject's widget will always have scrollbars
    if (aPO->mParent != nsnull && aPO->mContent) {
      nsCOMPtr<nsIFrameManager> frameMan;
      aPO->mParent->mPresShell->GetFrameManager(getter_AddRefs(frameMan));
      NS_ASSERTION(frameMan, "No Frame manager!");
      nsIFrame* frame;
      frameMan->GetPrimaryFrameFor(aPO->mContent, &frame);
      if (frame && (aPO->mFrameType == eIFrame || aPO->mFrameType == eFrame)) {
        frame->FirstChild(aPO->mParent->mPresContext, nsnull, &frame);
      }

      if (frame) {
        nsIView* view = nsnull;
        frame->GetView(aPO->mParent->mPresContext, &view);
        if (view) {
          nsCOMPtr<nsIWidget> w2;
          view->GetWidget(*getter_AddRefs(w2));
          if (w2) {
            widget = w2;
          }
          canCreateScrollbars = PR_FALSE;
        }
      }
    } else {
      canCreateScrollbars = PR_TRUE;
    }
    rv = aPO->mRootView->CreateWidget(kWidgetCID, nsnull, widget->GetNativeData(NS_NATIVE_WIDGET));
    aPO->mRootView->GetWidget(*getter_AddRefs(aPO->mWindow));
    aPO->mPresContext->SetPaginatedScrolling(canCreateScrollbars);
  }
#endif // NS_PRINT_PREVIEW

  nsCompatibility mode;
  mPresContext->GetCompatibilityMode(&mode);

  // Setup hierarchical relationship in view manager
  aPO->mViewManager->SetRootView(aPO->mRootView);
  aPO->mPresShell->Init(document, aPO->mPresContext,
                        aPO->mViewManager, aPO->mStyleSet, mode);

  if (!containerIsSet) {
    nsCOMPtr<nsISupports> supps(do_QueryInterface(aPO->mWebShell));
    aPO->mPresContext->SetContainer(supps);
  }

  // get the old history
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsILayoutHistoryState>  layoutState;
  NS_ENSURE_SUCCESS(GetPresShell(*(getter_AddRefs(presShell))), NS_ERROR_FAILURE);
  presShell->CaptureHistoryState(getter_AddRefs(layoutState), PR_TRUE);

  // set it on the new pres shell
  aPO->mPresShell->SetHistoryState(layoutState);

  // turn off animated GIFs
  if (aPO->mPresContext) {
    aPO->mPresContext->GetImageAnimationMode(&aPO->mImgAnimationMode);
    aPO->mPresContext->SetImageAnimationMode(imgIContainer::kDontAnimMode);
  }

  aPO->mPresShell->BeginObservingDocument();

  nsMargin margin(0,0,0,0);
  mPrt->mPrintSettings->GetMarginInTwips(margin);

  // initialize it with the default/generic case
  nsRect adjRect(aPO->mRect.x != 0?margin.left:0, aPO->mRect.y != 0?margin.top:0, width, height);

  // XXX This is an arbitray height,
  // but reflow somethimes gets upset when using max int
  // basically, we want to reflow a single page that is large
  // enough to fit any atomic object like an IFrame
  const PRInt32 kFivePagesHigh = 5;

  // now, change the value for special cases
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    if (aPO->mFrameType == eFrame) {
      adjRect.SetRect(0, 0, width, height);
    } else if (aPO->mFrameType == eIFrame) {
      height = pageHeight*kFivePagesHigh;
      adjRect.SetRect(aPO->mRect.x != 0?margin.left:0, aPO->mRect.y != 0?margin.top:0, width, height);
    }

  } else if (mPrt->mPrintFrameType == nsIPrintSettings::kSelectedFrame) {
    if (aPO->mFrameType == eFrame) {
      adjRect.SetRect(0, 0, width, height);
    } else if (aPO->mFrameType == eIFrame) {
      if (aPO == mPrt->mSelectedPO) {
        adjRect.x = 0;
        adjRect.y = 0;
      } else {
        height = pageHeight*kFivePagesHigh;
        adjRect.SetRect(aPO->mRect.x != 0?margin.left:0, aPO->mRect.y != 0?margin.top:0, width, height);
      }
    }
  }

  if (!adjRect.width || !adjRect.height) {
    aPO->mDontPrint = PR_TRUE;
    return NS_OK;
  }

  aPO->mPresContext->SetPageDim(&adjRect);
  rv = aPO->mPresShell->InitialReflow(width, height);
  if (NS_SUCCEEDED(rv)) {
    // Transfer Selection Ranges to the new Print PresShell
    nsCOMPtr<nsISelection> selection;
    nsCOMPtr<nsISelection> selectionPS;
    nsresult rvv = GetDocumentSelection(getter_AddRefs(selection), wsPresShell);
    if (NS_SUCCEEDED(rvv) && selection) {
      rvv = GetDocumentSelection(getter_AddRefs(selectionPS), aPO->mPresShell);
      if (NS_SUCCEEDED(rvv) && selectionPS) {
        PRInt32 cnt;
        selection->GetRangeCount(&cnt);
        PRInt32 inx;
        for (inx=0;inx<cnt;inx++) {
          nsCOMPtr<nsIDOMRange> range;
          if (NS_SUCCEEDED(selection->GetRangeAt(inx, getter_AddRefs(range)))) {
            selectionPS->AddRange(range);
          }
        }
      }

      // If we are trying to shrink the contents to fit on the page
      // we must first locate the "pageContent" frame
      // Then we walk the frame tree and look for the "xmost" frame
      // this is the frame where the right-hand side of the frame extends
      // the furthest
      if (mPrt->mShrinkToFit) {
        // First find the seq frame
        nsIFrame* rootFrame;
        aPO->mPresShell->GetRootFrame(&rootFrame);
        NS_ASSERTION(rootFrame, "There has to be a root frame!");
        if (rootFrame) {
          nsIFrame* seqFrame;
          rootFrame->FirstChild(aPO->mPresContext, nsnull, &seqFrame);
          while (seqFrame) {
            nsCOMPtr<nsIAtom> frameType;
            seqFrame->GetFrameType(getter_AddRefs(frameType));
            if (nsLayoutAtoms::sequenceFrame == frameType.get()) {
              break;
            }
            seqFrame->FirstChild(aPO->mPresContext, nsnull, &seqFrame);
          }
          NS_ASSERTION(seqFrame, "There has to be a Sequence frame!");
          if (seqFrame) {
            // Get the first page of all the pages
            nsIFrame* pageFrame;
            seqFrame->FirstChild(aPO->mPresContext, nsnull, &pageFrame);
            NS_ASSERTION(pageFrame, "There has to be a Page Frame!");
            // loop thru all the Page Frames
            nscoord overallMaxWidth  = 0;
            nscoord overMaxRectWidth = 0;
            while (pageFrame) {
              nsIFrame* child;
              // Now get it's first child (for HTML docs it is an area frame)
              // then gets it size which would be the size it is suppose to be to fit
              pageFrame->FirstChild(aPO->mPresContext, nsnull, &child);
              NS_ASSERTION(child, "There has to be a child frame!");

              nsRect rect;
              child->GetRect(rect);
              // Create a RenderingContext and set the PresContext
              // appropriately if we are printing selection
              nsCOMPtr<nsIRenderingContext> rc;
              if (nsIPrintSettings::kRangeSelection == printRangeType) {
                aPO->mPresContext->SetIsRenderingOnlySelection(PR_TRUE);
                mPrt->mPrintDocDC->CreateRenderingContext(*getter_AddRefs(rc));
              }
              // Find the Size of the XMost frame
              // then calc the ratio for shrinkage
              nscoord maxWidth = 0;
              FindXMostFrameSize(aPO->mPresContext, rc, child, 0, 0, maxWidth);
              if (maxWidth > overallMaxWidth) {
                overallMaxWidth  = maxWidth;
                overMaxRectWidth = rect.width;
              }
              pageFrame->GetNextSibling(&pageFrame);
            }  // while
            // Now calc the ratio from the widest frames from all the pages
            float ratio = 1.0f;
            NS_ASSERTION(overallMaxWidth, "Overall Max Width must be bigger than zero");
            if (overallMaxWidth > 0) {
              ratio = float(overMaxRectWidth) / float(overallMaxWidth);
              aPO->mXMost = overallMaxWidth;
              aPO->mShrinkRatio = PR_MIN(ratio, 1.0f);
#ifdef EXTENDED_DEBUG_PRINTING
              printf("PO %p ****** RW: %d MW: %d  xMost %d  width: %d  %10.4f\n", aPO, overMaxRectWidth, overallMaxWidth, aPO->mXMost, overMaxRectWidth, ratio*100.0);
#endif
            }
          }
        }
      }
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
        nsIFrame  *theRootFrame;
        aPO->mPresShell->GetRootFrame(&theRootFrame);
        fprintf(fd, "Title: %s\n", docStr?docStr:"");
        fprintf(fd, "URL:   %s\n", urlStr?urlStr:"");
        fprintf(fd, "--------------- Frames ----------------\n");
        nsCOMPtr<nsIRenderingContext> renderingContext;
        mPrt->mPrintDocDC->CreateRenderingContext(*getter_AddRefs(renderingContext));
        RootFrameList(aPO->mPresContext, fd, 0);
        //DumpFrames(fd, aPO->mPresContext, renderingContext, theRootFrame, 0);
        fprintf(fd, "---------------------------------------\n\n");
        fprintf(fd, "--------------- Views From Root Frame----------------\n");
        nsIView * v;
        theRootFrame->GetView(aPO->mPresContext, &v);
        if (v) {
          v->List(fd);
        } else {
          printf("View is null!\n");
        }
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aPO->mWebShell));
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
  }

  aPO->mPresShell->EndObservingDocument();

  return rv;
}


//-------------------------------------------------------
// Given a DOMWindow it recursively finds the PO object that matches
PrintObject*
DocumentViewerImpl::FindPrintObjectByDOMWin(PrintObject* aPO, nsIDOMWindowInternal * aDOMWin)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  // Often the CurFocused DOMWindow is passed in
  // andit is valid for it to be null, so short circut
  if (aDOMWin == nsnull) {
    return nsnull;
  }

  nsCOMPtr<nsIDOMWindowInternal> domWin(GetDOMWinForWebShell(aPO->mWebShell));
  if (domWin != nsnull && domWin.get() == aDOMWin) {
    return aPO;
  }

  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i=0;i<cnt;i++) {
    PrintObject* po = FindPrintObjectByDOMWin((PrintObject*)aPO->mKids[i], aDOMWin);
    if (po != nsnull) {
      return po;
    }
  }
  return nsnull;
}

//-------------------------------------------------------
// return the DOMWindowInternal for a WebShell
nsIDOMWindowInternal *
DocumentViewerImpl::GetDOMWinForWebShell(nsIWebShell* aWebShell)
{
  NS_ASSERTION(aWebShell, "Pointer is null!");

  nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(aWebShell); 

  nsCOMPtr<nsIDOMWindowInternal> domWinInt(do_QueryInterface(domWin));
  if (!domWinInt) return nsnull;

  nsIDOMWindowInternal * dw = domWinInt.get();
  NS_ADDREF(dw);

  return dw;
}

//-------------------------------------------------------
nsresult
DocumentViewerImpl::EnablePOsForPrinting()
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
  PR_PL(("********* DocumentViewerImpl::EnablePOsForPrinting *********\n"));
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
          PrintObject* po = (PrintObject*)mPrt->mPrintObject->mKids[i];
          NS_ASSERTION(po, "PrintObject can't be null!");
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
        PrintObject * po = FindPrintObjectByDOMWin(mPrt->mPrintObject, mPrt->mCurrentFocusWin);
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
          nsCOMPtr<nsIDOMWindowInternal> domWin = getter_AddRefs(GetDOMWinForWebShell(po->mWebShell));
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
          PrintObject* po = (PrintObject*)mPrt->mPrintDocList->ElementAt(i);
          NS_ASSERTION(po, "PrintObject can't be null!");
          nsCOMPtr<nsIDOMWindowInternal> domWin = getter_AddRefs(GetDOMWinForWebShell(po->mWebShell));
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
      PrintObject * po = FindPrintObjectByDOMWin(mPrt->mPrintObject, mPrt->mCurrentFocusWin);
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
        nsCOMPtr<nsIDOMWindowInternal> domWin = getter_AddRefs(GetDOMWinForWebShell(po->mWebShell));
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
      PrintObject * po = FindPrintObjectByDOMWin(mPrt->mPrintObject, mPrt->mCurrentFocusWin);
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
      PrintObject* po = (PrintObject*)mPrt->mPrintDocList->ElementAt(i);
      NS_ASSERTION(po, "PrintObject can't be null!");
      if (po->mFrameType == eFrameSet) {
        po->mDontPrint = PR_TRUE;
      }
    }
  }

  return NS_OK;
}

//---------------------------------------------------
// Find the Frame in a Frame List that is XMost
void
DocumentViewerImpl::FindXMostFrameInList(nsIPresContext* aPresContext,
                                         nsIRenderingContext* aRC,
                                         nsIAtom*        aList,
                                         nsIFrame*       aFrame,
                                         nscoord         aX,
                                         nscoord         aY,
                                         PRInt32&        aMaxWidth)
{
  nsIFrame * child;
  aFrame->FirstChild(aPresContext, aList, &child);
  while (child) {
    PRBool isVisible = PR_TRUE;
    // If the aRC is nsnull, then we skip the more expensive check and
    // just check visibility
    if (aRC) {
      child->IsVisibleForPainting(aPresContext, *aRC, PR_TRUE, &isVisible);
    } else {
      nsCOMPtr<nsIStyleContext> sc;
      child->GetStyleContext(getter_AddRefs(sc));
      if (sc) {
        const nsStyleVisibility* vis = (const nsStyleVisibility*)sc->GetStyleData(eStyleStruct_Visibility);
        isVisible = vis->IsVisible();
      }
    }

    if (isVisible) {
      nsRect rect;
      child->GetRect(rect);
      rect.x += aX;
      rect.y += aY;
      nscoord xMost = rect.XMost();
      // make sure we have a reasonable value
      NS_ASSERTION(xMost < NS_UNCONSTRAINEDSIZE, "Some frame's size is bad.");
      if (xMost >= NS_UNCONSTRAINEDSIZE) {
        xMost = 0;
      }

#ifdef DEBUG_PRINTING_X // keep this here but leave it turned off
      nsAutoString tmp;
      nsIFrameDebug*  frameDebug;
      if (NS_SUCCEEDED(CallQueryInterface(child, &frameDebug))) {
        frameDebug->GetFrameName(tmp);
      }
      printf("%p - %d,%d,%d,%d %s (%d > %d)\n", child, rect.x, rect.y, rect.width, rect.height, NS_LossyConvertUCS2toASCII(tmp).get(), xMost, aMaxWidth);
#endif

      if (xMost > aMaxWidth) {
        aMaxWidth = xMost;
#ifdef DEBUG_PRINTING_X // keep this here but leave it turned off
        printf("%p - %d %s ", child, aMaxWidth, NS_LossyConvertUCS2toASCII(tmp).get());
        if (aList == nsLayoutAtoms::overflowList) printf(" nsLayoutAtoms::overflowList\n");
        if (aList == nsLayoutAtoms::floaterList) printf(" nsLayoutAtoms::floaterList\n");
        if (aList == nsLayoutAtoms::fixedList) printf(" nsLayoutAtoms::fixedList\n");
        if (aList == nsLayoutAtoms::absoluteList) printf(" nsLayoutAtoms::absoluteList\n");
        if (aList == nsnull) printf(" nsnull\n");
#endif
      }
      FindXMostFrameSize(aPresContext, aRC, child, rect.x, rect.y, aMaxWidth);
    }
    child->GetNextSibling(&child);
  }
}

//---------------------------------------------------
// Find the Frame that is XMost
void
DocumentViewerImpl::FindXMostFrameSize(nsIPresContext* aPresContext,
                                       nsIRenderingContext* aRC,
                                       nsIFrame*      aFrame,
                                       nscoord        aX,
                                       nscoord        aY,
                                       PRInt32&       aMaxWidth)
{
  NS_ASSERTION(aPresContext, "Pointer is null!");
  NS_ASSERTION(aFrame, "Pointer is null!");

  // loop thru named child lists
  nsIAtom* childListName = nsnull;
  PRInt32  childListIndex = 0;
  do {
    FindXMostFrameInList(aPresContext, aRC, childListName, aFrame, aX, aY, aMaxWidth);
    NS_IF_RELEASE(childListName);
    aFrame->GetAdditionalChildListName(childListIndex++, &childListName);
  } while (childListName);

}


//-------------------------------------------------------
// Return the PrintObject with that is XMost (The widest frameset frame) AND
// contains the XMost (widest) layout frame
PrintObject*
DocumentViewerImpl::FindXMostPO()
{
  nscoord xMostForPO   = 0;
  nscoord xMost        = 0;
  PrintObject* xMostPO = nsnull;

  for (PRInt32 i=0;i<mPrt->mPrintDocList->Count();i++) {
    PrintObject* po = (PrintObject*)mPrt->mPrintDocList->ElementAt(i);
    NS_ASSERTION(po, "PrintObject can't be null!");
    if (po->mFrameType != eFrameSet && po->mFrameType != eIFrame) {
      if (po->mRect.XMost() >= xMostForPO) {
        if (po->mRect.XMost() > xMostForPO || (po->mRect.XMost() == xMostForPO && po->mXMost > xMost)) {
          xMostForPO = po->mRect.XMost();
          xMost      = po->mXMost;
          xMostPO    = po;
        }
      }
    }
  }

#ifdef EXTENDED_DEBUG_PRINTING
  if (xMostPO) printf("*PO: %p  Type: %d  XM: %d  XM2: %d  %10.3f\n", xMostPO, xMostPO->mFrameType, xMostPO->mRect.XMost(), xMostPO->mXMost, xMostPO->mShrinkRatio);
#endif
  return xMostPO;
}

//-------------------------------------------------------
nsresult
DocumentViewerImpl::SetupToPrintContent(nsIWebShell*          aParent,
                                        nsIDeviceContext*     aDContext,
                                        nsIDOMWindowInternal* aCurrentFocusedDOMWin)
{

  NS_ENSURE_ARG_POINTER(aParent);
  NS_ENSURE_ARG_POINTER(aDContext);
  // NOTE: aCurrentFocusedDOMWin may be null (which is OK)

  mPrt->mPrintDocDC = aDContext;

  // In this step we figure out which documents should be printed
  // i.e. if we are printing the selection then only enable that PrintObject
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
  // The first tru we do not want to do this, the second time thru we do
  PRBool doSetPixelScale = PR_FALSE;
  PRBool ppIsShrinkToFit = mPrtPreview && mPrtPreview->mShrinkToFit;
  if (ppIsShrinkToFit) {
    mPrt->mShrinkRatio = mPrtPreview->mShrinkRatio;
    doSetPixelScale = PR_TRUE;
  }

  // Here we reflow all the PrintObjects
  if (NS_FAILED(ReflowDocList(mPrt->mPrintObject, doSetPixelScale, mPrt->mShrinkToFit))) {
    return NS_ERROR_FAILURE;
  }

  // Here is where we do the extra reflow for shrinking the content
  // But skip this step if we are in PrintPreview
  if (mPrt->mShrinkToFit && !ppIsShrinkToFit) {
    if (mPrt->mPrintDocList->Count() > 1 && mPrt->mPrintObject->mFrameType == eFrameSet) {
      PrintObject* xMostPO = FindXMostPO();
      NS_ASSERTION(xMostPO, "There must always be an XMost PO!");
      if (xMostPO) {
        // The margin is included in the PO's mRect so we need to subtract it
        nsMargin margin(0,0,0,0);
        mPrt->mPrintSettings->GetMarginInTwips(margin);
        nsRect rect = xMostPO->mRect;
        rect.x -= margin.left;
        // Calc the shrinkage based on the entire content area
        mPrt->mShrinkRatio = float(rect.XMost()) / float(rect.x + xMostPO->mXMost);
      }
    } else {
      // Single document so use the Shrink as calculated for the PO
      mPrt->mShrinkRatio = mPrt->mPrintObject->mShrinkRatio;
    }

    // Only Shrink if we are smaller
    if (mPrt->mShrinkRatio < 0.998f) {
      // Clamp Shrink to Fit to 50%
      mPrt->mShrinkRatio = PR_MAX(mPrt->mShrinkRatio, 0.5f);

      for (PRInt32 i=0;i<mPrt->mPrintDocList->Count();i++) {
        PrintObject* po = (PrintObject*)mPrt->mPrintDocList->ElementAt(i);
        NS_ASSERTION(po, "PrintObject can't be null!");
        // Wipe out the presentation before we reflow
        po->DestroyPresentation();
      }

#if defined(XP_PC) && defined(EXTENDED_DEBUG_PRINTING)
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
      if (NS_FAILED(ReflowDocList(mPrt->mPrintObject, PR_TRUE, PR_FALSE))) {
        return NS_ERROR_FAILURE;
      }
    }

#ifdef PR_LOGGING
    {
      float calcRatio;
      if (mPrt->mPrintDocList->Count() > 1 && mPrt->mPrintObject->mFrameType == eFrameSet) {
        PrintObject* xMostPO = FindXMostPO();
        NS_ASSERTION(xMostPO, "There must always be an XMost PO!");
        if (xMostPO) {
          // The margin is included in the PO's mRect so we need to subtract it
          nsMargin margin(0,0,0,0);
          mPrt->mPrintSettings->GetMarginInTwips(margin);
          nsRect rect = xMostPO->mRect;
          rect.x -= margin.left;
          // Calc the shrinkage based on the entire content area
          calcRatio = float(rect.XMost()) / float(rect.x + xMostPO->mXMost);
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

  // Set up the clipping rectangle for all documents
  // When frames are being printed as part of a frame set and also IFrames,
  // they are reflowed with a very large page height. We need to setup the
  // clipping so they do not rpint over top of anything else
  PR_PL(("SetClipRect-------------------------------------------------------\n"));
  nsRect clipRect(-1,-1,-1, -1);
  SetClipRect(mPrt->mPrintObject, clipRect, 0, 0, PR_FALSE);

  CalcNumPrintableDocsAndPages(mPrt->mNumPrintableDocs, mPrt->mNumPrintablePages);

  PR_PL(("--- Printing %d docs and %d pages\n", mPrt->mNumPrintableDocs, mPrt->mNumPrintablePages));
  DUMP_DOC_TREELAYOUT;

  mPrt->mPrintDocDW = aCurrentFocusedDOMWin;

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
  GetDisplayTitleAndURL(mPrt->mPrintObject, mPrt->mPrintSettings, mPrt->mBrandName, &docTitleStr, &docURLStr, eDocTitleDefURLDoc); 

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

  nsresult rv = NS_OK;
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

  // This will print the webshell document
  // when it completes asynchronously in the DonePrintingPages method
  // it will check to see if there are more webshells to be printed and
  // then PrintDocContent will be called again.

  if (mIsDoingPrinting) {
    PrintDocContent(mPrt->mPrintObject, rv); // ignore return value
  }

  return rv;
}

//-------------------------------------------------------
nsresult
DocumentViewerImpl::DoPrint(PrintObject * aPO, PRBool aDoSyncPrinting, PRBool& aDonePrinting)
{
  NS_ASSERTION(mPrt->mPrintDocList, "Pointer is null!");

  PR_PL(("\n"));
  PR_PL(("**************************** %s ****************************\n", gFrameTypesStr[aPO->mFrameType]));
  PR_PL(("****** In DV::DoPrint   PO: %p aDoSyncPrinting: %s \n", aPO, PRT_YESNO(aDoSyncPrinting)));

  nsIWebShell*    webShell      = aPO->mWebShell.get();
  nsIPresShell*   poPresShell   = aPO->mPresShell;
  nsIPresContext* poPresContext = aPO->mPresContext;
  nsIView*        poRootView    = aPO->mRootView;

  nsCOMPtr<nsIWebShell> webContainer(do_QueryInterface(mContainer));
  NS_ASSERTION(webShell, "The WebShell can't be NULL!");

  if (mPrt->mPrintProgressParams) {
    SetDocAndURLIntoProgress(aPO, mPrt->mPrintProgressParams);
  }

  if (webShell != nsnull) {

    PRInt16 printRangeType = nsIPrintSettings::kRangeAllPages;
    nsresult rv;
    if (mPrt->mPrintSettings != nsnull) {
      mPrt->mPrintSettings->GetPrintRange(&printRangeType);
    }

    // Ask the page sequence frame to print all the pages
    nsIPageSequenceFrame* pageSequence;
    poPresShell->GetPageSequenceFrame(&pageSequence);
    NS_ASSERTION(nsnull != pageSequence, "no page sequence frame");

    // Now, depending how we are printing and what type of doc we are printing
    // we must configure the sequencer correctly.
    // so we are about to be very explicit about the whole process

    PRBool skipPageEjectOnly      = PR_FALSE;
    PRBool skipAllPageAdjustments = PR_FALSE;
    PRBool doOffsetting           = PR_FALSE;
    PRBool doAddInParentsOffset   = PR_TRUE;
    PRBool skipSetTitle           = PR_FALSE;

    if (aPO->mFrameType == eFrame) {
      switch (mPrt->mPrintFrameType) {
      case nsIPrintSettings::kFramesAsIs:
          skipAllPageAdjustments = PR_TRUE;
          doOffsetting           = PR_TRUE;
          break;

        case nsIPrintSettings::kSelectedFrame:
          if (aPO->mKids.Count() > 0) {
            skipPageEjectOnly = PR_TRUE;
          }
          break;

        case nsIPrintSettings::kEachFrameSep:
          if (aPO->mKids.Count() > 0) {
            skipPageEjectOnly = PR_TRUE;
          }
          break;
      } // switch

    } else if (aPO->mFrameType == eIFrame) {
      switch (mPrt->mPrintFrameType) {
        case nsIPrintSettings::kFramesAsIs:
          skipAllPageAdjustments = PR_TRUE;
          doOffsetting           = PR_TRUE;
          skipSetTitle           = PR_TRUE;
          break;

        case nsIPrintSettings::kSelectedFrame:
          if (aPO != mPrt->mSelectedPO) {
            skipAllPageAdjustments = PR_TRUE;
            doOffsetting           = PR_TRUE;
            doAddInParentsOffset   = aPO->mParent != nsnull && aPO->mParent->mFrameType == eIFrame && aPO->mParent != mPrt->mSelectedPO;
            skipSetTitle           = PR_TRUE;
          } else {
            skipPageEjectOnly = aPO->mKids.Count() > 0;
          }
          break;

        case nsIPrintSettings::kEachFrameSep:
          skipAllPageAdjustments = PR_TRUE;
          doOffsetting           = PR_TRUE;
          doAddInParentsOffset   = aPO->mParent != nsnull && aPO->mParent->mFrameType == eIFrame;
          skipSetTitle           = PR_TRUE;
          break;
      } // switch
    } else {
      // FrameSets skip page eject only if printing AsIs
      // Also note, that when printing selection is a single document
      // we do not want to skip page ejects
      skipPageEjectOnly = aPO->mPrintAsIs && printRangeType != nsIPrintSettings::kRangeSelection;
    }

    // That we are all configured,
    // let's set everything up to print
    if (skipPageEjectOnly) {
      pageSequence->SkipPageEnd();
      aPO->mSkippedPageEject = PR_TRUE;

    } else {

      if (skipAllPageAdjustments) {
        pageSequence->SuppressHeadersAndFooters(PR_TRUE);
        pageSequence->SkipPageBegin();
        pageSequence->SkipPageEnd();
        aPO->mSkippedPageEject = PR_TRUE;
      } else {
        aPO->mSkippedPageEject = PR_FALSE;
      }

      if (doOffsetting) {
        nscoord x = 0;
        nscoord y = 0;
        PrintObject * po = aPO;
        while (po != nsnull) {
          //if (mPrt->mPrintFrameType != nsIPrintSettings::kSelectedFrame || po != aPO->mParent) {
          PRBool isParent = po == aPO->mParent;
          if (!isParent || (isParent && doAddInParentsOffset)) {
            x += po->mRect.x;
            y += po->mRect.y;
          }
          po = po->mParent;
        }
        pageSequence->SetOffset(x, y);
      }
    }

    PR_PL(("*** skipPageEjectOnly: %s  skipAllPageAdjustments: %s  doOffsetting: %s  doAddInParentsOffset: %s\n",
                      PRT_YESNO(skipPageEjectOnly), PRT_YESNO(skipAllPageAdjustments),
                      PRT_YESNO(doOffsetting), PRT_YESNO(doAddInParentsOffset)));

    // We are done preparing for printing, so we can turn this off
    mPrt->mPreparingForPrint = PR_FALSE;

    // mPrt->mDebugFilePtr this is onlu non-null when compiled for debugging
    if (nsnull != mPrt->mDebugFilePtr) {
#ifdef NS_DEBUG
      // output the regression test
      nsIFrameDebug* fdbg;
      nsIFrame* root;
      poPresShell->GetRootFrame(&root);

      if (NS_SUCCEEDED(CallQueryInterface(root, &fdbg))) {
        fdbg->DumpRegressionData(poPresContext, mPrt->mDebugFilePtr, 0, PR_TRUE);
      }
      fclose(mPrt->mDebugFilePtr);
#endif
    } else {
      nsIFrame* rootFrame;
      poPresShell->GetRootFrame(&rootFrame);

#ifdef EXTENDED_DEBUG_PRINTING
      if (aPO->IsPrintable()) {
        char * docStr;
        char * urlStr;
        GetDocTitleAndURL(aPO, docStr, urlStr);
        DumpLayoutData(docStr, urlStr, poPresContext, mPrt->mPrintDocDC, rootFrame, webShell);
        if (docStr) nsMemory::Free(docStr);
        if (urlStr) nsMemory::Free(urlStr);
      }
#endif

      if (mPrt->mPrintSettings) {
        PRUnichar * docTitleStr = nsnull;
        PRUnichar * docURLStr   = nsnull;

        if (!skipSetTitle) {
          GetDisplayTitleAndURL(aPO, mPrt->mPrintSettings, mPrt->mBrandName, &docTitleStr, &docURLStr, eDocTitleDefBlank); 
        }

        if (nsIPrintSettings::kRangeSelection == printRangeType) {
          poPresContext->SetIsRenderingOnlySelection(PR_TRUE);
          // temporarily creating rendering context
          // which is needed to dinf the selection frames
          nsCOMPtr<nsIRenderingContext> rc;
          mPrt->mPrintDocDC->CreateRenderingContext(*getter_AddRefs(rc));

          // find the starting and ending page numbers
          // via the selection
          nsIFrame* startFrame;
          nsIFrame* endFrame;
          PRInt32   startPageNum;
          PRInt32   endPageNum;
          nsRect    startRect;
          nsRect    endRect;

          nsCOMPtr<nsISelection> selectionPS;
          nsresult rvv = GetDocumentSelection(getter_AddRefs(selectionPS), poPresShell);
          if (NS_SUCCEEDED(rvv) && selectionPS) {
          }

          rv = GetPageRangeForSelection(poPresShell, poPresContext, *rc, selectionPS, pageSequence,
                                        &startFrame, startPageNum, startRect,
                                        &endFrame, endPageNum, endRect);
          if (NS_SUCCEEDED(rv)) {
            mPrt->mPrintSettings->SetStartPageRange(startPageNum);
            mPrt->mPrintSettings->SetEndPageRange(endPageNum);
            nsMargin margin(0,0,0,0);
            mPrt->mPrintSettings->GetMarginInTwips(margin);

            if (startPageNum == endPageNum) {
              nsIFrame * seqFrame;
              if (NS_FAILED(CallQueryInterface(pageSequence, &seqFrame))) {
                mIsDoingPrinting = PR_FALSE;
                return NS_ERROR_FAILURE;
              }
              nsRect rect(0,0,0,0);
              nsRect areaRect;
              nsIFrame * areaFrame = FindFrameByType(poPresContext, startFrame, nsHTMLAtoms::body, rect, areaRect);
              if (areaFrame) {
                nsRect areaRect;
                areaFrame->GetRect(areaRect);
                startRect.y -= margin.top+areaRect.y;
                endRect.y   -= margin.top;
                areaRect.y -= startRect.y;
                areaRect.x -= margin.left;
                // XXX This is temporary fix for printing more than one page of a selection
                pageSequence->SetSelectionHeight(startRect.y, endRect.y+endRect.height-startRect.y);

                // calc total pages by getting calculating the selection's height
                // and then dividing it by how page content frames will fit.
                nscoord selectionHgt = endRect.y + endRect.height - startRect.y;
                PRInt32 pageWidth, pageHeight;
                mPrt->mPrintDocDC->GetDeviceSurfaceDimensions(pageWidth, pageHeight);
                nsMargin margin(0,0,0,0);
                mPrt->mPrintSettings->GetMarginInTwips(margin);
                pageHeight -= margin.top + margin.bottom;
                PRInt32 totalPages = PRInt32((float(selectionHgt) / float(pageHeight))+0.99);
                pageSequence->SetTotalNumPages(totalPages);
              }
            }
          }
        }

        nsIFrame * seqFrame;
        if (NS_FAILED(CallQueryInterface(pageSequence, &seqFrame))) {
          mIsDoingPrinting = PR_FALSE;
          return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsIPrintPreviewContext> ppContext = do_QueryInterface(poPresContext);
        if (!ppContext) {

          nsRect srect;
          seqFrame->GetRect(srect);

          nsRect r;
          poRootView->GetBounds(r);
          r.x = r.y = 0;
          r.height = srect.height;
          aPO->mViewManager->ResizeView(poRootView, r, PR_FALSE);

          rootFrame->GetRect(r);

          r.height = srect.height;
          rootFrame->SetRect(poPresContext, r);

          mPageSeqFrame = pageSequence;
          mPageSeqFrame->StartPrint(poPresContext, mPrt->mPrintSettings, docTitleStr, docURLStr);

          if (!aDoSyncPrinting) {
            // Get the delay time in between the printing of each page
            // this gives the user more time to press cancel
            PRInt32 printPageDelay = 500;
            mPrt->mPrintSettings->GetPrintPageDelay(&printPageDelay);

            // Schedule Page to Print
            PR_PL(("Scheduling Print of PO: %p (%s) \n", aPO, gFrameTypesStr[aPO->mFrameType]));
            StartPagePrintTimer(poPresContext, mPrt->mPrintSettings, aPO, printPageDelay);
          } else {
            DoProgressForAsIsFrames();
            // Print the page synchronously
            PR_PL(("Async Print of PO: %p (%s) \n", aPO, gFrameTypesStr[aPO->mFrameType]));
            PRBool inRange;
            aDonePrinting = PrintPage(poPresContext, mPrt->mPrintSettings, aPO, inRange);
          }
        } else {
          pageSequence->StartPrint(poPresContext, mPrt->mPrintSettings, docTitleStr, docURLStr);
        }
      } else {
        // not sure what to do here!
        mIsDoingPrinting = PR_FALSE;
        return NS_ERROR_FAILURE;
      }
    }
  } else {
    aPO->mDontPrint = PR_TRUE;
    aDonePrinting = PR_FALSE;
  }

  return NS_OK;
}

//-------------------------------------------------------
// Figure out how many documents and how many total pages we are printing
void
DocumentViewerImpl::CalcNumPrintableDocsAndPages(PRInt32& aNumDocs, PRInt32& aNumPages)
{
  aNumPages = 0;
  // Count the number of printable documents
  // and printable pages
  PRInt32 numOfPrintableDocs = 0;
  PRInt32 i;
  for (i=0;i<mPrt->mPrintDocList->Count();i++) {
    PrintObject* po = (PrintObject*)mPrt->mPrintDocList->ElementAt(i);
    NS_ASSERTION(po, "PrintObject can't be null!");
    if (po->IsPrintable()) {
      if (po->mPresShell &&
          po->mFrameType != eIFrame &&
          po->mFrameType != eFrameSet) {
        nsIPageSequenceFrame* pageSequence;
        po->mPresShell->GetPageSequenceFrame(&pageSequence);
        nsIFrame * seqFrame;
        if (NS_SUCCEEDED(CallQueryInterface(pageSequence, &seqFrame))) {
          nsIFrame* frame;
          seqFrame->FirstChild(po->mPresContext, nsnull, &frame);
          while (frame) {
            aNumPages++;
            frame->GetNextSibling(&frame);
          }
        }
      }

      numOfPrintableDocs++;
    }
  }
}

//-------------------------------------------------------
void
DocumentViewerImpl::DoProgressForAsIsFrames()
{
  // mPrintFrameType is set to kFramesAsIs event though the Doc Type maybe eDoc
  // this is done to make the printing of embedded IFrames easier
  // NOTE: we don't want to advance the progress in that case, it is down elsewhere
  if (mPrt->mPrintFrameType == nsIPrintSettings::kFramesAsIs && mPrt->mPrintObject->mFrameType != eDoc) {
    mPrt->mNumDocsPrinted++;
    PrintData::DoOnProgressChange(mPrt->mPrintProgressListeners, mPrt->mNumDocsPrinted, mPrt->mNumPrintableDocs);
  }
}

//-------------------------------------------------------
void
DocumentViewerImpl::DoProgressForSeparateFrames()
{
  if (mPrt->mPrintFrameType == nsIPrintSettings::kEachFrameSep) {
    mPrt->mNumPagesPrinted++;
    // notify the listener of printed docs
    PrintData::DoOnProgressChange(mPrt->mPrintProgressListeners, mPrt->mNumPagesPrinted+1, mPrt->mNumPrintablePages);
  }
}

//-------------------------------------------------------
// Called for each WebShell that needs to be printed
PRBool
DocumentViewerImpl::PrintDocContent(PrintObject* aPO, nsresult& aStatus)
{
  NS_ASSERTION(aPO, "Pointer is null!");


  if (!aPO->mHasBeenPrinted && aPO->IsPrintable()) {
    PRBool donePrinting = PR_TRUE;
    // donePrinting is only valid when when doing synchronous printing
    aStatus = DoPrint(aPO, PR_FALSE, donePrinting);
    if (donePrinting) {
      return PR_TRUE;
    }
  }

  for (PRInt32 i=0;i<aPO->mKids.Count();i++) {
    PrintObject* po = (PrintObject*)aPO->mKids[i];
    PRBool printed = PrintDocContent(po, aStatus);
    if (printed || NS_FAILED(aStatus)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP
DocumentViewerImpl::SetEnableRendering(PRBool aOn)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  mEnableRendering = aOn;
  if (mViewManager) {
    if (aOn) {
      mViewManager->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
      nsIView* view;
      mViewManager->GetRootView(view);   // views are not refCounted
      if (view) {
        mViewManager->UpdateView(view, NS_VMREFRESH_IMMEDIATE);
      }
    }
    else {
      mViewManager->DisableRefresh();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetSticky(PRBool *aSticky)
{
  *aSticky = mIsSticky;

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetSticky(PRBool aSticky)
{
  mIsSticky = aSticky;

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetEnableRendering(PRBool* aResult)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(nsnull != aResult, "null OUT ptr");
  if (aResult) {
    *aResult = mEnableRendering;
  }
  return NS_OK;
}

void
DocumentViewerImpl::ForceRefresh()
{
  mWindow->Invalidate(PR_TRUE);
}

nsresult
DocumentViewerImpl::CreateStyleSet(nsIDocument* aDocument,
                                   nsIStyleSet** aStyleSet)
{
  // this should eventually get expanded to allow for creating
  // different sets for different media
  nsresult rv;

  if (!mUAStyleSheet) {
    NS_WARNING("unable to load UA style sheet");
  }

  rv = CallCreateInstance(kStyleSetCID, aStyleSet);
  if (NS_OK == rv) {
    PRInt32 index = 0;
    aDocument->GetNumberOfStyleSheets(&index);

    while (0 < index--) {
      nsCOMPtr<nsIStyleSheet> sheet;
      aDocument->GetStyleSheetAt(index, getter_AddRefs(sheet));

      /*
       * GetStyleSheetAt will return all style sheets in the document but
       * we're only interested in the ones that are enabled.
       */

      PRBool styleEnabled;
      sheet->GetEnabled(styleEnabled);

      if (styleEnabled) {
        (*aStyleSet)->AddDocStyleSheet(sheet, aDocument);
      }
    }

    nsCOMPtr<nsIChromeRegistry> chromeRegistry =
      do_GetService("@mozilla.org/chrome/chrome-registry;1");

    if (chromeRegistry) {
      nsCOMPtr<nsISupportsArray> sheets;

      // Now handle the user sheets.
      nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(mContainer));
      PRInt32 shellType;
      docShell->GetItemType(&shellType);
      PRBool isChrome = (shellType == nsIDocShellTreeItem::typeChrome);
      sheets = nsnull;
      chromeRegistry->GetUserSheets(isChrome, getter_AddRefs(sheets));
      if(sheets){
        nsCOMPtr<nsICSSStyleSheet> sheet;
        PRUint32 count;
        sheets->Count(&count);
        for(PRUint32 i=0; i<count; i++) {
          sheets->GetElementAt(i, getter_AddRefs(sheet));
          (*aStyleSet)->AppendUserStyleSheet(sheet);
        }
      }

      // Append chrome sheets (scrollbars + forms).
      nsCOMPtr<nsIDocShell> ds(do_QueryInterface(mContainer));
      chromeRegistry->GetAgentSheets(ds, getter_AddRefs(sheets));
      if(sheets){
        nsCOMPtr<nsICSSStyleSheet> sheet;
        PRUint32 count;
        sheets->Count(&count);
        for(PRUint32 i=0; i<count; i++) {
          sheets->GetElementAt(i, getter_AddRefs(sheet));
          (*aStyleSet)->AppendAgentStyleSheet(sheet);
        }
      }
    }

    if (mUAStyleSheet) {
      (*aStyleSet)->AppendAgentStyleSheet(mUAStyleSheet);
    }
  }
  return NS_OK;
}

nsresult
DocumentViewerImpl::MakeWindow(nsIWidget* aParentWidget,
                               const nsRect& aBounds)
{
  nsresult rv;

  mViewManager = do_CreateInstance(kViewManagerCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDeviceContext> dx;
  mPresContext->GetDeviceContext(getter_AddRefs(dx));


  nsRect tbounds = aBounds;
  float p2t;
  mPresContext->GetPixelsToTwips(&p2t);
  tbounds *= p2t;

   // Initialize the view manager with an offset. This allows the viewmanager
   // to manage a coordinate space offset from (0,0)
  rv = mViewManager->Init(dx);
  if (NS_FAILED(rv))
    return rv;
  rv = mViewManager->SetWindowOffset(tbounds.x, tbounds.y);
  if (NS_FAILED(rv))
    return rv;

  // Reset the bounds offset so the root view is set to 0,0. The
  // offset is specified in nsIViewManager::Init above.
  // Besides, layout will reset the root view to (0,0) during reflow,
  // so changing it to 0,0 eliminates placing the root view in the
  // wrong place initially.
  tbounds.x = 0;
  tbounds.y = 0;

  // Create a child window of the parent that is our "root view/window"
  // Create a view

  nsIView *view = nsnull;
  rv = CallCreateInstance(kViewCID, &view);
  if (NS_FAILED(rv))
    return rv;

  // if aParentWidget has a view, we'll hook our view manager up to its view tree
  void* clientData;
  nsIView* containerView = nsnull;
  if (NS_SUCCEEDED(aParentWidget->GetClientData(clientData))) {
    nsISupports* data = (nsISupports*)clientData;

    if (data) {
      data->QueryInterface(NS_GET_IID(nsIView), (void **)&containerView);
    }
  }

  if (containerView) {
    // see if the containerView has already been hooked into a foreign view manager hierarchy
    // if it has, then we have to hook into the hierarchy too otherwise bad things will happen.
    nsCOMPtr<nsIViewManager> containerVM;
    containerView->GetViewManager(*getter_AddRefs(containerVM));
    nsCOMPtr<nsIViewManager> checkVM;
    nsIView* pView = containerView;
    do {
      pView->GetParent(pView);
    } while (pView != nsnull
             && NS_SUCCEEDED(pView->GetViewManager(*getter_AddRefs(checkVM))) && checkVM == containerVM);

    if (!pView) {
      // OK, so the container is not already hooked up into a foreign view manager hierarchy.
      // That means we can choose not to hook ourselves up.
      //
      // If the parent container is a chrome shell, or a frameset, then we won't hook into its view
      // tree. This will improve performance a little bit (especially given scrolling/painting perf bugs)
      // but is really just for peace of mind. This check can be removed if we want to support fancy
      // chrome effects like transparent controls floating over content, transparent Web browsers, and
      // things like that, and the perf bugs are fixed.
      nsCOMPtr<nsIDocShellTreeItem> container(do_QueryInterface(mContainer));
      nsCOMPtr<nsIDocShellTreeItem> parentContainer;
      PRInt32 itemType;
      if (nsnull == container
          || NS_FAILED(container->GetParent(getter_AddRefs(parentContainer)))
          || nsnull == parentContainer
          || NS_FAILED(parentContainer->GetItemType(&itemType))
          || itemType != nsIDocShellTreeItem::typeContent) {
        containerView = nsnull;
      } else {
        nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(parentContainer));
        if (nsnull == webShell || IsWebShellAFrameSet(webShell)) {
          containerView = nsnull;
        }
      }
    }
  }

  rv = view->Init(mViewManager, tbounds, containerView);
  if (NS_FAILED(rv))
    return rv;

  // pass in a native widget to be the parent widget ONLY if the view hierarchy will stand alone.
  // otherwise the view will find its own parent widget and "do the right thing" to
  // establish a parent/child widget relationship
  rv = view->CreateWidget(kWidgetCID, nsnull,
                          containerView != nsnull ? nsnull : aParentWidget->GetNativeData(NS_NATIVE_WIDGET),
                          PR_TRUE, PR_FALSE);
  if (rv != NS_OK)
    return rv;

  // Setup hierarchical relationship in view manager
  mViewManager->SetRootView(view);

  view->GetWidget(*getter_AddRefs(mWindow));

  // This SetFocus is necessary so the Arrow Key and Page Key events
  // go to the scrolled view as soon as the Window is created instead of going to
  // the browser window (this enables keyboard scrolling of the document)
  // mWindow->SetFocus();

  return rv;
}

nsresult DocumentViewerImpl::GetDocumentSelection(nsISelection **aSelection,
                                                  nsIPresShell *aPresShell)
{
  if (!aPresShell) {
    if (!mPresShell) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    aPresShell = mPresShell;
  }
  if (!aSelection)
    return NS_ERROR_NULL_POINTER;
  if (!aPresShell)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsISelectionController> selcon;
  selcon = do_QueryInterface(aPresShell);
  if (selcon)
    return selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                aSelection);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DocumentViewerImpl::CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                              nsIDocumentViewer*& aResult)
{
  if (!mDocument) {
    // XXX better error
    return NS_ERROR_NULL_POINTER;
  }
  if (!aPresContext) {
    return NS_ERROR_NULL_POINTER;
  }

  // Create new viewer
  DocumentViewerImpl* viewer = new DocumentViewerImpl(aPresContext);
  if (!viewer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(viewer);

  // XXX make sure the ua style sheet is used (for now; need to be
  // able to specify an alternate)
  viewer->SetUAStyleSheet(mUAStyleSheet);

  // Bind the new viewer to the old document
  nsresult rv = viewer->LoadStart(mDocument);

  aResult = viewer;

  return rv;
}

nsresult DocumentViewerImpl::DocumentReadyForPrinting()
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIWebShell> webContainer;

  webContainer = do_QueryInterface(mContainer);
  if(webContainer) {
    //
    // Send the document to the printer...
    //
    rv = SetupToPrintContent(webContainer, mPrt->mPrintDC, mPrt->mCurrentFocusWin);
    if (NS_FAILED(rv)) {
      // The print job was canceled or there was a problem
      // So remove all other documents from the print list
      DonePrintingPages(nsnull);
    }
  }
  return rv;
}

#ifdef XP_MAC
#pragma mark -
#endif

/* ========================================================================================
 * nsIContentViewerEdit
 * ======================================================================================== */

NS_IMETHODIMP DocumentViewerImpl::Search()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetSearchable(PRBool *aSearchable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::ClearSelection()
{
  nsresult rv;
  nsCOMPtr<nsISelection> selection;

  rv = GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  return selection->CollapseToStart();
}

NS_IMETHODIMP DocumentViewerImpl::SelectAll()
{
  // XXX this is a temporary implementation copied from nsWebShell
  // for now. I think nsDocument and friends should have some helper
  // functions to make this easier.
  nsCOMPtr<nsISelection> selection;
  nsresult rv;
  rv = GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMHTMLDocument> htmldoc = do_QueryInterface(mDocument);
  nsCOMPtr<nsIDOMNode> bodyNode;

  if (htmldoc)
  {
    nsCOMPtr<nsIDOMHTMLElement>bodyElement;
    rv = htmldoc->GetBody(getter_AddRefs(bodyElement));
    if (NS_FAILED(rv) || !bodyElement) return rv;

    bodyNode = do_QueryInterface(bodyElement);
  }
  else if (mDocument)
  {
    nsCOMPtr<nsIContent> rootContent;
    mDocument->GetRootContent(getter_AddRefs(rootContent));
    bodyNode = do_QueryInterface(rootContent);
  }
  if (!bodyNode) return NS_ERROR_FAILURE;

  rv = selection->RemoveAllRanges();
  if (NS_FAILED(rv)) return rv;

  rv = selection->SelectAllChildren(bodyNode);
  return rv;
}

NS_IMETHODIMP DocumentViewerImpl::CopySelection()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  return mPresShell->DoCopy();
}

NS_IMETHODIMP DocumentViewerImpl::CopyLinkLocation()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupLinkNode(getter_AddRefs(node));
  // make noise if we're not in a link
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);
  return mPresShell->DoCopyLinkLocation(node);
}

NS_IMETHODIMP DocumentViewerImpl::CopyImageLocation()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupImageNode(getter_AddRefs(node));
  // make noise if we're not in an image
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);
  return mPresShell->DoCopyImageLocation(node);
}

NS_IMETHODIMP DocumentViewerImpl::CopyImageContents()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupImageNode(getter_AddRefs(node));
  // make noise if we're not in an image
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);
  return mPresShell->DoCopyImageContents(node);
}

NS_IMETHODIMP DocumentViewerImpl::GetCopyable(PRBool *aCopyable)
{
  nsCOMPtr<nsISelection> selection;
  nsresult rv;
  rv = GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);

  *aCopyable = !isCollapsed;
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::CutSelection()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetCutable(PRBool *aCutable)
{
  *aCutable = PR_FALSE;  // mm, will this ever be called for an editable document?
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::Paste()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetPasteable(PRBool *aPasteable)
{
  *aPasteable = PR_FALSE;
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

static NS_DEFINE_IID(kDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);

nsresult DocumentViewerImpl::GetSelectionDocument(nsIDeviceContextSpec * aDevSpec, nsIDocument ** aNewDoc)
{
  //NS_ENSURE_ARG_POINTER(*aDevSpec);
  NS_ENSURE_ARG_POINTER(aNewDoc);

    // create document
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = NS_NewHTMLDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv)) { return rv; }
  if (!doc) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsINodeInfoManager> nimgr;
  rv = doc->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nimgr->GetNodeInfo(nsHTMLAtoms::html, nsnull, kNameSpaceID_None,
                     *getter_AddRefs(nodeInfo));

  // create document content
  nsCOMPtr<nsIHTMLContent> htmlElement;
  nsCOMPtr<nsIHTMLContent> headElement;
  nsCOMPtr<nsIHTMLContent> bodyElement;
    // create the root
  rv = NS_NewHTMLHtmlElement(getter_AddRefs(htmlElement), nodeInfo);
  if (NS_FAILED(rv)) { return rv; }
  if (!htmlElement) { return NS_ERROR_NULL_POINTER; }
    // create the head

  nimgr->GetNodeInfo(NS_LITERAL_STRING("head"), nsnull,
                     kNameSpaceID_None, *getter_AddRefs(nodeInfo));

  rv = NS_NewHTMLHeadElement(getter_AddRefs(headElement), nodeInfo);
  if (NS_FAILED(rv)) { return rv; }
  if (!headElement) { return NS_ERROR_NULL_POINTER; }
  headElement->SetDocument(doc, PR_FALSE, PR_TRUE);
    // create the body

  nimgr->GetNodeInfo(nsHTMLAtoms::body, nsnull, kNameSpaceID_None,
                    *getter_AddRefs(nodeInfo));

  rv = NS_NewHTMLBodyElement(getter_AddRefs(bodyElement), nodeInfo);
  if (NS_FAILED(rv)) { return rv; }
  if (!bodyElement) { return NS_ERROR_NULL_POINTER; }
  bodyElement->SetDocument(doc, PR_FALSE, PR_TRUE);
    // put the head and body into the root
  rv = htmlElement->AppendChildTo(headElement, PR_FALSE, PR_FALSE);
  if (NS_FAILED(rv)) { return rv; }
  rv = htmlElement->AppendChildTo(bodyElement, PR_FALSE, PR_FALSE);
  if (NS_FAILED(rv)) { return rv; }

  // load the document into the docshell
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
  if (!domDoc) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMElement> htmlDOMElement = do_QueryInterface(htmlElement);
  if (!htmlDOMElement) { return NS_ERROR_NULL_POINTER; }

  //nsCOMPtr<nsIContent> rootContent(do_QueryInterface(htmlElement));
  //doc->SetRootContent(rootContent);

  *aNewDoc = doc.get();
  NS_ADDREF(*aNewDoc);

  return rv;

}

//------------------------------------------------------
PRBool
DocumentViewerImpl::IsThereAnIFrameSelected(nsIWebShell*           aWebShell,
                                            nsIDOMWindowInternal * aDOMWin,
                                            PRPackedBool&          aIsParentFrameSet)
{
  aIsParentFrameSet = IsParentAFrameSet(aWebShell);
  PRBool iFrameIsSelected = PR_FALSE;
  if (mPrt && mPrt->mPrintObject) {
    PrintObject* po = FindPrintObjectByDOMWin(mPrt->mPrintObject, aDOMWin);
    iFrameIsSelected = po && po->mFrameType == eIFrame;
  } else {
    // First, check to see if we are a frameset
    if (!aIsParentFrameSet) {
      // Check to see if there is a currenlt focused frame
      // if so, it means the selected frame is either the main webshell
      // or an IFRAME
      if (aDOMWin != nsnull) {
        // Get the main webshell's DOMWin to see if it matches 
        // the frame that is selected
        nsCOMPtr<nsIDOMWindowInternal> domWin = getter_AddRefs(GetDOMWinForWebShell(aWebShell));
        if (aDOMWin != nsnull && domWin != aDOMWin) {
          iFrameIsSelected = PR_TRUE; // we have a selected IFRAME
        }
      }
    }
  }

  return iFrameIsSelected;
}


//------------------------------------------------------
PRBool
DocumentViewerImpl::IsThereARangeSelection(nsIDOMWindowInternal * aDOMWin)
{
  nsCOMPtr<nsIPresShell> presShell;
  if (aDOMWin != nsnull) {
    nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(aDOMWin));
    nsCOMPtr<nsIDocShell> docShell;
    scriptObj->GetDocShell(getter_AddRefs(docShell));
    docShell->GetPresShell(getter_AddRefs(presShell));
  }

  // check here to see if there is a range selection
  // so we know whether to turn on the "Selection" radio button
  nsCOMPtr<nsISelection> selection;
  GetDocumentSelection(getter_AddRefs(selection), presShell);
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
  }
  return PR_FALSE;
}

#ifdef NS_PRINT_PREVIEW
//-------------------------------------------------------
// Recursively walks the PrintObject tree and installs the DocViewer
// as an event processor and it shows the window
nsresult
DocumentViewerImpl::ShowDocList(PrintObject* aPO, PRBool aShow)
{
  NS_ASSERTION(aPO, "Pointer is null!");

  if (aPO->IsPrintable()) {
    PRBool donePrinting;
    DoPrint(aPO, PR_FALSE, donePrinting);

    // mWindow will be null for POs that are hidden, so they don't get
    // shown
    if (aPO->mWindow) {
      aPO->mWindow->Show(aShow);
    }
  }

  PRInt32 cnt = aPO->mKids.Count();
  for (PRInt32 i=0;i<cnt;i++) {
    if (NS_FAILED(ShowDocList((PrintObject *)aPO->mKids[i], aShow))) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}


void
DocumentViewerImpl::TurnScriptingOn(PRBool aDoTurnOn)
{
  NS_ASSERTION(mDocument, "We MUST have a document.");

  // get the script global object
  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObj;
  nsresult rv = mDocument->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObj));
  NS_ASSERTION(NS_SUCCEEDED(rv) && scriptGlobalObj, "Can't get nsIScriptGlobalObject");
  nsCOMPtr<nsIScriptContext> scx;
  rv = scriptGlobalObj->GetContext(getter_AddRefs(scx));
  NS_ASSERTION(NS_SUCCEEDED(rv) && scx, "Can't get nsIScriptContext");
  scx->SetScriptsEnabled(aDoTurnOn, PR_TRUE);
}

//-------------------------------------------------------
// Install our event listeners on the document to prevent 
// some events from being processed while in PrintPreview 
//
// No return code - if this fails, there isn't much we can do
void
DocumentViewerImpl::InstallPrintPreviewListener()
{
  if (!mPrt->mPPEventListeners) {
    nsCOMPtr<nsIDOMEventReceiver> evRec (do_QueryInterface(mDocument));
    mPrt->mPPEventListeners = new nsPrintPreviewListener(evRec);

    if (mPrt->mPPEventListeners) {
      mPrt->mPPEventListeners->AddListeners();
    }
  }
}

//----------------------------------------------------------------------
static nsresult GetSeqFrameAndCountPages(PrintObject*  aPO,
                                         nsIFrame*&    aSeqFrame,
                                         PRInt32&      aCount)
{
  NS_ENSURE_ARG_POINTER(aPO);

  // Finds the SimplePageSequencer frame
  // in PP mPrtPreview->mPrintObject->mSeqFrame is null
  nsIFrame* curFrame;
  aSeqFrame  = nsnull;
  aPO->mPresShell->GetRootFrame(&curFrame);
  while (curFrame != nsnull) {
    nsIPageSequenceFrame * sqf = nsnull;
    if (NS_SUCCEEDED(CallQueryInterface(curFrame, &sqf)) && sqf) {
      aSeqFrame = curFrame;
      break;
    }
    curFrame->FirstChild(aPO->mPresContext, nsnull, &curFrame);
  }
  if (aSeqFrame == nsnull) return NS_ERROR_FAILURE;

  // first count the total number of pages
  aCount = 0;
  nsIFrame * pageFrame;
  aSeqFrame->FirstChild(aPO->mPresContext, nsnull, &pageFrame);
  while (pageFrame != nsnull) {
    aCount++;
    pageFrame->GetNextSibling(&pageFrame);
  }

  return NS_OK;

}

//----------------------------------------------------------------------
NS_IMETHODIMP
DocumentViewerImpl::PrintPreviewNavigate(PRInt16 aType, PRInt32 aPageNum)
{
#ifdef NS_PRINT_PREVIEW
  if (mIsDoingPrinting) return NS_ERROR_FAILURE;

  if (!mPrtPreview) return NS_ERROR_FAILURE;

  nsIScrollableView* scrollableView;
  mViewManager->GetRootScrollableView(&scrollableView);
  if (scrollableView == nsnull) return NS_OK;

  // Check to see if we can short circut scrolling to the top
  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_HOME ||
      (aType == nsIWebBrowserPrint::PRINTPREVIEW_GOTO_PAGENUM && aPageNum == 1)) {
    scrollableView->ScrollTo(0, 0, PR_TRUE);
    return NS_OK;
  }

  // Finds the SimplePageSequencer frame
  // in PP mPrtPreview->mPrintObject->mSeqFrame is null
  nsIFrame* seqFrame  = nsnull;
  PRInt32   pageCount = 0;
  if (NS_FAILED(GetSeqFrameAndCountPages(mPrtPreview->mPrintObject, seqFrame, pageCount))) {
    return NS_ERROR_FAILURE;
  }

  // Figure where we are currently scrolled to
  const nsIView * clippedView;
  scrollableView->GetClipView(&clippedView);
  nscoord x;
  nscoord y;
  scrollableView->GetScrollPosition(x, y);

  PRInt32    pageNum = 1;
  nsIFrame * fndPageFrame  = nsnull;
  nsIFrame * currentPage   = nsnull;

  // If it is "End" then just do a "goto" to the last page
  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_END) {
    aType    = nsIWebBrowserPrint::PRINTPREVIEW_GOTO_PAGENUM;
    aPageNum = pageCount;
  }

  // Now, locate the current page we are on and
  // and the page of the page number
  nscoord gap = 0;
  nsIFrame * pageFrame;
  seqFrame->FirstChild(mPresContext, nsnull, &pageFrame);
  while (pageFrame != nsnull) {
    nsRect pageRect;
    pageFrame->GetRect(pageRect);
    if (pageNum == 1) {
      gap = pageRect.y;
    }
    pageRect.y -= gap;
    if (pageRect.Contains(pageRect.x, y)) {
      currentPage = pageFrame;
    }
    if (pageNum == aPageNum) {
      fndPageFrame = pageFrame;
      break;
    }
    pageNum++;
    pageFrame->GetNextSibling(&pageFrame);
  }

  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_PREV_PAGE) {
    if (currentPage) {
      currentPage->GetPrevInFlow(&fndPageFrame);
      if (!fndPageFrame) {
        return NS_OK;
      }
    } else {
      return NS_OK;
    }
  } else if (aType == nsIWebBrowserPrint::PRINTPREVIEW_NEXT_PAGE) {
    if (currentPage) {
      currentPage->GetNextInFlow(&fndPageFrame);
      if (!fndPageFrame) {
        return NS_OK;
      }
    } else {
      return NS_OK;
    }
  } else { // If we get here we are doing "GoTo"
    if (aPageNum < 0 || aPageNum > pageCount) {
      return NS_OK;
    }
  }

  if (fndPageFrame && scrollableView) {
    // get the child rect
    nsRect fRect;
    fndPageFrame->GetRect(fRect);
    // find offset from view
    nsPoint pnt;
    nsIView * view;
    fndPageFrame->GetOffsetFromView(mPresContext, pnt, &view);

    nscoord deadSpaceGap = 0;
    nsIPageSequenceFrame * sqf;
    if (NS_SUCCEEDED(CallQueryInterface(seqFrame, &sqf))) {
      sqf->GetDeadSpaceValue(&deadSpaceGap);
    }

    // scroll so that top of page (plus the gray area) is at the top of the scroll area
    scrollableView->ScrollTo(0, fRect.y-deadSpaceGap, PR_TRUE);
  }
#endif // NS_PRINT_PREVIEW

  return NS_OK;
}

/* readonly attribute boolean isFramesetDocument; */
NS_IMETHODIMP
DocumentViewerImpl::GetIsFramesetDocument(PRBool *aIsFramesetDocument)
{
  nsCOMPtr<nsIWebShell> webContainer(do_QueryInterface(mContainer));
  *aIsFramesetDocument = IsParentAFrameSet(webContainer);
  return NS_OK;
}

/* readonly attribute boolean isIFrameSelected; */
NS_IMETHODIMP 
DocumentViewerImpl::GetIsIFrameSelected(PRBool *aIsIFrameSelected)
{
  *aIsIFrameSelected = PR_FALSE;

  // Get the webshell for this documentviewer
  nsCOMPtr<nsIWebShell> webContainer(do_QueryInterface(mContainer));
  // Get the currently focused window
  nsCOMPtr<nsIDOMWindowInternal> currentFocusWin = getter_AddRefs(FindFocusedDOMWindowInternal());
  if (currentFocusWin && webContainer) {
    // Get whether the doc contains a frameset 
    // Also, check to see if the currently focus webshell 
    // is a child of this webshell
    PRPackedBool isParentFrameSet;
    *aIsIFrameSelected = IsThereAnIFrameSelected(webContainer, currentFocusWin, isParentFrameSet);
  }
  return NS_OK;
}

/* readonly attribute boolean isRangeSelection; */
NS_IMETHODIMP 
DocumentViewerImpl::GetIsRangeSelection(PRBool *aIsRangeSelection)
{
  // Get the currently focused window 
  nsCOMPtr<nsIDOMWindowInternal> currentFocusWin = getter_AddRefs(FindFocusedDOMWindowInternal());
  *aIsRangeSelection = IsThereARangeSelection(currentFocusWin);
  return NS_OK;
}

/* readonly attribute boolean isFramesetFrameSelected; */
NS_IMETHODIMP 
DocumentViewerImpl::GetIsFramesetFrameSelected(PRBool *aIsFramesetFrameSelected)
{
  // Get the currently focused window 
  nsCOMPtr<nsIDOMWindowInternal> currentFocusWin = getter_AddRefs(FindFocusedDOMWindowInternal());
  *aIsFramesetFrameSelected = currentFocusWin != nsnull;
  return NS_OK;
}

/* readonly attribute long printPreviewNumPages; */
NS_IMETHODIMP
DocumentViewerImpl::GetPrintPreviewNumPages(PRInt32 *aPrintPreviewNumPages)
{
  NS_ENSURE_ARG_POINTER(aPrintPreviewNumPages);

  // Finds the SimplePageSequencer frame
  // in PP mPrtPreview->mPrintObject->mSeqFrame is null
  nsIFrame* seqFrame  = nsnull;
  *aPrintPreviewNumPages = 0;
  if (!mPrtPreview ||
      NS_FAILED(GetSeqFrameAndCountPages(mPrtPreview->mPrintObject, seqFrame, *aPrintPreviewNumPages))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* void exitPrintPreview (); */
NS_IMETHODIMP
DocumentViewerImpl::ExitPrintPreview()
{
  if (mIsDoingPrinting) return NS_ERROR_FAILURE;

  if (mIsDoingPrintPreview) {
    ReturnToGalleyPresentation();
  }
  return NS_OK;
}


void
DocumentViewerImpl::InstallNewPresentation()
{
  // Get the current size of what is being viewed
  nsRect area;
  mPresContext->GetVisibleArea(area);

  nsRect bounds;
  mWindow->GetBounds(bounds);

  // In case we have focus focus the parent DocShell
  // which in this case should always be chrome
  nsCOMPtr<nsIDocShellTreeItem>  dstParentItem;
  nsCOMPtr<nsIDocShellTreeItem>  dstItem(do_QueryInterface(mContainer));
  if (dstItem) {
    dstItem->GetParent(getter_AddRefs(dstParentItem));
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(dstParentItem));
    if (docShell) {
      docShell->SetHasFocus(PR_TRUE);
    }
  }

  // turn off selection painting
  nsCOMPtr<nsISelectionController> selectionController =
    do_QueryInterface(mPresShell);
  if (selectionController) {
    selectionController->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
  }

  // Start to kill off the old Presentation
  // by cleaning up the PresShell
  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();
    nsCOMPtr<nsISelection> selection;
    nsresult rv = GetDocumentSelection(getter_AddRefs(selection));
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
    if (NS_SUCCEEDED(rv) && selPrivate && mSelectionListener)
      selPrivate->RemoveSelectionListener(mSelectionListener);

    // We need to destroy the PreShell if there is an existing PP
    // or we are not caching the original Presentation
    if (!mPrt->IsCachingPres() || mOldPrtPreview) {
      mPresShell->Destroy();
    }
  }

  // clear weak references before we go away
  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
  }

  // See if we are suppose to be caching the old Presentation
  // and then check to see if we already have.
  if (mPrt->IsCachingPres() && !mPrt->HasCachedPres()) {
    NS_ASSERTION(!mPrt->mCachedPresObj, "Should be null!");
    // Cach old presentation
    mPrt->mCachedPresObj = new CachedPresentationObj(mPresShell, mPresContext, mViewManager, mWindow);
    mWindow->Show(PR_FALSE);
  } else {
    // Destroy the old Presentation
    mPresShell    = nsnull;
    mPresContext  = nsnull;
    mViewManager  = nsnull;
    mWindow       = nsnull;
  }

  // Default to the main Print Object
  PrintObject * prtObjToDisplay = mPrt->mPrintObject;

  // This is the new code for selecting the appropriate Frame of a Frameset
  // for Print Preview. But it can't be turned on yet
#if 0
  // If it is a Frameset then choose the selected one
  // or select the one with the largest area
  if (mPrt->mPrintObject->mFrameType == eFrameSet) {
    if (mPrt->mCurrentFocusWin) {
      PRInt32 cnt = mPrt->mPrintObject->mKids.Count();
      // Start at "1" and skip the FrameSet document itself
      for (PRInt32 i=1;i<cnt;i++) {
        PrintObject* po = (PrintObject *)mPrt->mPrintObject->mKids[i];
        nsCOMPtr<nsIDOMWindowInternal> domWin(getter_AddRefs(GetDOMWinForWebShell(po->mWebShell)));
        if (domWin.get() == mPrt->mCurrentFocusWin.get()) {
          prtObjToDisplay = po;
          break;
        }
      }
    } else {
      PrintObject* largestPO = nsnull;
      nscoord area = 0;
      PRInt32 cnt = mPrt->mPrintObject->mKids.Count();
      // Start at "1" and skip the FrameSet document itself
      for (PRInt32 i=1;i<cnt;i++) {
        PrintObject* po = (PrintObject *)mPrt->mPrintObject->mKids[i];
        nsCOMPtr<nsIDOMWindowInternal> domWin(getter_AddRefs(GetDOMWinForWebShell(po->mWebShell)));
        if (domWin.get() == mPrt->mCurrentFocusWin.get()) {
          nscoord width;
          nscoord height;
          domWin->GetInnerWidth(&width);
          domWin->GetInnerHeight(&height);
          nscoord newArea = width * height;
          if (newArea > area) {
            largestPO = po;
            area = newArea;
          }
        }
      }
      // make sure we got one
      if (largestPO) {
        prtObjToDisplay = largestPO;
      }
    }
  }
#endif

  InstallPrintPreviewListener();

  // Install the new Presentation
  mPresShell    = prtObjToDisplay->mPresShell;
  mPresContext  = prtObjToDisplay->mPresContext;
  mViewManager  = prtObjToDisplay->mViewManager;
  mWindow       = prtObjToDisplay->mWindow;

  if (mIsDoingPrintPreview && mOldPrtPreview) {
    delete mOldPrtPreview;
    mOldPrtPreview = nsnull;
  }

  prtObjToDisplay->mSharedPresShell = PR_TRUE;
  mPresShell->BeginObservingDocument();

  nscoord width  = bounds.width;
  nscoord height = bounds.height;
  float p2t;
  mPresContext->GetPixelsToTwips(&p2t);
  width = NSIntPixelsToTwips(width, p2t);
  height = NSIntPixelsToTwips(height, p2t);
  mViewManager->DisableRefresh();
  mViewManager->SetWindowDimensions(width, height);

  mDeviceContext->SetUseAltDC(kUseAltDCFor_FONTMETRICS, PR_FALSE);
  mDeviceContext->SetUseAltDC(kUseAltDCFor_CREATERC_PAINT, PR_TRUE);

  mViewManager->EnableRefresh(NS_VMREFRESH_NO_SYNC);
  Show();

  ShowDocList(mPrt->mPrintObject, PR_TRUE);
}

void
DocumentViewerImpl::ReturnToGalleyPresentation()
{
  if (!mIsDoingPrintPreview) {
    NS_ASSERTION(0, "Wow, we should never get here!");
    return;
  }

  if (!mPrtPreview->HasCachedPres()) {
    delete mPrtPreview;
    mPrtPreview = nsnull;
  }

  // Get the current size of what is being viewed
  nsRect area;
  mPresContext->GetVisibleArea(area);

  nsRect bounds;
  mWindow->GetBounds(bounds);

  // In case we have focus focus the parent DocShell
  // which in this case should always be chrome
  nsCOMPtr<nsIDocShellTreeItem>  dstParentItem;
  nsCOMPtr<nsIDocShellTreeItem>  dstItem(do_QueryInterface(mContainer));
  if (dstItem) {
    dstItem->GetParent(getter_AddRefs(dstParentItem));
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(dstParentItem));
    if (docShell) {
      docShell->SetHasFocus(PR_TRUE);
    }
  }

  // Start to kill off the old Presentation
  // by cleaning up the PresShell
  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();
    nsCOMPtr<nsISelection> selection;
    nsresult rv = GetDocumentSelection(getter_AddRefs(selection));
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
    if (NS_SUCCEEDED(rv) && selPrivate && mSelectionListener)
      selPrivate->RemoveSelectionListener(mSelectionListener);
    mPresShell->Destroy();
  }

  // clear weak references before we go away
  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
  }

  // wasCached will be used below to indicate whether the 
  // InitInternal should create all new objects or just
  // initialize the existing ones
  PRBool wasCached = PR_FALSE;

  if (mPrtPreview && mPrtPreview->HasCachedPres()) {
    NS_ASSERTION(mPrtPreview->mCachedPresObj, "mCachedPresObj can't be null!");
    mPresShell    = mPrtPreview->mCachedPresObj->mPresShell;
    mPresContext  = mPrtPreview->mCachedPresObj->mPresContext;
    mViewManager  = mPrtPreview->mCachedPresObj->mViewManager;
    mWindow       = mPrtPreview->mCachedPresObj->mWindow;

    // Tell the "real" presshell to start observing the document
    // again.
    mPresShell->BeginObservingDocument();

    mWindow->Show(PR_TRUE);

    // Very important! Turn On scripting
    TurnScriptingOn(PR_TRUE);

    delete mPrtPreview;
    mPrtPreview = nsnull;

    wasCached = PR_TRUE;
  } else {
    // Destroy the old Presentation
    mPresShell    = nsnull;
    mPresContext  = nsnull;
    mViewManager  = nsnull;
    mWindow       = nsnull;
  }

  // Very important! Turn On scripting
  TurnScriptingOn(PR_TRUE);

  InitInternal(mParentWidget, mDeviceContext, bounds, !wasCached);

  // this needs to be set here not earlier,
  // because it is needing when re-constructing the Galley Mode)
  mIsDoingPrintPreview = PR_FALSE;

  mViewManager->EnableRefresh(NS_VMREFRESH_NO_SYNC);

  Show();
}

#endif // NS_PRINT_PREVIEW

//-----------------------------------------------------------------
// This method checks to see if there is at least one printer defined
// and if so, it sets the first printer in the list as the default name
// in the PrintSettings which is then used for Printer Preview
nsresult
DocumentViewerImpl::CheckForPrinters(nsIPrintOptions*  aPrintOptions,
                                     nsIPrintSettings* aPrintSettings,
                                     PRUint32          aErrorCode,
                                     PRBool            aIsPrinting)
{
  NS_ENSURE_ARG_POINTER(aPrintOptions);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> simpEnum;
  aPrintOptions->AvailablePrinters(getter_AddRefs(simpEnum));
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
      if (!defPrinterName || (defPrinterName && !*defPrinterName)) {
        if (defPrinterName) nsMemory::Free(defPrinterName);
        nsCOMPtr<nsISupportsWString> wStr = do_QueryInterface(supps);
        if (wStr) {
          PRUnichar* defPrinterName;
          wStr->ToString(&defPrinterName);
          aPrintSettings->SetPrinterName(defPrinterName);
          nsMemory::Free(defPrinterName);
        }
      } else {
        nsMemory::Free(defPrinterName);
      }
      rv = NS_OK;
    } else {
      // this means there were no printers
      ShowPrintErrorDialog(aErrorCode, aIsPrinting);
    }
  } else {
    // this means there were no printers
    // XXX the ifdefs are temporary until they correctly implement Available Printers
#if defined(XP_MAC) || defined(XP_MACOSX)
    rv = NS_OK;
#else
    ShowPrintErrorDialog(aErrorCode, aIsPrinting);
#endif
  }
  return rv;
}


/** ---------------------------------------------------
 *  Check to see if the current Presentation should be cached
 *
 */
PRBool
DocumentViewerImpl::CheckDocumentForPPCaching()
{
  // Here is where we determine if we need to cache the old presentation
  PRBool cacheOldPres = PR_FALSE;

  // Only check if it is the first time into PP
  if (!mOldPrtPreview) {
    // First check the Pref
    nsCOMPtr<nsIPref> prefs (do_GetService(NS_PREF_CONTRACTID));
    if (prefs) {
      prefs->GetBoolPref("print.always_cache_old_pres", &cacheOldPres);
    }

    // Temp fix for FrameSet Print Preview Bugs
    if (!cacheOldPres && mPrt->mPrintObject->mFrameType == eFrameSet) {
      cacheOldPres = PR_TRUE;
    }

    if (!cacheOldPres) {
      for (PRInt32 i=0;i<mPrt->mPrintDocList->Count();i++) {
        PrintObject* po = (PrintObject*)mPrt->mPrintDocList->ElementAt(i);
        NS_ASSERTION(po, "PrintObject can't be null!");

        // Temp fix for FrameSet Print Preview Bugs
        if (po->mFrameType == eIFrame) {
          cacheOldPres = PR_TRUE;
          break;
        }

        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(po->mWebShell));
        NS_ASSERTION(docShell, "The DocShell can't be NULL!");
        if (!docShell) continue;

        nsCOMPtr<nsIPresShell>  presShell;
        docShell->GetPresShell(getter_AddRefs(presShell));
        NS_ASSERTION(presShell, "The PresShell can't be NULL!");
        if (!presShell) continue;

        nsCOMPtr<nsIDocument> doc;
        presShell->GetDocument(getter_AddRefs(doc));
        NS_ASSERTION(doc, "The PresShell can't be NULL!");
        if (!doc) continue;

        // If we aren't caching because of prefs check embeds.
        nsCOMPtr<nsIDOMNSHTMLDocument> nshtmlDoc = do_QueryInterface(doc);
        if (nshtmlDoc) {
          nsCOMPtr<nsIDOMHTMLCollection> applets;
          nshtmlDoc->GetEmbeds(getter_AddRefs(applets));
          if (applets) {
            PRUint32 length = 0;
            if (NS_SUCCEEDED(applets->GetLength(&length))) {
              if (length > 0) {
                cacheOldPres = PR_TRUE;
                break;
              }
            }
          }
        }

        // If we aren't caching because of prefs or embeds check applets.
        nsCOMPtr<nsIDOMHTMLDocument> htmldoc = do_QueryInterface(doc);
        if (htmldoc) {
          nsCOMPtr<nsIDOMHTMLCollection> embeds;
          htmldoc->GetApplets(getter_AddRefs(embeds));
          if (embeds) {
            PRUint32 length = 0;
            if (NS_SUCCEEDED(embeds->GetLength(&length))) {
              if (length > 0) {
                cacheOldPres = PR_TRUE;
                break;
              }
            }
          }
        }
      }
    }
  }

  return cacheOldPres;

}

/** ---------------------------------------------------
 *  Check to see if the FrameSet Frame is Hidden
 *  if it is then don't let it be reflowed, printed, or shown
 */
void DocumentViewerImpl::CheckForHiddenFrameSetFrames()
{
  for (PRInt32 i=0;i<mPrt->mPrintDocList->Count();i++) {
    PrintObject* po = (PrintObject*)mPrt->mPrintDocList->ElementAt(i);
    NS_ASSERTION(po, "PrintObject can't be null!");
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(po->mWebShell));
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell;
      docShell->GetPresShell(getter_AddRefs(presShell));
      if (presShell) {
        nsIFrame* frame;
        presShell->GetRootFrame(&frame);
        if (frame) {
          nsRect rect;
          frame->GetRect(rect);
          if (rect.height == 0) {
            // set this PO and its children to not print and be hidden
            SetPrintPO(po, PR_FALSE, PR_TRUE, eSetPrintFlag | eSetHiddenFlag);
          }
        }
      }
    }
  }
}


/** ---------------------------------------------------
 *  See documentation above in the nsIContentViewerfile class definition
 *	@update 11/01/01 rods
 *
 *  For a full and detailed understanding of the issues with
 *  PrintPreview: See the design spec that is attached to Bug 107562
 */
NS_IMETHODIMP
DocumentViewerImpl::PrintPreview(nsIPrintSettings* aPrintSettings, 
                                 nsIDOMWindow *aChildDOMWin, 
                                 nsIWebProgressListener* aWebProgressListener)
{
  if (!mPresShell) {
    // A frame that's not displayed can't be printed!

    return NS_OK;
  }

  if (mIsDoingPrinting) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mContainer));
  NS_ASSERTION(docShell, "This has to be a docshell");

  // Temporary code for Bug 136185
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (xulDoc) {
    ShowPrintErrorDialog(NS_ERROR_GFX_PRINTER_NO_XUL, PR_FALSE);
    return NS_ERROR_FAILURE;
  }

  // Get the webshell for this documentviewer
  nsCOMPtr<nsIWebShell> webContainer(do_QueryInterface(mContainer));

  // Get the DocShell and see if it is busy
  // We can't Print or Print Preview this document if it is still busy

  PRUint32 busyFlags = nsIDocShell::BUSY_FLAGS_NONE;

  // Preview this document if it is still busy

  if (NS_FAILED(docShell->GetBusyFlags(&busyFlags)) ||
      busyFlags != nsIDocShell::BUSY_FLAGS_NONE) {
    ShowPrintErrorDialog(NS_ERROR_GFX_PRINTER_DOC_IS_BUSY_PP, PR_FALSE);
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

#if defined(XP_PC) && defined(EXTENDED_DEBUG_PRINTING)
  if (!mIsDoingPrintPreview) {
    if (kPrintingLogMod && kPrintingLogMod->level == DUMP_LAYOUT_LEVEL) {
      RemoveFilesInDir(".\\");
    }
  }
#endif

#ifdef NS_PRINT_PREVIEW
  if (mIsDoingPrintPreview) {
    mOldPrtPreview = mPrtPreview;
    mPrtPreview = nsnull;
  }

  mPrt = new PrintData(PrintData::eIsPrintPreview);
  if (!mPrt) {
    mIsCreatingPrintPreview = PR_FALSE;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Check to see if we need to transfer any of our old values
  // over to the new PrintData object
  if (mOldPrtPreview) {
    mPrt->mOrigZoom     = mOldPrtPreview->mOrigZoom;
    mPrt->mOrigTextZoom = mOldPrtPreview->mOrigTextZoom;
    mPrt->mOrigDCScale  = mOldPrtPreview->mOrigDCScale;

    // maintain the the original presentation if it is there
    // by transfering it over to the new PrintData object
    if (mOldPrtPreview->HasCachedPres()) {
      mPrt->mIsCachingPresentation = mOldPrtPreview->mIsCachingPresentation;
      mPrt->mCachedPresObj         = mOldPrtPreview->mCachedPresObj;
      // don't want it to get deleted when the mOldPrtPreview is deleted 
      mOldPrtPreview->mIsCachingPresentation = PR_FALSE;
      mOldPrtPreview->mCachedPresObj         = nsnull;
    }
  } else {
    // Get the Original PixelScale in case we need to start changing it
    mDeviceContext->GetCanonicalPixelScale(mPrt->mOrigDCScale);
  }

  // You have to have both a PrintOptions and a PrintSetting to call
  // CheckForPrinters.
  // The user can pass in a null PrintSettings, but you can only
  // create one if you have a PrintOptions.  So we we might as check
  // to if we have a PrintOptions first, because we can't do anything
  // below without it then inside we check to se if the printSettings
  // is null to know if we need to create on.
  // if they don't pass in a PrintSettings, then get the Global PS
  mPrt->mPrintSettings = aPrintSettings;
  if (!mPrt->mPrintSettings) {
    GetGlobalPrintSettings(getter_AddRefs(mPrt->mPrintSettings));
  }

  mPrt->mPrintOptions = do_GetService(sPrintOptionsContractID, &rv);
  if (NS_SUCCEEDED(rv) && mPrt->mPrintOptions && mPrt->mPrintSettings) {
    // Get the default printer name and set it into the PrintSettings
    rv = CheckForPrinters(mPrt->mPrintOptions, mPrt->mPrintSettings, NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE, PR_TRUE);
  } else {
    NS_ASSERTION(mPrt->mPrintSettings, "You can't Print without a PrintSettings!");
    rv = NS_ERROR_FAILURE;
  }
  if (NS_FAILED(rv)) {
    delete mPrt;
    mPrt = nsnull;
    return NS_ERROR_FAILURE;
  }

  // Let's print ...
  mIsCreatingPrintPreview = PR_TRUE;
  mIsDoingPrintPreview    = PR_TRUE;

  // Very important! Turn Off scripting
  TurnScriptingOn(PR_FALSE);

  // Get the currently focused window and cache it
  // because the Print Dialog will "steal" focus and later when you try
  // to get the currently focused windows it will be NULL
  mPrt->mCurrentFocusWin = getter_AddRefs(FindFocusedDOMWindowInternal());

  // Check to see if there is a "regular" selection
  PRBool isSelection = IsThereARangeSelection(mPrt->mCurrentFocusWin);

  // Create a list for storing the WebShells that need to be printed
  if (!mPrt->mPrintDocList) {
    mPrt->mPrintDocList = new nsVoidArray();
    if (!mPrt->mPrintDocList) {
      mIsCreatingPrintPreview = PR_FALSE;
      mIsDoingPrintPreview    = PR_FALSE;
      TurnScriptingOn(PR_TRUE);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    mPrt->mPrintDocList->Clear();
  }

  // Add Root Doc to Tree and List
  mPrt->mPrintObject             = new PrintObject;
  mPrt->mPrintObject->mWebShell  = webContainer;
  mPrt->mPrintDocList->AppendElement(mPrt->mPrintObject);

  mPrt->mIsParentAFrameSet = IsParentAFrameSet(webContainer);
  mPrt->mPrintObject->mFrameType = mPrt->mIsParentAFrameSet ? eFrameSet : eDoc;

  // Build the "tree" of PrintObjects
  nsCOMPtr<nsIDocShellTreeNode>  parentAsNode(do_QueryInterface(webContainer));
  BuildDocTree(parentAsNode, mPrt->mPrintDocList, mPrt->mPrintObject);

  // Create the linkage from the suv-docs back to the content element
  // in the parent document
  MapContentToWebShells(mPrt->mPrintObject, mPrt->mPrintObject);

  // Get whether the doc contains a frameset
  // Also, check to see if the currently focus webshell
  // is a child of this webshell
  mPrt->mIsIFrameSelected = IsThereAnIFrameSelected(webContainer, mPrt->mCurrentFocusWin, mPrt->mIsParentAFrameSet);

  CheckForHiddenFrameSetFrames();

  DUMP_DOC_LIST("\nAfter Mapping------------------------------------------");

  // Setup print options for UI
  rv = NS_ERROR_FAILURE;
  if (mPrt->mPrintSettings != nsnull) {
    mPrt->mPrintSettings->GetShrinkToFit(&mPrt->mShrinkToFit);

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
  }

#ifdef PR_LOGGING
  if (mPrt->mPrintSettings) {
    PRInt16 printHowEnable = nsIPrintSettings::kFrameEnableNone;
    mPrt->mPrintSettings->GetHowToEnableFrameUI(&printHowEnable);
    PRBool val;
    mPrt->mPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &val);

    PR_PL(("********* DocumentViewerImpl::Print *********\n"));
    PR_PL(("IsParentAFrameSet:   %s \n", PRT_YESNO(mPrt->mIsParentAFrameSet)));
    PR_PL(("IsIFrameSelected:    %s \n", PRT_YESNO(mPrt->mIsIFrameSelected)));
    PR_PL(("Main Doc Frame Type: %s \n", gFrameTypesStr[mPrt->mPrintObject->mFrameType]));
    PR_PL(("HowToEnableFrameUI:  %s \n", gFrameHowToEnableStr[printHowEnable]));
    PR_PL(("EnableSelectionRB:   %s \n", PRT_YESNO(val)));
    PR_PL(("*********************************************\n"));
  }
#endif

  nscoord width  = NS_INCHES_TO_TWIPS(8.5);
  nscoord height = NS_INCHES_TO_TWIPS(11.0);

  nsCOMPtr<nsIDeviceContext> ppDC;
  nsCOMPtr<nsIDeviceContextSpecFactory> factory = do_CreateInstance(kDeviceContextSpecFactoryCID);
  if (factory) {
    nsCOMPtr<nsIDeviceContextSpec> devspec;
    nsCOMPtr<nsIDeviceContext> dx;
    nsresult rv = factory->CreateDeviceContextSpec(mWindow, mPrt->mPrintSettings, *getter_AddRefs(devspec), PR_TRUE);
    if (NS_SUCCEEDED(rv)) {
      rv = mDeviceContext->GetDeviceContextFor(devspec, *getter_AddRefs(ppDC));
      if (NS_SUCCEEDED(rv)) {
        mDeviceContext->SetAltDevice(ppDC);
        if (mPrt->mPrintSettings != nsnull) {
          // Shrink to Fit over rides and scaling values
          if (!mPrt->mShrinkToFit) {
            double scaling;
            mPrt->mPrintSettings->GetScaling(&scaling);
            mDeviceContext->SetCanonicalPixelScale(float(scaling)*mPrt->mOrigDCScale);
          }
        }
        ppDC->GetDeviceSurfaceDimensions(width, height);
      }
    }
  }

  mPrt->mPrintSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);

  // override any UI that wants to PrintPreview any selection
  PRInt16 printRangeType = nsIPrintSettings::kRangeAllPages;
  mPrt->mPrintSettings->GetPrintRange(&printRangeType);
  if (printRangeType == nsIPrintSettings::kRangeSelection) {
    mPrt->mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
  }

  mPrt->mPrintDC = mDeviceContext;

  // Cache original Zoom value and then set it to 1.0
  mPrt->mPrintDC->GetTextZoom(mPrt->mOrigTextZoom);
  mPrt->mPrintDC->GetZoom(mPrt->mOrigZoom);
  mPrt->mPrintDC->SetTextZoom(1.0f);
  mPrt->mPrintDC->SetZoom(1.0f);

  if (mDeviceContext) {
    mDeviceContext->SetUseAltDC(kUseAltDCFor_FONTMETRICS, PR_TRUE);
    mDeviceContext->SetUseAltDC(kUseAltDCFor_CREATERC_REFLOW, PR_TRUE);
    mDeviceContext->SetUseAltDC(kUseAltDCFor_SURFACE_DIM, PR_TRUE);
  }

  PRBool cacheOldPres = CheckDocumentForPPCaching();

  // If we are caching the Presentation then
  // end observing the document BEFORE we do any new reflows
  if (cacheOldPres && !mPrt->HasCachedPres()) {
    mPrt->SetCacheOldPres(PR_TRUE);
    mPresShell->EndObservingDocument();
  }

  rv = DocumentReadyForPrinting();

  mIsCreatingPrintPreview = PR_FALSE;
  /* cleaup on failure + notify user */
  if (NS_FAILED(rv)) {
    if (mPrt) {
      delete mPrt;
      mPrt = nsnull;
    }

    /* cleanup done, let's fire-up an error dialog to notify the user
     * what went wrong...
     */
    ShowPrintErrorDialog(rv, PR_FALSE);
    TurnScriptingOn(PR_TRUE);
    mIsCreatingPrintPreview = PR_FALSE;
    mIsDoingPrintPreview    = PR_FALSE;
    return rv;
  }


  // At this point we are done preparing everything
  // before it is to be created

  // Noew create the new Presentation and display it
  InstallNewPresentation();

  // PrintPreview was built using the mPrt (code reuse)
  // then we assign it over
  mPrtPreview = mPrt;
  mPrt        = nsnull;

  // Turning off the scaling of twips so any of the UI scrollbars
  // will not get scaled
  nsCOMPtr<nsIPrintPreviewContext> printPreviewContext(do_QueryInterface(mPresContext));
  if (printPreviewContext) {
    printPreviewContext->SetScalingOfTwips(PR_FALSE);
    mDeviceContext->SetCanonicalPixelScale(mPrtPreview->mOrigDCScale);
  }

#endif // NS_PRINT_PREVIEW

  return NS_OK;
}


void
DocumentViewerImpl::SetDocAndURLIntoProgress(PrintObject* aPO,
                                             nsIPrintProgressParams* aParams)
{
  NS_ASSERTION(aPO, "Must have vaild PrintObject");
  NS_ASSERTION(aParams, "Must have vaild nsIPrintProgressParams");

  if (!aPO || !aPO->mWebShell || !aParams) {
    return;
  }
  const PRUint32 kTitleLength = 64;

  PRUnichar * docTitleStr;
  PRUnichar * docURLStr;
  GetDisplayTitleAndURL(aPO, mPrt->mPrintSettings, mPrt->mBrandName,
                        &docTitleStr, &docURLStr, eDocTitleDefURLDoc);

  // Make sure the URLS don't get too long for the progress dialog
  if (docURLStr && nsCRT::strlen(docURLStr) > kTitleLength) {
    PRUnichar * ptr = &docURLStr[nsCRT::strlen(docURLStr)-kTitleLength+3];
    nsAutoString newURLStr;
    newURLStr.AppendWithConversion("...");
    newURLStr += ptr;
    nsMemory::Free(docURLStr);
    docURLStr = ToNewUnicode(newURLStr);
  }

  aParams->SetDocTitle((const PRUnichar*) docTitleStr);
  aParams->SetDocURL((const PRUnichar*) docURLStr);

  if (docTitleStr != nsnull) nsMemory::Free(docTitleStr);
  if (docURLStr != nsnull) nsMemory::Free(docURLStr);
}

//----------------------------------------------------------------------
// Set up to use the "pluggable" Print Progress Dialog
void
DocumentViewerImpl::DoPrintProgress(PRBool aIsForPrinting)
{
  // Assume we can't do progress and then see if we can
  mPrt->mShowProgressDialog = PR_FALSE;

  nsCOMPtr<nsIPref> prefs (do_GetService(NS_PREF_CONTRACTID));
  if (prefs) {
    prefs->GetBoolPref("print.show_print_progress", &mPrt->mShowProgressDialog);
  }

  // Turning off the showing of Print Progress in Prefs overrides
  // whether the calling PS desire to have it on or off, so only check PS if 
  // prefs says it's ok to be on.
  if (mPrt->mShowProgressDialog) {
    mPrt->mPrintSettings->GetShowPrintProgress(&mPrt->mShowProgressDialog);
  }

  // Now open the service to get the progress dialog
  nsCOMPtr<nsIPrintingPromptService> printPromptService(do_GetService(kPrintingPromptService));
  if (printPromptService) {
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
    mDocument->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));
    if (!scriptGlobalObject) return;
    nsCOMPtr<nsIDOMWindow> domWin = do_QueryInterface(scriptGlobalObject); 
    if (!domWin) return;

    // If we don't get a service, that's ok, then just don't show progress
    if (mPrt->mShowProgressDialog) {
      PRBool notifyOnOpen;
      nsresult rv = printPromptService->ShowProgress(domWin, this, mPrt->mPrintSettings, nsnull, PR_TRUE, getter_AddRefs(mPrt->mPrintProgressListener), getter_AddRefs(mPrt->mPrintProgressParams), &notifyOnOpen);
      if (NS_SUCCEEDED(rv)) {
        mPrt->mShowProgressDialog = mPrt->mPrintProgressListener != nsnull && mPrt->mPrintProgressParams != nsnull;

        if (mPrt->mShowProgressDialog) {
          mPrt->mPrintProgressListeners.AppendElement((void*)mPrt->mPrintProgressListener);
          nsIWebProgressListener* wpl = NS_STATIC_CAST(nsIWebProgressListener*, mPrt->mPrintProgressListener.get());
          NS_ASSERTION(wpl, "nsIWebProgressListener is NULL!");
          NS_ADDREF(wpl);
          SetDocAndURLIntoProgress(mPrt->mPrintObject, mPrt->mPrintProgressParams);
        }
      }
    }
  }
}

/* ========================================================================================
 * nsIContentViewerFile
 * ======================================================================================== */
/** ---------------------------------------------------
 *  See documentation above in the nsIContentViewerfile class definition
 *	@update 01/24/00 dwc
 */
NS_IMETHODIMP
DocumentViewerImpl::Print(PRBool            aSilent,
                          FILE *            aDebugFile,
                          nsIPrintSettings* aPrintSettings)
{
  nsCOMPtr<nsIPrintSettings> printSettings;
#ifdef NS_DEBUG
  nsresult rv = NS_ERROR_FAILURE;

  mDebugFile = aDebugFile;
  // if they don't pass in a PrintSettings, then make one
  // it will have all the default values
  printSettings = aPrintSettings;
  nsCOMPtr<nsIPrintOptions> printOptions = do_GetService(sPrintOptionsContractID, &rv);
  if (NS_SUCCEEDED(rv)) {
    // if they don't pass in a PrintSettings, then make one
    if (printSettings == nsnull) {
      printOptions->CreatePrintSettings(getter_AddRefs(printSettings));
    }
    NS_ASSERTION(printSettings, "You can't PrintPreview without a PrintSettings!");
  }
  if (printSettings) printSettings->SetPrintSilent(aSilent);
  if (printSettings) printSettings->SetShowPrintProgress(PR_FALSE);
#endif


  return Print(printSettings, nsnull);

}

/** ---------------------------------------------------
 *  From nsIWebBrowserPrint
 */
NS_IMETHODIMP
DocumentViewerImpl::Print(nsIPrintSettings*       aPrintSettings,
                          nsIWebProgressListener* aWebProgressListener)
{
#ifdef EXTENDED_DEBUG_PRINTING
  // need for capturing result on each doc and sub-doc that is printed
  gDumpFileNameCnt   = 0;
  gDumpLOFileNameCnt = 0;
#if defined(XP_PC)
  if (kPrintingLogMod && kPrintingLogMod->level == DUMP_LAYOUT_LEVEL) {
    RemoveFilesInDir(".\\");
  }
#endif // XP_PC
#endif // EXTENDED_DEBUG_PRINTING

  // Temporary code for Bug 136185
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (xulDoc) {
    ShowPrintErrorDialog(NS_ERROR_GFX_PRINTER_NO_XUL);
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mContainer));
  NS_ASSERTION(docShell, "This has to be a docshell");

  // Check to see if this document is still busy
  // If it is busy and we aren't already "queued" up to print then
  // Indicate there is a print pending and cache the args for later
  PRUint32 busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
  if ((NS_FAILED(docShell->GetBusyFlags(&busyFlags)) ||
       busyFlags != nsIDocShell::BUSY_FLAGS_NONE) && 
      !mPrintDocIsFullyLoaded) {
    if (!mPrintIsPending) {
      mCachedPrintSettings           = aPrintSettings;
      mCachedPrintWebProgressListner = aWebProgressListener;
      mPrintIsPending                = PR_TRUE;
    }
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell) {
    // A frame that's not displayed can't be printed!
    PR_PL(("Printing Stopped - PreShell was NULL!"));
    return NS_OK;
  }

  if (mIsDoingPrintPreview) {
    PRBool okToPrint = PR_FALSE;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    if (prefs) {
      prefs->GetBoolPref("print.whileInPrintPreview", &okToPrint);
    }
    if (!okToPrint) {
      ShowPrintErrorDialog(NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW, PR_FALSE);
      return NS_OK;
    }
  }

  // if we are printing another URL, then exit
  // the reason we check here is because this method can be called while
  // another is still in here (the printing dialog is a good example).
  // the only time we can print more than one job at a time is the regression tests
  if (mIsDoingPrinting) {
    // Let the user know we are not ready to print.
    rv = NS_ERROR_NOT_AVAILABLE;
    ShowPrintErrorDialog(rv);
    return rv;
  }
  
  mPrt = new PrintData(PrintData::eIsPrinting);
  if (!mPrt) {
    PR_PL(("NS_ERROR_OUT_OF_MEMORY - Creating PrintData"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // if they don't pass in a PrintSettings, then get the Global PS
  mPrt->mPrintSettings = aPrintSettings;
  if (!mPrt->mPrintSettings) {
    GetGlobalPrintSettings(getter_AddRefs(mPrt->mPrintSettings));
  }

  mPrt->mPrintOptions = do_GetService(sPrintOptionsContractID, &rv);
  if (NS_SUCCEEDED(rv) && mPrt->mPrintOptions && mPrt->mPrintSettings) {
    // Get the default printer name and set it into the PrintSettings
    rv = CheckForPrinters(mPrt->mPrintOptions, mPrt->mPrintSettings, NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE, PR_TRUE);
  } else {
    NS_ASSERTION(mPrt->mPrintSettings, "You can't Print without a PrintSettings!");
    rv = NS_ERROR_FAILURE;
  }
  if (NS_FAILED(rv)) {
    delete mPrt;
    PR_PL(("NS_ERROR_FAILURE - CheckForPrinters for Printers failed"));
    mPrt = nsnull;
    return NS_ERROR_FAILURE;
  }
  mPrt->mPrintSettings->SetIsCancelled(PR_FALSE);
  mPrt->mPrintSettings->GetShrinkToFit(&mPrt->mShrinkToFit);

  // Let's print ...
  mIsDoingPrinting = PR_TRUE;

  // We need to  make sure this document doesn't get unloaded
  // before we have a chance to print, so this stops the Destroy from
  // being called
  mPrt->mPreparingForPrint = PR_TRUE;

  if (aWebProgressListener != nsnull) {
    mPrt->mPrintProgressListeners.AppendElement((void*)aWebProgressListener);
    NS_ADDREF(aWebProgressListener);
  }

  // Get the currently focused window and cache it
  // because the Print Dialog will "steal" focus and later when you try
  // to get the currently focused windows it will be NULL
  mPrt->mCurrentFocusWin = getter_AddRefs(FindFocusedDOMWindowInternal());

  // Check to see if there is a "regular" selection
  PRBool isSelection = IsThereARangeSelection(mPrt->mCurrentFocusWin);

  // Create a list for storing the WebShells that need to be printed
  if (mPrt->mPrintDocList == nsnull) {
    mPrt->mPrintDocList = new nsVoidArray();
    if (mPrt->mPrintDocList == nsnull) {
      mIsDoingPrinting = PR_FALSE;
      delete mPrt;
      mPrt = nsnull;
      PR_PL(("NS_ERROR_FAILURE - Couldn't create mPrintDocList"));
      return NS_ERROR_FAILURE;
    }
  } else {
    mPrt->mPrintDocList->Clear();
  }

  // Get the webshell for this documentviewer
  nsCOMPtr<nsIWebShell> webContainer(do_QueryInterface(mContainer));

  // Add Root Doc to Tree and List
  mPrt->mPrintObject             = new PrintObject;
  mPrt->mPrintObject->mWebShell  = webContainer;
  mPrt->mPrintDocList->AppendElement(mPrt->mPrintObject);

  mPrt->mIsParentAFrameSet = IsParentAFrameSet(webContainer);
  mPrt->mPrintObject->mFrameType = mPrt->mIsParentAFrameSet?eFrameSet:eDoc;

  // Build the "tree" of PrintObjects
  nsCOMPtr<nsIDocShellTreeNode>  parentAsNode(do_QueryInterface(webContainer));
  BuildDocTree(parentAsNode, mPrt->mPrintDocList, mPrt->mPrintObject);

  // Create the linkage from the suv-docs back to the content element
  // in the parent document
  MapContentToWebShells(mPrt->mPrintObject, mPrt->mPrintObject);


  // Get whether the doc contains a frameset
  // Also, check to see if the currently focus webshell
  // is a child of this webshell
  mPrt->mIsIFrameSelected = IsThereAnIFrameSelected(webContainer, mPrt->mCurrentFocusWin, mPrt->mIsParentAFrameSet);

  CheckForHiddenFrameSetFrames();

  DUMP_DOC_LIST("\nAfter Mapping------------------------------------------");

  rv = NS_ERROR_FAILURE;
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

#ifdef PR_LOGGING
  if (mPrt->mPrintSettings) {
    PRInt16 printHowEnable = nsIPrintSettings::kFrameEnableNone;
    mPrt->mPrintSettings->GetHowToEnableFrameUI(&printHowEnable);
    PRBool val;
    mPrt->mPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &val);

    PR_PL(("********* DocumentViewerImpl::Print *********\n"));
    PR_PL(("IsParentAFrameSet:   %s \n", PRT_YESNO(mPrt->mIsParentAFrameSet)));
    PR_PL(("IsIFrameSelected:    %s \n", PRT_YESNO(mPrt->mIsIFrameSelected)));
    PR_PL(("Main Doc Frame Type: %s \n", gFrameTypesStr[mPrt->mPrintObject->mFrameType]));
    PR_PL(("HowToEnableFrameUI:  %s \n", gFrameHowToEnableStr[printHowEnable]));
    PR_PL(("EnableSelectionRB:   %s \n", PRT_YESNO(val)));
    PR_PL(("*********************************************\n"));
  }
#endif

  /* create factory (incl. create print dialog) */
  nsCOMPtr<nsIDeviceContextSpecFactory> factory =
          do_CreateInstance(kDeviceContextSpecFactoryCID, &rv);

  if (NS_SUCCEEDED(rv)) {
#ifdef DEBUG_dcone
    printf("PRINT JOB STARTING\n");
#endif

    nsCOMPtr<nsIDeviceContextSpec> devspec;
    nsCOMPtr<nsIDeviceContext> dx;
    mPrt->mPrintDC = nsnull; // XXX why?

#ifdef NS_DEBUG
    mPrt->mDebugFilePtr = mDebugFile;
#endif

    PRBool printSilently;
    mPrt->mPrintSettings->GetPrintSilent(&printSilently);

    // Ask dialog to be Print Shown via the Plugable Printing Dialog Service
    // This service is for the Print Dialog and the Print Progress Dialog
    // If printing silently or you can't get the service continue on
    if (!printSilently) {
      nsCOMPtr<nsIPrintingPromptService> printPromptService(do_GetService(kPrintingPromptService));
      if (printPromptService) {
        nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
        mDocument->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));
        if (!scriptGlobalObject) return nsnull;
        nsCOMPtr<nsIDOMWindow> domWin = do_QueryInterface(scriptGlobalObject); 
        if (!domWin) return nsnull;

        // Platforms not implementing a given dialog for the service may
        // return NS_ERROR_NOT_IMPLEMENTED or an error code.
        //
        // NS_ERROR_NOT_IMPLEMENTED indicates they want default behavior
        // Any other error code means we must bail out
        //
        rv = printPromptService->ShowPrintDialog(domWin, this, mPrt->mPrintSettings);
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

    if (NS_FAILED(rv)) {
      if (rv != NS_ERROR_ABORT) {
        ShowPrintErrorDialog(rv);
      }
      delete mPrt;
      mPrt = nsnull;
      PR_PL(("**** Printing Stopped before CreateDeviceContextSpec"));
      return rv;
    }

    // Create DeviceSpec for Printing
    rv = factory->CreateDeviceContextSpec(mWindow, mPrt->mPrintSettings, *getter_AddRefs(devspec), PR_FALSE);

    // If the page was intended to be destroyed while we were in the print dialog 
    // then we need to clean up and abort the printing.
    if (mPrt->mDocWasToBeDestroyed) {
      mPrt->mPreparingForPrint = PR_FALSE;
      Destroy();
      mIsDoingPrinting = PR_FALSE;
      // If they hit cancel then rv will equal NS_ERROR_ABORT and 
      // then we don't want to display the message
      if (rv != NS_ERROR_ABORT) {
        ShowPrintErrorDialog(NS_ERROR_GFX_PRINTER_DOC_WAS_DESTORYED);
      }
      PR_PL(("**** mDocWasToBeDestroyed - %s", rv != NS_ERROR_ABORT?"NS_ERROR_GFX_PRINTER_DOC_WAS_DESTORYED":"NS_ERROR_ABORT"));
      return NS_ERROR_ABORT;
    }

    if (NS_SUCCEEDED(rv)) {
      rv = mPresContext->GetDeviceContext(getter_AddRefs(dx));
      if (NS_SUCCEEDED(rv)) {
        rv = dx->GetDeviceContextFor(devspec, *getter_AddRefs(mPrt->mPrintDC));
        if (NS_SUCCEEDED(rv)) {
          // Get the Original PixelScale incase we need to start changing it
          mPrt->mPrintDC->GetCanonicalPixelScale(mPrt->mOrigDCScale);
          // Shrink to Fit over rides and scaling values
          if (!mPrt->mShrinkToFit) {
            double scaling;
            mPrt->mPrintSettings->GetScaling(&scaling);
            mPrt->mPrintDC->SetCanonicalPixelScale(float(scaling)*mPrt->mOrigDCScale);
          }

          if(webContainer) {
#ifdef DEBUG_dcone
            float   a1,a2;
            PRInt32 i1,i2;

            printf("CRITICAL PRINTING INFORMATION\n");

            // DEVICE CONTEXT INFORMATION from PresContext
            printf("DeviceContext of Presentation Context(%x)\n",dx);
            dx->GetDevUnitsToTwips(a1);
            dx->GetTwipsToDevUnits(a2);
            printf("    DevToTwips = %f TwipToDev = %f\n",a1,a2);
            dx->GetAppUnitsToDevUnits(a1);
            dx->GetDevUnitsToAppUnits(a2);
            printf("    AppUnitsToDev = %f DevUnitsToApp = %f\n",a1,a2);
            dx->GetCanonicalPixelScale(a1);
            printf("    GetCanonicalPixelScale = %f\n",a1);
            dx->GetScrollBarDimensions(a1, a2);
            printf("    ScrollBar x = %f y = %f\n",a1,a2);
            dx->GetZoom(a1);
            printf("    Zoom = %f\n",a1);
            dx->GetDepth((PRUint32&)i1);
            printf("    Depth = %d\n",i1);
            dx->GetDeviceSurfaceDimensions(i1,i2);
            printf("    DeviceDimension w = %d h = %d\n",i1,i2);


            // DEVICE CONTEXT INFORMATION
            printf("DeviceContext created for print(%x)\n",mPrt->mPrintDC);
            mPrt->mPrintDC->GetDevUnitsToTwips(a1);
            mPrt->mPrintDC->GetTwipsToDevUnits(a2);
            printf("    DevToTwips = %f TwipToDev = %f\n",a1,a2);
            mPrt->mPrintDC->GetAppUnitsToDevUnits(a1);
            mPrt->mPrintDC->GetDevUnitsToAppUnits(a2);
            printf("    AppUnitsToDev = %f DevUnitsToApp = %f\n",a1,a2);
            mPrt->mPrintDC->GetCanonicalPixelScale(a1);
            printf("    GetCanonicalPixelScale = %f\n",a1);
            mPrt->mPrintDC->GetScrollBarDimensions(a1, a2);
            printf("    ScrollBar x = %f y = %f\n",a1,a2);
            mPrt->mPrintDC->GetZoom(a1);
            printf("    Zoom = %f\n",a1);
            mPrt->mPrintDC->GetDepth((PRUint32&)i1);
            printf("    Depth = %d\n",i1);
            mPrt->mPrintDC->GetDeviceSurfaceDimensions(i1,i2);
            printf("    DeviceDimension w = %d h = %d\n",i1,i2);

#endif /* DEBUG_dcone */

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
            }

            // Get the Needed info for Calling PrepareDocument
            PRUnichar* fileName = nsnull;
            // check to see if we are printing to a file
            PRBool isPrintToFile = PR_FALSE;
            mPrt->mPrintSettings->GetPrintToFile(&isPrintToFile);
            if (isPrintToFile) {
              // On some platforms The PrepareDocument needs to know the name of the file
              // and it uses the PrintService to get it, so we need to set it into the PrintService here
              mPrt->mPrintSettings->GetToFileName(&fileName);
            }

            PRUnichar * docTitleStr;
            PRUnichar * docURLStr;

            GetDisplayTitleAndURL(mPrt->mPrintObject, mPrt->mPrintSettings, mPrt->mBrandName, &docTitleStr, &docURLStr, eDocTitleDefURLDoc); 
            PR_PL(("Title: %s\n", docTitleStr?NS_LossyConvertUCS2toASCII(docTitleStr).get():""));
            PR_PL(("URL:   %s\n", docURLStr?NS_LossyConvertUCS2toASCII(docURLStr).get():""));

            rv = mPrt->mPrintDC->PrepareDocument(docTitleStr, fileName);

            if (docTitleStr) nsMemory::Free(docTitleStr);
            if (docURLStr) nsMemory::Free(docURLStr);
            NS_ENSURE_SUCCESS(rv, rv);

            DoPrintProgress(PR_TRUE);

            // Print listener setup...
            if (mPrt != nsnull) {
              mPrt->OnStartPrinting();    
            }

            rv = DocumentReadyForPrinting();
            PR_PL(("PRINT JOB ENDING, OBSERVER WAS NOT CALLED\n"));
          }
        }
      }
    } else {
      mPrt->mPrintSettings->SetIsCancelled(PR_TRUE);
    }
  }

  /* cleaup on failure + notify user */
  if (NS_FAILED(rv)) {
    /* cleanup... */
    if (mPagePrintTimer) {
      mPagePrintTimer->Stop();
      NS_RELEASE(mPagePrintTimer);
    }

    if (mPrt) {
      delete mPrt;
      mPrt = nsnull;
    }
    mIsDoingPrinting = PR_FALSE;

    /* cleanup done, let's fire-up an error dialog to notify the user
     * what went wrong...
     *
     * When rv == NS_ERROR_ABORT, it means we want out of the
     * print job without displaying any error messages
     */
    if (rv != NS_ERROR_ABORT) {
      ShowPrintErrorDialog(rv);
    }
    PR_PL(("**** Printing Failed - rv 0x%X", rv));
  }

  return rv;
}


void
DocumentViewerImpl::ShowPrintErrorDialog(nsresult aPrintError, PRBool aIsPrinting)
{
  nsresult rv;

  static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
  nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(kCStringBundleServiceCID);

  if (!stringBundleService) {
    NS_WARNING("ERROR: Failed to get StringBundle Service instance.\n");
    return;
  }
  nsCOMPtr<nsIStringBundle> myStringBundle;
  rv = stringBundleService->CreateBundle(NS_ERROR_GFX_PRINTER_BUNDLE_URL, getter_AddRefs(myStringBundle));
  if (NS_FAILED(rv))
    return;

  nsXPIDLString msg,
                title;
  nsAutoString  stringName;

  switch(aPrintError)
  {
#define NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(nserr) case nserr: stringName = NS_LITERAL_STRING(#nserr); break;
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
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_GFX_PRINTER_NO_XUL)   // Temporary code for Bug 136185

    default:
      NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG(NS_ERROR_FAILURE)
#undef NS_ERROR_TO_LOCALIZED_PRINT_ERROR_MSG
  }

  PR_PL(("*******************************************\n"));
  PR_PL(("*** ShowPrintErrorDialog %s\n", NS_LossyConvertUCS2toASCII(stringName).get()));
  PR_PL(("*******************************************\n"));

  myStringBundle->GetStringFromName(stringName.get(), getter_Copies(msg));
  if (aIsPrinting) {
    myStringBundle->GetStringFromName(NS_LITERAL_STRING("print_error_dialog_title").get(), getter_Copies(title));
  } else {
    myStringBundle->GetStringFromName(NS_LITERAL_STRING("printpreview_error_dialog_title").get(), getter_Copies(title));
  }

  if (!msg)
    return;

  nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIDOMWindow> active;
  wwatch->GetActiveWindow(getter_AddRefs(active));

  nsCOMPtr<nsIDOMWindowInternal> parent = do_QueryInterface(active, &rv);
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIPrompt> dialog;
  parent->GetPrompter(getter_AddRefs(dialog));
  if (!dialog)
    return;

  dialog->Alert(title, msg);
}

// nsIContentViewerFile interface
NS_IMETHODIMP
DocumentViewerImpl::GetPrintable(PRBool *aPrintable)
{
  NS_ENSURE_ARG_POINTER(aPrintable);

  *aPrintable = !mIsDoingPrinting;

  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

//*****************************************************************************
// nsIMarkupDocumentViewer
//*****************************************************************************

NS_IMETHODIMP DocumentViewerImpl::ScrollToNode(nsIDOMNode* aNode)
{
   NS_ENSURE_ARG(aNode);
   NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(*(getter_AddRefs(presShell))), NS_ERROR_FAILURE);

   // Get the nsIContent interface, because that's what we need to
   // get the primary frame

   nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
   NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

   // Get the primary frame
   nsIFrame* frame;  // Remember Frames aren't ref-counted.  They are in their
                     // own special little world.

   NS_ENSURE_SUCCESS(presShell->GetPrimaryFrameFor(content, &frame),
      NS_ERROR_FAILURE);

   // tell the pres shell to scroll to the frame
   NS_ENSURE_SUCCESS(presShell->ScrollFrameIntoView(frame,
                                                    NS_PRESSHELL_SCROLL_TOP,
                                                    NS_PRESSHELL_SCROLL_ANYWHERE),
                     NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetAllowPlugins(PRBool* aAllowPlugins)
{
   NS_ENSURE_ARG_POINTER(aAllowPlugins);

   *aAllowPlugins = mAllowPlugins;
   return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetAllowPlugins(PRBool aAllowPlugins)
{
   mAllowPlugins = aAllowPlugins;
   return NS_OK;
}

nsresult
DocumentViewerImpl::CallChildren(CallChildFunc aFunc, void* aClosure)
{
  nsCOMPtr<nsIDocShellTreeNode> docShellNode(do_QueryInterface(mContainer));
  if (docShellNode)
  {
    PRInt32 i;
    PRInt32 n;
    docShellNode->GetChildCount(&n);
    for (i=0; i < n; i++)
    {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      NS_ASSERTION(childAsShell, "null child in docshell");
      if (childAsShell)
      {
        nsCOMPtr<nsIContentViewer> childCV;
        childAsShell->GetContentViewer(getter_AddRefs(childCV));
        if (childCV)
        {
          nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
          if (markupCV) {
            (*aFunc)(markupCV, aClosure);
          }
        }
      }
    }
  }
  return NS_OK;
}

struct TextZoomInfo
{
  float mTextZoom;
};

static void
SetChildTextZoom(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  struct TextZoomInfo* textZoomInfo = (struct TextZoomInfo*) aClosure;
  aChild->SetTextZoom(textZoomInfo->mTextZoom);
}

NS_IMETHODIMP DocumentViewerImpl::SetTextZoom(float aTextZoom)
{
  if (mDeviceContext) {
    mDeviceContext->SetTextZoom(aTextZoom);
    if (mPresContext) {
      mPresContext->ClearStyleDataAndReflow();
    }
  }

  // now set the text zoom on all children of mContainer
  struct TextZoomInfo textZoomInfo = { aTextZoom };
  return CallChildren(SetChildTextZoom, &textZoomInfo);
}

NS_IMETHODIMP DocumentViewerImpl::GetTextZoom(float* aTextZoom)
{
  NS_ENSURE_ARG_POINTER(aTextZoom);

  if (mDeviceContext) {
    return mDeviceContext->GetTextZoom(*aTextZoom);
  }

  *aTextZoom = 1.0;
  return NS_OK;
}

// XXX: SEMANTIC CHANGE!
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aDefaultCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetDefaultCharacterSet(PRUnichar** aDefaultCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aDefaultCharacterSet);
  NS_ENSURE_STATE(mContainer);

  if (mDefaultCharacterSet.IsEmpty())
  {
    nsXPIDLString defCharset;

    nsCOMPtr<nsIWebShell> webShell;
    webShell = do_QueryInterface(mContainer);
    if (webShell)
    {
      nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
      if (prefs)
        prefs->GetLocalizedUnicharPref("intl.charset.default", getter_Copies(defCharset));
    }

    if (!defCharset.IsEmpty())
      mDefaultCharacterSet.Assign(defCharset.get());
    else
      mDefaultCharacterSet.Assign(NS_LITERAL_STRING("ISO-8859-1"));
  }
  *aDefaultCharacterSet = ToNewUnicode(mDefaultCharacterSet);
  return NS_OK;
}

static void
SetChildDefaultCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetDefaultCharacterSet((PRUnichar*) aClosure);
}

NS_IMETHODIMP DocumentViewerImpl::SetDefaultCharacterSet(const PRUnichar* aDefaultCharacterSet)
{
  mDefaultCharacterSet = aDefaultCharacterSet;  // this does a copy of aDefaultCharacterSet
  // now set the default char set on all children of mContainer
  return CallChildren(SetChildDefaultCharacterSet,
                      (void*) aDefaultCharacterSet);
}

// XXX: SEMANTIC CHANGE!
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aForceCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetForceCharacterSet(PRUnichar** aForceCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aForceCharacterSet);

  nsAutoString emptyStr;
  if (mForceCharacterSet.Equals(emptyStr)) {
    *aForceCharacterSet = nsnull;
  }
  else {
    *aForceCharacterSet = ToNewUnicode(mForceCharacterSet);
  }
  return NS_OK;
}

static void
SetChildForceCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetForceCharacterSet((PRUnichar*) aClosure);
}

NS_IMETHODIMP DocumentViewerImpl::SetForceCharacterSet(const PRUnichar* aForceCharacterSet)
{
  mForceCharacterSet = aForceCharacterSet;
  // now set the force char set on all children of mContainer
  return CallChildren(SetChildForceCharacterSet, (void*) aForceCharacterSet);
}

// XXX: SEMANTIC CHANGE!
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aHintCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetHintCharacterSet(PRUnichar * *aHintCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aHintCharacterSet);

  if(kCharsetUninitialized == mHintCharsetSource) {
    *aHintCharacterSet = nsnull;
  } else {
    *aHintCharacterSet = ToNewUnicode(mHintCharset);
    // this can't possibly be right.  we can't set a value just because somebody got a related value!
    //mHintCharsetSource = kCharsetUninitialized;
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetHintCharacterSetSource(PRInt32 *aHintCharacterSetSource)
{
  NS_ENSURE_ARG_POINTER(aHintCharacterSetSource);

  *aHintCharacterSetSource = mHintCharsetSource;
  return NS_OK;
}

static void
SetChildHintCharacterSetSource(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetHintCharacterSetSource(NS_PTR_TO_INT32(aClosure));
}

NS_IMETHODIMP DocumentViewerImpl::SetHintCharacterSetSource(PRInt32 aHintCharacterSetSource)
{
  mHintCharsetSource = aHintCharacterSetSource;
  // now set the hint char set source on all children of mContainer
  return CallChildren(SetChildHintCharacterSetSource,
                      (void*) aHintCharacterSetSource);
}

static void
SetChildHintCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetHintCharacterSet((PRUnichar*) aClosure);
}

NS_IMETHODIMP DocumentViewerImpl::SetHintCharacterSet(const PRUnichar* aHintCharacterSet)
{
  mHintCharset = aHintCharacterSet;
  // now set the hint char set on all children of mContainer
  return CallChildren(SetChildHintCharacterSet, (void*) aHintCharacterSet);
}

#ifdef IBMBIDI
static void
SetChildBidiOptions(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetBidiOptions(NS_PTR_TO_INT32(aClosure));
}

#endif // IBMBIDI

NS_IMETHODIMP DocumentViewerImpl::SetBidiTextDirection(PRUint8 aTextDirection)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_DIRECTION(bidiOptions, aTextDirection);
  SetBidiOptions(bidiOptions);
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiTextDirection(PRUint8* aTextDirection)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  if (aTextDirection) {
    GetBidiOptions(&bidiOptions);
    *aTextDirection = GET_BIDI_OPTION_DIRECTION(bidiOptions);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiTextType(PRUint8 aTextType)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_TEXTTYPE(bidiOptions, aTextType);
  SetBidiOptions(bidiOptions);
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiTextType(PRUint8* aTextType)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  if (aTextType) {
    GetBidiOptions(&bidiOptions);
    *aTextType = GET_BIDI_OPTION_TEXTTYPE(bidiOptions);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiControlsTextMode(PRUint8 aControlsTextMode)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_CONTROLSTEXTMODE(bidiOptions, aControlsTextMode);
  SetBidiOptions(bidiOptions);
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiControlsTextMode(PRUint8* aControlsTextMode)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  if (aControlsTextMode) {
    GetBidiOptions(&bidiOptions);
    *aControlsTextMode = GET_BIDI_OPTION_CONTROLSTEXTMODE(bidiOptions);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiClipboardTextMode(PRUint8 aClipboardTextMode)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_CLIPBOARDTEXTMODE(bidiOptions, aClipboardTextMode);
  SetBidiOptions(bidiOptions);
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiClipboardTextMode(PRUint8* aClipboardTextMode)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  if (aClipboardTextMode) {
    GetBidiOptions(&bidiOptions);
    *aClipboardTextMode = GET_BIDI_OPTION_CLIPBOARDTEXTMODE(bidiOptions);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiNumeral(PRUint8 aNumeral)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_NUMERAL(bidiOptions, aNumeral);
  SetBidiOptions(bidiOptions);
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiNumeral(PRUint8* aNumeral)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  if (aNumeral) {
    GetBidiOptions(&bidiOptions);
    *aNumeral = GET_BIDI_OPTION_NUMERAL(bidiOptions);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiSupport(PRUint8 aSupport)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_SUPPORT(bidiOptions, aSupport);
  SetBidiOptions(bidiOptions);
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiSupport(PRUint8* aSupport)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  if (aSupport) {
    GetBidiOptions(&bidiOptions);
    *aSupport = GET_BIDI_OPTION_SUPPORT(bidiOptions);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiCharacterSet(PRUint8 aCharacterSet)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_CHARACTERSET(bidiOptions, aCharacterSet);
  SetBidiOptions(bidiOptions);
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiCharacterSet(PRUint8* aCharacterSet)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;

  if (aCharacterSet) {
    GetBidiOptions(&bidiOptions);
    *aCharacterSet = GET_BIDI_OPTION_CHARACTERSET(bidiOptions);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiOptions(PRUint32 aBidiOptions)
{
#ifdef IBMBIDI
  if (mPresContext) {
#if 1
    // forcing reflow will cause bug 80352. Temp turn off force reflow and
    // wait for simon@softel.co.il to find the real solution
    mPresContext->SetBidi(aBidiOptions, PR_FALSE);
#else
    mPresContext->SetBidi(aBidiOptions, PR_TRUE); // force reflow
#endif
  }
  // now set bidi on all children of mContainer
  CallChildren(SetChildBidiOptions, (void*) aBidiOptions);
#endif // IBMBIDI
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiOptions(PRUint32* aBidiOptions)
{
#ifdef IBMBIDI
  if (aBidiOptions) {
    if (mPresContext) {
      mPresContext->GetBidi(aBidiOptions);
    }
    else
      *aBidiOptions = IBMBIDI_DEFAULT_BIDI_OPTIONS;
  }
#endif // IBMBIDI
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SizeToContent()
{
   NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

   nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mContainer));
   NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDocShellTreeItem> docShellParent;
   docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

   // It's only valid to access this from a top frame.  Doesn't work from
   // sub-frames.
   NS_ENSURE_TRUE(!docShellParent, NS_ERROR_FAILURE);

   nsCOMPtr<nsIPresShell> presShell;
   GetPresShell(*getter_AddRefs(presShell));
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE,
      NS_UNCONSTRAINEDSIZE), NS_ERROR_FAILURE);

   nsCOMPtr<nsIPresContext> presContext;
   GetPresContext(*getter_AddRefs(presContext));
   NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

   nsRect  shellArea;
   PRInt32 width, height;
   float   pixelScale;

   // so how big is it?
   presContext->GetVisibleArea(shellArea);
   presContext->GetTwipsToPixels(&pixelScale);
   width = PRInt32((float)shellArea.width*pixelScale);
   height = PRInt32((float)shellArea.height*pixelScale);

   nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
   docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
   NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

   /* presContext's size was calculated in twips and has already been
      rounded to the equivalent pixels (so the width/height calculation
      we just performed was probably exact, though it was based on
      values already rounded during ResizeReflow). In a surprising
      number of instances, this rounding makes a window which for want
      of one extra pixel's width ends up wrapping the longest line of
      text during actual window layout. This makes the window too short,
      generally clipping the OK/Cancel buttons. Here we add one pixel
      to the calculated width, to circumvent this problem. */
   NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, width+1, height),
      NS_ERROR_FAILURE);

   return NS_OK;
}



#ifdef XP_MAC
#pragma mark -
#endif

NS_IMPL_ISUPPORTS1(nsDocViewerSelectionListener, nsISelectionListener);

nsresult nsDocViewerSelectionListener::Init(DocumentViewerImpl *aDocViewer)
{
  mDocViewer = aDocViewer;
  return NS_OK;
}

/*
 * GetPopupNode, GetPopupLinkNode and GetPopupImageNode are helpers
 * for the cmd_copyLink / cmd_copyImageLocation / cmd_copyImageContents family
 * of commands. The focus controller stores the popup node, these retrieve
 * them and munge appropriately. Note that we have to store the popup node
 * rather than retrieving it from EventStateManager::GetFocusedContent because
 * not all content (images included) can receive focus.
 */

nsresult
DocumentViewerImpl::GetPopupNode(nsIDOMNode** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  nsresult rv;

  // get the document
  nsCOMPtr<nsIDocument> document;
  rv = GetDocument(*getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  // get the script global object
  nsCOMPtr<nsIScriptGlobalObject> global;
  rv = document->GetScriptGlobalObject(getter_AddRefs(global));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);

  // get the internal dom window
  nsCOMPtr<nsIDOMWindowInternal> internalWin(do_QueryInterface(global, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(internalWin, NS_ERROR_FAILURE);

  // get the private dom window
  nsCOMPtr<nsPIDOMWindow> privateWin(do_QueryInterface(internalWin, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(privateWin, NS_ERROR_FAILURE);

  // get the focus controller
  nsCOMPtr<nsIFocusController> focusController;
  rv = privateWin->GetRootFocusController(getter_AddRefs(focusController));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);

  // get the popup node
  rv = focusController->GetPopupNode(aNode); // addref happens here
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

// GetPopupLinkNode: return popup link node or fail
nsresult
DocumentViewerImpl::GetPopupLinkNode(nsIDOMNode** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  // you get null unless i say so
  *aNode = nsnull;

  // find popup node
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);

  // find out if we have a link in our ancestry
  while (node) {

    // are we an anchor?
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(node));
    nsCOMPtr<nsIDOMHTMLAreaElement> area;
    nsCOMPtr<nsIDOMHTMLLinkElement> link;
    nsAutoString xlinkType;
    if (!anchor) {
      // area?
      area = do_QueryInterface(node);
      if (!area) {
        // link?
        link = do_QueryInterface(node);
        if (!link) {
          // XLink?
          nsCOMPtr<nsIDOMElement> element(do_QueryInterface(node));
          if (element) {
            element->GetAttributeNS(NS_LITERAL_STRING("http://www.w3.org/1999/xlink"),NS_LITERAL_STRING("type"),xlinkType);
          }
        }
      }
    }
    if (anchor || area || link || xlinkType.Equals(NS_LITERAL_STRING("simple"))) {
      *aNode = node;
      NS_IF_ADDREF(*aNode); // addref
      return NS_OK;
    }
    else {
      // if not, get our parent and keep trying...
      nsCOMPtr<nsIDOMNode> parentNode;
      node->GetParentNode(getter_AddRefs(parentNode));
      node = parentNode;
    }
  }

  // if we have no node, fail
  return NS_ERROR_FAILURE;
}

// GetPopupLinkNode: return popup image node or fail
nsresult
DocumentViewerImpl::GetPopupImageNode(nsIDOMNode** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  // you get null unless i say so
  *aNode = nsnull;

  // find popup node
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX find out if we're an image. this really ought to look for objects
  // XXX with type "image/...", but this is good enough for now.
  nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(node, &rv));
  if (NS_FAILED(rv)) return rv;
  NS_ENSURE_TRUE(img, NS_ERROR_FAILURE);

  // if we made it here, we're an image.
  *aNode = node;
  NS_IF_ADDREF(*aNode); // addref
  return NS_OK;
}

/*
 * XXX dr
 * ------
 * These two functions -- GetInLink and GetInImage -- are kind of annoying
 * in that they only get called from the controller (in
 * nsDOMWindowController::IsCommandEnabled). The actual construction of the
 * context menus in communicator (nsContextMenu.js) has its own, redundant
 * tests. No big deal, but good to keep in mind if we ever clean context
 * menus.
 */

NS_IMETHODIMP DocumentViewerImpl::GetInLink(PRBool* aInLink)
{
#ifdef DEBUG_dr
  printf("dr :: DocumentViewerImpl::GetInLink\n");
#endif

  NS_ENSURE_ARG_POINTER(aInLink);

  // we're not in a link unless i say so
  *aInLink = PR_FALSE;

  // get the popup link
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupLinkNode(getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // if we made it here, we're in a link
  *aInLink = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetInImage(PRBool* aInImage)
{
#ifdef DEBUG_dr
  printf("dr :: DocumentViewerImpl::GetInImage\n");
#endif

  NS_ENSURE_ARG_POINTER(aInImage);

  // we're not in an image unless i say so
  *aInImage = PR_FALSE;

  // get the popup image
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupImageNode(getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // if we made it here, we're in an image
  *aInImage = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsDocViewerSelectionListener::NotifySelectionChanged(nsIDOMDocument *, nsISelection *, short)
{
  NS_ASSERTION(mDocViewer, "Should have doc viewer!");

  // get the selection state
  nsCOMPtr<nsISelection> selection;
  nsresult rv = mDocViewer->GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  PRBool selectionCollapsed;
  selection->GetIsCollapsed(&selectionCollapsed);
  // we only call UpdateCommands when the selection changes from collapsed
  // to non-collapsed or vice versa. We might need another update string
  // for simple selection changes, but that would be expenseive.
  if (!mGotSelectionState || mSelectionWasCollapsed != selectionCollapsed)
  {
    nsCOMPtr<nsIDocument> theDoc;
    mDocViewer->GetDocument(*getter_AddRefs(theDoc));
    if (!theDoc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
    theDoc->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));

    nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(scriptGlobalObject);
    if (!domWindow) return NS_ERROR_FAILURE;

    domWindow->UpdateCommands(NS_LITERAL_STRING("select"));
    mGotSelectionState = PR_TRUE;
    mSelectionWasCollapsed = selectionCollapsed;
  }

  return NS_OK;
}

//nsDocViewerFocusListener
NS_IMPL_ISUPPORTS1(nsDocViewerFocusListener, nsIDOMFocusListener);

nsDocViewerFocusListener::nsDocViewerFocusListener()
:mDocViewer(nsnull)
{
  NS_INIT_REFCNT();
}

nsDocViewerFocusListener::~nsDocViewerFocusListener(){}

nsresult
nsDocViewerFocusListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDocViewerFocusListener::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIPresShell> shell;
  if(!mDocViewer)
    return NS_ERROR_FAILURE;

  nsresult result = mDocViewer->GetPresShell(*getter_AddRefs(shell));//deref once cause it take a ptr ref
  if(NS_FAILED(result) || !shell)
    return result?result:NS_ERROR_FAILURE;
  nsCOMPtr<nsISelectionController> selCon;
  selCon = do_QueryInterface(shell);
  PRInt16 selectionStatus;
  selCon->GetDisplaySelection( &selectionStatus);

  //if selection was nsISelectionController::SELECTION_OFF, do nothing
  //otherwise re-enable it.
  if(selectionStatus == nsISelectionController::SELECTION_DISABLED ||
     selectionStatus == nsISelectionController::SELECTION_HIDDEN)
  {
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
    selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  }
  return result;
}

NS_IMETHODIMP
nsDocViewerFocusListener::Blur(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIPresShell> shell;
  if(!mDocViewer)
    return NS_ERROR_FAILURE;

  nsresult result = mDocViewer->GetPresShell(*getter_AddRefs(shell));//deref once cause it take a ptr ref
  if(NS_FAILED(result) || !shell)
    return result?result:NS_ERROR_FAILURE;
  nsCOMPtr<nsISelectionController> selCon;
  selCon = do_QueryInterface(shell);
  PRInt16 selectionStatus;
  selCon->GetDisplaySelection(&selectionStatus);

  //if selection was nsISelectionController::SELECTION_OFF, do nothing
  //otherwise re-enable it.
  if(selectionStatus == nsISelectionController::SELECTION_ON)
  {
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
    selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  }
  return result;
}


nsresult
nsDocViewerFocusListener::Init(DocumentViewerImpl *aDocViewer)
{
  mDocViewer = aDocViewer;
  return NS_OK;
}


PRBool
DocumentViewerImpl::IsWindowsInOurSubTree(nsIDOMWindowInternal * aDOMWindow)
{
  PRBool found = PR_FALSE;
  if(aDOMWindow) {
    // now check to make sure it is in "our" tree of webshells
    nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(aDOMWindow));
    if (scriptObj) {
      nsCOMPtr<nsIDocShell> docShell;
      scriptObj->GetDocShell(getter_AddRefs(docShell));
      if (docShell) {
        nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
        if (docShellAsItem) {
          // get this DocViewer webshell
          nsCOMPtr<nsIWebShell> thisDVWebShell(do_QueryInterface(mContainer));
          while (!found) {
            nsCOMPtr<nsIDocShellTreeItem> docShellParent;
            docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));
            if (docShellParent) {
              nsCOMPtr<nsIWebShell> parentWebshell(do_QueryInterface(docShellParent));
              if (parentWebshell) {
                if (parentWebshell.get() == thisDVWebShell.get()) {
                  found = PR_TRUE;
                  break;
                }
              }
            } else {
              break; // at top of tree
            }
            docShellAsItem = docShellParent;
          } // while
        }
      } // docshell
    } // scriptobj
  } // domWindow

  return found;
}


//----------------------------------------------------------------------------------
void
DocumentViewerImpl::CleanupDocTitleArray(PRUnichar**& aArray, PRInt32& aCount)
{
  for (PRInt32 i = aCount - 1; i >= 0; i--) {
    nsMemory::Free(aArray[i]);
  }
  nsMemory::Free(aArray);
  aArray = NULL;
  aCount = 0;
}

//----------------------------------------------------------------------------------
// Enumerate all the documents for their titles
NS_IMETHODIMP
DocumentViewerImpl::EnumerateDocumentNames(PRUint32* aCount,
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
    PrintObject* po = (PrintObject*)mPrt->mPrintDocList->ElementAt(i);
    NS_ASSERTION(po, "PrintObject can't be null!");
    PRUnichar * docTitleStr;
    PRUnichar * docURLStr;
    GetWebShellTitleAndURL(po->mWebShell, &docTitleStr, &docURLStr);

    // Use the URL if the doc is empty
    if (!docTitleStr || !*docTitleStr) {
      if (docURLStr && nsCRT::strlen(docURLStr) > 0) {
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

/* readonly attribute nsIPrintSettings globalPrintSettings; */
NS_IMETHODIMP
DocumentViewerImpl::GetGlobalPrintSettings(nsIPrintSettings * *aGlobalPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aGlobalPrintSettings);

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPrintSettingsService> printSettingsService = do_GetService(sPrintSettingsServiceContractID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = printSettingsService->GetGlobalPrintSettings(aGlobalPrintSettings);
  }
  return rv;
}

static void
GetParentWebBrowserPrint(nsISupports *aContainer, nsIWebBrowserPrint **aParent)
{
  *aParent = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(aContainer));

  if (item) {
    nsCOMPtr<nsIDocShellTreeItem> parent;
    item->GetParent(getter_AddRefs(parent));

    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(parent));

    if (docShell) {
      nsCOMPtr<nsIContentViewer> viewer;
      docShell->GetContentViewer(getter_AddRefs(viewer));

      if (viewer) {
        CallQueryInterface(viewer, aParent);
      }
    }
  }
}

/* readonly attribute boolean doingPrint; */
NS_IMETHODIMP
DocumentViewerImpl::GetDoingPrint(PRBool *aDoingPrint)
{
  NS_ENSURE_ARG_POINTER(aDoingPrint);
  *aDoingPrint = mIsDoingPrinting;

  if (!*aDoingPrint) {
    nsCOMPtr<nsIWebBrowserPrint> wbp;
    GetParentWebBrowserPrint(mContainer, getter_AddRefs(wbp));

    if (wbp) {
      return wbp->GetDoingPrint(aDoingPrint);
    }
  }

  return NS_OK;
}

/* readonly attribute boolean doingPrintPreview; */
NS_IMETHODIMP
DocumentViewerImpl::GetDoingPrintPreview(PRBool *aDoingPrintPreview)
{
  NS_ENSURE_ARG_POINTER(aDoingPrintPreview);
  *aDoingPrintPreview = mIsDoingPrintPreview;

  if (!*aDoingPrintPreview) {
    nsCOMPtr<nsIWebBrowserPrint> wbp;
    GetParentWebBrowserPrint(mContainer, getter_AddRefs(wbp));

    if (wbp) {
      return wbp->GetDoingPrintPreview(aDoingPrintPreview);
    }
  }

  return NS_OK;
}

/* readonly attribute nsIPrintSettings currentPrintSettings; */
NS_IMETHODIMP
DocumentViewerImpl::GetCurrentPrintSettings(nsIPrintSettings * *aCurrentPrintSettings)
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

/* readonly attribute nsIDOMWindow currentChildDOMWindow; */
NS_IMETHODIMP 
DocumentViewerImpl::GetCurrentChildDOMWindow(nsIDOMWindow * *aCurrentChildDOMWindow)
{
  NS_ENSURE_ARG_POINTER(aCurrentChildDOMWindow);
  *aCurrentChildDOMWindow = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancel (); */
NS_IMETHODIMP
DocumentViewerImpl::Cancel()
{
  if (mPrt && mPrt->mPrintSettings) {
    return mPrt->mPrintSettings->SetIsCancelled(PR_TRUE);
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  Get the Focused Frame for a documentviewer
 *
 */
nsIDOMWindowInternal*
DocumentViewerImpl::FindFocusedDOMWindowInternal()
{
  nsCOMPtr<nsIDOMWindowInternal>  theDOMWin;
  nsCOMPtr<nsIDocument>           theDoc;
  nsCOMPtr<nsIScriptGlobalObject> theSGO;
  nsCOMPtr<nsIFocusController>    focusController;
  nsIDOMWindowInternal *          domWin = nsnull;

  this->GetDocument(*getter_AddRefs(theDoc));
  if(theDoc){
    theDoc->GetScriptGlobalObject(getter_AddRefs(theSGO));
    if(theSGO){
      nsCOMPtr<nsPIDOMWindow> theDOMWindow = do_QueryInterface(theSGO);
      if(theDOMWindow){
        theDOMWindow->GetRootFocusController(getter_AddRefs(focusController));
        if(focusController){
          focusController->GetFocusedWindow(getter_AddRefs(theDOMWin));
          domWin = theDOMWin.get();
          if(domWin != nsnull) {
            if (IsWindowsInOurSubTree(domWin)){
              NS_ADDREF(domWin);
            } else {
              domWin = nsnull;
            }
          }
        }
      }
    }
  }
  return domWin;
}

/*=============== Timer Related Code ======================*/
nsresult
DocumentViewerImpl::StartPagePrintTimer(nsIPresContext * aPresContext,
                                        nsIPrintSettings* aPrintSettings,
                                        PrintObject*     aPOect,
                                        PRUint32         aDelay)
{
  nsresult result;

  if (!mPagePrintTimer) {
    result = NS_NewUpdateTimer(&mPagePrintTimer);

    if (NS_FAILED(result))
      return result;

    ++mDestroyRefCount;
  }

  return mPagePrintTimer->Start(this, aPresContext, aPrintSettings, aPOect, aDelay);
}

