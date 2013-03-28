/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGTEXTFRAME2_H
#define NS_SVGTEXTFRAME2_H

#include "gfxFont.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "gfxSVGGlyphs.h"
#include "nsStubMutationObserver.h"
#include "nsSVGGlyphFrame.h" // for SVGTextObjectPaint
#include "nsSVGTextContainerFrame.h"

class nsDisplaySVGText;
class nsRenderingContext;
class nsSVGTextFrame2;
class nsTextFrame;

typedef nsSVGDisplayContainerFrame nsSVGTextFrame2Base;

namespace mozilla {

class TextFrameIterator;
class TextNodeCorrespondenceRecorder;
struct TextRenderedRun;
class TextRenderedRunIterator;

namespace dom {
class SVGIRect;
}

/**
 * Information about the positioning for a single character in an SVG <text>
 * element.
 *
 * During SVG text layout, we use infinity values to represent positions and
 * rotations that are not explicitly specified with x/y/rotate attributes.
 */
struct CharPosition
{
  CharPosition()
    : mAngle(0),
      mHidden(false),
      mUnaddressable(false),
      mClusterOrLigatureGroupMiddle(false),
      mRunBoundary(false),
      mStartOfChunk(false)
  {
  }

  CharPosition(gfxPoint aPosition, double aAngle)
    : mPosition(aPosition),
      mAngle(aAngle),
      mHidden(false),
      mUnaddressable(false),
      mClusterOrLigatureGroupMiddle(false),
      mRunBoundary(false),
      mStartOfChunk(false)
  {
  }

  static CharPosition Unspecified(bool aUnaddressable)
  {
    CharPosition cp(UnspecifiedPoint(), UnspecifiedAngle());
    cp.mUnaddressable = aUnaddressable;
    return cp;
  }

  bool IsAngleSpecified() const
  {
    return mAngle != UnspecifiedAngle();
  }

  bool IsXSpecified() const
  {
    return mPosition.x != UnspecifiedCoord();
  }

  bool IsYSpecified() const
  {
    return mPosition.y != UnspecifiedCoord();
  }

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
  static gfxFloat UnspecifiedCoord()
  {
    return std::numeric_limits<gfxFloat>::infinity();
  }

  static double UnspecifiedAngle()
  {
    return std::numeric_limits<double>::infinity();
  }

  static gfxPoint UnspecifiedPoint()
  {
    return gfxPoint(UnspecifiedCoord(), UnspecifiedCoord());
  }
};

/**
 * A runnable to mark glyph positions as needing to be recomputed
 * and to invalid the bounds of the nsSVGTextFrame2 frame.
 */
class GlyphMetricsUpdater : public nsRunnable {
public:
  NS_DECL_NSIRUNNABLE
  GlyphMetricsUpdater(nsSVGTextFrame2* aFrame) : mFrame(aFrame) { }
  static void Run(nsSVGTextFrame2* aFrame);
  void Revoke() { mFrame = nullptr; }
private:
  nsSVGTextFrame2* mFrame;
};

}

