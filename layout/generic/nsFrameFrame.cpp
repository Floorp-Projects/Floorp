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
 *   Håkan Waara <hwaara@chello.se>
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
#include "nsHTMLAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIDocumentLoader.h"
#include "nsFrameSetFrame.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMXULElement.h"
#include "nsIFrameLoader.h"
#include "nsLayoutAtoms.h"
#include "nsIScriptSecurityManager.h"
#include "nsXPIDLString.h"
#include "nsIScrollable.h"
#include "nsINameSpaceManager.h"
#include "nsIWidget.h"
#include "nsIWebBrowserPrint.h"
#include "nsWeakReference.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIRenderingContext.h"
#include "nsIFrameFrame.h"
#include "nsAutoPtr.h"
#include "nsIDOMNSHTMLDocument.h"

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
                           public nsIFrameFrame
{
public:
  nsSubDocumentFrame();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return 2; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return 1; }

  virtual nsIAtom* GetType() const;

  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Destroy(nsPresContext* aPresContext);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

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

  NS_IMETHOD  VerifyTree() const;

protected:
  nsSize GetMargin();
  PRBool IsInline() { return mIsInline; }
  nsresult ReloadURL();
  nsresult ShowDocShell();
  nsresult CreateViewAndWidget(nsContentType aContentType);

  virtual void GetDesiredSize(nsPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);
  virtual PRIntn GetSkipSides() const;

  nsCOMPtr<nsIFrameLoader> mFrameLoader;
  nsIView* mInnerView;
  PRPackedBool mDidCreateDoc;
  PRPackedBool mOwnsFrameLoader;
  PRPackedBool mIsInline;
};

nsSubDocumentFrame::nsSubDocumentFrame()
  : nsLeafFrame(), mDidCreateDoc(PR_FALSE), mOwnsFrameLoader(PR_FALSE),
    mIsInline(PR_FALSE)
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

//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsSubDocumentFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
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

  return nsLeafFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsSubDocumentFrame::Init(nsPresContext* aPresContext,
                         nsIContent*     aContent,
                         nsIFrame*       aParent,
                         nsStyleContext* aContext,
                         nsIFrame*       aPrevInFlow)
{
  // determine if we are a <frame> or <iframe>
  if (aContent) {
    nsCOMPtr<nsIDOMHTMLFrameElement> frameElem = do_QueryInterface(aContent);
    mIsInline = frameElem ? PR_FALSE : PR_TRUE;
  }

  nsresult rv =  nsLeafFrame::Init(aPresContext, aContent, aParent,
                                   aContext, aPrevInFlow);
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
    // To properly initialize the view we need to know the frame for the content
    // that is the parent of content for this frame. This might not be our actual
    // frame parent if we are out of flow (e.g., positioned) so our parent frame
    // may have been set to some other ancestor.
    // We look for a content parent frame in the frame property list, where it
    // will have been set by nsCSSFrameConstructor if necessary.
    nsCOMPtr<nsIAtom> contentParentAtom = do_GetAtom("contentParent");
    nsIFrame* contentParent = nsnull;

    void *value =
      aPresContext->PropertyTable()->UnsetProperty(this,
                                                   contentParentAtom, &rv);
    if (NS_SUCCEEDED(rv)) {
          contentParent = (nsIFrame*)value;
    }

    nsHTMLContainerFrame::CreateViewForFrame(this, contentParent, PR_TRUE);
  }
  nsIView* view = GetView();
  NS_ASSERTION(view, "We should always have a view now");

  if (aParent->GetStyleDisplay()->mDisplay == NS_STYLE_DISPLAY_DECK
      && !view->HasWidget()) {
    view->CreateWidget(kCChildCID);
  }

  // determine if we are a printcontext
  PRBool shouldCreateDoc;

  if (aPresContext->Medium() == nsLayoutAtoms::print) {
    if (aPresContext->Type() == nsPresContext::eContext_PrintPreview) {
      // for print preview we want to create the view and widget but
      // we do not want to load the document, it is already loaded.
      rv = CreateViewAndWidget(eContentTypeContent);
      NS_ENSURE_SUCCESS(rv,rv);
    }

    shouldCreateDoc = PR_FALSE;
  } else {
    shouldCreateDoc = PR_TRUE;
  }

  if (shouldCreateDoc) {
    rv = ShowDocShell();
    NS_ENSURE_SUCCESS(rv,rv);
    mDidCreateDoc = PR_TRUE;
  }

  return NS_OK;
}

