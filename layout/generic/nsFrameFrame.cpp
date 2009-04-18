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
 *   Travis Bogard <travis@netscape.com>
 *   HÂkan Waara <hwaara@chello.se>
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

/*
 * rendering object for replaced elements that contain a document, such
 * as <frame>, <iframe>, and some <object>s
 */

#include "nsCOMPtr.h"
#include "nsLeafFrame.h"
#include "nsGenericHTMLElement.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIComponentManager.h"
#include "nsFrameManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsGkAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsFrameSetFrame.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMXULElement.h"
#include "nsIFrameLoader.h"
#include "nsIScriptSecurityManager.h"
#include "nsXPIDLString.h"
#include "nsIScrollable.h"
#include "nsINameSpaceManager.h"
#include "nsWeakReference.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIRenderingContext.h"
#include "nsIFrameFrame.h"
#include "nsAutoPtr.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsDisplayList.h"
#include "nsUnicharUtils.h"
#include "nsIReflowCallback.h"
#include "nsIScrollableFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsLayoutUtils.h"

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif

// For Accessibility
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kCChildCID, NS_CHILD_CID);

/******************************************************************************
 * nsSubDocumentFrame
 *****************************************************************************/
class nsSubDocumentFrame : public nsLeafFrame,
                           public nsIFrameFrame,
                           public nsIReflowCallback
{
public:
  nsSubDocumentFrame(nsStyleContext* aContext);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  NS_DECL_QUERYFRAME

  virtual nsIAtom* GetType() const;

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    // nsLeafFrame is already eReplacedContainsBlock, but that's somewhat bogus
    return nsLeafFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual void Destroy();

  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);

  virtual IntrinsicSize GetIntrinsicSize();
  virtual nsSize  GetIntrinsicRatio();

  virtual nsSize ComputeAutoSize(nsIRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, PRBool aShrinkWrap);

  virtual nsSize ComputeSize(nsIRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRBool aShrinkWrap);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  // if the content is "visibility:hidden", then just hide the view
  // and all our contents. We don't extend "visibility:hidden" to
  // the child content ourselves, since it belongs to a different
  // document and CSS doesn't inherit in there.
  virtual PRBool SupportsVisibilityHidden() { return PR_FALSE; }

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

  // nsIFrameFrame
  NS_IMETHOD GetDocShell(nsIDocShell **aDocShell);
  NS_IMETHOD BeginSwapDocShells(nsIFrame* aOther);
  virtual void EndSwapDocShells(nsIFrame* aOther);

  NS_IMETHOD  VerifyTree() const;

  // nsIReflowCallback
  virtual PRBool ReflowFinished();
  virtual void ReflowCallbackCanceled();

protected:
  nsIntSize GetMargin();
  PRBool IsInline() { return mIsInline; }
  nsresult ShowDocShell();
  nsresult CreateViewAndWidget(nsContentType aContentType);

  virtual nscoord GetIntrinsicWidth();
  virtual nscoord GetIntrinsicHeight();

  virtual PRIntn GetSkipSides() const;

  // Hide or show our document viewer
  void HideViewer();
  void ShowViewer();

  /* Obtains the frame we should use for intrinsic size information if we are
   * an HTML <object>, <embed> or <applet> (a replaced element - not <iframe>)
   * and our sub-document has an intrinsic size. The frame returned is the
   * frame for the document element of the document we're embedding.
   *
   * Called "Obtain*" and not "Get*" because of comment on GetDocShell that
   * says it should be called ObtainDocShell because of it's side effects.
   */
  nsIFrame* ObtainIntrinsicSizeFrame();

  nsCOMPtr<nsIFrameLoader> mFrameLoader;
  nsIView* mInnerView;
  PRPackedBool mDidCreateDoc;
  PRPackedBool mIsInline;
  PRPackedBool mPostedReflowCallback;
};

