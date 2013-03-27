/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGGlyphFrame.h"
#include <algorithm>

// Keep others in (case-insensitive) order:
#include "DOMSVGPoint.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxPlatform.h"
#include "mozilla/LookAndFeel.h"
#include "nsBidiPresUtils.h"
#include "nsDisplayList.h"
#include "nsError.h"
#include "nsRenderingContext.h"
#include "nsSVGEffects.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGPaintServerFrame.h"
#include "mozilla/dom/SVGRect.h"
#include "nsSVGTextPathFrame.h"
#include "nsSVGUtils.h"
#include "nsTextFragment.h"
#include "SVGContentUtils.h"
#include "SVGLengthList.h"

using namespace mozilla;

struct CharacterPosition {
  gfxPoint pos;
  gfxFloat angle;
  bool draw;
};

static gfxContext* MakeTmpCtx() {
  return new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());
}

/**
 * This is a do-it-all helper class. It supports iterating through the
 * drawable character clusters of a string. For each cluster, it can set up
 * a graphics context with a transform appropriate for drawing the
 * character, or a transform appropriate for emitting geometry in the
 * text metrics coordinate system (which differs from the drawing
 * coordinate system by a scale factor of AppUnitPerCSSPixels). These
 * transforms include offsets and rotations of characters along paths, and
 * the mPosition of the nsSVGGlyphFrame.
 * 
 * This helper also creates the textrun as needed. It supports detecting
 * the special case when the entire textrun can be drawn or measured
 * as a unit, and setting the graphics context transform up for that. It
 * takes care of setting up the global transform if requested. It also
 * provides direct access to the character path position data for the
 * DOM APIs that need that.
 * 
 * If an error occurs, for example, a canvas TM is not available because
 * the element is in a <defs> section, then the CharacterIterator will
 * behave as if the frame has no drawable characters.
 *
 * XXX needs RTL love
 * XXX might want to make AdvanceToCharacter constant time (e.g. by
 * caching advances and/or the CharacterPosition array across DOM
 * API calls) to ensure that calling Get*OfChar (etc) for each character
 * in the text is O(N)
 */
class CharacterIterator
{
public:
  /**
   * Sets up the iterator so that NextCluster will return the first drawable
   * cluster.
   * @param aForceGlobalTransform passed on to EnsureTextRun (see below)
   */
  CharacterIterator(nsSVGGlyphFrame *aSource, bool aForceGlobalTransform);
  /**
   * This matrix will be applied to aContext in the SetupFor methods below,
   * before any glyph translation/rotation.
   */
  bool SetInitialMatrix(gfxContext *aContext) {
    mInitialMatrix = aContext->CurrentMatrix();
    if (mInitialMatrix.IsSingular()) {
      mInError = true;
    }
    return !mInError;
  }
  /**
   * Try to set up aContext so we can draw the whole textrun at once.
   * This applies any global transform requested by SetInitialMatrix,
   * then applies the positioning of the text. Returns false if drawing
   * the whole textrun at once is impossible due to individual positioning
   * and/or rotation of glyphs.
   */
  bool SetupForDirectTextRunDrawing(gfxContext *aContext) {
    return SetupForDirectTextRun(aContext, mDrawScale);
  }
  /**
   * Try to set up aContext so we can measure the whole textrun at once.
   * This applies any global transform requested by SetInitialMatrix,
   * then applies the positioning of the text, then applies a scale
   * from appunits to device pixels so drawing in appunits works.
   * Returns false if drawing the whole textrun at once is impossible due
   * to individual positioning and/or rotation of glyphs.
   */
  bool SetupForDirectTextRunMetrics(gfxContext *aContext) {
    return SetupForDirectTextRun(aContext, mMetricsScale);
  }
  /**
   * We are scaling the glyphs up/down to the size we want so we need to
   * inverse scale the outline widths of those glyphs so they are invariant
   */
  void SetLineWidthAndDashesForDrawing(gfxContext *aContext) {
    aContext->SetLineWidth(aContext->CurrentLineWidth() / mDrawScale);
    AutoFallibleTArray<gfxFloat, 10> dashes;
    gfxFloat dashOffset;
    if (aContext->CurrentDash(dashes, &dashOffset)) {
      for (uint32_t i = 0; i <  dashes.Length(); i++) {
        dashes[i] /= mDrawScale;
      }
      aContext->SetDash(dashes.Elements(), dashes.Length(), dashOffset / mDrawScale);
    }
  }

  /**
   * Returns the index of the next cluster in the string that should be drawn,
   * or InvalidCluster() (i.e. uint32_t(-1)) if there is no such cluster.
   */
  uint32_t NextCluster();

  /**
   * Returns the length of the current cluster (usually 1, unless there
   * are combining marks)
   */
  uint32_t ClusterLength();

  /**
   * Repeated calls NextCluster until it returns aIndex (i.e. aIndex is the
   * current drawable character). Returns false if that never happens
   * (because aIndex is before or equal to the current character, or
   * out of bounds, or not drawable).
   */
  bool AdvanceToCharacter(uint32_t aIndex);

  /**
   * Resets the iterator to the beginning of the string.
   */
  void Reset() {
    // There are two ways mInError can be set
    // a) If there was a problem creating the iterator (mCurrentChar == -1)
    // b) If we ran off the end of the string (mCurrentChar != -1)
    // We can only reset the mInError flag in case b)
    if (mCurrentChar != InvalidCluster()) {
      mCurrentChar = InvalidCluster();
      mInError = false;
    }
  }

  /**
   * Set up aContext for glyph drawing. This applies any global transform
   * requested by SetInitialMatrix, then applies any positioning and
   * rotation for the current character.
   */
  void SetupForDrawing(gfxContext *aContext) {
    return SetupFor(aContext, mDrawScale);
  }
  /**
   * Set up aContext for glyph measuring. This applies any global transform
   * requested by SetInitialMatrix, then applies any positioning and
   * rotation for the current character, then applies a scale from appunits
   * to device pixels so that drawing in appunits sizes works.
   */
  void SetupForMetrics(gfxContext *aContext) {
    return SetupFor(aContext, mMetricsScale);
  }
  /**
   * Get the raw position data for the current character.
   */
  CharacterPosition GetPositionData();

  /**
   * "Invalid" cluster index returned to indicate error state
   */
  uint32_t InvalidCluster() {
    return uint32_t(-1);
  }

private:
  bool SetupForDirectTextRun(gfxContext *aContext, float aScale);
  void SetupFor(gfxContext *aContext, float aScale);

  nsSVGGlyphFrame *mSource;
  nsAutoTArray<CharacterPosition,80> mPositions;
  gfxMatrix mInitialMatrix;
  // Textrun advance width from start to mCurrentChar, in appunits
  gfxFloat mCurrentAdvance;
  uint32_t mCurrentChar;
  float mDrawScale;
  float mMetricsScale;
  bool mInError;
};


class nsDisplaySVGGlyphs : public nsDisplayItem {
public:
  nsDisplaySVGGlyphs(nsDisplayListBuilder* aBuilder,
                     nsSVGGlyphFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplaySVGGlyphs);
    NS_ABORT_IF_FALSE(aFrame, "Must have a frame!");
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySVGGlyphs() {
    MOZ_COUNT_DTOR(nsDisplaySVGGlyphs);
  }
#endif

  NS_DISPLAY_DECL_NAME("nsDisplaySVGGlyphs", TYPE_SVG_GLYPHS)

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
};

void
nsDisplaySVGGlyphs::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsSVGGlyphFrame *frame = static_cast<nsSVGGlyphFrame*>(mFrame);
  nsPoint pointRelativeToReferenceFrame = aRect.Center();
  // ToReferenceFrame() includes frame->GetPosition(), our user space position.
  nsPoint userSpacePt = pointRelativeToReferenceFrame -
                          (ToReferenceFrame() - frame->GetPosition());
  if (frame->GetFrameForPoint(userSpacePt)) {
    aOutFrames->AppendElement(frame);
  }
}

