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
 *    Håkan Waara <hwaara@chello.se>
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
#include "nsIComponentManager.h"
#include "nsIFrameManager.h"
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
#include "nsFrameSetFrame.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIFrameLoader.h"
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
#include "nsIWebBrowserPrint.h"
#include "nsWeakReference.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include "nsIRenderingContext.h"
#include "nsIFrameFrame.h"
#include "nsIPluginViewer.h"

// For Accessibility
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"

class nsHTMLFrame;

static NS_DEFINE_CID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_CID(kCChildCID, NS_CHILD_CID);

/******************************************************************************
 * FrameLoadingInfo 
 *****************************************************************************/
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


/******************************************************************************
 * nsHTMLFrameOuterFrame
 *****************************************************************************/
#define nsHTMLFrameOuterFrameSuper nsHTMLContainerFrame

class nsHTMLFrameOuterFrame : public nsHTMLFrameOuterFrameSuper,
                              public nsIFrameFrame
{
public:
  nsHTMLFrameOuterFrame();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return 2; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return 1; }

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

  // nsIFrameFrame
  NS_IMETHOD GetDocShell(nsIDocShell **aDocShell);

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

/******************************************************************************
 * nsHTMLFrameInnerFrame
 *****************************************************************************/
class nsHTMLFrameInnerFrame : public nsLeafFrame,
                              public nsSupportsWeakReference
{
public:
  nsHTMLFrameInnerFrame();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return 2; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return 1; }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  /**
    * @see nsIFrame::Paint
    */
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);

  // Make sure we never think this frame is opaque because it has
  // a background set; we won't be painting it in most cases
  virtual PRBool CanPaintBackground() { return PR_FALSE; }

  /**
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD DidReflow(nsIPresContext*           aPresContext,
                       const nsHTMLReflowState*  aReflowState,
                       nsDidReflowStatus         aStatus);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  void GetParentContent(nsIContent** aContent);
  nsresult GetDocShell(nsIDocShell **aDocShell);

  PRBool GetURL(nsIContent* aContent, nsString& aResult);
  PRBool GetName(nsIContent* aContent, nsString& aResult);
  PRInt32 GetScrolling(nsIContent* aContent);
  nsFrameborder GetFrameBorder();
  PRInt32 GetMarginWidth(nsIContent* aContent);
  PRInt32 GetMarginHeight(nsIContent* aContent);

friend class nsHTMLFrameOuterFrame;

protected:
  nsresult ShowDocShell(nsIPresContext* aPresContext);
  nsresult CreateViewAndWidget(nsIPresContext* aPresContext,
                               nsIWidget**     aWidget);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  nsresult ReloadURL();

  nsCOMPtr<nsIFrameLoader> mFrameLoader;
  PRPackedBool mOwnsFrameLoader;

  PRPackedBool mCreatingViewer;
};


/******************************************************************************
 * nsHTMLFrameOuterFrame
 *****************************************************************************/
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
    return accService->CreateIFrameAccessible(node, aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

NS_IMETHODIMP
nsHTMLFrameOuterFrame::GetDocShell(nsIDocShell **aDocShell)
{
  *aDocShell = nsnull;

  nsHTMLFrameInnerFrame* firstChild = NS_STATIC_CAST(nsHTMLFrameInnerFrame*,
                                                     mFrames.FirstChild());
  if (!firstChild) 
    return NS_OK;

  return firstChild->GetDocShell(aDocShell);
}


//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsHTMLFrameOuterFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsIFrameFrame))) {
    nsISupports *tmp = NS_STATIC_CAST(nsIFrameFrame *, this);
    *aInstancePtr = tmp;
    return NS_OK;
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

  // nsHTMLFrameInnerFrame is going to create a view for its frame
  // right away, in the call to Init().  If we need a view for the
  // OuterFrame but we wait for the normal view creation path in
  // nsCSSFrameConstructor, then we will lose because the InnerFrame's
  // view's parent will already have been set to some outer view
  // (e.g., the canvas) when it really needs to have the OuterFrame's
  // view as its parent. So, create the OuterFrame's view right away
  // if we need it, and the InnerFrame's view will get it as the parent.
  nsIView* view = nsnull;
  GetView(aPresContext, &view);
  if (!view) {
    // To properly initialize the view we need to know the frame for the content
    // that is the parent of content for this frame. This might not be our actual
    // frame parent if we are out of flow (e.g., positioned) so our parent frame
    // may have been set to some other ancestor.
    // We look for a content parent frame in the frame property list, where it
    // will have been set by nsCSSFrameConstructor if necessary.
    nsCOMPtr<nsIAtom> contentParentAtom = do_GetAtom("contentParent");
    nsIFrame* contentParent = nsnull;
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));

    if (presShell) {
      nsCOMPtr<nsIFrameManager> frameManager;
      presShell->GetFrameManager(getter_AddRefs(frameManager));

      if (frameManager) {
        void* value;
        rv = frameManager->GetFrameProperty(this,
                                           contentParentAtom, 
                                           NS_IFRAME_MGR_REMOVE_PROP,
                                           &value);
        if (NS_SUCCEEDED(rv)) {
          contentParent = (nsIFrame*)value;
        }
      }
    }
  
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext,this,mStyleContext,contentParent,PR_TRUE); 
    GetView(aPresContext, &view);
  }

  const nsStyleDisplay* disp;
  aParent->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));
  if (disp->mDisplay == NS_STYLE_DISPLAY_DECK) {
    nsCOMPtr<nsIWidget> widget;
    view->GetWidget(*getter_AddRefs(widget));

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
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                     aDirtyRect, aWhichLayer);
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFrameOuterFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FrameOuter"), aResult);
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
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
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

  // Reflow the child and get its desired size. We'll need to convert
  // an incremental reflow to a dirty reflow unless our child is along
  // the path.
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
  FinishReflowChild(firstChild, aPresContext, nsnull,
                    kidMetrics, offset.x, offset.y, 0);

  // Determine if we need to repaint our border
  CheckInvalidateBorder(aPresContext, aDesiredSize, aReflowState);

  {
    // Invalidate the frame
    nsRect frameRect;
    GetRect(frameRect);
    nsRect rect(0, 0, frameRect.width, frameRect.height);
    if (!rect.IsEmpty()) {
      Invalidate(aPresContext, rect, PR_FALSE);
    }
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsHTMLFrameOuterFrame::Reflow: size=%d,%d status=%x",
      aDesiredSize.width, aDesiredSize.height, aStatus));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
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

  if ((type != nsHTMLAtoms::object && aAttribute == nsHTMLAtoms::src) ||
      (type == nsHTMLAtoms::object && aAttribute == nsHTMLAtoms::data)) {
    nsHTMLFrameInnerFrame* firstChild =
      NS_STATIC_CAST(nsHTMLFrameInnerFrame*, mFrames.FirstChild());

    if (firstChild && firstChild->mOwnsFrameLoader) {
      firstChild->ReloadURL();
    }
  }
  // If the noResize attribute changes, dis/allow frame to be resized
  else if (aAttribute == nsHTMLAtoms::noresize) {
    nsCOMPtr<nsIContent> parentContent;
    mContent->GetParent(*getter_AddRefs(parentContent));

    nsCOMPtr<nsIAtom> parentTag;
    parentContent->GetTag(*getter_AddRefs(parentTag));

    if (parentTag == nsHTMLAtoms::frameset) {
      nsIFrame* parentFrame = nsnull;
      GetParent(&parentFrame);

      if (parentFrame) {
        // There is no interface for nsHTMLFramesetFrame so QI'ing to
        // concrete class, yay!
        nsHTMLFramesetFrame* framesetFrame = nsnull;
        parentFrame->QueryInterface(NS_GET_IID(nsHTMLFramesetFrame),
                                    (void **)&framesetFrame);

        if (framesetFrame) {
          framesetFrame->RecalculateBorderResize();
        }
      }
    }
  }
  else if (aAttribute == nsHTMLAtoms::type) {
    nsHTMLFrameInnerFrame* firstChild = NS_STATIC_CAST(nsHTMLFrameInnerFrame*,
                                                       mFrames.FirstChild());
    if (!firstChild || !firstChild->mFrameLoader) 
      return NS_OK;

    nsAutoString value;
    aChild->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, value);

    // Notify our enclosing chrome that the primary content shell
    // has changed.

    nsCOMPtr<nsIDocShell> docShell;
    firstChild->mFrameLoader->GetDocShell(getter_AddRefs(docShell));

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));

    // If our container is a web-shell, inform it that it has a new
    // child. If it's not a web-shell then some things will not operate
    // properly.
    nsCOMPtr<nsISupports> container;
    aPresContext->GetContainer(getter_AddRefs(container));

    nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(container));

    if (parentAsNode) {
      nsCOMPtr<nsIDocShellTreeItem> parentAsItem =
        do_QueryInterface(parentAsNode);

      nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
      parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
      if (parentTreeOwner) {
        PRInt32 parentType;
        parentAsItem->GetItemType(&parentType);
        PRBool is_primary_content =
          parentType == nsIDocShellTreeItem::typeChrome &&
          value.EqualsIgnoreCase("content-primary");

        parentTreeOwner->ContentShellAdded(docShellAsItem, is_primary_content,
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

/******************************************************************************
 * nsHTMLFrameInnerFrame
 *****************************************************************************/
nsHTMLFrameInnerFrame::nsHTMLFrameInnerFrame()
  : nsLeafFrame(), mOwnsFrameLoader(PR_FALSE), mCreatingViewer(PR_FALSE)
{
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Destroy(nsIPresContext* aPresContext)
{
  
  PRBool containsPluginViewer = PR_FALSE;

  if (mFrameLoader) {
    // Get the content viewer through the docshell, but don't call
    // GetDocShell() since we don't want to create one if we don't
    // have one.

    nsCOMPtr<nsIDocShell> docShell;
    mFrameLoader->GetDocShell(getter_AddRefs(docShell));

    if (docShell) {
      nsCOMPtr<nsIContentViewer> content_viewer;
      docShell->GetContentViewer(getter_AddRefs(content_viewer));

      if (content_viewer) {
        // Mark the content viewer as non-sticky so that the presentation
        // can safely go away when this frame is destroyed.

        content_viewer->SetSticky(PR_FALSE);

        // Hide the content viewer now that the frame is going away...

        content_viewer->Hide();

        // Full-page plugins don't have a DOM so they always need to be destroyed
        // here, before this frame and window are destroyed. See bug 173938.
        // When bug 90256 is fixed this code can probably go away.
        nsCOMPtr<nsIPluginViewer> pluginViewer (do_QueryInterface(content_viewer));
        if (pluginViewer)
          containsPluginViewer = PR_TRUE;
      }
    }
  }

  if (mFrameLoader) {
    if (mOwnsFrameLoader) {
      // We own this frame loader, and we're going away, so destroy our
      // frame loader.

      mFrameLoader->Destroy();

    } else if (containsPluginViewer) {
      // If this is a full-page plugin, we need to notify content to let go
      // of the frame loader so that it will be re-recreated if this frame
      // is shown later.
      nsCOMPtr<nsIContent> content;
      GetParentContent(getter_AddRefs(content));
      nsCOMPtr<nsIFrameLoaderOwner> frame_loader_owner (do_QueryInterface(content));
      if (frame_loader_owner)
        frame_loader_owner->SetFrameLoader(nsnull);

      mFrameLoader->Destroy();
      mFrameLoader = nsnull;
    }
  }

  return nsLeafFrame::Destroy(aPresContext);
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

PRInt32 nsHTMLFrameInnerFrame::GetScrolling(nsIContent* aContent)
{
  PRInt32 returnValue = -1;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &rv);

  if (NS_SUCCEEDED(rv) && content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetHTMLAttribute(nsHTMLAtoms::scrolling, value)) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        switch (value.GetIntValue()) {
          case NS_STYLE_FRAME_ON:
          case NS_STYLE_FRAME_SCROLL:
          case NS_STYLE_FRAME_YES:
            returnValue = NS_STYLE_OVERFLOW_SCROLL;
            break;

          case NS_STYLE_FRAME_OFF:
          case NS_STYLE_FRAME_NOSCROLL:
          case NS_STYLE_FRAME_NO:
            returnValue = NS_STYLE_OVERFLOW_HIDDEN;
            break;
        
          case NS_STYLE_FRAME_AUTO:
          default:
            returnValue = NS_STYLE_OVERFLOW_AUTO;
            break;
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

nsFrameborder nsHTMLFrameInnerFrame::GetFrameBorder()
{
  nsFrameborder rv = eFrameborder_Notset;
  nsresult res = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &res);
  if (NS_SUCCEEDED(res) && content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::frameborder, value))) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        switch (value.GetIntValue())
        {
          case NS_STYLE_FRAME_1:
          case NS_STYLE_FRAME_YES:
            rv = eFrameborder_Yes;
            break;
          
          case NS_STYLE_FRAME_0:
          case NS_STYLE_FRAME_NO:
            rv = eFrameborder_No;
            break;
        }
      }
    }
  }
  // XXX if we get here, check for nsIDOMFRAMESETElement interface
  return rv;
}