/**
 * Frame class for SVG <text> elements, used when the
 * layout.svg.css-text.enabled is true.
 *
 * An nsSVGTextFrame2 manages SVG text layout, painting and interaction for
 * all descendent text content elements.  The frame tree will look like this:
 *
 *   nsSVGTextFrame2                  -- for <text>
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
class nsSVGTextFrame2 : public nsSVGTextFrame2Base
{
  friend nsIFrame*
  NS_NewSVGTextFrame2(nsIPresShell* aPresShell, nsStyleContext* aContext);

  friend class mozilla::GlyphMetricsUpdater;
  friend class mozilla::TextFrameIterator;
  friend class mozilla::TextNodeCorrespondenceRecorder;
  friend struct mozilla::TextRenderedRun;
  friend class mozilla::TextRenderedRunIterator;
  friend class AutoCanvasTMForMarker;
  friend class MutationObserver;
  friend class nsDisplaySVGText;

protected:
  nsSVGTextFrame2(nsStyleContext* aContext)
    : nsSVGTextFrame2Base(aContext),
      mFontSizeScaleFactor(1.0f),
      mGetCanvasTMForFlag(FOR_OUTERSVG_TM),
      mPositioningDirty(true)
  {
  }

public:
  NS_DECL_QUERYFRAME_TARGET(nsSVGTextFrame2)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
  virtual void Init(nsIContent* aContent,
                    nsIFrame*   aParent,
                    nsIFrame*   aPrevInFlow) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  NS_IMETHOD AttributeChanged(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              int32_t aModType);

  virtual nsIFrame* GetContentInsertionFrame()
  {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgTextFrame2
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGText2"), aResult);
  }
#endif

  /**
   * Finds the nsTextFrame for the closest rendered run to the specified point.
   */
  virtual void FindCloserFrameForSelection(nsPoint aPoint,
                                          FrameWithDistance* aCurrentBestFrame);


  // nsISVGChildFrame interface:
  virtual void NotifySVGChanged(uint32_t aFlags);
  NS_IMETHOD PaintSVG(nsRenderingContext* aContext,
                      const nsIntRect* aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint& aPoint);
  virtual void ReflowSVG();
  NS_IMETHOD_(nsRect) GetCoveredRegion();
  virtual SVGBBox GetBBoxContribution(const gfxMatrix& aToBBoxUserspace,
                                      uint32_t aFlags);

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM(uint32_t aFor);
  
  // SVG DOM text methods:
  uint32_t GetNumberOfChars(nsIContent* aContent);
  float GetComputedTextLength(nsIContent* aContent);
  nsresult GetSubStringLength(nsIContent* aContent, uint32_t charnum,
                              uint32_t nchars, float* aResult);
  int32_t GetCharNumAtPosition(nsIContent* aContent, mozilla::nsISVGPoint* point);

  nsresult GetStartPositionOfChar(nsIContent* aContent, uint32_t aCharNum,
                                  mozilla::nsISVGPoint** aResult);
  nsresult GetEndPositionOfChar(nsIContent* aContent, uint32_t aCharNum,
                                mozilla::nsISVGPoint** aResult);
  nsresult GetExtentOfChar(nsIContent* aContent, uint32_t aCharNum,
                           mozilla::dom::SVGIRect** aResult);
  nsresult GetRotationOfChar(nsIContent* aContent, uint32_t aCharNum,
                             float* aResult);

  // nsSVGTextFrame2 methods:

  /**
   * Schedules mPositions to be recomputed and the covered region to be
   * updated.  The aFlags argument can take the ePositioningDirtyDueToMutation
   * value to indicate that glyph metrics need to be recomputed due to
   * a DOM mutation in the <text> element on one of its descendants.
   */
  void NotifyGlyphMetricsChange(uint32_t aFlags = 0);

  /**
   * Enum for NotifyGlyphMetricsChange's aFlags argument.
   */
  enum { ePositioningDirtyDueToMutation = 1 };

  /**
   * Updates the mFontSizeScaleFactor value by looking at the range of
   * font-sizes used within the <text>.
   */
  void UpdateFontSizeScaleFactor(bool aForceGlobalTransform);

  double GetFontSizeScaleFactor() const;

  /**
   * Takes a point from the <text> element's user space and
   * converts it to the appropriate frame user space of aChildFrame,
   * according to which rendered run the point hits.
   */
  gfxPoint TransformFramePointToTextChild(const gfxPoint& aPoint,
                                          nsIFrame* aChildFrame);

  /**
   * Takes a rectangle, aRect, in the <text> element's user space, and
   * returns a rectangle in aChildFrame's frame user space that
   * covers intersections of aRect with each rendered run for text frames
   * within aChildFrame.
   */
  gfxRect TransformFrameRectToTextChild(const gfxRect& aRect,
                                        nsIFrame* aChildFrame);

  /**
   * Takes an app unit rectangle in the coordinate space of a given descendant
   * frame of this frame, and returns a rectangle in the <text> element's user
   * space that covers all parts of rendered runs that intersect with the
   * rectangle.
   */
  gfxRect TransformFrameRectFromTextChild(const nsRect& aRect,
                                          nsIFrame* aChildFrame);

