/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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

#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsFrameList.h"
#include "nsLayoutAtoms.h"
#include "nsIAtom.h"
#include "nsCSSPseudoElements.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsPlaceholderFrame.h"
#include "nsIScrollableFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsGUIEvent.h"
#include "nsDisplayList.h"
#include "nsRegion.h"

#ifdef MOZ_SVG_FOREIGNOBJECT
#include "nsSVGForeignObjectFrame.h"
#include "nsSVGUtils.h"
#include "nsSVGOuterSVGFrame.h"
#endif

/**
 * A namespace class for static layout utilities.
 */

/**
 * GetFirstChildFrame returns the first "real" child frame of a
 * given frame.  It will descend down into pseudo-frames (unless the
 * pseudo-frame is the :before generated frame).   
 * @param aFrame the frame
 * @param aFrame the frame's content node
 */
static nsIFrame*
GetFirstChildFrame(nsIFrame*       aFrame,
                   nsIContent*     aContent)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  // Get the first child frame
  nsIFrame* childFrame = aFrame->GetFirstChild(nsnull);

  // If the child frame is a pseudo-frame, then return its first child.
  // Note that the frame we create for the generated content is also a
  // pseudo-frame and so don't drill down in that case
  if (childFrame &&
      childFrame->IsPseudoFrame(aContent) &&
      !childFrame->IsGeneratedContentFrame()) {
    return GetFirstChildFrame(childFrame, aContent);
  }

  return childFrame;
}

/**
 * GetLastChildFrame returns the last "real" child frame of a
 * given frame.  It will descend down into pseudo-frames (unless the
 * pseudo-frame is the :after generated frame).   
 * @param aFrame the frame
 * @param aFrame the frame's content node
 */
static nsIFrame*
GetLastChildFrame(nsIFrame*       aFrame,
                  nsIContent*     aContent)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  // Get the last in flow frame
  nsIFrame* lastInFlow = aFrame->GetLastInFlow();

  // Get the last child frame
  nsIFrame* firstChildFrame = lastInFlow->GetFirstChild(nsnull);
  if (firstChildFrame) {
    nsFrameList frameList(firstChildFrame);
    nsIFrame*   lastChildFrame = frameList.LastChild();

    NS_ASSERTION(lastChildFrame, "unexpected error");

    // Get the frame's first-in-flow. This matters in case the frame has
    // been continuted across multiple lines
    lastChildFrame = lastChildFrame->GetFirstInFlow();
    
    // If the last child frame is a pseudo-frame, then return its last child.
    // Note that the frame we create for the generated content is also a
    // pseudo-frame and so don't drill down in that case
    if (lastChildFrame &&
        lastChildFrame->IsPseudoFrame(aContent) &&
        !lastChildFrame->IsGeneratedContentFrame()) {
      return GetLastChildFrame(lastChildFrame, aContent);
    }

    return lastChildFrame;
  }

  return nsnull;
}

// static
nsIFrame*
nsLayoutUtils::GetBeforeFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");
  NS_ASSERTION(!aFrame->GetPrevInFlow(), "aFrame must be first-in-flow");
  
  nsIFrame* firstFrame = GetFirstChildFrame(aFrame, aFrame->GetContent());

  if (firstFrame && IsGeneratedContentFor(nsnull, firstFrame,
                                          nsCSSPseudoElements::before)) {
    return firstFrame;
  }

  return nsnull;
}

// static
nsIFrame*
nsLayoutUtils::GetAfterFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  nsIFrame* lastFrame = GetLastChildFrame(aFrame, aFrame->GetContent());

  if (lastFrame && IsGeneratedContentFor(nsnull, lastFrame,
                                         nsCSSPseudoElements::after)) {
    return lastFrame;
  }

  return nsnull;
}

// static
nsIFrame*
nsLayoutUtils::GetPageFrame(nsIFrame* aFrame)
{
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    if (frame->GetType() == nsLayoutAtoms::pageFrame) {
      return frame;
    }
  }
  return nsnull;
}

nsIFrame*
nsLayoutUtils::GetFloatFromPlaceholder(nsIFrame* aFrame) {
  if (nsLayoutAtoms::placeholderFrame != aFrame->GetType()) {
    return nsnull;
  }

  nsIFrame *outOfFlowFrame =
    nsPlaceholderFrame::GetRealFrameForPlaceholder(aFrame);
  if (outOfFlowFrame->GetStyleDisplay()->IsFloating()) {
    return outOfFlowFrame;
  }

  return nsnull;
}

// static
PRBool
nsLayoutUtils::IsGeneratedContentFor(nsIContent* aContent,
                                     nsIFrame* aFrame,
                                     nsIAtom* aPseudoElement)
{
  NS_PRECONDITION(aFrame, "Must have a frame");
  NS_PRECONDITION(aPseudoElement, "Must have a pseudo name");

  if (!aFrame->IsGeneratedContentFrame()) {
    return PR_FALSE;
  }
  
  if (aContent && aFrame->GetContent() != aContent) {
    return PR_FALSE;
  }

  return aFrame->GetStyleContext()->GetPseudoType() == aPseudoElement;
}

// static
nsIFrame*
nsLayoutUtils::GetCrossDocParentFrame(nsIFrame* aFrame)
{
  nsIFrame* p = aFrame->GetParent();
  if (p)
    return p;

  nsIView* v = aFrame->GetView();
  if (!v)
    return nsnull;
  v = v->GetParent(); // anonymous inner view
  if (!v)
    return nsnull;
  v = v->GetParent(); // subdocumentframe's view
  if (!v)
    return nsnull;
  return NS_STATIC_CAST(nsIFrame*, v->GetClientData());
}