PRInt32 nsHTMLFrameInnerFrame::GetMarginWidth(nsIContent* aContent)
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

PRInt32 nsHTMLFrameInnerFrame::GetMarginHeight(nsIContent* aContent)
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

  if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference))) {
    nsISupports *tmp = NS_STATIC_CAST(nsISupportsWeakReference *, this);
    *aInstancePtr = tmp;
    return NS_OK;
  }

  return nsLeafFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFrameInnerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FrameInner"), aResult);
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
  //printf("inner paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x,
  //aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  // if there is not web shell paint based on our background color, otherwise
  // let the web shell paint the sub document

  // isPaginated is a temporary fix for Bug 75737 and this should all
  // be fixed correctly by Bug 75739
  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);

  if (!isPaginated) {
    nsCOMPtr<nsIDocShell> docShell;
    GetDocShell(getter_AddRefs(docShell));

    if (!docShell) {
      const nsStyleBackground* color =
        (const nsStyleBackground*)mStyleContext->
        GetStyleData(eStyleStruct_Background);

      aRenderingContext.SetColor(color->mBackgroundColor);
      aRenderingContext.FillRect(mRect);
    }
  }

  DO_GLOBAL_REFLOW_COUNT_DSP("nsHTMLFrameInnerFrame", &aRenderingContext);

  return NS_OK;
}

