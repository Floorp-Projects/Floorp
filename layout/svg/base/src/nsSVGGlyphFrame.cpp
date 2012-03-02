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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsSVGTextFrame.h"
#include "mozilla/LookAndFeel.h"
#include "nsTextFragment.h"
#include "nsBidiPresUtils.h"
#include "nsSVGUtils.h"
#include "SVGLengthList.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGRect.h"
#include "DOMSVGPoint.h"
#include "nsSVGGlyphFrame.h"
#include "nsSVGTextPathFrame.h"
#include "nsSVGPathElement.h"
#include "nsSVGRect.h"
#include "nsDOMError.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxPlatform.h"
#include "nsSVGEffects.h"
#include "nsSVGPaintServerFrame.h"

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
  void SetInitialMatrix(gfxContext *aContext) {
    mInitialMatrix = aContext->CurrentMatrix();
    if (mInitialMatrix.IsSingular()) {
      mInError = true;
    }
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
      for (PRUint32 i = 0; i <  dashes.Length(); i++) {
        dashes[i] /= mDrawScale;
      }
      aContext->SetDash(dashes.Elements(), dashes.Length(), dashOffset / mDrawScale);
    }
  }

  /**
   * Returns the index of the next cluster in the string that should be drawn,
   * or InvalidCluster() (i.e. PRUint32(-1)) if there is no such cluster.
   */
  PRUint32 NextCluster();

  /**
   * Returns the length of the current cluster (usually 1, unless there
   * are combining marks)
   */
  PRUint32 ClusterLength();

  /**
   * Repeated calls NextCluster until it returns aIndex (i.e. aIndex is the
   * current drawable character). Returns false if that never happens
   * (because aIndex is before or equal to the current character, or
   * out of bounds, or not drawable).
   */
  bool AdvanceToCharacter(PRUint32 aIndex);

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
  PRUint32 InvalidCluster() {
    return PRUint32(-1);
  }

private:
  bool SetupForDirectTextRun(gfxContext *aContext, float aScale);
  void SetupFor(gfxContext *aContext, float aScale);

  nsSVGGlyphFrame *mSource;
  nsAutoTArray<CharacterPosition,80> mPositions;
  gfxMatrix mInitialMatrix;
  // Textrun advance width from start to mCurrentChar, in appunits
  gfxFloat mCurrentAdvance;
  PRUint32 mCurrentChar;
  float mDrawScale;
  float mMetricsScale;
  bool mInError;
};

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
  ClearTextRun();
  NotifyGlyphMetricsChange();
  if (IsTextEmpty()) {
    // That's it for this frame. Leave no trace we were here
    nsSVGUtils::UpdateGraphic(this);
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

NS_IMETHODIMP
nsSVGGlyphFrame::IsSelectable(bool* aIsSelectable,
                              PRUint8* aSelectStyle) const
{
  nsresult rv = nsSVGGlyphFrameBase::IsSelectable(aIsSelectable, aSelectStyle);
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::IsSelectable()=(%d,%d)\n", this, *aIsSelectable, aSelectStyle);
#endif
  return rv;
}

NS_IMETHODIMP
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

  return nsSVGGlyphFrameBase::Init(aContent, aParent, aPrevInFlow);
}

