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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Travis Bogard <travis@netscape.com> 
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
#include "nsCOMPtr.h"
#include "nsLeafFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsIHTMLContent.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIComponentManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsHTMLAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIDocumentLoader.h"
#include "nsIPref.h"
#include "nsFrameSetFrame.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsLayoutAtoms.h"
#include "nsIChromeEventHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"
#include "nsXPIDLString.h"
#include "nsIScrollable.h"
#include "nsINameSpaceManager.h"
#include "nsIPrintContext.h"
#include "nsIPrintPreviewContext.h"
#include "nsIWidget.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMWindow.h"
#include "nsIRenderingContext.h"

// For Accessibility 
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"

#ifdef INCLUDE_XUL
#include "nsIDOMXULElement.h"
#include "nsIBoxObject.h"
#include "nsIBrowserBoxObject.h"
#include "nsISHistory.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsISecureBrowserUI.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#endif

class nsHTMLFrame;

static NS_DEFINE_IID(kIFramesetFrameIID, NS_IFRAMESETFRAME_IID);
static NS_DEFINE_CID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_CID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_CID(kCChildCID, NS_CHILD_CID);

// Bug 8065: Limit content frame depth to some reasonable level.
//           This does not count chrome frames when determining depth,
//           nor does it prevent chrome recursion.
#define MAX_DEPTH_CONTENT_FRAMES 25

/*******************************************************************************
 * FrameLoadingInfo 
 ******************************************************************************/
class FrameLoadingInfo : public nsISupports
{
public:
  FrameLoadingInfo(const nsSize& aSize);

  // nsISupports interface...
  NS_DECL_ISUPPORTS

protected:
  virtual ~FrameLoadingInfo() {}

public:
  nsSize mFrameSize;
};


/*******************************************************************************
 * nsHTMLFrameOuterFrame
 ******************************************************************************/
#define nsHTMLFrameOuterFrameSuper nsHTMLContainerFrame

class nsHTMLFrameOuterFrame : public nsHTMLFrameOuterFrameSuper {

public:
  nsHTMLFrameOuterFrame();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif

  // nsISupports   
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint);

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

  NS_IMETHOD  VerifyTree() const;
  PRBool IsInline();

protected:
  virtual ~nsHTMLFrameOuterFrame();
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);
  virtual PRIntn GetSkipSides() const;
  PRBool mIsInline;
  nsCOMPtr<nsIPresContext> mPresContext;
};

/*******************************************************************************
 * nsHTMLFrameInnerFrame
 ******************************************************************************/
class nsHTMLFrameInnerFrame : public nsLeafFrame,
                              public nsIWebProgressListener,
                              public nsSupportsWeakReference
{
public:
  nsHTMLFrameInnerFrame();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return 2; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return 1; }

  NS_DECL_NSIWEBPROGRESSLISTENER

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif

  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  /**
    * @see nsIFrame::Paint
    */
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);

  /**
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD DidReflow(nsIPresContext* aPresContext,
                       nsDidReflowStatus aStatus);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD GetParentContent(nsIContent*& aContent);
  PRBool GetURL(nsIContent* aContent, nsString& aResult);
  PRBool GetName(nsIContent* aContent, nsString& aResult);
  PRInt32 GetScrolling(nsIContent* aContent, PRBool aStandardMode);
  nsFrameborder GetFrameBorder(PRBool aStandardMode);
  PRInt32 GetMarginWidth(nsIPresContext* aPresContext, nsIContent* aContent);
  PRInt32 GetMarginHeight(nsIPresContext* aPresContext, nsIContent* aContent);

  nsresult ReloadURL(nsIPresContext* aPresContext);

friend class nsHTMLFrameOuterFrame;

protected:
  nsresult CreateDocShell(nsIPresContext* aPresContext);
  nsresult DoLoadURL(nsIPresContext* aPresContext);
  nsresult CreateViewAndWidget(nsIPresContext* aPresContext,
                               nsIWidget*&     aWidget);

  virtual ~nsHTMLFrameInnerFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  nsCOMPtr<nsIBaseWindow> mSubShell;
  nsWeakPtr mPresShellWeak;   // weak reference to the nsIPresShell
  PRBool mCreatingViewer;
};


/*******************************************************************************
 * nsHTMLFrameOuterFrame
 ******************************************************************************/
nsHTMLFrameOuterFrame::nsHTMLFrameOuterFrame()
  : nsHTMLContainerFrame()
{
  mIsInline = PR_FALSE;
}

