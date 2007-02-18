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
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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
#include "nsIFontMetrics.h"
#include "nsIFormControlFrame.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsFrameList.h"
#include "nsGkAtoms.h"
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
#include "nsFrameManager.h"
#include "nsBlockFrame.h"
#include "nsBidiPresUtils.h"

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

  // Get the last continuation frame
  nsIFrame* lastContinuation = aFrame->GetLastContinuation();

  // Get the last child frame
  nsIFrame* firstChildFrame = lastContinuation->GetFirstChild(nsnull);
  if (firstChildFrame) {
    nsFrameList frameList(firstChildFrame);
    nsIFrame*   lastChildFrame = frameList.LastChild();

    NS_ASSERTION(lastChildFrame, "unexpected error");

    // Get the frame's first continuation. This matters in case the frame has
    // been continued across multiple lines or split by BiDi resolution.
    lastChildFrame = lastChildFrame->GetFirstContinuation();
    
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
  NS_ASSERTION(!aFrame->GetPrevContinuation(),
               "aFrame must be first continuation");
  
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
    if (frame->GetType() == nsGkAtoms::pageFrame) {
      return frame;
    }
  }
  return nsnull;
}

nsIFrame*
nsLayoutUtils::GetFloatFromPlaceholder(nsIFrame* aFrame) {
  if (nsGkAtoms::placeholderFrame != aFrame->GetType()) {
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

static nsIFrame* FillAncestors(nsIFrame* aFrame,
                               nsIFrame* aStopAtAncestor, nsFrameManager* aFrameManager,
                               nsTArray<nsIFrame*>* aAncestors)
{
  while (aFrame && aFrame != aStopAtAncestor) {
    aAncestors->AppendElement(aFrame);
    aFrame = nsLayoutUtils::GetParentOrPlaceholderFor(aFrameManager, aFrame);
  }
  return aFrame;
}

// Return true if aFrame1 is after aFrame2
static PRBool IsFrameAfter(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  nsIFrame* f = aFrame2;
  do {
    f = f->GetNextSibling();
    if (f == aFrame1)
      return PR_TRUE;
  } while (f);
  return PR_FALSE;
}

// static
PRInt32
nsLayoutUtils::DoCompareTreePosition(nsIFrame* aFrame1,
                                     nsIFrame* aFrame2,
                                     PRInt32 aIf1Ancestor,
                                     PRInt32 aIf2Ancestor,
                                     nsIFrame* aCommonAncestor)
{
  NS_PRECONDITION(aFrame1, "aFrame1 must not be null");
  NS_PRECONDITION(aFrame2, "aFrame2 must not be null");

  nsPresContext* presContext = aFrame1->GetPresContext();
  if (presContext != aFrame2->GetPresContext()) {
    NS_ERROR("no common ancestor at all, different documents");
    return 0;
  }
  nsFrameManager* frameManager = presContext->PresShell()->FrameManager();

  nsAutoTArray<nsIFrame*,20> frame1Ancestors;
  if (!FillAncestors(aFrame1, aCommonAncestor, frameManager, &frame1Ancestors)) {
    // We reached the root of the frame tree ... if aCommonAncestor was set,
    // it is wrong
    aCommonAncestor = nsnull;
  }

  nsAutoTArray<nsIFrame*,20> frame2Ancestors;
  if (!FillAncestors(aFrame2, aCommonAncestor, frameManager, &frame2Ancestors) &&
      aCommonAncestor) {
    // We reached the root of the frame tree ... aCommonAncestor was wrong.
    // Try again with no hint.
    return DoCompareTreePosition(aFrame1, aFrame2,
                                 aIf1Ancestor, aIf2Ancestor, nsnull);
  }

  PRInt32 last1 = PRInt32(frame1Ancestors.Length()) - 1;
  PRInt32 last2 = PRInt32(frame2Ancestors.Length()) - 1;
  while (last1 >= 0 && last2 >= 0 &&
         frame1Ancestors[last1] == frame2Ancestors[last2]) {
    last1--;
    last2--;
  }

  if (last1 < 0) {
    if (last2 < 0) {
      NS_ASSERTION(aFrame1 == aFrame2, "internal error?");
      return 0;
    }
    // aFrame1 is an ancestor of aFrame2
    return aIf1Ancestor;
  }

  if (last2 < 0) {
    // aFrame2 is an ancestor of aFrame1
    return aIf2Ancestor;
  }

  nsIFrame* ancestor1 = frame1Ancestors[last1];
  nsIFrame* ancestor2 = frame2Ancestors[last2];
  // Now we should be able to walk sibling chains to find which one is first
  if (IsFrameAfter(ancestor2, ancestor1))
    return -1;
  if (IsFrameAfter(ancestor1, ancestor2))
    return 1;
  NS_WARNING("Frames were in different child lists???");
  return 0;
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
  nsPoint refPointAppUnits(aPresContext->DevPixelsToAppUnits(aPt.x),
                           aPresContext->DevPixelsToAppUnits(aPt.y));
  return refPointAppUnits - viewToWidget - aView->GetOffsetTo(baseView);
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
  NS_ASSERTION(aRootFrame != aMovingFrame,
               "The root frame shouldn't be the one that's moving, that makes no sense");

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
  if ((range == 1) || (range == 2 && NS_IS_HIGH_SURROGATE(aText[aStartInx]))) {
    aIndex   = aStartInx + aBaseInx;
    aRendContext->GetWidth(aText, aIndex, aTextWidth);
    return PR_TRUE;
  }

  PRInt32 inx = aStartInx + (range / 2);

  // Make sure we don't leave a dangling low surrogate
  if (NS_IS_HIGH_SURROGATE(aText[inx-1]))
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

nsRect
nsLayoutUtils::GetAllInFlowBoundingRect(nsIFrame* aFrame)
{
  // Get the union of all rectangles in this and continuation frames
  nsRect r = aFrame->GetRect();
  nsIFrame* parent = aFrame->GetParent();
  if (!parent)
    return r;

  for (nsIFrame* f = aFrame->GetNextContinuation(); f; f = f->GetNextContinuation()) {
    r.UnionRect(r, nsRect(f->GetOffsetTo(parent), f->GetSize()));
  }

  if (r.IsEmpty()) {
    // It could happen that all the rects are empty (eg zero-width or
    // zero-height).  In that case, use the first rect for the frame.
    r = aFrame->GetRect();
  }

  return r - aFrame->GetPosition();
}

nsresult
nsLayoutUtils::GetFontMetricsForFrame(nsIFrame* aFrame,
                                      nsIFontMetrics** aFontMetrics)
{
  nsStyleContext* sc = aFrame->GetStyleContext();
  return aFrame->GetPresContext()->DeviceContext()->
    GetMetricsFor(sc->GetStyleFont()->mFont,
                  sc->GetStyleVisibility()->mLangGroup,
                  *aFontMetrics);
}

nsIFrame*
nsLayoutUtils::FindChildContainingDescendant(nsIFrame* aParent, nsIFrame* aDescendantFrame)
{
  nsIFrame* result = aDescendantFrame;

  while (result) {
    nsIFrame* parent = result->GetParent();
    if (parent == aParent) {
      break;
    }

    // The frame is not an immediate child of aParent so walk up another level
    result = parent;
  }

  return result;
}

nsBlockFrame*
nsLayoutUtils::FindNearestBlockAncestor(nsIFrame* aFrame)
{
  nsIFrame* nextAncestor;
  for (nextAncestor = aFrame->GetParent(); nextAncestor;
       nextAncestor = nextAncestor->GetParent()) {
    nsBlockFrame* block;
    if (NS_SUCCEEDED(nextAncestor->QueryInterface(kBlockFrameCID, (void**)&block)))
      return block;
  }
  return nsnull;
}

nsIFrame*
nsLayoutUtils::GetParentOrPlaceholderFor(nsFrameManager* aFrameManager,
                                         nsIFrame* aFrame)
{
  if (aFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
    return aFrameManager->GetPlaceholderFrameFor(aFrame);
  return aFrame->GetParent();
}

nsIFrame*
nsLayoutUtils::GetClosestCommonAncestorViaPlaceholders(nsIFrame* aFrame1,
                                                       nsIFrame* aFrame2,
                                                       nsIFrame* aKnownCommonAncestorHint)
{
  NS_PRECONDITION(aFrame1, "aFrame1 must not be null");
  NS_PRECONDITION(aFrame2, "aFrame2 must not be null");

  nsPresContext* presContext = aFrame1->GetPresContext();
  if (presContext != aFrame2->GetPresContext()) {
    // different documents, no common ancestor
    return nsnull;
  }
  nsFrameManager* frameManager = presContext->PresShell()->FrameManager();

  nsAutoVoidArray frame1Ancestors;
  nsIFrame* f1;
  for (f1 = aFrame1; f1 && f1 != aKnownCommonAncestorHint;
       f1 = GetParentOrPlaceholderFor(frameManager, f1)) {
    frame1Ancestors.AppendElement(f1);
  }
  if (!f1 && aKnownCommonAncestorHint) {
    // So, it turns out aKnownCommonAncestorHint was not an ancestor of f1. Oops.
    // Never mind. We can continue as if aKnownCommonAncestorHint was null.
    aKnownCommonAncestorHint = nsnull;
  }

  nsAutoVoidArray frame2Ancestors;
  nsIFrame* f2;
  for (f2 = aFrame2; f2 && f2 != aKnownCommonAncestorHint;
       f2 = GetParentOrPlaceholderFor(frameManager, f2)) {
    frame2Ancestors.AppendElement(f2);
  }
  if (!f2 && aKnownCommonAncestorHint) {
    // So, it turns out aKnownCommonAncestorHint was not an ancestor of f2.
    // We need to retry with no common ancestor hint.
    return GetClosestCommonAncestorViaPlaceholders(aFrame1, aFrame2, nsnull);
  }

  // now frame1Ancestors and frame2Ancestors give us the parent frame chain
  // up to aKnownCommonAncestorHint, or if that is null, up to and including
  // the root frame. We need to walk from the end (i.e., the top of the
  // frame (sub)tree) down to aFrame1/aFrame2 looking for the first difference.
  nsIFrame* lastCommonFrame = aKnownCommonAncestorHint;
  PRInt32 last1 = frame1Ancestors.Count() - 1;
  PRInt32 last2 = frame2Ancestors.Count() - 1;
  while (last1 >= 0 && last2 >= 0) {
    nsIFrame* frame1 = NS_STATIC_CAST(nsIFrame*, frame1Ancestors.ElementAt(last1));
    if (frame1 != frame2Ancestors.ElementAt(last2))
      break;
    lastCommonFrame = frame1;
    last1--;
    last2--;
  }
  return lastCommonFrame;
}

PRBool
nsLayoutUtils::IsViewportScrollbarFrame(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  nsIFrame* rootScrollFrame =
    aFrame->GetPresContext()->PresShell()->GetRootScrollFrame();
  if (!rootScrollFrame)
    return PR_FALSE;

  nsIScrollableFrame* rootScrollableFrame = nsnull;
  CallQueryInterface(rootScrollFrame, &rootScrollableFrame);
  NS_ASSERTION(rootScrollableFrame, "The root scorollable frame is null");

  if (!IsProperAncestorFrame(rootScrollFrame, aFrame))
    return PR_FALSE;

  nsIFrame* rootScrolledFrame = rootScrollableFrame->GetScrolledFrame();
  return !(rootScrolledFrame == aFrame ||
           IsProperAncestorFrame(rootScrolledFrame, aFrame));
}

static nscoord AddPercents(nsLayoutUtils::IntrinsicWidthType aType,
                           nscoord aCurrent, float aPercent)
{
  nscoord result = aCurrent;
  if (aPercent > 0.0f && aType == nsLayoutUtils::PREF_WIDTH) {
    // XXX Should we also consider percentages for min widths, up to a
    // limit?
    if (aPercent >= 1.0f)
      result = nscoord_MAX;
    else
      result = NSToCoordRound(float(result) / (1.0f - aPercent));
  }
  return result;
}

static PRBool GetAbsoluteCoord(const nsStyleCoord& aStyle,
                               nsIRenderingContext* aRenderingContext,
                               nsIFrame* aFrame,
                               nscoord& aResult)
{
  nsStyleUnit unit = aStyle.GetUnit();
  if (eStyleUnit_Coord == unit) {
    aResult = aStyle.GetCoordValue();
    return PR_TRUE;
  }
  if (eStyleUnit_Chars == unit) {
    SetFontFromStyle(aRenderingContext, aFrame->GetStyleContext());
    nscoord fontWidth;
    aRenderingContext->SetTextRunRTL(PR_FALSE);
    aRenderingContext->GetWidth('M', fontWidth);
    aResult = aStyle.GetIntValue() * fontWidth;
    return PR_TRUE;
  }
  return PR_FALSE;
}

#undef  DEBUG_INTRINSIC_WIDTH

#ifdef DEBUG_INTRINSIC_WIDTH
static PRInt32 gNoiseIndent = 0;
#endif

/* static */ nscoord
nsLayoutUtils::IntrinsicForContainer(nsIRenderingContext *aRenderingContext,
                                     nsIFrame *aFrame,
                                     IntrinsicWidthType aType)
{
  NS_PRECONDITION(aFrame, "null frame");
  NS_PRECONDITION(aType == MIN_WIDTH || aType == PREF_WIDTH, "bad type");

#ifdef DEBUG_INTRINSIC_WIDTH
  nsFrame::IndentBy(stdout, gNoiseIndent);
  NS_STATIC_CAST(nsFrame*, aFrame)->ListTag(stdout);
  printf(" %s intrinsic width for container:\n",
         aType == MIN_WIDTH ? "min" : "pref");
#endif

  nsIFrame::IntrinsicWidthOffsetData offsets =
    aFrame->IntrinsicWidthOffsets(aRenderingContext);

  const nsStylePosition *stylePos = aFrame->GetStylePosition();
  const PRUint8 boxSizing = stylePos->mBoxSizing;
  const nsStyleCoord &styleWidth = stylePos->mWidth;
  const nsStyleCoord &styleMinWidth = stylePos->mMinWidth;
  const nsStyleCoord &styleMaxWidth = stylePos->mMaxWidth;

  // We build up two values starting with the content box, and then
  // adding padding, border and margin.  The result is normally
  // |result|.  Then, when we handle 'width', 'min-width', and
  // 'max-width', we use the results we've been building in |min| as a
  // minimum, overriding 'min-width'.  This ensures two things:
  //   * that we don't let a value of 'box-sizing' specifying a width
  //     smaller than the padding/border inside the box-sizing box give
  //     a content width less than zero
  //   * that we prevent tables from becoming smaller than their
  //     intrinsic minimum width
  nscoord result = 0, min = 0;

  // If we have a specified width (or a specified 'min-width' greater
  // than the specified 'max-width', which works out to the same thing),
  // don't even bother getting the frame's intrinsic width.
  if (styleWidth.GetUnit() != eStyleUnit_Coord &&
      (styleMinWidth.GetUnit() != eStyleUnit_Coord ||
       styleMaxWidth.GetUnit() != eStyleUnit_Coord ||
       styleMaxWidth.GetCoordValue() > styleMinWidth.GetCoordValue())) {
#ifdef DEBUG_INTRINSIC_WIDTH
    ++gNoiseIndent;
#endif
    if (aType == MIN_WIDTH)
      result = aFrame->GetMinWidth(aRenderingContext);
    else
      result = aFrame->GetPrefWidth(aRenderingContext);
#ifdef DEBUG_INTRINSIC_WIDTH
    --gNoiseIndent;
    nsFrame::IndentBy(stdout, gNoiseIndent);
    NS_STATIC_CAST(nsFrame*, aFrame)->ListTag(stdout);
    printf(" %s intrinsic width from frame is %d.\n",
           aType == MIN_WIDTH ? "min" : "pref", result);
#endif
  }
      
  if (aFrame->GetType() == nsGkAtoms::tableFrame) {
    // Tables can't shrink smaller than their intrinsic minimum width,
    // no matter what.
    min = aFrame->GetMinWidth(aRenderingContext);
  }

  // We also need to track what has been added on outside of the box
  // (controlled by 'box-sizing') where 'width', 'min-width' and
  // 'max-width' are applied.  We have to account for these properties
  // after getting all the offsets (margin, border, padding) because
  // percentages do not operate linearly.
  // Doing this is ok because although percentages aren't handled
  // linearly, they are handled monotonically.
  nscoord coordOutsideWidth = offsets.hPadding;
  float pctOutsideWidth = offsets.hPctPadding;

  float pctTotal = 0.0f;

  if (boxSizing == NS_STYLE_BOX_SIZING_PADDING) {
    min += coordOutsideWidth;
    result += coordOutsideWidth;
    pctTotal += pctOutsideWidth;

    coordOutsideWidth = 0;
    pctOutsideWidth = 0.0f;
  }

  coordOutsideWidth += offsets.hBorder;

  if (boxSizing == NS_STYLE_BOX_SIZING_BORDER) {
    min += coordOutsideWidth;
    result += coordOutsideWidth;
    pctTotal += pctOutsideWidth;

    coordOutsideWidth = 0;
    pctOutsideWidth = 0.0f;
  }

  coordOutsideWidth += offsets.hMargin;
  pctOutsideWidth += offsets.hPctMargin;

  min += coordOutsideWidth;
  result += coordOutsideWidth;
  pctTotal += pctOutsideWidth;

  nscoord w;
  if (GetAbsoluteCoord(styleWidth, aRenderingContext, aFrame, w)) {
    result = AddPercents(aType, w + coordOutsideWidth, pctOutsideWidth);
  }
  else if (aType == MIN_WIDTH && eStyleUnit_Percent == styleWidth.GetUnit() &&
           aFrame->IsFrameOfType(nsIFrame::eReplaced)) {
    // A percentage width on replaced elements means they can shrink to 0.
    result = 0; // let |min| handle padding/border/margin
  }
  else {
    result = AddPercents(aType, result, pctTotal);
  }

  nscoord maxw;
  if (GetAbsoluteCoord(styleMaxWidth, aRenderingContext, aFrame, maxw)) {
    maxw = AddPercents(aType, maxw + coordOutsideWidth, pctOutsideWidth);
    if (result > maxw)
      result = maxw;
  }

  nscoord minw;
  if (GetAbsoluteCoord(styleMinWidth, aRenderingContext, aFrame, minw)) {
    minw = AddPercents(aType, minw + coordOutsideWidth, pctOutsideWidth);
    if (result < minw)
      result = minw;
  }

  min = AddPercents(aType, min, pctTotal);
  if (result < min)
    result = min;

  const nsStyleDisplay *disp = aFrame->GetStyleDisplay();
  if (aFrame->IsThemed(disp)) {
    nsSize size(0, 0);
    PRBool canOverride = PR_TRUE;
    nsPresContext *presContext = aFrame->GetPresContext();
    presContext->GetTheme()->
      GetMinimumWidgetSize(aRenderingContext, aFrame, disp->mAppearance,
                           &size, &canOverride);

    nscoord themeWidth = presContext->DevPixelsToAppUnits(size.width);

    // GMWS() returns a border-box width
    themeWidth += offsets.hMargin;
    themeWidth = AddPercents(aType, themeWidth, offsets.hPctMargin);

    if (themeWidth > result || !canOverride)
      result = themeWidth;
  }

#ifdef DEBUG_INTRINSIC_WIDTH
  nsFrame::IndentBy(stdout, gNoiseIndent);
  NS_STATIC_CAST(nsFrame*, aFrame)->ListTag(stdout);
  printf(" %s intrinsic width for container is %d twips.\n",
         aType == MIN_WIDTH ? "min" : "pref", result);
#endif

  return result;
}

/* static */ nscoord
nsLayoutUtils::ComputeWidthDependentValue(
                 nsIRenderingContext* aRenderingContext,
                 nsIFrame*            aFrame,
                 nscoord              aContainingBlockWidth,
                 const nsStyleCoord&  aCoord)
{
  NS_PRECONDITION(aFrame, "non-null frame expected");
  NS_PRECONDITION(aRenderingContext, "non-null rendering context expected");
  NS_PRECONDITION(aContainingBlockWidth != NS_UNCONSTRAINEDSIZE,
                  "unconstrained widths no longer supported");

  nscoord result;
  if (GetAbsoluteCoord(aCoord, aRenderingContext, aFrame, result)) {
    return result;
  }
  if (eStyleUnit_Percent == aCoord.GetUnit()) {
    return NSToCoordFloor(aContainingBlockWidth * aCoord.GetPercentValue());
  }
  return 0;
}

/* static */ nscoord
nsLayoutUtils::ComputeHeightDependentValue(
                 nsIRenderingContext* aRenderingContext,
                 nsIFrame*            aFrame,
                 nscoord              aContainingBlockHeight,
                 const nsStyleCoord&  aCoord)
{
  NS_PRECONDITION(aFrame, "non-null frame expected");
  NS_PRECONDITION(aRenderingContext, "non-null rendering context expected");

  nscoord result;
  if (GetAbsoluteCoord(aCoord, aRenderingContext, aFrame, result)) {
    return result;
  }
  if (eStyleUnit_Percent == aCoord.GetUnit()) {
    // XXXldb Some callers explicitly check aContainingBlockHeight
    // against NS_AUTOHEIGHT *and* unit against eStyleUnit_Percent
    // before calling this function, so this assertion probably needs to
    // be inside the percentage case.  However, it would be much more
    // likely to catch problems if it were at the start of the function.
    // XXXldb Many callers pass a non-'auto' containing block height when
    // according to CSS2.1 they should be passing 'auto'.
    NS_PRECONDITION(NS_AUTOHEIGHT != aContainingBlockHeight,
                    "unexpected 'containing block height'");

    if (NS_AUTOHEIGHT != aContainingBlockHeight) {
      return NSToCoordFloor(aContainingBlockHeight * aCoord.GetPercentValue());
    }
  }
  return 0;
}

inline PRBool
IsAutoHeight(const nsStyleCoord &aCoord, nscoord aCBHeight)
{
  nsStyleUnit unit = aCoord.GetUnit();
  return unit == eStyleUnit_Auto ||  // only for 'height'
         unit == eStyleUnit_Null ||  // only for 'max-height'
         (unit == eStyleUnit_Percent && 
          aCBHeight == NS_AUTOHEIGHT);
}


/* static */ nsSize
nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                   nsIRenderingContext* aRenderingContext,
                   nsIFrame* aFrame, nsSize aIntrinsicSize, nsSize aCBSize,
                   nsSize aBorder, nsSize aPadding)
{
  const nsStylePosition *stylePos = aFrame->GetStylePosition();
  // Handle intrinsic sizes and their interaction with
  // {min-,max-,}{width,height} according to the rules in
  // http://www.w3.org/TR/CSS21/visudet.html#min-max-widths

  // Note: throughout the following section of the function, I avoid
  // a * (b / c) because of its reduced accuracy relative to a * b / c
  // or (a * b) / c (which are equivalent).

  PRBool isAutoWidth = stylePos->mWidth.GetUnit() == eStyleUnit_Auto;
  PRBool isAutoHeight = IsAutoHeight(stylePos->mHeight, aCBSize.height);

  nsSize boxSizingAdjust(0,0);
  switch (stylePos->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      boxSizingAdjust += aBorder;
      // fall through
    case NS_STYLE_BOX_SIZING_PADDING:
      boxSizingAdjust += aPadding;
  }

  nscoord width, minWidth, maxWidth, height, minHeight, maxHeight;

  if (!isAutoWidth) {
    width = nsLayoutUtils::ComputeWidthDependentValue(aRenderingContext,
              aFrame, aCBSize.width, stylePos->mWidth) -
            boxSizingAdjust.width;
    if (width < 0)
      width = 0;
  }

  if (stylePos->mMaxWidth.GetUnit() != eStyleUnit_Null) {
    maxWidth = nsLayoutUtils::ComputeWidthDependentValue(aRenderingContext,
                 aFrame, aCBSize.width, stylePos->mMaxWidth) -
               boxSizingAdjust.width;
    if (maxWidth < 0)
      maxWidth = 0;
  } else {
    maxWidth = nscoord_MAX;
  }

  minWidth = nsLayoutUtils::ComputeWidthDependentValue(aRenderingContext,
               aFrame, aCBSize.width, stylePos->mMinWidth) -
             boxSizingAdjust.width;
  if (minWidth < 0)
    minWidth = 0;

  if (!isAutoHeight) {
    height = nsLayoutUtils::ComputeHeightDependentValue(aRenderingContext,
               aFrame, aCBSize.height, stylePos->mHeight) -
             boxSizingAdjust.height;
    if (height < 0)
      height = 0;
  }

  if (!IsAutoHeight(stylePos->mMaxHeight, aCBSize.height)) {
    maxHeight = nsLayoutUtils::ComputeHeightDependentValue(aRenderingContext,
                  aFrame, aCBSize.height, stylePos->mMaxHeight) -
                boxSizingAdjust.height;
    if (maxHeight < 0)
      maxHeight = 0;
  } else {
    maxHeight = nscoord_MAX;
  }

  if (!IsAutoHeight(stylePos->mMinHeight, aCBSize.height)) {
    minHeight = nsLayoutUtils::ComputeHeightDependentValue(aRenderingContext,
                  aFrame, aCBSize.height, stylePos->mMinHeight) -
                boxSizingAdjust.height;
    if (minHeight < 0)
      minHeight = 0;
  } else {
    minHeight = 0;
  }

  if (isAutoWidth) {
    if (isAutoHeight) {

      // 'auto' width, 'auto' height
      if (minWidth > maxWidth)
        maxWidth = minWidth;
      if (minHeight > maxHeight)
        maxHeight = minHeight;

      nscoord heightAtMaxWidth, heightAtMinWidth,
              widthAtMaxHeight, widthAtMinHeight;
      if (aIntrinsicSize.width > 0) {
        heightAtMaxWidth = maxWidth * aIntrinsicSize.height / aIntrinsicSize.width;
        if (heightAtMaxWidth < minHeight)
          heightAtMaxWidth = minHeight;
        heightAtMinWidth = minWidth * aIntrinsicSize.height / aIntrinsicSize.width;
        if (heightAtMinWidth > maxHeight)
          heightAtMinWidth = maxHeight;
      } else {
        heightAtMaxWidth = aIntrinsicSize.height;
        heightAtMinWidth = aIntrinsicSize.height;
      }

      if (aIntrinsicSize.height > 0) {
        widthAtMaxHeight = maxHeight * aIntrinsicSize.width / aIntrinsicSize.height;
        if (widthAtMaxHeight < minWidth)
          widthAtMaxHeight = minWidth;
        widthAtMinHeight = minHeight * aIntrinsicSize.width / aIntrinsicSize.height;
        if (widthAtMinHeight > maxWidth)
          widthAtMinHeight = maxWidth;
      } else {
        widthAtMaxHeight = aIntrinsicSize.width;
        widthAtMinHeight = aIntrinsicSize.width;
      }

      if (aIntrinsicSize.width > maxWidth) {
        if (aIntrinsicSize.height > maxHeight) {
          if (maxWidth * aIntrinsicSize.height <= maxHeight * aIntrinsicSize.width) {
            width = maxWidth;
            height = heightAtMaxWidth;
          } else {
            height = maxHeight;
            width = widthAtMaxHeight;
          }
        } else {
          width = maxWidth;
          height = heightAtMaxWidth;
        }
      } else if (aIntrinsicSize.width < minWidth) {
        if (aIntrinsicSize.height < minHeight) {
          if (minWidth * aIntrinsicSize.height <= minHeight * aIntrinsicSize.width) {
            height = minHeight;
            width = widthAtMinHeight;
          } else {
            width = minWidth;
            height = heightAtMinWidth;
          }
        } else {
          width = minWidth;
          height = heightAtMinWidth;
        }
      } else {
        if (aIntrinsicSize.height > maxHeight) {
          height = maxHeight;
          width = widthAtMaxHeight;
        } else if (aIntrinsicSize.height < minHeight) {
          height = minHeight;
          width = widthAtMinHeight;
        } else {
          width = aIntrinsicSize.width;
          height = aIntrinsicSize.height;
        }
      }

    } else {

      // 'auto' width, non-'auto' height
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);
      if (aIntrinsicSize.height != 0) {
        width = aIntrinsicSize.width * height / aIntrinsicSize.height;
      } else {
        width = aIntrinsicSize.width;
      }
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);

    }
  } else {
    if (isAutoHeight) {

      // non-'auto' width, 'auto' height
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);
      if (aIntrinsicSize.width != 0) {
        height = aIntrinsicSize.height * width / aIntrinsicSize.width;
      } else {
        height = aIntrinsicSize.height;
      }
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);

    } else {

      // non-'auto' width, non-'auto' height
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);

    }
  }

  return nsSize(width, height);
}

