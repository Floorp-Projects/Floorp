/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsIPresShell.h"
#include "nsIPresContext.h" 
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIStyleSet.h"
#include "nsICSSStyleSheet.h" // XXX for UA sheet loading hack, can this go away please?
#include "nsIStyleContext.h"
#include "nsIServiceManager.h"
#include "nsFrame.h"
#include "nsIReflowCommand.h"
#include "nsIViewManager.h"
#include "nsCRT.h"
#include "prlog.h"
#include "prinrval.h"
#include "nsVoidArray.h"
#include "nsIPref.h"
#include "nsIViewObserver.h"
#include "nsContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsHTMLParts.h"
#include "nsIDOMSelection.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsHTMLAtoms.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsICaret.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIXMLDocument.h"
#include "nsIScrollableView.h"
#include "nsIDOMSelectionListener.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"
#include "nsXIFDTD.h"
#include "nsIFrameSelection.h"
#include "nsViewsCID.h"
#include "nsIFrameManager.h"
#include "nsISupportsPrimitives.h"

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIFormatConverter.h"
#include "nsIWebShell.h"
#include "nsIBrowserWindow.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,           NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCXIFConverterCID,        NS_XIFFORMATCONVERTER_CID);

static PRBool gsNoisyRefs = PR_FALSE;
#undef NOISY

// comment out to hide caret
#define SHOW_CARET

//----------------------------------------------------------------------

// Class IID's
static NS_DEFINE_IID(kFrameSelectionCID, NS_FRAMESELECTION_CID);
static NS_DEFINE_IID(kCRangeCID, NS_RANGE_CID);

// IID's
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPresShellIID, NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);
static NS_DEFINE_IID(kIViewObserverIID, NS_IVIEWOBSERVER_IID);
static NS_DEFINE_IID(kIFrameSelectionIID, NS_IFRAMESELECTION_IID);
static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMRangeIID, NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIFocusTrackerIID, NS_IFOCUSTRACKER_IID);
static NS_DEFINE_IID(kIDomSelectionListenerIID, NS_IDOMSELECTIONLISTENER_IID);
static NS_DEFINE_IID(kICaretIID, NS_ICARET_IID);
static NS_DEFINE_IID(kICaretID,  NS_ICARET_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

class PresShell : public nsIPresShell, public nsIViewObserver,
                  private nsIDocumentObserver, public nsIFocusTracker,
                  public nsIDOMSelectionListener, public nsSupportsWeakReference
{
public:
  PresShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIPresShell
  NS_IMETHOD Init(nsIDocument* aDocument,
                  nsIPresContext* aPresContext,
                  nsIViewManager* aViewManager,
                  nsIStyleSet* aStyleSet);
  NS_IMETHOD GetDocument(nsIDocument** aResult);
  NS_IMETHOD GetPresContext(nsIPresContext** aResult);
  NS_IMETHOD GetViewManager(nsIViewManager** aResult);
  NS_IMETHOD GetStyleSet(nsIStyleSet** aResult);
  NS_IMETHOD GetActiveAlternateStyleSheet(nsString& aSheetTitle);
  NS_IMETHOD SelectAlternateStyleSheet(const nsString& aSheetTitle);
  NS_IMETHOD ListAlternateStyleSheets(nsStringArray& aTitleList);
  NS_IMETHOD GetSelection(SelectionType aType, nsIDOMSelection** aSelection);
  NS_IMETHOD GetFrameSelection(nsIFrameSelection** aSelection);

  NS_IMETHOD EnterReflowLock();
  NS_IMETHOD ExitReflowLock();
  NS_IMETHOD BeginObservingDocument();
  NS_IMETHOD EndObservingDocument();
  NS_IMETHOD InitialReflow(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD ResizeReflow(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD StyleChangeReflow();
  NS_IMETHOD GetRootFrame(nsIFrame** aFrame) const;
  NS_IMETHOD GetPageSequenceFrame(nsIPageSequenceFrame** aResult) const;
  NS_IMETHOD GetPrimaryFrameFor(nsIContent* aContent,
                                nsIFrame**  aPrimaryFrame) const;
  NS_IMETHOD GetStyleContextFor(nsIFrame*         aFrame,
                                nsIStyleContext** aStyleContext) const;
  NS_IMETHOD GetLayoutObjectFor(nsIContent*   aContent,
                                nsISupports** aResult) const;
  NS_IMETHOD GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                    nsIFrame** aPlaceholderFrame) const;
  NS_IMETHOD AppendReflowCommand(nsIReflowCommand* aReflowCommand);
  NS_IMETHOD CancelReflowCommand(nsIFrame* aTargetFrame);
  NS_IMETHOD ProcessReflowCommands();
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame);
  NS_IMETHOD CreateRenderingContext(nsIFrame *aFrame,
                                    nsIRenderingContext** aContext);
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);
  NS_IMETHOD GoToAnchor(const nsString& aAnchorName) const;

  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame,
                                 PRIntn   aVPercent, 
                                 PRIntn   aHPercent) const;

  NS_IMETHOD NotifyDestroyingFrame(nsIFrame* aFrame);
  
  NS_IMETHOD GetFrameManager(nsIFrameManager** aFrameManager) const;

  NS_IMETHOD DoCopy();
 
  //nsIViewObserver interface

  NS_IMETHOD Paint(nsIView *aView,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD HandleEvent(nsIView*        aView,
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
  NS_IMETHOD Scrolled(nsIView *aView);
  NS_IMETHOD ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight);

  //nsIFocusTracker interface
  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame);
  // caret handling
  NS_IMETHOD GetCaret(nsICaret **aOutCaret);
  NS_IMETHOD SetCaretEnabled(PRBool aaInEnable);
  NS_IMETHOD GetCaretEnabled(PRBool *aOutEnabled);

  NS_IMETHOD SetDisplayNonTextSelection(PRBool aaInEnable);
  NS_IMETHOD GetDisplayNonTextSelection(PRBool *aOutEnable);

  // nsIDOMSelectionListener interface
  NS_IMETHOD NotifySelectionChanged();

  // nsIDocumentObserver
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument);
  NS_IMETHOD EndUpdate(nsIDocument *aDocument);
  NS_IMETHOD BeginLoad(nsIDocument *aDocument);
  NS_IMETHOD EndLoad(nsIDocument *aDocument);
  NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell);
  NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell);
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                            nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2);
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled);
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint);
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument);

protected:
  virtual ~PresShell();

  nsresult ReconstructFrames(void);

  // turn the caret on and off.
  nsresult RefreshCaret(nsIView *aView,
  									nsIRenderingContext& aRendContext,
  									const nsRect& aDirtyRect);
  nsresult SuspendCaret();
  nsresult ResumeCaret();
   
  PRBool	mCaretEnabled;
  
#ifdef NS_DEBUG
  nsresult CloneStyleSet(nsIStyleSet* aSet, nsIStyleSet** aResult);
  void VerifyIncrementalReflow();
  PRBool mInVerifyReflow;
#endif

  // IMPORTANT: The ownership implicit in the following member variables has been 
  // explicitly checked and set using nsCOMPtr for owning pointers and raw COM interface 
  // pointers for weak (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).

  // these are the same Document and PresContext owned by the DocViewer.
  // we must share ownership.
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIStyleSet> mStyleSet;
  nsIFrame* mRootFrame;
  nsIViewManager* mViewManager;   // [WEAK] docViewer owns it so I don't have to
  PRUint32 mUpdateCount;
  nsVoidArray mReflowCommands;
  PRUint32 mReflowLockCount;
  PRBool mIsDestroying;
  nsIFrame* mCurrentEventFrame;
  nsIContent* mCurrentEventContent;
  
  nsCOMPtr<nsIFrameSelection>   mSelection;
  nsCOMPtr<nsICaret>            mCaret;
  PRBool                        mDisplayNonTextSelection;
  PRBool                        mScrollingEnabled; //used to disable programmable scrolling from outside
  nsIFrameManager*              mFrameManager;  // we hold a reference

