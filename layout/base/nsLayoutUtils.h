/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutUtils_h__
#define nsLayoutUtils_h__

class nsIFormControlFrame;
class nsPresContext;
class nsIContent;
class nsIAtom;
class nsIScrollableFrame;
class nsIDOMEvent;
class nsRegion;
class nsDisplayListBuilder;
class nsDisplayItem;
class nsFontMetrics;
class nsClientRectList;
class nsFontFaceList;
class nsIImageLoadingContent;

#include "mozilla/MemoryReporting.h"
#include "nsChangeHint.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsIFrame.h"
#include "nsThreadUtils.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "gfxPattern.h"
#include "imgIContainer.h"
#include "nsCSSPseudoElements.h"
#include "nsHTMLReflowState.h"
#include "nsIFrameLoader.h"
#include "FrameMetrics.h"

#include <limits>
#include <algorithm>

class nsBlockFrame;
class gfxDrawable;
class nsView;

namespace mozilla {
class SVGImageContext;
namespace dom {
class Element;
class HTMLImageElement;
class HTMLCanvasElement;
class HTMLVideoElement;
} // namespace dom
} // namespace mozilla

/**
 * nsLayoutUtils is a namespace class used for various helper
 * functions that are useful in multiple places in layout.  The goal
 * is not to define multiple copies of the same static helper.
 */
class nsLayoutUtils
{
  typedef gfxPattern::GraphicsFilter GraphicsFilter;

public:
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;

  /**
   * Finds previously assigned or generates a unique ViewID for the given
   * content element.
   */
  static ViewID FindIDFor(nsIContent* aContent);

  /**
   * Find content for given ID.
   */
  static nsIContent* FindContentFor(ViewID aId);

  /**
   * Get display port for the given element.
   */
  static bool GetDisplayPort(nsIContent* aContent, nsRect *aResult);

  /**
   * Get the critical display port for the given element.
   */
  static bool GetCriticalDisplayPort(nsIContent* aContent, nsRect* aResult);

  /**
   * Use heuristics to figure out the child list that
   * aChildFrame is currently in.
   */
  static nsIFrame::ChildListID GetChildListNameFor(nsIFrame* aChildFrame);

  /**
   * GetBeforeFrame returns the outermost :before frame of the given frame, if
   * one exists.  This is typically O(1).  The frame passed in must be
   * the first-in-flow.
   *
   * @param aFrame the frame whose :before is wanted
   * @return the :before frame or nullptr if there isn't one
   */
  static nsIFrame* GetBeforeFrame(nsIFrame* aFrame);

  /**
   * GetAfterFrame returns the outermost :after frame of the given frame, if one
   * exists.  This will walk the in-flow chain to the last-in-flow if
   * needed.  This function is typically O(N) in the number of child
   * frames, following in-flows, etc.
   *
   * @param aFrame the frame whose :after is wanted
   * @return the :after frame or nullptr if there isn't one
   */
  static nsIFrame* GetAfterFrame(nsIFrame* aFrame);

  /**
   * Given a frame, search up the frame tree until we find an
   * ancestor that (or the frame itself) is of type aFrameType, if any.
   *
   * @param aFrame the frame to start at
   * @param aFrameType the frame type to look for
   * @return a frame of the given type or nullptr if no
   *         such ancestor exists
   */
  static nsIFrame* GetClosestFrameOfType(nsIFrame* aFrame, nsIAtom* aFrameType);

  /**
   * Given a frame, search up the frame tree until we find an
   * ancestor that (or the frame itself) is a "Page" frame, if any.
   *
   * @param aFrame the frame to start at
   * @return a frame of type nsGkAtoms::pageFrame or nullptr if no
   *         such ancestor exists
   */
  static nsIFrame* GetPageFrame(nsIFrame* aFrame)
  {
    return GetClosestFrameOfType(aFrame, nsGkAtoms::pageFrame);
  }

  /**
   * Given a frame which is the primary frame for an element,
   * return the frame that has the non-psuedoelement style context for
   * the content.
   * This is aPrimaryFrame itself except for tableOuter frames.
   */
  static nsIFrame* GetStyleFrame(nsIFrame* aPrimaryFrame);

  /**
   * Given a content node,
   * return the frame that has the non-psuedoelement style context for
   * the content.  May return null.
   * This is aContent->GetPrimaryFrame() except for tableOuter frames.
   */
  static nsIFrame* GetStyleFrame(const nsIContent* aContent);

  /**
   * IsGeneratedContentFor returns true if aFrame is the outermost
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
  static bool IsGeneratedContentFor(nsIContent* aContent, nsIFrame* aFrame,
                                      nsIAtom* aPseudoElement);

#ifdef DEBUG
  // TODO: remove, see bug 598468.
  static bool gPreventAssertInCompareTreePosition;
#endif // DEBUG

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
  static int32_t CompareTreePosition(nsIContent* aContent1,
                                     nsIContent* aContent2,
                                     const nsIContent* aCommonAncestor = nullptr)
  {
    return DoCompareTreePosition(aContent1, aContent2, -1, 1, aCommonAncestor);
  }

  /*
   * More generic version of |CompareTreePosition|.  |aIf1Ancestor|
   * gives the value to return when 1 is an ancestor of 2, and likewise
   * for |aIf2Ancestor|.  Passing (-1, 1) gives preorder traversal
   * order, and (1, -1) gives postorder traversal order.
   */
  static int32_t DoCompareTreePosition(nsIContent* aContent1,
                                       nsIContent* aContent2,
                                       int32_t aIf1Ancestor,
                                       int32_t aIf2Ancestor,
                                       const nsIContent* aCommonAncestor = nullptr);

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
  static int32_t CompareTreePosition(nsIFrame* aFrame1,
                                     nsIFrame* aFrame2,
                                     nsIFrame* aCommonAncestor = nullptr)
  {
    return DoCompareTreePosition(aFrame1, aFrame2, -1, 1, aCommonAncestor);
  }

  /*
   * More generic version of |CompareTreePosition|.  |aIf1Ancestor|
   * gives the value to return when 1 is an ancestor of 2, and likewise
   * for |aIf2Ancestor|.  Passing (-1, 1) gives preorder traversal
   * order, and (1, -1) gives postorder traversal order.
   */
  static int32_t DoCompareTreePosition(nsIFrame* aFrame1,
                                       nsIFrame* aFrame2,
                                       int32_t aIf1Ancestor,
                                       int32_t aIf2Ancestor,
                                       nsIFrame* aCommonAncestor = nullptr);

  /**
   * Sorts the given nsFrameList, so that for every two adjacent frames in the
   * list, the former is less than or equal to the latter, according to the
   * templated IsLessThanOrEqual method.
   *
   * Note: this method uses a stable merge-sort algorithm.
   */
  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  static void SortFrameList(nsFrameList& aFrameList);

  /**
   * Returns true if the given frame list is already sorted, according to the
   * templated IsLessThanOrEqual function.
   */
  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  static bool IsFrameListSorted(nsFrameList& aFrameList);

  /**
   * GetLastContinuationWithChild gets the last continuation in aFrame's chain
   * that has a child, or the first continuation if the frame has no children.
   */
  static nsIFrame* GetLastContinuationWithChild(nsIFrame* aFrame);

  /**
   * GetLastSibling simply finds the last sibling of aFrame, or returns nullptr if
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
  static nsView* FindSiblingViewFor(nsView* aParentView, nsIFrame* aFrame);

  /**
   * Get the parent of aFrame. If aFrame is the root frame for a document,
   * and the document has a parent document in the same view hierarchy, then
   * we try to return the subdocumentframe in the parent document.
   * @param aExtraOffset [in/out] if non-null, then as we cross documents
   * an extra offset may be required and it will be added to aCrossDocOffset.
   * Be careful dealing with this extra offset as it is in app units of the
   * parent document, which may have a different app units per dev pixel ratio
   * than the child document.
   */
  static nsIFrame* GetCrossDocParentFrame(const nsIFrame* aFrame,
                                          nsPoint* aCrossDocOffset = nullptr);

  /**
   * IsProperAncestorFrame checks whether aAncestorFrame is an ancestor
   * of aFrame and not equal to aFrame.
   * @param aCommonAncestor nullptr, or a common ancestor of aFrame and
   * aAncestorFrame. If non-null, this can bound the search and speed up
   * the function
   */
  static bool IsProperAncestorFrame(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                      nsIFrame* aCommonAncestor = nullptr);

  /**
   * Like IsProperAncestorFrame, but looks across document boundaries.
   *
   * Just like IsAncestorFrameCrossDoc, except that it returns false when
   * aFrame == aAncestorFrame.
   */
  static bool IsProperAncestorFrameCrossDoc(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                              nsIFrame* aCommonAncestor = nullptr);