nsIAtom *
nsSVGGlyphFrame::GetType() const
{
  return nsGkAtoms::svgGlyphFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGGlyphFrame::PaintSVG(nsRenderingContext *aContext,
                          const nsIntRect *aDirtyRect)
{
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  gfxContext *gfx = aContext->ThebesContext();
  PRUint16 renderMode = SVGAutoRenderState::GetRenderMode(aContext);

  switch (GetStyleSVG()->mTextRendering) {
  case NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED:
    gfx->SetAntialiasMode(gfxContext::MODE_ALIASED);
    break;
  default:
    gfx->SetAntialiasMode(gfxContext::MODE_COVERAGE);
    break;
  }

  if (renderMode != SVGAutoRenderState::NORMAL) {

    gfxMatrix matrix = gfx->CurrentMatrix();
    SetupGlobalTransform(gfx);

    CharacterIterator iter(this, true);
    iter.SetInitialMatrix(gfx);

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

    gfx->SetMatrix(matrix);
    return NS_OK;
  }

  // We are adding patterns or gradients to the context. Save
  // it so we don't leak them into the next object we draw
  gfx->Save();
  SetupGlobalTransform(gfx);

  CharacterIterator iter(this, true);
  iter.SetInitialMatrix(gfx);

  nsRefPtr<gfxPattern> strokePattern;
  DrawMode drawMode = SetupCairoState(gfx, getter_AddRefs(strokePattern));

  if (drawMode) {
    DrawCharacters(&iter, gfx, drawMode, strokePattern);
  }
  
  gfx->Restore();

  return NS_OK;
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGGlyphFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  PRUint16 hitTestFlags = GetHitTestFlags();
  if (!hitTestFlags) {
    return nsnull;
  }

  nsRefPtr<gfxContext> context = MakeTmpCtx();
  SetupGlobalTransform(context);
  CharacterIterator iter(this, true);
  iter.SetInitialMatrix(context);

  // The SVG 1.1 spec says that text is hit tested against the character cells
  // of the text, not the fill and stroke. See the section starting "For text
  // elements..." here:
  //
  //   http://www.w3.org/TR/SVG11/interact.html#PointerEventsProperty
  //
  // Currently we just test the character cells if GetHitTestFlags says we're
  // supposed to be testing either the fill OR the stroke:

  PRUint32 i;
  while ((i = iter.NextCluster()) != iter.InvalidCluster()) {
    gfxTextRun::Metrics metrics =
    mTextRun->MeasureText(i, iter.ClusterLength(),
                          gfxFont::LOOSE_INK_EXTENTS, nsnull, nsnull);
    iter.SetupForMetrics(context);
    context->Rectangle(metrics.mBoundingBox);
  }

  gfxPoint userSpacePoint =
    context->DeviceToUser(gfxPoint(PresContext()->AppUnitsToGfxUnits(aPoint.x),
                                   PresContext()->AppUnitsToGfxUnits(aPoint.y)));

  bool isHit = false;
  if (hitTestFlags & SVG_HIT_TEST_FILL || hitTestFlags & SVG_HIT_TEST_STROKE) {
    isHit = context->PointInFill(userSpacePoint);
  }

  // If isHit is false, we may also want to fill and stroke the text to check
  // whether the pointer is over an area of fill or stroke that lies outside
  // the character cells. (With a thick stroke, or with fonts like Zapfino, such
  // areas may be very significant.) This is what Opera appears to do, but
  // currently we do not.

  if (isHit && nsSVGUtils::HitTestClip(this, aPoint))
    return this;

  return nsnull;
}

NS_IMETHODIMP_(nsRect)
nsSVGGlyphFrame::GetCoveredRegion()
{
  // See bug 614732 comment 32:
  //return nsSVGUtils::TransformFrameRectToOuterSVG(mRect, GetCanvasTM(), PresContext());
  return mCoveredRegion;
}

NS_IMETHODIMP
nsSVGGlyphFrame::UpdateCoveredRegion()
{
  mRect.SetEmpty();

  // XXX here we have tmpCtx use its default identity matrix, but does this
  // function call anything that will call GetCanvasTM and break things?
  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();

  bool hasStroke = HasStroke();
  if (hasStroke) {
    SetupCairoStrokeGeometry(tmpCtx);
  } else if (GetStyleSVG()->mFill.mType == eStyleSVGPaintType_None) {
    return NS_OK;
  }

  CharacterIterator iter(this, true);
  iter.SetInitialMatrix(tmpCtx);
  AddBoundingBoxesToPath(&iter, tmpCtx);
  tmpCtx->IdentityMatrix();

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
  // gfxContext for *stroke* extents, we'll neet to wrap the
  // AddBoundingBoxesToPath() call with CurrentLineWidth()/SetLineWidth()
  // calls to record and then reset the stroke width.
  gfxRect extent = tmpCtx->GetUserPathExtent();
  if (hasStroke) {
    extent =
      nsSVGUtils::PathExtentsToMaxStrokeExtents(extent, this, gfxMatrix());
  }

  if (!extent.IsEmpty()) {
    mRect = nsLayoutUtils::RoundGfxRectToAppRect(extent, 
              PresContext()->AppUnitsPerCSSPixel());
  }

  // See bug 614732 comment 32.
  mCoveredRegion = nsSVGUtils::TransformFrameRectToOuterSVG(
    mRect, GetCanvasTM(), PresContext());

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::InitialUpdate()
{
  NS_ASSERTION(GetStateBits() & NS_FRAME_FIRST_REFLOW,
               "Yikes! We've been called already! Hopefully we weren't called "
               "before our nsSVGOuterSVGFrame's initial Reflow()!!!");

  NS_ASSERTION(!(mState & NS_FRAME_IN_REFLOW),
               "We don't actually participate in reflow");

  // Do unset the various reflow bits, though.
  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);
  
  return NS_OK;
}  

void
nsSVGGlyphFrame::NotifySVGChanged(PRUint32 aFlags)
{
  if (aFlags & TRANSFORM_CHANGED) {
    ClearTextRun();
  }
  if (!(aFlags & SUPPRESS_INVALIDATION)) {
    nsSVGUtils::UpdateGraphic(this);
  }
}

void
nsSVGGlyphFrame::NotifyRedrawSuspended()
{
  AddStateBits(NS_STATE_SVG_REDRAW_SUSPENDED);
}

void
nsSVGGlyphFrame::NotifyRedrawUnsuspended()
{
  RemoveStateBits(NS_STATE_SVG_REDRAW_SUSPENDED);

  if (GetStateBits() & NS_STATE_SVG_DIRTY)
    nsSVGUtils::UpdateGraphic(this);
}

void
nsSVGGlyphFrame::AddBoundingBoxesToPath(CharacterIterator *aIter,
                                        gfxContext *aContext)
{
  if (aIter->SetupForDirectTextRunMetrics(aContext)) {
    gfxTextRun::Metrics metrics =
      mTextRun->MeasureText(0, mTextRun->GetLength(),
                            gfxFont::LOOSE_INK_EXTENTS, nsnull, nsnull);
    aContext->Rectangle(metrics.mBoundingBox);
    return;
  }

  PRUint32 i;
  while ((i = aIter->NextCluster()) != aIter->InvalidCluster()) {
    aIter->SetupForMetrics(aContext);
    gfxTextRun::Metrics metrics =
      mTextRun->MeasureText(i, aIter->ClusterLength(),
                            gfxFont::LOOSE_INK_EXTENTS, nsnull, nsnull);
    aContext->Rectangle(metrics.mBoundingBox);
  }
}

void
nsSVGGlyphFrame::DrawCharacters(CharacterIterator *aIter,
                                gfxContext *aContext,
                                DrawMode aDrawMode,
                                gfxPattern *aStrokePattern)
{
  if (aDrawMode & gfxFont::GLYPH_STROKE) {
    aIter->SetLineWidthAndDashesForDrawing(aContext);
  }

  if (aIter->SetupForDirectTextRunDrawing(aContext)) {
    mTextRun->Draw(aContext, gfxPoint(0, 0), aDrawMode, 0,
                   mTextRun->GetLength(), nsnull, nsnull, aStrokePattern);
    return;
  }

  PRUint32 i;
  while ((i = aIter->NextCluster()) != aIter->InvalidCluster()) {
    aIter->SetupForDrawing(aContext);
    mTextRun->Draw(aContext, gfxPoint(0, 0), aDrawMode, i,
                   aIter->ClusterLength(), nsnull, nsnull, aStrokePattern);
  }
}

gfxRect
nsSVGGlyphFrame::GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                     PRUint32 aFlags)
{
  if (mOverrideCanvasTM) {
    *mOverrideCanvasTM = aToBBoxUserspace;
  } else {
    mOverrideCanvasTM = new gfxMatrix(aToBBoxUserspace);
  }

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  SetupGlobalTransform(tmpCtx);
  CharacterIterator iter(this, true);
  iter.SetInitialMatrix(tmpCtx);
  AddBoundingBoxesToPath(&iter, tmpCtx);
  tmpCtx->IdentityMatrix();

  mOverrideCanvasTM = nsnull;

  gfxRect bbox;

  gfxRect pathExtents = tmpCtx->GetUserPathExtent();

  // Account for fill:
  if ((aFlags & nsSVGUtils::eBBoxIncludeFill) != 0 &&
      ((aFlags & nsSVGUtils::eBBoxIgnoreFillIfNone) == 0 ||
       GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None)) {
    bbox = pathExtents;
  }

  // Account for stroke:
  if ((aFlags & nsSVGUtils::eBBoxIncludeStroke) != 0 &&
      ((aFlags & nsSVGUtils::eBBoxIgnoreStrokeIfNone) == 0 || HasStroke())) {
    bbox =
      bbox.Union(nsSVGUtils::PathExtentsToMaxStrokeExtents(pathExtents,
                                                           this,
                                                           aToBBoxUserspace));
  }

  return bbox;
}