nsHTMLFrameOuterFrame::~nsHTMLFrameOuterFrame()
{
  //printf("nsHTMLFrameOuterFrame destructor %X \n", this);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsHTMLFrameOuterFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mContent);
    return accService->CreateHTMLIFrameAccessible(node, mPresContext, aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsHTMLFrameOuterFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  return nsHTMLFrameOuterFrameSuper::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::Init(nsIPresContext*  aPresContext,
                            nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIStyleContext* aContext,
                            nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext;
  // determine if we are a <frame> or <iframe>
  if (aContent) {
    nsCOMPtr<nsIDOMHTMLFrameElement> frameElem = do_QueryInterface(aContent);
    mIsInline = frameElem ? PR_FALSE : PR_TRUE;
  }

  nsresult rv =  nsHTMLFrameOuterFrameSuper::Init(aPresContext, aContent, aParent,
                                                  aContext, aPrevInFlow);
  if (NS_FAILED(rv))
    return rv;

  const nsStyleDisplay* disp;
  aParent->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));
  if (disp->mDisplay == NS_STYLE_DISPLAY_DECK) {
    nsIView* view = nsnull;
    GetView(aPresContext, &view);

    if (!view) {
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext,this,mStyleContext,nsnull,PR_TRUE); 
      GetView(aPresContext, &view);
    }

    nsIWidget* widget;
    view->GetWidget(widget);

    if (!widget)
      view->CreateWidget(kCChildCID);   
  }

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsIFrame* firstChild = new (shell.get()) nsHTMLFrameInnerFrame;
  if (firstChild) {
    mFrames.SetFrames(firstChild);
    // Resolve the style context for the inner frame
    nsresult rv = NS_OK;
    nsIStyleContext *innerStyleContext = nsnull;
    rv = aPresContext->ResolveStyleContextFor(mContent, mStyleContext,
                                              PR_FALSE,
                                              &innerStyleContext);
    if (NS_SUCCEEDED(rv)) {
      rv = firstChild->Init(aPresContext, mContent, this, innerStyleContext, nsnull);
      // have to release the context: Init does its own AddRef...
      NS_RELEASE(innerStyleContext);
    } else {
      NS_WARNING( "Error resolving style for InnerFrame in nsHTMLFrameOuterFrame");
    }
    if (NS_FAILED(rv)){
      NS_WARNING( "Error initializing InnerFrame in nsHTMLFrameOuterFrame");
      return rv;
    }
  } else {
    NS_WARNING("no memory allocating inner frame in nsHTMLFrameOuterFrame");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

PRIntn
nsHTMLFrameOuterFrame::GetSkipSides() const
{
  return 0;
}

void 
nsHTMLFrameOuterFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsHTMLReflowMetrics& aDesiredSize)
{
  // <frame> processing does not use this routine, only <iframe>
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);

  // If no width/height was specified, use 300/150.
  // This is for compatability with IE.
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
    aDesiredSize.width = aReflowState.mComputedWidth;
  }
  else {
    aDesiredSize.width = NSIntPixelsToTwips(300, p2t);
  }
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
    aDesiredSize.height = aReflowState.mComputedHeight;
  }
  else {
    aDesiredSize.height = NSIntPixelsToTwips(150, p2t);
  }
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // For unknown reasons, the maxElementSize for the InnerFrame is used, but the
  // maxElementSize for the OuterFrame is ignored, the following is not used!
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
}

PRBool nsHTMLFrameOuterFrame::IsInline()
{ 
  return mIsInline;
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::Paint(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible) {
    return NS_OK;
  }

  //printf("outer paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  nsIFrame* firstChild = mFrames.FirstChild();
  if (nsnull != firstChild) {
    firstChild->Paint(aPresContext, aRenderingContext, aDirtyRect,
                      aWhichLayer);
  }
  if (IsInline()) {
    return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                       aDirtyRect, aWhichLayer);
  } else {
    return NS_OK;
  }
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFrameOuterFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("FrameOuter", aResult);
}
#endif