  /**
   * IsAncestorFrameCrossDoc checks whether aAncestorFrame is an ancestor
   * of aFrame or equal to aFrame, looking across document boundaries.
   * @param aCommonAncestor nullptr, or a common ancestor of aFrame and
   * aAncestorFrame. If non-null, this can bound the search and speed up
   * the function.
   *
   * Just like IsProperAncestorFrameCrossDoc, except that it returns true when
   * aFrame == aAncestorFrame.
   */
  static bool IsAncestorFrameCrossDoc(const nsIFrame* aAncestorFrame, const nsIFrame* aFrame,
                                        const nsIFrame* aCommonAncestor = nullptr);

  /**
   * Finds the nearest ancestor frame that is the root of an "actively
   * scrolled" frame subtree, or aStopAtAncestor if there is no
   * such ancestor before we reach aStopAtAncestor in the ancestor chain.
   * We expect frames with the same "active scrolled root" to be
   * scrolled together, so we'll place them in the same ThebesLayer.
   */
  static nsIFrame* GetActiveScrolledRootFor(nsIFrame* aFrame,
                                            const nsIFrame* aStopAtAncestor);

  static nsIFrame* GetActiveScrolledRootFor(nsDisplayItem* aItem,
                                            nsDisplayListBuilder* aBuilder,
                                            bool* aShouldFixToViewport = nullptr);

  /**
   * Returns true if aActiveScrolledRoot is in a content document,
   * and its topmost content document ancestor has a root scroll frame with
   * a displayport set, and aActiveScrolledRoot is scrolled by that scrollframe.
   */
  static bool IsScrolledByRootContentDocumentDisplayportScrolling(const nsIFrame* aActiveScrolledRoot,
                                                                  nsDisplayListBuilder* aBuilder);

  /**
    * GetScrollableFrameFor returns the scrollable frame for a scrolled frame
    */
  static nsIScrollableFrame* GetScrollableFrameFor(const nsIFrame *aScrolledFrame);

  /**
   * GetNearestScrollableFrameForDirection locates the first ancestor of
   * aFrame (or aFrame itself) that is scrollable with overflow:scroll or
   * overflow:auto in the given direction and where either the scrollbar for
   * that direction is visible or the frame can be scrolled by some
   * positive amount in that direction.
   * The search extends across document boundaries.
   *
   * @param  aFrame the frame to start with
   * @param  aDirection Whether it's for horizontal or vertical scrolling.
   * @return the nearest scrollable frame or nullptr if not found
   */
  enum Direction { eHorizontal, eVertical };
  static nsIScrollableFrame* GetNearestScrollableFrameForDirection(nsIFrame* aFrame,
                                                                   Direction aDirection);

  /**
   * GetNearestScrollableFrame locates the first ancestor of aFrame
   * (or aFrame itself) that is scrollable with overflow:scroll or
   * overflow:auto in some direction.
   * The search extends across document boundaries.
   *
   * @param  aFrame the frame to start with
   * @return the nearest scrollable frame or nullptr if not found
   */
  static nsIScrollableFrame* GetNearestScrollableFrame(nsIFrame* aFrame);

  /**
   * GetScrolledRect returns the range of allowable scroll offsets
   * for aScrolledFrame, assuming the scrollable overflow area is
   * aScrolledFrameOverflowArea and the scrollport size is aScrollPortSize.
   * aDirection is either NS_STYLE_DIRECTION_LTR or NS_STYLE_DIRECTION_RTL.
   */
  static nsRect GetScrolledRect(nsIFrame* aScrolledFrame,
                                const nsRect& aScrolledFrameOverflowArea,
                                const nsSize& aScrollPortSize,
                                uint8_t aDirection);

  /**
   * HasPseudoStyle returns true if aContent (whose primary style
   * context is aStyleContext) has the aPseudoElement pseudo-style
   * attached to it; returns false otherwise.
   *
   * @param aContent the content node we're looking at
   * @param aStyleContext aContent's style context
   * @param aPseudoElement the id of the pseudo style we care about
   * @param aPresContext the presentation context
   * @return whether aContent has aPseudoElement style attached to it
   */
  static bool HasPseudoStyle(nsIContent* aContent,
                               nsStyleContext* aStyleContext,
                               nsCSSPseudoElements::Type aPseudoElement,
                               nsPresContext* aPresContext);

  /**
   * If this frame is a placeholder for a float, then return the float,
   * otherwise return nullptr.  aPlaceholder must be a placeholder frame.
   */
  static nsIFrame* GetFloatFromPlaceholder(nsIFrame* aPlaceholder);

  // Combine aNewBreakType with aOrigBreakType, but limit the break types
  // to NS_STYLE_CLEAR_LEFT, RIGHT, LEFT_AND_RIGHT.
  static uint8_t CombineBreakType(uint8_t aOrigBreakType, uint8_t aNewBreakType);

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
   * Get the coordinates of a given point relative to an event and a
   * given frame.
   * @param aEvent the event
   * @param aPoint the point to get the coordinates relative to
   * @param aFrame the frame to make coordinates relative to
   * @return the point, or (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if
   * for some reason the coordinates for the mouse are not known (e.g.,
   * the event is not a GUI event).
   */
  static nsPoint GetEventCoordinatesRelativeTo(const nsEvent* aEvent,
                                               const nsIntPoint aPoint,
                                               nsIFrame* aFrame);

  /**
   * Get the coordinates of a given point relative to a widget and a
   * given frame.
   * @param aWidget the event src widget
   * @param aPoint the point to get the coordinates relative to
   * @param aFrame the frame to make coordinates relative to
   * @return the point, or (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if
   * for some reason the coordinates for the mouse are not known (e.g.,
   * the event is not a GUI event).
   */
  static nsPoint GetEventCoordinatesRelativeTo(nsIWidget* aWidget,
                                               const nsIntPoint aPoint,
                                               nsIFrame* aFrame);

  /**
   * Get the popup frame of a given native mouse event.
   * @param aPresContext only check popups within aPresContext or a descendant
   * @param aEvent  the event.
   * @return        Null, if there is no popup frame at the point, otherwise,
   *                returns top-most popup frame at the point.
   */
  static nsIFrame* GetPopupFrameForEventCoordinates(nsPresContext* aPresContext,
                                                    const nsEvent* aEvent);

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
                                       nsView* aView);

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
  static gfx3DMatrix ChangeMatrixBasis(const gfxPoint3D &aOrigin, const gfx3DMatrix &aMatrix);

  /**
   * Find IDs corresponding to a scrollable content element in the child process.
   * In correspondence with the shadow layer tree, you can use this to perform a
   * hit test that corresponds to a specific shadow layer that you can then perform
   * transformations on to do parent-side scrolling.
   *
   * @param aFrame The root frame of a stack context
   * @param aTarget The rect to hit test relative to the frame origin
   * @param aOutIDs All found IDs are added here
   * @param aIgnoreRootScrollFrame a boolean to control if the display list
   *        builder should ignore the root scroll frame
   */
  static nsresult GetRemoteContentIds(nsIFrame* aFrame,
                                     const nsRect& aTarget,
                                     nsTArray<ViewID> &aOutIDs,
                                     bool aIgnoreRootScrollFrame);

  enum FrameForPointFlags {
    /**
     * When set, paint suppression is ignored, so we'll return non-root page
     * elements even if paint suppression is stopping them from painting.
     */
    IGNORE_PAINT_SUPPRESSION = 0x01,
    /**
     * When set, clipping due to the root scroll frame (and any other viewport-
     * related clipping) is ignored.
     */
    IGNORE_ROOT_SCROLL_FRAME = 0x02
  };

