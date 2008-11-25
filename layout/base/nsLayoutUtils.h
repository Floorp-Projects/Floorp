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
 *   Boris Zbarsky <bzbarsky@mit.edu> (original author)
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

#ifndef nsLayoutUtils_h__
#define nsLayoutUtils_h__

class nsIFormControlFrame;
class nsPresContext;
class nsIContent;
class nsIAtom;
class nsIScrollableView;
class nsIScrollableFrame;
class nsIDOMEvent;
class nsRegion;
class nsDisplayListBuilder;
class nsIFontMetrics;

#include "prtypes.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsIView.h"
#include "nsIFrame.h"
#include "nsThreadUtils.h"

class nsBlockFrame;

/**
 * nsLayoutUtils is a namespace class used for various helper
 * functions that are useful in multiple places in layout.  The goal
 * is not to define multiple copies of the same static helper.
 */
class nsLayoutUtils
{
public:
  /**
   * GetBeforeFrame returns the outermost :before frame of the given frame, if
   * one exists.  This is typically O(1).  The frame passed in must be
   * the first-in-flow.   
   *
   * @param aFrame the frame whose :before is wanted
   * @return the :before frame or nsnull if there isn't one
   */
  static nsIFrame* GetBeforeFrame(nsIFrame* aFrame);

  /**
   * GetAfterFrame returns the outermost :after frame of the given frame, if one
   * exists.  This will walk the in-flow chain to the last-in-flow if
   * needed.  This function is typically O(N) in the number of child
   * frames, following in-flows, etc.
   *
   * @param aFrame the frame whose :after is wanted
   * @return the :after frame or nsnull if there isn't one
   */
  static nsIFrame* GetAfterFrame(nsIFrame* aFrame);

  /** 
   * Given a frame, search up the frame tree until we find an
   * ancestor that (or the frame itself) is of type aFrameType, if any.
   *
   * @param aFrame the frame to start at
   * @param aFrameType the frame type to look for
   * @return a frame of the given type or nsnull if no
   *         such ancestor exists
   */
  static nsIFrame* GetClosestFrameOfType(nsIFrame* aFrame, nsIAtom* aFrameType);

  /** 
   * Given a frame, search up the frame tree until we find an
   * ancestor that (or the frame itself) is a "Page" frame, if any.
   *
   * @param aFrame the frame to start at
   * @return a frame of type nsGkAtoms::pageFrame or nsnull if no
   *         such ancestor exists
   */
  static nsIFrame* GetPageFrame(nsIFrame* aFrame)
  {
    return GetClosestFrameOfType(aFrame, nsGkAtoms::pageFrame);
  }

  /**
   * IsGeneratedContentFor returns PR_TRUE if aFrame is the outermost
   * frame for generated content of type aPseudoElement for aContent.
   * aFrame *might not* have the aPseudoElement pseudo-style! For example
   * it might be a table outer frame and the inner table frame might
   * have the pseudo-style.
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
   * CompareTreePosition determines whether aFrame1 comes before or
   * after aFrame2 in a preorder traversal of the frame tree, where out
   * of flow frames are treated as children of their placeholders. This is
   * basically the same ordering as DoCompareTreePosition(nsIContent*) except
   * that it handles anonymous content properly and there are subtleties with
   * continuations.
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
   *           different frame trees)
   */
  static PRInt32 CompareTreePosition(nsIFrame* aFrame1,
                                     nsIFrame* aFrame2,
                                     nsIFrame* aCommonAncestor = nsnull)
  {
    return DoCompareTreePosition(aFrame1, aFrame2, -1, 1, aCommonAncestor);
  }