NS_IMETHODIMP
nsHTMLFrameOuterFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::htmlFrameOuterFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::Reflow(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFrameOuterFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aDesiredSize, aStatus);
  //printf("OuterFrame::Reflow %X (%d,%d) \n", this, aReflowState.availableWidth, aReflowState.availableHeight); 
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsHTMLFrameOuterFrame::Reflow: maxSize=%d,%d reason=%d",
      aReflowState.availableWidth, aReflowState.availableHeight, aReflowState.reason));

  if (IsInline()) {
    GetDesiredSize(aPresContext, aReflowState, aDesiredSize); // IFRAME
  } else {
    aDesiredSize.width  = aReflowState.availableWidth; // FRAME
    aDesiredSize.height = aReflowState.availableHeight;
    if (aDesiredSize.maxElementSize) { // Probably not used...
      aDesiredSize.maxElementSize->width = aDesiredSize.width;
      aDesiredSize.maxElementSize->height = aDesiredSize.height;
    }
  }

  nsSize innerSize(aDesiredSize.width, aDesiredSize.height);
  nsPoint offset(0,0);
  nsMargin border = aReflowState.mComputedBorderPadding;
  if (IsInline()) {
    offset.x = border.left;
    offset.y = border.top;
    // XXX Don't subtract the border!!! The size we are given does not include our
    // border! -EDV
    //innerSize.width  -= border.left + border.right;
    //innerSize.height -= border.top  + border.bottom;

    // we now need to add our border in. -EDV
    aDesiredSize.width += border.left + border.right;
    aDesiredSize.height += border.top + border.bottom;
  }

  // Reflow the child and get its desired size
  nsIFrame* firstChild = mFrames.FirstChild();
  nsHTMLReflowMetrics kidMetrics(aDesiredSize.maxElementSize);
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, firstChild,
                                     innerSize);
  ReflowChild(firstChild, aPresContext, kidMetrics, kidReflowState,
              offset.x, offset.y, 0, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

  // For unknown reasons, the maxElementSize for the InnerFrame is used, but the
  // maxElementSize for the OuterFrame is ignored, add in border here to prevent
  // a table from shrinking inside the iframe's border when resized.
  if (IsInline()) {
    if (kidMetrics.maxElementSize) {
      kidMetrics.maxElementSize->width += border.left + border.right;
      kidMetrics.maxElementSize->height += border.top + border.bottom;
    }
  }

  // Place and size the child
  FinishReflowChild(firstChild, aPresContext, kidMetrics, offset.x, offset.y, 0);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsHTMLFrameOuterFrame::Reflow: size=%d,%d status=%x",
      aDesiredSize.width, aDesiredSize.height, aStatus));

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::VerifyTree() const
{
  // XXX Completely disabled for now; once pseud-frames are reworked
  // then we can turn it back on.
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::AttributeChanged(nsIPresContext* aPresContext,
                                        nsIContent* aChild,
                                        PRInt32 aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        PRInt32 aModType, 
                                        PRInt32 aHint)
{
  nsCOMPtr<nsIAtom> type;
  aChild->GetTag(*getter_AddRefs(type));

  if (((nsHTMLAtoms::src == aAttribute) && (nsHTMLAtoms::object != type)) ||
     ((nsHTMLAtoms::data == aAttribute) && (nsHTMLAtoms::object == type))) {
    nsHTMLFrameInnerFrame* firstChild = NS_STATIC_CAST(nsHTMLFrameInnerFrame*,
                                        mFrames.FirstChild());
    if (firstChild) {
      firstChild->ReloadURL(aPresContext);
    }
  }
  // If the noResize attribute changes, dis/allow frame to be resized
  else if (nsHTMLAtoms::noresize == aAttribute) {
    nsCOMPtr<nsIContent> parentContent;
    mContent->GetParent(*getter_AddRefs(parentContent));

    nsCOMPtr<nsIAtom> parentTag;
    parentContent->GetTag(*getter_AddRefs(parentTag));

    if (nsHTMLAtoms::frameset == parentTag) {
      nsIFrame* parentFrame = nsnull;
      GetParent(&parentFrame);

      if (parentFrame) {
        // There is no interface for kIFramesetFrameIID
        // so QI'ing to concrete class, yay!
        nsHTMLFramesetFrame* framesetFrame = nsnull;
        parentFrame->QueryInterface(kIFramesetFrameIID, (void **)&framesetFrame);
        if (framesetFrame) {
          framesetFrame->RecalculateBorderResize();
        }
      }
    }
  }
  else if (aAttribute == nsHTMLAtoms::type) {
    nsHTMLFrameInnerFrame* firstChild = NS_STATIC_CAST(nsHTMLFrameInnerFrame*,
                                        mFrames.FirstChild());
    if (!firstChild)
      return NS_OK;

    nsAutoString value;
    aChild->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, value);
    
    // Notify our enclosing chrome that the primary content shell
    // has changed.
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(firstChild->mSubShell));
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(firstChild->mSubShell));
  
    // If our container is a web-shell, inform it that it has a new
    // child. If it's not a web-shell then some things will not operate
    // properly.
    nsCOMPtr<nsISupports> container;
    aPresContext->GetContainer(getter_AddRefs(container));
    if (container) {
      nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(container));
      if (parentAsNode) {
        nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(parentAsNode));
        
        nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
        parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
        if (parentTreeOwner)
          parentTreeOwner->ContentShellAdded(docShellAsItem, 
            value.EqualsIgnoreCase("content-primary") ? PR_TRUE : PR_FALSE, 
            value.get());
      }
    }
  }
  return NS_OK;
}

nsresult
NS_NewHTMLFrameOuterFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLFrameOuterFrame* it = new (aPresShell) nsHTMLFrameOuterFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

/*******************************************************************************
 * nsHTMLFrameInnerFrame
 ******************************************************************************/
nsHTMLFrameInnerFrame::nsHTMLFrameInnerFrame()
  : nsLeafFrame()
{
  mCreatingViewer = PR_FALSE;
  mPresShellWeak = nsnull;
}

nsHTMLFrameInnerFrame::~nsHTMLFrameInnerFrame()
{
   //printf("nsHTMLFrameInnerFrame destructor %X \n", this);

#ifdef INCLUDE_XUL
  nsCOMPtr<nsIDOMXULElement> xulElt(do_QueryInterface(mContent));
  if (xulElt && mSubShell) {
    // We might be a XUL browser and may need to store the current URL in our box object.
    nsCOMPtr<nsIBoxObject> boxObject;
    xulElt->GetBoxObject(getter_AddRefs(boxObject));
    if (boxObject) {
      nsCOMPtr<nsIBrowserBoxObject> browser(do_QueryInterface(boxObject));
      if (browser) {
        nsCOMPtr<nsIWebNavigation> webShell(do_QueryInterface(mSubShell));
        nsCOMPtr<nsISHistory> hist;
        webShell->GetSessionHistory(getter_AddRefs(hist));
        if (hist)
          boxObject->SetPropertyAsSupports(NS_LITERAL_STRING("history").get(), hist);
        nsCOMPtr<nsISupports> supports;
        boxObject->GetPropertyAsSupports(NS_LITERAL_STRING("listenerkungfu").get(), getter_AddRefs(supports));
        if (supports)
          boxObject->SetPropertyAsSupports(NS_LITERAL_STRING("listener").get(), supports);
      }
    }
  }
#endif

  nsCOMPtr<nsIDOMWindow> win(do_GetInterface(mSubShell));
  nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(win));
  nsCOMPtr<nsIDOMEventListener> eventListener(do_QueryInterface(mContent));

  if (eventTarget && eventListener) {
    eventTarget->RemoveEventListener(NS_LITERAL_STRING("load"), eventListener,
                                     PR_FALSE);
  }

  if(mSubShell) {
  // notify the pres shell that a docshell has been destroyed
    if (mPresShellWeak) {
      nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
      if (ps) {
        ps->SetSubShellFor(mContent, nsnull);
      }
    }
    mSubShell->Destroy();
  }
  mSubShell = nsnull; // This is the location it was released before...
                      // Not sure if there is ordering depending on this.
}