//----------------------------------------------------------------------
// nsSVGGeometryFrame methods:

gfxMatrix
nsSVGGlyphFrame::GetCanvasTM()
{
  if (mOverrideCanvasTM) {
    return *mOverrideCanvasTM;
  }
  NS_ASSERTION(mParent, "null parent");
  return static_cast<nsSVGContainerFrame*>(mParent)->GetCanvasTM();
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
  PRUint32 strLength = mTextRun->GetLength();
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

    for (PRUint32 i = 0; i < strLength; i++) {
      gfxFloat halfAdvance =
        mTextRun->GetAdvanceWidth(i, 1, nsnull)*aMetricsScale / 2.0;

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

  PRUint16 anchor = GetTextAnchor();

  for (PRUint32 i = 0; i < strLength; i++) {
    cp[i].draw = true;

    gfxFloat advance = mTextRun->GetAdvanceWidth(i, 1, nsnull)*aMetricsScale;
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

PRUint32
nsSVGGlyphFrame::GetTextRunFlags(PRUint32 strLength)
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
nsSVGGlyphFrame::GetSubStringAdvance(PRUint32 aCharnum, 
                                     PRUint32 aFragmentChars,
                                     float aMetricsScale)
{
  if (aFragmentChars == 0)
    return 0.0f;
 
  gfxFloat advance =
    mTextRun->GetAdvanceWidth(aCharnum, aFragmentChars, nsnull) * aMetricsScale;

  nsTArray<float> dxlist, notUsed;
  GetEffectiveDxDy(mTextRun->GetLength(), dxlist, notUsed);
  PRUint32 dxcount = dxlist.Length();
  if (dxcount) {
    gfxFloat pathScale = 1.0;
    nsSVGTextPathFrame *textPath = FindTextPathParent();
    if (textPath)
      pathScale = textPath->GetOffsetScale();
    if (dxcount > aFragmentChars) 
      dxcount = aFragmentChars;
    for (PRUint32 i = aCharnum; i < dxcount; i++) {
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
                          gfxFont::LOOSE_INK_EXTENTS, nsnull, nsnull);

  PRUint16 dominantBaseline;

  for (nsIFrame *frame = GetParent(); frame; frame = frame->GetParent()) {
    dominantBaseline = frame->GetStyleSVGReset()->mDominantBaseline;
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
nsSVGGlyphFrame::SetupCairoState(gfxContext *aContext, gfxPattern **aStrokePattern)
{
  DrawMode toDraw = DrawMode(0);
  const nsStyleSVG* style = GetStyleSVG();

  if (HasStroke()) {
    gfxContextMatrixAutoSaveRestore matrixRestore(aContext);
    aContext->IdentityMatrix();

    toDraw = DrawMode(toDraw | gfxFont::GLYPH_STROKE);

    SetupCairoStrokeHitGeometry(aContext);
    float opacity = style->mStrokeOpacity;
    nsSVGPaintServerFrame *ps = GetPaintServer(&style->mStroke,
                                               nsSVGEffects::StrokeProperty());

    nsRefPtr<gfxPattern> strokePattern;

    if (ps) {
      // Gradient or Pattern: can get pattern directly from frame
      strokePattern = ps->GetPaintServerPattern(this, opacity);
    }

    if (!strokePattern) {
      nscolor color;
      nsSVGUtils::GetFallbackOrPaintColor(aContext, GetStyleContext(),
                                          &nsStyleSVG::mStroke, &opacity,
                                          &color);
      strokePattern = new gfxPattern(gfxRGBA(NS_GET_R(color) / 255.0,
                                             NS_GET_G(color) / 255.0,
                                             NS_GET_B(color) / 255.0,
                                             NS_GET_A(color) / 255.0 * opacity));
    }

    strokePattern.forget(aStrokePattern);
  }

  if (SetupCairoFill(aContext)) {
    toDraw = DrawMode(toDraw | gfxFont::GLYPH_FILL);
  }

  return toDraw;
}

//----------------------------------------------------------------------

// Utilities for converting from indices in the uncompressed content
// element strings to compressed frame string and back:
static int
CompressIndex(int index, const nsTextFragment*fragment)
{
  int ci=0;
  if (fragment->Is2b()) {
    const PRUnichar *data=fragment->Get2b();
    while(*data && index) {
      if (XP_IS_SPACE_W(*data)){
        do {
          ++data;
          --index;
        }while(XP_IS_SPACE_W(*data) && index);
      }
      else {
        ++data;
        --index;
      }
      ++ci;
    }
  }
  else {
    const char *data=fragment->Get1b();
    while(*data && index) {
      if (XP_IS_SPACE_W(*data)){
        do {
          ++data;
          --index;
        }while(XP_IS_SPACE_W(*data) && index);
      }
      else {
        ++data;
        --index;
      }
      ++ci;
    }
  }
    
  return ci;
}

nsresult
nsSVGGlyphFrame::GetHighlight(PRUint32 *charnum, PRUint32 *nchars,
                              nscolor *foreground, nscolor *background)
{
  *foreground = NS_RGB(255,255,255);
  *background = NS_RGB(0,0,0); 
  *charnum=0;
  *nchars=0;

  bool hasHighlight = IsSelected();
  if (!hasHighlight) {
    NS_ERROR("nsSVGGlyphFrame::GetHighlight() called by renderer when there is no highlight");
    return NS_ERROR_FAILURE;
  }

  nsPresContext *presContext = PresContext();

  // The selection ranges are relative to the uncompressed text in
  // the content element. We'll need the text fragment:
  const nsTextFragment *fragment = mContent->GetText();
  NS_ASSERTION(fragment, "no text");
  
  // get the selection details 
  SelectionDetails *details = nsnull;
  {
    nsRefPtr<nsFrameSelection> frameSelection = presContext->PresShell()->FrameSelection();
    if (!frameSelection) {
      NS_ERROR("no frameselection interface");
      return NS_ERROR_FAILURE;
    }

    details = frameSelection->LookUpSelection(
      mContent, 0, fragment->GetLength(), false
      );
  }

#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  {
    SelectionDetails *dp = details;
    printf("nsSVGGlyphFrame(%p)::GetHighlight() [\n", this);
    while (dp) {
      printf("selection detail: %d(%d)->%d(%d) type %d\n",
             dp->mStart, CompressIndex(dp->mStart, fragment),
             dp->mEnd, CompressIndex(dp->mEnd, fragment),
             dp->mType);
      dp = dp->mNext;
    }
    printf("]\n");
      
  }
#endif
  
  if (details) {
    NS_ASSERTION(details->mNext==nsnull, "can't do multiple selection ranges");

    *charnum=CompressIndex(details->mStart, fragment);
    *nchars=CompressIndex(details->mEnd, fragment)-*charnum;  

    LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectBackground,
                          background);
    LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectForeground,
                          foreground);

    SelectionDetails *dp = details;
    while ((dp=details->mNext) != nsnull) {
      delete details;
      details = dp;
    }
    delete details;
  }
  
  return NS_OK;
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

  PRUint32 strLength = mTextRun->GetLength();

  nsTArray<float> xList, yList;
  GetEffectiveXY(strLength, xList, yList);
  PRUint32 xCount = NS_MIN(xList.Length(), strLength);
  PRUint32 yCount = NS_MIN(yList.Length(), strLength);

  // move aPosition to the last glyph position
  gfxFloat x = aPosition->x;
  if (xCount > 1) {
    x = xList[xCount - 1];
    x +=
      mTextRun->GetAdvanceWidth(xCount - 1, 1, nsnull) * metricsScale;

      // advance to the last glyph
      if (strLength > xCount) {
        x +=
          mTextRun->GetAdvanceWidth(xCount, strLength - xCount, nsnull) *
            metricsScale;
      }
  } else {
    x += mTextRun->GetAdvanceWidth(0, strLength, nsnull) * metricsScale;
  }

  gfxFloat y = (textPath || yCount <= 1) ? aPosition->y : yList[yCount - 1];
  aPosition->MoveTo(x, y);

  gfxFloat pathScale = 1.0;
  if (textPath)
    pathScale = textPath->GetOffsetScale();

  nsTArray<float> dxList, dyList;
  GetEffectiveDxDy(strLength, dxList, dyList);

  PRUint32 dxcount = NS_MIN(dxList.Length(), strLength);
  if (dxcount > 0) {
    mPosition.x += dxList[0] * pathScale;
  }
  for (PRUint32 i = 0; i < dxcount; i++) {
    aPosition->x += dxList[i] * pathScale;
  }
  PRUint32 dycount = NS_MIN(dyList.Length(), strLength);
  if (dycount > 0) {
    mPosition.y += dyList[0]* pathScale;
  }
  for (PRUint32 i = 0; i < dycount; i++) {
    aPosition->y += dyList[i] * pathScale;
  }
}

nsresult
nsSVGGlyphFrame::GetStartPositionOfChar(PRUint32 charnum,
                                        nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  CharacterIterator iter(this, false);
  if (!iter.AdvanceToCharacter(charnum))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  NS_ADDREF(*_retval = new DOMSVGPoint(iter.GetPositionData().pos));
  return NS_OK;
}

nsresult
nsSVGGlyphFrame::GetEndPositionOfChar(PRUint32 charnum,
                                      nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  CharacterIterator iter(this, false);
  if (!iter.AdvanceToCharacter(charnum))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  iter.SetupForMetrics(tmpCtx);
  tmpCtx->MoveTo(gfxPoint(mTextRun->GetAdvanceWidth(charnum, 1, nsnull), 0));
  tmpCtx->IdentityMatrix();
  NS_ADDREF(*_retval = new DOMSVGPoint(tmpCtx->CurrentPoint()));
  return NS_OK;
}

nsresult
nsSVGGlyphFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  CharacterIterator iter(this, false);
  if (!iter.AdvanceToCharacter(0))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  PRUint32 start = charnum, limit = charnum + 1;
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
                          nsnull, nsnull);

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  iter.SetupForMetrics(tmpCtx);
  tmpCtx->Rectangle(gfxRect(0, -metrics.mAscent,
                            metrics.mAdvanceWidth,
                            metrics.mAscent + metrics.mDescent));
  tmpCtx->IdentityMatrix();
  return NS_NewSVGRect(_retval, tmpCtx->GetUserPathExtent());
}

nsresult
nsSVGGlyphFrame::GetRotationOfChar(PRUint32 charnum, float *_retval)
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
       frame != nsnull;
       frame = frame->GetParent()) {
    nsIAtom* type = frame->GetType();
    if (type == nsGkAtoms::svgTextPathFrame) {
      return static_cast<nsSVGTextPathFrame*>(frame);
    } else if (type == nsGkAtoms::svgTextFrame)
      return nsnull;
  }
  return nsnull;
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
nsSVGGlyphFrame::SetStartIndex(PRUint32 aStartIndex)
{
  mStartIndex = aStartIndex;
}