// static
PRBool
nsLayoutUtils::IsProperAncestorFrameCrossDoc(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                             nsIFrame* aCommonAncestor)
{
  if (aFrame == aCommonAncestor)
    return PR_FALSE;
  
  nsIFrame* parentFrame = GetCrossDocParentFrame(aFrame);

  while (parentFrame != aCommonAncestor) {
    if (parentFrame == aAncestorFrame)
      return PR_TRUE;

    parentFrame = GetCrossDocParentFrame(parentFrame);
  }

  return PR_FALSE;
}

// static
PRBool
nsLayoutUtils::IsProperAncestorFrame(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                     nsIFrame* aCommonAncestor)
{
  if (aFrame == aCommonAncestor) {
    return PR_FALSE;
  }
  
  nsIFrame* parentFrame = aFrame->GetParent();

  while (parentFrame != aCommonAncestor) {
    if (parentFrame == aAncestorFrame) {
      return PR_TRUE;
    }

    parentFrame = parentFrame->GetParent();
  }

  return PR_FALSE;
}

// static
PRInt32
nsLayoutUtils::DoCompareTreePosition(nsIContent* aContent1,
                                     nsIContent* aContent2,
                                     PRInt32 aIf1Ancestor,
                                     PRInt32 aIf2Ancestor,
                                     nsIContent* aCommonAncestor)
{
  NS_PRECONDITION(aContent1, "aContent1 must not be null");
  NS_PRECONDITION(aContent2, "aContent2 must not be null");

  nsAutoVoidArray content1Ancestors;
  nsINode* c1;
  for (c1 = aContent1; c1 && c1 != aCommonAncestor; c1 = c1->GetNodeParent()) {
    content1Ancestors.AppendElement(c1);
  }
  if (!c1 && aCommonAncestor) {
    // So, it turns out aCommonAncestor was not an ancestor of c1. Oops.
    // Never mind. We can continue as if aCommonAncestor was null.
    aCommonAncestor = nsnull;
  }

  nsAutoVoidArray content2Ancestors;
  nsINode* c2;
  for (c2 = aContent2; c2 && c2 != aCommonAncestor; c2 = c2->GetNodeParent()) {
    content2Ancestors.AppendElement(c2);
  }
  if (!c2 && aCommonAncestor) {
    // So, it turns out aCommonAncestor was not an ancestor of c2.
    // We need to retry with no common ancestor hint.
    return DoCompareTreePosition(aContent1, aContent2,
                                 aIf1Ancestor, aIf2Ancestor, nsnull);
  }
  
  int last1 = content1Ancestors.Count() - 1;
  int last2 = content2Ancestors.Count() - 1;
  nsINode* content1Ancestor = nsnull;
  nsINode* content2Ancestor = nsnull;
  while (last1 >= 0 && last2 >= 0
         && ((content1Ancestor = NS_STATIC_CAST(nsINode*, content1Ancestors.ElementAt(last1)))
             == (content2Ancestor = NS_STATIC_CAST(nsINode*, content2Ancestors.ElementAt(last2))))) {
    last1--;
    last2--;
  }

  if (last1 < 0) {
    if (last2 < 0) {
      NS_ASSERTION(aContent1 == aContent2, "internal error?");
      return 0;
    }
    // aContent1 is an ancestor of aContent2
    return aIf1Ancestor;
  }

  if (last2 < 0) {
    // aContent2 is an ancestor of aContent1
    return aIf2Ancestor;
  }

  // content1Ancestor != content2Ancestor, so they must be siblings with the same parent
  nsINode* parent = content1Ancestor->GetNodeParent();
  NS_ASSERTION(parent, "no common ancestor at all???");
  if (!parent) { // different documents??
    return 0;
  }

  PRInt32 index1 = parent->IndexOf(content1Ancestor);
  PRInt32 index2 = parent->IndexOf(content2Ancestor);
  if (index1 < 0 || index2 < 0) {
    // one of them must be anonymous; we can't determine the order
    return 0;
  }

  return index1 - index2;
}

// static
nsIFrame* nsLayoutUtils::GetLastSibling(nsIFrame* aFrame) {
  if (!aFrame) {
    return nsnull;
  }

  nsIFrame* next;
  while ((next = aFrame->GetNextSibling()) != nsnull) {
    aFrame = next;
  }
  return aFrame;
}

// static
nsIView*
nsLayoutUtils::FindSiblingViewFor(nsIView* aParentView, nsIFrame* aFrame) {
  nsIFrame* parentViewFrame = NS_STATIC_CAST(nsIFrame*, aParentView->GetClientData());
  nsIContent* parentViewContent = parentViewFrame ? parentViewFrame->GetContent() : nsnull;
  for (nsIView* insertBefore = aParentView->GetFirstChild(); insertBefore;
       insertBefore = insertBefore->GetNextSibling()) {
    nsIFrame* f = NS_STATIC_CAST(nsIFrame*, insertBefore->GetClientData());
    if (!f) {
      // this view could be some anonymous view attached to a meaningful parent
      for (nsIView* searchView = insertBefore->GetParent(); searchView;
           searchView = searchView->GetParent()) {
        f = NS_STATIC_CAST(nsIFrame*, searchView->GetClientData());
        if (f) {
          break;
        }
      }
      NS_ASSERTION(f, "Can't find a frame anywhere!");
    }
    if (!f || !aFrame->GetContent() || !f->GetContent() ||
        CompareTreePosition(aFrame->GetContent(), f->GetContent(), parentViewContent) > 0) {
      // aFrame's content is after f's content (or we just don't know),
      // so put our view before f's view
      return insertBefore;
    }
  }
  return nsnull;
}