void
nsDisplaySVGGlyphs::Paint(nsDisplayListBuilder* aBuilder,
                          nsRenderingContext* aCtx)
{
  // ToReferenceFrame includes our mRect offset, but painting takes
  // account of that too. To avoid double counting, we subtract that
  // here.
  nsPoint offset = ToReferenceFrame() - mFrame->GetPosition();

  aCtx->PushState();
  aCtx->Translate(offset);
  static_cast<nsSVGGlyphFrame*>(mFrame)->PaintSVG(aCtx, nullptr);
  aCtx->PopState();
}

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGGlyphFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGGlyphFrame)

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(nsSVGGlyphFrame)
  NS_QUERYFRAME_ENTRY(nsISVGGlyphFragmentNode)
  NS_QUERYFRAME_ENTRY(nsISVGChildFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGGlyphFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::CharacterDataChanged(CharacterDataChangeInfo* aInfo)
{
  // NotifyGlyphMetricsChange takes care of calling
  // nsSVGUtils::InvalidateAndScheduleBoundsUpdate on the appropriate frames.

  NotifyGlyphMetricsChange();

  ClearTextRun();
  if (IsTextEmpty()) {
    // The one time that NotifyGlyphMetricsChange fails to call
    // nsSVGUtils::InvalidateAndScheduleBoundsUpdate properly is when all our
    // text is gone, since it skips empty frames. So we have to invalidate
    // ourself.
    nsSVGUtils::InvalidateBounds(this);
  }

  return NS_OK;
}

// Usable font size range in devpixels / user-units
#define CLAMP_MIN_SIZE 8
#define CLAMP_MAX_SIZE 200
#define PRECISE_SIZE   200

/* virtual */ void
nsSVGGlyphFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsSVGGlyphFrameBase::DidSetStyleContext(aOldStyleContext);

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    ClearTextRun();
    NotifyGlyphMetricsChange();
  }
}

void
nsSVGGlyphFrame::Init(nsIContent* aContent,
                      nsIFrame* aParent,
                      nsIFrame* aPrevInFlow)
{
#ifdef DEBUG
  NS_ASSERTION(aParent, "null parent");

  nsIFrame* ancestorFrame = nsSVGUtils::GetFirstNonAAncestorFrame(aParent);
  NS_ASSERTION(ancestorFrame, "Must have ancestor");

  nsSVGTextContainerFrame *metrics = do_QueryFrame(ancestorFrame);
  NS_ASSERTION(metrics,
               "trying to construct an SVGGlyphFrame for an invalid container");

  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "trying to construct an SVGGlyphFrame for wrong content element");
#endif /* DEBUG */

  nsSVGGlyphFrameBase::Init(aContent, aParent, aPrevInFlow);
}

nsIAtom *
nsSVGGlyphFrame::GetType() const
{
  return nsGkAtoms::svgGlyphFrame;
}

void
nsSVGGlyphFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  if (StyleFont()->mFont.size <= 0) {
    return;
  }
  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplaySVGGlyphs(aBuilder, this));
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::PaintSVG(nsRenderingContext *aContext,
                          const nsIntRect *aDirtyRect)
{
  if (!StyleVisibility()->IsVisible())
    return NS_OK;

  if (StyleFont()->mFont.size <= 0) {
    // Don't even try to paint, or cairo will go into an error state.
    return NS_OK;
  }

  AutoCanvasTMForMarker autoCanvasTMFor(this, FOR_PAINTING);

  gfxContext *gfx = aContext->ThebesContext();
  uint16_t renderMode = SVGAutoRenderState::GetRenderMode(aContext);

  switch (StyleSVG()->mTextRendering) {
  case NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED:
    gfx->SetAntialiasMode(gfxContext::MODE_ALIASED);
    break;
  default:
    gfx->SetAntialiasMode(gfxContext::MODE_COVERAGE);
    break;
  }

  if (renderMode != SVGAutoRenderState::NORMAL) {
    NS_ABORT_IF_FALSE(renderMode == SVGAutoRenderState::CLIP ||
                      renderMode == SVGAutoRenderState::CLIP_MASK,
                      "Unknown render mode");
    gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(gfx);
    SetupGlobalTransform(gfx, FOR_PAINTING);

    CharacterIterator iter(this, true);
    if (!iter.SetInitialMatrix(gfx)) {
      return NS_OK;
    }

    if (GetClipRule() == NS_STYLE_FILL_RULE_EVENODD)
      gfx->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
    else
      gfx->SetFillRule(gfxContext::FILL_RULE_WINDING);

    if (renderMode == SVGAutoRenderState::CLIP_MASK) {
      gfx->SetColor(gfxRGBA(1.0f, 1.0f, 1.0f, 1.0f));
      DrawCharacters(&iter, gfx, gfxFont::GLYPH_FILL);
    } else {
      DrawCharacters(&iter, gfx, gfxFont::GLYPH_PATH);
    }

    return NS_OK;
  }

  // We are adding patterns or gradients to the context. Save
  // it so we don't leak them into the next object we draw
  gfx->Save();
  SetupGlobalTransform(gfx, FOR_PAINTING);

  CharacterIterator iter(this, true);
  if (!iter.SetInitialMatrix(gfx)) {
    gfx->Restore();
    return NS_OK;
  }

  gfxTextObjectPaint *outerObjectPaint =
    (gfxTextObjectPaint*)aContext->GetUserData(&gfxTextObjectPaint::sUserDataKey);

  nsAutoPtr<gfxTextObjectPaint> objectPaint;
  DrawMode drawMode = SetupCairoState(gfx, outerObjectPaint, getter_Transfers(objectPaint));

  if (drawMode) {
    DrawCharacters(&iter, gfx, drawMode, objectPaint);
  }
  
  gfx->Restore();

  return NS_OK;
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGGlyphFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  uint16_t hitTestFlags = GetHitTestFlags();
  if (!hitTestFlags) {
    return nullptr;
  }

  AutoCanvasTMForMarker autoCanvasTMFor(this, FOR_HIT_TESTING);

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  SetupGlobalTransform(tmpCtx, FOR_HIT_TESTING);
  CharacterIterator iter(this, true);
  if (!iter.SetInitialMatrix(tmpCtx)) {
    return nullptr;
  }

  // The SVG 1.1 spec says that text is hit tested against the character cells
  // of the text, not the fill and stroke. See the section starting "For text
  // elements..." here:
  //
  //   http://www.w3.org/TR/SVG11/interact.html#PointerEventsProperty
  //
  // Currently we just test the character cells if GetHitTestFlags says we're
  // supposed to be testing either the fill OR the stroke:

  uint32_t i;
  while ((i = iter.NextCluster()) != iter.InvalidCluster()) {
    gfxTextRun::Metrics metrics =
    mTextRun->MeasureText(i, iter.ClusterLength(),
                          gfxFont::LOOSE_INK_EXTENTS, nullptr, nullptr);
    iter.SetupForMetrics(tmpCtx);
    tmpCtx->Rectangle(metrics.mBoundingBox);
  }

  gfxPoint userSpacePoint =
    tmpCtx->DeviceToUser(gfxPoint(PresContext()->AppUnitsToGfxUnits(aPoint.x),
                                  PresContext()->AppUnitsToGfxUnits(aPoint.y)));

  bool isHit = false;
  if (hitTestFlags & SVG_HIT_TEST_FILL || hitTestFlags & SVG_HIT_TEST_STROKE) {
    isHit = tmpCtx->PointInFill(userSpacePoint);
  }

  // If isHit is false, we may also want to fill and stroke the text to check
  // whether the pointer is over an area of fill or stroke that lies outside
  // the character cells. (With a thick stroke, or with fonts like Zapfino, such
  // areas may be very significant.) This is what Opera appears to do, but
  // currently we do not.

  if (isHit && nsSVGUtils::HitTestClip(this, aPoint))
    return this;

  return nullptr;
}

NS_IMETHODIMP_(nsRect)
nsSVGGlyphFrame::GetCoveredRegion()
{
  return nsSVGUtils::TransformFrameRectToOuterSVG(
                       mRect, GetCanvasTM(FOR_OUTERSVG_TM), PresContext());
}