void
nsSVGGlyphFrame::GetEffectiveXY(PRInt32 strLength, nsTArray<float> &aX, nsTArray<float> &aY)
{
  nsTArray<float> x, y;
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetEffectiveXY(x, y);

  PRInt32 xCount = NS_MAX((PRInt32)(x.Length() - mStartIndex), 0);
  xCount = NS_MIN(xCount, strLength);
  aX.AppendElements(x.Elements() + mStartIndex, xCount);

  PRInt32 yCount = NS_MAX((PRInt32)(y.Length() - mStartIndex), 0);
  yCount = NS_MIN(yCount, strLength);
  aY.AppendElements(y.Elements() + mStartIndex, yCount);
}

void
nsSVGGlyphFrame::GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy)
{
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetDxDy(aDx, aDy);
}

void
nsSVGGlyphFrame::GetEffectiveDxDy(PRInt32 strLength, nsTArray<float> &aDx, nsTArray<float> &aDy)
{
  nsTArray<float> dx, dy;
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetEffectiveDxDy(dx, dy);

  PRInt32 dxCount = NS_MAX((PRInt32)(dx.Length() - mStartIndex), 0);
  dxCount = NS_MIN(dxCount, strLength);
  aDx.AppendElements(dx.Elements() + mStartIndex, dxCount);

  PRInt32 dyCount = NS_MAX((PRInt32)(dy.Length() - mStartIndex), 0);
  dyCount = NS_MIN(dyCount, strLength);
  aDy.AppendElements(dy.Elements() + mStartIndex, dyCount);
}