//static
nsIScrollableFrame*
nsLayoutUtils::GetScrollableFrameFor(nsIFrame *aScrolledFrame)
{
  nsIFrame *frame = aScrolledFrame->GetParent();
  if (!frame) {
    return nsnull;
  }
  nsIScrollableFrame *sf;
  CallQueryInterface(frame, &sf);
  return sf;
}

//static
nsIScrollableFrame*
nsLayoutUtils::GetScrollableFrameFor(nsIScrollableView *aScrollableView)
{
  nsIFrame *frame = GetFrameFor(aScrollableView->View()->GetParent());
  if (frame) {
    nsIScrollableFrame *sf;
    CallQueryInterface(frame, &sf);
    return sf;
  }
  return nsnull;
}

//static
nsPresContext::ScrollbarStyles
nsLayoutUtils::ScrollbarStylesOfView(nsIScrollableView *aScrollableView)
{
  nsIScrollableFrame *sf = GetScrollableFrameFor(aScrollableView);
  return sf ? sf->GetScrollbarStyles() :
              nsPresContext::ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN,
                                             NS_STYLE_OVERFLOW_HIDDEN);
}

// static
nsIScrollableView*
nsLayoutUtils::GetNearestScrollingView(nsIView* aView, Direction aDirection)
{
  // If aDirection is eEither, find first view with a scrolllable frame.
  // Otherwise, find the first view that has a scrollable frame whose
  // ScrollbarStyles is not NS_STYLE_OVERFLOW_HIDDEN in aDirection
  // and where there is something currently not visible
  // that can be scrolled to in aDirection.
  NS_ASSERTION(aView, "GetNearestScrollingView expects a non-null view");
  nsIScrollableView* scrollableView = nsnull;
  for (; aView; aView = aView->GetParent()) {
    scrollableView = aView->ToScrollableView();
    if (scrollableView) {
      nsPresContext::ScrollbarStyles ss =
        nsLayoutUtils::ScrollbarStylesOfView(scrollableView);
      nsIScrollableFrame *scrollableFrame = GetScrollableFrameFor(scrollableView);
      NS_ASSERTION(scrollableFrame, "Must have scrollable frame for view!");
      nsMargin margin = scrollableFrame->GetActualScrollbarSizes();
      // Get size of total scrollable area
      nscoord totalWidth, totalHeight;
      scrollableView->GetContainerSize(&totalWidth, &totalHeight);
      // Get size of currently visible area
      nsSize visibleSize = aView->GetBounds().Size();
      // aDirection can be eHorizontal, eVertical, or eEither
      // If scrolling in a specific direction, require visible scrollbars or
      // something to scroll to in that direction.
      if (aDirection != eHorizontal &&
          ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN &&
          (aDirection == eEither || totalHeight > visibleSize.height || margin.LeftRight()))
        break;
      if (aDirection != eVertical &&
          ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN &&
          (aDirection == eEither || totalWidth > visibleSize.width || margin.TopBottom()))
        break;
    }
  }
  return scrollableView;
}

nsPoint
nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(nsIDOMEvent* aDOMEvent, nsIFrame* aFrame)
{
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aDOMEvent));
  NS_ASSERTION(privateEvent, "bad implementation");
  if (!privateEvent)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  nsEvent* event;
  nsresult rv = privateEvent->GetInternalNSEvent(&event);
  if (NS_FAILED(rv))
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  return GetEventCoordinatesRelativeTo(event, aFrame);
}

nsPoint
nsLayoutUtils::GetEventCoordinatesRelativeTo(nsEvent* aEvent, nsIFrame* aFrame)
{
  if (!aEvent || (aEvent->eventStructType != NS_MOUSE_EVENT && 
                  aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT))
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  nsGUIEvent* GUIEvent = NS_STATIC_CAST(nsGUIEvent*, aEvent);
  if (!GUIEvent->widget)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  // If it is, or is a descendant of, an SVG foreignobject frame,
  // then we need to do extra work
  nsIFrame* rootFrame = aFrame;
  for (nsIFrame* f = aFrame; f; f = GetCrossDocParentFrame(f)) {
#ifdef MOZ_SVG_FOREIGNOBJECT
    if (f->IsFrameOfType(nsIFrame::eSVGForeignObject) && f->GetFirstChild(nsnull)) {
      nsSVGForeignObjectFrame* fo = NS_STATIC_CAST(nsSVGForeignObjectFrame*, f);
      nsIFrame* outer = nsSVGUtils::GetOuterSVGFrame(fo);
      return fo->TransformPointFromOuter(
          GetEventCoordinatesRelativeTo(aEvent, outer)) -
        aFrame->GetOffsetTo(fo->GetFirstChild(nsnull));
    }
#endif
    rootFrame = f;
  }

  nsIView* rootView = rootFrame->GetView();
  if (!rootView)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  nsPoint widgetToView = TranslateWidgetToView(rootFrame->GetPresContext(),
                               GUIEvent->widget, GUIEvent->refPoint,
                               rootView);

  if (widgetToView == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE))
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  return widgetToView - aFrame->GetOffsetTo(rootFrame);
}

