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

/**
 * A namespace class for static layout utilities.
 */

/**
 * GetFirstChildFrame returns the first "real" child frame of a
 * given frame.  It will descend down into pseudo-frames (unless the
 * pseudo-frame is the :before generated frame).   
 * @param aPresContext the prescontext
 * @param aFrame the frame
 * @param aFrame the frame's content node
 */
static nsIFrame*
GetFirstChildFrame(nsPresContext* aPresContext,
                   nsIFrame*       aFrame,
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
    return GetFirstChildFrame(aPresContext, childFrame, aContent);
  }

  return childFrame;
}

/**
 * GetLastChildFrame returns the last "real" child frame of a
 * given frame.  It will descend down into pseudo-frames (unless the
 * pseudo-frame is the :after generated frame).   
 * @param aPresContext the prescontext
 * @param aFrame the frame
 * @param aFrame the frame's content node
 */
static nsIFrame*
GetLastChildFrame(nsPresContext* aPresContext,
                  nsIFrame*       aFrame,
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
      return GetLastChildFrame(aPresContext, lastChildFrame, aContent);
    }

    return lastChildFrame;
  }

  return nsnull;
}

// static
nsIFrame*
nsLayoutUtils::GetBeforeFrame(nsIFrame* aFrame, nsPresContext* aPresContext)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");
  NS_ASSERTION(!aFrame->GetPrevInFlow(), "aFrame must be first-in-flow");
  
  nsIFrame* firstFrame = GetFirstChildFrame(aPresContext, aFrame, aFrame->GetContent());

  if (firstFrame && IsGeneratedContentFor(nsnull, firstFrame,
                                          nsCSSPseudoElements::before)) {
    return firstFrame;
  }

  return nsnull;
}

// static
nsIFrame*
nsLayoutUtils::GetAfterFrame(nsIFrame* aFrame, nsPresContext* aPresContext)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  nsIFrame* lastFrame = GetLastChildFrame(aPresContext, aFrame, aFrame->GetContent());

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
    NS_STATIC_CAST(nsPlaceholderFrame*, aFrame)->GetOutOfFlowFrame();
  // This is a hack.
  if (outOfFlowFrame &&
      !outOfFlowFrame->GetStyleDisplay()->IsAbsolutelyPositioned()) {
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
  nsIContent* c1;
  for (c1 = aContent1; c1 && c1 != aCommonAncestor; c1 = c1->GetParent()) {
    content1Ancestors.AppendElement(c1);
  }
  if (!c1 && aCommonAncestor) {
    // So, it turns out aCommonAncestor was not an ancestor of c1. Oops.
    // Never mind. We can continue as if aCommonAncestor was null.
    aCommonAncestor = nsnull;
  }

  nsAutoVoidArray content2Ancestors;
  nsIContent* c2;
  for (c2 = aContent2; c2 && c2 != aCommonAncestor; c2 = c2->GetParent()) {
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
  nsIContent* content1Ancestor = nsnull;
  nsIContent* content2Ancestor = nsnull;
  while (last1 >= 0 && last2 >= 0
         && ((content1Ancestor = NS_STATIC_CAST(nsIContent*, content1Ancestors.ElementAt(last1)))
             == (content2Ancestor = NS_STATIC_CAST(nsIContent*, content2Ancestors.ElementAt(last2))))) {
    last1--;
    last2--;
  }

  if (last1 < 0) {
    if (last2 < 0) {
      NS_ASSERTION(aContent1 == aContent2, "internal error?");
      return 0;
    } else {
      // aContent1 is an ancestor of aContent2
      return aIf1Ancestor;
    }
  } else {
    if (last2 < 0) {
      // aContent2 is an ancestor of aContent1
      return aIf2Ancestor;
    } else {
      // content1Ancestor != content2Ancestor, so they must be siblings with the same parent
      nsIContent* parent = content1Ancestor->GetParent();
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
  }
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
    if (f && CompareTreePosition(aFrame->GetContent(),
                                 f->GetContent(), parentViewContent) > 0) {
      // aFrame's content is after f's content, so put our view before f's view
      return insertBefore;
    }
  }
  return nsnull;
}

//static
nsPresContext::ScrollbarStyles
nsLayoutUtils::ScrollbarStylesOfView(nsIScrollableView *aScrollableView)
{
  nsIFrame *frame =
    NS_STATIC_CAST(nsIFrame*, aScrollableView->View()->GetClientData());
  if (frame && ((frame = frame->GetParent()))) {
    nsIScrollableFrame *sf;
    CallQueryInterface(frame, &sf);
    if (sf)
      return sf->GetScrollbarStyles();
  }
  return nsPresContext::ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN,
                                        NS_STYLE_OVERFLOW_HIDDEN);
}

// static
nsIScrollableView*
nsLayoutUtils::GetNearestScrollingView(nsIView* aView, Direction aDirection)
{
  // Find the first view that has a scrollable frame whose
  // ScrollbarStyles is not NS_STYLE_OVERFLOW_HIDDEN in aDirection.
  NS_ASSERTION(aView, "GetNearestScrollingView expects a non-null view");
  nsIScrollableView* scrollableView = nsnull;
  for (; aView; aView = aView->GetParent()) {
    CallQueryInterface(aView, &scrollableView);
    if (scrollableView) {
      nsPresContext::ScrollbarStyles ss =
        nsLayoutUtils::ScrollbarStylesOfView(scrollableView);
      // aDirection can be eHorizontal, eVertical, or eEither
      if (aDirection != eHorizontal &&
          ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN)
        break;
      if (aDirection != eVertical &&
          ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN)
        break;
    }
  }
  return scrollableView;
}