nsSubDocumentFrame::nsSubDocumentFrame(nsStyleContext* aContext)
  : nsLeafFrame(aContext), mDidCreateDoc(PR_FALSE),
    mIsInline(PR_FALSE), mPostedReflowCallback(PR_FALSE)
{
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsSubDocumentFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mContent);
    return accService->CreateOuterDocAccessible(node, aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

NS_QUERYFRAME_HEAD(nsSubDocumentFrame)
  NS_QUERYFRAME_ENTRY(nsIFrameFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsLeafFrame)

NS_IMETHODIMP
nsSubDocumentFrame::Init(nsIContent*     aContent,
                         nsIFrame*       aParent,
                         nsIFrame*       aPrevInFlow)
{
  // determine if we are a <frame> or <iframe>
  if (aContent) {
    nsCOMPtr<nsIDOMHTMLFrameElement> frameElem = do_QueryInterface(aContent);
    mIsInline = frameElem ? PR_FALSE : PR_TRUE;
  }

  nsresult rv =  nsLeafFrame::Init(aContent, aParent, aPrevInFlow);
  if (NS_FAILED(rv))
    return rv;

  // We are going to create an inner view.  If we need a view for the
  // OuterFrame but we wait for the normal view creation path in
  // nsCSSFrameConstructor, then we will lose because the inner view's
  // parent will already have been set to some outer view (e.g., the
  // canvas) when it really needs to have this frame's view as its
  // parent. So, create this frame's view right away, whether we
  // really need it or not, and the inner view will get it as the
  // parent.
  if (!HasView()) {
    rv = nsHTMLContainerFrame::CreateViewForFrame(this, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  nsIView* view = GetView();

  if (aParent->GetStyleDisplay()->mDisplay == NS_STYLE_DISPLAY_DECK
      && !view->HasWidget()) {
    view->CreateWidget(kCChildCID);
  }

  ShowViewer();
  return NS_OK;
}

void
nsSubDocumentFrame::ShowViewer()
{
  if (!PresContext()->IsDynamic()) {
    // We let the printing code take care of loading the document; just
    // create a widget for it to use
    nsresult rv = CreateViewAndWidget(eContentTypeContent);
    if (NS_FAILED(rv)) {
      return;
    }
  } else {
    nsresult rv = ShowDocShell();
    if (NS_FAILED(rv)) {
      return;
    }
    mDidCreateDoc = PR_TRUE;
  }
}

PRIntn
nsSubDocumentFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsSubDocumentFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!mInnerView)
    return NS_OK;
  nsIView* subdocView = mInnerView->GetFirstChild();
  if (!subdocView)
    return NS_OK;

  // Get the PresShell so we can check if painting is suppressed
  // on the subdocument. We use this roundabout way in case we
  // don't have a frame tree.
  if (!mFrameLoader)
    return NS_OK;
  nsCOMPtr<nsIDocShell> docShell;
  mFrameLoader->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return NS_OK;
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return NS_OK;

  PRBool suppressed = PR_TRUE;
  presShell->IsPaintingSuppressed(&suppressed);

  nsIFrame* f = static_cast<nsIFrame*>(subdocView->GetClientData());

  if (!f || suppressed) {
    // If we don't have a frame or painting of the PresShell is suppressed,
    // try to draw the default background color. (Bug 485275)

    // Get the bounds of subdocView relative to the reference frame.
    nsRect shellBounds = subdocView->GetBounds() +
                         mInnerView->GetPosition() +
                         GetOffsetTo(aBuilder->ReferenceFrame());
    rv = aLists.Content()->AppendNewToBottom(
             new (aBuilder) nsDisplaySolidColor(
                  shellBounds,
                  presShell->GetCanvasBackground()));
    return rv;
  }

  nsRect dirty = aDirtyRect - f->GetOffsetTo(this);

  aBuilder->EnterPresShell(f, dirty);

  // Clip children to the child root frame's rectangle
  nsDisplayList childItems;
  rv = f->BuildDisplayListForStackingContext(aBuilder, dirty, &childItems);
  if (NS_SUCCEEDED(rv)) {
    rv = aLists.Content()->AppendNewToTop(
        new (aBuilder) nsDisplayClip(nsnull, this, &childItems,
              nsRect(aBuilder->ToReferenceFrame(f), f->GetSize())));
    // delete childItems in case of OOM
    childItems.DeleteAll();
  }

  aBuilder->LeavePresShell(f, dirty);
  return rv;
}

nscoord
nsSubDocumentFrame::GetIntrinsicWidth()
{
  if (!IsInline()) {
    return 0;  // HTML <frame> has no useful intrinsic width
  }

  if (mContent->IsNodeOfType(nsINode::eXUL)) {
    return 0;  // XUL <iframe> and <browser> have no useful intrinsic width
  }

  NS_ASSERTION(ObtainIntrinsicSizeFrame() == nsnull,
               "Intrinsic width should come from the embedded document.");

  // We must be an HTML <iframe>.  Default to a width of 300, for IE
  // compat (and per CSS2.1 draft).
  return nsPresContext::CSSPixelsToAppUnits(300);
}

nscoord
nsSubDocumentFrame::GetIntrinsicHeight()
{
  // <frame> processing does not use this routine, only <iframe>
  NS_ASSERTION(IsInline(), "Shouldn't have been called");

  if (mContent->IsNodeOfType(nsINode::eXUL)) {
    return 0;
  }

  NS_ASSERTION(ObtainIntrinsicSizeFrame() == nsnull,
               "Intrinsic height should come from the embedded document.");

  // Use 150px, for compatibility with IE, and per CSS2.1 draft.
  return nsPresContext::CSSPixelsToAppUnits(150);
}

#ifdef DEBUG
NS_IMETHODIMP nsSubDocumentFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FrameOuter"), aResult);
}
#endif