nsPoint
nsLayoutUtils::GetEventCoordinatesForNearestView(nsEvent* aEvent,
                                                 nsIFrame* aFrame,
                                                 nsIView** aView)
{
  if (!aEvent || (aEvent->eventStructType != NS_MOUSE_EVENT && 
                  aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT))
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  nsGUIEvent* GUIEvent = NS_STATIC_CAST(nsGUIEvent*, aEvent);
  if (!GUIEvent->widget)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  nsPoint viewToFrame;
  nsIView* frameView;
  aFrame->GetOffsetFromView(viewToFrame, &frameView);
  if (aView)
    *aView = frameView;

  return TranslateWidgetToView(aFrame->GetPresContext(), GUIEvent->widget,
                               GUIEvent->refPoint, frameView);
}

nsPoint
nsLayoutUtils::TranslateWidgetToView(nsPresContext* aPresContext, 
                                     nsIWidget* aWidget, nsIntPoint aPt,
                                     nsIView* aView)
{
  nsIView* baseView = nsIView::GetViewFor(aWidget);
  if (!baseView)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  nsPoint viewToWidget;
  nsIWidget* wid = baseView->GetNearestWidget(&viewToWidget);
  NS_ASSERTION(aWidget == wid, "Clashing widgets");
  float pixelsToTwips = aPresContext->PixelsToTwips();
  nsPoint refPointTwips(NSIntPixelsToTwips(aPt.x, pixelsToTwips),
                        NSIntPixelsToTwips(aPt.y, pixelsToTwips));
  return refPointTwips - viewToWidget - aView->GetOffsetTo(baseView);
}

// Combine aNewBreakType with aOrigBreakType, but limit the break types
// to NS_STYLE_CLEAR_LEFT, RIGHT, LEFT_AND_RIGHT.
PRUint8
nsLayoutUtils::CombineBreakType(PRUint8 aOrigBreakType,
                                PRUint8 aNewBreakType)
{
  PRUint8 breakType = aOrigBreakType;
  switch(breakType) {
  case NS_STYLE_CLEAR_LEFT:
    if ((NS_STYLE_CLEAR_RIGHT          == aNewBreakType) ||
        (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aNewBreakType)) {
      breakType = NS_STYLE_CLEAR_LEFT_AND_RIGHT;
    }
    break;
  case NS_STYLE_CLEAR_RIGHT:
    if ((NS_STYLE_CLEAR_LEFT           == aNewBreakType) ||
        (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aNewBreakType)) {
      breakType = NS_STYLE_CLEAR_LEFT_AND_RIGHT;
    }
    break;
  case NS_STYLE_CLEAR_NONE:
    if ((NS_STYLE_CLEAR_LEFT           == aNewBreakType) ||
        (NS_STYLE_CLEAR_RIGHT          == aNewBreakType) ||
        (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aNewBreakType)) {
      breakType = aNewBreakType;
    }
  }
  return breakType;
}

PRBool
nsLayoutUtils::IsInitialContainingBlock(nsIFrame* aFrame)
{
  return aFrame ==
    aFrame->GetPresContext()->PresShell()->FrameConstructor()->GetInitialContainingBlock();
}

#ifdef DEBUG
#include <stdio.h>

static PRBool gDumpPaintList = PR_FALSE;
static PRBool gDumpEventList = PR_FALSE;
static PRBool gDumpRepaintRegionForCopy = PR_FALSE;
#endif

nsIFrame*
nsLayoutUtils::GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt)
{
  nsDisplayListBuilder builder(aFrame, PR_TRUE, PR_FALSE);
  nsDisplayList list;
  nsRect target(aPt, nsSize(1, 1));

  builder.EnterPresShell(aFrame, target);

  nsresult rv =
    aFrame->BuildDisplayListForStackingContext(&builder, target, &list);

  builder.LeavePresShell(aFrame, target);
  NS_ENSURE_SUCCESS(rv, nsnull);

#ifdef DEBUG
  if (gDumpEventList) {
    fprintf(stderr, "Event handling --- (%d,%d):\n", aPt.x, aPt.y);
    nsIFrameDebug::PrintDisplayList(&builder, list);
  }
#endif
  
  nsIFrame* result = list.HitTest(&builder, aPt);
  list.DeleteAll();
  return result;
}

/**
 * A simple display item that just renders a solid color across the entire
 * visible area.
 */