  /*
   * More generic version of |CompareTreePosition|.  |aIf1Ancestor|
   * gives the value to return when 1 is an ancestor of 2, and likewise
   * for |aIf2Ancestor|.  Passing (-1, 1) gives preorder traversal
   * order, and (1, -1) gives postorder traversal order.
   */
  static PRInt32 DoCompareTreePosition(nsIFrame* aFrame1,
                                       nsIFrame* aFrame2,
                                       PRInt32 aIf1Ancestor,
                                       PRInt32 aIf2Ancestor,
                                       nsIFrame* aCommonAncestor = nsnull);

  /**
   * GetLastContinuationWithChild gets the last continuation in aFrame's chain
   * that has a child, or the first continuation if the frame has no children.
   */
  static nsIFrame* GetLastContinuationWithChild(nsIFrame* aFrame);

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
   * Get the parent of aFrame. If aFrame is the root frame for a document,
   * and the document has a parent document in the same view hierarchy, then
   * we try to return the subdocumentframe in the parent document.
   * @param aExtraOffset [in/out] if non-null, then as we cross documents
   * an extra offset may be required and it will be added to aCrossDocOffset
   */
  static nsIFrame* GetCrossDocParentFrame(const nsIFrame* aFrame,
                                          nsPoint* aCrossDocOffset = nsnull);
  
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
   * Like IsProperAncestorFrame, but looks across document boundaries.
   */
  static PRBool IsProperAncestorFrameCrossDoc(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                              nsIFrame* aCommonAncestor = nsnull);

  /**
    * GetFrameFor returns the root frame for a view
    * @param aView is the view to return the root frame for
    * @return the root frame for the view
    */
  static nsIFrame* GetFrameFor(nsIView *aView)
  { return static_cast<nsIFrame*>(aView->GetClientData()); }
  
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

  // Combine aNewBreakType with aOrigBreakType, but limit the break types
  // to NS_STYLE_CLEAR_LEFT, RIGHT, LEFT_AND_RIGHT.
  static PRUint8 CombineBreakType(PRUint8 aOrigBreakType, PRUint8 aNewBreakType);

  /**
   * @return PR_TRUE if aFrame is the CSS initial containing block for
   * its pres-shell
   */
  static PRBool IsInitialContainingBlock(nsIFrame* aFrame);

  /**
   * Get the coordinates of a given DOM mouse event, relative to a given
   * frame. Works only for DOM events generated by nsGUIEvents.
   * @param aDOMEvent the event
   * @param aFrame the frame to make coordinates relative to
   * @return the point, or (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if
   * for some reason the coordinates for the mouse are not known (e.g.,
   * the event is not a GUI event).
   */
  static nsPoint GetDOMEventCoordinatesRelativeTo(nsIDOMEvent* aDOMEvent,
                                                  nsIFrame* aFrame);

  /**
   * Get the coordinates of a given native mouse event, relative to a given
   * frame.
   * @param aEvent the event
   * @param aFrame the frame to make coordinates relative to
   * @return the point, or (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if
   * for some reason the coordinates for the mouse are not known (e.g.,
   * the event is not a GUI event).
   */
  static nsPoint GetEventCoordinatesRelativeTo(const nsEvent* aEvent,
                                               nsIFrame* aFrame);

/**
   * Get the coordinates of a given native mouse event, relative to the nearest 
   * view for a given frame.
   * The "nearest view" is the view returned by nsFrame::GetOffsetFromView.
   * XXX this is extremely BOGUS because "nearest view" is a mess; every
   * use of this method is really a bug!
   * @param aEvent the event
   * @param aFrame the frame to make coordinates relative to
   * @param aView  view to which returned coordinates are relative 
   * @return the point, or (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if
   * for some reason the coordinates for the mouse are not known (e.g.,
   * the event is not a GUI event).
   */
  static nsPoint GetEventCoordinatesForNearestView(nsEvent* aEvent,
                                                   nsIFrame* aFrame,
                                                   nsIView** aView = nsnull);

/**
   * Translate from widget coordinates to the view's coordinates
   * @param aPresContext the PresContext for the view
   * @param aWidget the widget
   * @param aPt the point relative to the widget
   * @param aView  view to which returned coordinates are relative
   * @return the point in the view's coordinates
   */
  static nsPoint TranslateWidgetToView(nsPresContext* aPresContext, 
                                       nsIWidget* aWidget, nsIntPoint aPt,
                                       nsIView* aView);