void
nsSVGGlyphFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                    "ReflowSVG mechanism not designed for this");

  mRect.SetEmpty();

  uint32_t flags = nsSVGUtils::eBBoxIncludeFill |
                   nsSVGUtils::eBBoxIncludeStroke |
                   nsSVGUtils::eBBoxIncludeMarkers;
  // Our "visual" overflow rect needs to be valid for building display lists
  // for hit testing, which means that for certain values of 'pointer-events'
  // it needs to include the geometry of the fill or stroke even when the fill/
  // stroke don't actually render (e.g. when stroke="none" or
  // stroke-opacity="0"). GetHitTestFlags() accounts for 'pointer-events'.
  uint16_t hitTestFlags = GetHitTestFlags();
  if ((hitTestFlags & SVG_HIT_TEST_FILL)) {
   flags |= nsSVGUtils::eBBoxIncludeFillGeometry;
  }
  if ((hitTestFlags & SVG_HIT_TEST_STROKE)) {
   flags |= nsSVGUtils::eBBoxIncludeStrokeGeometry;
  }
  gfxRect extent = GetBBoxContribution(gfxMatrix(), flags);

  if (!extent.IsEmpty()) {
    mRect = nsLayoutUtils::RoundGfxRectToAppRect(extent, 
              PresContext()->AppUnitsPerCSSPixel());
  }

  nsRect overflow = nsRect(nsPoint(0,0), mRect.Size());
  nsOverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);

  // Invalidate, but only if this is not our first reflow (since if it is our
  // first reflow then we haven't had our first paint yet).
  if (!(GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    InvalidateFrame();
  }
}

void
nsSVGGlyphFrame::NotifySVGChanged(uint32_t aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  // Ancestor changes can't affect how we render from the perspective of
  // any rendering observers that we may have, so we don't need to
  // invalidate them. We also don't need to invalidate ourself, since our
  // changed ancestor will have invalidated its entire area, which includes
  // our area.
  // XXXjwatt: seems to me that our ancestor's change could change our glyph
  // metrics, in which case we should call NotifyGlyphMetricsChange instead.
  nsSVGUtils::ScheduleReflowSVG(this);

  if (aFlags & TRANSFORM_CHANGED) {
    ClearTextRun();
  }
}

void
nsSVGGlyphFrame::AddBoundingBoxesToPath(CharacterIterator *aIter,
                                        gfxContext *aContext)
{
  if (aIter->SetupForDirectTextRunMetrics(aContext)) {
    gfxTextRun::Metrics metrics =
      mTextRun->MeasureText(0, mTextRun->GetLength(),
                            gfxFont::LOOSE_INK_EXTENTS, nullptr, nullptr);
    aContext->Rectangle(metrics.mBoundingBox);
    return;
  }

  uint32_t i;
  while ((i = aIter->NextCluster()) != aIter->InvalidCluster()) {
    aIter->SetupForMetrics(aContext);
    gfxTextRun::Metrics metrics =
      mTextRun->MeasureText(i, aIter->ClusterLength(),
                            gfxFont::LOOSE_INK_EXTENTS, nullptr, nullptr);
    aContext->Rectangle(metrics.mBoundingBox);
  }
}

void
nsSVGGlyphFrame::DrawCharacters(CharacterIterator *aIter,
                                gfxContext *aContext,
                                DrawMode aDrawMode,
                                gfxTextObjectPaint *aObjectPaint)
{
  if (aDrawMode & gfxFont::GLYPH_STROKE) {
    aIter->SetLineWidthAndDashesForDrawing(aContext);
  }

  if (aIter->SetupForDirectTextRunDrawing(aContext)) {
    mTextRun->Draw(aContext, gfxPoint(0, 0), aDrawMode, 0,
                   mTextRun->GetLength(), nullptr, nullptr, aObjectPaint);
    return;
  }

  uint32_t i;
  while ((i = aIter->NextCluster()) != aIter->InvalidCluster()) {
    aIter->SetupForDrawing(aContext);
    mTextRun->Draw(aContext, gfxPoint(0, 0), aDrawMode, i,
                   aIter->ClusterLength(), nullptr, nullptr, aObjectPaint);
  }
}

SVGBBox
nsSVGGlyphFrame::GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                     uint32_t aFlags)
{
  SVGBBox bbox;

  if (mOverrideCanvasTM) {
    *mOverrideCanvasTM = aToBBoxUserspace;
  } else {
    mOverrideCanvasTM = new gfxMatrix(aToBBoxUserspace);
  }

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  SetupGlobalTransform(tmpCtx, FOR_OUTERSVG_TM);
  CharacterIterator iter(this, true);
  if (!iter.SetInitialMatrix(tmpCtx)) {
    return bbox;
  }
  AddBoundingBoxesToPath(&iter, tmpCtx);
  tmpCtx->IdentityMatrix();

  mOverrideCanvasTM = nullptr;

  // Be careful when replacing the following logic to get the fill and stroke
  // extents independently (instead of computing the stroke extents from the
  // path extents). You may think that you can just use the stroke extents if
  // there is both a fill and a stroke. In reality it's necessary to calculate
  // both the fill and stroke extents, and take the union of the two. There are
  // two reasons for this:
  //
  // # Due to stroke dashing, in certain cases the fill extents could actually
  //   extend outside the stroke extents.
  // # If the stroke is very thin, cairo won't paint any stroke, and so the
  //   stroke bounds that it will return will be empty.
  //
  // Another thing to be aware of is that under AddBoundingBoxesToPath the
  // gfxContext has SetLineWidth() called on it, so if we want to ask the
  // gfxContext for *stroke* extents, we'll need to wrap the
  // AddBoundingBoxesToPath() call with CurrentLineWidth()/SetLineWidth()
  // calls to record and then reset the stroke width.

  gfxRect pathExtents = tmpCtx->GetUserPathExtent();

  // Account for fill:
  if ((aFlags & nsSVGUtils::eBBoxIncludeFillGeometry) ||
      ((aFlags & nsSVGUtils::eBBoxIncludeFill) &&
       StyleSVG()->mFill.mType != eStyleSVGPaintType_None)) {
    bbox = pathExtents;
  }

  // Account for stroke:
  if ((aFlags & nsSVGUtils::eBBoxIncludeStrokeGeometry) ||
      ((aFlags & nsSVGUtils::eBBoxIncludeStroke) &&
       nsSVGUtils::HasStroke(this))) {
    bbox.UnionEdges(nsSVGUtils::PathExtentsToMaxStrokeExtents(pathExtents,
                                                              this,
                                                              aToBBoxUserspace));
  }

  return bbox;
}

//----------------------------------------------------------------------
// nsSVGGeometryFrame methods:

gfxMatrix
nsSVGGlyphFrame::GetCanvasTM(uint32_t aFor)
{
  if (mOverrideCanvasTM) {
    return *mOverrideCanvasTM;
  }
  if (!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
    if ((aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
  }
  NS_ASSERTION(mParent, "null parent");
  return static_cast<nsSVGContainerFrame*>(mParent)->GetCanvasTM(aFor);
}

//----------------------------------------------------------------------
// nsSVGGlyphFrame methods:

bool
nsSVGGlyphFrame::GetCharacterData(nsAString & aCharacterData)
{
  nsAutoString characterData;
  mContent->AppendTextTo(characterData);

  if (mCompressWhitespace) {
    characterData.CompressWhitespace(mTrimLeadingWhitespace,
                                     mTrimTrailingWhitespace);
  } else {
    nsAString::iterator start, end;
    characterData.BeginWriting(start);
    characterData.EndWriting(end);
    while (start != end) {
      if (NS_IsAsciiWhitespace(*start))
        *start = ' ';
      ++start;
    }
  }
  aCharacterData = characterData;

  return !characterData.IsEmpty();
}

bool
nsSVGGlyphFrame::GetCharacterPositions(nsTArray<CharacterPosition>* aCharacterPositions,
                                       float aMetricsScale)
{
  uint32_t strLength = mTextRun->GetLength();
  NS_ABORT_IF_FALSE(strLength > 0, "no text");

  const gfxFloat radPerDeg = M_PI / 180.0;

  nsTArray<float> xList, yList;
  GetEffectiveXY(strLength, xList, yList);
  nsTArray<float> dxList, dyList;
  GetEffectiveDxDy(strLength, dxList, dyList);
  nsTArray<float> rotateList;
  GetEffectiveRotate(strLength, rotateList);

  gfxPoint pos = mPosition;
  gfxFloat angle = 0.0;

  nsSVGTextPathFrame *textPath = FindTextPathParent();

  if (textPath) {
    nsRefPtr<gfxFlattenedPath> data = textPath->GetFlattenedPath();

    // textPath frame, but invalid target
    if (!data)
      return false;

    if (!aCharacterPositions->SetLength(strLength))
      return false;

    gfxFloat pathScale = textPath->GetOffsetScale();

    CharacterPosition *cp = aCharacterPositions->Elements();

    gfxFloat length = data->GetLength();

    for (uint32_t i = 0; i < strLength; i++) {
      gfxFloat halfAdvance =
        mTextRun->GetAdvanceWidth(i, 1, nullptr)*aMetricsScale / 2.0;

      // use only x position for horizontal writing
      if (i > 0 && i < xList.Length()) {
        pos.x = xList[i];
      }
      pos.x += (i > 0 && i < dxList.Length()) ? dxList[i] * pathScale : 0.0;
      pos.y += (i > 0 && i < dyList.Length()) ? dyList[i] * pathScale : 0.0;
      if (i < rotateList.Length()) {
        angle = rotateList[i] * radPerDeg;
      }

      // check that we're within the path boundaries
      cp[i].draw = (pos.x + halfAdvance >= 0.0 &&
                    pos.x + halfAdvance <= length);

      if (cp[i].draw) {

        // add y (normal)
        // add rotation
        // move point back along tangent
        gfxPoint pt = data->FindPoint(gfxPoint(pos.x + halfAdvance, pos.y),
                                      &(cp[i].angle));
        cp[i].pos =
          pt - gfxPoint(cos(cp[i].angle), sin(cp[i].angle)) * halfAdvance;
        cp[i].angle += angle;
      }
      pos.x += 2 * halfAdvance;
    }
    return true;
  }

  if (xList.Length() <= 1 &&
      yList.Length() <= 1 &&
      dxList.Length() <= 1 &&
      dyList.Length() <= 1 &&
      rotateList.IsEmpty()) {
    // simple text without individual positioning
    return true;
  }

  if (!aCharacterPositions->SetLength(strLength))
    return false;

  CharacterPosition *cp = aCharacterPositions->Elements();

  uint16_t anchor = GetTextAnchor();

  for (uint32_t i = 0; i < strLength; i++) {
    cp[i].draw = true;

    gfxFloat advance = mTextRun->GetAdvanceWidth(i, 1, nullptr)*aMetricsScale;
    if (xList.Length() > 1 && i < xList.Length()) {
      pos.x = xList[i];

      // apply text-anchor to character
      if (anchor == NS_STYLE_TEXT_ANCHOR_MIDDLE)
        pos.x -= advance/2.0;
      else if (anchor == NS_STYLE_TEXT_ANCHOR_END)
        pos.x -= advance;
    }
    if (yList.Length() > 1 && i < yList.Length()) {
      pos.y = yList[i];
    }
    pos.x += (i > 0 && i < dxList.Length()) ? dxList[i] : 0.0;
    pos.y += (i > 0 && i < dyList.Length()) ? dyList[i] : 0.0;
    cp[i].pos = pos;
    pos.x += advance;
    if (i < rotateList.Length()) {
      angle = rotateList[i] * radPerDeg;
    }
    cp[i].angle = angle;
  }
  return true;
}

uint32_t
nsSVGGlyphFrame::GetTextRunFlags(uint32_t strLength)
{
  // Keep the logic here consistent with GetCharacterPositions

  if (FindTextPathParent()) {
    return gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES;
  }

  nsTArray<float> xList, yList;
  GetEffectiveXY(strLength, xList, yList);
  nsTArray<float> dxList, dyList;
  GetEffectiveDxDy(strLength, dxList, dyList);
  nsTArray<float> rotateList;
  GetEffectiveRotate(strLength, rotateList);

  return (xList.Length() > 1 ||
          yList.Length() > 1 ||
          dxList.Length() > 1 ||
          dyList.Length() > 1 ||
          !rotateList.IsEmpty()) ?
    gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES : 0;
}

float
nsSVGGlyphFrame::GetSubStringAdvance(uint32_t aCharnum, 
                                     uint32_t aFragmentChars,
                                     float aMetricsScale)
{
  if (aFragmentChars == 0)
    return 0.0f;
 
  gfxFloat advance =
    mTextRun->GetAdvanceWidth(aCharnum, aFragmentChars, nullptr) * aMetricsScale;

  nsTArray<float> dxlist, notUsed;
  GetEffectiveDxDy(mTextRun->GetLength(), dxlist, notUsed);
  uint32_t dxcount = dxlist.Length();
  if (dxcount) {
    gfxFloat pathScale = 1.0;
    nsSVGTextPathFrame *textPath = FindTextPathParent();
    if (textPath)
      pathScale = textPath->GetOffsetScale();
    if (dxcount > aFragmentChars) 
      dxcount = aFragmentChars;
    for (uint32_t i = aCharnum; i < dxcount; i++) {
      advance += dxlist[i] * pathScale;
    }
  }

  return float(advance);
}

gfxFloat
nsSVGGlyphFrame::GetBaselineOffset(float aMetricsScale)
{
  gfxTextRun::Metrics metrics =
    mTextRun->MeasureText(0, mTextRun->GetLength(),
                          gfxFont::LOOSE_INK_EXTENTS, nullptr, nullptr);

  uint16_t dominantBaseline;

  for (nsIFrame *frame = GetParent(); frame; frame = frame->GetParent()) {
    dominantBaseline = frame->StyleSVGReset()->mDominantBaseline;
    if (dominantBaseline != NS_STYLE_DOMINANT_BASELINE_AUTO ||
        frame->GetType() == nsGkAtoms::svgTextFrame) {
      break;
    }
  }

  gfxFloat baselineAppUnits;
  switch (dominantBaseline) {
  case NS_STYLE_DOMINANT_BASELINE_HANGING:
    // not really right, but the best we can do with the information provided
    // FALLTHROUGH
  case NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE:
    baselineAppUnits = -metrics.mAscent;
    break;
  case NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE:
  case NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC:
    baselineAppUnits = metrics.mDescent;
    break;
  case NS_STYLE_DOMINANT_BASELINE_CENTRAL:
  case NS_STYLE_DOMINANT_BASELINE_MIDDLE:
    baselineAppUnits = -(metrics.mAscent - metrics.mDescent) / 2.0;
    break;
  case NS_STYLE_DOMINANT_BASELINE_AUTO:
  case NS_STYLE_DOMINANT_BASELINE_ALPHABETIC:
    return 0.0;
  default:
    NS_WARNING("We don't know about this type of dominant-baseline");
    return 0.0;
  }
  return baselineAppUnits * aMetricsScale;
}

DrawMode
nsSVGGlyphFrame::SetupCairoState(gfxContext *aContext,
                                 gfxTextObjectPaint *aOuterObjectPaint,
                                 gfxTextObjectPaint **aThisObjectPaint)
{
  DrawMode toDraw = DrawMode(0);
  SVGTextObjectPaint *thisObjectPaint = new SVGTextObjectPaint();

  if (SetupCairoStroke(aContext, aOuterObjectPaint, thisObjectPaint)) {
    toDraw = DrawMode(toDraw | gfxFont::GLYPH_STROKE);
  }

  if (SetupCairoFill(aContext, aOuterObjectPaint, thisObjectPaint)) {
    toDraw = DrawMode(toDraw | gfxFont::GLYPH_FILL);
  }

  uint32_t paintOrder = StyleSVG()->mPaintOrder;
  while (paintOrder) {
    uint32_t component =
      paintOrder & ((1 << NS_STYLE_PAINT_ORDER_BITWIDTH) - 1);
    if (component == NS_STYLE_PAINT_ORDER_FILL) {
      break;
    }
    if (component == NS_STYLE_PAINT_ORDER_STROKE) {
      toDraw = DrawMode(toDraw | gfxFont::GLYPH_STROKE_UNDERNEATH);
      break;
    }
    paintOrder >>= NS_STYLE_PAINT_ORDER_BITWIDTH;
  }

  *aThisObjectPaint = thisObjectPaint;

  return toDraw;
}

bool
nsSVGGlyphFrame::SetupCairoStroke(gfxContext *aContext,
                                  gfxTextObjectPaint *aOuterObjectPaint,
                                  SVGTextObjectPaint *aThisObjectPaint)
{
  if (!nsSVGUtils::HasStroke(this, aOuterObjectPaint)) {
    return false;
  }

  const nsStyleSVG *style = StyleSVG();
  nsSVGUtils::SetupCairoStrokeHitGeometry(this, aContext, aOuterObjectPaint);
  float opacity = nsSVGUtils::GetOpacity(style->mStrokeOpacitySource,
                                         style->mStrokeOpacity,
                                         aOuterObjectPaint);

  SetupInheritablePaint(aContext, opacity, aOuterObjectPaint,
                        aThisObjectPaint->mStrokePaint, &nsStyleSVG::mStroke,
                        nsSVGEffects::StrokeProperty());

  aThisObjectPaint->SetStrokeOpacity(opacity);

  return opacity != 0.0f;
}

bool
nsSVGGlyphFrame::SetupCairoFill(gfxContext *aContext,
                                gfxTextObjectPaint *aOuterObjectPaint,
                                SVGTextObjectPaint *aThisObjectPaint)
{
  const nsStyleSVG *style = StyleSVG();
  if (style->mFill.mType == eStyleSVGPaintType_None) {
    aThisObjectPaint->SetFillOpacity(0.0f);
    return false;
  }

  float opacity = nsSVGUtils::GetOpacity(style->mFillOpacitySource,
                                         style->mFillOpacity,
                                         aOuterObjectPaint);

  SetupInheritablePaint(aContext, opacity, aOuterObjectPaint,
                        aThisObjectPaint->mFillPaint, &nsStyleSVG::mFill,
                        nsSVGEffects::FillProperty());

  aThisObjectPaint->SetFillOpacity(opacity);

  return true;
}

void
nsSVGGlyphFrame::SetupInheritablePaint(gfxContext *aContext,
                                       float& aOpacity,
                                       gfxTextObjectPaint *aOuterObjectPaint,
                                       SVGTextObjectPaint::Paint& aTargetPaint,
                                       nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                       const FramePropertyDescriptor *aProperty)
{
  const nsStyleSVG *style = StyleSVG();
  nsSVGPaintServerFrame *ps =
    nsSVGEffects::GetPaintServer(this, &(style->*aFillOrStroke), aProperty);

  if (ps && ps->SetupPaintServer(aContext, this, aFillOrStroke, aOpacity)) {
    aTargetPaint.SetPaintServer(this, aContext->CurrentMatrix(), ps);
  } else if (SetupObjectPaint(aContext, aFillOrStroke, aOpacity, aOuterObjectPaint)) {
    aTargetPaint.SetObjectPaint(aOuterObjectPaint, (style->*aFillOrStroke).mType);
  } else {
    nscolor color = nsSVGUtils::GetFallbackOrPaintColor(aContext,
                                                        StyleContext(),
                                                        aFillOrStroke);
    aTargetPaint.SetColor(color);

    nsRefPtr<gfxPattern> pattern =
      new gfxPattern(gfxRGBA(NS_GET_R(color) / 255.0,
                             NS_GET_G(color) / 255.0,
                             NS_GET_B(color) / 255.0,
                             NS_GET_A(color) / 255.0 * aOpacity));

    aContext->SetPattern(pattern);
  }
}

bool
nsSVGGlyphFrame::SetupObjectPaint(gfxContext *aContext,
                                  nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                  float& aOpacity,
                                  gfxTextObjectPaint *aOuterObjectPaint)
{
  if (!aOuterObjectPaint) {
    NS_WARNING("Outer object paint value used outside SVG glyph");
    return false;
  }

  const nsStyleSVG *style = StyleSVG();
  const nsStyleSVGPaint &paint = style->*aFillOrStroke;

  if (paint.mType != eStyleSVGPaintType_ObjectFill &&
      paint.mType != eStyleSVGPaintType_ObjectStroke) {
    return false;
  }

  gfxMatrix current = aContext->CurrentMatrix();
  nsRefPtr<gfxPattern> pattern =
    paint.mType == eStyleSVGPaintType_ObjectFill ?
      aOuterObjectPaint->GetFillPattern(aOpacity, current) :
      aOuterObjectPaint->GetStrokePattern(aOpacity, current);
  if (!pattern) {
    return false;
  }

  aContext->SetPattern(pattern);
  return true;
}

//----------------------------------------------------------------------
// SVGTextObjectPaint methods:

already_AddRefed<gfxPattern>
mozilla::SVGTextObjectPaint::GetFillPattern(float aOpacity,
                                            const gfxMatrix& aCTM)
{
  return mFillPaint.GetPattern(aOpacity, &nsStyleSVG::mFill, aCTM);
}

already_AddRefed<gfxPattern>
mozilla::SVGTextObjectPaint::GetStrokePattern(float aOpacity,
                                              const gfxMatrix& aCTM)
{
  return mStrokePaint.GetPattern(aOpacity, &nsStyleSVG::mStroke, aCTM);
}

already_AddRefed<gfxPattern>
mozilla::SVGTextObjectPaint::Paint::GetPattern(float aOpacity,
                                               nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                               const gfxMatrix& aCTM)
{
  nsRefPtr<gfxPattern> pattern;
  if (mPatternCache.Get(aOpacity, getter_AddRefs(pattern))) {
    // Set the pattern matrix just in case it was messed with by a previous
    // caller. We should get the same matrix each time a pattern is constructed
    // so this should be fine.
    pattern->SetMatrix(aCTM * mPatternMatrix);
    return pattern.forget();
  }

  switch (mPaintType) {
  case eStyleSVGPaintType_None:
    pattern = new gfxPattern(gfxRGBA(0.0f, 0.0f, 0.0f, 0.0f));
    mPatternMatrix = gfxMatrix();
    break;
  case eStyleSVGPaintType_Color:
    pattern = new gfxPattern(gfxRGBA(NS_GET_R(mPaintDefinition.mColor) / 255.0,
                                     NS_GET_G(mPaintDefinition.mColor) / 255.0,
                                     NS_GET_B(mPaintDefinition.mColor) / 255.0,
                                     NS_GET_A(mPaintDefinition.mColor) / 255.0 * aOpacity));
    mPatternMatrix = gfxMatrix();
    break;
  case eStyleSVGPaintType_Server:
    pattern = mPaintDefinition.mPaintServerFrame->GetPaintServerPattern(mFrame,
                                                                        mContextMatrix,
                                                                        aFillOrStroke,
                                                                        aOpacity);
    {
      // m maps original-user-space to pattern space
      gfxMatrix m = pattern->GetMatrix();
      gfxMatrix deviceToOriginalUserSpace = mContextMatrix;
      deviceToOriginalUserSpace.Invert();
      // mPatternMatrix maps device space to pattern space via original user space
      mPatternMatrix = deviceToOriginalUserSpace * m;
    }
    pattern->SetMatrix(aCTM * mPatternMatrix);
    break;
  case eStyleSVGPaintType_ObjectFill:
    pattern = mPaintDefinition.mObjectPaint->GetFillPattern(aOpacity, aCTM);
    // Don't cache this. mObjectPaint will have cached it anyway. If we
    // cache it, we'll have to compute mPatternMatrix, which is annoying.
    return pattern.forget();
  case eStyleSVGPaintType_ObjectStroke:
    pattern = mPaintDefinition.mObjectPaint->GetStrokePattern(aOpacity, aCTM);
    // Don't cache this. mObjectPaint will have cached it anyway. If we
    // cache it, we'll have to compute mPatternMatrix, which is annoying.
    return pattern.forget();
  default:
    return nullptr;
  }

  mPatternCache.Put(aOpacity, pattern);
  return pattern.forget();
}

//----------------------------------------------------------------------
// Internal methods

void
nsSVGGlyphFrame::SetGlyphPosition(gfxPoint *aPosition, bool aForceGlobalTransform)
{
  float drawScale, metricsScale;

  nsSVGTextPathFrame *textPath = FindTextPathParent();
  // In a textPath, the 'y' attribute has no effect, so we reset 'y' here
  // to use aPosition.y for dy only
  if (textPath && textPath->GetFirstPrincipalChild() == this) {
    aPosition->y = 0.0;
  }

  if (!EnsureTextRun(&drawScale, &metricsScale, aForceGlobalTransform))
    return;

  mPosition.MoveTo(aPosition->x, aPosition->y - GetBaselineOffset(metricsScale));

  uint32_t strLength = mTextRun->GetLength();

  nsTArray<float> xList, yList;
  GetEffectiveXY(strLength, xList, yList);
  uint32_t xCount = std::min(xList.Length(), strLength);
  uint32_t yCount = std::min(yList.Length(), strLength);

  // move aPosition to the last glyph position
  gfxFloat x = aPosition->x;
  if (xCount > 1) {
    x = xList[xCount - 1];
    x +=
      mTextRun->GetAdvanceWidth(xCount - 1, 1, nullptr) * metricsScale;

      // advance to the last glyph
      if (strLength > xCount) {
        x +=
          mTextRun->GetAdvanceWidth(xCount, strLength - xCount, nullptr) *
            metricsScale;
      }
  } else {
    x += mTextRun->GetAdvanceWidth(0, strLength, nullptr) * metricsScale;
  }

  gfxFloat y = (textPath || yCount <= 1) ? aPosition->y : yList[yCount - 1];
  aPosition->MoveTo(x, y);

  gfxFloat pathScale = 1.0;
  if (textPath)
    pathScale = textPath->GetOffsetScale();

  nsTArray<float> dxList, dyList;
  GetEffectiveDxDy(strLength, dxList, dyList);

  uint32_t dxcount = std::min(dxList.Length(), strLength);
  if (dxcount > 0) {
    mPosition.x += dxList[0] * pathScale;
  }
  for (uint32_t i = 0; i < dxcount; i++) {
    aPosition->x += dxList[i] * pathScale;
  }
  uint32_t dycount = std::min(dyList.Length(), strLength);
  if (dycount > 0) {
    mPosition.y += dyList[0]* pathScale;
  }
  for (uint32_t i = 0; i < dycount; i++) {
    aPosition->y += dyList[i] * pathScale;
  }
}

nsresult
nsSVGGlyphFrame::GetStartPositionOfChar(uint32_t charnum,
                                        nsISupports **_retval)
{
  *_retval = nullptr;

  CharacterIterator iter(this, false);
  if (!iter.AdvanceToCharacter(charnum))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  NS_ADDREF(*_retval = new DOMSVGPoint(iter.GetPositionData().pos));
  return NS_OK;
}

nsresult
nsSVGGlyphFrame::GetEndPositionOfChar(uint32_t charnum,
                                      nsISupports **_retval)
{
  *_retval = nullptr;

  CharacterIterator iter(this, false);
  if (!iter.AdvanceToCharacter(charnum))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  iter.SetupForMetrics(tmpCtx);
  tmpCtx->MoveTo(gfxPoint(mTextRun->GetAdvanceWidth(charnum, 1, nullptr), 0));
  tmpCtx->IdentityMatrix();
  NS_ADDREF(*_retval = new DOMSVGPoint(tmpCtx->CurrentPoint()));
  return NS_OK;
}

nsresult
nsSVGGlyphFrame::GetExtentOfChar(uint32_t charnum, dom::SVGIRect **_retval)
{
  *_retval = nullptr;

  CharacterIterator iter(this, false);
  if (!iter.AdvanceToCharacter(0))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  uint32_t start = charnum, limit = charnum + 1;
  while (start > 0 && !mTextRun->IsClusterStart(start)) {
    --start;
  }
  while (limit < mTextRun->GetLength() && !mTextRun->IsClusterStart(limit)) {
    ++limit;
  }

  if (start > 0 && !iter.AdvanceToCharacter(start))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  gfxTextRun::Metrics metrics =
    mTextRun->MeasureText(start, limit - start, gfxFont::LOOSE_INK_EXTENTS,
                          nullptr, nullptr);

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  iter.SetupForMetrics(tmpCtx);
  tmpCtx->Rectangle(gfxRect(0, -metrics.mAscent,
                            metrics.mAdvanceWidth,
                            metrics.mAscent + metrics.mDescent));
  tmpCtx->IdentityMatrix();

  nsRefPtr<dom::SVGRect> rect;
  nsresult rv = NS_NewSVGRect(getter_AddRefs(rect), tmpCtx->GetUserPathExtent());
  NS_ENSURE_SUCCESS(rv, rv);

  rect.forget(_retval);
  return NS_OK;
}

nsresult
nsSVGGlyphFrame::GetRotationOfChar(uint32_t charnum, float *_retval)
{
  CharacterIterator iter(this, false);
  if (!iter.AdvanceToCharacter(charnum))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  CharacterPosition pos = iter.GetPositionData();
  if (!pos.draw)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  const gfxFloat radPerDeg = M_PI/180.0;
  *_retval = float(pos.angle / radPerDeg);
  return NS_OK;
}

float
nsSVGGlyphFrame::GetAdvance(bool aForceGlobalTransform)
{
  float drawScale, metricsScale;
  if (!EnsureTextRun(&drawScale, &metricsScale, aForceGlobalTransform))
    return 0.0f;

  return GetSubStringAdvance(0, mTextRun->GetLength(), metricsScale);
}

nsSVGTextPathFrame*
nsSVGGlyphFrame::FindTextPathParent()
{
  /* check if we're the child of a textPath */
  for (nsIFrame *frame = GetParent();
       frame != nullptr;
       frame = frame->GetParent()) {
    nsIAtom* type = frame->GetType();
    if (type == nsGkAtoms::svgTextPathFrame) {
      return static_cast<nsSVGTextPathFrame*>(frame);
    } else if (type == nsGkAtoms::svgTextFrame)
      return nullptr;
  }
  return nullptr;
}

bool
nsSVGGlyphFrame::IsStartOfChunk()
{
  // this fragment is a chunk if it has a corresponding absolute
  // position adjustment in an ancestors' x or y array. (At the moment
  // we don't map the full arrays, but only the first elements.)

  return false;
}

void
nsSVGGlyphFrame::GetXY(SVGUserUnitList *aX, SVGUserUnitList *aY)
{
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetXY(aX, aY);
}

void
nsSVGGlyphFrame::SetStartIndex(uint32_t aStartIndex)
{
  mStartIndex = aStartIndex;
}

void
nsSVGGlyphFrame::GetEffectiveXY(int32_t strLength, nsTArray<float> &aX, nsTArray<float> &aY)
{
  nsTArray<float> x, y;
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetEffectiveXY(x, y);

  int32_t xCount = std::max((int32_t)(x.Length() - mStartIndex), 0);
  xCount = std::min(xCount, strLength);
  aX.AppendElements(x.Elements() + mStartIndex, xCount);

  int32_t yCount = std::max((int32_t)(y.Length() - mStartIndex), 0);
  yCount = std::min(yCount, strLength);
  aY.AppendElements(y.Elements() + mStartIndex, yCount);
}

void
nsSVGGlyphFrame::GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy)
{
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetDxDy(aDx, aDy);
}

void
nsSVGGlyphFrame::GetEffectiveDxDy(int32_t strLength, nsTArray<float> &aDx, nsTArray<float> &aDy)
{
  nsTArray<float> dx, dy;
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetEffectiveDxDy(dx, dy);

  int32_t dxCount = std::max((int32_t)(dx.Length() - mStartIndex), 0);
  dxCount = std::min(dxCount, strLength);
  aDx.AppendElements(dx.Elements() + mStartIndex, dxCount);

  int32_t dyCount = std::max((int32_t)(dy.Length() - mStartIndex), 0);
  dyCount = std::min(dyCount, strLength);
  aDy.AppendElements(dy.Elements() + mStartIndex, dyCount);
}

const SVGNumberList*
nsSVGGlyphFrame::GetRotate()
{
  nsSVGTextContainerFrame *containerFrame;
  containerFrame = static_cast<nsSVGTextContainerFrame *>(mParent);
  if (containerFrame)
    return containerFrame->GetRotate();
  return nullptr;
}

void
nsSVGGlyphFrame::GetEffectiveRotate(int32_t strLength, nsTArray<float> &aRotate)
{
  nsTArray<float> rotate;
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetEffectiveRotate(rotate);

  int32_t rotateCount = std::max((int32_t)(rotate.Length() - mStartIndex), 0);
  rotateCount = std::min(rotateCount, strLength);
  if (rotateCount > 0) {
    aRotate.AppendElements(rotate.Elements() + mStartIndex, rotateCount);
  } else if (!rotate.IsEmpty()) {
    // rotate is applied for extra characters too
    aRotate.AppendElement(rotate[rotate.Length() - 1]);
  }
}

uint16_t
nsSVGGlyphFrame::GetTextAnchor()
{
  return StyleSVG()->mTextAnchor;
}

bool
nsSVGGlyphFrame::IsAbsolutelyPositioned()
{
  bool hasTextPathAncestor = false;
  for (nsIFrame *frame = GetParent();
       frame != nullptr;
       frame = frame->GetParent()) {

    // at the start of a 'text' element
    // at the start of each 'textPath' element
    if (frame->GetType() == nsGkAtoms::svgTextPathFrame) {
      hasTextPathAncestor = true;
    }
    if ((frame->GetType() == nsGkAtoms::svgTextFrame ||
         frame->GetType() == nsGkAtoms::svgTextPathFrame) &&
        frame->GetFirstPrincipalChild() == this) {
        return true;
    }

    if (frame->GetType() == nsGkAtoms::svgTextFrame)
      break;
  }

  // for each character within a 'text', 'tspan', 'tref' and 'altGlyph' element
  // which has an x or y attribute value assigned to it explicitly
  nsTArray<float> x, y;
  GetEffectiveXY(GetNumberOfChars(), x, y);
  // Note: the y of descendants of textPath has no effect in horizontal writing
  return (!x.IsEmpty() || (!hasTextPathAncestor && !y.IsEmpty()));
}


//----------------------------------------------------------------------
// nsISVGGlyphFragmentNode interface:

uint32_t
nsSVGGlyphFrame::GetNumberOfChars()
{
  if (mCompressWhitespace) {
    nsAutoString text;
    GetCharacterData(text);
    return text.Length();
  }

  return mContent->TextLength();
}

float
nsSVGGlyphFrame::GetComputedTextLength()
{
  return GetAdvance(false);
}

float
nsSVGGlyphFrame::GetSubStringLength(uint32_t charnum, uint32_t fragmentChars)
{
  float drawScale, metricsScale;
  if (!EnsureTextRun(&drawScale, &metricsScale, false))
    return 0.0f;

  return GetSubStringAdvance(charnum, fragmentChars, metricsScale);
}

int32_t
nsSVGGlyphFrame::GetCharNumAtPosition(nsISVGPoint *point)
{
  float xPos = point->X(), yPos = point->Y();

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  CharacterIterator iter(this, false);

  uint32_t i;
  int32_t last = -1;
  gfxPoint pt(xPos, yPos);
  while ((i = iter.NextCluster()) != iter.InvalidCluster()) {
    uint32_t limit = i + iter.ClusterLength();
    gfxTextRun::Metrics metrics =
      mTextRun->MeasureText(i, limit - i, gfxFont::LOOSE_INK_EXTENTS,
                            nullptr, nullptr);

    // the SVG spec tells us to divide the width of the cluster equally among
    // its chars, so we'll step through the chars, allocating a share of the
    // total advance to each
    int32_t current, end, step;
    if (mTextRun->IsRightToLeft()) {
      current = limit - 1;
      end = i - 1;
      step = -1;
    } else {
      current = i;
      end = limit;
      step = 1;
    }
    gfxFloat leftEdge = 0.0;
    gfxFloat width = metrics.mAdvanceWidth / (limit - i);
    while (current != end) {
      iter.SetupForMetrics(tmpCtx);
      tmpCtx->NewPath();
      tmpCtx->Rectangle(gfxRect(leftEdge, -metrics.mAscent,
                                width, metrics.mAscent + metrics.mDescent));
      tmpCtx->IdentityMatrix();
      if (tmpCtx->PointInFill(pt)) {
        // Can't return yet; if there's glyph overlap, the last character
        // to be rendered wins, so we still have to check the rest...
        last = current;
        break; // ...but we don't need to check more slices of this cluster
      }
      current += step;
      leftEdge += width;
    }
  }

  return last;
}

NS_IMETHODIMP_(nsSVGGlyphFrame *)
nsSVGGlyphFrame::GetFirstGlyphFrame()
{
  nsSVGGlyphFrame *frame = this;
  while (frame && frame->IsTextEmpty()) {
    frame = frame->GetNextGlyphFrame();
  }
  return frame;
}

NS_IMETHODIMP_(nsSVGGlyphFrame *)
nsSVGGlyphFrame::GetNextGlyphFrame()
{
  nsIFrame* sibling = GetNextSibling();
  while (sibling) {
    nsISVGGlyphFragmentNode *node = do_QueryFrame(sibling);
    if (node)
      return node->GetFirstGlyphFrame();
    sibling = sibling->GetNextSibling();
  }

  // no more siblings. go back up the tree.
  
  NS_ASSERTION(GetParent(), "null parent");
  nsISVGGlyphFragmentNode *node = do_QueryFrame(GetParent());
  return node ? node->GetNextGlyphFrame() : nullptr;
}

bool
nsSVGGlyphFrame::EndsWithWhitespace() const
{
  const nsTextFragment* text = mContent->GetText();
  NS_ABORT_IF_FALSE(text->GetLength() > 0, "text expected");

  return NS_IsAsciiWhitespace(text->CharAt(text->GetLength() - 1));
}

bool
nsSVGGlyphFrame::IsAllWhitespace() const
{
  const nsTextFragment* text = mContent->GetText();

  if (text->Is2b())
    return false;
  int32_t len = text->GetLength();
  const char* str = text->Get1b();
  for (int32_t i = 0; i < len; ++i) {
    if (!NS_IsAsciiWhitespace(str[i]))
      return false;
  }
  return true;
}

//----------------------------------------------------------------------
//

void
nsSVGGlyphFrame::NotifyGlyphMetricsChange()
{
  nsSVGTextContainerFrame *containerFrame =
    static_cast<nsSVGTextContainerFrame *>(mParent);
  if (containerFrame)
    containerFrame->NotifyGlyphMetricsChange();
}

void
nsSVGGlyphFrame::SetupGlobalTransform(gfxContext *aContext, uint32_t aFor)
{
  gfxMatrix matrix = GetCanvasTM(aFor);
  if (!matrix.IsSingular()) {
    aContext->Multiply(matrix);
  }
}

void
nsSVGGlyphFrame::ClearTextRun()
{
  delete mTextRun;
  mTextRun = nullptr;
}

bool
nsSVGGlyphFrame::EnsureTextRun(float *aDrawScale, float *aMetricsScale,
                               bool aForceGlobalTransform)
{
  // Compute the size at which the text should render (excluding the CTM)
  const nsStyleFont* fontData = StyleFont();
  // Since SVG has its own scaling, we really don't want
  // fonts in SVG to respond to the browser's "TextZoom"
  // (Ctrl++,Ctrl+-)
  nsPresContext *presContext = PresContext();
  float textZoom = presContext->TextZoom();
  double size =
    presContext->AppUnitsToFloatCSSPixels(fontData->mSize) / textZoom;

  double textRunSize;
  if (mTextRun) {
    textRunSize = mTextRun->GetFontGroup()->GetStyle()->size;
  } else {
    nsAutoString text;
    if (!GetCharacterData(text))
      return false;

    nsAutoString visualText;
      
    /*
     * XXXsmontagu: The SVG spec says:
     *
     * http://www.w3.org/TR/SVG11/text.html#DirectionProperty
     *  "For the 'direction' property to have any effect, the 'unicode-bidi'
     *   property's value must be embed or bidi-override."
     *
     * The SVGTiny spec, on the other hand, says 
     *
     * http://www.w3.org/TR/SVGTiny12/text.html#DirectionProperty
     *  "For the 'direction' property to have any effect on an element that 
     *   does not by itself establish a new text chunk (such as the 'tspan'
     *   element in SVG 1.2 Tiny), the 'unicode-bidi' property's value must
     *   be embed or bidi-override."
     *
     * Note that this is different from HTML/CSS, where setting the 'dir'
     *  attribute on an inline element automatically sets unicode-bidi: embed
     *
     * Our current implementation of bidi in SVG does not distinguish between
     * different text elements, but treats every text container frame as a
     * new text chunk, so we always set the base direction according to the
     * direction property
     *
     * See also XXXsmontagu comments in nsSVGTextFrame::UpdateGlyphPositioning
     */
    
    // Get the unicodeBidi property from the parent, because it doesn't
    // inherit
    bool bidiOverride = !!(mParent->StyleTextReset()->mUnicodeBidi &
                           NS_STYLE_UNICODE_BIDI_OVERRIDE);
    nsBidiLevel baseDirection =
      StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL ?
        NSBIDI_RTL : NSBIDI_LTR;
    nsBidiPresUtils::CopyLogicalToVisual(text, visualText,
                                         baseDirection, bidiOverride);
    if (!visualText.IsEmpty()) {
      text = visualText;
    }

    gfxMatrix m;
    if (aForceGlobalTransform ||
        !(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
      m = GetCanvasTM(mGetCanvasTMForFlag);
      if (m.IsSingular())
        return false;
    }

    // The context scale is the ratio of the length of the transformed
    // diagonal vector (1,1) to the length of the untransformed diagonal
    // (which is sqrt(2)).
    gfxPoint p = m.Transform(gfxPoint(1, 1)) - m.Transform(gfxPoint(0, 0));
    double contextScale = SVGContentUtils::ComputeNormalizedHypotenuse(p.x, p.y);

    if (StyleSVG()->mTextRendering ==
        NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION) {
      textRunSize = PRECISE_SIZE;
    } else {
      textRunSize = size*contextScale;
      textRunSize = std::max(textRunSize, double(CLAMP_MIN_SIZE));
      textRunSize = std::min(textRunSize, double(CLAMP_MAX_SIZE));
    }

    const nsFont& font = fontData->mFont;
    bool printerFont = (presContext->Type() == nsPresContext::eContext_PrintPreview ||
                          presContext->Type() == nsPresContext::eContext_Print);
    gfxFontStyle fontStyle(font.style, font.weight, font.stretch, textRunSize,
                           StyleFont()->mLanguage,
                           font.sizeAdjust, font.systemFont,
                           printerFont,
                           font.languageOverride);

    font.AddFontFeaturesToStyle(&fontStyle);

    nsRefPtr<gfxFontGroup> fontGroup =
      gfxPlatform::GetPlatform()->CreateFontGroup(font.name, &fontStyle, presContext->GetUserFontSet());

    uint32_t flags = gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX |
      GetTextRunFlags(text.Length()) |
      nsLayoutUtils::GetTextRunFlagsForStyle(StyleContext(), StyleFont(), 0);

    // XXX We should use a better surface here! But then we'd have to
    // change things so we can ensure we always have the "right" sort of
    // surface available, by creating the textrun only at the right times
    nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
    tmpCtx->SetMatrix(m);

    // Use only the fonts' internal word caching here.
    // We don't cache the textrun globally because we create
    // a new fontgroup every time. Even if we cached fontgroups, we
    // might render at very many different sizes (e.g. during zoom
    // animation) and caching a textrun for each such size would be bad.
    gfxTextRunFactory::Parameters params = {
        tmpCtx, nullptr, nullptr, nullptr, 0, GetTextRunUnitsFactor()
    };
    mTextRun =
      fontGroup->MakeTextRun(text.get(), text.Length(), &params, flags);
    if (!mTextRun)
      return false;
  }

  *aDrawScale = float(size/textRunSize);
  *aMetricsScale = (*aDrawScale)/GetTextRunUnitsFactor();
  return true;
}

//----------------------------------------------------------------------
// helper class

CharacterIterator::CharacterIterator(nsSVGGlyphFrame *aSource,
        bool aForceGlobalTransform)
  : mSource(aSource)
  , mCurrentAdvance(0)
  , mCurrentChar(uint32_t(-1))
  , mInError(false)
{
  if (!aSource->EnsureTextRun(&mDrawScale, &mMetricsScale,
                              aForceGlobalTransform) ||
      !aSource->GetCharacterPositions(&mPositions, mMetricsScale)) {
    mInError = true;
  }
}

bool
CharacterIterator::SetupForDirectTextRun(gfxContext *aContext, float aScale)
{
  if (!mPositions.IsEmpty() || mInError)
    return false;
  aContext->SetMatrix(mInitialMatrix);
  aContext->Translate(mSource->mPosition);
  aContext->Scale(aScale, aScale);
  return true;
}

uint32_t
CharacterIterator::NextCluster()
{
  if (mInError) {
#ifdef DEBUG
    if (mCurrentChar != InvalidCluster()) {
      bool pastEnd = (mCurrentChar >= mSource->mTextRun->GetLength());
      NS_ABORT_IF_FALSE(pastEnd, "Past the end of CharacterIterator. Missing Reset?");
    }
#endif
    return InvalidCluster();
  }

  while (true) {
    if (mCurrentChar != InvalidCluster() &&
        (mPositions.IsEmpty() || mPositions[mCurrentChar].draw)) {
      mCurrentAdvance +=
        mSource->mTextRun->GetAdvanceWidth(mCurrentChar, 1, nullptr);
    }
    ++mCurrentChar;

    if (mCurrentChar >= mSource->mTextRun->GetLength()) {
      mInError = true;
      return InvalidCluster();
    }

    if (mSource->mTextRun->IsClusterStart(mCurrentChar) &&
        (mPositions.IsEmpty() || mPositions[mCurrentChar].draw)) {
      return mCurrentChar;
    }
  }
}

uint32_t
CharacterIterator::ClusterLength()
{
  if (mInError) {
    return 0;
  }

  uint32_t i = mCurrentChar;
  while (++i < mSource->mTextRun->GetLength()) {
    if (mSource->mTextRun->IsClusterStart(i)) {
      break;
    }
  }
  return i - mCurrentChar;
}

bool
CharacterIterator::AdvanceToCharacter(uint32_t aIndex)
{
  while (NextCluster() != InvalidCluster()) {
    if (mCurrentChar == aIndex)
      return true;
  }
  return false;
}

void
CharacterIterator::SetupFor(gfxContext *aContext, float aScale)
{
  NS_ASSERTION(!mInError, "We should not have reached here");

  aContext->SetMatrix(mInitialMatrix);
  if (mPositions.IsEmpty()) {
    aContext->Translate(mSource->mPosition);
    aContext->Scale(aScale, aScale);
    aContext->Translate(gfxPoint(mCurrentAdvance, 0));
  } else {
    aContext->Translate(mPositions[mCurrentChar].pos);
    aContext->Rotate(mPositions[mCurrentChar].angle);
    aContext->Scale(aScale, aScale);
  }
}

CharacterPosition
CharacterIterator::GetPositionData()
{
  if (!mPositions.IsEmpty())
    return mPositions[mCurrentChar];

  gfxFloat advance = mCurrentAdvance * mMetricsScale;
  CharacterPosition cp =
    { mSource->mPosition + gfxPoint(advance, 0), 0, true };
  return cp;
}