class nsDisplaySolidColor : public nsDisplayItem {
public:
  nsDisplaySolidColor(nsIFrame* aFrame, nscolor aColor)
    : nsDisplayItem(aFrame), mColor(aColor) {
    MOZ_COUNT_CTOR(nsDisplaySolidColor);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySolidColor() {
    MOZ_COUNT_DTOR(nsDisplaySolidColor);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("SolidColor")
private:
  nscolor   mColor;
};

void nsDisplaySolidColor::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  aCtx->SetColor(mColor);
  aCtx->FillRect(aDirtyRect);
}

nsresult
nsLayoutUtils::PaintFrame(nsIRenderingContext* aRenderingContext, nsIFrame* aFrame,
                          const nsRegion& aDirtyRegion, nscolor aBackground)
{
  nsDisplayListBuilder builder(aFrame, PR_FALSE, PR_TRUE);
  nsDisplayList list;
  nsRect dirtyRect = aDirtyRegion.GetBounds();

  builder.EnterPresShell(aFrame, dirtyRect);

  nsresult rv =
    aFrame->BuildDisplayListForStackingContext(&builder, dirtyRect, &list);

  builder.LeavePresShell(aFrame, dirtyRect);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_GET_A(aBackground) > 0) {
    // Fill the visible area with a background color. In the common case,
    // the visible area is entirely covered by the background of the root
    // document (at least!) so this will be removed by the optimizer. In some
    // cases we might not have a root frame, so this will prevent garbage
    // from being drawn.
    rv = list.AppendNewToBottom(new (&builder) nsDisplaySolidColor(aFrame, aBackground));
    NS_ENSURE_SUCCESS(rv, rv);
  }

#ifdef DEBUG
  if (gDumpPaintList) {
    fprintf(stderr, "Painting --- before optimization (dirty %d,%d,%d,%d):\n",
            dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
    nsIFrameDebug::PrintDisplayList(&builder, list);
  }
#endif
  
  nsRegion visibleRegion = aDirtyRegion;
  list.OptimizeVisibility(&builder, &visibleRegion);

#ifdef DEBUG
  if (gDumpPaintList) {
    fprintf(stderr, "Painting --- after optimization:\n");
    nsIFrameDebug::PrintDisplayList(&builder, list);
  }
#endif
  
  list.Paint(&builder, aRenderingContext, aDirtyRegion.GetBounds());
  // Flush the list so we don't trigger the IsEmpty-on-destruction assertion
  list.DeleteAll();
  return NS_OK;
}

static void
AccumulateItemInRegion(nsRegion* aRegion, const nsRect& aAreaRect,
                       const nsRect& aItemRect, nsDisplayItem* aItem)
{
  nsRect damageRect;
  if (damageRect.IntersectRect(aAreaRect, aItemRect)) {
#ifdef DEBUG
    if (gDumpRepaintRegionForCopy) {
      fprintf(stderr, "Adding rect %d,%d,%d,%d for frame %p\n",
              damageRect.x, damageRect.y, damageRect.width, damageRect.height,
              (void*)aItem->GetUnderlyingFrame());
    }
#endif
    aRegion->Or(*aRegion, damageRect);
  }
}

static void
AddItemsToRegion(nsDisplayListBuilder* aBuilder, nsDisplayList* aList,
                 const nsRect& aRect, const nsRect& aClipRect, nsPoint aDelta,
                 nsRegion* aRegion)
{
  for (nsDisplayItem* item = aList->GetBottom(); item; item = item->GetAbove()) {
    nsDisplayList* sublist = item->GetList();
    if (sublist) {
      if (item->GetType() == nsDisplayItem::TYPE_CLIP) {
        nsRect clip;
        clip.IntersectRect(aClipRect, NS_STATIC_CAST(nsDisplayClip*, item)->GetClipRect());
        AddItemsToRegion(aBuilder, sublist, aRect, clip, aDelta, aRegion);
      } else {
        // opacity, or a generic sublist
        AddItemsToRegion(aBuilder, sublist, aRect, aClipRect, aDelta, aRegion);
      }
    } else {
      // Items left in the list are either IsVaryingRelativeToFrame
      // or !IsMovingFrame (i.e., not in the moving subtree)
      nsRect r;
      if (r.IntersectRect(aClipRect, item->GetBounds(aBuilder))) {
        PRBool inMovingSubtree = PR_FALSE;
        if (item->IsVaryingRelativeToFrame(aBuilder, aBuilder->GetRootMovingFrame())) {
          nsIFrame* f = item->GetUnderlyingFrame();
          NS_ASSERTION(f, "Must have an underlying frame for leaf item");
          inMovingSubtree = aBuilder->IsMovingFrame(f);
          AccumulateItemInRegion(aRegion, aRect + aDelta, r, item);
        }
      
        if (!inMovingSubtree) {
          // if it's uniform and it includes both the old and new areas, then
          // we don't need to paint it
          PRBool skip = r.Contains(aRect) && r.Contains(aRect + aDelta) &&
              item->IsUniform(aBuilder);
          if (!skip) {
            // area where a non-moving element is visible must be repainted
            AccumulateItemInRegion(aRegion, aRect + aDelta, r, item);
            // we may have bitblitted an area that was painted by a non-moving
            // element. This bitblitted data is invalid and was copied to
            // "r + aDelta".
            AccumulateItemInRegion(aRegion, aRect + aDelta, r + aDelta, item);
          }
        }
      }
    }
  }
}

nsresult
nsLayoutUtils::ComputeRepaintRegionForCopy(nsIFrame* aRootFrame,
                                           nsIFrame* aMovingFrame,
                                           nsPoint aDelta,
                                           const nsRect& aCopyRect,
                                           nsRegion* aRepaintRegion)
{
  // Build the 'after' display list over the whole area of interest.
  // Frames under aMovingFrame will not be allowed to affect (clip or cover)
  // non-moving frame display items ... then we can be sure the non-moving
  // frame display items we get are the same ones we would have gotten if
  // we had constructed the 'before' display list.
  // (We have to build the 'after' display list because the frame/view
  // hierarchy has already been updated for the move.)
  nsRect rect;
  rect.UnionRect(aCopyRect, aCopyRect + aDelta);
  nsDisplayListBuilder builder(aRootFrame, PR_FALSE, PR_TRUE, aMovingFrame);
  nsDisplayList list;

  builder.EnterPresShell(aRootFrame, rect);

  nsresult rv =
    aRootFrame->BuildDisplayListForStackingContext(&builder, rect, &list);

  builder.LeavePresShell(aRootFrame, rect);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  if (gDumpRepaintRegionForCopy) {
    fprintf(stderr,
            "Repaint region for copy --- before optimization (area %d,%d,%d,%d, frame %p):\n",
            rect.x, rect.y, rect.width, rect.height, (void*)aMovingFrame);
    nsIFrameDebug::PrintDisplayList(&builder, list);
  }
#endif

  // Optimize for visibility, but frames under aMovingFrame will not be
  // considered opaque, so they don't cover non-moving frames.
  nsRegion visibleRegion(aCopyRect);
  visibleRegion.Or(visibleRegion, aCopyRect + aDelta);
  list.OptimizeVisibility(&builder, &visibleRegion);

#ifdef DEBUG
  if (gDumpRepaintRegionForCopy) {
    fprintf(stderr, "Repaint region for copy --- after optimization:\n");
    nsIFrameDebug::PrintDisplayList(&builder, list);
  }
#endif

  aRepaintRegion->SetEmpty();
  // Any visible non-moving display items get added to the repaint region
  // a) at their current location and b) offset by -aPt (their position in
  // the 'before' display list) (unless they're uniform and we can exclude them).
  // Also, any visible position-varying display items get added to the
  // repaint region. All these areas are confined to aCopyRect+aDelta.
  // We could do more work here: e.g., do another optimize-visibility pass
  // with the moving items taken into account, either on the before-list
  // or the after-list, or even both if we cloned the display lists ... but
  // it's probably not worth it.
  AddItemsToRegion(&builder, &list, aCopyRect, rect, aDelta, aRepaintRegion);
  // Flush the list so we don't trigger the IsEmpty-on-destruction assertion
  list.DeleteAll();
  return NS_OK;
}

nsresult
nsLayoutUtils::CreateOffscreenContext(nsIDeviceContext* deviceContext, nsIDrawingSurface* surface,
                                      const nsRect& aRect, nsIRenderingContext** aResult)
{
  nsresult            rv;
  nsIRenderingContext *context = nsnull;

  rv = deviceContext->CreateRenderingContext(surface, context);
  NS_ENSURE_SUCCESS(rv, rv);

  // always initialize clipping, linux won't draw images otherwise.
  nsRect clip(0, 0, aRect.width, aRect.height);
  context->SetClipRect(clip, nsClipCombine_kReplace);

  context->Translate(-aRect.x, -aRect.y);
  
  *aResult = context;
  return NS_OK;
}

PRInt32
nsLayoutUtils::GetZIndex(nsIFrame* aFrame) {
  if (!aFrame->GetStyleDisplay()->IsPositioned())
    return 0;

  const nsStylePosition* position =
    aFrame->GetStylePosition();
  if (position->mZIndex.GetUnit() == eStyleUnit_Integer)
    return position->mZIndex.GetIntValue();

  // sort the auto and 0 elements together
  return 0;
}

/**
 * Uses a binary search for find where the cursor falls in the line of text
 * It also keeps track of the part of the string that has already been measured
 * so it doesn't have to keep measuring the same text over and over
 *
 * @param "aBaseWidth" contains the width in twips of the portion 
 * of the text that has already been measured, and aBaseInx contains
 * the index of the text that has already been measured.
 *
 * @param aTextWidth returns the (in twips) the length of the text that falls
 * before the cursor aIndex contains the index of the text where the cursor falls
 */
PRBool
nsLayoutUtils::BinarySearchForPosition(nsIRenderingContext* aRendContext, 
                        const PRUnichar* aText,
                        PRInt32    aBaseWidth,
                        PRInt32    aBaseInx,
                        PRInt32    aStartInx, 
                        PRInt32    aEndInx, 
                        PRInt32    aCursorPos, 
                        PRInt32&   aIndex,
                        PRInt32&   aTextWidth)
{
  PRInt32 range = aEndInx - aStartInx;
  if ((range == 1) || (range == 2 && IS_HIGH_SURROGATE(aText[aStartInx]))) {
    aIndex   = aStartInx + aBaseInx;
    aRendContext->GetWidth(aText, aIndex, aTextWidth);
    return PR_TRUE;
  }

  PRInt32 inx = aStartInx + (range / 2);

  // Make sure we don't leave a dangling low surrogate
  if (IS_HIGH_SURROGATE(aText[inx-1]))
    inx++;

  PRInt32 textWidth = 0;
  aRendContext->GetWidth(aText, inx, textWidth);

  PRInt32 fullWidth = aBaseWidth + textWidth;
  if (fullWidth == aCursorPos) {
    aTextWidth = textWidth;
    aIndex = inx;
    return PR_TRUE;
  } else if (aCursorPos < fullWidth) {
    aTextWidth = aBaseWidth;
    if (BinarySearchForPosition(aRendContext, aText, aBaseWidth, aBaseInx, aStartInx, inx, aCursorPos, aIndex, aTextWidth)) {
      return PR_TRUE;
    }
  } else {
    aTextWidth = fullWidth;
    if (BinarySearchForPosition(aRendContext, aText, aBaseWidth, aBaseInx, inx, aEndInx, aCursorPos, aIndex, aTextWidth)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void
nsLayoutUtils::ScrollIntoView(nsIFormControlFrame* aFormFrame)
{
  NS_ASSERTION(aFormFrame, "Null frame passed into ScrollIntoView");
  nsIFrame* frame = nsnull;
  CallQueryInterface(aFormFrame, &frame);
  NS_ASSERTION(frame, "Form frame did not implement nsIFrame.");
  if (frame) {
    nsIPresShell* presShell = frame->GetPresContext()->GetPresShell();
    if (presShell) {
      presShell->ScrollFrameIntoView(frame,
                                     NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                     NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
  }
}

static PRInt32 FindSafeLength(nsIRenderingContext* aContext,
                              const PRUnichar *aString, PRUint32 aLength)
{
  if (aLength <= MAX_GFX_TEXT_BUF_SIZE)
    return aLength;
  
  PRUint8 buffer[MAX_GFX_TEXT_BUF_SIZE + 1];
  // Fill in the cluster hint information, if it's available.
  PRUint32 clusterHint;
  aContext->GetHints(clusterHint);
  clusterHint &= NS_RENDERING_HINT_TEXT_CLUSTERS;

  PRInt32 len = MAX_GFX_TEXT_BUF_SIZE;

  if (clusterHint) {
    nsresult rv =
      aContext->GetClusterInfo(aString, MAX_GFX_TEXT_BUF_SIZE + 1, buffer);
    if (NS_FAILED(rv))
      return len;
  }

  // Ensure that we don't break inside a cluster or inside a surrogate pair
  while (len > 0 &&
         (IS_LOW_SURROGATE(aString[len]) || (clusterHint && !buffer[len]))) {
    len--;
  }
  if (len == 0) {
    // We don't want our caller to go into an infinite loop, so don't return
    // zero. It's hard to imagine how we could actually get here unless there
    // are languages that allow clusters of arbitrary size. If there are and
    // someone feeds us a 500+ character cluster, too bad.
    return MAX_GFX_TEXT_BUF_SIZE;
  }
  return len;
}

nsresult
nsLayoutUtils::SafeGetWidth(nsIRenderingContext* aContext,
                            const char *aString, PRUint32 aLength, nscoord& aWidth)
{
  // Since it's ASCII, we don't need to worry about clusters or RTL
  aWidth = 0;
  while (aLength > 0) {
    PRInt32 len = PR_MIN(aLength, MAX_GFX_TEXT_BUF_SIZE);
    nscoord width;
    nsresult rv = aContext->GetWidth(aString, len, width);
    if (NS_FAILED(rv))
      return rv;
    aWidth += width;
    aLength -= len;
    aString += len;
  }
  return NS_OK;
}

nsresult
nsLayoutUtils::SafeGetWidth(nsIRenderingContext* aContext,
                            const PRUnichar *aString, PRUint32 aLength, nscoord& aWidth)
{
  aWidth = 0;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(aContext, aString, aLength);
    nscoord width;
    nsresult rv = aContext->GetWidth(aString, len, width);
    if (NS_FAILED(rv))
      return rv;
    aWidth += width;
    aLength -= len;
    aString += len;
  }
  return NS_OK;
}

void
nsLayoutUtils::SafeDrawString(nsIRenderingContext* aContext,
                              const char *aString, PRUint32 aLength,
                              nscoord aX, nscoord aY)
{
  // Since it's ASCII, we don't need to worry about clusters or RTL
  while (aLength > 0) {
    PRInt32 len = PR_MIN(aLength, MAX_GFX_TEXT_BUF_SIZE);
    aContext->DrawString(aString, len, aX, aY);
    aLength -= len;

    if (aLength > 0) {
      nscoord width;
      aContext->GetWidth(aString, len, width);
      aX += width;
      aString += len;
    }
  }
}

void
nsLayoutUtils::SafeDrawString(nsIRenderingContext* aContext,
                              const PRUnichar *aString, PRUint32 aLength,
                              nscoord aX, nscoord aY,
                              PRInt32 aFontID,
                              const nscoord* aSpacing)
{
  if (aLength <= MAX_GFX_TEXT_BUF_SIZE) {
    aContext->DrawString(aString, aLength, aX, aY, aFontID, aSpacing);
    return;
  }

  PRBool isRTL = PR_FALSE;
  aContext->GetRightToLeftText(&isRTL);

  if (isRTL) {
    nscoord totalWidth = 0;
    if (aSpacing) {
      for (PRUint32 i = 0; i < aLength; ++i) {
        totalWidth += aSpacing[i];
      }
    } else {
      SafeGetWidth(aContext, aString, aLength, totalWidth);
    }
    aX += totalWidth;
  }
  
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(aContext, aString, aLength);
    nscoord width = 0;
    if (aSpacing) {
      for (PRInt32 i = 0; i < len; ++i) {
        width += aSpacing[i];
      }
    } else {
      aContext->GetWidth(aString, len, width);
    }

    if (isRTL) {
      aX -= width;
    }
    aContext->DrawString(aString, len, aX, aY, aFontID, aSpacing);
    aLength -= len;
    if (!isRTL) {
      aX += width;
    }
    aString += len;
    if (aSpacing) {
      aSpacing += len;
    }
  }
}

void
nsLayoutUtils::SafeGetTextDimensions(nsIRenderingContext* aContext,
                                     const char* aString, PRUint32 aLength,
                                     nsTextDimensions& aDimensions)
{
  if (aLength <= MAX_GFX_TEXT_BUF_SIZE) {
    aContext->GetTextDimensions(aString, aLength, aDimensions);
    return;
  }
 
  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = PR_MIN(aLength, MAX_GFX_TEXT_BUF_SIZE);
    nsTextDimensions dimensions;
    nsresult rv = aContext->GetTextDimensions(aString, len, dimensions);
    if (NS_FAILED(rv))
      return;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsTextDimensions, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned.
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(dimensions);
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }
}

void
nsLayoutUtils::SafeGetTextDimensions(nsIRenderingContext* aContext,
                                     const PRUnichar* aString, PRUint32 aLength,
                                     nsTextDimensions& aDimensions)
{
  if (aLength <= MAX_GFX_TEXT_BUF_SIZE) {
    aContext->GetTextDimensions(aString, aLength, aDimensions);
    return;
  }
  
  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(aContext, aString, aLength);
    nsTextDimensions dimensions;
    nsresult rv = aContext->GetTextDimensions(aString, len, dimensions);
    if (NS_FAILED(rv))
      return;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsTextDimensions, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned.
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(dimensions);
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }
}

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11) || defined(XP_BEOS)
void
nsLayoutUtils::SafeGetTextDimensions(nsIRenderingContext* aContext,
                                     const char*       aString,
                                     PRInt32           aLength,
                                     PRInt32           aAvailWidth,
                                     PRInt32*          aBreaks,
                                     PRInt32           aNumBreaks,
                                     nsTextDimensions& aDimensions,
                                     PRInt32&          aNumCharsFit,
                                     nsTextDimensions& aLastWordDimensions)
{
  if (aLength <= MAX_GFX_TEXT_BUF_SIZE) {
    aContext->GetTextDimensions(aString, aLength, aAvailWidth, aBreaks, aNumBreaks,
                                aDimensions, aNumCharsFit, aLastWordDimensions);
    return;
  }

  // Do a naive implementation based on 3-arg GetTextDimensions
  PRInt32 x = 0;
  PRInt32 wordCount;
  for (wordCount = 0; wordCount < aNumBreaks; ++wordCount) {
    PRInt32 lastBreak = wordCount > 0 ? aBreaks[wordCount - 1] : 0;
    nsTextDimensions dimensions;
    SafeGetTextDimensions(aContext, aString + lastBreak, aBreaks[wordCount],
                          dimensions);
    x += dimensions.width;
    // The first word always "fits"
    if (x > aAvailWidth && wordCount > 0)
      break;
    // aDimensions ascent/descent should exclude the last word (unless there
    // is only one word) so we let it run one word behind
    if (wordCount == 0) {
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(aLastWordDimensions);
    }
    aNumCharsFit = aBreaks[wordCount];
    aLastWordDimensions = dimensions;
  }
  // aDimensions width should include all the text
  aDimensions.width = x;
}

void
nsLayoutUtils::SafeGetTextDimensions(nsIRenderingContext* aContext,
                                     const PRUnichar*  aString,
                                     PRInt32           aLength,
                                     PRInt32           aAvailWidth,
                                     PRInt32*          aBreaks,
                                     PRInt32           aNumBreaks,
                                     nsTextDimensions& aDimensions,
                                     PRInt32&          aNumCharsFit,
                                     nsTextDimensions& aLastWordDimensions)
{
  if (aLength <= MAX_GFX_TEXT_BUF_SIZE) {
    aContext->GetTextDimensions(aString, aLength, aAvailWidth, aBreaks, aNumBreaks,
                                aDimensions, aNumCharsFit, aLastWordDimensions);
    return;
  }

  // Do a naive implementation based on 3-arg GetTextDimensions
  PRInt32 x = 0;
  PRInt32 wordCount;
  for (wordCount = 0; wordCount < aNumBreaks; ++wordCount) {
    PRInt32 lastBreak = wordCount > 0 ? aBreaks[wordCount - 1] : 0;
    nsTextDimensions dimensions;
    SafeGetTextDimensions(aContext, aString + lastBreak, aBreaks[wordCount],
                          dimensions);
    x += dimensions.width;
    // The first word always "fits"
    if (x > aAvailWidth && wordCount > 0)
      break;
    // aDimensions ascent/descent should exclude the last word (unless there
    // is only one word) so we let it run one word behind
    if (wordCount == 0) {
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(aLastWordDimensions);
    }
    aNumCharsFit = aBreaks[wordCount];
    aLastWordDimensions = dimensions;
  }
  // aDimensions width should include all the text
  aDimensions.width = x;
}
#endif

#ifdef MOZ_MATHML
nsresult
nsLayoutUtils::SafeGetBoundingMetrics(nsIRenderingContext* aContext,
                                      const char*          aString,
                                      PRUint32             aLength,
                                      nsBoundingMetrics&   aBoundingMetrics)
{
  if (aLength <= MAX_GFX_TEXT_BUF_SIZE)
    return aContext->GetBoundingMetrics(aString, aLength, aBoundingMetrics);

  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = PR_MIN(MAX_GFX_TEXT_BUF_SIZE, aLength);
    nsBoundingMetrics metrics;
    nsresult rv = aContext->GetBoundingMetrics(aString, len, metrics);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsBoundingMetrics, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned and the left bearing is properly initialized.
      aBoundingMetrics = metrics;
    } else {
      aBoundingMetrics += metrics;
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }
  return NS_OK;
}