PRBool nsHTMLFrameInnerFrame::GetURL(nsIContent* aContent, nsString& aResult)
{
  aResult.SetLength(0);    
  nsCOMPtr<nsIAtom> type;
  aContent->GetTag(*getter_AddRefs(type));
  
  if (type.get() == nsHTMLAtoms::object) {
    if (NS_CONTENT_ATTR_HAS_VALUE == (aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::data, aResult)))
      if (aResult.Length() > 0)
        return PR_TRUE;
  }else
    if (NS_CONTENT_ATTR_HAS_VALUE == (aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, aResult)))
      if (aResult.Length() > 0)
        return PR_TRUE;

  return PR_FALSE;
}

PRBool nsHTMLFrameInnerFrame::GetName(nsIContent* aContent, nsString& aResult)
{
  aResult.SetLength(0);     

  if (NS_CONTENT_ATTR_HAS_VALUE == (aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, aResult))) {
    if (aResult.Length() > 0) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRInt32 nsHTMLFrameInnerFrame::GetScrolling(nsIContent* aContent, PRBool aStandardMode)
{
  PRInt32 returnValue = -1;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &rv);
  if (NS_SUCCEEDED(rv) && content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::scrolling, value))) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        PRInt32 intValue;
        intValue = value.GetIntValue();
        if (!aStandardMode) {
          if ((NS_STYLE_FRAME_ON == intValue) || (NS_STYLE_FRAME_SCROLL == intValue)) {
            returnValue = NS_STYLE_OVERFLOW_SCROLL;
          } else if ((NS_STYLE_FRAME_OFF == intValue) || (NS_STYLE_FRAME_NOSCROLL == intValue)) {
            returnValue = NS_STYLE_OVERFLOW_HIDDEN;
          }
        } else {
          if (NS_STYLE_FRAME_YES == intValue) {
            returnValue = NS_STYLE_OVERFLOW_SCROLL;
          } else if (NS_STYLE_FRAME_NO == intValue) {
            returnValue = NS_STYLE_OVERFLOW_HIDDEN;
          } else if (NS_STYLE_FRAME_AUTO == intValue) {
            returnValue = NS_STYLE_OVERFLOW_AUTO;
          }
        }
      }      
    }

    // Check style for overflow
    const nsStyleDisplay* display;
    GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
    if (display->mOverflow)
      returnValue = display->mOverflow;

  }
  return returnValue;
}

nsFrameborder nsHTMLFrameInnerFrame::GetFrameBorder(PRBool aStandardMode)
{
  nsFrameborder rv = eFrameborder_Notset;
  nsresult res = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &res);
  if (NS_SUCCEEDED(res) && content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::frameborder, value))) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        PRInt32 intValue;
        intValue = value.GetIntValue();
        if (!aStandardMode) {
          if (NS_STYLE_FRAME_YES == intValue) {
            rv = eFrameborder_Yes;
          } 
          else if (NS_STYLE_FRAME_NO == intValue) {
            rv = eFrameborder_No;
          }
        } else {
          if (NS_STYLE_FRAME_0 == intValue) {
            rv = eFrameborder_No;
          } 
          else if (NS_STYLE_FRAME_1 == intValue) {
            rv = eFrameborder_Yes;
          }
        }
      }
    }
  }
  // XXX if we get here, check for nsIDOMFRAMESETElement interface
  return rv;
}


PRInt32 nsHTMLFrameInnerFrame::GetMarginWidth(nsIPresContext* aPresContext, nsIContent* aContent)
{
  PRInt32 marginWidth = -1;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &rv);
  if (NS_SUCCEEDED(rv) && content) {
    nsHTMLValue value;
    content->GetHTMLAttribute(nsHTMLAtoms::marginwidth, value);
    if (eHTMLUnit_Pixel == value.GetUnit())
      return value.GetPixelValue();
  }
  return marginWidth;
}