const SVGNumberList*
nsSVGGlyphFrame::GetRotate()
{
  nsSVGTextContainerFrame *containerFrame;
  containerFrame = static_cast<nsSVGTextContainerFrame *>(mParent);
  if (containerFrame)
    return containerFrame->GetRotate();
  return nsnull;
}

void
nsSVGGlyphFrame::GetEffectiveRotate(PRInt32 strLength, nsTArray<float> &aRotate)
{
  nsTArray<float> rotate;
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetEffectiveRotate(rotate);

  PRInt32 rotateCount = NS_MAX((PRInt32)(rotate.Length() - mStartIndex), 0);
  rotateCount = NS_MIN(rotateCount, strLength);
  if (rotateCount > 0) {
    aRotate.AppendElements(rotate.Elements() + mStartIndex, rotateCount);
  } else if (!rotate.IsEmpty()) {
    // rotate is applied for extra characters too
    aRotate.AppendElement(rotate[rotate.Length() - 1]);
  }
}

PRUint16
nsSVGGlyphFrame::GetTextAnchor()
{
  return GetStyleSVG()->mTextAnchor;
}

bool
nsSVGGlyphFrame::IsAbsolutelyPositioned()
{
  bool hasTextPathAncestor = false;
  for (nsIFrame *frame = GetParent();
       frame != nsnull;
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

PRUint32
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
nsSVGGlyphFrame::GetSubStringLength(PRUint32 charnum, PRUint32 fragmentChars)
{
  float drawScale, metricsScale;
  if (!EnsureTextRun(&drawScale, &metricsScale, false))
    return 0.0f;

  return GetSubStringAdvance(charnum, fragmentChars, metricsScale);
}

PRInt32
nsSVGGlyphFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point)
{
  float xPos, yPos;
  point->GetX(&xPos);
  point->GetY(&yPos);

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  CharacterIterator iter(this, false);

  PRUint32 i;
  PRInt32 last = -1;
  gfxPoint pt(xPos, yPos);
  while ((i = iter.NextCluster()) != iter.InvalidCluster()) {
    PRUint32 limit = i + iter.ClusterLength();
    gfxTextRun::Metrics metrics =
      mTextRun->MeasureText(i, limit - i, gfxFont::LOOSE_INK_EXTENTS,
                            nsnull, nsnull);

    // the SVG spec tells us to divide the width of the cluster equally among
    // its chars, so we'll step through the chars, allocating a share of the
    // total advance to each
    PRInt32 current, end, step;
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
  return node ? node->GetNextGlyphFrame() : nsnull;
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
  PRInt32 len = text->GetLength();
  const char* str = text->Get1b();
  for (PRInt32 i = 0; i < len; ++i) {
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
nsSVGGlyphFrame::SetupGlobalTransform(gfxContext *aContext)
{
  gfxMatrix matrix = GetCanvasTM();
  if (!matrix.IsSingular()) {
    aContext->Multiply(matrix);
  }
}

void
nsSVGGlyphFrame::ClearTextRun()
{
  delete mTextRun;
  mTextRun = nsnull;
}

bool
nsSVGGlyphFrame::EnsureTextRun(float *aDrawScale, float *aMetricsScale,
                               bool aForceGlobalTransform)
{
  // Compute the size at which the text should render (excluding the CTM)
  const nsStyleFont* fontData = GetStyleFont();
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
    bool bidiOverride = !!(mParent->GetStyleTextReset()->mUnicodeBidi &
                           NS_STYLE_UNICODE_BIDI_OVERRIDE);
    nsBidiLevel baseDirection =
      GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL ?
        NSBIDI_RTL : NSBIDI_LTR;
    nsBidiPresUtils::CopyLogicalToVisual(text, visualText,
                                         baseDirection, bidiOverride);
    if (!visualText.IsEmpty()) {
      text = visualText;
    }

    gfxMatrix m;
    if (aForceGlobalTransform ||
        !(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
      m = GetCanvasTM();
      if (m.IsSingular())
        return false;
    }

    // The context scale is the ratio of the length of the transformed
    // diagonal vector (1,1) to the length of the untransformed diagonal
    // (which is sqrt(2)).
    gfxPoint p = m.Transform(gfxPoint(1, 1)) - m.Transform(gfxPoint(0, 0));
    double contextScale = nsSVGUtils::ComputeNormalizedHypotenuse(p.x, p.y);

    if (GetStyleSVG()->mTextRendering ==
        NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION) {
      textRunSize = PRECISE_SIZE;
    } else {
      textRunSize = size*contextScale;
      textRunSize = NS_MAX(textRunSize, double(CLAMP_MIN_SIZE));
      textRunSize = NS_MIN(textRunSize, double(CLAMP_MAX_SIZE));
    }

    const nsFont& font = fontData->mFont;
    bool printerFont = (presContext->Type() == nsPresContext::eContext_PrintPreview ||
                          presContext->Type() == nsPresContext::eContext_Print);
    gfxFontStyle fontStyle(font.style, font.weight, font.stretch, textRunSize,
                           mStyleContext->GetStyleFont()->mLanguage,
                           font.sizeAdjust, font.systemFont,
                           printerFont,
                           font.featureSettings,
                           font.languageOverride);

    nsRefPtr<gfxFontGroup> fontGroup =
      gfxPlatform::GetPlatform()->CreateFontGroup(font.name, &fontStyle, presContext->GetUserFontSet());

    PRUint32 flags = gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX |
      GetTextRunFlags(text.Length()) |
      nsLayoutUtils::GetTextRunFlagsForStyle(GetStyleContext(), GetStyleText(), GetStyleFont());

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
        tmpCtx, nsnull, nsnull, nsnull, 0, GetTextRunUnitsFactor()
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
  , mCurrentChar(PRUint32(-1))
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

PRUint32
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
        mSource->mTextRun->GetAdvanceWidth(mCurrentChar, 1, nsnull);
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

PRUint32
CharacterIterator::ClusterLength()
{
  if (mInError) {
    return 0;
  }

  PRUint32 i = mCurrentChar;
  while (++i < mSource->mTextRun->GetLength()) {
    if (mSource->mTextRun->IsClusterStart(i)) {
      break;
    }
  }
  return i - mCurrentChar;
}

bool
CharacterIterator::AdvanceToCharacter(PRUint32 aIndex)
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