  /**
   * Given aFrame, the root frame of a stacking context, find its descendant
   * frame under the point aPt that receives a mouse event at that location,
   * or nullptr if there is no such frame.
   * @param aPt the point, relative to the frame origin
   * @param aFlags some combination of FrameForPointFlags
   */
  static nsIFrame* GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt,
                                    uint32_t aFlags = 0);

  /**
   * Given aFrame, the root frame of a stacking context, find all descendant
   * frames under the area of a rectangle that receives a mouse event,
   * or nullptr if there is no such frame.
   * @param aRect the rect, relative to the frame origin
   * @param aOutFrames an array to add all the frames found
   * @param aFlags some combination of FrameForPointFlags
   */
  static nsresult GetFramesForArea(nsIFrame* aFrame, const nsRect& aRect,
                                   nsTArray<nsIFrame*> &aOutFrames,
                                   uint32_t aFlags = 0);

  /**
   * Transform aRect relative to aAncestor down to the coordinate system of
   * aFrame. Computes the bounding-box of the true quadrilateral.
   */
  static nsRect TransformAncestorRectToFrame(nsIFrame* aFrame,
                                             const nsRect& aRect,
                                             const nsIFrame* aAncestor);

  /**
   * Transform aRect relative to aFrame up to the coordinate system of
   * aAncestor. Computes the bounding-box of the true quadrilateral.
   */
  static nsRect TransformFrameRectToAncestor(nsIFrame* aFrame,
                                             const nsRect& aRect,
                                             const nsIFrame* aAncestor);


  /**
   * Gets the transform for aFrame relative to aAncestor. Pass null for aAncestor
   * to go up to the root frame.
   */
  static gfx3DMatrix GetTransformToAncestor(nsIFrame *aFrame, const nsIFrame *aAncestor);

  /**
   * Return true if a "layer transform" could be computed for aFrame,
   * and optionally return the computed transform.  The returned
   * transform is what would be set on the layer currently if a layers
   * transaction were opened at the time this helper is called.
   */
  static bool GetLayerTransformForFrame(nsIFrame* aFrame,
                                        gfx3DMatrix* aTransform);

  /**
   * Given a point in the global coordinate space, returns that point expressed
   * in the coordinate system of aFrame.  This effectively inverts all transforms
   * between this point and the root frame.
   *
   * @param aFrame The frame that acts as the coordinate space container.
   * @param aPoint The point, in the global space, to get in the frame-local space.
   * @return aPoint, expressed in aFrame's canonical coordinate space.
   */
  static nsPoint TransformRootPointToFrame(nsIFrame* aFrame,
                                           const nsPoint &aPoint)
  {
    return TransformAncestorPointToFrame(aFrame, aPoint, nullptr);
  }

  /**
   * Transform aPoint relative to aAncestor down to the coordinate system of
   * aFrame.
   */
  static nsPoint TransformAncestorPointToFrame(nsIFrame* aFrame,
                                               const nsPoint& aPoint,
                                               nsIFrame* aAncestor);

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
                                    const gfx3DMatrix &aMatrix, float aFactor);

  /**
   * Helper function that, given a rectangle and a matrix, returns the smallest
   * rectangle containing the image of the source rectangle rounded out to the nearest
   * pixel value.
   *
   * @param aBounds The rectangle to transform.
   * @param aMatrix The matrix to transform it with.
   * @param aFactor The number of app units per graphics unit.
   * @return The smallest rect that contains the image of aBounds.
   */
  static nsRect MatrixTransformRectOut(const nsRect &aBounds,
                                    const gfx3DMatrix &aMatrix, float aFactor);
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
                                      const gfx3DMatrix &aMatrix, float aFactor);

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
   * Returns a subrectangle of aContainedRect that is entirely inside the rounded
   * rect. Complex cases are handled conservatively by returning a smaller
   * rect than necessary.
   */
  static nsRegion RoundedRectIntersectRect(const nsRect& aRoundedRect,
                                           const nscoord aRadii[8],
                                           const nsRect& aContainedRect);

  /**
   * Return whether any part of aTestRect is inside of the rounded
   * rectangle formed by aBounds and aRadii (which are indexed by the
   * NS_CORNER_* constants in nsStyleConsts.h). This is precise.
   */
  static bool RoundedRectIntersectsRect(const nsRect& aRoundedRect,
                                        const nscoord aRadii[8],
                                        const nsRect& aTestRect);

  enum {
    PAINT_IN_TRANSFORM = 0x01,
    PAINT_SYNC_DECODE_IMAGES = 0x02,
    PAINT_WIDGET_LAYERS = 0x04,
    PAINT_IGNORE_SUPPRESSION = 0x08,
    PAINT_DOCUMENT_RELATIVE = 0x10,
    PAINT_HIDE_CARET = 0x20,
    PAINT_ALL_CONTINUATIONS = 0x40,
    PAINT_TO_WINDOW = 0x80,
    PAINT_EXISTING_TRANSACTION = 0x100,
    PAINT_NO_COMPOSITE = 0x200
  };

  /**
   * Given aFrame, the root frame of a stacking context, paint it and its
   * descendants to aRenderingContext.
   * @param aRenderingContext a rendering context translated so that (0,0)
   * is the origin of aFrame; for best results, (0,0) should transform
   * to pixel-aligned coordinates. This can be null, in which case
   * aFrame must be a "display root" (root frame for a root document,
   * or the root of a popup) with an associated widget and we draw using
   * the layer manager for the frame's widget.
   * @param aDirtyRegion the region that must be painted, in the coordinates
   * of aFrame
   * @param aBackstop paint the dirty area with this color before drawing
   * the actual content; pass NS_RGBA(0,0,0,0) to draw no background
   * @param aFlags if PAINT_IN_TRANSFORM is set, then we assume
   * this is inside a transform or SVG foreignObject. If
   * PAINT_SYNC_DECODE_IMAGES is set, we force synchronous decode on all
   * images. If PAINT_WIDGET_LAYERS is set, aFrame must be a display root,
   * and we will use the frame's widget's layer manager to paint
   * even if aRenderingContext is non-null. This is useful if you want
   * to force rendering to use the widget's layer manager for testing
   * or speed. PAINT_WIDGET_LAYERS must be set if aRenderingContext is null.
   * If PAINT_DOCUMENT_RELATIVE is used, the visible region is interpreted
   * as being relative to the document.  (Normally it's relative to the CSS
   * viewport.) PAINT_TO_WINDOW sets painting to window to true on the display
   * list builder even if we can't tell that we are painting to the window.
   * If PAINT_EXISTING_TRANSACTION is set, then BeginTransaction() has already
   * been called on aFrame's widget's layer manager and should not be
   * called again.
   *
   * So there are three possible behaviours:
   * 1) PAINT_WIDGET_LAYERS is set and aRenderingContext is null; we paint
   * by calling BeginTransaction on the widget's layer manager
   * 2) PAINT_WIDGET_LAYERS is set and aRenderingContext is non-null; we
   * paint by calling BeginTransactionWithTarget on the widget's layer
   * maanger
   * 3) PAINT_WIDGET_LAYERS is not set and aRenderingContext is non-null;
   * we paint by construct a BasicLayerManager and calling
   * BeginTransactionWithTarget on it. This is desirable if we're doing
   * something like drawWindow in a mode where what gets rendered doesn't
   * necessarily correspond to what's visible in the window; we don't
   * want to mess up the widget's layer tree.
   */
  static nsresult PaintFrame(nsRenderingContext* aRenderingContext, nsIFrame* aFrame,
                             const nsRegion& aDirtyRegion, nscolor aBackstop,
                             uint32_t aFlags = 0);

  /**
   * Compute the used z-index of aFrame; returns zero for elements to which
   * z-index does not apply, and for z-index:auto
   */
  static int32_t GetZIndex(nsIFrame* aFrame);

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
  static bool
  BinarySearchForPosition(nsRenderingContext* acx,
                          const PRUnichar* aText,
                          int32_t    aBaseWidth,
                          int32_t    aBaseInx,
                          int32_t    aStartInx,
                          int32_t    aEndInx,
                          int32_t    aCursorPos,
                          int32_t&   aIndex,
                          int32_t&   aTextWidth);

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

  struct RectAccumulator : public RectCallback {
    nsRect mResultRect;
    nsRect mFirstRect;
    bool mSeenFirstRect;

    RectAccumulator();

    virtual void AddRect(const nsRect& aRect);
  };

  struct RectListBuilder : public RectCallback {
    nsClientRectList* mRectList;

    RectListBuilder(nsClientRectList* aList);
    virtual void AddRect(const nsRect& aRect);
  };

  static nsIFrame* GetContainingBlockForClientRect(nsIFrame* aFrame);

  enum {
    RECTS_ACCOUNT_FOR_TRANSFORMS = 0x01
  };
  /**
   * Collect all CSS border-boxes associated with aFrame and its
   * continuations, "drilling down" through outer table frames and
   * some anonymous blocks since they're not real CSS boxes.
   * The boxes are positioned relative to aRelativeTo (taking scrolling
   * into account) and passed to the callback in frame-tree order.
   * If aFrame is null, no boxes are returned.
   * For SVG frames, returns one rectangle, the bounding box.
   * If aFlags includes RECTS_ACCOUNT_FOR_TRANSFORMS, then when converting
   * the boxes into aRelativeTo coordinates, transforms (including CSS
   * and SVG transforms) are taken into account.
   */
  static void GetAllInFlowRects(nsIFrame* aFrame, nsIFrame* aRelativeTo,
                                RectCallback* aCallback, uint32_t aFlags = 0);

  /**
   * Computes the union of all rects returned by GetAllInFlowRects. If
   * the union is empty, returns the first rect.
   * If aFlags includes RECTS_ACCOUNT_FOR_TRANSFORMS, then when converting
   * the boxes into aRelativeTo coordinates, transforms (including CSS
   * and SVG transforms) are taken into account.
   */
  static nsRect GetAllInFlowRectsUnion(nsIFrame* aFrame, nsIFrame* aRelativeTo,
                                       uint32_t aFlags = 0);

  enum {
    EXCLUDE_BLUR_SHADOWS = 0x01
  };
  /**
   * Takes a text-shadow array from the style properties of a given nsIFrame and
   * computes the union of those shadows along with the given initial rect.
   * If there are no shadows, the initial rect is returned.
   */
  static nsRect GetTextShadowRectsUnion(const nsRect& aTextAndDecorationsRect,
                                        nsIFrame* aFrame,
                                        uint32_t aFlags = 0);

  /**
   * Get the font metrics corresponding to the frame's style data.
   * @param aFrame the frame
   * @param aFontMetrics the font metrics result
   * @param aSizeInflation number to multiply font size by
   * @return success or failure code
   */
  static nsresult GetFontMetricsForFrame(const nsIFrame* aFrame,
                                         nsFontMetrics** aFontMetrics,
                                         float aSizeInflation = 1.0f);

  /**
   * Get the font metrics corresponding to the given style data.
   * @param aStyleContext the style data
   * @param aFontMetrics the font metrics result
   * @param aSizeInflation number to multiply font size by
   * @return success or failure code
   */
  static nsresult GetFontMetricsForStyleContext(nsStyleContext* aStyleContext,
                                                nsFontMetrics** aFontMetrics,
                                                float aSizeInflation = 1.0f);

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
   * Find the nearest ancestor that's not for generated content. Will return
   * aFrame if aFrame is not for generated content.
   */
  static nsIFrame* GetNonGeneratedAncestor(nsIFrame* aFrame);

  /**
   * Cast aFrame to an nsBlockFrame* or return null if it's not
   * an nsBlockFrame.
   */
  static nsBlockFrame* GetAsBlock(nsIFrame* aFrame);

  /*
   * Whether the frame is an nsBlockFrame which is not a wrapper block.
   */
  static bool IsNonWrapperBlock(nsIFrame* aFrame) {
    return GetAsBlock(aFrame) && !aFrame->IsBlockWrapper();
  }

  /**
   * If aFrame is an out of flow frame, return its placeholder, otherwise
   * return its parent.
   */
  static nsIFrame* GetParentOrPlaceholderFor(nsIFrame* aFrame);

  /**
   * If aFrame is an out of flow frame, return its placeholder, otherwise
   * return its (possibly cross-doc) parent.
   */
  static nsIFrame* GetParentOrPlaceholderForCrossDoc(nsIFrame* aFrame);

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
  static bool IsViewportScrollbarFrame(nsIFrame* aFrame);

  /**
   * Get the contribution of aFrame to its containing block's intrinsic
   * width.  This considers the child's intrinsic width, its 'width',
   * 'min-width', and 'max-width' properties, and its padding, border,
   * and margin.
   */
  enum IntrinsicWidthType { MIN_WIDTH, PREF_WIDTH };
  static nscoord IntrinsicForContainer(nsRenderingContext* aRenderingContext,
                                       nsIFrame* aFrame,
                                       IntrinsicWidthType aType);

  /*
   * Convert nsStyleCoord to nscoord when percentages depend on the
   * containing block size.
   * @param aPercentBasis The width or height of the containing block
   * (whichever the client wants to use for resolving percentages).
   */
  static nscoord ComputeCBDependentValue(nscoord aPercentBasis,
                                         const nsStyleCoord& aCoord);

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
                   nsRenderingContext* aRenderingContext,
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
   * Likewise, but for 'height', 'min-height', or 'max-height'.
   */
  static nscoord ComputeHeightValue(nscoord aContainingBlockHeight,
                                    nscoord aContentEdgeToBoxSizingBoxEdge,
                                    const nsStyleCoord& aCoord)
  {
    MOZ_ASSERT(aContainingBlockHeight != NS_AUTOHEIGHT || !aCoord.HasPercent(),
               "caller must deal with %% of unconstrained height");
    MOZ_ASSERT(aCoord.IsCoordPercentCalcUnit());

    nscoord result =
      nsRuleNode::ComputeCoordPercentCalc(aCoord, aContainingBlockHeight);
    // Clamp calc(), and the subtraction for box-sizing.
    return std::max(0, result - aContentEdgeToBoxSizingBoxEdge);
  }

  static bool IsAutoHeight(const nsStyleCoord &aCoord, nscoord aCBHeight)
  {
    nsStyleUnit unit = aCoord.GetUnit();
    return unit == eStyleUnit_Auto ||  // only for 'height'
           unit == eStyleUnit_None ||  // only for 'max-height'
           (aCBHeight == NS_AUTOHEIGHT && aCoord.HasPercent());
  }

  static bool IsPaddingZero(const nsStyleCoord &aCoord)
  {
    return (aCoord.GetUnit() == eStyleUnit_Coord &&
            aCoord.GetCoordValue() == 0) ||
           (aCoord.GetUnit() == eStyleUnit_Percent &&
            aCoord.GetPercentValue() == 0.0f) ||
           (aCoord.IsCalcUnit() &&
            // clamp negative calc() to 0
            nsRuleNode::ComputeCoordPercentCalc(aCoord, nscoord_MAX) <= 0 &&
            nsRuleNode::ComputeCoordPercentCalc(aCoord, 0) <= 0);
  }

  static bool IsMarginZero(const nsStyleCoord &aCoord)
  {
    return (aCoord.GetUnit() == eStyleUnit_Coord &&
            aCoord.GetCoordValue() == 0) ||
           (aCoord.GetUnit() == eStyleUnit_Percent &&
            aCoord.GetPercentValue() == 0.0f) ||
           (aCoord.IsCalcUnit() &&
            nsRuleNode::ComputeCoordPercentCalc(aCoord, nscoord_MAX) == 0 &&
            nsRuleNode::ComputeCoordPercentCalc(aCoord, 0) == 0);
  }

  /*
   * Calculate the used values for 'width' and 'height' for a replaced element.
   *
   *   http://www.w3.org/TR/CSS21/visudet.html#min-max-widths
   */
  static nsSize ComputeSizeWithIntrinsicDimensions(
                    nsRenderingContext* aRenderingContext, nsIFrame* aFrame,
                    const nsIFrame::IntrinsicSize& aIntrinsicSize,
                    nsSize aIntrinsicRatio, nsSize aCBSize,
                    nsSize aMargin, nsSize aBorder, nsSize aPadding);

  /*
   * Calculate the used values for 'width' and 'height' when width
   * and height are 'auto'. The tentWidth and tentHeight arguments should be
   * the result of applying the rules for computing intrinsic sizes and ratios.
   * as specified by CSS 2.1 sections 10.3.2 and 10.6.2
   */
  static nsSize ComputeAutoSizeWithIntrinsicDimensions(nscoord minWidth, nscoord minHeight,
                                                       nscoord maxWidth, nscoord maxHeight,
                                                       nscoord tentWidth, nscoord tentHeight);

  // Implement nsIFrame::GetPrefWidth in terms of nsIFrame::AddInlinePrefWidth
  static nscoord PrefWidthFromInline(nsIFrame* aFrame,
                                     nsRenderingContext* aRenderingContext);

  // Implement nsIFrame::GetMinWidth in terms of nsIFrame::AddInlineMinWidth
  static nscoord MinWidthFromInline(nsIFrame* aFrame,
                                    nsRenderingContext* aRenderingContext);

  // Get a suitable foreground color for painting aProperty for aFrame.
  static nscolor GetColor(nsIFrame* aFrame, nsCSSProperty aProperty);

  // Get a baseline y position in app units that is snapped to device pixels.
  static gfxFloat GetSnappedBaselineY(nsIFrame* aFrame, gfxContext* aContext,
                                      nscoord aY, nscoord aAscent);

  static void DrawString(const nsIFrame*      aFrame,
                         nsRenderingContext* aContext,
                         const PRUnichar*     aString,
                         int32_t              aLength,
                         nsPoint              aPoint,
                         uint8_t              aDirection = NS_STYLE_DIRECTION_INHERIT);

  static nscoord GetStringWidth(const nsIFrame*      aFrame,
                                nsRenderingContext* aContext,
                                const PRUnichar*     aString,
                                int32_t              aLength);

  /**
   * Helper function for drawing text-shadow. The callback's job
   * is to draw whatever needs to be blurred onto the given context.
   */
  typedef void (* TextShadowCallback)(nsRenderingContext* aCtx,
                                      nsPoint aShadowOffset,
                                      const nscolor& aShadowColor,
                                      void* aData);

  static void PaintTextShadow(const nsIFrame*     aFrame,
                              nsRenderingContext* aContext,
                              const nsRect&       aTextRect,
                              const nsRect&       aDirtyRect,
                              const nscolor&      aForegroundColor,
                              TextShadowCallback  aCallback,
                              void*               aCallbackData);

  /**
   * Gets the baseline to vertically center text from a font within a
   * line of specified height.
   *
   * Returns the baseline position relative to the top of the line.
   */
  static nscoord GetCenteredFontBaseline(nsFontMetrics* aFontMetrics,
                                         nscoord         aLineHeight);

  /**
   * Derive a baseline of |aFrame| (measured from its top border edge)
   * from its first in-flow line box (not descending into anything with
   * 'overflow' not 'visible', potentially including aFrame itself).
   *
   * Returns true if a baseline was found (and fills in aResult).
   * Otherwise returns false.
   */
  static bool GetFirstLineBaseline(const nsIFrame* aFrame, nscoord* aResult);

  /**
   * Just like GetFirstLineBaseline, except also returns the top and
   * bottom of the line with the baseline.
   *
   * Returns true if a line was found (and fills in aResult).
   * Otherwise returns false.
   */
  struct LinePosition {
    nscoord mTop, mBaseline, mBottom;

    LinePosition operator+(nscoord aOffset) const {
      LinePosition result;
      result.mTop = mTop + aOffset;
      result.mBaseline = mBaseline + aOffset;
      result.mBottom = mBottom + aOffset;
      return result;
    }
  };
  static bool GetFirstLinePosition(const nsIFrame* aFrame,
                                   LinePosition* aResult);


  /**
   * Derive a baseline of |aFrame| (measured from its top border edge)
   * from its last in-flow line box (not descending into anything with
   * 'overflow' not 'visible', potentially including aFrame itself).
   *
   * Returns true if a baseline was found (and fills in aResult).
   * Otherwise returns false.
   */
  static bool GetLastLineBaseline(const nsIFrame* aFrame, nscoord* aResult);

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
   * (the child of a scroll frame).
   */
  static nsIFrame* GetClosestLayer(nsIFrame* aFrame);

  /**
   * Gets the graphics filter for the frame
   */
  static GraphicsFilter GetGraphicsFilterForFrame(nsIFrame* aFrame);

  /* N.B. The only difference between variants of the Draw*Image
   * functions below is the type of the aImage argument.
   */

  /**
   * Draw a background image.  The image's dimensions are as specified in aDest;
   * the image itself is not consulted to determine a size.
   * See https://wiki.mozilla.org/Gecko:Image_Snapping_and_Rendering
   *   @param aRenderingContext Where to draw the image, set up with an
   *                            appropriate scale and transform for drawing in
   *                            app units.
   *   @param aImage            The image.
   *   @param aImageSize        The unscaled size of the image being drawn.
   *                            (This might be the image's size if no scaling
   *                            occurs, or it might be the image's size if
   *                            the image is a vector image being rendered at
   *                            that size.)
   *   @param aDest             The position and scaled area where one copy of
   *                            the image should be drawn.
   *   @param aFill             The area to be filled with copies of the
   *                            image.
   *   @param aAnchor           A point in aFill which we will ensure is
   *                            pixel-aligned in the output.
   *   @param aDirty            Pixels outside this area may be skipped.
   *   @param aImageFlags       Image flags of the imgIContainer::FLAG_* variety
   */
  static nsresult DrawBackgroundImage(nsRenderingContext* aRenderingContext,
                                      imgIContainer*      aImage,
                                      const nsIntSize&    aImageSize,
                                      GraphicsFilter      aGraphicsFilter,
                                      const nsRect&       aDest,
                                      const nsRect&       aFill,
                                      const nsPoint&      aAnchor,
                                      const nsRect&       aDirty,
                                      uint32_t            aImageFlags);

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
   *   @param aImageFlags       Image flags of the imgIContainer::FLAG_* variety
   */
  static nsresult DrawImage(nsRenderingContext* aRenderingContext,
                            imgIContainer*       aImage,
                            GraphicsFilter       aGraphicsFilter,
                            const nsRect&        aDest,
                            const nsRect&        aFill,
                            const nsPoint&       aAnchor,
                            const nsRect&        aDirty,
                            uint32_t             aImageFlags);

  /**
   * Convert an nsRect to a gfxRect.
   */
  static gfxRect RectToGfxRect(const nsRect& aRect,
                               int32_t aAppUnitsPerDevPixel);

  /**
   * Draw a drawable using the pixel snapping algorithm.
   * See https://wiki.mozilla.org/Gecko:Image_Snapping_and_Rendering
   *   @param aRenderingContext Where to draw the image, set up with an
   *                            appropriate scale and transform for drawing in
   *                            app units.
   *   @param aDrawable         The drawable we want to draw.
   *   @param aFilter           The graphics filter we should draw with.
   *   @param aDest             Where one copy of the image should mapped to.
   *   @param aFill             The area to be filled with copies of the
   *                            image.
   *   @param aAnchor           A point in aFill which we will ensure is
   *                            pixel-aligned in the output.
   *   @param aDirty            Pixels outside this area may be skipped.
   */
  static void DrawPixelSnapped(nsRenderingContext* aRenderingContext,
                               gfxDrawable*         aDrawable,
                               GraphicsFilter       aFilter,
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
   *   @param aDirty            If non-null, then pixels outside this area may
   *                            be skipped.
   *   @param aImageFlags       Image flags of the imgIContainer::FLAG_* variety
   *   @param aSourceArea       If non-null, this area is extracted from
   *                            the image and drawn at aDest. It's
   *                            in appunits. For best results it should
   *                            be aligned with image pixels.
   */
  static nsresult DrawSingleUnscaledImage(nsRenderingContext* aRenderingContext,
                                          imgIContainer*       aImage,
                                          GraphicsFilter       aGraphicsFilter,
                                          const nsPoint&       aDest,
                                          const nsRect*        aDirty,
                                          uint32_t             aImageFlags,
                                          const nsRect*        aSourceArea = nullptr);

  /**
   * Draw a whole image without tiling.
   *
   *   @param aRenderingContext Where to draw the image, set up with an
   *                            appropriate scale and transform for drawing in
   *                            app units.
   *   @param aImage            The image.
   *   @param aDest             The area that the image should fill
   *   @param aDirty            Pixels outside this area may be skipped.
   *   @param aSVGContext       If non-null, SVG-related rendering context
   *                            such as overridden attributes on the image
   *                            document's root <svg> node. Ignored for
   *                            raster images.
   *   @param aImageFlags       Image flags of the imgIContainer::FLAG_*
   *                            variety.
   *   @param aSourceArea       If non-null, this area is extracted from
   *                            the image and drawn in aDest. It's
   *                            in appunits. For best results it should
   *                            be aligned with image pixels.
   */
  static nsresult DrawSingleImage(nsRenderingContext*    aRenderingContext,
                                  imgIContainer*         aImage,
                                  GraphicsFilter         aGraphicsFilter,
                                  const nsRect&          aDest,
                                  const nsRect&          aDirty,
                                  const mozilla::SVGImageContext* aSVGContext,
                                  uint32_t               aImageFlags,
                                  const nsRect*          aSourceArea = nullptr);

  /**
   * Given an imgIContainer, this method attempts to obtain an intrinsic
   * px-valued height & width for it.  If the imgIContainer has a non-pixel
   * value for either height or width, this method tries to generate a pixel
   * value for that dimension using the intrinsic ratio (if available).  The
   * intrinsic ratio will be assigned to aIntrinsicRatio; if there's no
   * intrinsic ratio then (0, 0) will be assigned.
   *
   * This method will always set aGotWidth and aGotHeight to indicate whether
   * we were able to successfully obtain (or compute) a value for each
   * dimension.
   *
   * NOTE: This method is similar to ComputeSizeWithIntrinsicDimensions.  The
   * difference is that this one is simpler and is suited to places where we
   * have less information about the frame tree.
   */
  static void ComputeSizeForDrawing(imgIContainer* aImage,
                                    nsIntSize&     aImageSize,
                                    nsSize&        aIntrinsicRatio,
                                    bool&          aGotWidth,
                                    bool&          aGotHeight);

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
   * Determine if any corner radius is of nonzero size
   *   @param aCorners the |nsStyleCorners| object to check
   *   @return true unless all the coordinates are 0%, 0 or null.
   *
   * A corner radius with one dimension zero and one nonzero is
   * treated as a nonzero-radius corner, even though it will end up
   * being rendered like a zero-radius corner.  This is because such
   * corners are not expected to appear outside of test cases, and it's
   * simpler to implement the test this way.
   */
  static bool HasNonZeroCorner(const nsStyleCorners& aCorners);

  /**
   * Determine if there is any corner radius on corners adjacent to the
   * given side.
   */
  static bool HasNonZeroCornerOnSide(const nsStyleCorners& aCorners,
                                       mozilla::css::Side aSide);

  /**
   * Determine if a widget is likely to require transparency or translucency.
   *   @param aBackgroundFrame The frame that the background is set on. For
   *                           <window>s, this will be the canvas frame.
   *   @param aCSSRootFrame    The frame that holds CSS properties affecting
   *                           the widget's transparency. For menupopups,
   *                           aBackgroundFrame and aCSSRootFrame will be the
   *                           same.
   *   @return a value suitable for passing to SetWindowTranslucency
   */
  static nsTransparencyMode GetFrameTransparency(nsIFrame* aBackgroundFrame,
                                                 nsIFrame* aCSSRootFrame);

  /**
   * A frame is a popup if it has its own floating window. Menus, panels
   * and combobox dropdowns are popups.
   */
  static bool IsPopup(nsIFrame* aFrame);

  /**
   * Find the nearest "display root". This is the nearest enclosing
   * popup frame or the root prescontext's root frame.
   */
  static nsIFrame* GetDisplayRootFrame(nsIFrame* aFrame);

  /**
   * Get textrun construction flags determined by a given style; in particular
   * some combination of:
   * -- TEXT_DISABLE_OPTIONAL_LIGATURES if letter-spacing is in use
   * -- TEXT_OPTIMIZE_SPEED if the text-rendering CSS property and font size
   * and prefs indicate we should be optimizing for speed over quality
   */
  static uint32_t GetTextRunFlagsForStyle(nsStyleContext* aStyleContext,
                                          const nsStyleFont* aStyleFont,
                                          nscoord aLetterSpacing);

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
   * dimensions for the given window. For some reason, this is more
   * complicated than it ought to be in multi-monitor situations.
   */
  static nsDeviceContext*
  GetDeviceContextForScreenInfo(nsPIDOMWindow* aWindow);

  /**
   * Some frames with 'position: fixed' (nsStylePosition::mDisplay ==
   * NS_STYLE_POSITION_FIXED) are not really fixed positioned, since
   * they're inside an element with -moz-transform.  This function says
   * whether such an element is a real fixed-pos element.
   */
  static bool IsReallyFixedPos(nsIFrame* aFrame);

  /**
   * Return true if aFrame is in an {ib} split and is NOT one of the
   * continuations of the first inline in it.
   */
  static bool FrameIsNonFirstInIBSplit(const nsIFrame* aFrame) {
    return (aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL) &&
      aFrame->GetFirstContinuation()->
        Properties().Get(nsIFrame::IBSplitSpecialPrevSibling());
  }

  /**
   * Return true if aFrame is in an {ib} split and is NOT one of the
   * continuations of the last inline in it.
   */
  static bool FrameIsNonLastInIBSplit(const nsIFrame* aFrame) {
    return (aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL) &&
      aFrame->GetFirstContinuation()->
        Properties().Get(nsIFrame::IBSplitSpecialSibling());
  }

  /**
   * Obtain a gfxASurface from the given DOM element, if possible.
   * This obtains the most natural surface from the element; that
   * is, the one that can be obtained with the fewest conversions.
   *
   * The flags below can modify the behaviour of this function.  The
   * result is returned as a SurfaceFromElementResult struct, also
   * defined below.
   *
   * Currently, this will do:
   *  - HTML Canvas elements: will return the underlying canvas surface
   *  - HTML Video elements: will return the current video frame
   *  - Image elements: will return the image
   *
   * The above results are modified by the below flags (copying,
   * forcing image surface, etc.).
   */

  enum {
    /* Always create a new surface for the result */
    SFE_WANT_NEW_SURFACE   = 1 << 0,
    /* When creating a new surface, create an image surface */
    SFE_WANT_IMAGE_SURFACE = 1 << 1,
    /* Whether to extract the first frame (as opposed to the
       current frame) in the case that the element is an image. */
    SFE_WANT_FIRST_FRAME = 1 << 2,
    /* Whether we should skip colorspace/gamma conversion */
    SFE_NO_COLORSPACE_CONVERSION = 1 << 3,
    /* Whether we should skip premultiplication -- the resulting
       image will always be an image surface, and must not be given to
       Thebes for compositing! */
    SFE_NO_PREMULTIPLY_ALPHA = 1 << 4
  };

  struct SurfaceFromElementResult {
    SurfaceFromElementResult() :
      // Use safe default values here
      mIsWriteOnly(true), mIsStillLoading(false), mCORSUsed(false) {}

    /* mSurface will contain the resulting surface, or will be NULL on error */
    nsRefPtr<gfxASurface> mSurface;
    /* The size of the surface */
    gfxIntSize mSize;
    /* The principal associated with the element whose surface was returned.
       If there is a surface, this will never be null. */
    nsCOMPtr<nsIPrincipal> mPrincipal;
    /* The image request, if the element is an nsIImageLoadingContent */
    nsCOMPtr<imgIRequest> mImageRequest;
    /* Whether the element was "write only", that is, the bits should not be exposed to content */
    bool mIsWriteOnly;
    /* Whether the element was still loading.  Some consumers need to handle
       this case specially. */
    bool mIsStillLoading;
    /* Whether the element used CORS when loading. */
    bool mCORSUsed;
  };

  static SurfaceFromElementResult SurfaceFromElement(mozilla::dom::Element *aElement,
                                                     uint32_t aSurfaceFlags = 0);
  static SurfaceFromElementResult SurfaceFromElement(nsIImageLoadingContent *aElement,
                                                     uint32_t aSurfaceFlags = 0);
  // Need an HTMLImageElement overload, because otherwise the
  // nsIImageLoadingContent and mozilla::dom::Element overloads are ambiguous
  // for HTMLImageElement.
  static SurfaceFromElementResult SurfaceFromElement(mozilla::dom::HTMLImageElement *aElement,
                                                     uint32_t aSurfaceFlags = 0);
  static SurfaceFromElementResult SurfaceFromElement(mozilla::dom::HTMLCanvasElement *aElement,
                                                     uint32_t aSurfaceFlags = 0);
  static SurfaceFromElementResult SurfaceFromElement(mozilla::dom::HTMLVideoElement *aElement,
                                                     uint32_t aSurfaceFlags = 0);

  /**
   * When the document is editable by contenteditable attribute of its root
   * content or body content.
   *
   * Be aware, this returns NULL if it's in designMode.
   *
   * For example:
   *
   *  <html contenteditable="true"><body></body></html>
   *    returns the <html>.
   *
   *  <html><body contenteditable="true"></body></html>
   *  <body contenteditable="true"></body>
   *    With these cases, this returns the <body>.
   *    NOTE: The latter case isn't created normally, however, it can be
   *          created by script with XHTML.
   *
   *  <body><p contenteditable="true"></p></body>
   *    returns NULL because <body> isn't editable.
   */
  static nsIContent*
    GetEditableRootContentByContentEditable(nsIDocument* aDocument);

  /**
   * Returns true if the passed in prescontext needs the dark grey background
   * that goes behind the page of a print preview presentation.
   */
  static bool NeedsPrintPreviewBackground(nsPresContext* aPresContext) {
    return aPresContext->IsRootPaginatedDocument() &&
      (aPresContext->Type() == nsPresContext::eContext_PrintPreview ||
       aPresContext->Type() == nsPresContext::eContext_PageLayout);
  }

  /**
   * Adds all font faces used in the frame tree starting from aFrame
   * to the list aFontFaceList.
   */
  static nsresult GetFontFacesForFrames(nsIFrame* aFrame,
                                        nsFontFaceList* aFontFaceList);

  /**
   * Adds all font faces used within the specified range of text in aFrame,
   * and optionally its continuations, to the list in aFontFaceList.
   * Pass 0 and INT32_MAX for aStartOffset and aEndOffset to specify the
   * entire text is to be considered.
   */
  static nsresult GetFontFacesForText(nsIFrame* aFrame,
                                      int32_t aStartOffset,
                                      int32_t aEndOffset,
                                      bool aFollowContinuations,
                                      nsFontFaceList* aFontFaceList);

  /**
   * Walks the frame tree starting at aFrame looking for textRuns.
   * If |clear| is true, just clears the TEXT_RUN_MEMORY_ACCOUNTED flag
   * on each textRun found (and |aMallocSizeOf| is not used).
   * If |clear| is false, adds the storage used for each textRun to the
   * total, and sets the TEXT_RUN_MEMORY_ACCOUNTED flag to avoid double-
   * accounting. (Runs with this flag already set will be skipped.)
   * Expected usage pattern is therefore to call twice:
   *    (void)SizeOfTextRunsForFrames(rootFrame, nullptr, true);
   *    total = SizeOfTextRunsForFrames(rootFrame, mallocSizeOf, false);
   */
  static size_t SizeOfTextRunsForFrames(nsIFrame* aFrame,
                                        mozilla::MallocSizeOf aMallocSizeOf,
                                        bool clear);

  /**
   * Returns true if the content node has animations or transitions that can be
   * performed on the compositor.
   */
  static bool HasAnimationsForCompositor(nsIContent* aContent,
                                         nsCSSProperty aProperty);

  /**
   * Checks if off-main-thread animations are enabled.
   */
  static bool AreAsyncAnimationsEnabled();

  /**
   * Checks if we should warn about animations that can't be async
   */
  static bool IsAnimationLoggingEnabled();

  /**
   * Find the maximum scale for an element (aContent) over the course of any
   * animations and transitions on the element. Will return 1,1 if there is no
   * animated scaling.
   */
  static gfxSize GetMaximumAnimatedScale(nsIContent* aContent);

  /**
   * Checks if we should forcibly use nearest pixel filtering for the
   * background.
   */
  static bool UseBackgroundNearestFiltering();

  /**
   * Checks whether we want to use the GPU to scale images when
   * possible.
   */
  static bool GPUImageScalingEnabled();

  /**
   * Checks whether we want to layerize animated images whenever possible.
   */
  static bool AnimatedImageLayersEnabled();

  /**
   * Checks if we should enable parsing for CSS Filters.
   */
  static bool CSSFiltersEnabled();

  /**
   * Unions the overflow areas of all non-popup children of aFrame with
   * aOverflowAreas.
   */
  static void UnionChildOverflow(nsIFrame* aFrame,
                                 nsOverflowAreas& aOverflowAreas);

  /**
   * Return whether this is a frame whose width is used when computing
   * the font size inflation of its descendants.
   */
  static bool IsContainerForFontSizeInflation(const nsIFrame *aFrame)
  {
    return aFrame->GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER;
  }

  /**
   * Return the font size inflation *ratio* for a given frame.  This is
   * the factor by which font sizes should be inflated; it is never
   * smaller than 1.
   */
  static float FontSizeInflationFor(const nsIFrame *aFrame);

  /**
   * Perform the first half of the computation of FontSizeInflationFor
   * (see above).
   * This includes determining whether inflation should be performed
   * within this container and returning 0 if it should not be.
   *
   * The result is guaranteed not to vary between line participants
   * (inlines, text frames) within a line.
   *
   * The result should not be used directly since font sizes slightly
   * above the minimum should always be adjusted as done by
   * FontSizeInflationInner.
   */
  static nscoord InflationMinFontSizeFor(const nsIFrame *aFrame);

  /**
   * Perform the second half of the computation done by
   * FontSizeInflationFor (see above).
   *
   * aMinFontSize must be the result of one of the
   *   InflationMinFontSizeFor methods above.
   */
  static float FontSizeInflationInner(const nsIFrame *aFrame,
                                      nscoord aMinFontSize);

  static bool FontSizeInflationEnabled(nsPresContext *aPresContext);

  /**
   * See comment above "font.size.inflation.maxRatio" in
   * modules/libpref/src/init/all.js .
   */
  static uint32_t FontSizeInflationMaxRatio() {
    return sFontSizeInflationMaxRatio;
  }

  /**
   * See comment above "font.size.inflation.emPerLine" in
   * modules/libpref/src/init/all.js .
   */
  static uint32_t FontSizeInflationEmPerLine() {
    return sFontSizeInflationEmPerLine;
  }

  /**
   * See comment above "font.size.inflation.minTwips" in
   * modules/libpref/src/init/all.js .
   */
  static uint32_t FontSizeInflationMinTwips() {
    return sFontSizeInflationMinTwips;
  }

  /**
   * See comment above "font.size.inflation.lineThreshold" in
   * modules/libpref/src/init/all.js .
   */
  static uint32_t FontSizeInflationLineThreshold() {
    return sFontSizeInflationLineThreshold;
  }

  static bool FontSizeInflationForceEnabled() {
    return sFontSizeInflationForceEnabled;
  }

  static bool FontSizeInflationDisabledInMasterProcess() {
    return sFontSizeInflationDisabledInMasterProcess;
  }

  /**
   * See comment above "font.size.inflation.mappingIntercept" in
   * modules/libpref/src/init/all.js .
   */
  static int32_t FontSizeInflationMappingIntercept() {
    return sFontSizeInflationMappingIntercept;
  }

  /**
   * Returns true if the nglayout.debug.invalidation pref is set to true.
   * Note that sInvalidationDebuggingIsEnabled is declared outside this function to
   * allow it to be accessed an manipulated from breakpoint conditions.
   */
  static bool InvalidationDebuggingIsEnabled() {
    return sInvalidationDebuggingIsEnabled || getenv("MOZ_DUMP_INVALIDATION") != 0;
  }

  static void Initialize();
  static void Shutdown();

  /**
   * Register an imgIRequest object with a refresh driver.
   *
   * @param aPresContext The nsPresContext whose refresh driver we want to
   *        register with.
   * @param aRequest A pointer to the imgIRequest object which the client wants
   *        to register with the refresh driver.
   * @param aRequestRegistered A pointer to a boolean value which indicates
   *        whether the given image request is registered. If
   *        *aRequestRegistered is true, then this request will not be
   *        registered again. If the request is registered by this function,
   *        then *aRequestRegistered will be set to true upon the completion of
   *        this function.
   *
   */
  static void RegisterImageRequest(nsPresContext* aPresContext,
                                   imgIRequest* aRequest,
                                   bool* aRequestRegistered);

  /**
   * Register an imgIRequest object with a refresh driver, but only if the
   * request is for an image that is animated.
   *
   * @param aPresContext The nsPresContext whose refresh driver we want to
   *        register with.
   * @param aRequest A pointer to the imgIRequest object which the client wants
   *        to register with the refresh driver.
   * @param aRequestRegistered A pointer to a boolean value which indicates
   *        whether the given image request is registered. If
   *        *aRequestRegistered is true, then this request will not be
   *        registered again. If the request is registered by this function,
   *        then *aRequestRegistered will be set to true upon the completion of
   *        this function.
   *
   */
  static void RegisterImageRequestIfAnimated(nsPresContext* aPresContext,
                                             imgIRequest* aRequest,
                                             bool* aRequestRegistered);

  /**
   * Deregister an imgIRequest object from a refresh driver.
   *
   * @param aPresContext The nsPresContext whose refresh driver we want to
   *        deregister from.
   * @param aRequest A pointer to the imgIRequest object with which the client
   *        previously registered and now wants to deregister from the refresh
   *        driver.
   * @param aRequestRegistered A pointer to a boolean value which indicates
   *        whether the given image request is registered. If
   *        *aRequestRegistered is false, then this request will not be
   *        deregistered. If the request is deregistered by this function,
   *        then *aRequestRegistered will be set to false upon the completion of
   *        this function.
   */
  static void DeregisterImageRequest(nsPresContext* aPresContext,
                                     imgIRequest* aRequest,
                                     bool* aRequestRegistered);

  /**
   * Shim to nsCSSFrameConstructor::PostRestyleEvent. Exists so that we
   * can avoid including nsCSSFrameConstructor.h and all its dependencies
   * in content files.
   */
  static void PostRestyleEvent(mozilla::dom::Element* aElement,
                               nsRestyleHint aRestyleHint,
                               nsChangeHint aMinChangeHint);

  /**
   * Updates a pair of x and y distances if a given point is closer to a given
   * rectangle than the original distance values.  If aPoint is closer to
   * aRect than aClosestXDistance and aClosestYDistance indicate, then those
   * two variables are updated with the distance between aPoint and aRect,
   * and true is returned.  If aPoint is not closer, then aClosestXDistance
   * and aClosestYDistance are left unchanged, and false is returned.
   *
   * Distances are measured in the two dimensions separately; a closer x
   * distance beats a closer y distance.
   */
  template<typename PointType, typename RectType, typename CoordType>
  static bool PointIsCloserToRect(PointType aPoint, const RectType& aRect,
                                  CoordType& aClosestXDistance,
                                  CoordType& aClosestYDistance);
  /**
   * Computes the box shadow rect for the frame, or returns an empty rect if
   * there are no shadows.
   *
   * @param aFrame Frame to compute shadows for.
   * @param aFrameSize Size of aFrame (in case it hasn't been set yet).
   */
  static nsRect GetBoxShadowRectForFrame(nsIFrame* aFrame, const nsSize& aFrameSize);

#ifdef DEBUG
  /**
   * Assert that there are no duplicate continuations of the same frame
   * within aFrameList.  Optimize the tests by assuming that all frames
   * in aFrameList have parent aContainer.
   */
  static void
  AssertNoDuplicateContinuations(nsIFrame* aContainer,
                                 const nsFrameList& aFrameList);

  /**
   * Assert that the frame tree rooted at |aSubtreeRoot| is empty, i.e.,
   * that it contains no first-in-flows.
   */
  static void
  AssertTreeOnlyEmptyNextInFlows(nsIFrame *aSubtreeRoot);
#endif

private:
  // Helper-functions for SortFrameList():
  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  static nsIFrame* SortedMerge(nsIFrame *aLeft, nsIFrame *aRight);

  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  static nsIFrame* MergeSort(nsIFrame *aSource);

  static uint32_t sFontSizeInflationEmPerLine;
  static uint32_t sFontSizeInflationMinTwips;
  static uint32_t sFontSizeInflationLineThreshold;
  static int32_t  sFontSizeInflationMappingIntercept;
  static uint32_t sFontSizeInflationMaxRatio;
  static bool sFontSizeInflationForceEnabled;
  static bool sFontSizeInflationDisabledInMasterProcess;
  static bool sInvalidationDebuggingIsEnabled;
};