PRInt32 nsHTMLFrameInnerFrame::GetMarginHeight(nsIPresContext* aPresContext, nsIContent* aContent)
{
  PRInt32 marginHeight = -1;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &rv);
  if (NS_SUCCEEDED(rv) && content) {
    nsHTMLValue value;
    content->GetHTMLAttribute(nsHTMLAtoms::marginheight, value);
    if (eHTMLUnit_Pixel == value.GetUnit())
      return value.GetPixelValue();
  }
  return marginHeight;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);

  if (aIID.Equals(NS_GET_IID(nsIWebProgressListener))) {
    nsISupports *tmp = NS_STATIC_CAST(nsIWebProgressListener *, this);
    *aInstancePtr = tmp;
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference))) {
    nsISupports *tmp = NS_STATIC_CAST(nsISupportsWeakReference *, this);
    *aInstancePtr = tmp;
    return NS_OK;
  }

  return nsLeafFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::OnStateChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRInt32 aStateFlags, PRUint32 aStatus)
{
  if (!((~aStateFlags) & (nsIWebProgressListener::STATE_IS_DOCUMENT |
                          nsIWebProgressListener::STATE_TRANSFERRING))) {
    nsCOMPtr<nsIDOMWindow> win(do_GetInterface(mSubShell));
    nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(win));
    nsCOMPtr<nsIDOMEventListener> eventListener(do_QueryInterface(mContent));

    if (eventTarget && eventListener) {
      eventTarget->AddEventListener(NS_LITERAL_STRING("load"), eventListener,
                                    PR_FALSE);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::OnProgressChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest,
                                        PRInt32 aCurSelfProgress,
                                        PRInt32 aMaxSelfProgress,
                                        PRInt32 aCurTotalProgress,
                                        PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::OnLocationChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest, nsIURI *location)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::OnStatusChange(nsIWebProgress *aWebProgress,
                                      nsIRequest *aRequest,
                                      nsresult aStatus,
                                      const PRUnichar *aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::OnSecurityChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest, PRInt32 state)
{
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFrameInnerFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("FrameInner", aResult);
}
#endif

NS_IMETHODIMP
nsHTMLFrameInnerFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::htmlFrameInnerFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Paint(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags)
{
  //printf("inner paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  // if there is not web shell paint based on our background color, 
  // otherwise let the web shell paint the sub document 

  // isPaginated is a temporary fix for Bug 75737 
  // and this should all be fixed correctly by Bug 75739
  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);
   if (!mSubShell && !isPaginated) {
    const nsStyleBackground* color =
      (const nsStyleBackground*)mStyleContext->GetStyleData(eStyleStruct_Background);
    aRenderingContext.SetColor(color->mBackgroundColor);
    aRenderingContext.FillRect(mRect);
  }
  DO_GLOBAL_REFLOW_COUNT_DSP("nsHTMLFrameInnerFrame", &aRenderingContext);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::GetParentContent(nsIContent*& aContent)
{
  nsHTMLFrameOuterFrame* parent;
  nsresult rv = GetParent((nsIFrame**)&parent);
  if (NS_SUCCEEDED(rv) && parent) {
    rv = parent->GetContent(&aContent);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::DidReflow(nsIPresContext* aPresContext,
                        nsDidReflowStatus aStatus)
{
  nsresult rv = nsLeafFrame::DidReflow(aPresContext, aStatus);


  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsIView* view = nsnull;
    GetView(aPresContext, &view);
    if (view) {
      const nsStyleVisibility* vis;
      GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)vis));
      nsViewVisibility newVis = vis->IsVisible() ? nsViewVisibility_kShow : nsViewVisibility_kHide;
      nsViewVisibility oldVis;
      // only change if different.
      view->GetVisibility(oldVis);
      if (newVis != oldVis) 
        view->SetVisibility(newVis);
    }
  }
  
  return rv;
}

