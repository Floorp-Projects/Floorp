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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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
#include "nsIFontMetrics.h"
#include "nsIFormControlFrame.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsFrameList.h"
#include "nsGkAtoms.h"
#include "nsIAtom.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsIView.h"
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
#include "imgIContainer.h"
#include "gfxRect.h"
#include "gfxContext.h"
#include "gfxFont.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCSSRendering.h"
#include "nsContentUtils.h"
#include "nsThemeConstants.h"
#include "nsPIDOMWindow.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWidget.h"
#include "gfxMatrix.h"
#include "gfxTypes.h"
#include "gfxUserFontSet.h"
#include "nsTArray.h"
#include "nsHTMLCanvasElement.h"
#include "nsICanvasRenderingContextInternal.h"
#include "gfxPlatform.h"
#include "nsClientRect.h"
#ifdef MOZ_MEDIA
#include "nsHTMLVideoElement.h"
#endif
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsIImageLoadingContent.h"
#include "nsCOMPtr.h"
#include "nsListControlFrame.h"
#include "ImageLayers.h"
#include "mozilla/dom/Element.h"
#include "nsCanvasFrame.h"
#include "gfxDrawable.h"
#include "gfxUtils.h"

#ifdef MOZ_SVG
#include "nsSVGUtils.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGForeignObjectFrame.h"
#include "nsSVGOuterSVGFrame.h"
#endif

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif

using namespace mozilla::layers;
using namespace mozilla::dom;
namespace css = mozilla::css;

/**
 * A namespace class for static layout utilities.
 */