// Helper-functions for nsLayoutUtils::SortFrameList()
// ---------------------------------------------------

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ nsIFrame*
nsLayoutUtils::SortedMerge(nsIFrame *aLeft, nsIFrame *aRight)
{
  NS_PRECONDITION(aLeft && aRight, "SortedMerge must have non-empty lists");

  nsIFrame *result;
  // Unroll first iteration to avoid null-check 'result' inside the loop.
  if (IsLessThanOrEqual(aLeft, aRight)) {
    result = aLeft;
    aLeft = aLeft->GetNextSibling();
    if (!aLeft) {
      result->SetNextSibling(aRight);
      return result;
    }
  }
  else {
    result = aRight;
    aRight = aRight->GetNextSibling();
    if (!aRight) {
      result->SetNextSibling(aLeft);
      return result;
    }
  }

  nsIFrame *last = result;
  for (;;) {
    if (IsLessThanOrEqual(aLeft, aRight)) {
      last->SetNextSibling(aLeft);
      last = aLeft;
      aLeft = aLeft->GetNextSibling();
      if (!aLeft) {
        last->SetNextSibling(aRight);
        return result;
      }
    }
    else {
      last->SetNextSibling(aRight);
      last = aRight;
      aRight = aRight->GetNextSibling();
      if (!aRight) {
        last->SetNextSibling(aLeft);
        return result;
      }
    }
  }
}

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ nsIFrame*
nsLayoutUtils::MergeSort(nsIFrame *aSource)
{
  NS_PRECONDITION(aSource, "MergeSort null arg");

  nsIFrame *sorted[32] = { nullptr };
  nsIFrame **fill = &sorted[0];
  nsIFrame **left;
  nsIFrame *rest = aSource;

  do {
    nsIFrame *current = rest;
    rest = rest->GetNextSibling();
    current->SetNextSibling(nullptr);

    // Merge it with sorted[0] if present; then merge the result with sorted[1] etc.
    // sorted[0] is a list of length 1 (or nullptr).
    // sorted[1] is a list of length 2 (or nullptr).
    // sorted[2] is a list of length 4 (or nullptr). etc.
    for (left = &sorted[0]; left != fill && *left; ++left) {
      current = SortedMerge<IsLessThanOrEqual>(*left, current);
      *left = nullptr;
    }

    // Fill the empty slot that we couldn't merge with the last result.
    *left = current;

    if (left == fill)
      ++fill;
  } while (rest);

  // Collect and merge the results.
  nsIFrame *result = nullptr;
  for (left = &sorted[0]; left != fill; ++left) {
    if (*left) {
      result = result ? SortedMerge<IsLessThanOrEqual>(*left, result) : *left;
    }
  }
  return result;
}

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ void
nsLayoutUtils::SortFrameList(nsFrameList& aFrameList)
{
  nsIFrame* head = MergeSort<IsLessThanOrEqual>(aFrameList.FirstChild());
  aFrameList = nsFrameList(head, GetLastSibling(head));
  MOZ_ASSERT(IsFrameListSorted<IsLessThanOrEqual>(aFrameList),
             "After we sort a frame list, it should be in sorted order...");
}

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ bool
nsLayoutUtils::IsFrameListSorted(nsFrameList& aFrameList)
{
  if (aFrameList.IsEmpty()) {
    // empty lists are trivially sorted.
    return true;
  }

  // We'll walk through the list with two iterators, one trailing behind the
  // other. The list is sorted IFF trailingIter <= iter, across the whole list.
  nsFrameList::Enumerator trailingIter(aFrameList);
  nsFrameList::Enumerator iter(aFrameList);
  iter.Next(); // Skip |iter| past first frame. (List is nonempty, so we can.)

  // Now, advance the iterators in parallel, comparing each adjacent pair.
  while (!iter.AtEnd()) {
    MOZ_ASSERT(!trailingIter.AtEnd(), "trailing iter shouldn't finish first");
    if (!IsLessThanOrEqual(trailingIter.get(), iter.get())) {
      return false;
    }
    trailingIter.Next();
    iter.Next();
  }

  // We made it to the end without returning early, so the list is sorted.
  return true;
}