void
nsHTMLFrameInnerFrame::GetParentContent(nsIContent** aContent)
{
  *aContent = nsnull;

  nsIFrame* parent = nsnull;
  GetParent(&parent);

  if (parent) {
    parent->GetContent(aContent);
  }
}

nsresult
nsHTMLFrameInnerFrame::GetDocShell(nsIDocShell **aDocShell)
{
  *aDocShell = nsnull;

  nsCOMPtr<nsIContent> content;
  GetParentContent(getter_AddRefs(content));

  if (!content) {
    // Hmm, no content in this frame (or in the parent, really),
    // that's odd, not much to be done here then.

    return NS_OK;
  }

  if (!mFrameLoader) {
    nsCOMPtr<nsIFrameLoaderOwner> frame_loader_owner =
      do_QueryInterface(content);

    if (frame_loader_owner) {
      frame_loader_owner->GetFrameLoader(getter_AddRefs(mFrameLoader));
    }

    if (!mFrameLoader) {
      nsresult rv = NS_OK;

      // No frame loader available from the content, create our own...
      mFrameLoader = do_CreateInstance(NS_FRAMELOADER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      // ... remember that we own this frame loader...
      mOwnsFrameLoader = PR_TRUE;

      // ... initialize it...
      mFrameLoader->Init(content);

      // ... and tell it to start loading.
      rv = mFrameLoader->LoadFrame();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return mFrameLoader->GetDocShell(aDocShell);
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::DidReflow(nsIPresContext*           aPresContext,
                                 const nsHTMLReflowState*  aReflowState,
                                 nsDidReflowStatus         aStatus)
{
  nsresult rv = nsLeafFrame::DidReflow(aPresContext, nsnull, aStatus);


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
      if (newVis != oldVis) {
        nsCOMPtr<nsIViewManager> vm;
        view->GetViewManager(*getter_AddRefs(vm));
        if (vm) {
          vm->SetViewVisibility(view, newVis);
        }
      }
    }
  }

  return rv;
}

nsresult
nsHTMLFrameInnerFrame::ShowDocShell(nsIPresContext* aPresContext)
{
  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));

  if (presShell) {
    // The docshell is already showing, nothing left to do...

    return NS_OK;
  }

  nsCOMPtr<nsIContent> content;
  GetParentContent(getter_AddRefs(content));

  // pass along marginwidth, marginheight, scrolling so sub document
  // can use it
  docShell->SetMarginWidth(GetMarginWidth(content));
  docShell->SetMarginHeight(GetMarginHeight(content));

  // Current and initial scrolling is set so that all succeeding docs
  // will use the scrolling value set here, regardless if scrolling is
  // set by viewing a particular document (e.g. XUL turns off scrolling)
  nsCOMPtr<nsIScrollable> sc(do_QueryInterface(docShell));

  if (sc) {
    PRInt32 scrolling = GetScrolling(content);

    sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                       scrolling);
    sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                       scrolling);
  }

  nsCOMPtr<nsIWidget> widget;

  rv = CreateViewAndWidget(aPresContext, getter_AddRefs(widget));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

  if (baseWindow) {
    baseWindow->InitWindow(nsnull, widget, 0, 0, 10, 10);

    // This is kinda whacky, this "Create()" call doesn't really
    // create anything, one starts to wonder why this was named
    // "Create"...

    baseWindow->Create();

    baseWindow->SetVisibility(PR_TRUE);
  }

  return NS_OK;
}

