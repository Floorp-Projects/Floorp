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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#ifndef nsLayoutUtils_h__
#define nsLayoutUtils_h__

class nsIFrame;
class nsPresContext;
class nsIContent;
class nsIAtom;
class nsIScrollableView;
class nsIScrollableFrame;

#include "prtypes.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsIView.h"

/**
 * nsLayoutUtils is a namespace class used for various helper
 * functions that are useful in multiple places in layout.  The goal
 * is not to define multiple copies of the same static helper.
 */
class nsLayoutUtils
{
public:
  /**
   * GetBeforeFrame returns the :before frame of the given frame, if
   * one exists.  This is typically O(1).  The frame passed in must be
   * the first-in-flow.   
   *
   * @param aFrame the frame whose :before is wanted
   * @param aPresContext the prescontext
   * @return the :before frame or nsnull if there isn't one
   */
  static nsIFrame* GetBeforeFrame(nsIFrame* aFrame, nsPresContext* aPresContext);

  /**
   * GetAfterFrame returns the :after frame of the given frame, if one
   * exists.  This will walk the in-flow chain to the last-in-flow if
   * needed.  This function is typically O(N) in the number of child
   * frames, following in-flows, etc.
   *
   * @param aFrame the frame whose :after is wanted
   * @param aPresContext the prescontext
   * @return the :after frame or nsnull if there isn't one
   */
  static nsIFrame* GetAfterFrame(nsIFrame* aFrame, nsPresContext* aPresContext);

  /** 
   * Given a frame, search up the frame tree until we find an
   * ancestor "Page" frame, if any.
   *
   * @param the frame to start at
   * @return a frame of type nsLayoutAtoms::pageFrame or nsnull if no
   *         such ancestor exists
   */
  static nsIFrame* GetPageFrame(nsIFrame* aFrame);

  /**
   * IsGeneratedContentFor returns PR_TRUE if aFrame is generated
   * content of type aPseudoElement for aContent
   *
   * @param aContent the content node we're looking at.  If this is
   *        null, then we just assume that aFrame has the right content
   *        pointer.
   * @param aFrame the frame we're looking at
   * @param aPseudoElement the pseudo type we're interested in
   * @return whether aFrame is the generated aPseudoElement frame for aContent
   */
  static PRBool IsGeneratedContentFor(nsIContent* aContent, nsIFrame* aFrame,
                                      nsIAtom* aPseudoElement);

  /**
   * CompareTreePosition determines whether aContent1 comes before or
   * after aContent2 in a preorder traversal of the content tree.
   * 
   * @param aCommonAncestor either null, or a common ancestor of
   *                        aContent1 and aContent2.  Actually this is
   *                        only a hint; if it's not an ancestor of
   *                        aContent1 or aContent2, this function will
   *                        still work, but it will be slower than
   *                        normal.
   * @return < 0 if aContent1 is before aContent2
   *         > 0 if aContent1 is after aContent2,
   *         0 otherwise (meaning they're the same, or they're in
   *           different documents)
   */
  static PRInt32 CompareTreePosition(nsIContent* aContent1,
                                     nsIContent* aContent2,
                                     nsIContent* aCommonAncestor = nsnull)
  {
    return DoCompareTreePosition(aContent1, aContent2, -1, 1, aCommonAncestor);
  }

  /*
   * More generic version of |CompareTreePosition|.  |aIf1Ancestor|
   * gives the value to return when 1 is an ancestor of 2, and likewise
   * for |aIf2Ancestor|.  Passing (-1, 1) gives preorder traversal
   * order, and (1, -1) gives postorder traversal order.
   */
  static PRInt32 DoCompareTreePosition(nsIContent* aContent1,
                                       nsIContent* aContent2,
                                       PRInt32 aIf1Ancestor,
                                       PRInt32 aIf2Ancestor,
                                       nsIContent* aCommonAncestor = nsnull);
  