private:
  //helper funcs for disabing autoscrolling
  void DisableScrolling(){mScrollingEnabled = PR_FALSE;}
  void EnableScrolling(){mScrollingEnabled = PR_TRUE;}
  PRBool IsScrollingEnabled(){return mScrollingEnabled;}
  nsIFrame* GetCurrentEventFrame();
};

#ifdef NS_DEBUG
/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule;
#endif

static PRBool gVerifyReflow = PRBool(0x55);
#ifdef NS_DEBUG
static PRBool gVerifyReflowAll;
#endif

NS_LAYOUT PRBool
nsIPresShell::GetVerifyReflowEnable()
{
#ifdef NS_DEBUG
  if (gVerifyReflow == PRBool(0x55)) {
    gLogModule = PR_NewLogModule("verifyreflow");
    gVerifyReflow = 0 != gLogModule->level;
    if (gLogModule->level > 1) {
      gVerifyReflowAll = PR_TRUE;
    }
    printf("Note: verifyreflow is %sabled",
           gVerifyReflow ? "en" : "dis");
    if (gVerifyReflowAll) {
      printf(" (diff all enabled)\n");
    }
    else {
      printf("\n");
    }
  }
#endif
  return gVerifyReflow;
}

NS_LAYOUT void
nsIPresShell::SetVerifyReflowEnable(PRBool aEnabled)
{
  gVerifyReflow = aEnabled;
}

//----------------------------------------------------------------------

NS_LAYOUT nsresult
NS_NewPresShell(nsIPresShell** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PresShell* it = new PresShell();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIPresShellIID, (void **) aInstancePtrResult);
}

PresShell::PresShell()
{
  //XXX joki 11/17 - temporary event hack.
  mIsDestroying = PR_FALSE;
  mCaretEnabled = PR_FALSE;
  mDisplayNonTextSelection = PR_FALSE;
  mCurrentEventContent = nsnull;
  EnableScrolling();
}

#ifdef NS_DEBUG
// for debugging only
nsrefcnt PresShell::AddRef(void)
{
  if (gsNoisyRefs) {
    printf("PresShell: AddRef: %p, cnt = %d \n",this, mRefCnt+1);
  }
  return ++mRefCnt;
}

// for debugging only
nsrefcnt PresShell::Release(void)
{
  if (gsNoisyRefs) {
    printf("PresShell Release: %p, cnt = %d \n",this, mRefCnt-1);
  }
  if (--mRefCnt == 0) {
    if (gsNoisyRefs) {
      printf("PresShell Delete: %p, \n",this);
    }
    delete this;
    return 0;
  }
  return mRefCnt;
}
#else
NS_IMPL_ADDREF(PresShell)
NS_IMPL_RELEASE(PresShell)
#endif

