/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGTEXTFRAME_H_
#define LAYOUT_SVG_SVGTEXTFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/PresShellForwards.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SVGContainerFrame.h"
#include "mozilla/gfx/2D.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "gfxTextRun.h"
#include "nsIContent.h"  // for GetContent
#include "nsStubMutationObserver.h"
#include "nsTextFrame.h"

class gfxContext;

namespace mozilla {

class CharIterator;
class DisplaySVGText;
class SVGTextFrame;
class TextFrameIterator;
class TextNodeCorrespondenceRecorder;
struct TextRenderedRun;
class TextRenderedRunIterator;

namespace dom {
struct DOMPointInit;
class DOMSVGPoint;
class SVGRect;
class SVGGeometryElement;
}  // namespace dom
}  // namespace mozilla

nsIFrame* NS_NewSVGTextFrame(mozilla::PresShell* aPresShell,
                             mozilla::ComputedStyle* aStyle);

namespace mozilla {

/**
 * Information about the positioning for a single character in an SVG <text>
 * element.
 *
 * During SVG text layout, we use infinity values to represent positions and
 * rotations that are not explicitly specified with x/y/rotate attributes.
 */
struct CharPosition {
  CharPosition()
      : mAngle(0),
        mHidden(false),
        mUnaddressable(false),
        mClusterOrLigatureGroupMiddle(false),
        mRunBoundary(false),
        mStartOfChunk(false) {}

  CharPosition(gfxPoint aPosition, double aAngle)
      : mPosition(aPosition),
        mAngle(aAngle),
        mHidden(false),
        mUnaddressable(false),
        mClusterOrLigatureGroupMiddle(false),
        mRunBoundary(false),
        mStartOfChunk(false) {}

  static CharPosition Unspecified(bool aUnaddressable) {
    CharPosition cp(UnspecifiedPoint(), UnspecifiedAngle());
    cp.mUnaddressable = aUnaddressable;
    return cp;
  }

  bool IsAngleSpecified() const { return mAngle != UnspecifiedAngle(); }

  bool IsXSpecified() const { return mPosition.x != UnspecifiedCoord(); }

  bool IsYSpecified() const { return mPosition.y != UnspecifiedCoord(); }

  gfxPoint mPosition;
  double mAngle;

  // not displayed due to falling off the end of a <textPath>
  bool mHidden;

  // skipped in positioning attributes due to being collapsed-away white space
  bool mUnaddressable;

  // a preceding character is what positioning attributes address
  bool mClusterOrLigatureGroupMiddle;

  // rendering is split here since an explicit position or rotation was given
  bool mRunBoundary;

  // an anchored chunk begins here
  bool mStartOfChunk;

 private:
  static gfxFloat UnspecifiedCoord() {
    return std::numeric_limits<gfxFloat>::infinity();
  }

  static double UnspecifiedAngle() {
    return std::numeric_limits<double>::infinity();
  }

  static gfxPoint UnspecifiedPoint() {
    return gfxPoint(UnspecifiedCoord(), UnspecifiedCoord());
  }
};

/**
 * A runnable to mark glyph positions as needing to be recomputed
 * and to invalid the bounds of the SVGTextFrame frame.
 */
class GlyphMetricsUpdater : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE
  explicit GlyphMetricsUpdater(SVGTextFrame* aFrame)
      : Runnable("GlyphMetricsUpdater"), mFrame(aFrame) {}
  static void Run(SVGTextFrame* aFrame);
  void Revoke() { mFrame = nullptr; }

 private:
  SVGTextFrame* mFrame;
};