nsresult
nsHTMLFrameInnerFrame::CreateViewAndWidget(nsIPresContext* aPresContext,
                                           nsIWidget**     aWidget)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aWidget);

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);

  // create, init, set the parent of the view
  nsIView* view;
  nsresult rv = nsComponentManager::CreateInstance(kCViewCID, nsnull,
                                                   NS_GET_IID(nsIView),
                                                   (void **)&view);
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not create view for nsHTMLFrame");

    return rv;
  }

  nsIView* parView;
  nsPoint origin;
  GetOffsetFromView(aPresContext, origin, &parView);
  nsRect viewBounds(origin.x, origin.y, 10, 10);

  nsCOMPtr<nsIViewManager> viewMan;
  presShell->GetViewManager(getter_AddRefs(viewMan));
  rv = view->Init(viewMan, viewBounds, parView);
  // XXX put it at the end of the document order until we can do better
  viewMan->InsertChild(parView, view, nsnull, PR_TRUE);

  nsWidgetInitData initData;
  initData.clipChildren = PR_TRUE;
  initData.clipSiblings = PR_TRUE;

  rv = view->CreateWidget(kCChildCID, &initData);
  SetView(aPresContext, view);

  nsContainerFrame::SyncFrameViewProperties(aPresContext, this, nsnull, view);

  // XXX the following should be unnecessary, given the above Sync call
  // if the visibility is hidden, reflect that in the view
  const nsStyleVisibility* vis;
  GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)vis));
  if (!vis->IsVisible()) {
    viewMan->SetViewVisibility(view, nsViewVisibility_kHide);
  }
  view->GetWidget(*aWidget);
  return rv;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Init(nsIPresContext*  aPresContext,
                            nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIStyleContext* aContext,
                            nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsLeafFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);
  if (NS_FAILED(rv))
    return rv;

  // determine if we are a printcontext
  PRBool shouldCreateDoc = PR_TRUE;
  nsCOMPtr<nsIPrintContext> thePrinterContext(do_QueryInterface(aPresContext));

  if (thePrinterContext) {
    // we are printing
    shouldCreateDoc = PR_FALSE;
  }

  // for print preview we want to create the view and widget but
  // we do not want to load the document, it is alerady loaded.
  nsCOMPtr<nsIPrintPreviewContext> thePrintPreviewContext =
    do_QueryInterface(aPresContext);

  if (thePrintPreviewContext) {
    nsCOMPtr<nsIWidget> widget;
    rv = CreateViewAndWidget(aPresContext, getter_AddRefs(widget));

    if (NS_FAILED(rv)) {
      return rv;
    }

    // we are in PrintPreview
    shouldCreateDoc = PR_FALSE;
  }

  if (shouldCreateDoc) {
    ShowDocShell(aPresContext);
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

  // use the max size set in aReflowState by the nsHTMLFrameOuterFrame
  // as our size

  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  // If doing Printing or Print Preview return here
  // the printing/print preview mechanism will resize the subshell
  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);
  if (isPaginated) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell;
  GetDocShell(getter_AddRefs(docShell));

  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

  // resize the sub document
  if (baseWindow) {
    float t2p;
    aPresContext->GetTwipsToPixels(&t2p);

    PRInt32 x = 0;
    PRInt32 y = 0;

    baseWindow->GetPositionAndSize(&x, &y, nsnull, nsnull);
    PRInt32 cx = NSToCoordRound(aDesiredSize.width * t2p);
    PRInt32 cy = NSToCoordRound(aDesiredSize.height * t2p);
    baseWindow->SetPositionAndSize(x, y, cx, cy, PR_FALSE);

    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                   ("exit nsHTMLFrameInnerFrame::Reflow: size=%d,%d rv=%x",
                    aDesiredSize.width, aDesiredSize.height, aStatus));
  }

  return rv;
}

