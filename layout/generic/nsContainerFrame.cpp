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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsContainerFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsStyleContext.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsVoidArray.h"
#include "nsHTMLReflowCommand.h"
#include "nsHTMLContainerFrame.h"
#include "nsFrameManager.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsLayoutAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
#include "nsCSSRendering.h"
#include "nsTransform2D.h"
#include "nsRegion.h"
#include "nsLayoutErrors.h"

#ifdef NS_DEBUG
#undef NOISY
#else
#undef NOISY
#endif

nsContainerFrame::nsContainerFrame()
{
}

nsContainerFrame::~nsContainerFrame()
{
}

NS_IMETHODIMP
nsContainerFrame::Init(nsPresContext*  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsStyleContext*  aContext,
                       nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSplittableFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  if (aPrevInFlow) {
    // Make sure we copy bits from our prev-in-flow that will affect
    // us. A continuation for a container frame needs to know if it
    // has a child with a view so that we'll properly reposition it.
    if (aPrevInFlow->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)
      AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  }
  return rv;
}

NS_IMETHODIMP
nsContainerFrame::SetInitialChildList(nsPresContext* aPresContext,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aChildList)
{
  nsresult  result;
  if (!mFrames.IsEmpty()) {
    // We already have child frames which means we've already been
    // initialized
    NS_NOTREACHED("unexpected second call to SetInitialChildList");
    result = NS_ERROR_UNEXPECTED;
  } else if (aListName) {
    // All we know about is the unnamed principal child list
    NS_NOTREACHED("unknown frame list");
    result = NS_ERROR_INVALID_ARG;
  } else {
#ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aChildList);
#endif
    mFrames.SetFrames(aChildList);
    result = NS_OK;
  }
  return result;
}

static void
CleanupGeneratedContentIn(nsIContent* aRealContent, nsIFrame* aRoot) {
  nsIAtom* frameList = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* child = aRoot->GetFirstChild(frameList);
    while (child) {
      nsIContent* content = child->GetContent();
      if (content && content != aRealContent) {
        content->SetParent(nsnull);
        content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
      }
      ::CleanupGeneratedContentIn(aRealContent, child);
      child = child->GetNextSibling();
    }
    frameList = aRoot->GetAdditionalChildListName(listIndex++);
  } while (frameList);
}

NS_IMETHODIMP
nsContainerFrame::Destroy(nsPresContext* aPresContext)
{
  // Prevent event dispatch during destruction
  if (HasView()) {
    GetView()->SetClientData(nsnull);
  }

  if (mState & NS_FRAME_GENERATED_CONTENT) {
    // Make sure all the content nodes for the generated content inside
    // this frame know it's going away.
    // XXXbz would this be better done via a global structure in
    // nsCSSFrameConstructor that could key off of
    // GeneratedContentFrameRemoved or something?  The problem is that
    // our kids are gone by the time that's called.
    ::CleanupGeneratedContentIn(mContent, this);
  }
  
  // Delete the primary child list
  mFrames.DestroyFrames(aPresContext);
  
  // Destroy overflow frames now
  nsFrameList overflowFrames(GetOverflowFrames(aPresContext, PR_TRUE));
  overflowFrames.DestroyFrames(aPresContext);

  // Destroy the frame and remove the flow pointers
  return nsSplittableFrame::Destroy(aPresContext);
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

nsIFrame*
nsContainerFrame::GetFirstChild(nsIAtom* aListName) const
{
  // We only know about the unnamed principal child list and the overflow
  // list
  if (nsnull == aListName) {
    return mFrames.FirstChild();
  } else if (nsLayoutAtoms::overflowList == aListName) {
    return GetOverflowFrames(GetPresContext(), PR_FALSE);
  } else {
    return nsnull;
  }
}

nsIAtom*
nsContainerFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  if (aIndex == 0) {
    return nsLayoutAtoms::overflowList;
  } else {
    return nsnull;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Painting/Events

NS_IMETHODIMP
nsContainerFrame::Paint(nsPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        nsFramePaintLayer    aWhichLayer,
                        PRUint32             aFlags)
{
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, aFlags);
  return NS_OK;
}

// Paint the children of a container, assuming nothing about the
// childrens spatial arrangement. Given relative positioning, negative
// margins, etc, that's probably a good thing.
//
// Note: aDirtyRect is in our coordinate system (and of course, child
// rect's are also in our coordinate system)
void
nsContainerFrame::PaintChildren(nsPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer,
                                PRUint32             aFlags)
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid, aWhichLayer, aFlags);
    kid = kid->GetNextSibling();
  }
}