nsresult
PresShell::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (aIID.Equals(kIPresShellIID)) {
    nsIPresShell* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDocumentObserverIID)) {
    nsIDocumentObserver* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIViewObserverIID)) {
    nsIViewObserver* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIFocusTrackerIID)) {
    nsIFocusTracker* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDomSelectionListenerIID)) {
    nsIDOMSelectionListener* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupportsWeakReference>::GetIID())) {
    nsISupportsWeakReference* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIPresShell* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

PresShell::~PresShell()
{
  mRefCnt = 99;/* XXX hack! get around re-entrancy bugs */

  mIsDestroying = PR_TRUE;

  NS_IF_RELEASE(mCurrentEventContent);
  if (mViewManager) {
    // Disable paints during tear down of the frame tree
    mViewManager->DisableRefresh();
  }
  // Destroy the frame manager before destroying the frame hierarchy. That way
  // we won't waste time removing content->frame mappings for frames being
  // destroyed
  NS_IF_RELEASE(mFrameManager);
  if (mRootFrame)
    mRootFrame->Destroy(*mPresContext);
  if (mDocument)
    mDocument->DeleteShell(this);
  mRefCnt = 0;
}

/**
 * Initialize the presentation shell. Create view manager and style
 * manager.
 */
nsresult
PresShell::Init(nsIDocument* aDocument,
                nsIPresContext* aPresContext,
                nsIViewManager* aViewManager,
                nsIStyleSet* aStyleSet)
{
  NS_PRECONDITION(nsnull != aDocument, "null ptr");
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  NS_PRECONDITION(nsnull != aViewManager, "null ptr");
  if ((nsnull == aDocument) || (nsnull == aPresContext) ||
      (nsnull == aViewManager)) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mDocument) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mDocument = dont_QueryInterface(aDocument);
  mViewManager = aViewManager;

  //doesn't add a ref since we own it... MMP
  mViewManager->SetViewObserver((nsIViewObserver *)this);

  // Bind the context to the presentation shell.
  mPresContext = dont_QueryInterface(aPresContext);
  aPresContext->SetShell(this);

  mStyleSet = dont_QueryInterface(aStyleSet);

  nsresult result = nsComponentManager::CreateInstance(kFrameSelectionCID, nsnull,
                                                 nsIFrameSelection::GetIID(),
                                                 getter_AddRefs(mSelection));
  if (!NS_SUCCEEDED(result))
    return result;

  // Create and initialize the frame manager
  result = NS_NewFrameManager(&mFrameManager);
  if (NS_FAILED(result)) {
    return result;
  }
  mFrameManager->Init(this, mStyleSet);

  nsCOMPtr<nsIDOMSelection> domSelection;
  mSelection->GetSelection(SELECTION_NORMAL, getter_AddRefs(domSelection));
  domSelection->AddSelectionListener(this);//possible circular reference
  
  result = mSelection->Init((nsIFocusTracker *) this);
  if (!NS_SUCCEEDED(result))
    return result;
  // Important: this has to happen after the selection has been set up
#ifdef SHOW_CARET
  // make the caret
  nsresult  err = NS_NewCaret(getter_AddRefs(mCaret));
  if (NS_SUCCEEDED(err))
  {
    mCaret->Init(this);
  }

  //SetCaretEnabled(PR_TRUE);			// make it show in browser windows
#endif  
//set up selection to be displayed in document
  nsCOMPtr<nsISupports> container;
  result = aPresContext->GetContainer(getter_AddRefs(container));
  if (NS_SUCCEEDED(result) && container) {
    nsCOMPtr<nsIWebShell> webShell;
    webShell = do_QueryInterface(container,&result);
    if (NS_SUCCEEDED(result) && webShell){
      nsWebShellType webShellType;
      result = webShell->GetWebShellType(webShellType);
      if (NS_SUCCEEDED(result)){
        if (nsWebShellContent == webShellType){
          mDocument->SetDisplaySelection(PR_TRUE);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::EnterReflowLock()
{
  ++mReflowLockCount;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ExitReflowLock()
{
  PRUint32 newReflowLockCount = mReflowLockCount - 1;
  if (newReflowLockCount == 0) {
    ProcessReflowCommands();
  }
  mReflowLockCount = newReflowLockCount;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetDocument(nsIDocument** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mDocument;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetPresContext(nsIPresContext** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mPresContext;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetViewManager(nsIViewManager** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mViewManager;
  NS_IF_ADDREF(mViewManager);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetStyleSet(nsIStyleSet** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mStyleSet;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetActiveAlternateStyleSheet(nsString& aSheetTitle)
{ // first non-html sheet in style set that has title
  if (mStyleSet) {
    PRInt32 count = mStyleSet->GetNumberOfDocStyleSheets();
    PRInt32 index;
    nsAutoString textHtml("text/html");
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = mStyleSet->GetDocStyleSheetAt(index);
      if (nsnull != sheet) {
        nsAutoString type;
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          nsAutoString title;
          sheet->GetTitle(title);
          if (0 < title.Length()) {
            aSheetTitle = title;
            index = count;  // stop looking
          }
        }
        NS_RELEASE(sheet);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::SelectAlternateStyleSheet(const nsString& aSheetTitle)
{
  if (mDocument && mStyleSet) {
    PRInt32 count = mDocument->GetNumberOfStyleSheets();
    PRInt32 index;
    nsAutoString textHtml("text/html");
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = mDocument->GetStyleSheetAt(index);
      if (nsnull != sheet) {
        nsAutoString type;
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          nsAutoString  title;
          sheet->GetTitle(title);
          if (0 < title.Length()) {
            if (title.EqualsIgnoreCase(aSheetTitle)) {
              mStyleSet->AddDocStyleSheet(sheet, mDocument);
            }
            else {
              mStyleSet->RemoveDocStyleSheet(sheet);
            }
          }
        }
        NS_RELEASE(sheet);
      }
    }
    ReconstructFrames();
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ListAlternateStyleSheets(nsStringArray& aTitleList)
{
  if (mDocument) {
    PRInt32 count = mDocument->GetNumberOfStyleSheets();
    PRInt32 index;
    nsAutoString textHtml("text/html");
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = mDocument->GetStyleSheetAt(index);
      if (nsnull != sheet) {
        nsAutoString type;
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          nsAutoString  title;
          sheet->GetTitle(title);
          if (0 < title.Length()) {
            if (-1 == aTitleList.IndexOfIgnoreCase(title)) {
              aTitleList.AppendString(title);
            }
          }
        }
        NS_RELEASE(sheet);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetSelection(SelectionType aType, nsIDOMSelection **aSelection)
{
  if (!aSelection || !mSelection)
    return NS_ERROR_NULL_POINTER;
  return mSelection->GetSelection(aType, aSelection);
}

NS_IMETHODIMP
PresShell::GetFrameSelection(nsIFrameSelection** aSelection)
{
  if (!aSelection || !mSelection)
    return NS_ERROR_NULL_POINTER;
  *aSelection = mSelection;
  (*aSelection)->AddRef();
  return NS_OK;
}


// Make shell be a document observer
NS_IMETHODIMP
PresShell::BeginObservingDocument()
{
  if (mDocument) {
    mDocument->AddObserver(this);
  }
  return NS_OK;
}

// Make shell stop being a document observer
NS_IMETHODIMP
PresShell::EndObservingDocument()
{
  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
  if (mSelection){
    nsCOMPtr<nsIDOMSelection> domselection;
    nsresult result;
    result = mSelection->GetSelection(SELECTION_NORMAL, getter_AddRefs(domselection));
    if (NS_FAILED(result))
      return result;
    if (!domselection)
      return NS_ERROR_UNEXPECTED;
    domselection->RemoveSelectionListener(this);
    mSelection->ShutDown();
  }
  return NS_OK;
}

#ifdef DEBUG_kipp
char* nsPresShell_ReflowStackPointerTop;
#endif

NS_IMETHODIMP
PresShell::InitialReflow(nscoord aWidth, nscoord aHeight)
{
  nsIContent* root = nsnull;

  SuspendCaret();
  EnterReflowLock();

  if (mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

  if (mDocument) {
    root = mDocument->GetRootContent();
  }

  if (nsnull != root) {
    if (nsnull == mRootFrame) {
      // Have style sheet processor construct a frame for the
      // precursors to the root content object's frame
      mStyleSet->ConstructRootFrame(mPresContext, root, mRootFrame);
    }

    // Have the style sheet processor construct frame for the root
    // content object down
    mStyleSet->ContentInserted(mPresContext, nsnull, root, 0);
    NS_RELEASE(root);
  }

  if (nsnull != mRootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::InitialReflow: %d,%d", aWidth, aHeight));
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
#ifdef DEBUG_kipp
    nsPresShell_ReflowStackPointerTop = (char*) &aWidth;
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIHTMLReflow*        htmlReflow;
    nsIRenderingContext*  rcx = nsnull;

    CreateRenderingContext(mRootFrame, &rcx);

    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Initial, rcx, maxSize);

    if (NS_OK == mRootFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->Reflow(*mPresContext, desiredSize, reflowState, status);
      mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
      mPresContext->SetVisibleArea(nsRect(0,0,desiredSize.width,desiredSize.height));
      
#ifdef NS_DEBUG
      if (nsIFrame::GetVerifyTreeEnable()) {
        mRootFrame->VerifyTree();
      }
#endif
    }
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::InitialReflow"));
  }

  ExitReflowLock();
  ResumeCaret();

  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight)
{
  SuspendCaret();
  EnterReflowLock();

  if (mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

  // If we don't have a root frame yet, that means we haven't had our initial
  // reflow...
  if (nsnull != mRootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::ResizeReflow: %d,%d", aWidth, aHeight));
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
#ifdef DEBUG_kipp
    nsPresShell_ReflowStackPointerTop = (char*) &aWidth;
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIHTMLReflow*        htmlReflow;
    nsIRenderingContext*  rcx = nsnull;

    CreateRenderingContext(mRootFrame, &rcx);

    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Resize, rcx, maxSize);

    if (NS_OK == mRootFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->Reflow(*mPresContext, desiredSize, reflowState, status);
      mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
#ifdef NS_DEBUG
      if (nsIFrame::GetVerifyTreeEnable()) {
        mRootFrame->VerifyTree();
      }
#endif
    }
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::ResizeReflow"));

    // XXX if debugging then we should assert that the cache is empty
  } else {
#ifdef NOISY
    printf("PresShell::ResizeReflow: null root frame\n");
#endif
  }
  ExitReflowLock();
  ResumeCaret();
  
  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::ScrollFrameIntoView(nsIFrame *aFrame){
  if (!aFrame)
    return NS_ERROR_NULL_POINTER;
  if (IsScrollingEnabled())
    return ScrollFrameIntoView(aFrame, NS_PRESSHELL_SCROLL_ANYWHERE,
                               NS_PRESSHELL_SCROLL_ANYWHERE);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  // Cancel any pending reflow commands targeted at this frame
  CancelReflowCommand(aFrame);

  // Notify the frame manager
  if (mFrameManager) {
    mFrameManager->NotifyDestroyingFrame(aFrame);
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetFrameManager(nsIFrameManager** aFrameManager) const
{
  *aFrameManager = mFrameManager;
  NS_IF_ADDREF(mFrameManager);
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetCaret(nsICaret **outCaret)
{
  if (!outCaret || !mCaret)
    return NS_ERROR_NULL_POINTER;
  return mCaret->QueryInterface(kICaretIID,(void **)outCaret);
}

nsresult PresShell::RefreshCaret(nsIView *aView, nsIRenderingContext& aRendContext, const nsRect& aDirtyRect)
{
  if (mCaret)
  	mCaret->Refresh(aView, aRendContext, aDirtyRect);

  return NS_OK;
}

NS_IMETHODIMP PresShell::SetCaretEnabled(PRBool aInEnable)
{
	nsresult	result = NS_OK;
	PRBool	oldEnabled = mCaretEnabled;
	
	mCaretEnabled = aInEnable;
	
	if (mCaret && (mCaretEnabled != oldEnabled))
	{
		if (mCaretEnabled)
			result = mCaret->SetCaretVisible(PR_TRUE);
		else
			result = mCaret->SetCaretVisible(PR_FALSE);
	}
	
	return result;
}

NS_IMETHODIMP PresShell::GetCaretEnabled(PRBool *aOutEnabled)
{
  if (!aOutEnabled) { return NS_ERROR_INVALID_ARG; }
  *aOutEnabled = mCaretEnabled;
  return NS_OK;
}

NS_IMETHODIMP PresShell::SetDisplayNonTextSelection(PRBool aInEnable)
{
  mDisplayNonTextSelection = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetDisplayNonTextSelection(PRBool *aOutEnable)
{
  if (!aOutEnable)
    return NS_ERROR_INVALID_ARG;
  *aOutEnable = mDisplayNonTextSelection;
  return NS_OK;
}


/*implementation of the nsIDOMSelectionListener
  it will invoke the resetselection to update the presentation shell's frames
*/
NS_IMETHODIMP PresShell::NotifySelectionChanged()
{
  if (!mSelection)
    return NS_ERROR_NULL_POINTER;
  return NS_ERROR_NULL_POINTER;
}

nsresult PresShell::SuspendCaret()
{
	if (mCaret)
		return mCaret->SetCaretVisible(PR_FALSE);
	return NS_OK;
}

nsresult PresShell::ResumeCaret()
{
	if (mCaret && mCaretEnabled)
		return mCaret->SetCaretVisible(PR_TRUE);
	return NS_OK;
}

NS_IMETHODIMP
PresShell::StyleChangeReflow()
{
  EnterReflowLock();

  if (nsnull != mRootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::StyleChangeReflow"));
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIHTMLReflow*        htmlReflow;
    nsIRenderingContext*  rcx = nsnull;

    CreateRenderingContext(mRootFrame, &rcx);

    // XXX We should be using eReflowReason_StyleChange
    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Resize, rcx, maxSize);

    if (NS_OK == mRootFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->Reflow(*mPresContext, desiredSize, reflowState, status);
      mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
#ifdef NS_DEBUG
      if (nsIFrame::GetVerifyTreeEnable()) {
        mRootFrame->VerifyTree();
      }
#endif
    }
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::StyleChangeReflow"));
  }

  ExitReflowLock();

  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::GetRootFrame(nsIFrame** aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mRootFrame;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetPageSequenceFrame(nsIPageSequenceFrame** aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIFrame*             child;
  nsIPageSequenceFrame* pageSequence;

  // The page sequence frame should be either the immediate child or
  // its child
  mRootFrame->FirstChild(nsnull, &child);
  if (nsnull != child) {
    if (NS_SUCCEEDED(child->QueryInterface(kIPageSequenceFrameIID, (void**)&pageSequence))) {
      *aResult = pageSequence;
      return NS_OK;
    }
  
    child->FirstChild(nsnull, &child);
    if (nsnull != child) {
      if (NS_SUCCEEDED(child->QueryInterface(kIPageSequenceFrameIID, (void**)&pageSequence))) {
        *aResult = pageSequence;
        return NS_OK;
      }
    }
  }
  *aResult = nsnull;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresShell::BeginUpdate(nsIDocument *aDocument)
{
  mUpdateCount++;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndUpdate(nsIDocument *aDocument)
{
  NS_PRECONDITION(0 != mUpdateCount, "too many EndUpdate's");
  if (--mUpdateCount == 0) {
    // XXX do something here
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::BeginLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::AppendReflowCommand(nsIReflowCommand* aReflowCommand)
{
#ifdef NS_DEBUG
  if (mInVerifyReflow) {
    return NS_OK;
  }
#endif
  NS_ADDREF(aReflowCommand);
  return mReflowCommands.AppendElement(aReflowCommand);
}

NS_IMETHODIMP
PresShell::CancelReflowCommand(nsIFrame* aTargetFrame)
{
  PRInt32 i, n = mReflowCommands.Count();
  for (i = 0; i < n; i++) {
    nsIReflowCommand* rc = (nsIReflowCommand*) mReflowCommands.ElementAt(0);
    if (rc) {
      nsIFrame* target;
      if (NS_SUCCEEDED(rc->GetTarget(target))) {
        if (target == aTargetFrame) {
          mReflowCommands.RemoveElementAt(i);
          n--;
          i--;
          continue;
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ProcessReflowCommands()
{
  if (0 != mReflowCommands.Count()) {
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsIRenderingContext*  rcx;

    CreateRenderingContext(mRootFrame, &rcx);

    while (0 != mReflowCommands.Count()) {
      nsIReflowCommand* rc = (nsIReflowCommand*) mReflowCommands.ElementAt(0);
      mReflowCommands.RemoveElementAt(0);

      // Dispatch the reflow command
      nsSize          maxSize;
      mRootFrame->GetSize(maxSize);
#ifdef NS_DEBUG
      nsIReflowCommand::ReflowType type;
      rc->GetType(type);
      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
         ("PresShell::ProcessReflowCommands: begin reflow command type=%d",
          type));
#endif
      rc->Dispatch(*mPresContext, desiredSize, maxSize, *rcx);
      NS_RELEASE(rc);
      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
         ("PresShell::ProcessReflowCommands: end reflow command"));
    }
    NS_IF_RELEASE(rcx);

#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
    if (GetVerifyReflowEnable()) {
      // First synchronously render what we have so far so that we can
      // see it.
//      if (gVerifyReflowAll) {
//        printf("Before verify-reflow\n");
        nsIView* rootView;
        mViewManager->GetRootView(rootView);
        mViewManager->UpdateView(rootView, nsnull, NS_VMREFRESH_IMMEDIATE);
//        PR_Sleep(PR_SecondsToInterval(3));
//      }

      mInVerifyReflow = PR_TRUE;
      VerifyIncrementalReflow();
      mInVerifyReflow = PR_FALSE;
      if (gVerifyReflowAll) {
        printf("After verify-reflow\n");
      }

      if (0 != mReflowCommands.Count()) {
        printf("XXX yikes!\n");
      }
    }
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::ClearFrameRefs(nsIFrame* aFrame)
{
  nsIEventStateManager *manager;
  if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
    manager->ClearFrameRefs(aFrame);
    NS_RELEASE(manager);
  }
  
  if (mCaret) {
  	mCaret->ClearFrameRefs(aFrame);
  }
  
  if (aFrame == mCurrentEventFrame) {
    mCurrentEventFrame->GetContent(&mCurrentEventContent);
    mCurrentEventFrame = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CreateRenderingContext(nsIFrame *aFrame,
                                  nsIRenderingContext** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIWidget *widget = nsnull;
  nsIView   *view = nsnull;
  nsPoint   pt;
  nsresult  rv;

  aFrame->GetView(&view);

  if (nsnull == view)
    aFrame->GetOffsetFromView(pt, &view);

  while (nsnull != view)
  {
    view->GetWidget(widget);

    if (nsnull != widget)
    {
      NS_RELEASE(widget);
      break;
    }

    view->GetParent(view);
  }

  nsCOMPtr<nsIDeviceContext> dx;

  nsIRenderingContext* result = nsnull;
  rv = mPresContext->GetDeviceContext(getter_AddRefs(dx));
  if (NS_SUCCEEDED(rv) && dx) {
    if (nsnull != view) {
      rv = dx->CreateRenderingContext(view, result);
    }
    else {
      rv = dx->CreateRenderingContext(result);
    }
  }
  *aResult = result;

  return rv;
}

NS_IMETHODIMP
PresShell::CantRenderReplacedElement(nsIPresContext* aPresContext,
                                     nsIFrame*       aFrame)
{
  if (mFrameManager) {
    return mFrameManager->CantRenderReplacedElement(aPresContext, aFrame);
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::GoToAnchor(const nsString& aAnchorName) const
{
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc;
  nsCOMPtr<nsIXMLDocument> xmlDoc;
  nsresult                     rv = NS_OK;
  nsCOMPtr<nsIContent>  content;

  if (NS_SUCCEEDED(mDocument->QueryInterface(kIDOMHTMLDocumentIID,
                                             getter_AddRefs(htmlDoc)))) {    
    nsCOMPtr<nsIDOMElement> element;

    // Find the element with the specified id
    rv = htmlDoc->GetElementById(aAnchorName, getter_AddRefs(element));
    if (NS_SUCCEEDED(rv) && element) {
      // Get the nsIContent interface, because that's what we need to
      // get the primary frame
      rv = element->QueryInterface(kIContentIID, getter_AddRefs(content));
    }
  }
  else if (NS_SUCCEEDED(mDocument->QueryInterface(kIXMLDocumentIID,
                                                  getter_AddRefs(xmlDoc)))) {
    rv = xmlDoc->GetContentById(aAnchorName,  getter_AddRefs(content));
  }

  if (NS_SUCCEEDED(rv) && content) {
    nsIFrame* frame;
    
    // Get the primary frame
    if (NS_SUCCEEDED(GetPrimaryFrameFor(content, &frame))) {
      rv = ScrollFrameIntoView(frame, NS_PRESSHELL_SCROLL_TOP,
                               NS_PRESSHELL_SCROLL_ANYWHERE);
    }
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
PresShell::ScrollFrameIntoView(nsIFrame *aFrame,
                               PRIntn   aVPercent, 
                               PRIntn   aHPercent) const
{
  nsresult rv = NS_OK;
  if (!aFrame) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mViewManager) {
    // Get the viewport scroller
    nsIScrollableView* scrollingView;
    mViewManager->GetRootScrollableView(&scrollingView);

    if (scrollingView) {
      nsIView*  scrolledView;
      nsPoint   offset;
      nsIView*  closestView;
          
      // Determine the offset from aFrame to the scrolled view. We do that by
      // getting the offset from its closest view and then walking up
      scrollingView->GetScrolledView(scrolledView);
      aFrame->GetOffsetFromView(offset, &closestView);

      // XXX Deal with the case where there is a scrolled element, e.g., a
      // DIV in the middle...
      while ((closestView != nsnull) && (closestView != scrolledView)) {
        nscoord x, y;

        // Update the offset
        closestView->GetPosition(&x, &y);
        offset.MoveBy(x, y);

        // Get its parent view
        closestView->GetParent(closestView);
      }

      // Determine the visible rect in the scrolled view's coordinate space.
      // The size of the visible area is the clip view size
      const nsIView*  clipView;
      nsRect          visibleRect;

      scrollingView->GetScrollPosition(visibleRect.x, visibleRect.y);
      scrollingView->GetClipView(&clipView);
      clipView->GetDimensions(&visibleRect.width, &visibleRect.height);

      // The actual scroll offsets
      nscoord scrollOffsetX = visibleRect.x;
      nscoord scrollOffsetY = visibleRect.y;

      // The frame's bounds in the coordinate space of the scrolled frame
      nsRect  frameBounds;
      aFrame->GetRect(frameBounds);
      frameBounds.x = offset.x;
      frameBounds.y = offset.y;

      // See how the frame should be positioned vertically
      if (NS_PRESSHELL_SCROLL_ANYWHERE == aVPercent) {
        // The caller doesn't care where the frame is positioned vertically,
        // so long as it's fully visible
        if (frameBounds.y < visibleRect.y) {
          // Scroll up so the frame's top edge is visible
          scrollOffsetY = frameBounds.y;
        } else if (frameBounds.YMost() > visibleRect.YMost()) {
          // Scroll down so the frame's bottom edge is visible. Make sure the
          // frame's top edge is still visible
          scrollOffsetY += frameBounds.YMost() - visibleRect.YMost();
          if (scrollOffsetY > frameBounds.y) {
            scrollOffsetY = frameBounds.y;
          }
        }
      } else {
        // Align the frame edge according to the specified percentage
        nscoord frameAlignY = frameBounds.y + (frameBounds.height * aVPercent) / 100;
        scrollOffsetY = frameAlignY - (visibleRect.height * aVPercent) / 100;
      }

      // See how the frame should be positioned horizontally
      if (NS_PRESSHELL_SCROLL_ANYWHERE == aHPercent) {
        // The caller doesn't care where the frame is positioned horizontally,
        // so long as it's fully visible
        if (frameBounds.x < visibleRect.x) {
          // Scroll left so the frame's left edge is visible
          scrollOffsetX = frameBounds.x;
        } else if (frameBounds.XMost() > visibleRect.XMost()) {
          // Scroll right so the frame's right edge is visible. Make sure the
          // frame's left edge is still visible
          scrollOffsetX += frameBounds.XMost() - visibleRect.XMost();
          if (scrollOffsetX > frameBounds.x) {
            scrollOffsetX = frameBounds.x;
          }
        }
      
      } else {
        // Align the frame edge according to the specified percentage
        nscoord frameAlignX = frameBounds.x + (frameBounds.width * aHPercent) / 100;
        scrollOffsetX = frameAlignX - (visibleRect.width * aHPercent) / 100;
      }
      
      scrollingView->ScrollTo(scrollOffsetX, scrollOffsetY, NS_VMREFRESH_IMMEDIATE);
    }
  }
  return rv;
}


NS_IMETHODIMP
PresShell::DoCopy()
{
  nsCOMPtr<nsIDocument> doc;
  GetDocument(getter_AddRefs(doc));
  if (doc) {
    nsString buffer;
    
    nsIDOMSelection* sel = nsnull;
    GetSelection(SELECTION_NORMAL, &sel);
      
    if (sel != nsnull)
      doc->CreateXIF(buffer,sel);
    NS_IF_RELEASE(sel);

    // Get the Clipboard
    nsIClipboard* clipboard = nsnull;
    nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                               nsIClipboard::GetIID(),
                                               (nsISupports **)&clipboard);

    if ( clipboard ) {
      // Create a transferable for putting data on the Clipboard
      nsCOMPtr<nsITransferable> trans;
      rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                              nsITransferable::GetIID(), 
                                              (void**) getter_AddRefs(trans));
      if ( trans ) {
        // The data on the clipboard will be in "XIF" format
        // so give the clipboard transferable a "XIFConverter" for 
        // converting from XIF to other formats
        nsCOMPtr<nsIFormatConverter> xifConverter;
        rv = nsComponentManager::CreateInstance(kCXIFConverterCID, nsnull, 
                                                 NS_GET_IID(nsIFormatConverter), getter_AddRefs(xifConverter));
        if ( xifConverter ) {
          // Add the XIF DataFlavor to the transferable
          // this tells the transferable that it can handle receiving the XIF format
          trans->AddDataFlavor(kXIFMime);

          // Add the converter for going from XIF to other formats
          trans->SetConverter(xifConverter);

          // Now add the XIF data to the transferable, placing it into a nsISupportsWString object.
          // the transferable wants the number bytes for the data and since it is double byte
          // we multiply by 2. 
          nsCOMPtr<nsISupportsWString> dataWrapper;
          rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull, 
                                                   NS_GET_IID(nsISupportsWString), getter_AddRefs(dataWrapper));
          if ( dataWrapper ) {
            dataWrapper->SetData ( NS_CONST_CAST(PRUnichar*,buffer.GetUnicode()) );
            // QI the data object an |nsISupports| so that when the transferable holds
            // onto it, it will addref the correct interface.
            nsCOMPtr<nsISupports> genericDataObj ( do_QueryInterface(dataWrapper) );
            trans->SetTransferData(kXIFMime, genericDataObj, buffer.Length()*2);
          }
          
          // put the transferable on the clipboard
          clipboard->SetData(trans, nsnull);
        }
      }
      nsServiceManager::ReleaseService(kCClipboardCID, clipboard);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
PresShell::ContentChanged(nsIDocument *aDocument,
                          nsIContent*  aContent,
                          nsISupports* aSubContent)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();
  nsresult rv = mStyleSet->ContentChanged(mPresContext, aContent, aSubContent);
  ExitReflowLock();

  return rv;
}

NS_IMETHODIMP
PresShell::ContentStatesChanged(nsIDocument* aDocument,
                                nsIContent* aContent1,
                                nsIContent* aContent2)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();
  nsresult rv = mStyleSet->ContentStatesChanged(mPresContext, aContent1, aContent2);
  ExitReflowLock();

  return rv;
}


NS_IMETHODIMP
PresShell::AttributeChanged(nsIDocument *aDocument,
                            nsIContent*  aContent,
                            nsIAtom*     aAttribute,
                            PRInt32      aHint)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();
  nsresult rv = mStyleSet->AttributeChanged(mPresContext, aContent, aAttribute, aHint);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentAppended(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           PRInt32     aNewIndexInContainer)
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->ContentAppended(mPresContext, aContainer, aNewIndexInContainer);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentInserted(nsIDocument* aDocument,
                           nsIContent*  aContainer,
                           nsIContent*  aChild,
                           PRInt32      aIndexInContainer)
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->ContentInserted(mPresContext, aContainer, aChild, aIndexInContainer);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentReplaced(nsIDocument* aDocument,
                           nsIContent*  aContainer,
                           nsIContent*  aOldChild,
                           nsIContent*  aNewChild,
                           PRInt32      aIndexInContainer)
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->ContentReplaced(mPresContext, aContainer, aOldChild,
                                            aNewChild, aIndexInContainer);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentRemoved(nsIDocument *aDocument,
                          nsIContent* aContainer,
                          nsIContent* aChild,
                          PRInt32     aIndexInContainer)
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->ContentRemoved(mPresContext, aContainer,
                                           aChild, aIndexInContainer);
  ExitReflowLock();
  return rv;
}

nsresult
PresShell::ReconstructFrames(void)
{
  nsresult rv = NS_OK;
          
  EnterReflowLock();
  rv = mStyleSet->ReconstructDocElementHierarchy(mPresContext);
  ExitReflowLock();

  return rv;
}

NS_IMETHODIMP
PresShell::StyleSheetAdded(nsIDocument *aDocument,
                           nsIStyleSheet* aStyleSheet)
{
  return ReconstructFrames();
}

NS_IMETHODIMP 
PresShell::StyleSheetRemoved(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet)
{
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                          nsIStyleSheet* aStyleSheet,
                                          PRBool aDisabled)
{
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::StyleRuleChanged(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule,
                            PRInt32 aHint) 
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->StyleRuleChanged(mPresContext, aStyleSheet,
                                             aStyleRule, aHint);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::StyleRuleAdded(nsIDocument *aDocument,
                          nsIStyleSheet* aStyleSheet,
                          nsIStyleRule* aStyleRule) 
{ 
  EnterReflowLock();
  nsresult rv = mStyleSet->StyleRuleAdded(mPresContext, aStyleSheet,
                                          aStyleRule);
  ExitReflowLock();
  if (NS_FAILED(rv)) {
    return rv;
  }
  // XXX For now reconstruct everything
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::StyleRuleRemoved(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) 
{ 
  EnterReflowLock();
  nsresult  rv = mStyleSet->StyleRuleRemoved(mPresContext, aStyleSheet,
                                             aStyleRule);
  ExitReflowLock();
  if (NS_FAILED(rv)) {
    return rv;
  }
  // XXX For now reconstruct everything
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetPrimaryFrameFor(nsIContent* aContent,
                              nsIFrame**  aResult) const
{
  nsresult  rv;

  if (mFrameManager) {
    rv = mFrameManager->GetPrimaryFrameFor(aContent, aResult);

  } else {
    *aResult = nsnull;
    rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP 
PresShell::GetStyleContextFor(nsIFrame*         aFrame,
                              nsIStyleContext** aStyleContext) const
{
  if (!aFrame || !aStyleContext) {
    return NS_ERROR_NULL_POINTER;
  }
  return (aFrame->GetStyleContext(aStyleContext));
}

NS_IMETHODIMP
PresShell::GetLayoutObjectFor(nsIContent*   aContent,
                              nsISupports** aResult) const 
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if ((nsnull!=aResult) && (nsnull!=aContent))
  {
    *aResult = nsnull;
    nsIFrame *primaryFrame=nsnull;
    result = GetPrimaryFrameFor(aContent, &primaryFrame);
    if ((NS_SUCCEEDED(result)) && (nsnull!=primaryFrame))
    {
      result = primaryFrame->QueryInterface(kISupportsIID, (void**)aResult);
    }
  }
  return result;
}
  

NS_IMETHODIMP
PresShell::GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                  nsIFrame** aResult) const
{
  nsresult  rv;

  if (mFrameManager) {
    rv = mFrameManager->GetPlaceholderFrameFor(aFrame, aResult);

  } else {
    *aResult = nsnull;
    rv = NS_OK;
  }

  return rv;
}

//nsIViewObserver

NS_IMETHODIMP
PresShell::Paint(nsIView              *aView,
                 nsIRenderingContext& aRenderingContext,
                 const nsRect&        aDirtyRect)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv = NS_OK;

  NS_ASSERTION(!(nsnull == aView), "null view");

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

  if (nsnull != frame) {
    rv = frame->Paint(*mPresContext, aRenderingContext, aDirtyRect,
                      NS_FRAME_PAINT_LAYER_BACKGROUND);
    rv = frame->Paint(*mPresContext, aRenderingContext, aDirtyRect,
                      NS_FRAME_PAINT_LAYER_FLOATERS);
    rv = frame->Paint(*mPresContext, aRenderingContext, aDirtyRect,
                      NS_FRAME_PAINT_LAYER_FOREGROUND);
#ifdef NS_DEBUG
    // Draw a border around the frame
    if (nsIFrame::GetShowFrameBorders()) {
      nsRect r;
      frame->GetRect(r);
      aRenderingContext.SetColor(NS_RGB(0,0,255));
      aRenderingContext.DrawRect(0, 0, r.width, r.height);
    }
#endif

	// ensure the caret gets redrawn
    RefreshCaret(aView, aRenderingContext, aDirtyRect);
  }
  return rv;
}

nsIFrame*
PresShell::GetCurrentEventFrame()
{
  if (!mCurrentEventFrame && mCurrentEventContent) {
    GetPrimaryFrameFor(mCurrentEventContent, &mCurrentEventFrame);
  }

  return mCurrentEventFrame;
}

NS_IMETHODIMP
PresShell::HandleEvent(nsIView         *aView,
                       nsGUIEvent*     aEvent,
                       nsEventStatus&  aEventStatus)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv = NS_OK;
  
  NS_ASSERTION(!(nsnull == aView), "null view");

  if (mIsDestroying || mReflowLockCount > 0) {
    return NS_OK;
  }

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

  if (nsnull != frame) {

    nsIWebShell* webShell = nsnull;
    nsISupports* container;
    mPresContext->GetContainer(&container);
    if (nsnull != container) {
      if (NS_OK != container->QueryInterface(kIWebShellIID, (void**)&webShell)) {
        NS_ASSERTION(webShell, "No webshell to grab during event dispatch");
      }
      NS_RELEASE(container);
    }

    if (mSelection && aEvent->eventStructType == NS_KEY_EVENT)
    {
      mSelection->EnableFrameNotification(PR_FALSE);
      mSelection->HandleKeyEvent(aEvent);
      mSelection->EnableFrameNotification(PR_TRUE); //prevents secondary reset selection called since
      //we are a listener now.
    }
    nsIEventStateManager *manager;
    nsIContent* focusContent = nsnull;
    if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
      if (NS_IS_KEY_EVENT(aEvent)) {
        //Key events go to the focused frame, not point based.
        manager->GetFocusedContent(&focusContent);
        if (focusContent)
          GetPrimaryFrameFor(focusContent, &mCurrentEventFrame);
        else frame->GetFrameForPoint(aEvent->point, &mCurrentEventFrame);
      }
      else frame->GetFrameForPoint(aEvent->point, &mCurrentEventFrame);
      NS_IF_RELEASE(mCurrentEventContent);
      if (GetCurrentEventFrame() || focusContent) {
      //Once we have the targetFrame, handle the event in this order
        //1. Give event to event manager for pre event state changes and generation of synthetic events.
        rv = manager->PreHandleEvent(*mPresContext, aEvent, mCurrentEventFrame, aEventStatus, aView);

        //2. Give event to the DOM for third party and JS use.
        if ((GetCurrentEventFrame() || focusContent) && NS_OK == rv) {
          if (focusContent) {
            rv = focusContent->HandleDOMEvent(*mPresContext, (nsEvent*)aEvent, nsnull, 
                                               NS_EVENT_FLAG_INIT, aEventStatus);
          }
          else {
            nsIContent* targetContent;
            if (NS_OK == mCurrentEventFrame->GetContent(&targetContent) && nsnull != targetContent) {
              // XXX Temporary fix for re-entracy isses
              // temporarily cache the current frame
              nsIFrame * currentEventFrame = mCurrentEventFrame;
              rv = targetContent->HandleDOMEvent(*mPresContext, (nsEvent*)aEvent, nsnull, 
                                                 NS_EVENT_FLAG_INIT, aEventStatus);
              mCurrentEventFrame = currentEventFrame;  // other part of re-entracy fix
              NS_RELEASE(targetContent);
            }
          }

          //3. Give event to the Frames for browser default processing.
          // XXX The event isn't translated into the local coordinate space
          // of the frame...
          if (GetCurrentEventFrame() && NS_OK == rv) {
            rv = mCurrentEventFrame->HandleEvent(*mPresContext, aEvent, aEventStatus);
          }

          //4. Give event to event manager for post event state changes and generation of synthetic events.
          if ((GetCurrentEventFrame() || focusContent) && NS_OK == rv) {
            rv = manager->PostHandleEvent(*mPresContext, aEvent, mCurrentEventFrame, aEventStatus, aView);
          }
        }
      }
      NS_RELEASE(manager);
      NS_IF_RELEASE(focusContent);
    }
    NS_IF_RELEASE(webShell);
  }
  else {
    rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
PresShell::Scrolled(nsIView *aView)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv;
  
  NS_ASSERTION(!(nsnull == aView), "null view");

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

  if (nsnull != frame)
    rv = frame->Scrolled(aView);
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP
PresShell::ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight)
{
  return ResizeReflow(aWidth, aHeight);
}

#ifdef NS_DEBUG
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIScrollableView.h"
#include "nsIDeviceContext.h"
#include "nsIURL.h"
#include "nsILinkHandler.h"

static NS_DEFINE_IID(kViewManagerCID, NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kIViewManagerIID, NS_IVIEWMANAGER_IID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kILinkHandlerIID, NS_ILINKHANDLER_IID);

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg)
{
  printf("verifyreflow: ");
  nsAutoString name;
  if (nsnull != k1) {
    k1->GetFrameName(name);
  }
  else {
    name = "(null)";
  }
  fputs(name, stdout);

  printf(" != ");

  if (nsnull != k2) {
    k2->GetFrameName(name);
  }
  else {
    name = "(null)";
  }
  fputs(name, stdout);

  printf(" %s", aMsg);
}

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg,
                 const nsRect& r1, const nsRect& r2)
{
  printf("verifyreflow: ");
  nsAutoString name;
  k1->GetFrameName(name);
  fputs(name, stdout);
  stdout << r1;

  printf(" != ");

  k2->GetFrameName(name);
  fputs(name, stdout);
  stdout << r2;

  printf(" %s\n", aMsg);
}

static PRBool
CompareTrees(nsIFrame* aA, nsIFrame* aB)
{
  PRBool ok = PR_TRUE;
  PRBool whoops = PR_FALSE;
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* k1, *k2;
    aA->FirstChild(listName, &k1);
    aB->FirstChild(listName, &k2);
    PRInt32 l1 = nsContainerFrame::LengthOf(k1);
    PRInt32 l2 = nsContainerFrame::LengthOf(k2);
    if (l1 != l2) {
      ok = PR_FALSE;
      LogVerifyMessage(k1, k2, "child counts don't match: ");
      printf("%d != %d\n", l1, l2);
      if (!gVerifyReflowAll) {
        break;
      }
    }

    nsRect r1, r2;
    nsIView* v1, *v2;
    nsIWidget* w1, *w2;
    for (;;) {
      if (((nsnull == k1) && (nsnull != k2)) ||
          ((nsnull != k1) && (nsnull == k2))) {
        ok = PR_FALSE;
        LogVerifyMessage(k1, k2, "child lists are different\n");
        whoops = PR_TRUE;
        break;
      }
      else if (nsnull != k1) {
        // Verify that the frames are the same size
        k1->GetRect(r1);
        k2->GetRect(r2);
        if (r1 != r2) {
          ok = PR_FALSE;
          LogVerifyMessage(k1, k2, "(frame rects)", r1, r2);
          whoops = PR_TRUE;
        }

        // Make sure either both have views or neither have views; if they
        // do have views, make sure the views are the same size. If the
        // views have widgets, make sure they both do or neither does. If
        // they do, make sure the widgets are the same size.
        k1->GetView(&v1);
        k2->GetView(&v2);
        if (((nsnull == v1) && (nsnull != v2)) ||
            ((nsnull != v1) && (nsnull == v2))) {
          ok = PR_FALSE;
          LogVerifyMessage(k1, k2, "child views are not matched\n");
          whoops = PR_TRUE;
        }
        else if (nsnull != v1) {
          v1->GetBounds(r1);
          v2->GetBounds(r2);
          if (r1 != r2) {
//            ok = PR_FALSE;
            LogVerifyMessage(k1, k2, "(view rects)", r1, r2);
//            whoops = PR_TRUE;
          }

          v1->GetWidget(w1);
          v2->GetWidget(w2);
          if (((nsnull == w1) && (nsnull != w2)) ||
              ((nsnull != w1) && (nsnull == w2))) {
            ok = PR_FALSE;
            LogVerifyMessage(k1, k2, "child widgets are not matched\n");
            whoops = PR_TRUE;
          }
          else if (nsnull != w1) {
            w1->GetBounds(r1);
            w2->GetBounds(r2);
            if (r1 != r2) {
//              ok = PR_FALSE;
              LogVerifyMessage(k1, k2, "(widget rects)", r1, r2);
//              whoops = PR_TRUE;
            }
          }
        }
        if (whoops && !gVerifyReflowAll) {
          break;
        }

        // Compare the sub-trees too
        if (!CompareTrees(k1, k2)) {
          if (!gVerifyReflowAll) {
            ok = PR_FALSE;
            whoops = PR_TRUE;
            break;
          }
        }

        // Advance to next sibling
        k1->GetNextSibling(&k1);
        k2->GetNextSibling(&k2);
      }
      else {
        break;
      }
    }
    if (whoops && !gVerifyReflowAll) {
      break;
    }
    NS_IF_RELEASE(listName);

    nsIAtom* listName1;
    nsIAtom* listName2;
    aA->GetAdditionalChildListName(listIndex, &listName1);
    aB->GetAdditionalChildListName(listIndex, &listName2);
    listIndex++;
    if (listName1 != listName2) {
      ok = PR_FALSE;
      LogVerifyMessage(k1, k2, "child list names are not matched: ");
      nsAutoString tmp;
      if (nsnull != listName1) {
        listName1->ToString(tmp);
        fputs(tmp, stdout);
      }
      else
        fputs("(null)", stdout);
      printf(" != ");
      if (nsnull != listName2) {
        listName2->ToString(tmp);
        fputs(tmp, stdout);
      }
      else
        fputs("(null)", stdout);
      printf("\n");
      NS_IF_RELEASE(listName1);
      NS_IF_RELEASE(listName2);
      break;
    }
    NS_IF_RELEASE(listName2);
    listName = listName1;
  } while (ok && (listName != nsnull));

  return ok;
}

#if 0
static nsIFrame*
FindTopFrame(nsIFrame* aRoot)
{
  if (nsnull != aRoot) {
    nsIContent* content;
    aRoot->GetContent(&content);
    if (nsnull != content) {
      nsIAtom* tag;
      content->GetTag(tag);
      if (nsnull != tag) {
        NS_RELEASE(tag);
        NS_RELEASE(content);
        return aRoot;
      }
      NS_RELEASE(content);
    }

    // Try one of the children
    nsIFrame* kid;
    aRoot->FirstChild(nsnull, &kid);
    while (nsnull != kid) {
      nsIFrame* result = FindTopFrame(kid);
      if (nsnull != result) {
        return result;
      }
      kid->GetNextSibling(&kid);
    }
  }
  return nsnull;
}
#endif

nsresult
PresShell::CloneStyleSet(nsIStyleSet* aSet, nsIStyleSet** aResult)
{
  nsIStyleSet* clone;
  nsresult rv = NS_NewStyleSet(&clone);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 i, n;
  n = aSet->GetNumberOfOverrideStyleSheets();
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss;
    ss = aSet->GetOverrideStyleSheetAt(i);
    if (nsnull != ss) {
      clone->AppendOverrideStyleSheet(ss);
      NS_RELEASE(ss);
    }
  }

  n = aSet->GetNumberOfDocStyleSheets();
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss;
    ss = aSet->GetDocStyleSheetAt(i);
    if (nsnull != ss) {
      clone->AddDocStyleSheet(ss, mDocument);
      NS_RELEASE(ss);
    }
  }

  n = aSet->GetNumberOfBackstopStyleSheets();
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss;
    ss = aSet->GetBackstopStyleSheetAt(i);
    if (nsnull != ss) {
      clone->AppendBackstopStyleSheet(ss);
      NS_RELEASE(ss);
    }
  }
  *aResult = clone;
  return NS_OK;
}

// After an incremental reflow, we verify the correctness by doing a
// full reflow into a fresh frame tree.
void
PresShell::VerifyIncrementalReflow()
{
  // All the stuff we are creating that needs releasing
  nsIPresContext* cx;
  nsIViewManager* vm;
  nsIPresShell* sh;

  // Create a presentation context to view the new frame tree
  nsresult rv;
  PRBool isPaginated = PR_FALSE;
  mPresContext->IsPaginated(&isPaginated);
  if (isPaginated) {
    rv = NS_NewPrintPreviewContext(&cx);
  }
  else {
    rv = NS_NewGalleyContext(&cx);
  }
#if 1
  nsISupports* container;
  if (NS_SUCCEEDED(mPresContext->GetContainer(&container)) &&
      (nsnull != container)) {
    cx->SetContainer(container);
    nsILinkHandler* lh;
    if (NS_SUCCEEDED(container->QueryInterface(kILinkHandlerIID,
                                               (void**)&lh))) {
      cx->SetLinkHandler(lh);
      NS_RELEASE(lh);
    }
    NS_RELEASE(container);
  }
#endif
  NS_ASSERTION(NS_OK == rv, "failed to create presentation context");
  nsCOMPtr<nsIDeviceContext> dc;
  mPresContext->GetDeviceContext(getter_AddRefs(dc));
  nsCOMPtr<nsIPref> prefs; 
  mPresContext->GetPrefs(getter_AddRefs(prefs));
  cx->Init(dc, prefs);

  // Get our scrolling preference
  nsScrollPreference scrolling;
  nsIView* rootView;
  mViewManager->GetRootView(rootView);
  nsIScrollableView* scrollView;
  rv = rootView->QueryInterface(kScrollViewIID, (void**)&scrollView);
  if (NS_OK == rv) {
    scrollView->GetScrollPreference(scrolling);
  }
  nsIWidget* rootWidget;
  rootView->GetWidget(rootWidget);
  void* nativeParentWidget = rootWidget->GetNativeData(NS_NATIVE_WIDGET);

  // Create a new view manager.
  rv = nsComponentManager::CreateInstance(kViewManagerCID, nsnull,
                                          kIViewManagerIID, (void**) &vm);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_OK == rv, "failed to create view manager");
  }
  rv = vm->Init(dc);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_OK == rv, "failed to init view manager");
  }

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  nsRect tbounds;
  mPresContext->GetVisibleArea(tbounds);
  nsIView* view;
  rv = nsComponentManager::CreateInstance(kViewCID, nsnull,
                                          kIViewIID, (void **) &view);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_OK == rv, "failed to create scroll view");
  }
  rv = view->Init(vm, tbounds, nsnull);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_OK == rv, "failed to init scroll view");
  }

  //now create the widget for the view
  rv = view->CreateWidget(kWidgetCID, nsnull, nativeParentWidget);
  if (NS_OK != rv) {
    NS_ASSERTION(NS_OK == rv, "failed to create scroll view widget");
  }

  // Setup hierarchical relationship in view manager
  vm->SetRootView(view);

  // Make the new presentation context the same size as our
  // presentation context.
  nsRect r;
  mPresContext->GetVisibleArea(r);
  cx->SetVisibleArea(r);

  // Create a new presentation shell to view the document. Use the
  // exact same style information that this document has.
  nsCOMPtr<nsIStyleSet> newSet;
  rv = CloneStyleSet(mStyleSet, getter_AddRefs(newSet));
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clone style set");
  rv = mDocument->CreateShell(cx, vm, newSet, &sh);
  NS_ASSERTION(NS_OK == rv, "failed to create presentation shell");
  vm->SetViewObserver((nsIViewObserver *)((PresShell*)sh));
  sh->InitialReflow(r.width, r.height);

  // Now that the document has been reflowed, use its frame tree to
  // compare against our frame tree.
  nsIFrame* root1;
  GetRootFrame(&root1);
  nsIFrame* root2;
  sh->GetRootFrame(&root2);
#if 0
  root1 = FindTopFrame(root1);
  root2 = FindTopFrame(root2);
#endif
  if (!CompareTrees(root1, root2)) {
    printf("Verify reflow failed, primary tree:\n");
    root1->List(stdout, 0);
    printf("Verification tree:\n");
    root2->List(stdout, 0);
  }

//  printf("Incremental reflow doomed view tree:\n");
//  view->List(stdout, 1);
//  view->SetVisibility(nsViewVisibility_kHide);
  cx->Stop();
  cx->SetContainer(nsnull);
  NS_RELEASE(cx);
  sh->EndObservingDocument();
  NS_RELEASE(sh);
  NS_RELEASE(vm);
}
#endif