nsIAtom*
nsSubDocumentFrame::GetType() const
{
  return nsGkAtoms::subDocumentFrame;
}

/* virtual */ nscoord
nsSubDocumentFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    result = subDocRoot->GetMinWidth(aRenderingContext);
  } else {
    result = GetIntrinsicWidth();
  }

  return result;
}

/* virtual */ nscoord
nsSubDocumentFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    result = subDocRoot->GetPrefWidth(aRenderingContext);
  } else {
    result = GetIntrinsicWidth();
  }

  return result;
}

/* virtual */ nsIFrame::IntrinsicSize
nsSubDocumentFrame::GetIntrinsicSize()
{
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return subDocRoot->GetIntrinsicSize();
  }
  return nsLeafFrame::GetIntrinsicSize();
}

/* virtual */ nsSize
nsSubDocumentFrame::GetIntrinsicRatio()
{
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return subDocRoot->GetIntrinsicRatio();
  }
  return nsLeafFrame::GetIntrinsicRatio();
}

/* virtual */ nsSize
nsSubDocumentFrame::ComputeAutoSize(nsIRenderingContext *aRenderingContext,
                                    nsSize aCBSize, nscoord aAvailableWidth,
                                    nsSize aMargin, nsSize aBorder,
                                    nsSize aPadding, PRBool aShrinkWrap)
{
  if (!IsInline()) {
    return nsFrame::ComputeAutoSize(aRenderingContext, aCBSize,
                                    aAvailableWidth, aMargin, aBorder,
                                    aPadding, aShrinkWrap);
  }

  return nsLeafFrame::ComputeAutoSize(aRenderingContext, aCBSize,
                                      aAvailableWidth, aMargin, aBorder,
                                      aPadding, aShrinkWrap);  
}


/* virtual */ nsSize
nsSubDocumentFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
                                nsSize aCBSize, nscoord aAvailableWidth,
                                nsSize aMargin, nsSize aBorder, nsSize aPadding,
                                PRBool aShrinkWrap)
{
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                            aRenderingContext, this,
                            subDocRoot->GetIntrinsicSize(),
                            subDocRoot->GetIntrinsicRatio(),
                            aCBSize, aMargin, aBorder, aPadding);
  }
  return nsLeafFrame::ComputeSize(aRenderingContext, aCBSize, aAvailableWidth,
                                  aMargin, aBorder, aPadding, aShrinkWrap);
}