private:
  /**
   * This class exists purely because it would be too messy to pass the "for"
   * flag for GetCanvasTM through the call chains to the GetCanvasTM() call in
   * UpdateFontSizeScaleFactor.
   */
  class AutoCanvasTMForMarker {
  public:
    AutoCanvasTMForMarker(nsSVGTextFrame2* aFrame, uint32_t aFor)
      : mFrame(aFrame)
    {
      mOldFor = mFrame->mGetCanvasTMForFlag;
      mFrame->mGetCanvasTMForFlag = aFor;
    }
    ~AutoCanvasTMForMarker()
    {
      // Default
      mFrame->mGetCanvasTMForFlag = mOldFor;
    }
  private:
    nsSVGTextFrame2* mFrame;
    uint32_t mOldFor;
  };

  /**
   * Mutation observer used to watch for text positioning attribute changes
   * on descendent text content elements (like <tspan>s).
   */
  class MutationObserver : public nsStubMutationObserver {
  public:
    MutationObserver()
      : mFrame(nullptr)
    {
    }

    void StartObserving(nsSVGTextFrame2* aFrame)
    {
      NS_ASSERTION(!mFrame, "should not be observing yet!");
      mFrame = aFrame;
      aFrame->GetContent()->AddMutationObserver(this);
    }

    virtual ~MutationObserver()
    {
      if (mFrame) {
        mFrame->GetContent()->RemoveMutationObserver(this);
      }
    }

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIMutationObserver
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  private:
    nsSVGTextFrame2* mFrame;
  };

  /**
   * Reflows the anonymous block child.
   */
  void DoReflow(bool aForceGlobalTransform);

  /**
   * Calls FrameNeedsReflow on the anonymous block child.
   */
  void RequestReflow(nsIPresShell::IntrinsicDirty aType, uint32_t aBit);

  /**
   * Reflows the anonymous block child and recomputes mPositions if needed.
   *
   * @param aForceGlobalTransform passed down to UpdateFontSizeScaleFactor to
   * control whether it should use the global transform even when
   * NS_STATE_NONDISPLAY_CHILD
   */
  void UpdateGlyphPositioning(bool aForceGlobalTransform);

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
  int32_t
  ConvertTextElementCharIndexToAddressableIndex(int32_t aIndex,
                                                nsIContent* aContent);

  /**
   * Recursive helper for ResolvePositions below.
   *
   * @param aContent The current node.
   * @param aIndex The current character index.
   * @param aInTextPath Whether we are currently under a <textPath> element.
   * @param aForceStartOfChunk Whether the next character we find should start a
   *   new anchored chunk.
   * @return The character index we got up to.
   */
  uint32_t ResolvePositions(nsIContent* aContent, uint32_t aIndex,
                            bool aInTextPath, bool& aForceStartOfChunk,
                            nsTArray<gfxPoint>& aDeltas);

  /**
   * Initializes mPositions with character position information based on
   * x/y/rotate attributes, leaving unspecified values in the array if a position
   * was not given for that character.  Also fills aDeltas with values based on
   * dx/dy attributes.
   *
   * @return True if we recorded any positions.
   */
  bool ResolvePositions(nsTArray<gfxPoint>& aDeltas);

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
  bool ShouldRenderAsPath(nsRenderingContext* aContext, nsTextFrame* aFrame,
                          bool& aShouldPaintSVGGlyphs);

  // Methods to get information for a <textPath> frame.
  nsIFrame* GetTextPathPathFrame(nsIFrame* aTextPathFrame);
  already_AddRefed<gfxFlattenedPath> GetFlattenedTextPath(nsIFrame* aTextPathFrame);
  gfxFloat GetOffsetScale(nsIFrame* aTextPathFrame);
  gfxFloat GetStartOffset(nsIFrame* aTextPathFrame);

  gfxFont::DrawMode SetupCairoState(gfxContext* aContext,
                                    nsIFrame* aFrame,
                                    gfxTextObjectPaint* aOuterObjectPaint,
                                    gfxTextObjectPaint** aThisObjectPaint);

  /**
   * Sets up the stroke style for |aFrame| in |aContext| and stores stroke
   * pattern information in |aThisObjectPaint|.
   */
  bool SetupCairoStroke(gfxContext* aContext,
                        nsIFrame* aFrame,
                        gfxTextObjectPaint* aOuterObjectPaint,
                        SVGTextObjectPaint* aThisObjectPaint);

  /**
   * Sets up the fill style for |aFrame| in |aContext| and stores fill pattern
   * information in |aThisObjectPaint|.
   */
  bool SetupCairoFill(gfxContext* aContext,
                      nsIFrame* aFrame,
                      gfxTextObjectPaint* aOuterObjectPaint,
                      SVGTextObjectPaint* aThisObjectPaint);

  /**
   * Sets the current pattern for |aFrame| to the fill or stroke style of the
   * outer text object. Will also set the paint opacity to transparent if the
   * paint is set to "none".
   */
  bool SetupObjectPaint(gfxContext* aContext,
                        nsIFrame* aFrame,
                        nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                        float& aOpacity,
                        gfxTextObjectPaint* aObjectPaint);

  /**
   * Stores in |aTargetPaint| information on how to reconstruct the current
   * fill or stroke pattern. Will also set the paint opacity to transparent if
   * the paint is set to "none".
   * @param aOuterObjectPaint pattern information from the outer text object
   * @param aTargetPaint where to store the current pattern information
   * @param aFillOrStroke member pointer to the paint we are setting up
   * @param aProperty the frame property descriptor of the fill or stroke paint
   *   server frame
   */
  void SetupInheritablePaint(gfxContext* aContext,
                             nsIFrame* aFrame,
                             float& aOpacity,
                             gfxTextObjectPaint* aOuterObjectPaint,
                             SVGTextObjectPaint::Paint& aTargetPaint,
                             nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                             const FramePropertyDescriptor* aProperty);

  /**
   * The MutationObserver we have registered for the <text> element subtree.
   */
  MutationObserver mMutationObserver;

  /**
   * The runnable we have dispatched to perform the work of
   * NotifyGlyphMetricsChange.
   */
  nsRefPtr<GlyphMetricsUpdater> mGlyphMetricsUpdater;

  /**
   * Cached canvasTM value.
   */
  nsAutoPtr<gfxMatrix> mCanvasTM;

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
  nsTArray<mozilla::CharPosition> mPositions;

  /**
   * mFontSizeScaleFactor is used to cause the nsTextFrames to create text
   * runs with a font size different from the actual font-size property value.
   * This is used so that, for example with:
   *
   *   <svg>
   *     <g transform="scale(2)">
   *       <text font-size="10">abc</text>
   *     </g>
   *  </svg>
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
   * nsSVGTextFrame2.cpp is 8..200.)
   */
  float mFontSizeScaleFactor;

  /**
   * The flag to pass to GetCanvasTM from UpdateFontSizeScaleFactor.  This is
   * normally FOR_OUTERSVG_TM, but while painting or hit testing a pattern or
   * marker, we set it to FOR_PAINTING or FOR_HIT_TESTING appropriately.
   */
  uint32_t mGetCanvasTMForFlag;

  /**
   * Whether something has changed to invalidate the values in mPositions.
   */
  bool mPositioningDirty;
};

#endif