/* static */ nscoord
nsLayoutUtils::MinWidthFromInline(nsIFrame* aFrame,
                                  nsIRenderingContext* aRenderingContext)
{
  nsIFrame::InlineMinWidthData data;
  DISPLAY_MIN_WIDTH(aFrame, data.prevLines);
  aFrame->AddInlineMinWidth(aRenderingContext, &data);
  data.Break(aRenderingContext);
  return data.prevLines;
}

/* static */ nscoord
nsLayoutUtils::PrefWidthFromInline(nsIFrame* aFrame,
                                   nsIRenderingContext* aRenderingContext)
{
  nsIFrame::InlinePrefWidthData data;
  DISPLAY_PREF_WIDTH(aFrame, data.prevLines);
  aFrame->AddInlinePrefWidth(aRenderingContext, &data);
  data.Break(aRenderingContext);
  return data.prevLines;
}

void
nsLayoutUtils::DrawString(const nsIFrame*      aFrame,
                          nsIRenderingContext* aContext,
                          const PRUnichar*     aString,
                          PRInt32              aLength,
                          nsPoint              aPoint)
{
#ifdef IBMBIDI
  nsresult rv = NS_ERROR_FAILURE;
  nsPresContext* presContext = aFrame->GetPresContext();
  if (presContext->BidiEnabled()) {
    nsBidiPresUtils* bidiUtils = presContext->GetBidiUtils();

    if (bidiUtils) {
      const nsStyleVisibility* vis = aFrame->GetStyleVisibility();
      nsBidiDirection direction =
        (NS_STYLE_DIRECTION_RTL == vis->mDirection) ?
        NSBIDI_RTL : NSBIDI_LTR;
      rv = bidiUtils->RenderText(aString, aLength, direction,
                                 presContext, *aContext,
                                 aPoint.x, aPoint.y);
    }
  }
  if (NS_FAILED(rv))
#endif // IBMBIDI
  { 
    aContext->SetTextRunRTL(PR_FALSE);
    aContext->DrawString(aString, aLength, aPoint.x, aPoint.y);
  }
}