nsresult
nsHTMLFrameInnerFrame::CreateDocShell(nsIPresContext* aPresContext)
{
  nsresult rv;
  nsCOMPtr<nsIContent> parentContent;
  GetParentContent(*getter_AddRefs(parentContent));

  // Bug 8065: Don't exceed some maximum depth in content frames (MAX_DEPTH_CONTENT_FRAMES)
  PRInt32 depth = 0;
  nsCOMPtr<nsISupports> parentAsSupports;
  aPresContext->GetContainer(getter_AddRefs(parentAsSupports));
  if (parentAsSupports) {
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(parentAsSupports));
    while (parentAsItem) {
      depth++;
      if (MAX_DEPTH_CONTENT_FRAMES < depth)
        return NS_ERROR_UNEXPECTED; // Too deep, give up!  (silently?)

      // Only count depth on content, not chrome.
      // If we wanted to limit total depth, skip the following check:
      PRInt32 parentType;
      parentAsItem->GetItemType(&parentType);
      if (nsIDocShellTreeItem::typeContent == parentType) {
        nsIDocShellTreeItem* temp = parentAsItem;
        temp->GetParent(getter_AddRefs(parentAsItem));
      } else {
        break; // we have exited content, stop counting, depth is OK!
      }
    }
  }

  mSubShell = do_CreateInstance(kWebShellCID);
  NS_ENSURE_TRUE(mSubShell, NS_ERROR_FAILURE);

  // notify the pres shell that a docshell has been created
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell)
  {
    nsCOMPtr<nsISupports> subShellAsSupports(do_QueryInterface(mSubShell));
    NS_ENSURE_TRUE(subShellAsSupports, NS_ERROR_FAILURE);
    presShell->SetSubShellFor(mContent, subShellAsSupports);
    //We need to be able to get back to the presShell to unset the subshell at destruction
    mPresShellWeak = getter_AddRefs(NS_GetWeakReference(presShell));   
  }
  
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mSubShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  // pass along marginwidth, marginheight, scrolling so sub document can use it
  docShell->SetMarginWidth(GetMarginWidth(aPresContext, parentContent));
  docShell->SetMarginHeight(GetMarginHeight(aPresContext, parentContent));
  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(&mode);
  // Current and initial scrolling is set so that all succeeding docs
  // will use the scrolling value set here, regardless if scrolling is
  // set by viewing a particular document (e.g. XUL turns off scrolling)
  nsCOMPtr<nsIScrollable> scrollableContainer(do_QueryInterface(mSubShell));
  if (scrollableContainer) {
    scrollableContainer->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
      GetScrolling(parentContent, mode));
    scrollableContainer->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
      GetScrolling(parentContent, mode));
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mSubShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  nsString frameName;
  if (GetName(parentContent, frameName)) {
    docShellAsItem->SetName(frameName.get());
  }

  // If our container is a web-shell, inform it that it has a new
  // child. If it's not a web-shell then some things will not operate
  // properly.
  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  if (container) {
    nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(container));
    if (parentAsNode) {
      nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(parentAsNode));
      PRInt32 parentType;
      parentAsItem->GetItemType(&parentType);

      nsIAtom* typeAtom = NS_NewAtom("type");
      nsAutoString value, valuePiece;
      PRBool isContent;

      isContent = PR_FALSE;
      if (NS_SUCCEEDED(parentContent->GetAttr(kNameSpaceID_None,
         typeAtom, value))) {

        // we accept "content" and "content-xxx" values.
        // at time of writing, we expect "xxx" to be "primary", but
        // someday it might be an integer expressing priority
        value.Left(valuePiece, 7);
        if (valuePiece.EqualsIgnoreCase("content") &&
           (value.Length() == 7 ||
              value.Mid(valuePiece, 7, 1) == 1 && valuePiece.EqualsWithConversion("-")))
            isContent = PR_TRUE;
      }
      NS_IF_RELEASE(typeAtom);
      if (isContent) {
        // The web shell's type is content.
        docShellAsItem->SetItemType(nsIDocShellTreeItem::typeContent);
      } else {
        // Inherit our type from our parent webshell.  If it is
        // chrome, we'll be chrome.  If it is content, we'll be
        // content.
        docShellAsItem->SetItemType(parentType);
      }
      
      parentAsNode->AddChild(docShellAsItem);

      if (isContent) {
        nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
        parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
        if(parentTreeOwner)
          parentTreeOwner->ContentShellAdded(docShellAsItem, 
            value.EqualsIgnoreCase("content-primary") ? PR_TRUE : PR_FALSE, 
            value.get());
      }
      // connect the container...
      nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mSubShell));
      nsCOMPtr<nsIWebShellContainer> outerContainer(do_QueryInterface(container));
      if (outerContainer)
        webShell->SetContainer(outerContainer);


      // Make sure all shells have links back to the content element in the
      // nearest enclosing chrome shell.
      nsCOMPtr<nsIDocShell> parentShell(do_QueryInterface(parentAsNode));
      nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
      if (parentType == nsIDocShellTreeItem::typeChrome) {
        // Our parent shell is a chrome shell. It is therefore our nearest
        // enclosing chrome shell.
        chromeEventHandler = do_QueryInterface(mContent);
        NS_WARN_IF_FALSE(chromeEventHandler, "This mContent should implement this.");
      }
      else {
        // Our parent shell is a content shell. Get the chrome info from
        // it and use that for our shell as well.
        parentShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
      }

      docShell->SetChromeEventHandler(chromeEventHandler);
    }
  }

  nsIWidget* widget;
  rv = CreateViewAndWidget(aPresContext, widget);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mSubShell->InitWindow(nsnull, widget, 0, 0, 10, 10);
  mSubShell->Create();

  mSubShell->SetVisibility(PR_TRUE);

  return NS_OK;
}