PRIntn
nsSubDocumentFrame::GetSkipSides() const
{
  return 0;
}

void
nsSubDocumentFrame::GetDesiredSize(nsPresContext* aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsHTMLReflowMetrics& aDesiredSize)
{
  // <frame> processing does not use this routine, only <iframe>
  float p2t = 0;
  if (!mContent->IsContentOfType(nsIContent::eXUL))
    // If no width/height was specified, use 300/150.
    // This is for compatability with IE.
    p2t = aPresContext->ScaledPixelsToTwips();

  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
    aDesiredSize.width = aReflowState.mComputedWidth;
  }
  else {
    aDesiredSize.width = PR_MAX(PR_MIN(NSIntPixelsToTwips(300, p2t),
                                       aReflowState.mComputedMaxWidth), 
                                aReflowState.mComputedMinWidth);
  }
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
    aDesiredSize.height = aReflowState.mComputedHeight;
  }
  else {
    aDesiredSize.height = PR_MAX(PR_MIN(NSIntPixelsToTwips(150, p2t),
                                        aReflowState.mComputedMaxHeight),
                                 aReflowState.mComputedMinHeight);
  }
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
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
  return nsLayoutAtoms::subDocumentFrame;
}

NS_IMETHODIMP
nsSubDocumentFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsSubDocumentFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  // printf("OuterFrame::Reflow %X (%d,%d) \n", this, aReflowState.availableWidth, aReflowState.availableHeight);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsSubDocumentFrame::Reflow: maxSize=%d,%d reason=%d",
      aReflowState.availableWidth, aReflowState.availableHeight, aReflowState.reason));

  aStatus = NS_FRAME_COMPLETE;

  if (IsInline()) {
    GetDesiredSize(aPresContext, aReflowState, aDesiredSize); // IFRAME
  } else {
    aDesiredSize.width  = aReflowState.availableWidth; // FRAME
    aDesiredSize.height = aReflowState.availableHeight;
  }

  nsSize innerSize(aDesiredSize.width, aDesiredSize.height);
  nsPoint offset(0, 0);
  nsMargin border = aReflowState.mComputedBorderPadding;

  if (IsInline()) {
    offset = nsPoint(border.left, border.top);
    aDesiredSize.width += border.left + border.right;
    aDesiredSize.height += border.top + border.bottom;
  }
  
  // might not have an inner view yet during printing
  if (mInnerView) {
    nsIViewManager* vm = mInnerView->GetViewManager();
    vm->MoveViewTo(mInnerView, offset.x, offset.y);
    vm->ResizeView(mInnerView, nsRect(0, 0, innerSize.width, innerSize.height), PR_TRUE);
  }

  if (aDesiredSize.mComputeMEW) {   
    nscoord defaultAutoWidth = NSIntPixelsToTwips(300, aPresContext->ScaledPixelsToTwips());
    if (mContent->IsContentOfType(nsIContent::eXUL)) {
        // XUL frames don't have a default 300px width
        defaultAutoWidth = 0;
    }
    nsStyleUnit widthUnit = GetStylePosition()->mWidth.GetUnit();
    switch (widthUnit) {
    case eStyleUnit_Percent:
      // if our width is percentage, then we can shrink until
      // there's nothing left but our borders
      aDesiredSize.mMaxElementWidth = border.left + border.right;
      break;
    case eStyleUnit_Auto:
      aDesiredSize.mMaxElementWidth = PR_MAX(PR_MIN(defaultAutoWidth,
                                                    aReflowState.mComputedMaxWidth),
                                             aReflowState.mComputedMinWidth) +
                                      border.left + border.right;
      break;
    default:
      // If our width is set by style to some fixed length,
      // then our actual width is our minimum width
      aDesiredSize.mMaxElementWidth = aDesiredSize.width;
      break;
    }
  }

  // Determine if we need to repaint our border, background or outline
  CheckInvalidateSizeChange(aPresContext, aDesiredSize, aReflowState);

  // Invalidate the frame contents
  nsRect rect(nsPoint(0, 0), GetSize());
  Invalidate(rect, PR_FALSE);

  if (!aPresContext->IsPaginated()) {
    nsCOMPtr<nsIDocShell> docShell;
    GetDocShell(getter_AddRefs(docShell));

    nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

    // resize the sub document
    if (baseWindow) {
      float t2p;
      t2p = aPresContext->TwipsToPixels();
      PRInt32 x = 0;
      PRInt32 y = 0;
      
      baseWindow->GetPositionAndSize(&x, &y, nsnull, nsnull);
      PRInt32 cx = NSToCoordRound(innerSize.width * t2p);
      PRInt32 cy = NSToCoordRound(innerSize.height * t2p);
      baseWindow->SetPositionAndSize(x, y, cx, cy, PR_FALSE);
    }
  }

  // printf("OuterFrame::Reflow DONE %X (%d,%d), MEW=%d(%d)\n", this,
  //        aDesiredSize.width, aDesiredSize.height, aDesiredSize.mMaxElementWidth,
  //        aDesiredSize.mComputeMEW);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsSubDocumentFrame::Reflow: size=%d,%d status=%x",
      aDesiredSize.width, aDesiredSize.height, aStatus));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
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
  nsIAtom *type = mContent->Tag();

  if ((type != nsHTMLAtoms::object && aAttribute == nsHTMLAtoms::src) ||
      (type == nsHTMLAtoms::object && aAttribute == nsHTMLAtoms::data)) {
    ReloadURL();
  }
  // If the noResize attribute changes, dis/allow frame to be resized
  else if (aAttribute == nsHTMLAtoms::noresize) {
    if (mContent->GetParent()->Tag() == nsHTMLAtoms::frameset) {
      nsIFrame* parentFrame = GetParent();

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
    if (!mFrameLoader) 
      return NS_OK;

    nsAutoString value;
    mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, value);

    // Notify our enclosing chrome that the primary content shell
    // has changed.

    nsCOMPtr<nsIDocShell> docShell;
    mFrameLoader->GetDocShell(getter_AddRefs(docShell));

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));

    // If our container is a web-shell, inform it that it has a new
    // child. If it's not a web-shell then some things will not operate
    // properly.
    nsCOMPtr<nsISupports> container = GetPresContext()->GetContainer();
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
          value.LowerCaseEqualsLiteral("content-primary");

        parentTreeOwner->ContentShellAdded(docShellAsItem, is_primary_content,
                                           value.get());
      }
    }
  }

  return NS_OK;
}