// load a new url
nsresult
nsHTMLFrameInnerFrame::ReloadURL()
{
  if (!mOwnsFrameLoader || !mFrameLoader) {
    // If we don't own the frame loader, we're not in charge of what's
    // loaded into it.

    return NS_OK;
  }

  return mFrameLoader->LoadFrame();
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

  // For unknown reasons, the maxElementSize for the InnerFrame is
  // used, but the maxElementSize for the OuterFrame is ignored, make
  // sure to get it right here!

  if (aDesiredSize.maxElementSize) {
    if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) ||
        (eStyleUnit_Percent ==
         aReflowState.mStylePosition->mWidth.GetUnit())) {
      // percent width springy down to 0 px

      aDesiredSize.maxElementSize->width = 0;
    }
    else {
      aDesiredSize.maxElementSize->width = aDesiredSize.width;
    }

    if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) ||
        (eStyleUnit_Percent ==
         aReflowState.mStylePosition->mHeight.GetUnit())) {
      // percent height springy down to 0px

      aDesiredSize.maxElementSize->height = 0;
    }
    else {
      aDesiredSize.maxElementSize->height = aDesiredSize.height;
    }
  }
}

/******************************************************************************
 * FrameLoadingInfo
 *****************************************************************************/
FrameLoadingInfo::FrameLoadingInfo(const nsSize& aSize)
{
  NS_INIT_ISUPPORTS();

  mFrameSize = aSize;
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ISUPPORTS0(FrameLoadingInfo)