NS_IMETHODIMP
nsSubDocumentFrame::Reflow(nsPresContext*           aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsSubDocumentFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  // printf("OuterFrame::Reflow %X (%d,%d) \n", this, aReflowState.availableWidth, aReflowState.availableHeight);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsSubDocumentFrame::Reflow: maxSize=%d,%d",
      aReflowState.availableWidth, aReflowState.availableHeight));

  aStatus = NS_FRAME_COMPLETE;

  NS_ASSERTION(aPresContext->GetPresShell()->GetPrimaryFrameFor(mContent) == this,
               "Shouldn't happen");

  // "offset" is the offset of our content area from our frame's
  // top-left corner.
  nsPoint offset(0, 0);
  
  if (IsInline()) {
    // XUL <iframe> or <browser>, or HTML <iframe>, <object> or <embed>
    nsresult rv = nsLeafFrame::DoReflow(aPresContext, aDesiredSize, aReflowState,
                                        aStatus);
    NS_ENSURE_SUCCESS(rv, rv);

    offset = nsPoint(aReflowState.mComputedBorderPadding.left,
                     aReflowState.mComputedBorderPadding.top);
  } else {
    // HTML <frame>
    SizeToAvailSize(aReflowState, aDesiredSize);
  }

  nsSize innerSize(aDesiredSize.width, aDesiredSize.height);
  if (IsInline()) {
    innerSize.width  -= aReflowState.mComputedBorderPadding.LeftRight();
    innerSize.height -= aReflowState.mComputedBorderPadding.TopBottom();
  }

  if (mInnerView) {
    nsIViewManager* vm = mInnerView->GetViewManager();
    vm->MoveViewTo(mInnerView, offset.x, offset.y);
    vm->ResizeView(mInnerView, nsRect(nsPoint(0, 0), innerSize), PR_TRUE);
  }

  // Determine if we need to repaint our border, background or outline
  CheckInvalidateSizeChange(aDesiredSize);

  FinishAndStoreOverflow(&aDesiredSize);

  // Invalidate the frame contents
  // XXX is this really needed?
  nsRect rect(nsPoint(0, 0), GetSize());
  Invalidate(rect);

  if (!aPresContext->IsPaginated() && !mPostedReflowCallback) {
    PresContext()->PresShell()->PostReflowCallback(this);
    mPostedReflowCallback = PR_TRUE;
  }

  // printf("OuterFrame::Reflow DONE %X (%d,%d)\n", this,
  //        aDesiredSize.width, aDesiredSize.height);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsSubDocumentFrame::Reflow: size=%d,%d status=%x",
      aDesiredSize.width, aDesiredSize.height, aStatus));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

PRBool
nsSubDocumentFrame::ReflowFinished()
{
  nsCOMPtr<nsIDocShell> docShell;
  GetDocShell(getter_AddRefs(docShell));

  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

  // resize the sub document
  if (baseWindow) {
    PRInt32 x = 0;
    PRInt32 y = 0;

    nsWeakFrame weakFrame(this);
    
    nsPresContext* presContext = PresContext();
    baseWindow->GetPositionAndSize(&x, &y, nsnull, nsnull);

    if (!weakFrame.IsAlive()) {
      // GetPositionAndSize() killed us
      return PR_FALSE;
    }

    // GetPositionAndSize might have resized us.  So now is the time to
    // get our size.
    mPostedReflowCallback = PR_FALSE;
  
    nsSize innerSize(GetSize());
    if (IsInline()) {
      nsMargin usedBorderPadding = GetUsedBorderAndPadding();

      // Sadly, XUL smacks the frame size without changing the used
      // border and padding, so we can't trust those.  Subtracting
      // them might make things negative.
      innerSize.width  -= usedBorderPadding.LeftRight();
      innerSize.width = PR_MAX(innerSize.width, 0);
      
      innerSize.height -= usedBorderPadding.TopBottom();
      innerSize.height = PR_MAX(innerSize.height, 0);
    }  

    PRInt32 cx = presContext->AppUnitsToDevPixels(innerSize.width);
    PRInt32 cy = presContext->AppUnitsToDevPixels(innerSize.height);
    baseWindow->SetPositionAndSize(x, y, cx, cy, PR_FALSE);
  } else {
    // Make sure that we can post a reflow callback in the future.
    mPostedReflowCallback = PR_FALSE;
  }

  return PR_FALSE;
}