  /**
   * GetLastSibling simply finds the last sibling of aFrame, or returns nsnull if
   * aFrame is null.
   */
  static nsIFrame* GetLastSibling(nsIFrame* aFrame);

  /**
   * FindSiblingViewFor locates the child of aParentView that aFrame's
   * view should be inserted 'above' (i.e., before in sibling view
   * order).  This is the first child view of aParentView whose
   * corresponding content is before aFrame's content (view siblings
   * are in reverse content order).
   */
  static nsIView* FindSiblingViewFor(nsIView* aParentView, nsIFrame* aFrame);

  /**
   * IsProperAncestorFrame checks whether aAncestorFrame is an ancestor
   * of aFrame and not equal to aFrame.
   * @param aCommonAncestor nsnull, or a common ancestor of aFrame and
   * aAncestorFrame. If non-null, this can bound the search and speed up
   * the function
   */
  static PRBool IsProperAncestorFrame(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                      nsIFrame* aCommonAncestor = nsnull);

  /**
    * GetFrameFor returns the root frame for a view
    * @param aView is the view to return the root frame for
    * @return the root frame for the view
    */
  static nsIFrame* GetFrameFor(nsIView *aView)
  { return NS_STATIC_CAST(nsIFrame*, aView->GetClientData()); }
  
  /**
    * GetScrollableFrameFor returns the scrollable frame for a scrollable view
    * @param aScrollableView is the scrollable view to return the 
    *        scrollable frame for.
    * @return the scrollable frame for the scrollable view
    */
  static nsIScrollableFrame* GetScrollableFrameFor(nsIScrollableView *aScrollableView);

  /**
    * GetScrollableFrameFor returns the scrollable frame for a scrolled frame
    */
  static nsIScrollableFrame* GetScrollableFrameFor(nsIFrame *aScrolledFrame);

  static nsPresContext::ScrollbarStyles
    ScrollbarStylesOfView(nsIScrollableView *aScrollableView);

  /**
   * GetNearestScrollingView locates the first ancestor of aView (or
   * aView itself) that is scrollable.  It does *not* count an
   * 'overflow' style of 'hidden' as scrollable, even though a scrolling
   * view is present.  Thus, the direction of the scroll is needed as
   * an argument.
   *
   * @param  aView the view we're looking at
   * @param  aDirection Whether it's for horizontal or vertical scrolling.
   * @return the nearest scrollable view or nsnull if not found
   */
  enum Direction { eHorizontal, eVertical, eEither };
  static nsIScrollableView* GetNearestScrollingView(nsIView* aView,
                                                    Direction aDirection);

  /**
   * HasPseudoStyle returns PR_TRUE if aContent (whose primary style
   * context is aStyleContext) has the aPseudoElement pseudo-style
   * attached to it; returns PR_FALSE otherwise.
   *
   * @param aContent the content node we're looking at
   * @param aStyleContext aContent's style context
   * @param aPseudoElement the name of the pseudo style we care about
   * @param aPresContext the presentation context
   * @return whether aContent has aPseudoElement style attached to it
   */
  static PRBool HasPseudoStyle(nsIContent* aContent,
                               nsStyleContext* aStyleContext,
                               nsIAtom* aPseudoElement,
                               nsPresContext* aPresContext)
  {
    NS_PRECONDITION(aPresContext, "Must have a prescontext");
    NS_PRECONDITION(aPseudoElement, "Must have a pseudo name");

    nsRefPtr<nsStyleContext> pseudoContext;
    if (aContent) {
      pseudoContext = aPresContext->StyleSet()->
        ProbePseudoStyleFor(aContent, aPseudoElement, aStyleContext);
    }
    return pseudoContext != nsnull;
  }

  /**
   * If this frame is a placeholder for a float, then return the float,
   * otherwise return nsnull.
   */
  static nsIFrame* GetFloatFromPlaceholder(nsIFrame* aPossiblePlaceholder);
};

#endif // nsLayoutUtils_h__