// Paint one child frame
void
nsContainerFrame::PaintChild(nsPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsIFrame*            aFrame,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags)
{
  NS_ASSERTION(aFrame, "no frame to paint!");
  if (!aFrame->HasView()) {
    nsRect kidRect = aFrame->GetRect();

    // Compute the constrained damage area; set the overlap flag to
    // PR_TRUE if any portion of the child frame intersects the
    // dirty rect.
    nsRect damageArea;
    PRBool overlap;
    if (NS_FRAME_OUTSIDE_CHILDREN & aFrame->GetStateBits()) {
      // If the child frame has children that leak out of our box
      // then we don't constrain the damageArea to just the childs
      // bounding rect.
      damageArea = aDirtyRect;
      overlap = PR_TRUE;
    }
    else {
      // Compute the intersection of the dirty rect and the childs
      // rect (both are in our coordinate space). This limits the
      // damageArea to just the portion that intersects the childs
      // rect.
      overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
#ifdef NS_DEBUG
      if (!overlap && (0 == kidRect.width) && (0 == kidRect.height)) {
        overlap = PR_TRUE;
      }
#endif
    }

    if (overlap) {
      // Translate damage area into the kids coordinate
      // system. Translate rendering context into the kids
      // coordinate system.
      damageArea.x -= kidRect.x;
      damageArea.y -= kidRect.y;


      // The transform components are saved and restored instead 
      // of using PushState and PopState because they are too slow
      // because they also save and restore the clip state.
      // Note: Setting a negative translation to restore the 
      // state does not work because the floating point errors can accumulate
      // causing the display of some frames to be off by one pixel. 
      // This happens frequently when running in 120DPI mode where frames are
      // often positioned at 1/2 pixel locations and small floating point errors
      // will cause the frames to vary their pixel x location during scrolling
      // operations causes a single scan line of pixels to be shifted left relative
      // to the other scan lines for the same text. 

      // Save the transformation matrix's translation components.
      float xMatrix; 
      float yMatrix;
      nsTransform2D *theTransform; 
      aRenderingContext.GetCurrentTransform(theTransform);
      NS_ASSERTION(theTransform != nsnull, "The rendering context transform is null");
      theTransform->GetTranslation(&xMatrix, &yMatrix);

      aRenderingContext.Translate(kidRect.x, kidRect.y);

      // Paint the kid
      aFrame->Paint(aPresContext, aRenderingContext, damageArea, aWhichLayer, aFlags);

      // Restore the transformation matrix's translation components.
      theTransform->SetTranslation(xMatrix, yMatrix);

#ifdef NS_DEBUG
      // Draw a border around the child
      if (nsIFrameDebug::GetShowFrameBorders() && !kidRect.IsEmpty()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(kidRect);
      }
#endif
    }
  }
}

NS_IMETHODIMP
nsContainerFrame::GetFrameForPoint(nsPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND), aFrame);
}