void
nsSubDocumentFrame::ReflowCallbackCanceled()
{
  mPostedReflowCallback = PR_FALSE;
}

NS_IMETHODIMP
nsSubDocumentFrame::VerifyTree() const
{
  // XXX Completely disabled for now; once pseud-frames are reworked
  // then we can turn it back on.
  return NS_OK;
}

NS_IMETHODIMP
nsSubDocumentFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     PRInt32 aModType)
{
  if (aNameSpaceID != kNameSpaceID_None) {
    return NS_OK;
  }
  
  // If the noResize attribute changes, dis/allow frame to be resized
  if (aAttribute == nsGkAtoms::noresize) {
    // Note that we're not doing content type checks, but that's ok -- if
    // they'd fail we will just end up with a null framesetFrame.
    if (mContent->GetParent()->Tag() == nsGkAtoms::frameset) {
      nsIFrame* parentFrame = GetParent();

      if (parentFrame) {
        // There is no interface for nsHTMLFramesetFrame so QI'ing to
        // concrete class, yay!
        nsHTMLFramesetFrame* framesetFrame = do_QueryFrame(parentFrame);
        if (framesetFrame) {
          framesetFrame->RecalculateBorderResize();
        }
      }
    }
  }
  else if (aAttribute == nsGkAtoms::type) {
    if (!mFrameLoader) 
      return NS_OK;

    if (!mContent->IsNodeOfType(nsINode::eXUL)) {
      return NS_OK;
    }

    // Note: This logic duplicates a lot of logic in
    // nsFrameLoader::EnsureDocShell.  We should fix that.

    // Notify our enclosing chrome that our type has changed.  We only do this
    // if our parent is chrome, since in all other cases we're random content
    // subframes and the treeowner shouldn't worry about us.

    nsCOMPtr<nsIDocShell> docShell;
    mFrameLoader->GetDocShell(getter_AddRefs(docShell));
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
    if (!docShellAsItem) {
      return NS_OK;
    }

    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    docShellAsItem->GetParent(getter_AddRefs(parentItem));

    PRInt32 parentType;
    parentItem->GetItemType(&parentType);

    if (parentType != nsIDocShellTreeItem::typeChrome) {
      return NS_OK;
    }

    nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
    parentItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
    if (parentTreeOwner) {
      nsAutoString value;
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, value);

      PRBool is_primary = value.LowerCaseEqualsLiteral("content-primary");

#ifdef MOZ_XUL
      // when a content panel is no longer primary, hide any open popups it may have
      if (!is_primary) {
        nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
        if (pm)
          pm->HidePopupsInDocShell(docShellAsItem);
      }
#endif

      parentTreeOwner->ContentShellRemoved(docShellAsItem);

      if (value.LowerCaseEqualsLiteral("content") ||
          StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                           nsCaseInsensitiveStringComparator())) {
        PRBool is_targetable = is_primary ||
          value.LowerCaseEqualsLiteral("content-targetable");

        parentTreeOwner->ContentShellAdded(docShellAsItem, is_primary,
                                           is_targetable, value);
      }
    }
  }

  return NS_OK;
}

nsIFrame*
NS_NewSubDocumentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSubDocumentFrame(aContext);
}

void
nsSubDocumentFrame::Destroy()
{
  if (mPostedReflowCallback) {
    PresContext()->PresShell()->CancelReflowCallback(this);
    mPostedReflowCallback = PR_FALSE;
  }
  
  HideViewer();

  nsLeafFrame::Destroy();
}

