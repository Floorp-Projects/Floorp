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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsContainerFrame.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsVoidArray.h"
#include "nsISizeOfHandler.h"
#include "nsHTMLReflowCommand.h"
#include "nsHTMLContainerFrame.h"
#include "nsIFrameManager.h"
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

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

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
nsContainerFrame::Init(nsIPresContext*  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIStyleContext* aContext,
                       nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSplittableFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  if (aPrevInFlow) {
    // Make sure we copy bits from our prev-in-flow that will affect
    // us. A continuation for a container frame needs to know if it
    // has a child with a view so that we'll properly reposition it.
    nsFrameState state;
    aPrevInFlow->GetFrameState(&state);
    if (state & NS_FRAME_HAS_CHILD_WITH_VIEW)
      mState |= NS_FRAME_HAS_CHILD_WITH_VIEW;
  }
  return rv;
}

NS_IMETHODIMP
nsContainerFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aChildList)
{
  nsresult  result;
  if (!mFrames.IsEmpty()) {
    // We already have child frames which means we've already been
    // initialized
#ifdef DEBUG_dbaron // XXX Fix asserts and remove this ifdef.
    NS_NOTREACHED("unexpected second call to SetInitialChildList");
#endif
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

NS_IMETHODIMP
nsContainerFrame::Destroy(nsIPresContext* aPresContext)
{
  // Prevent event dispatch during destruction
  nsIView* view;
  GetView(aPresContext, &view);
  if (nsnull != view) {
    view->SetClientData(nsnull);
  }

  // Delete the primary child list
  mFrames.DestroyFrames(aPresContext);

  // Destroy the frame and remove the flow pointers
  return nsSplittableFrame::Destroy(aPresContext);
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_IMETHODIMP
nsContainerFrame::FirstChild(nsIPresContext* aPresContext,
                             nsIAtom*        aListName,
                             nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  // We only know about the unnamed principal child list and the overflow
  // list
  if (nsnull == aListName) {
    *aFirstChild = mFrames.FirstChild();
    return NS_OK;
  } else if (nsLayoutAtoms::overflowList == aListName) {
    *aFirstChild = GetOverflowFrames(aPresContext, PR_FALSE);
    return NS_OK;
  } else {
    *aFirstChild = nsnull;
    return NS_ERROR_INVALID_ARG;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Painting/Events

NS_IMETHODIMP
nsContainerFrame::Paint(nsIPresContext*      aPresContext,
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
nsContainerFrame::PaintChildren(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer,
                                PRUint32             aFlags)
{
  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid, aWhichLayer, aFlags);
    kid->GetNextSibling(&kid);
  }
}

// Paint one child frame
void
nsContainerFrame::PaintChild(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsIFrame*            aFrame,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags)
{
  NS_ASSERTION(aFrame, "no frame to paint!");
  if (!aFrame) return;
  nsIView *pView;
  aFrame->GetView(aPresContext, &pView);
  if (!pView) {
    nsRect kidRect;
    aFrame->GetRect(kidRect);
    nsFrameState state;
    aFrame->GetFrameState(&state);

    // Compute the constrained damage area; set the overlap flag to
    // PR_TRUE if any portion of the child frame intersects the
    // dirty rect.
    nsRect damageArea;
    PRBool overlap;
    if (NS_FRAME_OUTSIDE_CHILDREN & state) {
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
nsContainerFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND), aFrame);
}

nsresult
nsContainerFrame::GetFrameForPointUsing(nsIPresContext* aPresContext,
                                        const nsPoint& aPoint,
                                        nsIAtom*       aList,
                                        nsFramePaintLayer aWhichLayer,
                                        PRBool         aConsiderSelf,
                                        nsIFrame**     aFrame)
{
  nsIFrame *kid, *hit;
  nsPoint tmp;

  PRBool inThisFrame = mRect.Contains(aPoint);

  if (! ((mState & NS_FRAME_OUTSIDE_CHILDREN) || inThisFrame ) ) {
    return NS_ERROR_FAILURE;
  }

  FirstChild(aPresContext, aList, &kid);
  *aFrame = nsnull;
  tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);

  nsPoint originOffset;
  nsIView *view = nsnull;
  nsresult rv = GetOriginToViewOffset(aPresContext, originOffset, &view);

  if (NS_SUCCEEDED(rv) && view)
    tmp += originOffset;

  while (nsnull != kid) {
    rv = kid->GetFrameForPoint(aPresContext, tmp, aWhichLayer, &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      *aFrame = hit;
    }
    kid->GetNextSibling(&kid);
  }

  if (*aFrame) {
    return NS_OK;
  }

  if ( inThisFrame && aConsiderSelf ) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    if (vis->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsContainerFrame::ReplaceFrame(nsIPresContext* aPresContext,
                               nsIPresShell&   aPresShell,
                               nsIAtom*        aListName,
                               nsIFrame*       aOldFrame,
                               nsIFrame*       aNewFrame)
{
  nsIFrame* prevFrame;
  nsIFrame* firstChild;
  nsresult  rv;

  // Get the old frame's previous sibling frame
  FirstChild(aPresContext, aListName, &firstChild);
  nsFrameList frames(firstChild);
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
    nscoord x, y;

    aChildView->GetPosition(&x, &y);
    aPoint.x += x;
    aPoint.y += y;
    aChildView->GetParent(aChildView);
  } while (aChildView && (aChildView != aParentView));

  return aChildView == aParentView;
}

/**
 * Position the view associated with |aKidFrame|, if there is one. A
 * container frame should call this method after positioning a frame,
 * but before |Reflow|.
 */
void
nsContainerFrame::PositionFrameView(nsIPresContext* aPresContext,
                                    nsIFrame*       aKidFrame)
{
  nsIView* view;
  aKidFrame->GetView(aPresContext, &view);
  if (view) {
    // Position view relative to its parent, not relative to aKidFrame's
    // frame which may not have a view
    nsIView*        parentView;
    view->GetParent(parentView);

    nsIView*        containingView;
    nsPoint         origin;
    aKidFrame->GetOffsetFromView(aPresContext, origin, &containingView);

    nsCOMPtr<nsIViewManager> vm;
    view->GetViewManager(*getter_AddRefs(vm));

    // it's possible for the parentView to be nonnull but containingView to be
    // null, when the parent view doesn't belong to this frame tree but to
    // the frame tree of some enclosing document. We do nothing in that case,
    // but we have to check that containingView is nonnull or we will crash.
    if (nsnull != containingView && containingView != parentView) {
      // it is possible for parent view not to have a frame attached to it
      // kind of an anonymous view. This happens with native scrollbars and
      // the clip view. To fix this we need to go up and parentView chain
      // until we find a view with client data. This is total HACK to fix
      // the HACK below. COMBO box code should NOT be in the container code!!!
      // And the case it looks from does not just happen for combo boxes. Native
      // scrollframes get in this situation too!! 
      while(parentView) {
        void *data = 0;
        parentView->GetClientData(data);
        if (data)
          break;
      
        nscoord x, y;
        parentView->GetPosition(&x, &y);
        origin.x -= x;
        origin.y -= y;
        parentView->GetParent(parentView);
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

static void
SyncFrameViewGeometryDependentProperties(nsIPresContext*  aPresContext,
                                         nsIFrame*        aFrame,
                                         nsIStyleContext* aStyleContext,
                                         nsIView*         aView,
                                         PRUint32         aFlags)
{
  nsCOMPtr<nsIViewManager> vm;
  aView->GetViewManager(*getter_AddRefs(vm));

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

  PRBool  viewHasTransparentContent =
    !hasBG ||
    (bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) ||
    !aFrame->CanPaintBackground();
  if (isCanvas && viewHasTransparentContent) {
    nsIView* rootView;
    vm->GetRootView(rootView);
    nsIView* rootParent;
    rootView->GetParent(rootParent);
    if (nsnull == rootParent) {
      viewHasTransparentContent = PR_FALSE;
    }
  }

  const nsStyleDisplay* display;
  ::GetStyleData(aStyleContext, &display);
  nsFrameState kidState;
  aFrame->GetFrameState(&kidState);
  
  if (!viewHasTransparentContent) {
    // If we're showing the view but the frame is hidden, then the view is transparent
    nsViewVisibility visibility;
    aView->GetVisibility(visibility);
    const nsStyleVisibility* vis;
    ::GetStyleData(aStyleContext, &vis);
    if ((nsViewVisibility_kShow == visibility
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
  PRBool hasOverflowClip = isBlockLevel && (display->mOverflow == NS_STYLE_OVERFLOW_HIDDEN);
  if (hasClip || hasOverflowClip) {
    nsSize frameSize;
    aFrame->GetSize(frameSize);
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
      const nsStyleBorder* borderStyle;
      ::GetStyleData(aStyleContext, &borderStyle);
      const nsStylePadding* paddingStyle;
      ::GetStyleData(aStyleContext, &paddingStyle);

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
  
    nsRect newSize;
    aView->GetBounds(newSize);
    nscoord x, y;
    aView->GetPosition(&x, &y);
    newSize.x -= x;
    newSize.y -= y;

    // If part of the view is being clipped out, then mark it transparent
    if (clipRect.y > newSize.y
        || clipRect.x > newSize.x
        || clipRect.XMost() < newSize.XMost()
        || clipRect.YMost() < newSize.YMost()) {
      viewHasTransparentContent = PR_TRUE;
    }

    // Set clipping of child views.
    nsRegion region;
    region.Copy(nsRectFast(clipRect));
    vm->SetViewChildClipRegion(aView, &region);
  } else {
    // Remove clipping of child views.
    vm->SetViewChildClipRegion(aView, nsnull);
  }

  vm->SetViewContentTransparency(aView, viewHasTransparentContent);
}

void
nsContainerFrame::SyncFrameViewAfterReflow(nsIPresContext* aPresContext,
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
    nsCOMPtr<nsIViewManager> vm;
    aView->GetViewManager(*getter_AddRefs(vm));
    nsRect oldBounds;
    aView->GetBounds(oldBounds);
    nsFrameState kidState;
    aFrame->GetFrameState(&kidState);

    // If the frame has child frames that stick outside the content
    // area, then size the view large enough to include those child
    // frames
    if ((kidState & NS_FRAME_OUTSIDE_CHILDREN) && aCombinedArea) {
      vm->ResizeView(aView, *aCombinedArea);
    } else {
      // If the width is unchanged and the height is not decreased
      // then repaint only the newly exposed or contracted area,
      // otherwise repaint the union of the old and new areas

      // XXX: We currently invalidate the newly exposed areas only
      // when the container frame's width is unchanged and the height
      // is either unchanged or increased This is because some frames
      // do not invalidate themselves properly. see bug 73825.  Once
      // bug 73825 is fixed, we should always pass PR_TRUE instead of
      // frameSize.width == width && frameSize.height >= height.
      nsSize frameSize;
      aFrame->GetSize(frameSize);
      nsRect newSize(0, 0, frameSize.width, frameSize.height);
      vm->ResizeView(aView, newSize, 
                     (frameSize.width == oldBounds.width && frameSize.height >= oldBounds.height));
    }

    // Even if the size hasn't changed, we need to sync up the
    // geometry dependent properties, because (kidState &
    // NS_FRAME_OUTSIDE_CHILDREN) might have changed, and we can't
    // detect whether it has or not. Likewise, whether the view size
    // has changed or not, we may need to change the transparency
    // state even if there is no clip.
    nsCOMPtr<nsIStyleContext> savedStyleContext;
    aFrame->GetStyleContext(getter_AddRefs(savedStyleContext));
    SyncFrameViewGeometryDependentProperties(aPresContext, aFrame, savedStyleContext, aView, aFlags);
  }
}

void
nsContainerFrame::SyncFrameViewAfterSizeChange(nsIPresContext*  aPresContext,
                                               nsIFrame*        aFrame,
                                               nsIStyleContext* aStyleContext,
                                               nsIView*         aView,
                                               PRUint32         aFlags)
{
  if (!aView) {
    return;
  }
  
  nsCOMPtr<nsIStyleContext> savedStyleContext;
  if (nsnull == aStyleContext) {
    aFrame->GetStyleContext(getter_AddRefs(savedStyleContext));
    aStyleContext = savedStyleContext;
  }

  SyncFrameViewGeometryDependentProperties(aPresContext, aFrame, aStyleContext, aView, aFlags);
}

void
nsContainerFrame::SyncFrameViewProperties(nsIPresContext*  aPresContext,
                                          nsIFrame*        aFrame,
                                          nsIStyleContext* aStyleContext,
                                          nsIView*         aView,
                                          PRUint32         aFlags)
{
  if (!aView) {
    return;
  }

  nsCOMPtr<nsIViewManager> vm;
  aView->GetViewManager(*getter_AddRefs(vm));

  nsCOMPtr<nsIStyleContext> savedStyleContext;
  if (nsnull == aStyleContext) {
    aFrame->GetStyleContext(getter_AddRefs(savedStyleContext));
    aStyleContext = savedStyleContext;
  }
    
  const nsStyleVisibility* vis;
  ::GetStyleData(aStyleContext, &vis);

  // Set the view's opacity
  vm->SetViewOpacity(aView, vis->mOpacity);

  // Make sure visibility is correct
  if (0 == (aFlags & NS_FRAME_NO_VISIBILITY)) {
    // See if the view should be hidden or visible
    PRBool  viewIsVisible = PR_TRUE;

    if (NS_STYLE_VISIBILITY_COLLAPSE == vis->mVisible) {
      viewIsVisible = PR_FALSE;
    }
    else if (NS_STYLE_VISIBILITY_HIDDEN == vis->mVisible) {
      // If it has a widget, hide the view because the widget can't deal with it
      nsCOMPtr<nsIWidget> widget;
      aView->GetWidget(*getter_AddRefs(widget));
      if (widget) {
        viewIsVisible = PR_FALSE;
      }
      else {
        // If it's a scroll frame or a list control frame which is derived from the scrollframe, 
        // then hide the view. This means that
        // child elements can't override their parent's visibility, but
        // it's not practical to leave it visible in all cases because
        // the scrollbars will be showing
        nsCOMPtr<nsIAtom> frameType;
        aFrame->GetFrameType(getter_AddRefs(frameType));

        if (frameType == nsLayoutAtoms::scrollFrame || frameType == nsLayoutAtoms::listControlFrame) {
          viewIsVisible = PR_FALSE;
        }
      }
    } else {
      // if the view is for a popup, don't show the view if the popup is closed
      nsCOMPtr<nsIWidget> widget;
      aView->GetWidget(*getter_AddRefs(widget));
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

  const nsStyleDisplay* display;
  ::GetStyleData(aStyleContext, &display);
  // See if the frame is being relatively positioned or absolutely
  // positioned
  PRBool isTopMostView = display->IsPositioned();

  // Make sure z-index is correct
  const nsStylePosition* position;
  ::GetStyleData(aStyleContext, &position);

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
nsContainerFrame::FrameNeedsView(nsIPresContext* aPresContext,
                                 nsIFrame* aFrame,
                                 nsIStyleContext* aStyleContext)
{
  const nsStyleVisibility* vis;
  ::GetStyleData(aStyleContext, &vis);
    
  if (vis->mOpacity != 1.0f) {
    return PR_TRUE;
  }

  // See if the frame has a fixed background attachment
  const nsStyleBackground *color;
  PRBool isCanvas;
  PRBool hasBackground = 
    nsCSSRendering::FindBackground(aPresContext, aFrame, &color, &isCanvas);
  if (hasBackground &&
      NS_STYLE_BG_ATTACHMENT_FIXED == color->mBackgroundAttachment) {
    return PR_TRUE;
  }
    
  const nsStyleDisplay* display;
  ::GetStyleData(aStyleContext, &display);

  if (NS_STYLE_POSITION_RELATIVE == display->mPosition) {
    return PR_TRUE;
  } else if (display->IsAbsolutelyPositioned()) {
    return PR_TRUE;
  } 

  nsCOMPtr<nsIAtom>  pseudoTag;
  aStyleContext->GetPseudoType(*getter_AddRefs(pseudoTag));
  if (pseudoTag == nsCSSAnonBoxes::scrolledContent) {
    return PR_TRUE;
  }

  // See if the frame is block-level and has 'overflow' set to 'hidden'. If
  // so, then we need to give it a view so clipping
  // of any child views works correctly. Note that if it's floated it is also
  // block-level, but we can't trust that the style context 'display' value is
  // set correctly
  if ((display->IsBlockLevel() || display->IsFloating()) &&
      (display->mOverflow == NS_STYLE_OVERFLOW_HIDDEN)) {
    // XXX Check for the frame being a block frame and only force a view
    // in that case, because adding a view for box frames seems to cause
    // problems for XUL...
    nsCOMPtr<nsIAtom> frameType;
    
    aFrame->GetFrameType(getter_AddRefs(frameType));
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
                              nsIPresContext*          aPresContext,
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
    aKidFrame->MoveTo(aPresContext, aX, aY);
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
      nsContainerFrame* parent;
      kidNextInFlow->GetParent((nsIFrame**)&parent);
      parent->DeleteNextInFlowChild(aPresContext, kidNextInFlow);
    }
  }
  return result;
}


/**
 * Position the views of |aFrame|'s descendants. A container frame
 * should call this method if it moves a frame after |Reflow|.
 */
void
nsContainerFrame::PositionChildViews(nsIPresContext* aPresContext,
                                     nsIFrame*       aFrame)
{
  nsFrameState frameState;
  aFrame->GetFrameState(&frameState);
  if (! ((frameState & NS_FRAME_HAS_CHILD_WITH_VIEW) == NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  nsIAtom*  childListName = nsnull;
  PRInt32   childListIndex = 0;

  do {
    // Recursively walk aFrame's child frames
    nsIFrame* childFrame;
    aFrame->FirstChild(aPresContext, childListName, &childFrame);
    while (childFrame) {
      // Position the frame's view (if it has one) and recursively
      // process its children
      PositionFrameView(aPresContext, childFrame);
      PositionChildViews(aPresContext, childFrame);

      // Get the next sibling child frame
      childFrame->GetNextSibling(&childFrame);
    }

    NS_IF_RELEASE(childListName);
    aFrame->GetAdditionalChildListName(childListIndex++, &childListName);
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
                                    nsIPresContext*           aPresContext,
                                    const nsHTMLReflowState*  aReflowState,
                                    nsHTMLReflowMetrics&      aDesiredSize,
                                    nscoord                   aX,
                                    nscoord                   aY,
                                    PRUint32                  aFlags)
{
  nsPoint curOrigin;
  nsRect  bounds(aX, aY, aDesiredSize.width, aDesiredSize.height);

  aKidFrame->GetOrigin(curOrigin);
  aKidFrame->SetRect(aPresContext, bounds);

  nsIView*  view;
  aKidFrame->GetView(aPresContext, &view);
  if (view) {
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                             &aDesiredSize.mOverflowArea,
                             aFlags);
  }
  else if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW) &&
           ((curOrigin.x != aX) || (curOrigin.y != aY))) {
    // If the frame has moved, then we need to make sure any child views are
    // correctly positioned
    PositionChildViews(aPresContext, aKidFrame);
  }
  
  return aKidFrame->DidReflow(aPresContext, aReflowState, NS_FRAME_REFLOW_FINISHED);
}

/**
 * Remove and delete aNextInFlow and its next-in-flows. Updates the sibling and flow
 * pointers
 */
void
nsContainerFrame::DeleteNextInFlowChild(nsIPresContext* aPresContext,
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
    nsContainerFrame* parent;
    nextNextInFlow->GetParent((nsIFrame**)&parent);
    parent->DeleteNextInFlowChild(aPresContext, nextNextInFlow);
  }

#ifdef IBMBIDI
  nsIFrame* nextBidi;
  prevInFlow->GetBidiProperty(aPresContext, nsLayoutAtoms::nextBidi,
                              (void**) &nextBidi,sizeof(nextBidi));
  if (nextBidi == aNextInFlow) {
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
nsContainerFrame::GetOverflowFrames(nsIPresContext* aPresContext,
                                    PRBool          aRemoveProperty) const
{
  nsCOMPtr<nsIPresShell>     presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  if (presShell) {
    nsCOMPtr<nsIFrameManager>  frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
  
    if (frameManager) {
      PRUint32  options = 0;
      void*     value;
  
      if (aRemoveProperty) {
        options |= NS_IFRAME_MGR_REMOVE_PROP;
      }
      frameManager->GetFrameProperty((nsIFrame*)this, nsLayoutAtoms::overflowProperty,
                                     options, &value);
      return (nsIFrame*)value;
    }
  }

  return nsnull;
}

// Destructor function for the overflow frame property
static void
DestroyOverflowFrames(nsIPresContext* aPresContext,
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
nsContainerFrame::SetOverflowFrames(nsIPresContext* aPresContext,
                                    nsIFrame*       aOverflowFrames)
{
  nsCOMPtr<nsIPresShell>     presShell;
  nsresult                   rv = NS_ERROR_FAILURE;

  aPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIFrameManager>  frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
  
    if (frameManager) {
      rv = frameManager->SetFrameProperty(this, nsLayoutAtoms::overflowProperty,
                                          aOverflowFrames, DestroyOverflowFrames);

      // Verify that we didn't overwrite an existing overflow list
      NS_ASSERTION(rv != NS_IFRAME_MGR_PROP_OVERWRITTEN,
                   "existing overflow list");
    }
  }

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
nsContainerFrame::PushChildren(nsIPresContext* aPresContext,
                               nsIFrame*       aFromChild,
                               nsIFrame*       aPrevSibling)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
#ifdef NS_DEBUG
  nsIFrame* prevNextSibling;
  aPrevSibling->GetNextSibling(&prevNextSibling);
  NS_PRECONDITION(prevNextSibling == aFromChild, "bad prev sibling");
#endif

  // Disconnect aFromChild from its previous sibling
  aPrevSibling->SetNextSibling(nsnull);

  if (nsnull != mNextInFlow) {
    // XXX This is not a very good thing to do. If it gets removed
    // then remove the copy of this routine that doesn't do this from
    // nsInlineFrame.
    nsContainerFrame* nextInFlow = (nsContainerFrame*)mNextInFlow;
    // When pushing and pulling frames we need to check for whether any
    // views need to be reparented.
    for (nsIFrame* f = aFromChild; f; f->GetNextSibling(&f)) {
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
nsContainerFrame::MoveOverflowToChildList(nsIPresContext* aPresContext)
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
      for (nsIFrame* f = prevOverflowFrames; f; f->GetNextSibling(&f)) {
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
nsContainerFrame::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", NS_STATIC_CAST(void*, mParent));
#endif
  nsIView* view;
  GetView(aPresContext, &view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, view));
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
    nsIFrame* kid;
    FirstChild(aPresContext, listName, &kid);
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
        nsIFrame* parentFrame;
        kid->GetParent(&parentFrame);
        NS_ASSERTION(parentFrame == (nsIFrame*)this, "bad parent frame pointer");

        // Have the child frame list
        nsIFrameDebug*  frameDebug;
        if (NS_SUCCEEDED(kid->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->List(aPresContext, out, aIndent + 1);
        }
        kid->GetNextSibling(&kid);
      }
      IndentBy(out, aIndent);
      fputs(">\n", out);
    }
    NS_IF_RELEASE(listName);
    GetAdditionalChildListName(listIndex++, &listName);
  } while(nsnull != listName);

  if (!outputOneList) {
    fputs("<>\n", out);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContainerFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