nsresult
nsLayoutUtils::SafeGetBoundingMetrics(nsIRenderingContext* aContext,
                                      const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics)
{
  if (aLength <= MAX_GFX_TEXT_BUF_SIZE)
    return aContext->GetBoundingMetrics(aString, aLength, aBoundingMetrics);

  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(aContext, aString, aLength);
    nsBoundingMetrics metrics;
    nsresult rv = aContext->GetBoundingMetrics(aString, len, metrics);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsBoundingMetrics, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned and the left bearing is properly initialized.
      aBoundingMetrics = metrics;
    } else {
      aBoundingMetrics += metrics;
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }
  return NS_OK;
}
#endif

nsRect
nsLayoutUtils::GetAllInFlowBoundingRect(nsIFrame* aFrame)
{
  // Get the union of all rectangles in this and continuation frames
  nsRect r = aFrame->GetRect();
  nsIFrame* parent = aFrame->GetParent();
  if (!parent)
    return r;

  for (nsIFrame* f = aFrame->GetNextContinuation(); f; f = f->GetNextContinuation()) {
    r.UnionRect(r, f->GetRect() + f->GetOffsetTo(parent));
  }

  if (r.IsEmpty()) {
    // It could happen that all the rects are empty (eg zero-width or
    // zero-height).  In that case, use the first rect for the frame.
    r = aFrame->GetRect();
  }

  return r - aFrame->GetPosition();
}