nscoord
nsLayoutUtils::GetStringWidth(const nsIFrame*      aFrame,
                              nsIRenderingContext* aContext,
                              const PRUnichar*     aString,
                              PRInt32              aLength)
{
#ifdef IBMBIDI
  PRUint32 hints = 0;
  aContext->GetHints(hints);
  // Only do bidi resolution for width measurement if we have a "real"
  // textrun implementation. Otherwise assume the platform can get
  // things right for a mixed-direction string.
  if (hints & NS_RENDERING_HINT_NEW_TEXT_RUNS) {
    nsPresContext* presContext = aFrame->GetPresContext();
    if (presContext->BidiEnabled()) {
      nsBidiPresUtils* bidiUtils = presContext->GetBidiUtils();

      if (bidiUtils) {
        const nsStyleVisibility* vis = aFrame->GetStyleVisibility();
        nsBidiDirection direction =
          (NS_STYLE_DIRECTION_RTL == vis->mDirection) ?
          NSBIDI_RTL : NSBIDI_LTR;
        return bidiUtils->MeasureTextWidth(aString, aLength,
                                           direction, presContext, *aContext);
      }
    }
  }
#endif // IBMBIDI
  aContext->SetTextRunRTL(PR_FALSE);
  nscoord width;
  aContext->GetWidth(aString, aLength, width);
  return width;
}