  /**
   * Given a matrix and a point, let T be the transformation matrix translating points
   * in the coordinate space with origin aOrigin to the coordinate space used by the
   * matrix.  If M is the stored matrix, this function returns (T-1)MT, the matrix
   * that's equivalent to aMatrix but in the coordinate space that treats aOrigin
   * as the origin.
   *
   * @param aOrigin The origin to translate to.
   * @param aMatrix The matrix to change the basis of.
   * @return A matrix equivalent to aMatrix, but operating in the coordinate system with
   *         origin aOrigin.
   */
  static gfxMatrix ChangeMatrixBasis(const gfxPoint &aOrigin, const gfxMatrix &aMatrix);

  /**
   * Given aFrame, the root frame of a stacking context, find its descendant
   * frame under the point aPt that receives a mouse event at that location,
   * or nsnull if there is no such frame.
   * @param aPt the point, relative to the frame origin
   * @param aShouldIgnoreSuppression a boolean to control if the display
   * list builder should ignore paint suppression or not
   */
  static nsIFrame* GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt,
                                    PRBool aShouldIgnoreSuppression = PR_FALSE,
                                    PRBool aIgnoreScrollFrame = PR_FALSE);

  /**
   * Given a point in the global coordinate space, returns that point expressed
   * in the coordinate system of aFrame.  This effectively inverts all transforms
   * between this point and the root frame.
   *
   * @param aFrame The frame that acts as the coordinate space container.
   * @param aPoint The point, in the global space, to get in the frame-local space.
   * @return aPoint, expressed in aFrame's canonical coordinate space.
   */
  static nsPoint InvertTransformsToRoot(nsIFrame* aFrame,
                                        const nsPoint &aPt);


  /**
   * Helper function that, given a rectangle and a matrix, returns the smallest
   * rectangle containing the image of the source rectangle.
   *
   * @param aBounds The rectangle to transform.
   * @param aMatrix The matrix to transform it with.
   * @param aFactor The number of app units per graphics unit.
   * @return The smallest rect that contains the image of aBounds.
   */
  static nsRect MatrixTransformRect(const nsRect &aBounds,
                                    const gfxMatrix &aMatrix, float aFactor);

  /**
   * Helper function that, given a point and a matrix, returns the image
   * of that point under the matrix transform.
   *
   * @param aPoint The point to transform.
   * @param aMatrix The matrix to transform it with.
   * @param aFactor The number of app units per graphics unit.
   * @return The image of the point under the transform.
   */
  static nsPoint MatrixTransformPoint(const nsPoint &aPoint,
                                      const gfxMatrix &aMatrix, float aFactor);

  /**
   * Given a graphics rectangle in graphics space, return a rectangle in
   * app space that contains the graphics rectangle, rounding out as necessary.
   *
   * @param aRect The graphics rect to round outward.
   * @param aFactor The number of app units per graphics unit.
   * @return The smallest rectangle in app space that contains aRect.
   */
  static nsRect RoundGfxRectToAppRect(const gfxRect &aRect, float aFactor);


  /**
   * Given aFrame, the root frame of a stacking context, paint it and its
   * descendants to aRenderingContext. 
   * @param aRenderingContext a rendering context translated so that (0,0)
   * is the origin of aFrame; for best results, (0,0) should transform
   * to pixel-aligned coordinates
   * @param aDirtyRegion the region that must be painted, in the coordinates
   * of aFrame
   * @param aBackground paint the dirty area with this color before drawing
   * the actual content; pass NS_RGBA(0,0,0,0) to draw no background
   */
  static nsresult PaintFrame(nsIRenderingContext* aRenderingContext, nsIFrame* aFrame,
                             const nsRegion& aDirtyRegion, nscolor aBackground);

  /**
   * @param aRootFrame the root frame of the tree to be displayed
   * @param aMovingFrame a frame that has moved
   * @param aPt the amount by which aMovingFrame has moved and the rect will
   * be copied
   * @param aCopyRect a rectangle that will be copied, relative to aRootFrame
   * @param aRepaintRegion a subregion of aCopyRect+aDelta that must be repainted
   * after doing the bitblt
   * 
   * Ideally this function would actually have the rect-to-copy as an output
   * rather than an input, but for now, scroll bitblitting is limited to
   * the whole of a single widget, so we cannot choose the rect.
   * 
   * This function assumes that the caller will do a bitblt copy of aCopyRect
   * to aCopyRect+aPt. It computes a region that must be repainted in order
   * for the resulting rendering to be correct. Frame geometry must have
   * already been adjusted for the scroll/copy operation.
   * 
   * Conceptually it works by computing a display list in the before-state
   * and a display list in the after-state and analyzing them to find the
   * differences. In practice it is only feasible to build a display list
   * in the after-state (plus building two display lists would be less
   * efficient), so we use some unfortunately tricky techniques to get by
   * with just the after-list.
   * 
   * The output region consists of:
   * a) any visible background-attachment:fixed areas in the after-move display
   * list
   * b) any visible areas of the before-move display list corresponding to
   * frames that will not move (translated by aDelta)
   * c) any visible areas of the after-move display list corresponding to
   * frames that did not move
   * d) except that if the same display list element is visible in b) and c)
   * for a frame that did not move and paints a uniform color within its
   * bounds, then the intersection of its old and new bounds can be excluded
   * when it is processed by b) and c).
   * 
   * We may return a larger region if computing the above region precisely is
   * too expensive.
   */
  static nsresult ComputeRepaintRegionForCopy(nsIFrame* aRootFrame,
                                              nsIFrame* aMovingFrame,
                                              nsPoint aDelta,
                                              const nsRect& aCopyRect,
                                              nsRegion* aRepaintRegion);

  /**
   * Compute the used z-index of aFrame; returns zero for elements to which
   * z-index does not apply, and for z-index:auto
   */
  static PRInt32 GetZIndex(nsIFrame* aFrame);

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
  static PRBool
  BinarySearchForPosition(nsIRenderingContext* acx, 
                          const PRUnichar* aText,
                          PRInt32    aBaseWidth,
                          PRInt32    aBaseInx,
                          PRInt32    aStartInx, 
                          PRInt32    aEndInx, 
                          PRInt32    aCursorPos, 
                          PRInt32&   aIndex,
                          PRInt32&   aTextWidth);

  class BoxCallback {
  public:
    virtual void AddBox(nsIFrame* aFrame) = 0;
  };
  /**
   * Collect all CSS boxes associated with aFrame and its
   * continuations, "drilling down" through outer table frames and
   * some anonymous blocks since they're not real CSS boxes.
   * If aFrame is null, no boxes are returned.
   * SVG frames return a single box, themselves.
   */
  static void GetAllInFlowBoxes(nsIFrame* aFrame, BoxCallback* aCallback);

  class RectCallback {
  public:
    virtual void AddRect(const nsRect& aRect) = 0;
  };
  /**
   * Collect all CSS border-boxes associated with aFrame and its
   * continuations, "drilling down" through outer table frames and
   * some anonymous blocks since they're not real CSS boxes.
   * The boxes are positioned relative to aRelativeTo (taking scrolling
   * into account) and passed to the callback in frame-tree order.
   * If aFrame is null, no boxes are returned.
   * For SVG frames, returns one rectangle, the bounding box.
   */
  static void GetAllInFlowRects(nsIFrame* aFrame, nsIFrame* aRelativeTo,
                                RectCallback* aCallback);

  /**
   * Computes the union of all rects returned by GetAllInFlowRects. If
   * the union is empty, returns the first rect.
   */
  static nsRect GetAllInFlowRectsUnion(nsIFrame* aFrame, nsIFrame* aRelativeTo);

  /**
   * Takes a text-shadow array from the style properties of a given nsIFrame and
   * computes the union of those shadows along with the given initial rect.
   * If there are no shadows, the initial rect is returned.
   */
  static nsRect GetTextShadowRectsUnion(const nsRect& aTextAndDecorationsRect,
                                        nsIFrame* aFrame);

  /**
   * Get the font metrics corresponding to the frame's style data.
   * @param aFrame the frame
   * @param aFontMetrics the font metrics result
   * @return success or failure code
   */
  static nsresult GetFontMetricsForFrame(nsIFrame* aFrame,
                                         nsIFontMetrics** aFontMetrics);

  /**
   * Get the font metrics corresponding to the given style data.
   * @param aStyleContext the style data
   * @param aFontMetrics the font metrics result
   * @return success or failure code
   */
  static nsresult GetFontMetricsForStyleContext(nsStyleContext* aStyleContext,
                                                nsIFontMetrics** aFontMetrics);

  /**
   * Find the immediate child of aParent whose frame subtree contains
   * aDescendantFrame. Returns null if aDescendantFrame is not a descendant
   * of aParent.
   */
  static nsIFrame* FindChildContainingDescendant(nsIFrame* aParent, nsIFrame* aDescendantFrame);
  
  /**
   * Find the nearest ancestor that's a block
   */
  static nsBlockFrame* FindNearestBlockAncestor(nsIFrame* aFrame);

  /**
   * Cast aFrame to an nsBlockFrame* or return null if it's not
   * an nsBlockFrame.
   */
  static nsBlockFrame* GetAsBlock(nsIFrame* aFrame);
  
  /**
   * If aFrame is an out of flow frame, return its placeholder, otherwise
   * return its parent.
   */
  static nsIFrame* GetParentOrPlaceholderFor(nsFrameManager* aFrameManager,
                                             nsIFrame* aFrame);

  /**
   * Find the closest common ancestor of aFrame1 and aFrame2, following
   * out of flow frames to their placeholders instead of their parents. Returns
   * nsnull if the frames are in different frame trees.
   * 
   * @param aKnownCommonAncestorHint a frame that is believed to be on the
   * ancestor chain of both aFrame1 and aFrame2. If null, or a frame that is
   * not in fact on both ancestor chains, then this function will still return
   * the correct result, but it will be slower.
   */
  static nsIFrame*
  GetClosestCommonAncestorViaPlaceholders(nsIFrame* aFrame1, nsIFrame* aFrame2,
                                          nsIFrame* aKnownCommonAncestorHint);

  /**
   * Get a frame's next-in-flow, or, if it doesn't have one, its special sibling.
   */
  static nsIFrame*
  GetNextContinuationOrSpecialSibling(nsIFrame *aFrame);

  /**
   * Get the first frame in the continuation-plus-special-sibling chain
   * containing aFrame.
   */
  static nsIFrame*
  GetFirstContinuationOrSpecialSibling(nsIFrame *aFrame);
  
  /**
   * Check whether aFrame is a part of the scrollbar or scrollcorner of
   * the root content.
   * @param aFrame the checking frame
   * @return if TRUE, the frame is a part of the scrollbar or scrollcorner of
   *         the root content.
   */
  static PRBool IsViewportScrollbarFrame(nsIFrame* aFrame);

  /**
   * Get the contribution of aFrame to its containing block's intrinsic
   * width.  This considers the child's intrinsic width, its 'width',
   * 'min-width', and 'max-width' properties, and its padding, border,
   * and margin.
   */
  enum IntrinsicWidthType { MIN_WIDTH, PREF_WIDTH };
  static nscoord IntrinsicForContainer(nsIRenderingContext* aRenderingContext,
                                       nsIFrame* aFrame,
                                       IntrinsicWidthType aType);

  /*
   * Convert nsStyleCoord to nscoord when percentages depend on the
   * containing block width.
   */
  static nscoord ComputeWidthDependentValue(
                   nscoord              aContainingBlockWidth,
                   const nsStyleCoord&  aCoord);

  /*
   * Convert nsStyleCoord to nscoord when percentages depend on the
   * containing block width, and enumerated values are for width,
   * min-width, or max-width.  Returns the content-box width value based
   * on aContentEdgeToBoxSizing and aBoxSizingToMarginEdge (which are
   * also used for the enumerated values for width.  This function does
   * not handle 'auto'.  It ensures that the result is nonnegative.
   *
   * @param aRenderingContext Rendering context for font measurement/metrics.
   * @param aFrame Frame whose (min-/max-/)width is being computed
   * @param aContainingBlockWidth Width of aFrame's containing block.
   * @param aContentEdgeToBoxSizing The sum of any left/right padding and
   *          border that goes inside the rect chosen by -moz-box-sizing.
   * @param aBoxSizingToMarginEdge The sum of any left/right padding, border,
   *          and margin that goes outside the rect chosen by -moz-box-sizing.
   * @param aCoord The width value to compute.
   */
  static nscoord ComputeWidthValue(
                   nsIRenderingContext* aRenderingContext,
                   nsIFrame*            aFrame,
                   nscoord              aContainingBlockWidth,
                   nscoord              aContentEdgeToBoxSizing,
                   nscoord              aBoxSizingToMarginEdge,
                   const nsStyleCoord&  aCoord);

  /*
   * Convert nsStyleCoord to nscoord when percentages depend on the
   * containing block height.
   */
  static nscoord ComputeHeightDependentValue(
                   nscoord              aContainingBlockHeight,
                   const nsStyleCoord&  aCoord);

  /*
   * Calculate the used values for 'width' and 'height' for a replaced element.
   *
   *   http://www.w3.org/TR/CSS21/visudet.html#min-max-widths
   */
  static nsSize ComputeSizeWithIntrinsicDimensions(
                    nsIRenderingContext* aRenderingContext, nsIFrame* aFrame,
                    const nsIFrame::IntrinsicSize& aIntrinsicSize,
                    nsSize aIntrinsicRatio, nsSize aCBSize,
                    nsSize aMargin, nsSize aBorder, nsSize aPadding);

  // Implement nsIFrame::GetPrefWidth in terms of nsIFrame::AddInlinePrefWidth
  static nscoord PrefWidthFromInline(nsIFrame* aFrame,
                                     nsIRenderingContext* aRenderingContext);

  // Implement nsIFrame::GetMinWidth in terms of nsIFrame::AddInlineMinWidth
  static nscoord MinWidthFromInline(nsIFrame* aFrame,
                                    nsIRenderingContext* aRenderingContext);

  static void DrawString(const nsIFrame*      aFrame,
                         nsIRenderingContext* aContext,
                         const PRUnichar*     aString,
                         PRInt32              aLength,
                         nsPoint              aPoint);

  static nscoord GetStringWidth(const nsIFrame*      aFrame,
                                nsIRenderingContext* aContext,
                                const PRUnichar*     aString,
                                PRInt32              aLength);

  /**
   * Derive a baseline of |aFrame| (measured from its top border edge)
   * from its first in-flow line box (not descending into anything with
   * 'overflow' not 'visible', potentially including aFrame itself).
   *
   * Returns true if a baseline was found (and fills in aResult).
   * Otherwise returns false.
   */
  static PRBool GetFirstLineBaseline(const nsIFrame* aFrame, nscoord* aResult);

  /**
   * Derive a baseline of |aFrame| (measured from its top border edge)
   * from its last in-flow line box (not descending into anything with
   * 'overflow' not 'visible', potentially including aFrame itself).
   *
   * Returns true if a baseline was found (and fills in aResult).
   * Otherwise returns false.
   */
  static PRBool GetLastLineBaseline(const nsIFrame* aFrame, nscoord* aResult);

  /**
   * Returns a y coordinate relative to this frame's origin that represents
   * the logical bottom of the frame or its visible content, whichever is lower.
   * Relative positioning is ignored and margins and glyph bounds are not
   * considered.
   * This value will be >= mRect.height() and <= overflowRect.YMost() unless
   * relative positioning is applied.
   */
  static nscoord CalculateContentBottom(nsIFrame* aFrame);

  /**
   * Gets the closest frame (the frame passed in or one of its parents) that
   * qualifies as a "layer"; used in DOM0 methods that depends upon that
   * definition. This is the nearest frame that is either positioned or scrolled
   * (the child of a scroll frame). In Gecko terms, it's approximately
   * equivalent to having a view, at least for simple HTML. However, views are
   * going away, so this is a cleaner definition.
   */
  static nsIFrame* GetClosestLayer(nsIFrame* aFrame);

  /**
   * Draw an image.
   * See https://wiki.mozilla.org/Gecko:Image_Snapping_and_Rendering
   *   @param aRenderingContext Where to draw the image, set up with an
   *                            appropriate scale and transform for drawing in
   *                            app units.
   *   @param aImage            The image.
   *   @param aDest             Where one copy of the image should mapped to.
   *   @param aFill             The area to be filled with copies of the
   *                            image.
   *   @param aAnchor           A point in aFill which we will ensure is
   *                            pixel-aligned in the output.
   *   @param aDirty            Pixels outside this area may be skipped.
   */
  static nsresult DrawImage(nsIRenderingContext* aRenderingContext,
                            imgIContainer*       aImage,
                            const nsRect&        aDest,
                            const nsRect&        aFill,
                            const nsPoint&       aAnchor,
                            const nsRect&        aDirty);

  /**
   * Draw a whole image without scaling or tiling.
   *
   *   @param aRenderingContext Where to draw the image, set up with an
   *                            appropriate scale and transform for drawing in
   *                            app units.
   *   @param aImage            The image.
   *   @param aDest             The top-left where the image should be drawn
   *   @param aDirty            Pixels outside this area may be skipped.
   *   @param aSourceArea       If non-null, this area is extracted from
   *                            the image and drawn at aDest. It's
   *                            in appunits. For best results it should
   *                            be aligned with image pixels.
   */
  static nsresult DrawSingleUnscaledImage(nsIRenderingContext* aRenderingContext,
                                          imgIContainer*       aImage,
                                          const nsPoint&       aDest,
                                          const nsRect&        aDirty,
                                          const nsRect*        aSourceArea = nsnull);

  /**
   * Draw a whole image without tiling.
   *
   *   @param aRenderingContext Where to draw the image, set up with an
   *                            appropriate scale and transform for drawing in
   *                            app units.
   *   @param aImage            The image.
   *   @param aDest             The area that the image should fill
   *   @param aDirty            Pixels outside this area may be skipped.
   *   @param aSourceArea       If non-null, this area is extracted from
   *                            the image and drawn in aDest. It's
   *                            in appunits. For best results it should
   *                            be aligned with image pixels.
   */
  static nsresult DrawSingleImage(nsIRenderingContext* aRenderingContext,
                                  imgIContainer*       aImage,
                                  const nsRect&        aDest,
                                  const nsRect&        aDirty,
                                  const nsRect*        aSourceArea = nsnull);

  /**
   * Given a source area of an image (in appunits) and a destination area
   * that we want to map that source area too, computes the area that
   * would be covered by the whole image. This is useful for passing to
   * the aDest parameter of DrawImage, when we want to draw a subimage
   * of an overall image.
   */
  static nsRect GetWholeImageDestination(const nsIntSize& aWholeImageSize,
                                         const nsRect& aImageSourceArea,
                                         const nsRect& aDestArea);

  /**
   * Set the font on aRC based on the style in aSC
   */
  static void SetFontFromStyle(nsIRenderingContext* aRC, nsStyleContext* aSC);

  /**
   * Determine if any corner radius is of nonzero size
   *   @param aCorners the |nsStyleCorners| object to check
   *   @return PR_TRUE unless all the coordinates are 0%, 0 or null.
   *
   * A corner radius with one dimension zero and one nonzero is
   * treated as a nonzero-radius corner, even though it will end up
   * being rendered like a zero-radius corner.  This is because such
   * corners are not expected to appear outside of test cases, and it's
   * simpler to implement the test this way.
   */
  static PRBool HasNonZeroCorner(const nsStyleCorners& aCorners);

  /**
   * Determine if a widget is likely to require transparency or translucency.
   *   @param aFrame the frame of a <window>, <popup> or <menupopup> element.
   *   @return a value suitable for passing to SetWindowTranslucency
   */
  static nsTransparencyMode GetFrameTransparency(nsIFrame* aFrame);

  /**
   * Get textrun construction flags determined by a given style; in particular
   * some combination of:
   * -- TEXT_DISABLE_OPTIONAL_LIGATURES if letter-spacing is in use
   * -- TEXT_OPTIMIZE_SPEED if the text-rendering CSS property and font size
   * and prefs indicate we should be optimizing for speed over quality
   */
  static PRUint32 GetTextRunFlagsForStyle(nsStyleContext* aStyleContext,
                                          const nsStyleText* aStyleText,
                                          const nsStyleFont* aStyleFont);

  /**
   * Takes two rectangles whose origins must be the same, and computes
   * the difference between their union and their intersection as two
   * rectangles. (This difference is a superset of the difference
   * between the two rectangles.)
   */
  static void GetRectDifferenceStrips(const nsRect& aR1, const nsRect& aR2,
                                      nsRect* aHStrip, nsRect* aVStrip);

  /**
   * Get a device context that can be used to get up-to-date device
   * dimensions for the given docshell.  For some reason, this is more
   * complicated than it ought to be in multi-monitor situations.
   */
  static nsIDeviceContext*
  GetDeviceContextForScreenInfo(nsIDocShell* aDocShell);

  /**
   * Some frames with 'position: fixed' (nsStylePosition::mDisplay ==
   * NS_STYLE_POSITION_FIXED) are not really fixed positioned, since
   * they're inside an element with -moz-transform.  This function says
   * whether such an element is a real fixed-pos element.
   */
  static PRBool IsReallyFixedPos(nsIFrame* aFrame);

  /**
   * Indicates if the nsIFrame::GetUsedXXX assertions in nsFrame.cpp should
   * disabled.
   */
  static PRBool sDisableGetUsedXAssertions;
};

class nsAutoDisableGetUsedXAssertions
{
public:
  nsAutoDisableGetUsedXAssertions()
    : mOldValue(nsLayoutUtils::sDisableGetUsedXAssertions)
  {
    nsLayoutUtils::sDisableGetUsedXAssertions = PR_TRUE;
  }
  ~nsAutoDisableGetUsedXAssertions()
  {
    nsLayoutUtils::sDisableGetUsedXAssertions = mOldValue;
  }

private:
  PRBool mOldValue;
};

class nsSetAttrRunnable : public nsRunnable
{
public:
  nsSetAttrRunnable(nsIContent* aContent, nsIAtom* aAttrName,
                    const nsAString& aValue);

  NS_DECL_NSIRUNNABLE

  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<nsIAtom> mAttrName;
  nsAutoString mValue;
};

class nsUnsetAttrRunnable : public nsRunnable
{
public:
  nsUnsetAttrRunnable(nsIContent* aContent, nsIAtom* aAttrName);

  NS_DECL_NSIRUNNABLE

  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<nsIAtom> mAttrName;
};

#endif // nsLayoutUtils_h__