void
nsSubDocumentFrame::HideViewer()
{
  if (mFrameLoader && mDidCreateDoc) {
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
      }

      nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(docShell);
      NS_ASSERTION(baseWin, "Docshell must be an nsIBaseWindow");

      // Now reverse the steps we took in ShowDocShell().  But don't call
      // Destroy(); that will be handled by destroying our frame loader, if
      // needed.

      // Hide the content viewer now that the frame is going away...
      baseWin->SetVisibility(PR_FALSE);

      // Clear out the parentWidget, since it might be about to die with us
      baseWin->SetParentWidget(nsnull);
    }
  }
}

nsIntSize nsSubDocumentFrame::GetMargin()
{
  nsIntSize result(-1, -1);
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);
  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::marginwidth);
    if (attr && attr->Type() == nsAttrValue::eInteger)
      result.width = attr->GetIntegerValue();
    attr = content->GetParsedAttr(nsGkAtoms::marginheight);
    if (attr && attr->Type() == nsAttrValue::eInteger)
      result.height = attr->GetIntegerValue();
  }
  return result;
}

// XXX this should be called ObtainDocShell or something like that,
// to indicate that it could have side effects
NS_IMETHODIMP
nsSubDocumentFrame::GetDocShell(nsIDocShell **aDocShell)
{
  *aDocShell = nsnull;

  nsIContent* content = GetContent();
  if (!content) {
    // Hmm, no content in this frame
    // that's odd, not much to be done here then.
    return NS_OK;
  }

  if (!mFrameLoader) {
    nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(content);

    if (loaderOwner) {
      loaderOwner->GetFrameLoader(getter_AddRefs(mFrameLoader));
    }

    NS_ENSURE_STATE(mFrameLoader);
  }

  return mFrameLoader->GetDocShell(aDocShell);
}

NS_IMETHODIMP
nsSubDocumentFrame::BeginSwapDocShells(nsIFrame* aOther)
{
  if (!aOther || aOther->GetType() != nsGkAtoms::subDocumentFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* other = static_cast<nsSubDocumentFrame*>(aOther);
  if (!mFrameLoader || !mDidCreateDoc || !other->mFrameLoader ||
      !other->mDidCreateDoc) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  HideViewer();
  other->HideViewer();

  mFrameLoader.swap(other->mFrameLoader);
  return NS_OK;
}

void
nsSubDocumentFrame::EndSwapDocShells(nsIFrame* aOther)
{
  nsSubDocumentFrame* other = static_cast<nsSubDocumentFrame*>(aOther);
  ShowViewer();
  other->ShowViewer();

  // Now make sure we reflow both frames, in case their contents
  // determine their size.
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);
  other->PresContext()->PresShell()->
    FrameNeedsReflow(other, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);

  // And repaint them, for good measure, in case there's nothing
  // interesting that happens during reflow.
  InvalidateOverflowRect();
  other->InvalidateOverflowRect();
}

inline PRInt32 ConvertOverflow(PRUint8 aOverflow)
{
  switch (aOverflow) {
    case NS_STYLE_OVERFLOW_VISIBLE:
    case NS_STYLE_OVERFLOW_AUTO:
      return nsIScrollable::Scrollbar_Auto;
    case NS_STYLE_OVERFLOW_HIDDEN:
    case NS_STYLE_OVERFLOW_CLIP:
      return nsIScrollable::Scrollbar_Never;
    case NS_STYLE_OVERFLOW_SCROLL:
      return nsIScrollable::Scrollbar_Always;
  }
  NS_NOTREACHED("invalid overflow value passed to ConvertOverflow");
  return nsIScrollable::Scrollbar_Auto;
}