/**
 * Frame class for SVG <text> elements.
 *
 * An SVGTextFrame manages SVG text layout, painting and interaction for
 * all descendent text content elements.  The frame tree will look like this:
 *
 *   SVGTextFrame                     -- for <text>
 *     <anonymous block frame>
 *       ns{Block,Inline,Text}Frames  -- for text nodes, <tspan>s, <a>s, etc.
 *
 * SVG text layout is done by:
 *
 *   1. Reflowing the anonymous block frame.
 *   2. Inspecting the (app unit) positions of the glyph for each character in
 *      the nsTextFrames underneath the anonymous block frame.
 *   3. Determining the (user unit) positions for each character in the <text>
 *      using the x/y/dx/dy/rotate attributes on all the text content elements,
 *      and using the step 2 results to fill in any gaps.
 *   4. Applying any other SVG specific text layout (anchoring and text paths)
 *      to the positions computed in step 3.
 *
 * Rendering of the text is done by splitting up each nsTextFrame into ranges
 * that can be contiguously painted.  (For example <text x="10 20">abcd</text>
 * would have two contiguous ranges: one for the "a" and one for the "bcd".)
 * Each range is called a "text rendered run", represented by a TextRenderedRun
 * object.  The TextRenderedRunIterator class performs that splitting and
 * returns a TextRenderedRun for each bit of text to be painted separately.
 *
 * Each rendered run is painted by calling nsTextFrame::PaintText.  If the text
 * formatting is simple enough (solid fill, no stroking, etc.), PaintText will
 * itself do the painting.  Otherwise, a DrawPathCallback is passed to
 * PaintText so that we can fill the text geometry with SVG paint servers.
 */
class SVGTextFrame final : public SVGDisplayContainerFrame {
  friend nsIFrame* ::NS_NewSVGTextFrame(mozilla::PresShell* aPresShell,
                                        ComputedStyle* aStyle);

  friend class CharIterator;
  friend class DisplaySVGText;
  friend class GlyphMetricsUpdater;
  friend class MutationObserver;
  friend class TextFrameIterator;
  friend class TextNodeCorrespondenceRecorder;
  friend struct TextRenderedRun;
  friend class TextRenderedRunIterator;

  using Range = gfxTextRun::Range;
  using DrawTarget = gfx::DrawTarget;
  using Path = gfx::Path;
  using Point = gfx::Point;
  using Rect = gfx::Rect;

 protected:
  explicit SVGTextFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGDisplayContainerFrame(aStyle, aPresContext, kClassID),
        mTrailingUndisplayedCharacters(0),
        mFontSizeScaleFactor(1.0f),
        mLastContextScale(1.0f),
        mLengthAdjustScaleFactor(1.0f) {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_SVG_TEXT |
                 NS_STATE_SVG_TEXT_CORRESPONDENCE_DIRTY |
                 NS_STATE_SVG_POSITIONING_DIRTY);
  }

  ~SVGTextFrame() = default;

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGTextFrame)

  // nsIFrame:
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  nsresult AttributeChanged(int32_t aNamespaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGText"_ns, aResult);
  }