template<typename PointType, typename RectType, typename CoordType>
/* static */ bool
nsLayoutUtils::PointIsCloserToRect(PointType aPoint, const RectType& aRect,
                                   CoordType& aClosestXDistance,
                                   CoordType& aClosestYDistance)
{
  CoordType fromLeft = aPoint.x - aRect.x;
  CoordType fromRight = aPoint.x - aRect.XMost();

  CoordType xDistance;
  if (fromLeft >= 0 && fromRight <= 0) {
    xDistance = 0;
  } else {
    xDistance = std::min(abs(fromLeft), abs(fromRight));
  }

  if (xDistance <= aClosestXDistance) {
    if (xDistance < aClosestXDistance) {
      aClosestYDistance = std::numeric_limits<CoordType>::max();
    }

    CoordType fromTop = aPoint.y - aRect.y;
    CoordType fromBottom = aPoint.y - aRect.YMost();

    CoordType yDistance;
    if (fromTop >= 0 && fromBottom <= 0) {
      yDistance = 0;
    } else {
      yDistance = std::min(abs(fromTop), abs(fromBottom));
    }

    if (yDistance < aClosestYDistance) {
      aClosestXDistance = xDistance;
      aClosestYDistance = yDistance;
      return true;
    }
  }

  return false;
}