/* static */ PRBool
nsLayoutUtils::GetFirstLineBaseline(const nsIFrame* aFrame, nscoord* aResult)
{
  const nsBlockFrame* block;
  if (NS_FAILED(NS_CONST_CAST(nsIFrame*, aFrame)->
                  QueryInterface(kBlockFrameCID, (void**)&block))) {
    // For the first-line baseline we also have to check for a table, and if
    // so, use the baseline of its first row.
    nsIAtom* fType = aFrame->GetType();
    if (fType == nsGkAtoms::tableOuterFrame) {
      *aResult = aFrame->GetBaseline();
      return PR_TRUE;
    }

    // For first-line baselines, we have to consider scroll frames.
    if (fType == nsGkAtoms::scrollFrame) {
      nsIScrollableFrame *sFrame;
      if (NS_FAILED(CallQueryInterface(NS_CONST_CAST(nsIFrame*,
                                         aFrame), &sFrame)) || !sFrame) {
        NS_NOTREACHED("not scroll frame");
      }
      nscoord kidBaseline;
      if (GetFirstLineBaseline(sFrame->GetScrolledFrame(), &kidBaseline)) {
        // Consider only the border and padding that contributes to the
        // kid's position, not the scrolling, so we get the initial
        // position.
        *aResult = kidBaseline + aFrame->GetUsedBorderAndPadding().top;
        return PR_TRUE;
      }
      return PR_FALSE;
    }

    // No baseline.
    return PR_FALSE;
  }

  for (nsBlockFrame::const_line_iterator line = block->begin_lines(),
                                     line_end = block->end_lines();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame *kid = line->mFirstChild;
      nscoord kidBaseline;
      if (GetFirstLineBaseline(kid, &kidBaseline)) {
        *aResult = kidBaseline + kid->GetPosition().y;
        return PR_TRUE;
      }
    } else {
      // XXX Is this the right test?  We have some bogus empty lines
      // floating around, but IsEmpty is perhaps too weak.
      if (line->GetHeight() != 0 || !line->IsEmpty()) {
        *aResult = line->mBounds.y + line->GetAscent();
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

/* static */ PRBool
nsLayoutUtils::GetLastLineBaseline(const nsIFrame* aFrame, nscoord* aResult)
{
  const nsBlockFrame* block;
  if (NS_FAILED(NS_CONST_CAST(nsIFrame*, aFrame)->
                  QueryInterface(kBlockFrameCID, (void**)&block)))
    // No baseline.  (We intentionally don't descend into scroll frames.)
    return PR_FALSE;

  for (nsBlockFrame::const_reverse_line_iterator line = block->rbegin_lines(),
                                             line_end = block->rend_lines();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame *kid = line->mFirstChild;
      nscoord kidBaseline;
      if (GetLastLineBaseline(kid, &kidBaseline)) {
        *aResult = kidBaseline + kid->GetPosition().y;
        return PR_TRUE;
      } else if (kid->GetType() == nsGkAtoms::scrollFrame) {
        // Use the bottom of the scroll frame.
        // XXX CSS2.1 really doesn't say what to do here.
        *aResult = kid->GetRect().YMost();
        return PR_TRUE;
      }
    } else {
      // XXX Is this the right test?  We have some bogus empty lines
      // floating around, but IsEmpty is perhaps too weak.
      if (line->GetHeight() != 0 || !line->IsEmpty()) {
        *aResult = line->mBounds.y + line->GetAscent();
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

/* static */ nsIFrame*
nsLayoutUtils::GetClosestLayer(nsIFrame* aFrame)
{
  nsIFrame* layer;
  for (layer = aFrame; layer; layer = layer->GetParent()) {
    if (layer->GetStyleDisplay()->IsPositioned() ||
        (layer->GetParent() &&
          layer->GetParent()->GetType() == nsGkAtoms::scrollFrame))
      break;
  }
  return layer;
}