#endif

  /**
   * Finds the nsTextFrame for the closest rendered run to the specified point.
   */
  void FindCloserFrameForSelection(
      const nsPoint& aPoint, FrameWithDistance* aCurrentBestFrame) override;

  // ISVGDisplayableFrame interface:
  void NotifySVGChanged(uint32_t aFlags) override;
  void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                imgDrawingParams& aImgParams) override;
  nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  void ReflowSVG() override;
  SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                              uint32_t aFlags) override;

  // SVG DOM text methods:
  uint32_t GetNumberOfChars(nsIContent* aContent);
  float GetComputedTextLength(nsIContent* aContent);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void SelectSubString(nsIContent* aContent,
                                                   uint32_t charnum,
                                                   uint32_t nchars,
                                                   ErrorResult& aRv);
  bool RequiresSlowFallbackForSubStringLength();
  float GetSubStringLengthFastPath(nsIContent* aContent, uint32_t charnum,
                                   uint32_t nchars, ErrorResult& aRv);
  /**
   * This fallback version of GetSubStringLength takes
   * into account glyph positioning and requires us to have flushed layout
   * before calling it. As per the SVG 2 spec, typically glyph
   * positioning does not affect the results of getSubStringLength, but one
   * exception is text in a textPath where we need to ignore characters that
   * fall off the end of the textPath path.
   */
  float GetSubStringLengthSlowFallback(nsIContent* aContent, uint32_t charnum,
                                       uint32_t nchars, ErrorResult& aRv);

  int32_t GetCharNumAtPosition(nsIContent* aContent,
                               const dom::DOMPointInit& aPoint);

  already_AddRefed<dom::DOMSVGPoint> GetStartPositionOfChar(
      nsIContent* aContent, uint32_t aCharNum, ErrorResult& aRv);
  already_AddRefed<dom::DOMSVGPoint> GetEndPositionOfChar(nsIContent* aContent,
                                                          uint32_t aCharNum,
                                                          ErrorResult& aRv);
  already_AddRefed<dom::SVGRect> GetExtentOfChar(nsIContent* aContent,
                                                 uint32_t aCharNum,
                                                 ErrorResult& aRv);
  float GetRotationOfChar(nsIContent* aContent, uint32_t aCharNum,
                          ErrorResult& aRv);

  // SVGTextFrame methods:

  /**
   * Handles a base or animated attribute value change to a descendant
   * text content element.
   */
  void HandleAttributeChangeInDescendant(dom::Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsAtom* aAttribute);

  /**
   * Calls ScheduleReflowSVGNonDisplayText if this is a non-display frame,
   * and SVGUtils::ScheduleReflowSVG otherwise.
   */
  void ScheduleReflowSVG();

  /**
   * Reflows the anonymous block frame of this non-display SVGTextFrame.
   *
   * When we are under SVGDisplayContainerFrame::ReflowSVG, we need to
   * reflow any SVGTextFrame frames in the subtree in case they are
   * being observed (by being for example in a <mask>) and the change
   * that caused the reflow would not already have caused a reflow.
   *
   * Note that displayed SVGTextFrames are reflowed as needed, when PaintSVG
   * is called or some SVG DOM method is called on the element.
   */
  void ReflowSVGNonDisplayText();

  /**
   * This is a function that behaves similarly to SVGUtils::ScheduleReflowSVG,
   * but which will skip over any ancestor non-display container frames on the
   * way to the SVGOuterSVGFrame.  It exists for the situation where a
   * non-display <text> element has changed and needs to ensure ReflowSVG will
   * be called on its closest display container frame, so that
   * SVGDisplayContainerFrame::ReflowSVG will call ReflowSVGNonDisplayText on
   * it.
   *
   * We have to do this in two cases: in response to a style change on a
   * non-display <text>, where aReason will be
   * IntrinsicDirty::FrameAncestorsAndDescendants (the common case), and also in
   * response to glyphs changes on non-display <text> (i.e., animated
   * SVG-in-OpenType glyphs), in which case aReason will be None, since layout
   * doesn't need to be recomputed.
   */
  void ScheduleReflowSVGNonDisplayText(IntrinsicDirty aReason);

  /**
   * Updates the mFontSizeScaleFactor value by looking at the range of
   * font-sizes used within the <text>.
   *
   * @return Whether mFontSizeScaleFactor changed.
   */
  bool UpdateFontSizeScaleFactor();

  double GetFontSizeScaleFactor() const;

  /**
   * Takes a point from the <text> element's user space and
   * converts it to the appropriate frame user space of aChildFrame,
   * according to which rendered run the point hits.
   */
  Point TransformFramePointToTextChild(const Point& aPoint,
                                       const nsIFrame* aChildFrame);

  /**
   * Takes an app unit rectangle in the coordinate space of a given descendant
   * frame of this frame, and returns a rectangle in the <text> element's user
   * space that covers all parts of rendered runs that intersect with the
   * rectangle.
   */
  gfxRect TransformFrameRectFromTextChild(const nsRect& aRect,
                                          const nsIFrame* aChildFrame);

  /** As above, but taking and returning a device px rect. */
  Rect TransformFrameRectFromTextChild(const Rect& aRect,
                                       const nsIFrame* aChildFrame);

  /** As above, but with a single point */
  Point TransformFramePointFromTextChild(const Point& aPoint,
                                         const nsIFrame* aChildFrame);

  // Return our ::-moz-svg-text anonymous box.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

 private:
  /**
   * Mutation observer used to watch for text positioning attribute changes
   * on descendent text content elements (like <tspan>s).
   */
  class MutationObserver final : public nsStubMutationObserver {
   public:
    explicit MutationObserver(SVGTextFrame* aFrame) : mFrame(aFrame) {
      MOZ_ASSERT(mFrame, "MutationObserver needs a non-null frame");
      mFrame->GetContent()->AddMutationObserver(this);
    }

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIMutationObserver
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

   private:
    ~MutationObserver() { mFrame->GetContent()->RemoveMutationObserver(this); }

    SVGTextFrame* const mFrame;
  };

  /**
   * Resolves Bidi for the anonymous block child if it needs it.
   */
  void MaybeResolveBidiForAnonymousBlockChild();

  /**
   * Reflows the anonymous block child if it is dirty or has dirty
   * children, or if the SVGTextFrame itself is dirty.
   */
  void MaybeReflowAnonymousBlockChild();

  /**
   * Performs the actual work of reflowing the anonymous block child.
   */
  void DoReflow();

  /**
   * Schedules mPositions to be recomputed and the covered region to be
   * updated.
   */
  void NotifyGlyphMetricsChange(bool aUpdateTextCorrespondence);

  /**
   * Recomputes mPositions by calling DoGlyphPositioning if this information
   * is out of date.
   */
  void UpdateGlyphPositioning();

  /**
   * Populates mPositions with positioning information for each character
   * within the <text>.
   */
  void DoGlyphPositioning();

  /**
   * Converts the specified index into mPositions to an addressable
   * character index (as can be used with the SVG DOM text methods)
   * relative to the specified text child content element.
   *
   * @param aIndex The global character index.
   * @param aContent The descendant text child content element that
   *   the returned addressable index will be relative to; null
   *   means the same as the <text> element.
   * @return The addressable index, or -1 if the index cannot be
   *   represented as an addressable index relative to aContent.
   */
  int32_t ConvertTextElementCharIndexToAddressableIndex(int32_t aIndex,
                                                        nsIContent* aContent);

  /**
   * Recursive helper for ResolvePositions below.
   *
   * @param aContent The current node.
   * @param aIndex (in/out) The current character index.
   * @param aInTextPath Whether we are currently under a <textPath> element.
   * @param aForceStartOfChunk (in/out) Whether the next character we find
   *   should start a new anchored chunk.
   * @param aDeltas (in/out) Receives the resolved dx/dy values for each
   *   character.
   * @return false if we discover that mPositions did not have enough
   *   elements; true otherwise.
   */
  bool ResolvePositionsForNode(nsIContent* aContent, uint32_t& aIndex,
                               bool aInTextPath, bool& aForceStartOfChunk,
                               nsTArray<gfxPoint>& aDeltas);

  /**
   * Initializes mPositions with character position information based on
   * x/y/rotate attributes, leaving unspecified values in the array if a
   * position was not given for that character.  Also fills aDeltas with values
   * based on dx/dy attributes.
   *
   * @param aDeltas (in/out) Receives the resolved dx/dy values for each
   *   character.
   * @param aRunPerGlyph Whether mPositions should record that a new run begins
   *   at each glyph.
   * @return false if we did not record any positions (due to having no
   *   displayed characters) or if we discover that mPositions did not have
   *   enough elements; true otherwise.
   */
  bool ResolvePositions(nsTArray<gfxPoint>& aDeltas, bool aRunPerGlyph);

  /**
   * Determines the position, in app units, of each character in the <text> as
   * laid out by reflow, and appends them to aPositions.  Any characters that
   * are undisplayed or trimmed away just get the last position.
   */
  void DetermineCharPositions(nsTArray<nsPoint>& aPositions);

  /**
   * Sets mStartOfChunk to true for each character in mPositions that starts a
   * line of text.
   */
  void AdjustChunksForLineBreaks();

  /**
   * Adjusts recorded character positions in mPositions to account for glyph
   * boundaries.  Four things are done:
   *
   *   1. mClusterOrLigatureGroupMiddle is set to true for all such characters.
   *
   *   2. Any run and anchored chunk boundaries that begin in the middle of a
   *      cluster/ligature group get moved to the start of the next
   *      cluster/ligature group.
   *
   *   3. The position of any character in the middle of a cluster/ligature
   *      group is updated to take into account partial ligatures and any
   *      rotation the glyph as a whole has.  (The values that come out of
   *      DetermineCharPositions which then get written into mPositions in
   *      ResolvePositions store the same position value for each part of the
   *      ligature.)
   *
   *   4. The rotation of any character in the middle of a cluster/ligature
   *      group is set to the rotation of the first character.
   */
  void AdjustPositionsForClusters();

  /**
   * Updates the character positions stored in mPositions to account for
   * text anchoring.
   */
  void DoAnchoring();

  /**
   * Updates character positions in mPositions for those characters inside a
   * <textPath>.
   */
  void DoTextPathLayout();

  /**
   * Returns whether we need to render the text using
   * nsTextFrame::DrawPathCallbacks rather than directly painting
   * the text frames.
   *
   * @param aShouldPaintSVGGlyphs (out) Whether SVG glyphs in the text
   *   should be painted.
   */
  bool ShouldRenderAsPath(nsTextFrame* aFrame, bool& aShouldPaintSVGGlyphs);

  // Methods to get information for a <textPath> frame.
  already_AddRefed<Path> GetTextPath(nsIFrame* aTextPathFrame);
  gfxFloat GetOffsetScale(nsIFrame* aTextPathFrame);
  gfxFloat GetStartOffset(nsIFrame* aTextPathFrame);

  /**
   * The MutationObserver we have registered for the <text> element subtree.
   */
  RefPtr<MutationObserver> mMutationObserver;

  /**
   * The number of characters in the DOM after the final nsTextFrame.  For
   * example, with
   *
   *   <text>abcd<tspan display="none">ef</tspan></text>
   *
   * mTrailingUndisplayedCharacters would be 2.
   */
  uint32_t mTrailingUndisplayedCharacters;

  /**
   * Computed position information for each DOM character within the <text>.
   */
  nsTArray<CharPosition> mPositions;

  /**
   * mFontSizeScaleFactor is used to cause the nsTextFrames to create text
   * runs with a font size different from the actual font-size property value.
   * This is used so that, for example with:
   *
   *   <svg>
   *     <g transform="scale(2)">
   *       <text font-size="10">abc</text>
   *     </g>
   *   </svg>
   *
   * a font size of 20 would be used.  It's preferable to use a font size that
   * is identical or close to the size that the text will appear on the screen,
   * because at very small or large font sizes, text metrics will be computed
   * differently due to the limited precision that text runs have.
   *
   * mFontSizeScaleFactor is the amount the actual font-size property value
   * should be multiplied by to cause the text run font size to (a) be within a
   * "reasonable" range, and (b) be close to the actual size to be painted on
   * screen.  (The "reasonable" range as determined by some #defines in
   * SVGTextFrame.cpp is 8..200.)
   */
  float mFontSizeScaleFactor;

  /**
   * The scale of the context that we last used to compute mFontSizeScaleFactor.
   * We record this so that we can tell when our scale transform has changed
   * enough to warrant reflowing the text.
   */
  float mLastContextScale;

  /**
   * The amount that we need to scale each rendered run to account for
   * lengthAdjust="spacingAndGlyphs".
   */
  float mLengthAdjustScaleFactor;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGTEXTFRAME_H_