namespace mozilla {
  namespace layout {

    /**
     * An RAII class which will, for the duration of its lifetime,
     * **if** the frame given is a container for font size inflation,
     * set the current inflation container on the pres context to null
     * (and then, in its destructor, restore the old value).
     */
    class AutoMaybeDisableFontInflation {
    public:
      AutoMaybeDisableFontInflation(nsIFrame *aFrame)
      {
        // FIXME: Now that inflation calculations are based on the flow
        // root's NCA's (nearest common ancestor of its inflatable
        // descendants) width, we could probably disable inflation in
        // fewer cases than we currently do.
        if (nsLayoutUtils::IsContainerForFontSizeInflation(aFrame)) {
          mPresContext = aFrame->PresContext();
          mOldValue = mPresContext->mInflationDisabledForShrinkWrap;
          mPresContext->mInflationDisabledForShrinkWrap = true;
        } else {
          // indicate we have nothing to restore
          mPresContext = nullptr;
        }
      }

      ~AutoMaybeDisableFontInflation()
      {
        if (mPresContext) {
          mPresContext->mInflationDisabledForShrinkWrap = mOldValue;
        }
      }
    private:
      nsPresContext *mPresContext;
      bool mOldValue;
    };

  }
}

class nsSetAttrRunnable : public nsRunnable
{
public:
  nsSetAttrRunnable(nsIContent* aContent, nsIAtom* aAttrName,
                    const nsAString& aValue);
  nsSetAttrRunnable(nsIContent* aContent, nsIAtom* aAttrName,
                    int32_t aValue);

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

class nsReflowFrameRunnable : public nsRunnable
{
public:
  nsReflowFrameRunnable(nsIFrame* aFrame,
                        nsIPresShell::IntrinsicDirty aIntrinsicDirty,
                        nsFrameState aBitToAdd);

  NS_DECL_NSIRUNNABLE

  nsWeakFrame mWeakFrame;
  nsIPresShell::IntrinsicDirty mIntrinsicDirty;
  nsFrameState mBitToAdd;
};

#endif // nsLayoutUtils_h__