static PRBool CheckForBrowser(nsIContent* aContent, nsIBaseWindow* aShell)
{
  PRBool didReload = PR_FALSE;
#ifdef INCLUDE_XUL
  // XXXjag see bug 68662
  nsCOMPtr<nsIDOMXULElement> xulElt(do_QueryInterface(aContent));
  if (xulElt) {
    // We might be a XUL browser and may have stored state in our box object.
    nsCOMPtr<nsIBoxObject> boxObject;
    xulElt->GetBoxObject(getter_AddRefs(boxObject));
    if (boxObject) {
      nsCOMPtr<nsIBrowserBoxObject> browser(do_QueryInterface(boxObject));
      if (browser) {
        nsCOMPtr<nsISupports> supp;
        /* reregister the progress listener */
        boxObject->GetPropertyAsSupports(NS_LITERAL_STRING("listener").get(), getter_AddRefs(supp));
        if (supp) {
          nsCOMPtr<nsIWebProgressListener> listener(do_QueryInterface(supp));
          if (listener) {
            nsCOMPtr<nsIDocShell> docShell;
            browser->GetDocShell(getter_AddRefs(docShell));
            nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(docShell));
            webProgress->AddProgressListener(listener);
            boxObject->RemoveProperty(NS_LITERAL_STRING("listener").get());
          }
        }

        /* reinitialize the security module */
        boxObject->GetPropertyAsSupports(NS_LITERAL_STRING("xulwindow").get(), getter_AddRefs(supp));
        nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(supp));
        boxObject->GetPropertyAsSupports(NS_LITERAL_STRING("secureBrowserUI").get(), getter_AddRefs(supp));
        nsCOMPtr<nsISecureBrowserUI> secureBrowserUI(do_QueryInterface(supp));
        if (domWindow && secureBrowserUI) {
          nsCOMPtr<nsIDOMWindow> contentWindow;
          domWindow->GetContent(getter_AddRefs(contentWindow));
          nsCOMPtr<nsIDOMDocument> doc;
          domWindow->GetDocument(getter_AddRefs(doc));
          if (contentWindow && doc) {
            nsCOMPtr<nsIDOMElement> button;
            doc->GetElementById(NS_LITERAL_STRING("security-button"), getter_AddRefs(button));
            if (button)
              secureBrowserUI->Init(contentWindow, button);
          }
        }

        /* set session history on the new docshell */
        boxObject->GetPropertyAsSupports(NS_LITERAL_STRING("history").get(), getter_AddRefs(supp));
        if (supp) {
          nsCOMPtr<nsISHistory> hist(do_QueryInterface(supp));
          if (hist) {
            nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(aShell));
            webNav->SetSessionHistory(hist);
            nsCOMPtr<nsIWebNavigation> histNav(do_QueryInterface(hist));
            histNav->Reload(0);
            didReload = PR_TRUE;
            boxObject->RemoveProperty(NS_LITERAL_STRING("history").get());
          }
        }
      }
    }
  }
#endif
  return didReload;
}

nsresult
nsHTMLFrameInnerFrame::DoLoadURL(nsIPresContext* aPresContext)
{
  // Bug 8065: Preventing frame nesting recursion - if the frames are
  //           too deep we don't create a mSubShell, so this isn't an assert:
  if (!mSubShell) return NS_OK;

  // Prevent recursion
  mCreatingViewer=PR_TRUE;

  // Get the URL to load
  nsCOMPtr<nsIContent> parentContent;
  nsresult rv = GetParentContent(*getter_AddRefs(parentContent));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && parentContent, rv);

  PRBool load = !CheckForBrowser(parentContent, mSubShell);

  if (load) {
    nsAutoString url;
    GetURL(parentContent, url);
    url.Trim(" \t\n\r");
    if (url.IsEmpty())  // Load about:blank into a frame if not URL is specified (bug 35986)
      url = NS_ConvertASCIItoUCS2("about:blank");

    // Make an absolute URL
    nsCOMPtr<nsIURI> baseURL;
    nsCOMPtr<nsIHTMLContent> htmlContent = do_QueryInterface(parentContent, &rv);
    if (NS_SUCCEEDED(rv) && htmlContent) {
      htmlContent->GetBaseURL(*getter_AddRefs(baseURL));
    }
    else {
      nsCOMPtr<nsIDocument> doc;
      rv = parentContent->GetDocument(*getter_AddRefs(doc));
      if (NS_SUCCEEDED(rv) && doc) {
        doc->GetBaseURL(*getter_AddRefs(baseURL));
      }
    }
    if (!baseURL) return NS_ERROR_NULL_POINTER;

    nsAutoString absURL;
    rv = NS_MakeAbsoluteURI(absURL, url, baseURL);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), absURL, nsnull);

    // Check for security
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get base URL
    nsCOMPtr<nsIURI> baseURI;
    rv = aPresContext->GetBaseURL(getter_AddRefs(baseURI));

    // Get docshell and create load info
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mSubShell));
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

    // Get referring URL
    nsCOMPtr<nsIURI> referrer;
    nsCOMPtr<nsIPrincipal> principal;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);
    // If we were called from script, get the referring URL from the script
    if (principal) {
      nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
      if (codebase) {
        rv = codebase->GetURI(getter_AddRefs(referrer));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // Pass the script principal to the docshell
      nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
      loadInfo->SetOwner(owner);
    }
    if (!referrer) { // We're not being called form script, tell the docshell
                     // to inherit an owner from the current document.
      loadInfo->SetInheritOwner(PR_TRUE);
      referrer = baseURI;
    }

    loadInfo->SetReferrer(referrer);

    // Check if we are allowed to load absURL
    nsCOMPtr<nsIURI> newURI;
    rv = NS_NewURI(getter_AddRefs(newURI), absURL, baseURI);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = secMan->CheckLoadURI(referrer, newURI, nsIScriptSecurityManager::STANDARD);
    if (NS_FAILED(rv))
      return rv; // We're not

    nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(mSubShell));

    if (webProgress) {
      webProgress->AddProgressListener(this);
    }

    rv = docShell->LoadURI(uri, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to load URL");
  }

  return rv;
}