nsresult
NS_NewSubDocumentFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSubDocumentFrame* it = new (aPresShell) nsSubDocumentFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
nsSubDocumentFrame::Destroy(nsPresContext* aPresContext)
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

        // Hide the content viewer now that the frame is going away...

        content_viewer->Hide();
      }
    }
  }

  if (mFrameLoader && mOwnsFrameLoader) {
    // We own this frame loader, and we're going away, so destroy our
    // frame loader.

    mFrameLoader->Destroy();
  }

  return nsLeafFrame::Destroy(aPresContext);
}

nsSize nsSubDocumentFrame::GetMargin()
{
  nsSize result(-1, -1);
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);
  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsHTMLAtoms::marginwidth);
    if (attr && attr->Type() == nsAttrValue::eInteger)
      result.width = attr->GetIntegerValue();
    attr = content->GetParsedAttr(nsHTMLAtoms::marginheight);
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

    if (!mFrameLoader) {
      // No frame loader available from the content, create our own...
      mFrameLoader = new nsFrameLoader(content);
      if (!mFrameLoader)
        return NS_ERROR_OUT_OF_MEMORY;

      // ... remember that we own this frame loader...
      mOwnsFrameLoader = PR_TRUE;

      // ... and tell it to start loading.
      // the failure to load a URL does not constitute failure to 
      // create/initialize the docshell and therefore the LoadFrame() 
      // call's return value should not be propagated.
      mFrameLoader->LoadFrame();
    }
  }

  return mFrameLoader->GetDocShell(aDocShell);
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

    return NS_OK;
  }

  // pass along marginwidth, marginheight, scrolling so sub document
  // can use it
  nsSize margin = GetMargin();
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

// load a new url
nsresult
nsSubDocumentFrame::ReloadURL()
{
  if (!mOwnsFrameLoader || !mFrameLoader) {
    // If we don't own the frame loader, we're not in charge of what's
    // loaded into it.
    return NS_OK;
  }

  return mFrameLoader->LoadFrame();
}