nsresult
nsSubDocumentFrame::ShowDocShell()
{
  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));

  if (presShell) {
    // The docshell is already showing, nothing left to do...
    NS_ASSERTION(mInnerView, "What's going on?");
    return NS_OK;
  }

  // pass along marginwidth, marginheight, scrolling so sub document
  // can use it
  nsIntSize margin = GetMargin();
  docShell->SetMarginWidth(margin.width);
  docShell->SetMarginHeight(margin.height);

  // Current and initial scrolling is set so that all succeeding docs
  // will use the scrolling value set here, regardless if scrolling is
  // set by viewing a particular document (e.g. XUL turns off scrolling)
  nsCOMPtr<nsIScrollable> sc(do_QueryInterface(docShell));

  if (sc) {
    const nsStyleDisplay *disp = GetStyleDisplay();
    sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                       ConvertOverflow(disp->mOverflowX));
    sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                       ConvertOverflow(disp->mOverflowY));
  }

  PRInt32 itemType = nsIDocShellTreeItem::typeContent;
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(docShell));
  if (treeItem) {
    treeItem->GetItemType(&itemType);
  }

  nsContentType contentType;
  if (itemType == nsIDocShellTreeItem::typeChrome) {
    contentType = eContentTypeUI;
  }
  else {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeParent;
    treeItem->GetSameTypeParent(getter_AddRefs(sameTypeParent));
    contentType = sameTypeParent ? eContentTypeContentFrame : eContentTypeContent;
  }
  rv = CreateViewAndWidget(contentType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

  if (baseWindow) {
    baseWindow->InitWindow(nsnull, mInnerView->GetWidget(), 0, 0, 10, 10);

    // This is kinda whacky, this "Create()" call doesn't really
    // create anything, one starts to wonder why this was named
    // "Create"...

    baseWindow->Create();

    baseWindow->SetVisibility(PR_TRUE);
  }

  // Trigger editor re-initialization if midas is turned on in the
  // sub-document. This shouldn't be necessary, but given the way our
  // editor works, it is. See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=284245
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIDOMNSHTMLDocument> doc =
      do_QueryInterface(presShell->GetDocument());

    if (doc) {
      nsAutoString designMode;
      doc->GetDesignMode(designMode);

      if (designMode.EqualsLiteral("on")) {
        doc->SetDesignMode(NS_LITERAL_STRING("off"));
        doc->SetDesignMode(NS_LITERAL_STRING("on"));
      }
    }
  }

  return NS_OK;
}

nsresult
nsSubDocumentFrame::CreateViewAndWidget(nsContentType aContentType)
{
  if (mInnerView) {
    // Nothing to do here
    return NS_OK;
  }
  
  // create, init, set the parent of the view
  nsIView* outerView = GetView();
  NS_ASSERTION(outerView, "Must have an outer view already");
  nsRect viewBounds(0, 0, 0, 0); // size will be fixed during reflow

  nsIViewManager* viewMan = outerView->GetViewManager();
  // Create the inner view hidden if the outer view is already hidden
  // (it won't get hidden properly otherwise)
  nsIView* innerView = viewMan->CreateView(viewBounds, outerView,
                                           outerView->GetVisibility());
  if (!innerView) {
    NS_ERROR("Could not create inner view");
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInnerView = innerView;
  viewMan->InsertChild(outerView, innerView, nsnull, PR_TRUE);

  return innerView->CreateWidget(kCChildCID, nsnull, nsnull, PR_TRUE, PR_TRUE,
                                 aContentType);
}

nsIFrame*
nsSubDocumentFrame::ObtainIntrinsicSizeFrame()
{
  nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(GetContent());
  if (olc) {
    // We are an HTML <object>, <embed> or <applet> (a replaced element).

    // Try to get an nsIFrame for our sub-document's document element
    nsIFrame* subDocRoot = nsnull;

    nsCOMPtr<nsIDocShell> docShell;
    GetDocShell(getter_AddRefs(docShell));
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell;
      docShell->GetPresShell(getter_AddRefs(presShell));
      if (presShell) {
        nsIScrollableFrame* scrollable = presShell->GetRootScrollFrameAsScrollable();
        if (scrollable) {
          nsIFrame* scrolled = scrollable->GetScrolledFrame();
          if (scrolled) {
            subDocRoot = scrolled->GetFirstChild(nsnull);
          }
        }
      }
    }

#ifdef MOZ_SVG
    if (subDocRoot && subDocRoot->GetContent() &&
        subDocRoot->GetContent()->NodeInfo()->Equals(nsGkAtoms::svg, kNameSpaceID_SVG)) {
      return subDocRoot; // SVG documents have an intrinsic size
    }
#endif
  }
  return nsnull;
}