nsresult
nsHTMLFrameInnerFrame::CreateViewAndWidget(nsIPresContext* aPresContext,
                                           nsIWidget*&     aWidget)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aWidget);

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (!presShell) return NS_ERROR_FAILURE;

  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);

  // create, init, set the parent of the view
  nsIView* view;
  nsresult rv = nsComponentManager::CreateInstance(kCViewCID, nsnull, NS_GET_IID(nsIView),
                                        (void **)&view);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "Could not create view for nsHTMLFrame");
    return rv;
  }

  nsIView* parView;
  nsPoint origin;
  GetOffsetFromView(aPresContext, origin, &parView);  
  nsRect viewBounds(origin.x, origin.y, 10, 10);

  nsCOMPtr<nsIViewManager> viewMan;
  presShell->GetViewManager(getter_AddRefs(viewMan));  
  rv = view->Init(viewMan, viewBounds, parView);
  viewMan->InsertChild(parView, view, 0);

  nsWidgetInitData initData;
  initData.clipChildren = PR_TRUE;
  initData.clipSiblings = PR_TRUE;

  rv = view->CreateWidget(kCChildCID, &initData);
  SetView(aPresContext, view);

  // if the visibility is hidden, reflect that in the view
  const nsStyleVisibility* vis;
  GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)vis));
  if (!vis->IsVisible()) {
    view->SetVisibility(nsViewVisibility_kHide);
  }
  view->GetWidget(aWidget);
  return rv;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Init(nsIPresContext*  aPresContext,
                            nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIStyleContext* aContext,
                            nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsLeafFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  if (NS_FAILED(rv))
    return rv;

  // determine if we are a printcontext
  PRBool shouldCreateDoc = PR_TRUE;
  nsCOMPtr<nsIPrintContext> thePrinterContext = do_QueryInterface(aPresContext);
  if  (thePrinterContext) {
    // we are printing
    shouldCreateDoc = PR_FALSE;
  }

  // for print preview we want to create the view and widget but 
  // we do not want to load the document, it is alerady loaded.
  nsCOMPtr<nsIPrintPreviewContext> thePrintPreviewContext = do_QueryInterface(aPresContext);
  if  (thePrintPreviewContext) {
    nsIWidget* widget;
    rv = CreateViewAndWidget(aPresContext, widget);
    NS_IF_RELEASE(widget);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // we are in PrintPreview
    shouldCreateDoc = PR_FALSE;
  }
  
  if (!mCreatingViewer && shouldCreateDoc) {
    // create the web shell
    // we do this even if the size is not positive (bug 11762)
    // we do this even if there is no src (bug 16218)
    if (!mSubShell)
      rv = CreateDocShell(aPresContext);
    // Whether or not we had to create a webshell, load the document
    if (NS_SUCCEEDED(rv)) {
      DoLoadURL(aPresContext);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Reflow(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFrameInnerFrame", aReflowState.reason);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsHTMLFrameInnerFrame::Reflow: maxSize=%d,%d reason=%d",
      aReflowState.availableWidth,
      aReflowState.availableHeight,
      aReflowState.reason));

  nsresult rv = NS_OK;

  // use the max size set in aReflowState by the nsHTMLFrameOuterFrame as our size

  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  // resize the sub document
  if(mSubShell) {
    float t2p;
    aPresContext->GetTwipsToPixels(&t2p);

    PRInt32 x = 0;
    PRInt32 y = 0;

    mSubShell->GetPositionAndSize(&x, &y, nsnull, nsnull);
    PRInt32 cx  = NSToCoordRound(aDesiredSize.width * t2p);
    PRInt32 cy = NSToCoordRound(aDesiredSize.height * t2p);
    mSubShell->SetPositionAndSize(x, y, cx, cy, PR_FALSE);

    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
      ("exit nsHTMLFrameInnerFrame::Reflow: size=%d,%d rv=%x",
      aDesiredSize.width, aDesiredSize.height, aStatus));
  }
    
  return rv;
}

// load a new url
nsresult
nsHTMLFrameInnerFrame::ReloadURL(nsIPresContext* aPresContext)
{
  return DoLoadURL(aPresContext);
}

void 
nsHTMLFrameInnerFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width  = aReflowState.availableWidth;
  aDesiredSize.height = aReflowState.availableHeight;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // For unknown reasons, the maxElementSize for the InnerFrame is used, but the
  // maxElementSize for the OuterFrame is ignored, make sure to get it right here!
  if (aDesiredSize.maxElementSize) {
    if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) ||
        (eStyleUnit_Percent == aReflowState.mStylePosition->mWidth.GetUnit())) {
      aDesiredSize.maxElementSize->width = 0; // percent width springy down to 0 px
    }
    else {
      aDesiredSize.maxElementSize->width = aDesiredSize.width;
    }
    if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) ||
        (eStyleUnit_Percent == aReflowState.mStylePosition->mHeight.GetUnit())) {
      aDesiredSize.maxElementSize->height = 0; // percent height springy down to 0px
    }
    else {
      aDesiredSize.maxElementSize->height = aDesiredSize.height;
    }
  }
}

/*******************************************************************************
 * FrameLoadingInfo
 ******************************************************************************/
FrameLoadingInfo::FrameLoadingInfo(const nsSize& aSize)
{
  NS_INIT_REFCNT();

  mFrameSize = aSize;
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ISUPPORTS0(FrameLoadingInfo)