nsresult
nsContainerFrame::GetFrameForPointUsing(nsPresContext* aPresContext,
                                        const nsPoint& aPoint,
                                        nsIAtom*       aList,
                                        nsFramePaintLayer aWhichLayer,
                                        PRBool         aConsiderSelf,
                                        nsIFrame**     aFrame)
{
  nsIFrame *hit;
  nsPoint tmp;

  PRBool inThisFrame = mRect.Contains(aPoint);

  if (! ((mState & NS_FRAME_OUTSIDE_CHILDREN) || inThisFrame ) ) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* kid = GetFirstChild(aList);
  *aFrame = nsnull;
  tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);

  nsPoint originOffset;
  nsIView *view = nsnull;
  nsresult rv = GetOriginToViewOffset(aPresContext, originOffset, &view);

  if (NS_SUCCEEDED(rv) && view)
    tmp += originOffset;

  while (kid) {
    rv = kid->GetFrameForPoint(aPresContext, tmp, aWhichLayer, &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      *aFrame = hit;
    }
    kid = kid->GetNextSibling();
  }

  if (*aFrame) {
    return NS_OK;
  }

  if ( inThisFrame && aConsiderSelf ) {
    if (GetStyleVisibility()->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsContainerFrame::ReplaceFrame(nsPresContext* aPresContext,
                               nsIPresShell&   aPresShell,
                               nsIAtom*        aListName,
                               nsIFrame*       aOldFrame,
                               nsIFrame*       aNewFrame)
{
  nsIFrame* prevFrame;
  nsresult  rv;

  // Get the old frame's previous sibling frame
  nsFrameList frames(GetFirstChild(aListName));
  NS_ASSERTION(frames.ContainsFrame(aOldFrame), "frame is not a valid child frame");
  prevFrame = frames.GetPrevSiblingFor(aOldFrame);

  // Default implementation treats it like two separate operations
  rv = RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  if (NS_SUCCEEDED(rv)) {
    rv = InsertFrames(aPresContext, aPresShell, aListName, prevFrame, aNewFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsContainerFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
  // The container frame always generates a reflow command
  // targeted at its child
  // Note that even if this flag is already set, we still need to reflow the
  // child because the frame may have more than one child
  mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

  nsFrame::CreateAndPostReflowCommand(aPresShell, aChild, 
    eReflowType_ReflowDirty, nsnull, nsnull, nsnull);

  return NS_OK;
}


/////////////////////////////////////////////////////////////////////////////
// Helper member functions

static PRBool
TranslatePointTo(nsPoint& aPoint, nsIView* aChildView, nsIView* aParentView)
{
  do {
    aPoint += aChildView->GetPosition();
    aChildView = aChildView->GetParent();
  } while (aChildView && (aChildView != aParentView));

  return aChildView == aParentView;
}

/**
 * Position the view associated with |aKidFrame|, if there is one. A
 * container frame should call this method after positioning a frame,
 * but before |Reflow|.
 */
void
nsContainerFrame::PositionFrameView(nsPresContext* aPresContext,
                                    nsIFrame*       aKidFrame)
{
  if (aKidFrame->HasView()) {
    nsIView* view = aKidFrame->GetView();
    // Position view relative to its parent, not relative to aKidFrame's
    // frame which may not have a view
    nsIView*        parentView = view->GetParent();

    nsIView*        containingView;
    nsPoint         origin;
    aKidFrame->GetOffsetFromView(aPresContext, origin, &containingView);

    nsIViewManager* vm = view->GetViewManager();

    // it's possible for the parentView to be nonnull but containingView to be
    // null, when the parent view doesn't belong to this frame tree but to
    // the frame tree of some enclosing document. We do nothing in that case,
    // but we have to check that containingView is nonnull or we will crash.
    if (nsnull != containingView && containingView != parentView) {
      NS_ERROR("This hack should not be needed now!!! See bug 126263.");

      // XXX roc this should be no longer needed, but it still is for printing/print preview

      // it is possible for parent view not to have a frame attached to it
      // kind of an anonymous view. This happens with native scrollbars and
      // the clip view. To fix this we need to go up and parentView chain
      // until we find a view with client data. This is total HACK to fix
      // the HACK below. COMBO box code should NOT be in the container code!!!
      // And the case it looks from does not just happen for combo boxes. Native
      // scrollframes get in this situation too!! 
      while (parentView) {
        void *data = parentView->GetClientData();
        if (data)
          break;
      
        origin -= parentView->GetPosition();
        parentView = parentView->GetParent();
      }
     
      if (containingView != parentView) 
      {
        // Huh, the view's parent view isn't the same as the containing view.
        // This happens for combo box drop-down frames.
        //
        // We have the origin in the coordinate space of the containing view,
        // but we need it in the coordinate space of the parent view so do a
        // view translation
#ifdef DEBUG
        PRBool ok = 
#endif
        TranslatePointTo(origin, containingView, parentView);

        NS_ASSERTION(ok, "translation failed");
      }
    }

    if (parentView) {
      // If the parent view is scrollable, then adjust the origin by
      // the parent's scroll position.
      nsIScrollableView* scrollable = nsnull;
      CallQueryInterface(parentView, &scrollable);
      if (scrollable) {
        nscoord scrollX = 0, scrollY = 0;
        scrollable->GetScrollPosition(scrollX, scrollY);

        origin.x -= scrollX;
        origin.y -= scrollY;
      }
    }

    vm->MoveViewTo(view, origin.x, origin.y);
  }
}

static PRBool
NonZeroStyleCoord(const nsStyleCoord& aCoord) {
  switch (aCoord.GetUnit()) {
  case eStyleUnit_Percent:
    return aCoord.GetPercentValue() > 0;
  case eStyleUnit_Coord:
    return aCoord.GetCoordValue() > 0;
  case eStyleUnit_Null:
    return PR_FALSE;
  default:
    return PR_TRUE;
  }
}

static PRBool
HasNonZeroBorderRadius(nsStyleContext* aStyleContext) {
  const nsStyleBorder* border = aStyleContext->GetStyleBorder();

  nsStyleCoord coord;
  border->mBorderRadius.GetTop(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;    
  border->mBorderRadius.GetRight(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;    
  border->mBorderRadius.GetBottom(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;    
  border->mBorderRadius.GetLeft(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;    

  return PR_FALSE;
}

static void
SyncFrameViewGeometryDependentProperties(nsPresContext*  aPresContext,
                                         nsIFrame*        aFrame,
                                         nsStyleContext*  aStyleContext,
                                         nsIView*         aView,
                                         PRUint32         aFlags)
{
  nsIViewManager* vm = aView->GetViewManager();

  PRBool isCanvas;
  const nsStyleBackground* bg;
  PRBool hasBG =
    nsCSSRendering::FindBackground(aPresContext, aFrame, &bg, &isCanvas);

  // background-attachment: fixed is not really geometry dependent, but
  // we set it here because it's cheap to do so
  PRBool fixedBackground = hasBG &&
    NS_STYLE_BG_ATTACHMENT_FIXED == bg->mBackgroundAttachment;
  // If the frame has a fixed background attachment, then indicate that the
  // view's contents should be repainted and not bitblt'd
  vm->SetViewBitBltEnabled(aView, !fixedBackground);

  // If the frame has a solid background color, 'background-clip:border',
  // and it's a kind of frame that paints its background, and rounded borders aren't
  // clipping the background, then it's opaque.
  PRBool  viewHasTransparentContent =
    !(hasBG && !(bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) &&
      bg->mBackgroundClip == NS_STYLE_BG_CLIP_BORDER &&
      aFrame->CanPaintBackground() &&
      !HasNonZeroBorderRadius(aStyleContext));

  PRBool drawnOnUniformField = PR_FALSE;
  if (aStyleContext->GetPseudoType() == nsCSSAnonBoxes::scrolledContent) {
    // If the nsGfxScrollFrame draws a solid unclipped background
    // color, and nothing else, then tell the view system that we're
    // drawn on a uniform field. Note that it's OK if the background
    // is clipped to the padding area, since the scrollport is within
    // the borders.
    nsIFrame* scrollFrame = aFrame->GetParent();
    while (scrollFrame->GetStyleContext()->GetPseudoType()
           == nsCSSAnonBoxes::scrolledContent) {
      scrollFrame = scrollFrame->GetParent();
    }
    PRBool scrollFrameIsCanvas;
    const nsStyleBackground* scrollFrameBG;
    PRBool scrollFrameHasBG =
      nsCSSRendering::FindBackground(aPresContext, scrollFrame, &scrollFrameBG,
                                     &scrollFrameIsCanvas);
    const nsStyleDisplay* bgDisplay = scrollFrame->GetStyleDisplay();
    drawnOnUniformField = scrollFrameHasBG &&
      !(scrollFrameBG->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) &&
      (scrollFrameBG->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE) &&
      !HasNonZeroBorderRadius(scrollFrame->GetStyleContext()) &&
      !(bgDisplay->IsAbsolutelyPositioned()
        && (bgDisplay->mClipFlags & NS_STYLE_CLIP_RECT));
  }
  aView->SetHasUniformBackground(drawnOnUniformField);

  if (isCanvas) {
    nsIView* rootView;
    vm->GetRootView(rootView);
    nsIView* rootParent = rootView->GetParent();
    if (!rootParent) {
      // We're the root of a view manager hierarchy. We will have to
      // paint something. NOTE: this can be overridden below.
      viewHasTransparentContent = PR_FALSE;
    }

    nsIDocument *doc = aPresContext->PresShell()->GetDocument();
    if (doc) {
      nsIContent *rootElem = doc->GetRootContent();
      if (!doc->GetParentDocument() &&
          rootElem && rootElem->IsContentOfType(nsIContent::eXUL)) {
        // we're XUL at the root of the document hierarchy. Try to make our
        // window translucent.
        // don't proceed unless this is the root view
        // (sometimes the non-root-view is a canvas)
        if (aView->HasWidget() && aView == rootView) {
          viewHasTransparentContent = hasBG && (bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);
          aView->GetWidget()->SetWindowTranslucency(viewHasTransparentContent);
        }
      }
    }
  }
  // XXX we should also set widget transparency for XUL popups

  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
  nsFrameState kidState = aFrame->GetStateBits();
  
  if (!viewHasTransparentContent) {
    // If we're showing the view but the frame is hidden, then the view is transparent
    const nsStyleVisibility* vis = aStyleContext->GetStyleVisibility();
    if ((nsViewVisibility_kShow == aView->GetVisibility()
         && NS_STYLE_VISIBILITY_HIDDEN == vis->mVisible)
        || (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow
            && (kidState & NS_FRAME_OUTSIDE_CHILDREN) != 0)) {
      viewHasTransparentContent = PR_TRUE;
    }
  }

  // If the frame has visible content that overflows the content area, then we
  // need the view marked as having transparent content

  // There are two types of clipping:
  // - 'clip' which only applies to absolutely positioned elements, and is
  //    relative to the element's border edge. 'clip' applies to the entire
  //    element
  // - 'overflow-clip' which only applies to block-level elements and replaced
  //   elements that have 'overflow' set to 'hidden'. 'overflow-clip' is relative
  //   to the content area and applies to content only (not border or background).
  //   Note that out-of-flow frames like floated or absolutely positioned frames
  //   are block-level, but we can't rely on the 'display' value being set correctly
  //   in the style context...
  PRBool isBlockLevel = display->IsBlockLevel() || (kidState & NS_FRAME_OUT_OF_FLOW);
  PRBool hasClip = display->IsAbsolutelyPositioned() && (display->mClipFlags & NS_STYLE_CLIP_RECT);
  PRBool hasOverflowClip = isBlockLevel && (display->mOverflow == NS_STYLE_OVERFLOW_CLIP);
  if (hasClip || hasOverflowClip) {
    nsSize frameSize = aFrame->GetSize();
    nsRect  clipRect;

    if (hasClip) {
      // Start with the 'auto' values and then factor in user specified values
      clipRect.SetRect(0, 0, frameSize.width, frameSize.height);

      if (display->mClipFlags & NS_STYLE_CLIP_RECT) {
        if (0 == (NS_STYLE_CLIP_TOP_AUTO & display->mClipFlags)) {
          clipRect.y = display->mClip.y;
        }
        if (0 == (NS_STYLE_CLIP_LEFT_AUTO & display->mClipFlags)) {
          clipRect.x = display->mClip.x;
        }
        if (0 == (NS_STYLE_CLIP_RIGHT_AUTO & display->mClipFlags)) {
          clipRect.width = display->mClip.width;
        }
        if (0 == (NS_STYLE_CLIP_BOTTOM_AUTO & display->mClipFlags)) {
          clipRect.height = display->mClip.height;
        }
      }
    }

    if (hasOverflowClip) {
      const nsStyleBorder* borderStyle = aStyleContext->GetStyleBorder();
      const nsStylePadding* paddingStyle = aStyleContext->GetStylePadding();

      nsMargin border, padding;
      // XXX We don't support the 'overflow-clip' property yet so just use the
      // content area (which is the default value) as the clip shape
      nsRect overflowClipRect(0, 0, frameSize.width, frameSize.height);
      borderStyle->GetBorder(border);
      overflowClipRect.Deflate(border);
      // XXX We need to handle percentage padding
      if (paddingStyle->GetPadding(padding)) {
        overflowClipRect.Deflate(padding);
      }

      if (hasClip) {
        // If both 'clip' and 'overflow-clip' apply then use the intersection
        // of the two
        clipRect.IntersectRect(clipRect, overflowClipRect);
      } else {
        clipRect = overflowClipRect;
      }
    }
  
    nsRect newSize = aView->GetBounds();
    newSize -= aView->GetPosition();

    // If part of the view is being clipped out, then mark it transparent
    if (clipRect.y > newSize.y
        || clipRect.x > newSize.x
        || clipRect.XMost() < newSize.XMost()
        || clipRect.YMost() < newSize.YMost()) {
      viewHasTransparentContent = PR_TRUE;
    }

    // Set clipping of child views.
    nsRegion region(clipRect);
    vm->SetViewChildClipRegion(aView, &region);
  } else {
    // Remove clipping of child views.
    vm->SetViewChildClipRegion(aView, nsnull);
  }

  vm->SetViewContentTransparency(aView, viewHasTransparentContent);
}

void
nsContainerFrame::SyncFrameViewAfterReflow(nsPresContext* aPresContext,
                                           nsIFrame*       aFrame,
                                           nsIView*        aView,
                                           const nsRect*   aCombinedArea,
                                           PRUint32        aFlags)
{
  if (!aView) {
    return;
  }

  // Make sure the view is sized and positioned correctly
  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aPresContext, aFrame);
  }

  if (0 == (aFlags & NS_FRAME_NO_SIZE_VIEW)) {
    nsIViewManager* vm = aView->GetViewManager();

    // If the frame has child frames that stick outside the content
    // area, then size the view large enough to include those child
    // frames
    NS_ASSERTION(!(aFrame->GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) ||
                 aCombinedArea,
                 "resizing view for frame with overflow to the wrong size");
    if ((aFrame->GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) && aCombinedArea) {
      vm->ResizeView(aView, *aCombinedArea, PR_TRUE);
    } else {
      nsSize frameSize = aFrame->GetSize();
      nsRect newSize(0, 0, frameSize.width, frameSize.height);
      vm->ResizeView(aView, newSize, PR_TRUE);
    }

    // Even if the size hasn't changed, we need to sync up the
    // geometry dependent properties, because (kidState &
    // NS_FRAME_OUTSIDE_CHILDREN) might have changed, and we can't
    // detect whether it has or not. Likewise, whether the view size
    // has changed or not, we may need to change the transparency
    // state even if there is no clip.
    nsStyleContext* savedStyleContext = aFrame->GetStyleContext();
    SyncFrameViewGeometryDependentProperties(aPresContext, aFrame, savedStyleContext, aView, aFlags);
  }
}

void
nsContainerFrame::SyncFrameViewAfterSizeChange(nsPresContext*  aPresContext,
                                               nsIFrame*        aFrame,
                                               nsStyleContext*  aStyleContext,
                                               nsIView*         aView,
                                               PRUint32         aFlags)
{
  NS_ASSERTION(!aStyleContext || aFrame->GetStyleContext() == aStyleContext,
               "Wrong style context for frame?");

  if (!aView) {
    return;
  }
  
  if (nsnull == aStyleContext) {
    aStyleContext = aFrame->GetStyleContext();
  }

  SyncFrameViewGeometryDependentProperties(aPresContext, aFrame, aStyleContext, aView, aFlags);
}

void
nsContainerFrame::SyncFrameViewProperties(nsPresContext*  aPresContext,
                                          nsIFrame*        aFrame,
                                          nsStyleContext*  aStyleContext,
                                          nsIView*         aView,
                                          PRUint32         aFlags)
{
  NS_ASSERTION(!aStyleContext || aFrame->GetStyleContext() == aStyleContext,
               "Wrong style context for frame?");

  if (!aView) {
    return;
  }

  nsIViewManager* vm = aView->GetViewManager();

  if (nsnull == aStyleContext) {
    aStyleContext = aFrame->GetStyleContext();
  }
    
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();

  // Set the view's opacity
  vm->SetViewOpacity(aView, display->mOpacity);

  // Make sure visibility is correct
  if (0 == (aFlags & NS_FRAME_NO_VISIBILITY)) {
    const nsStyleVisibility* vis = aStyleContext->GetStyleVisibility();

    // See if the view should be hidden or visible
    PRBool  viewIsVisible = PR_TRUE;

    if (NS_STYLE_VISIBILITY_COLLAPSE == vis->mVisible) {
      viewIsVisible = PR_FALSE;
    }
    else if (NS_STYLE_VISIBILITY_HIDDEN == vis->mVisible &&
             !aFrame->SupportsVisibilityHidden()) {
      // If it's a scrollable frame that can't hide its scrollbars,
      // hide the view. This means that child elements can't override
      // their parent's visibility, but it's not practical to leave it
      // visible in all cases because the scrollbars will be showing
      viewIsVisible = PR_FALSE;
    } else {
      // if the view is for a popup, don't show the view if the popup is closed
      nsIWidget* widget = aView->GetWidget();
      if (widget) {
        nsWindowType windowType;
        widget->GetWindowType(windowType);
        if (windowType == eWindowType_popup) {
          widget->IsVisible(viewIsVisible);
        }
      }
    }

    vm->SetViewVisibility(aView, viewIsVisible ? nsViewVisibility_kShow :
                          nsViewVisibility_kHide);
  }

  // See if the frame is being relatively positioned or absolutely
  // positioned
  PRBool isTopMostView = display->IsPositioned();

  // Make sure z-index is correct
  const nsStylePosition* position = aStyleContext->GetStylePosition();

  PRInt32 zIndex = 0;
  PRBool  autoZIndex = PR_FALSE;

  if (position->mZIndex.GetUnit() == eStyleUnit_Integer) {
    zIndex = position->mZIndex.GetIntValue();
  } else if (position->mZIndex.GetUnit() == eStyleUnit_Auto) {
    autoZIndex = PR_TRUE;
  }

  vm->SetViewZIndex(aView, autoZIndex, zIndex, isTopMostView);

  SyncFrameViewGeometryDependentProperties(aPresContext, aFrame, aStyleContext, aView, aFlags);
}

PRBool
nsContainerFrame::FrameNeedsView(nsIFrame* aFrame)
{
  nsStyleContext* sc = aFrame->GetStyleContext();
  const nsStyleDisplay* display = sc->GetStyleDisplay();
  if (display->mOpacity != 1.0f) {
    return PR_TRUE;
  }

  // See if the frame has a fixed background attachment
  const nsStyleBackground *color;
  PRBool isCanvas;
  PRBool hasBackground = 
    nsCSSRendering::FindBackground(aFrame->GetPresContext(),
                                   aFrame, &color, &isCanvas);
  if (hasBackground &&
      NS_STYLE_BG_ATTACHMENT_FIXED == color->mBackgroundAttachment) {
    return PR_TRUE;
  }
    
  if (NS_STYLE_POSITION_RELATIVE == display->mPosition) {
    return PR_TRUE;
  } else if (display->IsAbsolutelyPositioned()) {
    return PR_TRUE;
  } 

  if (sc->GetPseudoType() == nsCSSAnonBoxes::scrolledContent) {
    return PR_TRUE;
  }

  // See if the frame is block-level and has 'overflow' set to 'hidden'. If
  // so, then we need to give it a view so clipping
  // of any child views works correctly. Note that if it's floated it is also
  // block-level, but we can't trust that the style context 'display' value is
  // set correctly
  if ((display->IsBlockLevel() || display->IsFloating()) &&
      (display->mOverflow == NS_STYLE_OVERFLOW_CLIP)) {
    // XXX Check for the frame being a block frame and only force a view
    // in that case, because adding a view for box frames seems to cause
    // problems for XUL...
    nsIAtom* frameType = aFrame->GetType();
    if ((frameType == nsLayoutAtoms::blockFrame) ||
        (frameType == nsLayoutAtoms::areaFrame)) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

/**
 * Invokes the WillReflow() function, positions the frame and its view (if
 * requested), and then calls Reflow(). If the reflow succeeds and the child
 * frame is complete, deletes any next-in-flows using DeleteNextInFlowChild()
 */
nsresult
nsContainerFrame::ReflowChild(nsIFrame*                aKidFrame,
                              nsPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nscoord                  aX,
                              nscoord                  aY,
                              PRUint32                 aFlags,
                              nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");

  nsresult  result;

#ifdef DEBUG
#ifdef REALLY_NOISY_MAX_ELEMENT_SIZE
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = nscoord(0xdeadbeef);
  }
#endif
#endif

  // Send the WillReflow() notification, and position the child frame
  // and its view if requested
  aKidFrame->WillReflow(aPresContext);

  if (0 == (aFlags & NS_FRAME_NO_MOVE_FRAME)) {
    aKidFrame->SetPosition(nsPoint(aX, aY));
  }

  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aPresContext, aKidFrame);
  }

  // Reflow the child frame
  result = aKidFrame->Reflow(aPresContext, aDesiredSize, aReflowState,
                             aStatus);

#ifdef DEBUG
#ifdef REALLY_NOISY_MAX_ELEMENT_SIZE
  if (aDesiredSize.mComputeMEW &&
      (nscoord(0xdeadbeef) == aDesiredSize.mMaxElementWidth)) {
    printf("nsContainerFrame: ");
    nsFrame::ListTag(stdout, aKidFrame);
    printf(" didn't set max-element-width!\n");
  }
#endif
#endif

  // If the reflow was successful and the child frame is complete, delete any
  // next-in-flows
  if (NS_SUCCEEDED(result) && NS_FRAME_IS_COMPLETE(aStatus)) {
    nsIFrame* kidNextInFlow;
    aKidFrame->GetNextInFlow(&kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      NS_STATIC_CAST(nsContainerFrame*, kidNextInFlow->GetParent())
        ->DeleteNextInFlowChild(aPresContext, kidNextInFlow);
    }
  }
  return result;
}


/**
 * Position the views of |aFrame|'s descendants. A container frame
 * should call this method if it moves a frame after |Reflow|.
 */
void
nsContainerFrame::PositionChildViews(nsPresContext* aPresContext,
                                     nsIFrame*       aFrame)
{
  if (!(aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  nsIAtom*  childListName = nsnull;
  PRInt32   childListIndex = 0;

  do {
    // Recursively walk aFrame's child frames
    nsIFrame* childFrame = aFrame->GetFirstChild(childListName);
    while (childFrame) {
      // Position the frame's view (if it has one) otherwise recursively
      // process its children
      if (childFrame->HasView()) {
        PositionFrameView(aPresContext, childFrame);
      } else {
        PositionChildViews(aPresContext, childFrame);
      }

      // Get the next sibling child frame
      childFrame = childFrame->GetNextSibling();
    }

    childListName = aFrame->GetAdditionalChildListName(childListIndex++);
  } while (childListName);
}

/**
 * The second half of frame reflow. Does the following:
 * - sets the frame's bounds
 * - sizes and positions (if requested) the frame's view. If the frame's final
 *   position differs from the current position and the frame itself does not
 *   have a view, then any child frames with views are positioned so they stay
 *   in sync
 * - sets the view's visibility, opacity, content transparency, and clip
 * - invoked the DidReflow() function
 *
 * Flags:
 * NS_FRAME_NO_MOVE_FRAME - don't move the frame. aX and aY are ignored in this
 *    case. Also implies NS_FRAME_NO_MOVE_VIEW
 * NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
 *    don't want to automatically sync the frame and view
 * NS_FRAME_NO_SIZE_VIEW - don't size the frame's view
 */
nsresult
nsContainerFrame::FinishReflowChild(nsIFrame*                 aKidFrame,
                                    nsPresContext*           aPresContext,
                                    const nsHTMLReflowState*  aReflowState,
                                    nsHTMLReflowMetrics&      aDesiredSize,
                                    nscoord                   aX,
                                    nscoord                   aY,
                                    PRUint32                  aFlags)
{
  nsPoint curOrigin = aKidFrame->GetPosition();
  nsRect  bounds(aX, aY, aDesiredSize.width, aDesiredSize.height);

  aKidFrame->SetRect(bounds);

  if (aKidFrame->HasView()) {
    nsIView* view = aKidFrame->GetView();
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                             &aDesiredSize.mOverflowArea,
                             aFlags);
  }

  if (!(aFlags & NS_FRAME_NO_MOVE_VIEW) &&
      (curOrigin.x != aX || curOrigin.y != aY)) {
    if (!aKidFrame->HasView()) {
      // If the frame has moved, then we need to make sure any child views are
      // correctly positioned
      PositionChildViews(aPresContext, aKidFrame);
    }

    // We also need to redraw everything associated with the frame
    // because if the frame's Reflow issued any invalidates, then they
    // will be at the wrong offset ... note that this includes
    // invalidates issued against the frame's children, so we need to
    // invalidate the overflow area too.
    aKidFrame->Invalidate(aDesiredSize.mOverflowArea);
  }
  
  return aKidFrame->DidReflow(aPresContext, aReflowState, NS_FRAME_REFLOW_FINISHED);
}

/**
 * Remove and delete aNextInFlow and its next-in-flows. Updates the sibling and flow
 * pointers
 */
void
nsContainerFrame::DeleteNextInFlowChild(nsPresContext* aPresContext,
                                        nsIFrame*       aNextInFlow)
{
  nsIFrame* prevInFlow;
  aNextInFlow->GetPrevInFlow(&prevInFlow);
  NS_PRECONDITION(prevInFlow, "bad prev-in-flow");
  NS_PRECONDITION(mFrames.ContainsFrame(aNextInFlow), "bad geometric parent");

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  nsIFrame* nextNextInFlow;
  aNextInFlow->GetNextInFlow(&nextNextInFlow);
  if (nextNextInFlow) {
    NS_STATIC_CAST(nsContainerFrame*, nextNextInFlow->GetParent())
      ->DeleteNextInFlowChild(aPresContext, nextNextInFlow);
  }

#ifdef IBMBIDI
  if ((prevInFlow->GetStateBits() & NS_FRAME_IS_BIDI) &&
      (NS_STATIC_CAST(nsIFrame*,
                      prevInFlow->GetProperty(nsLayoutAtoms::nextBidi)) ==
       aNextInFlow)) {
    return;
  }
#endif // IBMBIDI

  // Disconnect the next-in-flow from the flow list
  nsSplittableFrame::BreakFromPrevFlow(aNextInFlow);

  // Take the next-in-flow out of the parent's child list
  PRBool result = mFrames.RemoveFrame(aNextInFlow);
  if (!result) {
    // We didn't find the child in the parent's principal child list.
    // Maybe it's on the overflow list?
    nsFrameList overflowFrames(GetOverflowFrames(aPresContext, PR_TRUE));

    if (overflowFrames.IsEmpty() || !overflowFrames.RemoveFrame(aNextInFlow)) {
      NS_ASSERTION(result, "failed to remove frame");
    }

    // Set the overflow property again
    if (overflowFrames.NotEmpty()) {
      SetOverflowFrames(aPresContext, overflowFrames.FirstChild());
    }
  }

  // Delete the next-in-flow frame and its descendants.
  aNextInFlow->Destroy(aPresContext);

#ifdef NS_DEBUG
  nsIFrame* nextInFlow;
  prevInFlow->GetNextInFlow(&nextInFlow);
  NS_POSTCONDITION(!nextInFlow, "non null next-in-flow");
#endif
}

nsIFrame*
nsContainerFrame::GetOverflowFrames(nsPresContext* aPresContext,
                                    PRBool          aRemoveProperty) const
{
  PRUint32  options = 0;

  if (aRemoveProperty) {
    options |= NS_IFRAME_MGR_REMOVE_PROP;
  }

  return (nsIFrame*) aPresContext->FrameManager()->
    GetFrameProperty(this, nsLayoutAtoms::overflowProperty, options);
}

// Destructor function for the overflow frame property
static void
DestroyOverflowFrames(nsPresContext* aPresContext,
                      nsIFrame*       aFrame,
                      nsIAtom*        aPropertyName,
                      void*           aPropertyValue)
{
  if (aPropertyValue) {
    nsFrameList frames((nsIFrame*)aPropertyValue);

    frames.DestroyFrames(aPresContext);
  }
}

nsresult
nsContainerFrame::SetOverflowFrames(nsPresContext* aPresContext,
                                    nsIFrame*       aOverflowFrames)
{
  nsresult rv = aPresContext->FrameManager()->
    SetFrameProperty(this, nsLayoutAtoms::overflowProperty,
                     aOverflowFrames, DestroyOverflowFrames);

  // Verify that we didn't overwrite an existing overflow list
  NS_ASSERTION(rv != NS_IFRAME_MGR_PROP_OVERWRITTEN, "existing overflow list");

  return rv;
}

/**
 * Push aFromChild and its next siblings to the next-in-flow. Change the
 * geometric parent of each frame that's pushed. If there is no next-in-flow
 * the frames are placed on the overflow list (and the geometric parent is
 * left unchanged).
 *
 * Updates the next-in-flow's child count. Does <b>not</b> update the
 * pusher's child count.
 *
 * @param   aFromChild the first child frame to push. It is disconnected from
 *            aPrevSibling
 * @param   aPrevSibling aFromChild's previous sibling. Must not be null. It's
 *            an error to push a parent's first child frame
 */
void
nsContainerFrame::PushChildren(nsPresContext* aPresContext,
                               nsIFrame*       aFromChild,
                               nsIFrame*       aPrevSibling)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
  NS_PRECONDITION(aPrevSibling->GetNextSibling() == aFromChild, "bad prev sibling");

  // Disconnect aFromChild from its previous sibling
  aPrevSibling->SetNextSibling(nsnull);

  if (nsnull != mNextInFlow) {
    // XXX This is not a very good thing to do. If it gets removed
    // then remove the copy of this routine that doesn't do this from
    // nsInlineFrame.
    nsContainerFrame* nextInFlow = (nsContainerFrame*)mNextInFlow;
    // When pushing and pulling frames we need to check for whether any
    // views need to be reparented.
    for (nsIFrame* f = aFromChild; f; f = f->GetNextSibling()) {
      nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, this, mNextInFlow);
    }
    nextInFlow->mFrames.InsertFrames(mNextInFlow, nsnull, aFromChild);
  }
  else {
    // Add the frames to our overflow list
    SetOverflowFrames(aPresContext, aFromChild);
  }
}

/**
 * Moves any frames on the overflow lists (the prev-in-flow's overflow list and
 * the receiver's overflow list) to the child list.
 *
 * Updates this frame's child count and content mapping.
 *
 * @return  PR_TRUE if any frames were moved and PR_FALSE otherwise
 */
PRBool
nsContainerFrame::MoveOverflowToChildList(nsPresContext* aPresContext)
{
  PRBool result = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsContainerFrame* prevInFlow = (nsContainerFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext,
                                                                 PR_TRUE);
    if (prevOverflowFrames) {
      NS_ASSERTION(mFrames.IsEmpty(), "bad overflow list");
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      for (nsIFrame* f = prevOverflowFrames; f; f = f->GetNextSibling()) {
        nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, prevInFlow, this);
      }
      mFrames.InsertFrames(this, nsnull, prevOverflowFrames);
      result = PR_TRUE;
    }
  }

  // It's also possible that we have an overflow list for ourselves
  nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    mFrames.AppendFrames(nsnull, overflowFrames);
    result = PR_TRUE;
  }
  return result;
}

/////////////////////////////////////////////////////////////////////////////
// Debugging

#ifdef NS_DEBUG
NS_IMETHODIMP
nsContainerFrame::List(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", NS_STATIC_CAST(void*, mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, GetView()));
  }
  if (nsnull != mNextSibling) {
    fprintf(out, " next=%p", NS_STATIC_CAST(void*, mNextSibling));
  }
  if (nsnull != mPrevInFlow) {
    fprintf(out, " prev-in-flow=%p", NS_STATIC_CAST(void*, mPrevInFlow));
  }
  if (nsnull != mNextInFlow) {
    fprintf(out, " next-in-flow=%p", NS_STATIC_CAST(void*, mNextInFlow));
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fprintf(out, " [content=%p]", NS_STATIC_CAST(void*, mContent));
  fprintf(out, " [sc=%p]", NS_STATIC_CAST(void*, mStyleContext));

  // Output the children
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  PRBool outputOneList = PR_FALSE;
  do {
    nsIFrame* kid = GetFirstChild(listName);
    if (nsnull != kid) {
      if (outputOneList) {
        IndentBy(out, aIndent);
      }
      outputOneList = PR_TRUE;
      nsAutoString tmp;
      if (nsnull != listName) {
        listName->ToString(tmp);
        fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
      }
      fputs("<\n", out);
      while (nsnull != kid) {
        // Verify the child frame's parent frame pointer is correct
        NS_ASSERTION(kid->GetParent() == (nsIFrame*)this, "bad parent frame pointer");

        // Have the child frame list
        nsIFrameDebug*  frameDebug;
        if (NS_SUCCEEDED(kid->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->List(aPresContext, out, aIndent + 1);
        }
        kid = kid->GetNextSibling();
      }
      IndentBy(out, aIndent);
      fputs(">\n", out);
    }
    listName = GetAdditionalChildListName(listIndex++);
  } while(nsnull != listName);

  if (!outputOneList) {
    fputs("<>\n", out);
  }

  return NS_OK;
}
#endif