nsIFrame*
nsLayoutUtils::GetLastContinuationWithChild(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");
  aFrame = aFrame->GetLastContinuation();
  while (!aFrame->GetFirstChild(nsnull) &&
         aFrame->GetPrevContinuation()) {
    aFrame = aFrame->GetPrevContinuation();
  }
  return aFrame;
}

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

  // Get the last continuation frame that's a parent
  nsIFrame* lastParentContinuation = nsLayoutUtils::GetLastContinuationWithChild(aFrame);

  nsIFrame* lastChildFrame = lastParentContinuation->GetLastChild(nsnull);
  if (lastChildFrame) {
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

//static
nsIAtom*
nsLayoutUtils::GetChildListNameFor(nsIFrame* aChildFrame)
{
  nsIAtom*      listName;

  if (aChildFrame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
    nsIFrame* pif = aChildFrame->GetPrevInFlow();
    if (pif->GetParent() == aChildFrame->GetParent()) {
      listName = nsGkAtoms::excessOverflowContainersList;
    }
    else {
      listName = nsGkAtoms::overflowContainersList;
    }
  }
  // See if the frame is moved out of the flow
  else if (aChildFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    // Look at the style information to tell
    const nsStyleDisplay* disp = aChildFrame->GetStyleDisplay();

    if (NS_STYLE_POSITION_ABSOLUTE == disp->mPosition) {
      listName = nsGkAtoms::absoluteList;
    } else if (NS_STYLE_POSITION_FIXED == disp->mPosition) {
      if (nsLayoutUtils::IsReallyFixedPos(aChildFrame)) {
        listName = nsGkAtoms::fixedList;
      } else {
        listName = nsGkAtoms::absoluteList;
      }
#ifdef MOZ_XUL
    } else if (NS_STYLE_DISPLAY_POPUP == disp->mDisplay) {
      // Out-of-flows that are DISPLAY_POPUP must be kids of the root popup set
#ifdef DEBUG
      nsIFrame* parent = aChildFrame->GetParent();
      NS_ASSERTION(parent && parent->GetType() == nsGkAtoms::popupSetFrame,
                   "Unexpected parent");
#endif // DEBUG

      // XXX FIXME: Bug 350740
      // Return here, because the postcondition for this function actually
      // fails for this case, since the popups are not in a "real" frame list
      // in the popup set.
      return nsGkAtoms::popupList;
#endif // MOZ_XUL
    } else {
      NS_ASSERTION(aChildFrame->GetStyleDisplay()->IsFloating(),
                   "not a floated frame");
      listName = nsGkAtoms::floatList;
    }

  } else {
    nsIAtom* childType = aChildFrame->GetType();
    if (nsGkAtoms::menuPopupFrame == childType) {
      nsIFrame* parent = aChildFrame->GetParent();
      nsIFrame* firstPopup = (parent)
                             ? parent->GetFirstChild(nsGkAtoms::popupList)
                             : nsnull;
      NS_ASSERTION(!firstPopup || !firstPopup->GetNextSibling(),
                   "We assume popupList only has one child, but it has more.");
      listName = firstPopup == aChildFrame ? nsGkAtoms::popupList : nsnull;
    } else if (nsGkAtoms::tableColGroupFrame == childType) {
      listName = nsGkAtoms::colGroupList;
    } else if (nsGkAtoms::tableCaptionFrame == aChildFrame->GetType()) {
      listName = nsGkAtoms::captionList;
    } else {
      listName = nsnull;
    }
  }

#ifdef NS_DEBUG
  // Verify that the frame is actually in that child list or in the
  // corresponding overflow list.
  nsIFrame* parent = aChildFrame->GetParent();
  PRBool found = parent->GetChildList(listName).ContainsFrame(aChildFrame);
  if (!found) {
    if (!(aChildFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
      found = parent->GetChildList(nsGkAtoms::overflowList)
                .ContainsFrame(aChildFrame);
    }
    else if (aChildFrame->GetStyleDisplay()->IsFloating()) {
      found = parent->GetChildList(nsGkAtoms::overflowOutOfFlowList)
                .ContainsFrame(aChildFrame);
    }
    // else it's positioned and should have been on the 'listName' child list.
    NS_POSTCONDITION(found, "not in child list");
  }
#endif

  return listName;
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
nsLayoutUtils::GetClosestFrameOfType(nsIFrame* aFrame, nsIAtom* aFrameType)
{
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    if (frame->GetType() == aFrameType) {
      return frame;
    }
  }
  return nsnull;
}

// static
nsIFrame*
nsLayoutUtils::GetStyleFrame(nsIFrame* aFrame)
{
  if (aFrame->GetType() == nsGkAtoms::tableOuterFrame) {
    nsIFrame* inner = aFrame->GetFirstChild(nsnull);
    NS_ASSERTION(inner, "Outer table must have an inner");
    return inner;
  }

  return aFrame;
}

nsIFrame*
nsLayoutUtils::GetFloatFromPlaceholder(nsIFrame* aFrame) {
  NS_ASSERTION(nsGkAtoms::placeholderFrame == aFrame->GetType(),
               "Must have a placeholder here");
  if (aFrame->GetStateBits() & PLACEHOLDER_FOR_FLOAT) {
    nsIFrame *outOfFlowFrame =
      nsPlaceholderFrame::GetRealFrameForPlaceholder(aFrame);
    NS_ASSERTION(outOfFlowFrame->GetStyleDisplay()->IsFloating(),
                 "How did that happen?");
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
  nsIFrame* parent = aFrame->GetParent();
  NS_ASSERTION(parent, "Generated content can't be root frame");
  if (parent->IsGeneratedContentFrame()) {
    // Not the root of the generated content
    return PR_FALSE;
  }

  if (aContent && parent->GetContent() != aContent) {
    return PR_FALSE;
  }

  return (aFrame->GetContent()->Tag() == nsGkAtoms::mozgeneratedcontentbefore) ==
    (aPseudoElement == nsCSSPseudoElements::before);
}

// static
nsIFrame*
nsLayoutUtils::GetCrossDocParentFrame(const nsIFrame* aFrame,
                                      nsPoint* aExtraOffset)
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
  if (aExtraOffset) {
    *aExtraOffset += v->GetPosition();
  }
  v = v->GetParent(); // subdocumentframe's view
  if (!v)
    return nsnull;
  return static_cast<nsIFrame*>(v->GetClientData());
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
nsLayoutUtils::IsAncestorFrameCrossDoc(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                       nsIFrame* aCommonAncestor)
{
  if (aFrame == aAncestorFrame)
    return PR_TRUE;
  return IsProperAncestorFrameCrossDoc(aAncestorFrame, aFrame, aCommonAncestor);
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
                                     const nsIContent* aCommonAncestor)
{
  NS_PRECONDITION(aContent1, "aContent1 must not be null");
  NS_PRECONDITION(aContent2, "aContent2 must not be null");

  nsAutoTArray<nsINode*, 32> content1Ancestors;
  nsINode* c1;
  for (c1 = aContent1; c1 && c1 != aCommonAncestor; c1 = c1->GetNodeParent()) {
    content1Ancestors.AppendElement(c1);
  }
  if (!c1 && aCommonAncestor) {
    // So, it turns out aCommonAncestor was not an ancestor of c1. Oops.
    // Never mind. We can continue as if aCommonAncestor was null.
    aCommonAncestor = nsnull;
  }

  nsAutoTArray<nsINode*, 32> content2Ancestors;
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

  int last1 = content1Ancestors.Length() - 1;
  int last2 = content2Ancestors.Length() - 1;
  nsINode* content1Ancestor = nsnull;
  nsINode* content2Ancestor = nsnull;
  while (last1 >= 0 && last2 >= 0
         && ((content1Ancestor = content1Ancestors.ElementAt(last1)) ==
             (content2Ancestor = content2Ancestors.ElementAt(last2)))) {
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

  nsPresContext* presContext = aFrame1->PresContext();
  if (presContext != aFrame2->PresContext()) {
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
  nsIFrame* parentViewFrame = static_cast<nsIFrame*>(aParentView->GetClientData());
  nsIContent* parentViewContent = parentViewFrame ? parentViewFrame->GetContent() : nsnull;
  for (nsIView* insertBefore = aParentView->GetFirstChild(); insertBefore;
       insertBefore = insertBefore->GetNextSibling()) {
    nsIFrame* f = static_cast<nsIFrame*>(insertBefore->GetClientData());
    if (!f) {
      // this view could be some anonymous view attached to a meaningful parent
      for (nsIView* searchView = insertBefore->GetParent(); searchView;
           searchView = searchView->GetParent()) {
        f = static_cast<nsIFrame*>(searchView->GetClientData());
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
  nsIScrollableFrame *sf = do_QueryFrame(frame);
  return sf;
}

nsIFrame*
nsLayoutUtils::GetActiveScrolledRootFor(nsIFrame* aFrame,
                                        nsIFrame* aStopAtAncestor)
{
  nsIFrame* f = aFrame;
  while (f != aStopAtAncestor) {
    if (IsPopup(f))
      break;
    nsIFrame* parent = GetCrossDocParentFrame(f);
    if (!parent)
      break;
    nsIScrollableFrame* sf = do_QueryFrame(parent);
    if (sf && sf->IsScrollingActive() && sf->GetScrolledFrame() == f)
      break;
    f = parent;
  }
  return f;
}

// static
nsIScrollableFrame*
nsLayoutUtils::GetNearestScrollableFrameForDirection(nsIFrame* aFrame,
                                                     Direction aDirection)
{
  NS_ASSERTION(aFrame, "GetNearestScrollableFrameForDirection expects a non-null frame");
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(f);
    if (scrollableFrame) {
      nsPresContext::ScrollbarStyles ss = scrollableFrame->GetScrollbarStyles();
      PRUint32 scrollbarVisibility = scrollableFrame->GetScrollbarVisibility();
      nsRect scrollRange = scrollableFrame->GetScrollRange();
      // Require visible scrollbars or something to scroll to in
      // the given direction.
      if (aDirection == eVertical ?
          (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN &&
           ((scrollbarVisibility & nsIScrollableFrame::VERTICAL) ||
            scrollRange.height > 0)) :
          (ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN &&
           ((scrollbarVisibility & nsIScrollableFrame::HORIZONTAL) ||
            scrollRange.width > 0)))
        return scrollableFrame;
    }
  }
  return nsnull;
}

// static
nsIScrollableFrame*
nsLayoutUtils::GetNearestScrollableFrame(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "GetNearestScrollableFrame expects a non-null frame");
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(f);
    if (scrollableFrame) {
      nsPresContext::ScrollbarStyles ss = scrollableFrame->GetScrollbarStyles();
      if (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN ||
          ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN)
        return scrollableFrame;
    }
  }
  return nsnull;
}

//static
PRBool
nsLayoutUtils::HasPseudoStyle(nsIContent* aContent,
                              nsStyleContext* aStyleContext,
                              nsCSSPseudoElements::Type aPseudoElement,
                              nsPresContext* aPresContext)
{
  NS_PRECONDITION(aPresContext, "Must have a prescontext");

  nsRefPtr<nsStyleContext> pseudoContext;
  if (aContent) {
    pseudoContext = aPresContext->StyleSet()->
      ProbePseudoElementStyle(aContent->AsElement(), aPseudoElement,
                              aStyleContext);
  }
  return pseudoContext != nsnull;
}

nsPoint
nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(nsIDOMEvent* aDOMEvent, nsIFrame* aFrame)
{
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aDOMEvent));
  NS_ASSERTION(privateEvent, "bad implementation");
  if (!privateEvent)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  nsEvent *event = privateEvent->GetInternalNSEvent();
  if (!event)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  return GetEventCoordinatesRelativeTo(event, aFrame);
}

nsPoint
nsLayoutUtils::GetEventCoordinatesRelativeTo(const nsEvent* aEvent, nsIFrame* aFrame)
{
  if (!aEvent || (aEvent->eventStructType != NS_MOUSE_EVENT &&
                  aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
                  aEvent->eventStructType != NS_DRAG_EVENT &&
                  aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT &&
                  aEvent->eventStructType != NS_GESTURENOTIFY_EVENT &&
                  aEvent->eventStructType != NS_MOZTOUCH_EVENT &&
                  aEvent->eventStructType != NS_QUERY_CONTENT_EVENT))
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  const nsGUIEvent* GUIEvent = static_cast<const nsGUIEvent*>(aEvent);
  if (!GUIEvent->widget)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  /* If we walk up the frame tree and discover that any of the frames are
   * transformed, we need to do extra work to convert from the global
   * space to the local space.
   */
  nsIFrame* rootFrame = aFrame;
  PRBool transformFound = PR_FALSE;

  for (nsIFrame* f = aFrame; f; f = GetCrossDocParentFrame(f)) {
    if (f->IsTransformed())
      transformFound = PR_TRUE;

    rootFrame = f;
  }

  nsIView* rootView = rootFrame->GetView();
  if (!rootView)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  nsPoint widgetToView = TranslateWidgetToView(rootFrame->PresContext(),
                               GUIEvent->widget, GUIEvent->refPoint,
                               rootView);

  if (widgetToView == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE))
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  // Convert from root document app units to app units of the document aFrame
  // is in.
  PRInt32 rootAPD = rootFrame->PresContext()->AppUnitsPerDevPixel();
  PRInt32 localAPD = aFrame->PresContext()->AppUnitsPerDevPixel();
  widgetToView = widgetToView.ConvertAppUnits(rootAPD, localAPD);

  /* If we encountered a transform, we can't do simple arithmetic to figure
   * out how to convert back to aFrame's coordinates and must use the CTM.
   */
  if (transformFound)
    return InvertTransformsToRoot(aFrame, widgetToView);

  /* Otherwise, all coordinate systems are translations of one another,
   * so we can just subtract out the different.
   */
  return widgetToView - aFrame->GetOffsetToCrossDoc(rootFrame);
}

nsIFrame*
nsLayoutUtils::GetPopupFrameForEventCoordinates(nsPresContext* aPresContext,
                                                const nsEvent* aEvent)
{
#ifdef MOZ_XUL
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm) {
    return nsnull;
  }
  nsTArray<nsIFrame*> popups = pm->GetVisiblePopups();
  PRUint32 i;
  // Search from top to bottom
  for (i = 0; i < popups.Length(); i++) {
    nsIFrame* popup = popups[i];
    if (popup->PresContext()->GetRootPresContext() == aPresContext &&
        popup->GetOverflowRect().Contains(
          GetEventCoordinatesRelativeTo(aEvent, popup))) {
      return popup;
    }
  }
#endif
  return nsnull;
}

gfxMatrix
nsLayoutUtils::ChangeMatrixBasis(const gfxPoint &aOrigin,
                                 const gfxMatrix &aMatrix)
{
  /* These are translation matrices from world-to-origin of relative frame and
   * vice-versa.  Although I could use the gfxMatrix::Translate function to
   * accomplish this, I'm hoping to reduce the overall number of matrix
   * operations by hardcoding as many of the matrices as possible.
   */
  gfxMatrix worldToOrigin(1.0, 0.0, 0.0, 1.0, -aOrigin.x, -aOrigin.y);
  gfxMatrix originToWorld(1.0, 0.0, 0.0, 1.0,  aOrigin.x,  aOrigin.y);

  /* Multiply all three to get the transform! */
  gfxMatrix result(worldToOrigin);
  result.Multiply(aMatrix);
  result.Multiply(originToWorld);
  return result;
}

/**
 * Given a gfxFloat, constrains its value to be between nscoord_MIN and nscoord_MAX.
 *
 * @param aVal The value to constrain (in/out)
 */
static void ConstrainToCoordValues(gfxFloat &aVal)
{
  if (aVal <= nscoord_MIN)
    aVal = nscoord_MIN;
  else if (aVal >= nscoord_MAX)
    aVal = nscoord_MAX;
}

nsRect
nsLayoutUtils::RoundGfxRectToAppRect(const gfxRect &aRect, float aFactor)
{
  /* Get a new gfxRect whose units are app units by scaling by the specified factor. */
  gfxRect scaledRect(aRect.pos.x * aFactor, aRect.pos.y * aFactor,
                     aRect.size.width * aFactor,
                     aRect.size.height * aFactor);

  /* Round outward. */
  scaledRect.RoundOut();

  /* We now need to constrain our results to the max and min values for coords. */
  ConstrainToCoordValues(scaledRect.pos.x);
  ConstrainToCoordValues(scaledRect.pos.y);
  ConstrainToCoordValues(scaledRect.size.width);
  ConstrainToCoordValues(scaledRect.size.height);

  /* Now typecast everything back.  This is guaranteed to be safe. */
  return nsRect(nscoord(scaledRect.pos.x), nscoord(scaledRect.pos.y),
                nscoord(scaledRect.size.width), nscoord(scaledRect.size.height));
}

nsRect
nsLayoutUtils::MatrixTransformRect(const nsRect &aBounds,
                                   const gfxMatrix &aMatrix, float aFactor)
{
  gfxRect image = aMatrix.TransformBounds(gfxRect(NSAppUnitsToFloatPixels(aBounds.x, aFactor),
                                                  NSAppUnitsToFloatPixels(aBounds.y, aFactor),
                                                  NSAppUnitsToFloatPixels(aBounds.width, aFactor),
                                                  NSAppUnitsToFloatPixels(aBounds.height, aFactor)));

  return RoundGfxRectToAppRect(image, aFactor);
}

nsPoint
nsLayoutUtils::MatrixTransformPoint(const nsPoint &aPoint,
                                    const gfxMatrix &aMatrix, float aFactor)
{
  gfxPoint image = aMatrix.Transform(gfxPoint(NSAppUnitsToFloatPixels(aPoint.x, aFactor),
                                              NSAppUnitsToFloatPixels(aPoint.y, aFactor)));
  return nsPoint(NSFloatPixelsToAppUnits(float(image.x), aFactor),
                 NSFloatPixelsToAppUnits(float(image.y), aFactor));
}

/**
 * Returns the CTM at the specified frame.
 *
 * @param aFrame The frame at which we should calculate the CTM.
 * @return The CTM at the specified frame.
 */
static gfxMatrix GetCTMAt(nsIFrame *aFrame)
{
  gfxMatrix ctm;

  /* Starting at the specified frame, we'll use the GetTransformMatrix
   * function of the frame, which gives us a matrix from this frame up
   * to some other ancestor frame.  Once this function returns null,
   * we've hit the top of the frame tree and can stop.  We get the CTM
   * by simply accumulating all of these matrices together.
   */
  while (aFrame)
    ctm *= aFrame->GetTransformMatrix(&aFrame);
  return ctm;
}

nsPoint
nsLayoutUtils::InvertTransformsToRoot(nsIFrame *aFrame,
                                      const nsPoint &aPoint)
{
  NS_PRECONDITION(aFrame, "Why are you inverting transforms when there is no frame?");

  /* To invert everything to the root, we'll get the CTM, invert it, and use it to transform
   * the point.
   */
  gfxMatrix ctm = GetCTMAt(aFrame);

  /* If the ctm is singular, hand back (0, 0) as a sentinel. */
  if (ctm.IsSingular())
    return nsPoint(0, 0);

  /* Otherwise, invert the CTM and use it to transform the point. */
  return MatrixTransformPoint(aPoint, ctm.Invert(), aFrame->PresContext()->AppUnitsPerDevPixel());
}

nsresult
nsLayoutUtils::GfxRectToIntRect(const gfxRect& aIn, nsIntRect* aOut)
{
  *aOut = nsIntRect(PRInt32(aIn.X()), PRInt32(aIn.Y()),
                    PRInt32(aIn.Width()), PRInt32(aIn.Height()));
  return gfxRect(aOut->x, aOut->y, aOut->width, aOut->height) == aIn
    ? NS_OK : NS_ERROR_FAILURE;
}

static nsIntPoint GetWidgetOffset(nsIWidget* aWidget, nsIWidget*& aRootWidget) {
  nsIntPoint offset(0, 0);
  nsIWidget* parent = aWidget->GetParent();
  while (parent) {
    nsIntRect bounds;
    aWidget->GetBounds(bounds);
    offset += bounds.TopLeft();
    aWidget = parent;
    parent = aWidget->GetParent();
  }
  aRootWidget = aWidget;
  return offset;
}

nsPoint
nsLayoutUtils::TranslateWidgetToView(nsPresContext* aPresContext,
                                     nsIWidget* aWidget, nsIntPoint aPt,
                                     nsIView* aView)
{
  nsPoint viewOffset;
  nsIWidget* viewWidget = aView->GetNearestWidget(&viewOffset);
  if (!viewWidget) {
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  nsIWidget* fromRoot;
  nsIntPoint fromOffset = GetWidgetOffset(aWidget, fromRoot);
  nsIWidget* toRoot;
  nsIntPoint toOffset = GetWidgetOffset(viewWidget, toRoot);

  nsIntPoint widgetPoint;
  if (fromRoot == toRoot) {
    widgetPoint = aPt + fromOffset - toOffset;
  } else {
    nsIntPoint screenPoint = aWidget->WidgetToScreenOffset();
    widgetPoint = aPt + screenPoint - viewWidget->WidgetToScreenOffset();
  }

  nsPoint widgetAppUnits(aPresContext->DevPixelsToAppUnits(widgetPoint.x),
                         aPresContext->DevPixelsToAppUnits(widgetPoint.y));
  return widgetAppUnits - viewOffset;
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

#ifdef DEBUG
#include <stdio.h>

static PRBool gDumpPaintList = PR_FALSE;
static PRBool gDumpEventList = PR_FALSE;
#endif

nsIFrame*
nsLayoutUtils::GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt,
                                PRBool aShouldIgnoreSuppression,
                                PRBool aIgnoreRootScrollFrame)
{
  nsresult rv;
  nsTArray<nsIFrame*> outFrames;
  rv = GetFramesForArea(aFrame, nsRect(aPt, nsSize(1, 1)), outFrames,
                        aShouldIgnoreSuppression, aIgnoreRootScrollFrame);
  NS_ENSURE_SUCCESS(rv, nsnull);
  return outFrames.Length() ? outFrames.ElementAt(0) : nsnull;
}

nsresult
nsLayoutUtils::GetFramesForArea(nsIFrame* aFrame, const nsRect& aRect,
                                nsTArray<nsIFrame*> &aOutFrames,
                                PRBool aShouldIgnoreSuppression,
                                PRBool aIgnoreRootScrollFrame)
{
  nsDisplayListBuilder builder(aFrame, PR_TRUE, PR_FALSE);
  nsDisplayList list;
  nsRect target(aRect);

  if (aShouldIgnoreSuppression) {
    builder.IgnorePaintSuppression();
  }

  if (aIgnoreRootScrollFrame) {
    nsIFrame* rootScrollFrame =
      aFrame->PresContext()->PresShell()->GetRootScrollFrame();
    if (rootScrollFrame) {
      builder.SetIgnoreScrollFrame(rootScrollFrame);
    }
  }

  builder.EnterPresShell(aFrame, target);

  nsresult rv =
    aFrame->BuildDisplayListForStackingContext(&builder, target, &list);

  builder.LeavePresShell(aFrame, target);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  if (gDumpEventList) {
    fprintf(stderr, "Event handling --- (%d,%d):\n", aRect.x, aRect.y);
    nsFrame::PrintDisplayList(&builder, list);
  }
#endif

  nsDisplayItem::HitTestState hitTestState;
  list.HitTest(&builder, target, &hitTestState, &aOutFrames);
  list.DeleteAll();
  return NS_OK;
}

/**
 * Remove all leaf display items that are not for descendants of
 * aBuilder->GetReferenceFrame() from aList, and move all nsDisplayClip
 * wrappers to their correct locations.
 * @param aExtraPage the page we constructed aList for
 * @param aY the Y-coordinate where aPage would be positioned relative
 * to the main page (aBuilder->GetReferenceFrame()), considering only
 * the content and ignoring page margins and dead space
 * @param aList the list that is modified in-place
 */
static void
PruneDisplayListForExtraPage(nsDisplayListBuilder* aBuilder,
        nsIFrame* aExtraPage, nscoord aY, nsDisplayList* aList)
{
  nsDisplayList newList;
  // The page which we're really constructing a display list for
  nsIFrame* mainPage = aBuilder->ReferenceFrame();

  while (PR_TRUE) {
    nsDisplayItem* i = aList->RemoveBottom();
    if (!i)
      break;
    nsDisplayList* subList = i->GetList();
    if (subList) {
      PruneDisplayListForExtraPage(aBuilder, aExtraPage, aY, subList);
      nsDisplayItem::Type type = i->GetType();
      if (type == nsDisplayItem::TYPE_CLIP ||
          type == nsDisplayItem::TYPE_CLIP_ROUNDED_RECT) {
        // This might clip an element which should appear on the first
        // page, and that element might be visible if this uses a 'clip'
        // property with a negative top.
        // The clip area needs to be moved because the frame geometry doesn't
        // put page content frames for adjacent pages vertically adjacent,
        // there are page margins and dead space between them in print
        // preview, and in printing all pages are at (0,0)...
        // XXX we have no way to test this right now that I know of;
        // the 'clip' property requires an abs-pos element and we never
        // paint abs-pos elements that start after the main page
        // (bug 426909).
        nsDisplayClip* clip = static_cast<nsDisplayClip*>(i);
        clip->SetClipRect(clip->GetClipRect() + nsPoint(0, aY) -
                aExtraPage->GetOffsetTo(mainPage));
      }
      newList.AppendToTop(i);
    } else {
      nsIFrame* f = i->GetUnderlyingFrame();
      if (f && nsLayoutUtils::IsProperAncestorFrameCrossDoc(mainPage, f)) {
        // This one is in the page we care about, keep it
        newList.AppendToTop(i);
      } else {
        // We're throwing this away so call its destructor now. The memory
        // is owned by aBuilder which destroys all items at once.
        i->~nsDisplayItem();
      }
    }
  }
  aList->AppendToTop(&newList);
}

static nsresult
BuildDisplayListForExtraPage(nsDisplayListBuilder* aBuilder,
        nsIFrame* aPage, nscoord aY, nsDisplayList* aList)
{
  nsDisplayList list;
  // Pass an empty dirty rect since we're only interested in finding
  // placeholders whose out-of-flows are in the page
  // aBuilder->GetReferenceFrame(), and the paths to those placeholders
  // have already been marked as NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO.
  // Note that we should still do a prune step since we don't want to
  // rely on dirty-rect checking for correctness.
  nsresult rv = aPage->BuildDisplayListForStackingContext(aBuilder, nsRect(), &list);
  if (NS_FAILED(rv))
    return rv;
  PruneDisplayListForExtraPage(aBuilder, aPage, aY, &list);
  aList->AppendToTop(&list);
  return NS_OK;
}

static nsIFrame*
GetNextPage(nsIFrame* aPageContentFrame)
{
  // XXX ugh
  nsIFrame* pageFrame = aPageContentFrame->GetParent();
  NS_ASSERTION(pageFrame->GetType() == nsGkAtoms::pageFrame,
               "pageContentFrame has unexpected parent");
  nsIFrame* nextPageFrame = pageFrame->GetNextSibling();
  if (!nextPageFrame)
    return nsnull;
  NS_ASSERTION(nextPageFrame->GetType() == nsGkAtoms::pageFrame,
               "pageFrame's sibling is not a page frame...");
  nsIFrame* f = nextPageFrame->GetFirstChild(nsnull);
  NS_ASSERTION(f, "pageFrame has no page content frame!");
  NS_ASSERTION(f->GetType() == nsGkAtoms::pageContentFrame,
               "pageFrame's child is not page content!");
  return f;
}

nsresult
nsLayoutUtils::PaintFrame(nsIRenderingContext* aRenderingContext, nsIFrame* aFrame,
                          const nsRegion& aDirtyRegion, nscolor aBackstop,
                          PRUint32 aFlags)
{
  if (aFlags & PAINT_WIDGET_LAYERS) {
    nsIView* view = aFrame->GetView();
    if (!(view && view->GetWidget() && GetDisplayRootFrame(aFrame) == aFrame)) {
      aFlags &= ~PAINT_WIDGET_LAYERS;
      NS_ASSERTION(aRenderingContext, "need a rendering context");
    }
  }

  nsPresContext* presContext = aFrame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();

  PRBool ignoreViewportScrolling = presShell->IgnoringViewportScrolling();
  nsRegion visibleRegion;
  if (aFlags & PAINT_WIDGET_LAYERS) {
    // This layer tree will be reused, so we'll need to calculate it
    // for the whole "visible" area of the window
    // 
    // |ignoreViewportScrolling| and |usingDisplayPort| are persistent
    // document-rendering state.  We rely on PresShell to flush
    // retained layers as needed when that persistent state changes.
    if (!presShell->UsingDisplayPort()) {
      visibleRegion = aFrame->GetOverflowRectRelativeToSelf();
    } else {
      visibleRegion = presShell->GetDisplayPort();
    }
  } else {
    visibleRegion = aDirtyRegion;
  }

  // If we're going to display something different from what we'd normally
  // paint in a window then we will flush out any retained layer trees before
  // *and after* we draw.
  PRBool willFlushRetainedLayers = (aFlags & PAINT_HIDE_CARET) != 0;

  nsDisplayListBuilder builder(aFrame, PR_FALSE, !(aFlags & PAINT_HIDE_CARET));
  nsDisplayList list;
  if (aFlags & PAINT_IN_TRANSFORM) {
    builder.SetInTransform(PR_TRUE);
  }
  if (aFlags & PAINT_SYNC_DECODE_IMAGES) {
    builder.SetSyncDecodeImages(PR_TRUE);
  }
  if (aFlags & PAINT_WIDGET_LAYERS) {
    builder.SetPaintingToWindow(PR_TRUE);
  }
  if (aFlags & PAINT_IGNORE_SUPPRESSION) {
    builder.IgnorePaintSuppression();
  }
  nsRect canvasArea(nsPoint(0, 0), aFrame->GetSize());
  if (ignoreViewportScrolling) {
    NS_ASSERTION(!aFrame->GetParent(), "must have root frame");
    nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
    if (rootScrollFrame) {
      nsIScrollableFrame* rootScrollableFrame =
        presShell->GetRootScrollFrameAsScrollable();
      if (aFlags & PAINT_DOCUMENT_RELATIVE) {
        // Make visibleRegion and aRenderingContext relative to the
        // scrolled frame instead of the root frame.
        nsPoint pos = rootScrollableFrame->GetScrollPosition();
        visibleRegion.MoveBy(-pos);
        if (aRenderingContext) {
          aRenderingContext->Translate(pos.x, pos.y);
        }
      }
      builder.SetIgnoreScrollFrame(rootScrollFrame);

      nsCanvasFrame* canvasFrame =
        do_QueryFrame(rootScrollableFrame->GetScrolledFrame());
      if (canvasFrame) {
        // Use UnionRect here to ensure that areas where the scrollbars
        // were are still filled with the background color.
        canvasArea.UnionRect(canvasArea,
          canvasFrame->CanvasArea() + builder.ToReferenceFrame(canvasFrame));
      }
    }
  }
  nsresult rv;

  nsRect dirtyRect = visibleRegion.GetBounds();
  builder.EnterPresShell(aFrame, dirtyRect);

  rv = aFrame->BuildDisplayListForStackingContext(&builder, dirtyRect, &list);

  const PRBool paintAllContinuations = aFlags & PAINT_ALL_CONTINUATIONS;
  NS_ASSERTION(!paintAllContinuations || !aFrame->GetPrevContinuation(),
               "If painting all continuations, the frame must be "
               "first-continuation");

  nsIAtom* frameType = aFrame->GetType();
  if (NS_SUCCEEDED(rv) && !paintAllContinuations &&
      frameType == nsGkAtoms::pageContentFrame) {
    NS_ASSERTION(!(aFlags & PAINT_WIDGET_LAYERS),
      "shouldn't be painting with widget layers for page content frames");
    // We may need to paint out-of-flow frames whose placeholders are
    // on other pages. Add those pages to our display list. Note that
    // out-of-flow frames can't be placed after their placeholders so
    // we don't have to process earlier pages. The display lists for
    // these extra pages are pruned so that only display items for the
    // page we currently care about (which we would have reached by
    // following placeholders to their out-of-flows) end up on the list.
    nsIFrame* page = aFrame;
    nscoord y = aFrame->GetSize().height;
    while ((page = GetNextPage(page)) != nsnull) {
      rv = BuildDisplayListForExtraPage(&builder, page, y, &list);
      if (NS_FAILED(rv))
        break;
      y += page->GetSize().height;
    }
  }

  if (paintAllContinuations) {
    nsIFrame* currentFrame = aFrame;
    while (NS_SUCCEEDED(rv) &&
           (currentFrame = currentFrame->GetNextContinuation()) != nsnull) {
      nsRect frameDirty = dirtyRect - builder.ToReferenceFrame(currentFrame);
      rv = currentFrame->BuildDisplayListForStackingContext(&builder,
                                                            frameDirty, &list);
    }
  }

  // For the viewport frame in print preview/page layout we want to paint
  // the grey background behind the page, not the canvas color.
  if (frameType == nsGkAtoms::viewportFrame && 
      nsLayoutUtils::NeedsPrintPreviewBackground(presContext)) {
    nsRect bounds = nsRect(builder.ToReferenceFrame(aFrame),
                           aFrame->GetSize());
    rv = presShell->AddPrintPreviewBackgroundItem(builder, list, aFrame, bounds);
  } else if (frameType != nsGkAtoms::pageFrame) {
    // For printing, this function is first called on an nsPageFrame, which
    // creates a display list with a PageContent item. The PageContent item's
    // paint function calls this function on the nsPageFrame's child which is
    // an nsPageContentFrame. We only want to add the canvas background color
    // item once, for the nsPageContentFrame.

    // Add the canvas background color to the bottom of the list. This
    // happens after we've built the list so that AddCanvasBackgroundColorItem
    // can monkey with the contents if necessary.
    rv = presShell->AddCanvasBackgroundColorItem(
           builder, list, aFrame, canvasArea, aBackstop);

    // If the passed in backstop color makes us draw something different from
    // normal, we need to flush layers.
    if ((aFlags & PAINT_WIDGET_LAYERS) && !willFlushRetainedLayers) {
      nsIView* view = aFrame->GetView();
      if (view) {
        nscolor backstop = presShell->ComputeBackstopColor(view);
        // The PresShell's canvas background color doesn't get updated until
        // EnterPresShell, so this check has to be done after that.
        nscolor canvasColor = presShell->GetCanvasBackground();
        if (NS_ComposeColors(aBackstop, canvasColor) !=
            NS_ComposeColors(backstop, canvasColor)) {
          willFlushRetainedLayers = PR_TRUE;
        }
      }
    }
  }

  builder.LeavePresShell(aFrame, dirtyRect);
  NS_ENSURE_SUCCESS(rv, rv);

  if (builder.GetHadToIgnorePaintSuppression()) {
    willFlushRetainedLayers = PR_TRUE;
  }

#ifdef DEBUG
  if (gDumpPaintList) {
    fprintf(stderr, "Painting --- before optimization (dirty %d,%d,%d,%d):\n",
            dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
    nsFrame::PrintDisplayList(&builder, list);
  }
#endif

  list.ComputeVisibilityForRoot(&builder, &visibleRegion);

#ifdef DEBUG
  if (gDumpPaintList) {
    fprintf(stderr, "Painting --- after optimization:\n");
    nsFrame::PrintDisplayList(&builder, list);
  }
#endif

  PRUint32 flags = nsDisplayList::PAINT_DEFAULT;
  if (aFlags & PAINT_WIDGET_LAYERS) {
    flags |= nsDisplayList::PAINT_USE_WIDGET_LAYERS;
    nsIWidget *widget = aFrame->GetNearestWidget();
    PRInt32 pixelRatio = presContext->AppUnitsPerDevPixel();
    nsIntRegion visibleWindowRegion(visibleRegion.ToOutsidePixels(pixelRatio));
    nsIntRegion dirtyWindowRegion(aDirtyRegion.ToOutsidePixels(pixelRatio));

    if (willFlushRetainedLayers) {
      // The caller wanted to paint from retained layers, but set up
      // the paint in such a way that we can't use them.  We're going
      // to display something different from what we'd normally paint
      // in a window, so make sure we flush out any retained layer
      // trees before *and after* we draw.  Callers should be fixed to
      // not do this.
      NS_WARNING("Flushing retained layers!");
      flags |= nsDisplayList::PAINT_FLUSH_LAYERS;
    } else if (widget && !(aFlags & PAINT_DOCUMENT_RELATIVE)) {
      // XXX we should simplify this API now that dirtyWindowRegion always
      // covers the entire window
      widget->UpdatePossiblyTransparentRegion(dirtyWindowRegion, visibleWindowRegion);
    }
  }

  list.PaintRoot(&builder, aRenderingContext, flags);

#ifdef DEBUG
  if (gDumpPaintList) {
    fprintf(stderr, "Painting --- retained layer tree:\n");
    builder.LayerBuilder()->DumpRetainedLayerTree();
  }
#endif

  // Flush the list so we don't trigger the IsEmpty-on-destruction assertion
  list.DeleteAll();
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

static void
AddBoxesForFrame(nsIFrame* aFrame,
                 nsLayoutUtils::BoxCallback* aCallback)
{
  nsIAtom* pseudoType = aFrame->GetStyleContext()->GetPseudo();

  if (pseudoType == nsCSSAnonBoxes::tableOuter) {
    AddBoxesForFrame(aFrame->GetFirstChild(nsnull), aCallback);
    nsIFrame* kid = aFrame->GetFirstChild(nsGkAtoms::captionList);
    if (kid) {
      AddBoxesForFrame(kid, aCallback);
    }
  } else if (pseudoType == nsCSSAnonBoxes::mozAnonymousBlock ||
             pseudoType == nsCSSAnonBoxes::mozAnonymousPositionedBlock ||
             pseudoType == nsCSSAnonBoxes::mozMathMLAnonymousBlock ||
             pseudoType == nsCSSAnonBoxes::mozXULAnonymousBlock) {
    for (nsIFrame* kid = aFrame->GetFirstChild(nsnull); kid; kid = kid->GetNextSibling()) {
      AddBoxesForFrame(kid, aCallback);
    }
  } else {
    aCallback->AddBox(aFrame);
  }
}

void
nsLayoutUtils::GetAllInFlowBoxes(nsIFrame* aFrame, BoxCallback* aCallback)
{
  while (aFrame) {
    AddBoxesForFrame(aFrame, aCallback);
    aFrame = nsLayoutUtils::GetNextContinuationOrSpecialSibling(aFrame);
  }
}

struct BoxToBorderRect : public nsLayoutUtils::BoxCallback {
  nsIFrame*                    mRelativeTo;
  nsLayoutUtils::RectCallback* mCallback;

  BoxToBorderRect(nsIFrame* aRelativeTo, nsLayoutUtils::RectCallback* aCallback)
    : mRelativeTo(aRelativeTo), mCallback(aCallback) {}

  virtual void AddBox(nsIFrame* aFrame) {
#ifdef MOZ_SVG
    nsRect r;
    nsIFrame* outer = nsSVGUtils::GetOuterSVGFrameAndCoveredRegion(aFrame, &r);
    if (outer) {
      mCallback->AddRect(r + outer->GetOffsetTo(mRelativeTo));
    } else
#endif
      mCallback->AddRect(nsRect(aFrame->GetOffsetTo(mRelativeTo), aFrame->GetSize()));
  }
};

void
nsLayoutUtils::GetAllInFlowRects(nsIFrame* aFrame, nsIFrame* aRelativeTo,
                                 RectCallback* aCallback)
{
  BoxToBorderRect converter(aRelativeTo, aCallback);
  GetAllInFlowBoxes(aFrame, &converter);
}

nsLayoutUtils::RectAccumulator::RectAccumulator() : mSeenFirstRect(PR_FALSE) {}

void nsLayoutUtils::RectAccumulator::AddRect(const nsRect& aRect) {
  mResultRect.UnionRect(mResultRect, aRect);
  if (!mSeenFirstRect) {
    mSeenFirstRect = PR_TRUE;
    mFirstRect = aRect;
  }
}

nsLayoutUtils::RectListBuilder::RectListBuilder(nsClientRectList* aList)
  : mRectList(aList), mRV(NS_OK) {}

void nsLayoutUtils::RectListBuilder::AddRect(const nsRect& aRect) {
  nsRefPtr<nsClientRect> rect = new nsClientRect();
  if (!rect) {
    mRV = NS_ERROR_OUT_OF_MEMORY;
    return;
  }

  rect->SetLayoutRect(aRect);
  mRectList->Append(rect);
}

nsIFrame* nsLayoutUtils::GetContainingBlockForClientRect(nsIFrame* aFrame)
{
  // get the nearest enclosing SVG foreign object frame or the root frame
  while (aFrame->GetParent() &&
         !aFrame->IsFrameOfType(nsIFrame::eSVGForeignObject)) {
    aFrame = aFrame->GetParent();
  }

  return aFrame;
}

nsRect
nsLayoutUtils::GetAllInFlowRectsUnion(nsIFrame* aFrame, nsIFrame* aRelativeTo) {
  RectAccumulator accumulator;
  GetAllInFlowRects(aFrame, aRelativeTo, &accumulator);
  return accumulator.mResultRect.IsEmpty() ? accumulator.mFirstRect
          : accumulator.mResultRect;
}

nsRect
nsLayoutUtils::GetTextShadowRectsUnion(const nsRect& aTextAndDecorationsRect,
                                       nsIFrame* aFrame)
{
  const nsStyleText* textStyle = aFrame->GetStyleText();
  if (!textStyle->mTextShadow)
    return aTextAndDecorationsRect;

  nsRect resultRect = aTextAndDecorationsRect;
  PRInt32 A2D = aFrame->PresContext()->AppUnitsPerDevPixel();
  for (PRUint32 i = 0; i < textStyle->mTextShadow->Length(); ++i) {
    nsRect tmpRect(aTextAndDecorationsRect);
    nsCSSShadowItem* shadow = textStyle->mTextShadow->ShadowAt(i);

    tmpRect.MoveBy(nsPoint(shadow->mXOffset, shadow->mYOffset));
    tmpRect.Inflate(
      nsContextBoxBlur::GetBlurRadiusMargin(shadow->mRadius, A2D));

    resultRect.UnionRect(resultRect, tmpRect);
  }
  return resultRect;
}

nsresult
nsLayoutUtils::GetFontMetricsForFrame(nsIFrame* aFrame,
                                      nsIFontMetrics** aFontMetrics)
{
  return nsLayoutUtils::GetFontMetricsForStyleContext(aFrame->GetStyleContext(),
                                                      aFontMetrics);
}

nsresult
nsLayoutUtils::GetFontMetricsForStyleContext(nsStyleContext* aStyleContext,
                                             nsIFontMetrics** aFontMetrics)
{
  // pass the user font set object into the device context to pass along to CreateFontGroup
  gfxUserFontSet* fs = aStyleContext->PresContext()->GetUserFontSet();

  return aStyleContext->PresContext()->DeviceContext()->GetMetricsFor(
                  aStyleContext->GetStyleFont()->mFont,
                  aStyleContext->GetStyleVisibility()->mLanguage,
                  fs, *aFontMetrics);
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
nsLayoutUtils::GetAsBlock(nsIFrame* aFrame)
{
  nsBlockFrame* block = do_QueryFrame(aFrame);
  return block;
}

nsBlockFrame*
nsLayoutUtils::FindNearestBlockAncestor(nsIFrame* aFrame)
{
  nsIFrame* nextAncestor;
  for (nextAncestor = aFrame->GetParent(); nextAncestor;
       nextAncestor = nextAncestor->GetParent()) {
    nsBlockFrame* block = GetAsBlock(nextAncestor);
    if (block)
      return block;
  }
  return nsnull;
}

nsIFrame*
nsLayoutUtils::GetNonGeneratedAncestor(nsIFrame* aFrame)
{
  if (!(aFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT))
    return aFrame;

  nsFrameManager* frameManager = aFrame->PresContext()->FrameManager();
  nsIFrame* f = aFrame;
  do {
    f = GetParentOrPlaceholderFor(frameManager, f);
  } while (f->GetStateBits() & NS_FRAME_GENERATED_CONTENT);
  return f;
}

nsIFrame*
nsLayoutUtils::GetParentOrPlaceholderFor(nsFrameManager* aFrameManager,
                                         nsIFrame* aFrame)
{
  if ((aFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
      && !aFrame->GetPrevInFlow()) {
    return aFrameManager->GetPlaceholderFrameFor(aFrame);
  }
  return aFrame->GetParent();
}

nsIFrame*
nsLayoutUtils::GetNextContinuationOrSpecialSibling(nsIFrame *aFrame)
{
  nsIFrame *result = aFrame->GetNextContinuation();
  if (result)
    return result;

  if ((aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL) != 0) {
    // We only store the "special sibling" annotation with the first
    // frame in the continuation chain. Walk back to find that frame now.
    aFrame = aFrame->GetFirstContinuation();

    void* value = aFrame->Properties().Get(nsIFrame::IBSplitSpecialSibling());
    return static_cast<nsIFrame*>(value);
  }

  return nsnull;
}

nsIFrame*
nsLayoutUtils::GetFirstContinuationOrSpecialSibling(nsIFrame *aFrame)
{
  nsIFrame *result = aFrame->GetFirstContinuation();
  if (result->GetStateBits() & NS_FRAME_IS_SPECIAL) {
    while (PR_TRUE) {
      nsIFrame *f = static_cast<nsIFrame*>
        (result->Properties().Get(nsIFrame::IBSplitSpecialPrevSibling()));
      if (!f)
        break;
      result = f;
    }
  }

  return result;
}

PRBool
nsLayoutUtils::IsViewportScrollbarFrame(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  nsIFrame* rootScrollFrame =
    aFrame->PresContext()->PresShell()->GetRootScrollFrame();
  if (!rootScrollFrame)
    return PR_FALSE;

  nsIScrollableFrame* rootScrollableFrame = do_QueryFrame(rootScrollFrame);
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

// Use only for widths/heights (or their min/max), since it clamps
// negative calc() results to 0.
static PRBool GetAbsoluteCoord(const nsStyleCoord& aStyle, nscoord& aResult)
{
  if (aStyle.IsCalcUnit()) {
    if (aStyle.CalcHasPercent()) {
      return PR_FALSE;
    }
    // If it has no percents, we can pass 0 for the percentage basis.
    aResult = nsRuleNode::ComputeComputedCalc(aStyle, 0);
    if (aResult < 0)
      aResult = 0;
    return PR_TRUE;
  }

  if (eStyleUnit_Coord != aStyle.GetUnit())
    return PR_FALSE;

  aResult = aStyle.GetCoordValue();
  NS_ASSERTION(aResult >= 0, "negative widths not allowed");
  return PR_TRUE;
}

static PRBool
GetPercentHeight(const nsStyleCoord& aStyle,
                 nsIFrame* aFrame,
                 nscoord& aResult)
{
  if (eStyleUnit_Percent != aStyle.GetUnit())
    return PR_FALSE;

  nsIFrame *f;
  for (f = aFrame->GetParent(); f && !f->IsContainingBlock();
       f = f->GetParent())
    ;
  if (!f) {
    NS_NOTREACHED("top of frame tree not a containing block");
    return PR_FALSE;
  }

  const nsStylePosition *pos = f->GetStylePosition();
  nscoord h;
  if (!GetAbsoluteCoord(pos->mHeight, h) &&
      !GetPercentHeight(pos->mHeight, f, h)) {
    NS_ASSERTION(pos->mHeight.GetUnit() == eStyleUnit_Auto ||
                 pos->mHeight.HasPercent(),
                 "unknown height unit");
    nsIAtom* fType = f->GetType();
    if (fType != nsGkAtoms::viewportFrame && fType != nsGkAtoms::canvasFrame &&
        fType != nsGkAtoms::pageContentFrame) {
      // There's no basis for the percentage height, so it acts like auto.
      // Should we consider a max-height < min-height pair a basis for
      // percentage heights?  The spec is somewhat unclear, and not doing
      // so is simpler and avoids troubling discontinuities in behavior,
      // so I'll choose not to. -LDB
      return PR_FALSE;
    }

    NS_ASSERTION(pos->mHeight.GetUnit() == eStyleUnit_Auto,
                 "Unexpected height unit for viewport or canvas or page-content");
    // For the viewport, canvas, and page-content kids, the percentage
    // basis is just the parent height.
    h = f->GetSize().height;
    if (h == NS_UNCONSTRAINEDSIZE) {
      // We don't have a percentage basis after all
      return PR_FALSE;
    }
  }

  nscoord maxh;
  if (GetAbsoluteCoord(pos->mMaxHeight, maxh) ||
      GetPercentHeight(pos->mMaxHeight, f, maxh)) {
    if (maxh < h)
      h = maxh;
  } else {
    NS_ASSERTION(pos->mMaxHeight.GetUnit() == eStyleUnit_None ||
                 pos->mMaxHeight.HasPercent(),
                 "unknown max-height unit");
  }

  nscoord minh;
  if (GetAbsoluteCoord(pos->mMinHeight, minh) ||
      GetPercentHeight(pos->mMinHeight, f, minh)) {
    if (minh > h)
      h = minh;
  } else {
    NS_ASSERTION(pos->mMinHeight.HasPercent(),
                 "unknown min-height unit");
  }

  aResult = NSToCoordRound(aStyle.GetPercentValue() * h);
  return PR_TRUE;
}

// Handles only -moz-max-content and -moz-min-content, and
// -moz-fit-content for min-width and max-width, since the others
// (-moz-fit-content for width, and -moz-available) have no effect on
// intrinsic widths.
enum eWidthProperty { PROP_WIDTH, PROP_MAX_WIDTH, PROP_MIN_WIDTH };
static PRBool
GetIntrinsicCoord(const nsStyleCoord& aStyle,
                  nsIRenderingContext* aRenderingContext,
                  nsIFrame* aFrame,
                  eWidthProperty aProperty,
                  nscoord& aResult)
{
  NS_PRECONDITION(aProperty == PROP_WIDTH || aProperty == PROP_MAX_WIDTH ||
                  aProperty == PROP_MIN_WIDTH, "unexpected property");
  if (aStyle.GetUnit() != eStyleUnit_Enumerated)
    return PR_FALSE;
  PRInt32 val = aStyle.GetIntValue();
  NS_ASSERTION(val == NS_STYLE_WIDTH_MAX_CONTENT ||
               val == NS_STYLE_WIDTH_MIN_CONTENT ||
               val == NS_STYLE_WIDTH_FIT_CONTENT ||
               val == NS_STYLE_WIDTH_AVAILABLE,
               "unexpected enumerated value for width property");
  if (val == NS_STYLE_WIDTH_AVAILABLE)
    return PR_FALSE;
  if (val == NS_STYLE_WIDTH_FIT_CONTENT) {
    if (aProperty == PROP_WIDTH)
      return PR_FALSE; // handle like 'width: auto'
    if (aProperty == PROP_MAX_WIDTH)
      // constrain large 'width' values down to -moz-max-content
      val = NS_STYLE_WIDTH_MAX_CONTENT;
    else
      // constrain small 'width' or 'max-width' values up to -moz-min-content
      val = NS_STYLE_WIDTH_MIN_CONTENT;
  }

  NS_ASSERTION(val == NS_STYLE_WIDTH_MAX_CONTENT ||
               val == NS_STYLE_WIDTH_MIN_CONTENT,
               "should have reduced everything remaining to one of these");
  if (val == NS_STYLE_WIDTH_MAX_CONTENT)
    aResult = aFrame->GetPrefWidth(aRenderingContext);
  else
    aResult = aFrame->GetMinWidth(aRenderingContext);
  return PR_TRUE;
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
  static_cast<nsFrame*>(aFrame)->ListTag(stdout);
  printf(" %s intrinsic width for container:\n",
         aType == MIN_WIDTH ? "min" : "pref");
#endif

  nsIFrame::IntrinsicWidthOffsetData offsets =
    aFrame->IntrinsicWidthOffsets(aRenderingContext);

  const nsStylePosition *stylePos = aFrame->GetStylePosition();
  PRUint8 boxSizing = stylePos->mBoxSizing;
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

  nscoord maxw;
  PRBool haveFixedMaxWidth = GetAbsoluteCoord(styleMaxWidth, maxw);
  nscoord minw;
  PRBool haveFixedMinWidth = GetAbsoluteCoord(styleMinWidth, minw);

  // If we have a specified width (or a specified 'min-width' greater
  // than the specified 'max-width', which works out to the same thing),
  // don't even bother getting the frame's intrinsic width.
  if (styleWidth.GetUnit() == eStyleUnit_Enumerated &&
      (styleWidth.GetIntValue() == NS_STYLE_WIDTH_MAX_CONTENT ||
       styleWidth.GetIntValue() == NS_STYLE_WIDTH_MIN_CONTENT)) {
    // -moz-fit-content and -moz-available enumerated widths compute intrinsic
    // widths just like auto.
    // For -moz-max-content and -moz-min-content, we handle them like
    // specified widths, but ignore -moz-box-sizing.
    boxSizing = NS_STYLE_BOX_SIZING_CONTENT;
  } else if (styleWidth.GetUnit() != eStyleUnit_Coord &&
             !(haveFixedMinWidth && haveFixedMaxWidth && maxw <= minw)) {
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
    static_cast<nsFrame*>(aFrame)->ListTag(stdout);
    printf(" %s intrinsic width from frame is %d.\n",
           aType == MIN_WIDTH ? "min" : "pref", result);
#endif

    // Handle elements with an intrinsic ratio (or size) and a specified
    // height, min-height, or max-height.
    const nsStyleCoord &styleHeight = stylePos->mHeight;
    const nsStyleCoord &styleMinHeight = stylePos->mMinHeight;
    const nsStyleCoord &styleMaxHeight = stylePos->mMaxHeight;
    if (styleHeight.GetUnit() != eStyleUnit_Auto ||
        !(styleMinHeight.GetUnit() == eStyleUnit_Coord &&
          styleMinHeight.GetCoordValue() == 0) ||
        styleMaxHeight.GetUnit() != eStyleUnit_None) {

      nsSize ratio = aFrame->GetIntrinsicRatio();

      if (ratio.height != 0) {

        nscoord h;
        if (GetAbsoluteCoord(styleHeight, h) ||
            GetPercentHeight(styleHeight, aFrame, h)) {
          result =
            NSToCoordRound(h * (float(ratio.width) / float(ratio.height)));
        }

        if (GetAbsoluteCoord(styleMaxHeight, h) ||
            GetPercentHeight(styleMaxHeight, aFrame, h)) {
          h = NSToCoordRound(h * (float(ratio.width) / float(ratio.height)));
          if (h < result)
            result = h;
        }

        if (GetAbsoluteCoord(styleMinHeight, h) ||
            GetPercentHeight(styleMinHeight, aFrame, h)) {
          h = NSToCoordRound(h * (float(ratio.width) / float(ratio.height)));
          if (h > result)
            result = h;
        }
      }
    }
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
    result = NSCoordSaturatingAdd(result, coordOutsideWidth);
    pctTotal += pctOutsideWidth;

    coordOutsideWidth = 0;
    pctOutsideWidth = 0.0f;
  }

  coordOutsideWidth += offsets.hBorder;

  if (boxSizing == NS_STYLE_BOX_SIZING_BORDER) {
    min += coordOutsideWidth;
    result = NSCoordSaturatingAdd(result, coordOutsideWidth);
    pctTotal += pctOutsideWidth;

    coordOutsideWidth = 0;
    pctOutsideWidth = 0.0f;
  }

  coordOutsideWidth += offsets.hMargin;
  pctOutsideWidth += offsets.hPctMargin;

  min += coordOutsideWidth;
  result = NSCoordSaturatingAdd(result, coordOutsideWidth);
  pctTotal += pctOutsideWidth;

  nscoord w;
  if (GetAbsoluteCoord(styleWidth, w) ||
      GetIntrinsicCoord(styleWidth, aRenderingContext, aFrame,
                        PROP_WIDTH, w)) {
    result = AddPercents(aType, w + coordOutsideWidth, pctOutsideWidth);
  }
  else if (aType == MIN_WIDTH &&
           // The only cases of coord-percent-calc() units that
           // GetAbsoluteCoord didn't handle are percent and calc()s
           // containing percent.
           styleWidth.IsCoordPercentCalcUnit() &&
           aFrame->IsFrameOfType(nsIFrame::eReplaced)) {
    // A percentage width on replaced elements means they can shrink to 0.
    result = 0; // let |min| handle padding/border/margin
  }
  else {
    // NOTE: We could really do a lot better for percents and for some
    // cases of calc() containing percent (certainly including any where
    // the coefficient on the percent is positive and there are no max()
    // expressions).  However, doing better for percents wouldn't be
    // backwards compatible.
    result = AddPercents(aType, result, pctTotal);
  }

  if (haveFixedMaxWidth ||
      GetIntrinsicCoord(styleMaxWidth, aRenderingContext, aFrame,
                        PROP_MAX_WIDTH, maxw)) {
    maxw = AddPercents(aType, maxw + coordOutsideWidth, pctOutsideWidth);
    if (result > maxw)
      result = maxw;
  }

  if (haveFixedMinWidth ||
      GetIntrinsicCoord(styleMinWidth, aRenderingContext, aFrame,
                        PROP_MIN_WIDTH, minw)) {
    minw = AddPercents(aType, minw + coordOutsideWidth, pctOutsideWidth);
    if (result < minw)
      result = minw;
  }

  min = AddPercents(aType, min, pctTotal);
  if (result < min)
    result = min;

  const nsStyleDisplay *disp = aFrame->GetStyleDisplay();
  if (aFrame->IsThemed(disp)) {
    nsIntSize size(0, 0);
    PRBool canOverride = PR_TRUE;
    nsPresContext *presContext = aFrame->PresContext();
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
  static_cast<nsFrame*>(aFrame)->ListTag(stdout);
  printf(" %s intrinsic width for container is %d twips.\n",
         aType == MIN_WIDTH ? "min" : "pref", result);
#endif

  return result;
}

/* static */ nscoord
nsLayoutUtils::ComputeWidthDependentValue(
                 nscoord              aContainingBlockWidth,
                 const nsStyleCoord&  aCoord)
{
  NS_WARN_IF_FALSE(aContainingBlockWidth != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");

  if (aCoord.IsCoordPercentCalcUnit()) {
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, aContainingBlockWidth);
  }
  NS_ASSERTION(aCoord.GetUnit() == eStyleUnit_None ||
               aCoord.GetUnit() == eStyleUnit_Auto,
               "unexpected width value");
  return 0;
}

/* static */ nscoord
nsLayoutUtils::ComputeWidthValue(
                 nsIRenderingContext* aRenderingContext,
                 nsIFrame*            aFrame,
                 nscoord              aContainingBlockWidth,
                 nscoord              aContentEdgeToBoxSizing,
                 nscoord              aBoxSizingToMarginEdge,
                 const nsStyleCoord&  aCoord)
{
  NS_PRECONDITION(aFrame, "non-null frame expected");
  NS_PRECONDITION(aRenderingContext, "non-null rendering context expected");
  NS_WARN_IF_FALSE(aContainingBlockWidth != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  NS_PRECONDITION(aContainingBlockWidth >= 0,
                  "width less than zero");

  nscoord result;
  if (aCoord.IsCoordPercentCalcUnit()) {
    result = nsRuleNode::ComputeCoordPercentCalc(aCoord, aContainingBlockWidth);
    // The result of a calc() expression might be less than 0; we
    // should clamp at runtime (below).  (Percentages and coords that
    // are less than 0 have already been dropped by the parser.)
    result -= aContentEdgeToBoxSizing;
  } else if (eStyleUnit_Enumerated == aCoord.GetUnit()) {
    PRInt32 val = aCoord.GetIntValue();
    switch (val) {
      case NS_STYLE_WIDTH_MAX_CONTENT:
        result = aFrame->GetPrefWidth(aRenderingContext);
        NS_ASSERTION(result >= 0, "width less than zero");
        break;
      case NS_STYLE_WIDTH_MIN_CONTENT:
        result = aFrame->GetMinWidth(aRenderingContext);
        NS_ASSERTION(result >= 0, "width less than zero");
        break;
      case NS_STYLE_WIDTH_FIT_CONTENT:
        {
          nscoord pref = aFrame->GetPrefWidth(aRenderingContext),
                   min = aFrame->GetMinWidth(aRenderingContext),
                  fill = aContainingBlockWidth -
                         (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
          result = NS_MAX(min, NS_MIN(pref, fill));
          NS_ASSERTION(result >= 0, "width less than zero");
        }
        break;
      case NS_STYLE_WIDTH_AVAILABLE:
        result = aContainingBlockWidth -
                 (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
    }
  } else {
    NS_NOTREACHED("unexpected width value");
    result = 0;
  }
  if (result < 0)
    result = 0;
  return result;
}


/* static */ nscoord
nsLayoutUtils::ComputeHeightDependentValue(
                 nscoord              aContainingBlockHeight,
                 const nsStyleCoord&  aCoord)
{
  // XXXldb Some callers explicitly check aContainingBlockHeight
  // against NS_AUTOHEIGHT *and* unit against eStyleUnit_Percent or
  // calc()s containing percents before calling this function.
  // However, it would be much more likely to catch problems without
  // the unit conditions.
  // XXXldb Many callers pass a non-'auto' containing block height when
  // according to CSS2.1 they should be passing 'auto'.
  NS_PRECONDITION(NS_AUTOHEIGHT != aContainingBlockHeight ||
                  !aCoord.HasPercent(),
                  "unexpected containing block height");

  if (aCoord.IsCoordPercentCalcUnit()) {
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, aContainingBlockHeight);
  }

  NS_ASSERTION(aCoord.GetUnit() == eStyleUnit_None ||
               aCoord.GetUnit() == eStyleUnit_Auto,
               "unexpected height value");
  return 0;
}

#define MULDIV(a,b,c) (nscoord(PRInt64(a) * PRInt64(b) / PRInt64(c)))

/* static */ nsSize
nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                   nsIRenderingContext* aRenderingContext, nsIFrame* aFrame,
                   const nsIFrame::IntrinsicSize& aIntrinsicSize,
                   nsSize aIntrinsicRatio, nsSize aCBSize,
                   nsSize aMargin, nsSize aBorder, nsSize aPadding)
{
  const nsStylePosition *stylePos = aFrame->GetStylePosition();
  // Handle intrinsic sizes and their interaction with
  // {min-,max-,}{width,height} according to the rules in
  // http://www.w3.org/TR/CSS21/visudet.html#min-max-widths

  // Note: throughout the following section of the function, I avoid
  // a * (b / c) because of its reduced accuracy relative to a * b / c
  // or (a * b) / c (which are equivalent).

  const PRBool isAutoWidth = stylePos->mWidth.GetUnit() == eStyleUnit_Auto;
  const PRBool isAutoHeight = IsAutoHeight(stylePos->mHeight, aCBSize.height);

  nsSize boxSizingAdjust(0,0);
  switch (stylePos->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      boxSizingAdjust += aBorder;
      // fall through
    case NS_STYLE_BOX_SIZING_PADDING:
      boxSizingAdjust += aPadding;
  }
  nscoord boxSizingToMarginEdgeWidth =
    aMargin.width + aBorder.width + aPadding.width - boxSizingAdjust.width;

  nscoord width, minWidth, maxWidth, height, minHeight, maxHeight;

  if (!isAutoWidth) {
    width = nsLayoutUtils::ComputeWidthValue(aRenderingContext,
              aFrame, aCBSize.width, boxSizingAdjust.width,
              boxSizingToMarginEdgeWidth, stylePos->mWidth);
    NS_ASSERTION(width >= 0, "negative result from ComputeWidthValue");
  }

  if (stylePos->mMaxWidth.GetUnit() != eStyleUnit_None) {
    maxWidth = nsLayoutUtils::ComputeWidthValue(aRenderingContext,
                 aFrame, aCBSize.width, boxSizingAdjust.width,
                 boxSizingToMarginEdgeWidth, stylePos->mMaxWidth);
    NS_ASSERTION(maxWidth >= 0, "negative result from ComputeWidthValue");
  } else {
    maxWidth = nscoord_MAX;
  }

  minWidth = nsLayoutUtils::ComputeWidthValue(aRenderingContext,
               aFrame, aCBSize.width, boxSizingAdjust.width,
               boxSizingToMarginEdgeWidth, stylePos->mMinWidth);
  NS_ASSERTION(minWidth >= 0, "negative result from ComputeWidthValue");

  if (!isAutoHeight) {
    height = nsLayoutUtils::
      ComputeHeightValue(aCBSize.height, stylePos->mHeight) -
      boxSizingAdjust.height;
    if (height < 0)
      height = 0;
  }

  if (!IsAutoHeight(stylePos->mMaxHeight, aCBSize.height)) {
    maxHeight = nsLayoutUtils::
      ComputeHeightValue(aCBSize.height, stylePos->mMaxHeight) -
      boxSizingAdjust.height;
    if (maxHeight < 0)
      maxHeight = 0;
  } else {
    maxHeight = nscoord_MAX;
  }

  if (!IsAutoHeight(stylePos->mMinHeight, aCBSize.height)) {
    minHeight = nsLayoutUtils::
      ComputeHeightValue(aCBSize.height, stylePos->mMinHeight) -
      boxSizingAdjust.height;
    if (minHeight < 0)
      minHeight = 0;
  } else {
    minHeight = 0;
  }

  // Resolve percentage intrinsic width/height as necessary:

  NS_ASSERTION(aCBSize.width != NS_UNCONSTRAINEDSIZE,
               "Our containing block must not have unconstrained width!");

  PRBool hasIntrinsicWidth, hasIntrinsicHeight;
  nscoord intrinsicWidth, intrinsicHeight;

  if (aIntrinsicSize.width.GetUnit() == eStyleUnit_Coord ||
      aIntrinsicSize.width.GetUnit() == eStyleUnit_Percent) {
    hasIntrinsicWidth = PR_TRUE;
    intrinsicWidth = nsLayoutUtils::ComputeWidthValue(aRenderingContext,
                           aFrame, aCBSize.width, 0, boxSizingAdjust.width +
                           boxSizingToMarginEdgeWidth, aIntrinsicSize.width);
  } else {
    hasIntrinsicWidth = PR_FALSE;
    intrinsicWidth = 0;
  }

  if (aIntrinsicSize.height.GetUnit() == eStyleUnit_Coord ||
      (aIntrinsicSize.height.GetUnit() == eStyleUnit_Percent &&
       aCBSize.height != NS_AUTOHEIGHT)) {
    hasIntrinsicHeight = PR_TRUE;
    intrinsicHeight = nsLayoutUtils::
      ComputeHeightDependentValue(aCBSize.height, aIntrinsicSize.height);
    if (intrinsicHeight < 0)
      intrinsicHeight = 0;
  } else {
    hasIntrinsicHeight = PR_FALSE;
    intrinsicHeight = 0;
  }

  NS_ASSERTION(aIntrinsicRatio.width >= 0 && aIntrinsicRatio.height >= 0,
               "Intrinsic ratio has a negative component!");

  // Now calculate the used values for width and height:

  if (isAutoWidth) {
    if (isAutoHeight) {

      // 'auto' width, 'auto' height

      // Get tentative values - CSS 2.1 sections 10.3.2 and 10.6.2:

      nscoord tentWidth, tentHeight;

      if (hasIntrinsicWidth) {
        tentWidth = intrinsicWidth;
      } else if (hasIntrinsicHeight && aIntrinsicRatio.height > 0) {
        tentWidth = MULDIV(intrinsicHeight, aIntrinsicRatio.width, aIntrinsicRatio.height);
      } else if (aIntrinsicRatio.width > 0) {
        tentWidth = aCBSize.width - boxSizingToMarginEdgeWidth; // XXX scrollbar?
        if (tentWidth < 0) tentWidth = 0;
      } else {
        tentWidth = nsPresContext::CSSPixelsToAppUnits(300);
      }

      if (hasIntrinsicHeight) {
        tentHeight = intrinsicHeight;
      } else if (aIntrinsicRatio.width > 0) {
        tentHeight = MULDIV(tentWidth, aIntrinsicRatio.height, aIntrinsicRatio.width);
      } else {
        tentHeight = nsPresContext::CSSPixelsToAppUnits(150);
      }

      // Now apply min/max-width/height - CSS 2.1 sections 10.4 and 10.7:

      if (minWidth > maxWidth)
        maxWidth = minWidth;
      if (minHeight > maxHeight)
        maxHeight = minHeight;

      nscoord heightAtMaxWidth, heightAtMinWidth,
              widthAtMaxHeight, widthAtMinHeight;

      if (tentWidth > 0) {
        heightAtMaxWidth = MULDIV(maxWidth, tentHeight, tentWidth);
        if (heightAtMaxWidth < minHeight)
          heightAtMaxWidth = minHeight;
        heightAtMinWidth = MULDIV(minWidth, tentHeight, tentWidth);
        if (heightAtMinWidth > maxHeight)
          heightAtMinWidth = maxHeight;
      } else {
        heightAtMaxWidth = tentHeight;
        heightAtMinWidth = tentHeight;
      }

      if (tentHeight > 0) {
        widthAtMaxHeight = MULDIV(maxHeight, tentWidth, tentHeight);
        if (widthAtMaxHeight < minWidth)
          widthAtMaxHeight = minWidth;
        widthAtMinHeight = MULDIV(minHeight, tentWidth, tentHeight);
        if (widthAtMinHeight > maxWidth)
          widthAtMinHeight = maxWidth;
      } else {
        widthAtMaxHeight = tentWidth;
        widthAtMinHeight = tentWidth;
      }

      // The table at http://www.w3.org/TR/CSS21/visudet.html#min-max-widths :

      if (tentWidth > maxWidth) {
        if (tentHeight > maxHeight) {
          if (PRInt64(maxWidth) * PRInt64(tentHeight) <=
              PRInt64(maxHeight) * PRInt64(tentWidth)) {
            width = maxWidth;
            height = heightAtMaxWidth;
          } else {
            width = widthAtMaxHeight;
            height = maxHeight;
          }
        } else {
          // This also covers "(w > max-width) and (h < min-height)" since in
          // that case (max-width/w < 1), and with (h < min-height):
          //   max(max-width * h/w, min-height) == min-height
          width = maxWidth;
          height = heightAtMaxWidth;
        }
      } else if (tentWidth < minWidth) {
        if (tentHeight < minHeight) {
          if (PRInt64(minWidth) * PRInt64(tentHeight) <=
              PRInt64(minHeight) * PRInt64(tentWidth)) {
            width = widthAtMinHeight;
            height = minHeight;
          } else {
            width = minWidth;
            height = heightAtMinWidth;
          }
        } else {
          // This also covers "(w < min-width) and (h > max-height)" since in
          // that case (min-width/w > 1), and with (h > max-height):
          //   min(min-width * h/w, max-height) == max-height
          width = minWidth;
          height = heightAtMinWidth;
        }
      } else {
        if (tentHeight > maxHeight) {
          width = widthAtMaxHeight;
          height = maxHeight;
        } else if (tentHeight < minHeight) {
          width = widthAtMinHeight;
          height = minHeight;
        } else {
          width = tentWidth;
          height = tentHeight;
        }
      }

    } else {

      // 'auto' width, non-'auto' height
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);
      if (aIntrinsicRatio.height > 0) {
        width = MULDIV(height, aIntrinsicRatio.width, aIntrinsicRatio.height);
      } else if (hasIntrinsicWidth) {
        width = intrinsicWidth;
      } else {
        width = nsPresContext::CSSPixelsToAppUnits(300);
      }
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);

    }
  } else {
    if (isAutoHeight) {

      // non-'auto' width, 'auto' height
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);
      if (aIntrinsicRatio.width > 0) {
        height = MULDIV(width, aIntrinsicRatio.height, aIntrinsicRatio.width);
      } else if (hasIntrinsicHeight) {
        height = intrinsicHeight;
      } else {
        height = nsPresContext::CSSPixelsToAppUnits(150);
      }
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);

    } else {

      // non-'auto' width, non-'auto' height
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);

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
  data.ForceBreak(aRenderingContext);
  return data.prevLines;
}

/* static */ nscoord
nsLayoutUtils::PrefWidthFromInline(nsIFrame* aFrame,
                                   nsIRenderingContext* aRenderingContext)
{
  nsIFrame::InlinePrefWidthData data;
  DISPLAY_PREF_WIDTH(aFrame, data.prevLines);
  aFrame->AddInlinePrefWidth(aRenderingContext, &data);
  data.ForceBreak(aRenderingContext);
  return data.prevLines;
}

void
nsLayoutUtils::DrawString(const nsIFrame*      aFrame,
                          nsIRenderingContext* aContext,
                          const PRUnichar*     aString,
                          PRInt32              aLength,
                          nsPoint              aPoint,
                          PRUint8              aDirection)
{
#ifdef IBMBIDI
  nsresult rv = NS_ERROR_FAILURE;
  nsPresContext* presContext = aFrame->PresContext();
  if (presContext->BidiEnabled()) {
    nsBidiPresUtils* bidiUtils = presContext->GetBidiUtils();

    if (bidiUtils) {
      if (aDirection == NS_STYLE_DIRECTION_INHERIT) {
        aDirection = aFrame->GetStyleVisibility()->mDirection;
      }
      nsBidiDirection direction =
        (NS_STYLE_DIRECTION_RTL == aDirection) ?
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
  nsPresContext* presContext = aFrame->PresContext();
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
#endif // IBMBIDI
  aContext->SetTextRunRTL(PR_FALSE);
  nscoord width;
  aContext->GetWidth(aString, aLength, width);
  return width;
}

/* static */ nscoord
nsLayoutUtils::GetCenteredFontBaseline(nsIFontMetrics* aFontMetrics,
                                       nscoord         aLineHeight)
{
  nscoord fontAscent, fontHeight;
  aFontMetrics->GetMaxAscent(fontAscent);
  aFontMetrics->GetHeight(fontHeight);

  nscoord leading = aLineHeight - fontHeight;
  return fontAscent + leading/2;
}


/* static */ PRBool
nsLayoutUtils::GetFirstLineBaseline(const nsIFrame* aFrame, nscoord* aResult)
{
  LinePosition position;
  if (!GetFirstLinePosition(aFrame, &position))
    return PR_FALSE;
  *aResult = position.mBaseline;
  return PR_TRUE;
}

/* static */ PRBool
nsLayoutUtils::GetFirstLinePosition(const nsIFrame* aFrame,
                                    LinePosition* aResult)
{
  const nsBlockFrame* block = nsLayoutUtils::GetAsBlock(const_cast<nsIFrame*>(aFrame));
  if (!block) {
    // For the first-line baseline we also have to check for a table, and if
    // so, use the baseline of its first row.
    nsIAtom* fType = aFrame->GetType();
    if (fType == nsGkAtoms::tableOuterFrame) {
      aResult->mTop = 0;
      aResult->mBaseline = aFrame->GetBaseline();
      // This is what we want for the list bullet caller; not sure if
      // other future callers will want the same.
      aResult->mBottom = aFrame->GetSize().height;
      return PR_TRUE;
    }

    // For first-line baselines, we have to consider scroll frames.
    if (fType == nsGkAtoms::scrollFrame) {
      nsIScrollableFrame *sFrame = do_QueryFrame(const_cast<nsIFrame*>(aFrame));
      if (!sFrame) {
        NS_NOTREACHED("not scroll frame");
      }
      LinePosition kidPosition;
      if (GetFirstLinePosition(sFrame->GetScrolledFrame(), &kidPosition)) {
        // Consider only the border and padding that contributes to the
        // kid's position, not the scrolling, so we get the initial
        // position.
        *aResult = kidPosition + aFrame->GetUsedBorderAndPadding().top;
        return PR_TRUE;
      }
      return PR_FALSE;
    }

    if (fType == nsGkAtoms::fieldSetFrame) {
      LinePosition kidPosition;
      nsIFrame* kid = aFrame->GetFirstChild(nsnull);
      // kid might be a legend frame here, but that's ok.
      if (GetFirstLinePosition(kid, &kidPosition)) {
        *aResult = kidPosition + kid->GetPosition().y;
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
      LinePosition kidPosition;
      if (GetFirstLinePosition(kid, &kidPosition)) {
        *aResult = kidPosition + kid->GetPosition().y;
        return PR_TRUE;
      }
    } else {
      // XXX Is this the right test?  We have some bogus empty lines
      // floating around, but IsEmpty is perhaps too weak.
      if (line->GetHeight() != 0 || !line->IsEmpty()) {
        nscoord top = line->mBounds.y;
        aResult->mTop = top;
        aResult->mBaseline = top + line->GetAscent();
        aResult->mBottom = top + line->GetHeight();
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

/* static */ PRBool
nsLayoutUtils::GetLastLineBaseline(const nsIFrame* aFrame, nscoord* aResult)
{
  const nsBlockFrame* block = nsLayoutUtils::GetAsBlock(const_cast<nsIFrame*>(aFrame));
  if (!block)
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

static nscoord
CalculateBlockContentBottom(nsBlockFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");

  nscoord contentBottom = 0;

  for (nsBlockFrame::line_iterator line = aFrame->begin_lines(),
                                   line_end = aFrame->end_lines();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame* child = line->mFirstChild;
      nscoord offset = child->GetRect().y - child->GetRelativeOffset().y;
      contentBottom = NS_MAX(contentBottom,
                        nsLayoutUtils::CalculateContentBottom(child) + offset);
    }
    else {
      contentBottom = NS_MAX(contentBottom, line->mBounds.YMost());
    }
  }
  return contentBottom;
}

/* static */ nscoord
nsLayoutUtils::CalculateContentBottom(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");

  nscoord contentBottom = aFrame->GetRect().height;

  if (aFrame->GetOverflowRect().height > contentBottom) {
    nsBlockFrame* blockFrame = GetAsBlock(aFrame);
    nsIAtom* childList = nsnull;
    PRIntn nextListID = 0;
    do {
      if (childList == nsnull && blockFrame) {
        contentBottom = NS_MAX(contentBottom, CalculateBlockContentBottom(blockFrame));
      }
      else if (childList != nsGkAtoms::overflowList &&
               childList != nsGkAtoms::excessOverflowContainersList &&
               childList != nsGkAtoms::overflowOutOfFlowList)
      {
        for (nsIFrame* child = aFrame->GetFirstChild(childList);
            child; child = child->GetNextSibling())
        {
          nscoord offset = child->GetRect().y - child->GetRelativeOffset().y;
          contentBottom = NS_MAX(contentBottom,
                                 CalculateContentBottom(child) + offset);
        }
      }

      childList = aFrame->GetAdditionalChildListName(nextListID);
      nextListID++;
    } while (childList);
  }

  return contentBottom;
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
  if (layer)
    return layer;
  return aFrame->PresContext()->PresShell()->FrameManager()->GetRootFrame();
}

gfxPattern::GraphicsFilter
nsLayoutUtils::GetGraphicsFilterForFrame(nsIFrame* aForFrame)
{
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  gfxPattern::GraphicsFilter defaultFilter = gfxPattern::FILTER_NEAREST;
#else
  gfxPattern::GraphicsFilter defaultFilter = gfxPattern::FILTER_GOOD;
#endif
#ifdef MOZ_SVG
  nsIFrame *frame = nsCSSRendering::IsCanvasFrame(aForFrame) ?
    nsCSSRendering::FindBackgroundStyleFrame(aForFrame) : aForFrame;

  switch (frame->GetStyleSVG()->mImageRendering) {
  case NS_STYLE_IMAGE_RENDERING_OPTIMIZESPEED:
    return gfxPattern::FILTER_FAST;
  case NS_STYLE_IMAGE_RENDERING_OPTIMIZEQUALITY:
    return gfxPattern::FILTER_BEST;
  case NS_STYLE_IMAGE_RENDERING_CRISPEDGES:
    return gfxPattern::FILTER_NEAREST;
  default:
    return defaultFilter;
  }
#else
  return defaultFilter;
#endif
}

/**
 * Given an image being drawn into an appunit coordinate system, and
 * a point in that coordinate system, map the point back into image
 * pixel space.
 * @param aSize the size of the image, in pixels
 * @param aDest the rectangle that the image is being mapped into
 * @param aPt a point in the same coordinate system as the rectangle
 */
static gfxPoint
MapToFloatImagePixels(const gfxSize& aSize,
                      const gfxRect& aDest, const gfxPoint& aPt)
{
  return gfxPoint(((aPt.x - aDest.pos.x)*aSize.width)/aDest.size.width,
                  ((aPt.y - aDest.pos.y)*aSize.height)/aDest.size.height);
}

/**
 * Given an image being drawn into an pixel-based coordinate system, and
 * a point in image space, map the point into the pixel-based coordinate
 * system.
 * @param aSize the size of the image, in pixels
 * @param aDest the rectangle that the image is being mapped into
 * @param aPt a point in image space
 */
static gfxPoint
MapToFloatUserPixels(const gfxSize& aSize,
                     const gfxRect& aDest, const gfxPoint& aPt)
{
  return gfxPoint(aPt.x*aDest.size.width/aSize.width + aDest.pos.x,
                  aPt.y*aDest.size.height/aSize.height + aDest.pos.y);
}

/* static */ gfxRect
nsLayoutUtils::RectToGfxRect(const nsRect& aRect, PRInt32 aAppUnitsPerDevPixel)
{
  return gfxRect(gfxFloat(aRect.x) / aAppUnitsPerDevPixel,
                 gfxFloat(aRect.y) / aAppUnitsPerDevPixel,
                 gfxFloat(aRect.width) / aAppUnitsPerDevPixel,
                 gfxFloat(aRect.height) / aAppUnitsPerDevPixel);
}

struct SnappedImageDrawingParameters {
  // A transform from either device space or user space (depending on mResetCTM)
  // to image space
  gfxMatrix mUserSpaceToImageSpace;
  // A device-space, pixel-aligned rectangle to fill
  gfxRect mFillRect;
  // A pixel rectangle in tiled image space outside of which gfx should not
  // sample (using EXTEND_PAD as necessary)
  nsIntRect mSubimage;
  // Whether there's anything to draw at all
  PRPackedBool mShouldDraw;
  // PR_TRUE iff the CTM of the rendering context needs to be reset to the
  // identity matrix before drawing
  PRPackedBool mResetCTM;

  SnappedImageDrawingParameters()
   : mShouldDraw(PR_FALSE)
   , mResetCTM(PR_FALSE)
  {}

  SnappedImageDrawingParameters(const gfxMatrix& aUserSpaceToImageSpace,
                                const gfxRect&   aFillRect,
                                const nsIntRect& aSubimage,
                                PRBool           aResetCTM)
   : mUserSpaceToImageSpace(aUserSpaceToImageSpace)
   , mFillRect(aFillRect)
   , mSubimage(aSubimage)
   , mShouldDraw(PR_TRUE)
   , mResetCTM(aResetCTM)
  {}
};

/**
 * Given a set of input parameters, compute certain output parameters
 * for drawing an image with the image snapping algorithm.
 * See https://wiki.mozilla.org/Gecko:Image_Snapping_and_Rendering
 *
 *  @see nsLayoutUtils::DrawImage() for the descriptions of input parameters
 */
static SnappedImageDrawingParameters
ComputeSnappedImageDrawingParameters(gfxContext*     aCtx,
                                     PRInt32         aAppUnitsPerDevPixel,
                                     const nsRect    aDest,
                                     const nsRect    aFill,
                                     const nsPoint   aAnchor,
                                     const nsRect    aDirty,
                                     const nsIntSize aImageSize)

{
  if (aDest.IsEmpty() || aFill.IsEmpty())
    return SnappedImageDrawingParameters();

  gfxRect devPixelDest =
    nsLayoutUtils::RectToGfxRect(aDest, aAppUnitsPerDevPixel);
  gfxRect devPixelFill =
    nsLayoutUtils::RectToGfxRect(aFill, aAppUnitsPerDevPixel);
  gfxRect devPixelDirty =
    nsLayoutUtils::RectToGfxRect(aDirty, aAppUnitsPerDevPixel);

  PRBool ignoreScale = PR_FALSE;
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  ignoreScale = PR_TRUE;
#endif
  gfxRect fill = devPixelFill;
  PRBool didSnap = aCtx->UserToDevicePixelSnapped(fill, ignoreScale);

  gfxSize imageSize(aImageSize.width, aImageSize.height);

  // Compute the set of pixels that would be sampled by an ideal rendering
  gfxPoint subimageTopLeft =
    MapToFloatImagePixels(imageSize, devPixelDest, devPixelFill.TopLeft());
  gfxPoint subimageBottomRight =
    MapToFloatImagePixels(imageSize, devPixelDest, devPixelFill.BottomRight());
  nsIntRect intSubimage;
  intSubimage.MoveTo(NSToIntFloor(subimageTopLeft.x),
                     NSToIntFloor(subimageTopLeft.y));
  intSubimage.SizeTo(NSToIntCeil(subimageBottomRight.x) - intSubimage.x,
                     NSToIntCeil(subimageBottomRight.y) - intSubimage.y);

  // Compute the anchor point and compute final fill rect.
  // This code assumes that pixel-based devices have one pixel per
  // device unit!
  gfxPoint anchorPoint(gfxFloat(aAnchor.x)/aAppUnitsPerDevPixel,
                       gfxFloat(aAnchor.y)/aAppUnitsPerDevPixel);
  gfxPoint imageSpaceAnchorPoint =
    MapToFloatImagePixels(imageSize, devPixelDest, anchorPoint);
  gfxMatrix currentMatrix = aCtx->CurrentMatrix();

  if (didSnap) {
    NS_ASSERTION(!currentMatrix.HasNonAxisAlignedTransform(),
                 "How did we snap, then?");
    imageSpaceAnchorPoint.Round();
    anchorPoint = imageSpaceAnchorPoint;
    anchorPoint = MapToFloatUserPixels(imageSize, devPixelDest, anchorPoint);
    anchorPoint = currentMatrix.Transform(anchorPoint);
    anchorPoint.Round();

    // This form of Transform is safe to call since non-axis-aligned
    // transforms wouldn't be snapped.
    devPixelDirty = currentMatrix.Transform(devPixelDirty);
  }

  gfxFloat scaleX = imageSize.width*aAppUnitsPerDevPixel/aDest.width;
  gfxFloat scaleY = imageSize.height*aAppUnitsPerDevPixel/aDest.height;
  if (didSnap) {
    // We'll reset aCTX to the identity matrix before drawing, so we need to
    // adjust our scales to match.
    scaleX /= currentMatrix.xx;
    scaleY /= currentMatrix.yy;
  }
  gfxFloat translateX = imageSpaceAnchorPoint.x - anchorPoint.x*scaleX;
  gfxFloat translateY = imageSpaceAnchorPoint.y - anchorPoint.y*scaleY;
  gfxMatrix transform(scaleX, 0, 0, scaleY, translateX, translateY);

  gfxRect finalFillRect = fill;
  // If the user-space-to-image-space transform is not a straight
  // translation by integers, then filtering will occur, and
  // restricting the fill rect to the dirty rect would change the values
  // computed for edge pixels, which we can't allow.
  // Also, if didSnap is false then rounding out 'devPixelDirty' might not
  // produce pixel-aligned coordinates, which would also break the values
  // computed for edge pixels.
  if (didSnap && !transform.HasNonIntegerTranslation()) {
    devPixelDirty.RoundOut();
    finalFillRect = fill.Intersect(devPixelDirty);
  }
  if (finalFillRect.IsEmpty())
    return SnappedImageDrawingParameters();

  return SnappedImageDrawingParameters(transform, finalFillRect, intSubimage,
                                       didSnap);
}


static nsresult
DrawImageInternal(nsIRenderingContext* aRenderingContext,
                  imgIContainer*       aImage,
                  gfxPattern::GraphicsFilter aGraphicsFilter,
                  const nsRect&        aDest,
                  const nsRect&        aFill,
                  const nsPoint&       aAnchor,
                  const nsRect&        aDirty,
                  const nsIntSize&     aImageSize,
                  PRUint32             aImageFlags)
{
  nsCOMPtr<nsIDeviceContext> dc;
  aRenderingContext->GetDeviceContext(*getter_AddRefs(dc));
  PRInt32 appUnitsPerDevPixel = dc->AppUnitsPerDevPixel();
  gfxContext* ctx = aRenderingContext->ThebesContext();

  SnappedImageDrawingParameters drawingParams =
    ComputeSnappedImageDrawingParameters(ctx, appUnitsPerDevPixel, aDest, aFill,
                                         aAnchor, aDirty, aImageSize);

  if (!drawingParams.mShouldDraw)
    return NS_OK;

  gfxContextMatrixAutoSaveRestore saveMatrix(ctx);
  if (drawingParams.mResetCTM) {
    ctx->IdentityMatrix();
  }

  aImage->Draw(ctx, aGraphicsFilter, drawingParams.mUserSpaceToImageSpace,
               drawingParams.mFillRect, drawingParams.mSubimage, aImageSize,
               aImageFlags);
  return NS_OK;
}

/* static */ void
nsLayoutUtils::DrawPixelSnapped(nsIRenderingContext* aRenderingContext,
                                gfxDrawable*         aDrawable,
                                gfxPattern::GraphicsFilter aFilter,
                                const nsRect&        aDest,
                                const nsRect&        aFill,
                                const nsPoint&       aAnchor,
                                const nsRect&        aDirty)
{
  nsCOMPtr<nsIDeviceContext> dc;
  aRenderingContext->GetDeviceContext(*getter_AddRefs(dc));
  PRInt32 appUnitsPerDevPixel = dc->AppUnitsPerDevPixel();
  gfxContext* ctx = aRenderingContext->ThebesContext();
  gfxIntSize drawableSize = aDrawable->Size();
  nsIntSize imageSize(drawableSize.width, drawableSize.height);

  SnappedImageDrawingParameters drawingParams =
    ComputeSnappedImageDrawingParameters(ctx, appUnitsPerDevPixel, aDest, aFill,
                                         aAnchor, aDirty, imageSize);

  if (!drawingParams.mShouldDraw)
    return;

  gfxContextMatrixAutoSaveRestore saveMatrix(ctx);
  if (drawingParams.mResetCTM) {
    ctx->IdentityMatrix();
  }

  gfxRect sourceRect =
    drawingParams.mUserSpaceToImageSpace.Transform(drawingParams.mFillRect);
  gfxRect imageRect(0, 0, imageSize.width, imageSize.height);
  gfxRect subimage(drawingParams.mSubimage.x, drawingParams.mSubimage.y,
                   drawingParams.mSubimage.width, drawingParams.mSubimage.height);

  NS_ASSERTION(!sourceRect.Intersect(subimage).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");

  gfxUtils::DrawPixelSnapped(ctx, aDrawable,
                             drawingParams.mUserSpaceToImageSpace, subimage,
                             sourceRect, imageRect, drawingParams.mFillRect,
                             gfxASurface::ImageFormatARGB32, aFilter);
}

/* static */ nsresult
nsLayoutUtils::DrawSingleUnscaledImage(nsIRenderingContext* aRenderingContext,
                                       imgIContainer*       aImage,
                                       const nsPoint&       aDest,
                                       const nsRect&        aDirty,
                                       PRUint32             aImageFlags,
                                       const nsRect*        aSourceArea)
{
  nsIntSize imageSize;
  aImage->GetWidth(&imageSize.width);
  aImage->GetHeight(&imageSize.height);
  NS_ENSURE_TRUE(imageSize.width > 0 && imageSize.height > 0, NS_ERROR_FAILURE);

  nscoord appUnitsPerCSSPixel = nsIDeviceContext::AppUnitsPerCSSPixel();
  nsSize size(imageSize.width*appUnitsPerCSSPixel,
              imageSize.height*appUnitsPerCSSPixel);

  nsRect source;
  if (aSourceArea) {
    source = *aSourceArea;
  } else {
    source.SizeTo(size);
  }

  nsRect dest(aDest - source.TopLeft(), size);
  nsRect fill(aDest, source.Size());
  // Ensure that only a single image tile is drawn. If aSourceArea extends
  // outside the image bounds, we want to honor the aSourceArea-to-aDest
  // translation but we don't want to actually tile the image.
  fill.IntersectRect(fill, dest);
  return DrawImageInternal(aRenderingContext, aImage, gfxPattern::FILTER_NEAREST,
                           dest, fill, aDest, aDirty, imageSize, aImageFlags);
}

/* static */ nsresult
nsLayoutUtils::DrawSingleImage(nsIRenderingContext* aRenderingContext,
                               imgIContainer*       aImage,
                               gfxPattern::GraphicsFilter aGraphicsFilter,
                               const nsRect&        aDest,
                               const nsRect&        aDirty,
                               PRUint32             aImageFlags,
                               const nsRect*        aSourceArea)
{
  nsIntSize imageSize;
  if (aImage->GetType() == imgIContainer::TYPE_VECTOR) {
    imageSize.width  = nsPresContext::AppUnitsToIntCSSPixels(aDest.width);
    imageSize.height = nsPresContext::AppUnitsToIntCSSPixels(aDest.height);
  } else {
    aImage->GetWidth(&imageSize.width);
    aImage->GetHeight(&imageSize.height);
  }
  NS_ENSURE_TRUE(imageSize.width > 0 && imageSize.height > 0, NS_ERROR_FAILURE);

  nsRect source;
  if (aSourceArea) {
    source = *aSourceArea;
  } else {
    nscoord appUnitsPerCSSPixel = nsIDeviceContext::AppUnitsPerCSSPixel();
    source.SizeTo(imageSize.width*appUnitsPerCSSPixel,
                  imageSize.height*appUnitsPerCSSPixel);
  }

  nsRect dest = nsLayoutUtils::GetWholeImageDestination(imageSize, source,
                                                        aDest);
  // Ensure that only a single image tile is drawn. If aSourceArea extends
  // outside the image bounds, we want to honor the aSourceArea-to-aDest
  // transform but we don't want to actually tile the image.
  nsRect fill;
  fill.IntersectRect(aDest, dest);
  return DrawImageInternal(aRenderingContext, aImage, aGraphicsFilter, dest, fill,
                           fill.TopLeft(), aDirty, imageSize, aImageFlags);
}

/* static */ void
nsLayoutUtils::ComputeSizeForDrawing(imgIContainer *aImage,
                                     nsIntSize&     aImageSize, /*outparam*/
                                     PRBool&        aGotWidth,  /*outparam*/
                                     PRBool&        aGotHeight  /*outparam*/)
{
  aGotWidth  = NS_SUCCEEDED(aImage->GetWidth(&aImageSize.width));
  aGotHeight = NS_SUCCEEDED(aImage->GetHeight(&aImageSize.height));

  if ((aGotWidth && aGotHeight) ||    // Trivial success!
      (!aGotWidth && !aGotHeight)) {  // Trivial failure!
    return;
  }

  // If we get here, we succeeded at querying *either* the width *or* the
  // height, but not both.
  NS_ASSERTION(aImage->GetType() == imgIContainer::TYPE_VECTOR,
               "GetWidth and GetHeight should only fail for vector images");

  nsIFrame* rootFrame = aImage->GetRootLayoutFrame();
  NS_ASSERTION(rootFrame,
               "We should have a VectorImage, which should have a rootFrame");

  // This falls back on failure, if we somehow end up without a rootFrame.
  nsSize ratio = rootFrame ? rootFrame->GetIntrinsicRatio() : nsSize(0,0);
  if (!aGotWidth) { // Have height, missing width
    if (ratio.height != 0) { // don't divide by zero
      aImageSize.width = NSToCoordRound(aImageSize.height *
                                        float(ratio.width) /
                                        float(ratio.height));
      aGotWidth = PR_TRUE;
    }
  } else { // Have width, missing height
    if (ratio.width != 0) { // don't divide by zero
      aImageSize.height = NSToCoordRound(aImageSize.width *
                                         float(ratio.height) /
                                         float(ratio.width));
      aGotHeight = PR_TRUE;
    }
  }
}


/* static */ nsresult
nsLayoutUtils::DrawImage(nsIRenderingContext* aRenderingContext,
                         imgIContainer*       aImage,
                         gfxPattern::GraphicsFilter aGraphicsFilter,
                         const nsRect&        aDest,
                         const nsRect&        aFill,
                         const nsPoint&       aAnchor,
                         const nsRect&        aDirty,
                         PRUint32             aImageFlags)
{
  nsIntSize imageSize;
  PRBool gotHeight, gotWidth;
  ComputeSizeForDrawing(aImage, imageSize, gotWidth, gotHeight);

  // fallback size based on aFill.
  if (!gotWidth) {
    imageSize.width = nsPresContext::AppUnitsToIntCSSPixels(aFill.width);
  }
  if (!gotHeight) {
    imageSize.height = nsPresContext::AppUnitsToIntCSSPixels(aFill.height);
  }

  return DrawImageInternal(aRenderingContext, aImage, aGraphicsFilter,
                           aDest, aFill, aAnchor, aDirty,
                           imageSize, aImageFlags);
}

/* static */ nsRect
nsLayoutUtils::GetWholeImageDestination(const nsIntSize& aWholeImageSize,
                                        const nsRect& aImageSourceArea,
                                        const nsRect& aDestArea)
{
  double scaleX = double(aDestArea.width)/aImageSourceArea.width;
  double scaleY = double(aDestArea.height)/aImageSourceArea.height;
  nscoord destOffsetX = NSToCoordRound(aImageSourceArea.x*scaleX);
  nscoord destOffsetY = NSToCoordRound(aImageSourceArea.y*scaleY);
  nscoord appUnitsPerCSSPixel = nsIDeviceContext::AppUnitsPerCSSPixel();
  nscoord wholeSizeX = NSToCoordRound(aWholeImageSize.width*appUnitsPerCSSPixel*scaleX);
  nscoord wholeSizeY = NSToCoordRound(aWholeImageSize.height*appUnitsPerCSSPixel*scaleY);
  return nsRect(aDestArea.TopLeft() - nsPoint(destOffsetX, destOffsetY),
                nsSize(wholeSizeX, wholeSizeY));
}

void
nsLayoutUtils::SetFontFromStyle(nsIRenderingContext* aRC, nsStyleContext* aSC)
{
  const nsStyleFont* font = aSC->GetStyleFont();
  const nsStyleVisibility* visibility = aSC->GetStyleVisibility();

  aRC->SetFont(font->mFont, visibility->mLanguage,
               aSC->PresContext()->GetUserFontSet());
}

static PRBool NonZeroStyleCoord(const nsStyleCoord& aCoord)
{
  if (aCoord.IsCoordPercentCalcUnit()) {
    // Since negative results are clamped to 0, check > 0.
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, nscoord_MAX) > 0 ||
           nsRuleNode::ComputeCoordPercentCalc(aCoord, 0) > 0;
  }

  return PR_TRUE;
}

/* static */ PRBool
nsLayoutUtils::HasNonZeroCorner(const nsStyleCorners& aCorners)
{
  NS_FOR_CSS_HALF_CORNERS(corner) {
    if (NonZeroStyleCoord(aCorners.Get(corner)))
      return PR_TRUE;
  }
  return PR_FALSE;
}

// aCorner is a "full corner" value, i.e. NS_CORNER_TOP_LEFT etc
static PRBool IsCornerAdjacentToSide(PRUint8 aCorner, mozilla::css::Side aSide)
{
  PR_STATIC_ASSERT((int)NS_SIDE_TOP == NS_CORNER_TOP_LEFT);
  PR_STATIC_ASSERT((int)NS_SIDE_RIGHT == NS_CORNER_TOP_RIGHT);
  PR_STATIC_ASSERT((int)NS_SIDE_BOTTOM == NS_CORNER_BOTTOM_RIGHT);
  PR_STATIC_ASSERT((int)NS_SIDE_LEFT == NS_CORNER_BOTTOM_LEFT);
  PR_STATIC_ASSERT((int)NS_SIDE_TOP == ((NS_CORNER_TOP_RIGHT - 1)&3));
  PR_STATIC_ASSERT((int)NS_SIDE_RIGHT == ((NS_CORNER_BOTTOM_RIGHT - 1)&3));
  PR_STATIC_ASSERT((int)NS_SIDE_BOTTOM == ((NS_CORNER_BOTTOM_LEFT - 1)&3));
  PR_STATIC_ASSERT((int)NS_SIDE_LEFT == ((NS_CORNER_TOP_LEFT - 1)&3));

  return aSide == aCorner || aSide == ((aCorner - 1)&3);
}

/* static */ PRBool
nsLayoutUtils::HasNonZeroCornerOnSide(const nsStyleCorners& aCorners,
                                      mozilla::css::Side aSide)
{
  PR_STATIC_ASSERT(NS_CORNER_TOP_LEFT_X/2 == NS_CORNER_TOP_LEFT);
  PR_STATIC_ASSERT(NS_CORNER_TOP_LEFT_Y/2 == NS_CORNER_TOP_LEFT);
  PR_STATIC_ASSERT(NS_CORNER_TOP_RIGHT_X/2 == NS_CORNER_TOP_RIGHT);
  PR_STATIC_ASSERT(NS_CORNER_TOP_RIGHT_Y/2 == NS_CORNER_TOP_RIGHT);
  PR_STATIC_ASSERT(NS_CORNER_BOTTOM_RIGHT_X/2 == NS_CORNER_BOTTOM_RIGHT);
  PR_STATIC_ASSERT(NS_CORNER_BOTTOM_RIGHT_Y/2 == NS_CORNER_BOTTOM_RIGHT);
  PR_STATIC_ASSERT(NS_CORNER_BOTTOM_LEFT_X/2 == NS_CORNER_BOTTOM_LEFT);
  PR_STATIC_ASSERT(NS_CORNER_BOTTOM_LEFT_Y/2 == NS_CORNER_BOTTOM_LEFT);

  NS_FOR_CSS_HALF_CORNERS(corner) {
    // corner is a "half corner" value, so dividing by two gives us a
    // "full corner" value.
    if (NonZeroStyleCoord(aCorners.Get(corner)) &&
        IsCornerAdjacentToSide(corner/2, aSide))
      return PR_TRUE;
  }
  return PR_FALSE;
}

/* static */ nsTransparencyMode
nsLayoutUtils::GetFrameTransparency(nsIFrame* aBackgroundFrame,
                                    nsIFrame* aCSSRootFrame) {
  if (aCSSRootFrame->GetStyleContext()->GetStyleDisplay()->mOpacity < 1.0f)
    return eTransparencyTransparent;

  if (HasNonZeroCorner(aCSSRootFrame->GetStyleContext()->GetStyleBorder()->mBorderRadius))
    return eTransparencyTransparent;

  nsITheme::Transparency transparency;
  if (aCSSRootFrame->IsThemed(&transparency))
    return transparency == nsITheme::eTransparent
         ? eTransparencyTransparent
         : eTransparencyOpaque;

  if (aCSSRootFrame->GetStyleDisplay()->mAppearance == NS_THEME_WIN_GLASS)
    return eTransparencyGlass;

  if (aCSSRootFrame->GetStyleDisplay()->mAppearance == NS_THEME_WIN_BORDERLESS_GLASS)
    return eTransparencyBorderlessGlass;

  // We need an uninitialized window to be treated as opaque because
  // doing otherwise breaks window display effects on some platforms,
  // specifically Vista. (bug 450322)
  if (aBackgroundFrame->GetType() == nsGkAtoms::viewportFrame &&
      !aBackgroundFrame->GetFirstChild(nsnull)) {
    return eTransparencyOpaque;
  }

  nsStyleContext* bgSC;
  if (!nsCSSRendering::FindBackground(aBackgroundFrame->PresContext(),
                                      aBackgroundFrame, &bgSC)) {
    return eTransparencyTransparent;
  }
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (NS_GET_A(bg->mBackgroundColor) < 255 ||
      // bottom layer's clip is used for the color
      bg->BottomLayer().mClip != NS_STYLE_BG_CLIP_BORDER)
    return eTransparencyTransparent;
  return eTransparencyOpaque;
}

/* static */ PRBool
nsLayoutUtils::IsPopup(nsIFrame* aFrame)
{
  nsIAtom* frameType = aFrame->GetType();

  // We're a popup if we're the list control frame dropdown for a combobox.
  if (frameType == nsGkAtoms::listControlFrame) {
    nsListControlFrame* listControlFrame = static_cast<nsListControlFrame*>(aFrame);

    if (listControlFrame) {
      return listControlFrame->IsInDropDownMode();
    }
  }

  // ... or if we're a XUL menupopup frame.
  return (frameType == nsGkAtoms::menuPopupFrame);
}

/* static */ nsIFrame*
nsLayoutUtils::GetDisplayRootFrame(nsIFrame* aFrame)
{
  nsIFrame* f = aFrame;
  for (;;) {
    if (IsPopup(f))
      return f;
    nsIFrame* parent = GetCrossDocParentFrame(f);
    if (!parent)
      return f;
    f = parent;
  }
}

static PRBool
IsNonzeroCoord(const nsStyleCoord& aCoord)
{
  if (eStyleUnit_Coord == aCoord.GetUnit())
    return aCoord.GetCoordValue() != 0;
  return PR_FALSE;
}

/* static */ PRUint32
nsLayoutUtils::GetTextRunFlagsForStyle(nsStyleContext* aStyleContext,
                                       const nsStyleText* aStyleText,
                                       const nsStyleFont* aStyleFont)
{
  PRUint32 result = 0;
  if (IsNonzeroCoord(aStyleText->mLetterSpacing)) {
    result |= gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES;
  }
#ifdef MOZ_SVG
  switch (aStyleContext->GetStyleSVG()->mTextRendering) {
  case NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED:
    result |= gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;
    break;
  case NS_STYLE_TEXT_RENDERING_AUTO:
    if (aStyleFont->mFont.size <
        aStyleContext->PresContext()->GetAutoQualityMinFontSize()) {
      result |= gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;
    }
    break;
  default:
    break;
  }
#endif
  return result;
}

/* static */ void
nsLayoutUtils::GetRectDifferenceStrips(const nsRect& aR1, const nsRect& aR2,
                                       nsRect* aHStrip, nsRect* aVStrip) {
  NS_ASSERTION(aR1.TopLeft() == aR2.TopLeft(),
               "expected rects at the same position");
  nsRect unionRect(aR1.x, aR1.y, NS_MAX(aR1.width, aR2.width),
                   NS_MAX(aR1.height, aR2.height));
  nscoord VStripStart = NS_MIN(aR1.width, aR2.width);
  nscoord HStripStart = NS_MIN(aR1.height, aR2.height);
  *aVStrip = unionRect;
  aVStrip->x += VStripStart;
  aVStrip->width -= VStripStart;
  *aHStrip = unionRect;
  aHStrip->y += HStripStart;
  aHStrip->height -= HStripStart;
}

nsIDeviceContext*
nsLayoutUtils::GetDeviceContextForScreenInfo(nsIDocShell* aDocShell)
{
  nsCOMPtr<nsIDocShell> docShell = aDocShell;
  while (docShell) {
    // Now make sure our size is up to date.  That will mean that the device
    // context does the right thing on multi-monitor systems when we return it to
    // the caller.  It will also make sure that our prescontext has been created,
    // if we're supposed to have one.
    nsCOMPtr<nsPIDOMWindow> win = do_GetInterface(docShell);
    if (!win) {
      // No reason to go on
      return nsnull;
    }

    win->EnsureSizeUpToDate();

    nsRefPtr<nsPresContext> presContext;
    docShell->GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
      nsIDeviceContext* context = presContext->DeviceContext();
      if (context) {
        return context;
      }
    }

    nsCOMPtr<nsIDocShellTreeItem> curItem = do_QueryInterface(docShell);
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    curItem->GetParent(getter_AddRefs(parentItem));
    docShell = do_QueryInterface(parentItem);
  }

  return nsnull;
}

/* static */ PRBool
nsLayoutUtils::IsReallyFixedPos(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame->GetParent(),
                  "IsReallyFixedPos called on frame not in tree");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->mPosition ==
                    NS_STYLE_POSITION_FIXED,
                  "IsReallyFixedPos called on non-'position:fixed' frame");

  nsIAtom *parentType = aFrame->GetParent()->GetType();
  return parentType == nsGkAtoms::viewportFrame ||
         parentType == nsGkAtoms::pageContentFrame;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(nsIDOMElement *aElement,
                                  PRUint32 aSurfaceFlags)
{
  SurfaceFromElementResult result;
  nsresult rv;

  nsCOMPtr<nsINode> node = do_QueryInterface(aElement);

  PRBool forceCopy = (aSurfaceFlags & SFE_WANT_NEW_SURFACE) != 0;
  PRBool wantImageSurface = (aSurfaceFlags & SFE_WANT_IMAGE_SURFACE) != 0;

  // If it's a <canvas>, we may be able to just grab its internal surface
  nsCOMPtr<nsIDOMHTMLCanvasElement> domCanvas = do_QueryInterface(aElement);
  if (node && domCanvas) {
    nsHTMLCanvasElement *canvas = static_cast<nsHTMLCanvasElement*>(domCanvas.get());
    nsIntSize nssize = canvas->GetSize();
    gfxIntSize size(nssize.width, nssize.height);

    nsRefPtr<gfxASurface> surf;

    if (!forceCopy && canvas->CountContexts() == 1) {
      nsICanvasRenderingContextInternal *srcCanvas = canvas->GetContextAtIndex(0);
      rv = srcCanvas->GetThebesSurface(getter_AddRefs(surf));

      if (NS_FAILED(rv))
        surf = nsnull;
    }

    if (surf && wantImageSurface && surf->GetType() != gfxASurface::SurfaceTypeImage)
      surf = nsnull;

    if (!surf) {
      if (wantImageSurface) {
        surf = new gfxImageSurface(size, gfxASurface::ImageFormatARGB32);
      } else {
        surf = gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, gfxASurface::CONTENT_COLOR_ALPHA);
      }

      nsRefPtr<gfxContext> ctx = new gfxContext(surf);
      // XXX shouldn't use the external interface, but maybe we can layerify this
      rv = (static_cast<nsICanvasElementExternal*>(canvas))->RenderContextsExternal(ctx, gfxPattern::FILTER_NEAREST);
      if (NS_FAILED(rv))
        return result;
    }

    nsCOMPtr<nsIPrincipal> principal = node->NodePrincipal();

    result.mSurface = surf;
    result.mSize = size;
    result.mPrincipal = node->NodePrincipal();
    result.mIsWriteOnly = canvas->IsWriteOnly();

    return result;
  }

#ifdef MOZ_MEDIA
  // Maybe it's <video>?
  nsCOMPtr<nsIDOMHTMLVideoElement> ve = do_QueryInterface(aElement);
  if (node && ve) {
    nsHTMLVideoElement *video = static_cast<nsHTMLVideoElement*>(ve.get());

    unsigned short readyState;
    if (NS_SUCCEEDED(ve->GetReadyState(&readyState)) &&
        (readyState == nsIDOMHTMLMediaElement::HAVE_NOTHING ||
         readyState == nsIDOMHTMLMediaElement::HAVE_METADATA)) {
      result.mIsStillLoading = PR_TRUE;
      return result;
    }

    // If it doesn't have a principal, just bail
    nsCOMPtr<nsIPrincipal> principal = video->GetCurrentPrincipal();
    if (!principal)
      return result;

    ImageContainer *container = video->GetImageContainer();
    if (!container)
      return result;

    gfxIntSize size;
    nsRefPtr<gfxASurface> surf = container->GetCurrentAsSurface(&size);
    if (!surf)
      return result;

    if (wantImageSurface && surf->GetType() != gfxASurface::SurfaceTypeImage) {
      nsRefPtr<gfxImageSurface> imgSurf =
        new gfxImageSurface(size, gfxASurface::ImageFormatARGB32);
      if (!imgSurf)
        return result;

      nsRefPtr<gfxContext> ctx = new gfxContext(imgSurf);
      if (!ctx)
        return result;
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
      ctx->DrawSurface(surf, size);
      surf = imgSurf;
    }

    result.mSurface = surf;
    result.mSize = size;
    result.mPrincipal = principal;
    result.mIsWriteOnly = PR_FALSE;

    return result;
  }
#endif

  // Finally, check if it's a normal image
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(aElement);

  if (!imageLoader)
    return result;

  nsCOMPtr<imgIRequest> imgRequest;
  rv = imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                               getter_AddRefs(imgRequest));
  if (NS_FAILED(rv) || !imgRequest)
    return result;

  PRUint32 status;
  imgRequest->GetImageStatus(&status);
  if ((status & imgIRequest::STATUS_LOAD_COMPLETE) == 0) {
    // Spec says to use GetComplete, but that only works on
    // nsIDOMHTMLImageElement, and we support all sorts of other stuff
    // here.  Do this for now pending spec clarification.
    result.mIsStillLoading = (status & imgIRequest::STATUS_ERROR) == 0;
    return result;
  }

  // In case of data: URIs, we want to ignore principals;
  // they should have the originating content's principal,
  // but that's broken at the moment in imgLib.
  nsCOMPtr<nsIURI> uri;
  rv = imgRequest->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return result;

  PRBool isDataURI = PR_FALSE;
  rv = uri->SchemeIs("data", &isDataURI);
  if (NS_FAILED(rv))
    return result;

  // Data URIs are always OK; set the principal
  // to null to indicate that.
  nsCOMPtr<nsIPrincipal> principal;
  if (!isDataURI) {
    rv = imgRequest->GetImagePrincipal(getter_AddRefs(principal));
    if (NS_FAILED(rv) || !principal)
      return result;
  }

  nsCOMPtr<imgIContainer> imgContainer;
  rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
  if (NS_FAILED(rv) || !imgContainer)
    return result;

  PRUint32 whichFrame = (aSurfaceFlags & SFE_WANT_FIRST_FRAME)
                        ? (PRUint32) imgIContainer::FRAME_FIRST
                        : (PRUint32) imgIContainer::FRAME_CURRENT;
  nsRefPtr<gfxASurface> framesurf;
  rv = imgContainer->GetFrame(whichFrame,
                              imgIContainer::FLAG_SYNC_DECODE,
                              getter_AddRefs(framesurf));
  if (NS_FAILED(rv))
    return result;

  PRInt32 imgWidth, imgHeight;
  rv = imgContainer->GetWidth(&imgWidth);
  rv |= imgContainer->GetHeight(&imgHeight);
  if (NS_FAILED(rv))
    return result;

  if (wantImageSurface && framesurf->GetType() != gfxASurface::SurfaceTypeImage) {
    forceCopy = PR_TRUE;
  }

  nsRefPtr<gfxASurface> gfxsurf = framesurf;
  if (forceCopy) {
    if (wantImageSurface) {
      gfxsurf = new gfxImageSurface (gfxIntSize(imgWidth, imgHeight), gfxASurface::ImageFormatARGB32);
    } else {
      gfxsurf = gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(imgWidth, imgHeight),
                                                                   gfxASurface::CONTENT_COLOR_ALPHA);
    }

    nsRefPtr<gfxContext> ctx = new gfxContext(gfxsurf);

    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->SetSource(framesurf);
    ctx->Paint();
  }

  result.mSurface = gfxsurf;
  result.mSize = gfxIntSize(imgWidth, imgHeight);
  result.mPrincipal = principal;
  result.mIsWriteOnly = PR_FALSE;

  return result;
}

/* static */
nsIContent*
nsLayoutUtils::GetEditableRootContentByContentEditable(nsIDocument* aDocument)
{
  // If the document is in designMode we should return NULL.
  if (!aDocument || aDocument->HasFlag(NODE_IS_EDITABLE)) {
    return nsnull;
  }

  // contenteditable only works with HTML document.
  // Note: Use nsIDOMHTMLDocument rather than nsIHTMLDocument for getting the
  //       body node because nsIDOMHTMLDocument::GetBody() does something
  //       additional work for some cases and nsEditor uses them.
  nsCOMPtr<nsIDOMHTMLDocument> domHTMLDoc = do_QueryInterface(aDocument);
  if (!domHTMLDoc) {
    return nsnull;
  }

  Element* rootElement = aDocument->GetRootElement();
  if (rootElement && rootElement->IsEditable()) {
    return rootElement;
  }

  // If there are no editable root element, check its <body> element.
  // Note that the body element could be <frameset> element.
  nsCOMPtr<nsIDOMHTMLElement> body;
  nsresult rv = domHTMLDoc->GetBody(getter_AddRefs(body));
  nsCOMPtr<nsIContent> content = do_QueryInterface(body);
  if (NS_SUCCEEDED(rv) && content && content->IsEditable()) {
    return content;
  }
  return nsnull;
}

nsSetAttrRunnable::nsSetAttrRunnable(nsIContent* aContent, nsIAtom* aAttrName,
                                     const nsAString& aValue)
  : mContent(aContent),
    mAttrName(aAttrName),
    mValue(aValue)
{
  NS_ASSERTION(aContent && aAttrName, "Missing stuff, prepare to crash");
}

nsSetAttrRunnable::nsSetAttrRunnable(nsIContent* aContent, nsIAtom* aAttrName,
                                     PRInt32 aValue)
  : mContent(aContent),
    mAttrName(aAttrName)
{
  NS_ASSERTION(aContent && aAttrName, "Missing stuff, prepare to crash");
  mValue.AppendInt(aValue);
}

NS_IMETHODIMP
nsSetAttrRunnable::Run()
{
  return mContent->SetAttr(kNameSpaceID_None, mAttrName, mValue, PR_TRUE);
}

nsUnsetAttrRunnable::nsUnsetAttrRunnable(nsIContent* aContent,
                                         nsIAtom* aAttrName)
  : mContent(aContent),
    mAttrName(aAttrName)
{
  NS_ASSERTION(aContent && aAttrName, "Missing stuff, prepare to crash");
}

NS_IMETHODIMP
nsUnsetAttrRunnable::Run()
{
  return mContent->UnsetAttr(kNameSpaceID_None, mAttrName, PR_TRUE);
}

nsReflowFrameRunnable::nsReflowFrameRunnable(nsIFrame* aFrame,
                          nsIPresShell::IntrinsicDirty aIntrinsicDirty,
                          nsFrameState aBitToAdd)
  : mWeakFrame(aFrame),
    mIntrinsicDirty(aIntrinsicDirty),
    mBitToAdd(aBitToAdd)
{
}

NS_IMETHODIMP
nsReflowFrameRunnable::Run()
{
  if (mWeakFrame.IsAlive()) {
    mWeakFrame->PresContext()->PresShell()->
      FrameNeedsReflow(mWeakFrame, mIntrinsicDirty, mBitToAdd);
  }
  return NS_OK;
}
