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
#include "nsILookAndFeel.h"
#include "nsTextFragment.h"
#include "nsSVGUtils.h"
#include "SVGLengthList.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGPoint.h"
#include "nsSVGGlyphFrame.h"
#include "nsSVGTextPathFrame.h"
#include "nsSVGPathElement.h"
#include "nsSVGPoint.h"
#include "nsSVGRect.h"
#include "nsDOMError.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxPlatform.h"
#include "gfxTextRunWordCache.h"

using namespace mozilla;

struct CharacterPosition {
  gfxPoint pos;
  gfxFloat angle;
  PRBool draw;
};
  
/**
 * This is a do-it-all helper class. It supports iterating through the
 * drawable characters of a string. For each character, it can set up
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
 * XXX should make this iterate clusters instead
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
   * Sets up the iterator so that NextChar will return the first drawable
   * char.
   * @param aForceGlobalTransform passed on to EnsureTextRun (see below)
   */
  CharacterIterator(nsSVGGlyphFrame *aSource, PRBool aForceGlobalTransform);
  /**
   * This matrix will be applied to aContext in the SetupFor methods below,
   * before any glyph translation/rotation.
   */
  void SetInitialMatrix(gfxContext *aContext) {
    mInitialMatrix = aContext->CurrentMatrix();
    if (mInitialMatrix.IsSingular()) {
      mInError = PR_TRUE;
    }
  }
  /**
   * Try to set up aContext so we can draw the whole textrun at once.
   * This applies any global transform requested by SetInitialMatrix,
   * then applies the positioning of the text. Returns false if drawing
   * the whole textrun at once is impossible due to individual positioning
   * and/or rotation of glyphs.
   */
  PRBool SetupForDirectTextRunDrawing(gfxContext *aContext) {
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
  PRBool SetupForDirectTextRunMetrics(gfxContext *aContext) {
    return SetupForDirectTextRun(aContext, mMetricsScale);
  }

  /**
   * Returns the index of the next char in the string that should be
   * drawn, or -1 if there is no such character.
   */
  PRInt32 NextChar();
  /**
   * Repeated calls NextChar until it returns aIndex (i.e. aIndex is the
   * current drawable character). Returns false if that never happens
   * (because aIndex is before or equal to the current character, or
   * out of bounds, or not drawable).
   */
  PRBool AdvanceToCharacter(PRInt32 aIndex);

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

private:
  PRBool SetupForDirectTextRun(gfxContext *aContext, float aScale);
  void SetupFor(gfxContext *aContext, float aScale);

  nsSVGGlyphFrame *mSource;
  nsAutoTArray<CharacterPosition,80> mPositions;
  gfxMatrix mInitialMatrix;
  // Textrun advance width from start to mCurrentChar, in appunits
  gfxFloat mCurrentAdvance;
  PRInt32 mCurrentChar;
  float mDrawScale;
  float mMetricsScale;
  PRPackedBool mInError;
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
  NS_QUERYFRAME_ENTRY(nsISVGGlyphFragmentLeaf)
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
nsSVGGlyphFrame::SetSelected(PRBool        aSelected,
                             SelectionType aType)
{
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::SetSelected()\n", this);
#endif

  if (aType != nsISelectionController::SELECTION_NORMAL)
    return;

  // check whether style allows selection
  PRBool selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
    return;

  if (aSelected) {
    AddStateBits(NS_FRAME_SELECTED_CONTENT);
  } else {
    RemoveStateBits(NS_FRAME_SELECTED_CONTENT);
  }

  nsSVGUtils::UpdateGraphic(this);
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetSelected(PRBool *aSelected) const
{
  nsresult rv = nsSVGGlyphFrameBase::GetSelected(aSelected);
#if defined(DEBUG) && defined(SVG_DEBUG_SELECTION)
  printf("nsSVGGlyphFrame(%p)::GetSelected()=%d\n", this, *aSelected);
#endif
  return rv;
}

NS_IMETHODIMP
nsSVGGlyphFrame::IsSelectable(PRBool* aIsSelectable,
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
nsSVGGlyphFrame::PaintSVG(nsSVGRenderState *aContext,
                          const nsIntRect *aDirtyRect)
{
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  gfxContext *gfx = aContext->GetGfxContext();
  PRUint16 renderMode = aContext->GetRenderMode();

  switch (GetStyleSVG()->mTextRendering) {
  case NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED:
    gfx->SetAntialiasMode(gfxContext::MODE_ALIASED);
    break;
  default:
    gfx->SetAntialiasMode(gfxContext::MODE_COVERAGE);
    break;
  }

  if (renderMode != nsSVGRenderState::NORMAL) {

    gfxMatrix matrix = gfx->CurrentMatrix();
    SetupGlobalTransform(gfx);

    CharacterIterator iter(this, PR_TRUE);
    iter.SetInitialMatrix(gfx);

    if (GetClipRule() == NS_STYLE_FILL_RULE_EVENODD)
      gfx->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
    else
      gfx->SetFillRule(gfxContext::FILL_RULE_WINDING);

    if (renderMode == nsSVGRenderState::CLIP_MASK) {
      gfx->SetColor(gfxRGBA(1.0f, 1.0f, 1.0f, 1.0f));
      FillCharacters(&iter, gfx);
    } else {
      AddCharactersToPath(&iter, gfx);
    }

    gfx->SetMatrix(matrix);
    return NS_OK;
  }

  // We are adding patterns or gradients to the context. Save
  // it so we don't leak them into the next object we draw
  gfx->Save();
  SetupGlobalTransform(gfx);

  if (SetupCairoFill(gfx)) {
    gfxMatrix matrix = gfx->CurrentMatrix();
    CharacterIterator iter(this, PR_TRUE);
    iter.SetInitialMatrix(gfx);

    FillCharacters(&iter, gfx);
    gfx->SetMatrix(matrix);
  }

  if (SetupCairoStroke(gfx)) {
    // SetupCairoStroke will clear mTextRun whenever
    // there is a pattern or gradient on the text
    CharacterIterator iter(this, PR_TRUE);
    iter.SetInitialMatrix(gfx);

    gfx->NewPath();
    AddCharactersToPath(&iter, gfx);
    gfx->Stroke();
    // We need to clear the context's path so state doesn't leak
    // out. See bug 337753.
    gfx->NewPath();
  }
  gfx->Restore();

  return NS_OK;
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGGlyphFrame::GetFrameForPoint(const nsPoint &aPoint)
{
#ifdef DEBUG
  //printf("nsSVGGlyphFrame(%p)::GetFrameForPoint\n", this);
#endif

  if (!mRect.Contains(aPoint))
    return nsnull;

  PRBool events = PR_FALSE;
  switch (GetStyleVisibility()->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
    case NS_STYLE_POINTER_EVENTS_AUTO:
      if (GetStyleVisibility()->IsVisible() &&
          (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None ||
           GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None))
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      if (GetStyleVisibility()->IsVisible())
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_PAINTED:
      if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None ||
          GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
        events = PR_TRUE;
      break;
    case NS_STYLE_POINTER_EVENTS_FILL:
    case NS_STYLE_POINTER_EVENTS_STROKE:
    case NS_STYLE_POINTER_EVENTS_ALL:
      events = PR_TRUE;
      break;
    default:
      NS_ERROR("not reached");
      break;
  }

  if (events && ContainsPoint(aPoint))
    return this;

  return nsnull;
}

NS_IMETHODIMP_(nsRect)
nsSVGGlyphFrame::GetCoveredRegion()
{
  return mRect;
}

static gfxContext *
MakeTmpCtx() {
  return new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());
}

NS_IMETHODIMP
nsSVGGlyphFrame::UpdateCoveredRegion()
{
  mRect.Empty();

  gfxMatrix matrix = GetCanvasTM();
  if (matrix.IsSingular()) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  tmpCtx->Multiply(matrix);

  PRBool hasStroke = HasStroke();
  if (hasStroke) {
    SetupCairoStrokeGeometry(tmpCtx);
  } else if (GetStyleSVG()->mFill.mType == eStyleSVGPaintType_None) {
    return NS_OK;
  }

  SetMatrixPropagation(PR_FALSE);
  CharacterIterator iter(this, PR_TRUE);
  iter.SetInitialMatrix(tmpCtx);
  AddBoundingBoxesToPath(&iter, tmpCtx); // iter is now unsafe to use! (at end)
  SetMatrixPropagation(PR_TRUE);
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
    extent = nsSVGUtils::PathExtentsToMaxStrokeExtents(extent, this);
  }

  if (!extent.IsEmpty()) {
    mRect = nsSVGUtils::ToAppPixelRect(PresContext(), extent);
  }

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

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyRedrawSuspended()
{
  // XXX should we cache the fact that redraw is suspended?
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGlyphFrame::NotifyRedrawUnsuspended()
{
  if (GetStateBits() & NS_STATE_SVG_DIRTY)
    nsSVGUtils::UpdateGraphic(this);

  return NS_OK;
}

void
nsSVGGlyphFrame::AddCharactersToPath(CharacterIterator *aIter,
                                     gfxContext *aContext)
{
  if (aIter->SetupForDirectTextRunDrawing(aContext)) {
    mTextRun->DrawToPath(aContext, gfxPoint(0, 0), 0,
                         mTextRun->GetLength(), nsnull, nsnull);
    return;
  }

  PRInt32 i;
  while ((i = aIter->NextChar()) >= 0) {
    aIter->SetupForDrawing(aContext);
    mTextRun->DrawToPath(aContext, gfxPoint(0, 0), i, 1, nsnull, nsnull);
  }
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

  PRInt32 i;
  while ((i = aIter->NextChar()) >= 0) {
    aIter->SetupForMetrics(aContext);
    gfxTextRun::Metrics metrics =
      mTextRun->MeasureText(i, 1, gfxFont::LOOSE_INK_EXTENTS, nsnull, nsnull);
    aContext->Rectangle(metrics.mBoundingBox);
  }
}

void
nsSVGGlyphFrame::FillCharacters(CharacterIterator *aIter,
                                gfxContext *aContext)
{
  if (aIter->SetupForDirectTextRunDrawing(aContext)) {
    mTextRun->Draw(aContext, gfxPoint(0, 0), 0,
                   mTextRun->GetLength(), nsnull, nsnull, nsnull);
    return;
  }

  PRInt32 i;
  while ((i = aIter->NextChar()) >= 0) {
    aIter->SetupForDrawing(aContext);
    mTextRun->Draw(aContext, gfxPoint(0, 0), i, 1, nsnull, nsnull, nsnull);
  }
}

gfxRect
nsSVGGlyphFrame::GetBBoxContribution(const gfxMatrix &aToBBoxUserspace)
{
  mOverrideCanvasTM = NS_NewSVGMatrix(aToBBoxUserspace);

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  SetupGlobalTransform(tmpCtx);
  CharacterIterator iter(this, PR_TRUE);
  iter.SetInitialMatrix(tmpCtx);
  AddBoundingBoxesToPath(&iter, tmpCtx);
  tmpCtx->IdentityMatrix();

  mOverrideCanvasTM = nsnull;

  return tmpCtx->GetUserPathExtent();
}

//----------------------------------------------------------------------
// nsSVGGeometryFrame methods:

gfxMatrix
nsSVGGlyphFrame::GetCanvasTM()
{
  if (mOverrideCanvasTM) {
    return nsSVGUtils::ConvertSVGMatrixToThebes(mOverrideCanvasTM);
  }
  NS_ASSERTION(mParent, "null parent");
  return static_cast<nsSVGContainerFrame*>(mParent)->GetCanvasTM();
}

//----------------------------------------------------------------------
// nsSVGGlyphFrame methods:

PRBool
nsSVGGlyphFrame::GetCharacterData(nsAString & aCharacterData)
{
  nsAutoString characterData;
  mContent->AppendTextTo(characterData);

  if (mWhitespaceHandling & COMPRESS_WHITESPACE) {
    PRBool trimLeadingWhitespace, trimTrailingWhitespace;
    trimLeadingWhitespace = ((mWhitespaceHandling & TRIM_LEADING_WHITESPACE) != 0);
    trimTrailingWhitespace = ((mWhitespaceHandling & TRIM_TRAILING_WHITESPACE) != 0);
    characterData.CompressWhitespace(trimLeadingWhitespace, 
                                     trimTrailingWhitespace);
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

PRBool
nsSVGGlyphFrame::GetCharacterPositions(nsTArray<CharacterPosition>* aCharacterPositions,
                                       float aMetricsScale)
{
  PRUint32 strLength = mTextRun->GetLength();
  NS_ASSERTION(strLength > 0, "no text");

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
      return PR_FALSE;

    if (!aCharacterPositions->SetLength(strLength))
      return PR_FALSE;

    gfxFloat pathScale = textPath->GetPathScale();

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
    return PR_TRUE;
  }

  if (xList.Length() <= 1 &&
      yList.Length() <= 1 &&
      dxList.Length() <= 1 &&
      dyList.Length() <= 1 &&
      rotateList.IsEmpty()) {
    // simple text without individual positioning
    return PR_TRUE;
  }

  if (!aCharacterPositions->SetLength(strLength))
    return PR_FALSE;

  CharacterPosition *cp = aCharacterPositions->Elements();

  PRUint16 anchor = GetTextAnchor();

  for (PRUint32 i = 0; i < strLength; i++) {
    cp[i].draw = PR_TRUE;

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
  return PR_TRUE;
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
      pathScale = textPath->GetPathScale();
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

  PRBool hasHighlight =
    (mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;

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
    nsCOMPtr<nsFrameSelection> frameSelection;
    {
      nsCOMPtr<nsISelectionController> controller;
      GetSelectionController(presContext, getter_AddRefs(controller));
      
      if (!controller) {
        NS_ERROR("no selection controller");
        return NS_ERROR_FAILURE;
      }
      frameSelection = do_QueryInterface(controller);
    }
    if (!frameSelection) {
      frameSelection = presContext->PresShell()->FrameSelection();
    }
    if (!frameSelection) {
      NS_ERROR("no frameselection interface");
      return NS_ERROR_FAILURE;
    }

    details = frameSelection->LookUpSelection(
      mContent, 0, fragment->GetLength(), PR_FALSE
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
    
    nsILookAndFeel *look = presContext->LookAndFeel();

    look->GetColor(nsILookAndFeel::eColor_TextSelectBackground, *background);
    look->GetColor(nsILookAndFeel::eColor_TextSelectForeground, *foreground);

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
// nsISVGGlyphFragmentLeaf interface:

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::SetGlyphPosition(gfxPoint *aPosition, PRBool aForceGlobalTransform)
{
  float drawScale, metricsScale;

  nsSVGTextPathFrame *textPath = FindTextPathParent();
  // In a textPath, the 'y' attribute has no effect, so we reset 'y' here
  // to use aPosition.y for dy only
  if (textPath && textPath->GetFirstChild(nsnull) == this) {
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
  aPosition->MoveTo(x, y - GetBaselineOffset(metricsScale));

  gfxFloat pathScale = 1.0;
  if (textPath)
    pathScale = textPath->GetPathScale();

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

NS_IMETHODIMP
nsSVGGlyphFrame::GetStartPositionOfChar(PRUint32 charnum,
                                        nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  CharacterIterator iter(this, PR_FALSE);
  if (!iter.AdvanceToCharacter(charnum))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  return NS_NewSVGPoint(_retval, iter.GetPositionData().pos);
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetEndPositionOfChar(PRUint32 charnum,
                                      nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  CharacterIterator iter(this, PR_FALSE);
  if (!iter.AdvanceToCharacter(charnum))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  iter.SetupForMetrics(tmpCtx);
  tmpCtx->MoveTo(gfxPoint(mTextRun->GetAdvanceWidth(charnum, 1, nsnull), 0));
  tmpCtx->IdentityMatrix();
  return NS_NewSVGPoint(_retval, tmpCtx->CurrentPoint());
}

NS_IMETHODIMP
nsSVGGlyphFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  CharacterIterator iter(this, PR_FALSE);
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

NS_IMETHODIMP
nsSVGGlyphFrame::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  CharacterIterator iter(this, PR_FALSE);
  if (!iter.AdvanceToCharacter(charnum))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  CharacterPosition pos = iter.GetPositionData();
  if (!pos.draw)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  const gfxFloat radPerDeg = M_PI/180.0;
  *_retval = float(pos.angle / radPerDeg);
  return NS_OK;
}

NS_IMETHODIMP_(float)
nsSVGGlyphFrame::GetAdvance(PRBool aForceGlobalTransform)
{
  float drawScale, metricsScale;
  if (!EnsureTextRun(&drawScale, &metricsScale, aForceGlobalTransform))
    return 0.0f;

  return GetSubStringAdvance(0, mTextRun->GetLength(), metricsScale);
}

NS_IMETHODIMP_(nsSVGTextPathFrame*) 
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

NS_IMETHODIMP_(PRBool)
nsSVGGlyphFrame::IsStartOfChunk()
{
  // this fragment is a chunk if it has a corresponding absolute
  // position adjustment in an ancestors' x or y array. (At the moment
  // we don't map the full arrays, but only the first elements.)

  return PR_FALSE;
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::GetXY(SVGUserUnitList *aX, SVGUserUnitList *aY)
{
  static_cast<nsSVGTextContainerFrame *>(mParent)->GetXY(aX, aY);
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::SetStartIndex(PRUint32 aStartIndex)
{
  mStartIndex = aStartIndex;
}

NS_IMETHODIMP_(void)
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

already_AddRefed<nsIDOMSVGNumberList>
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

NS_IMETHODIMP_(PRUint16)
nsSVGGlyphFrame::GetTextAnchor()
{
  return GetStyleSVG()->mTextAnchor;
}

NS_IMETHODIMP_(PRBool)
nsSVGGlyphFrame::IsAbsolutelyPositioned()
{
  PRBool hasTextPathAncestor = PR_FALSE;
  for (nsIFrame *frame = GetParent();
       frame != nsnull;
       frame = frame->GetParent()) {

    if (!frame)
      break;

    // at the start of a 'text' element
    // at the start of each 'textPath' element
    if (frame->GetType() == nsGkAtoms::svgTextPathFrame) {
      hasTextPathAncestor = PR_TRUE;
    }
    if ((frame->GetType() == nsGkAtoms::svgTextFrame ||
         frame->GetType() == nsGkAtoms::svgTextPathFrame) &&
        frame->GetFirstChild(nsnull) == this) {
        return PR_TRUE;
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
  if (mWhitespaceHandling == PRESERVE_WHITESPACE)
    return mContent->TextLength();

  nsAutoString text;
  GetCharacterData(text);
  return text.Length();
}

float
nsSVGGlyphFrame::GetComputedTextLength()
{
  return GetAdvance(PR_FALSE);
}

float
nsSVGGlyphFrame::GetSubStringLength(PRUint32 charnum, PRUint32 fragmentChars)
{
  float drawScale, metricsScale;
  if (!EnsureTextRun(&drawScale, &metricsScale, PR_FALSE))
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
  CharacterIterator iter(this, PR_FALSE);

  PRInt32 i;
  PRInt32 last = -1;
  gfxPoint pt(xPos, yPos);
  while ((i = iter.NextChar()) >= 0) {
    // iter is the beginning of a cluster (or of the entire run);
    // look ahead for the next cluster start, then measure the entire cluster
    PRInt32 limit = i + 1;
    while (limit < (PRInt32)mTextRun->GetLength() &&
           !mTextRun->IsClusterStart(limit)) {
      ++limit;
    }
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

    // move iter past any trailing chars of the cluster
    while (++i < limit) {
      iter.NextChar();
    }
  }

  return last;
}

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGGlyphFrame::GetFirstGlyphFragment()
{
  return this;
}

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGGlyphFrame::GetNextGlyphFragment()
{
  nsIFrame* sibling = GetNextSibling();
  while (sibling) {
    nsISVGGlyphFragmentNode *node = do_QueryFrame(sibling);
    if (node)
      return node->GetFirstGlyphFragment();
    sibling = sibling->GetNextSibling();
  }

  // no more siblings. go back up the tree.
  
  NS_ASSERTION(GetParent(), "null parent");
  nsISVGGlyphFragmentNode *node = do_QueryFrame(GetParent());
  return node ? node->GetNextGlyphFragment() : nsnull;
}

NS_IMETHODIMP_(void)
nsSVGGlyphFrame::SetWhitespaceHandling(PRUint8 aWhitespaceHandling)
{
  mWhitespaceHandling = aWhitespaceHandling;
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

PRBool
nsSVGGlyphFrame::ContainsPoint(const nsPoint &aPoint)
{
  nsRefPtr<gfxContext> tmpCtx = MakeTmpCtx();
  SetupGlobalTransform(tmpCtx);
  CharacterIterator iter(this, PR_TRUE);
  iter.SetInitialMatrix(tmpCtx);
  
  PRInt32 i;
  while ((i = iter.NextChar()) >= 0) {
    gfxTextRun::Metrics metrics =
      mTextRun->MeasureText(i, 1, gfxFont::LOOSE_INK_EXTENTS, nsnull, nsnull);
    iter.SetupForMetrics(tmpCtx);
    tmpCtx->Rectangle(metrics.mBoundingBox);
  }

  tmpCtx->IdentityMatrix();
  return tmpCtx->PointInFill(gfxPoint(PresContext()->AppUnitsToGfxUnits(aPoint.x),
                                      PresContext()->AppUnitsToGfxUnits(aPoint.y)));
}

PRBool
nsSVGGlyphFrame::GetGlobalTransform(gfxMatrix *aMatrix)
{
  if (!GetMatrixPropagation()) {
    aMatrix->Reset();
    return PR_TRUE;
  }

  *aMatrix = GetCanvasTM();
  return !aMatrix->IsSingular();
}

void
nsSVGGlyphFrame::SetupGlobalTransform(gfxContext *aContext)
{
  gfxMatrix matrix;
  GetGlobalTransform(&matrix);
  if (!matrix.IsSingular()) {
    aContext->Multiply(matrix);
  }
}

void
nsSVGGlyphFrame::ClearTextRun()
{
  if (!mTextRun)
    return;
  gfxTextRunWordCache::RemoveTextRun(mTextRun);
  delete mTextRun;
  mTextRun = nsnull;
}

PRBool
nsSVGGlyphFrame::EnsureTextRun(float *aDrawScale, float *aMetricsScale,
                               PRBool aForceGlobalTransform)
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
      return PR_FALSE;

    gfxMatrix m;
    if (aForceGlobalTransform ||
        !(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
      if (!GetGlobalTransform(&m))
        return PR_FALSE;
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
    PRBool printerFont = (presContext->Type() == nsPresContext::eContext_PrintPreview ||
                          presContext->Type() == nsPresContext::eContext_Print);
    gfxFontStyle fontStyle(font.style, font.weight, font.stretch, textRunSize,
                           mStyleContext->GetStyleVisibility()->mLanguage,
                           font.sizeAdjust, font.systemFont,
                           font.familyNameQuirks,
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

    // Use only the word cache here. We don't want to cache the textrun
    // globally because we'll never hit in that cache, since we create
    // a new fontgroup every time. Even if we cached fontgroups, we
    // might render at very many different sizes (e.g. during zoom
    // animation) and caching a textrun for each such size would be bad.
    gfxTextRunFactory::Parameters params = {
        tmpCtx, nsnull, nsnull, nsnull, 0, GetTextRunUnitsFactor()
    };
    mTextRun = gfxTextRunWordCache::MakeTextRun(text.get(), text.Length(),
      fontGroup, &params, flags);
    if (!mTextRun)
      return PR_FALSE;
  }

  *aDrawScale = float(size/textRunSize);
  *aMetricsScale = (*aDrawScale)/GetTextRunUnitsFactor();
  return PR_TRUE;
}

NS_IMETHODIMP
nsSVGGlyphFrame::SetMatrixPropagation(PRBool aPropagate)
{
  if (aPropagate) {
    AddStateBits(NS_STATE_SVG_PROPAGATE_TRANSFORM);
  } else {
    RemoveStateBits(NS_STATE_SVG_PROPAGATE_TRANSFORM);
  }
  return NS_OK;
}

PRBool
nsSVGGlyphFrame::GetMatrixPropagation()
{
  return (GetStateBits() & NS_STATE_SVG_PROPAGATE_TRANSFORM) != 0;
}

//----------------------------------------------------------------------
// helper class

CharacterIterator::CharacterIterator(nsSVGGlyphFrame *aSource,
        PRBool aForceGlobalTransform)
  : mSource(aSource), mCurrentAdvance(0), mCurrentChar(-1),
    mInError(PR_FALSE)
{
  if (!aSource->EnsureTextRun(&mDrawScale, &mMetricsScale,
                              aForceGlobalTransform) ||
      !aSource->GetCharacterPositions(&mPositions, mMetricsScale)) {
    mInError = PR_TRUE;
  }
}

PRBool
CharacterIterator::SetupForDirectTextRun(gfxContext *aContext, float aScale)
{
  if (!mPositions.IsEmpty() || mInError)
    return PR_FALSE;
  aContext->SetMatrix(mInitialMatrix);
  aContext->Translate(mSource->mPosition);
  aContext->Scale(aScale, aScale);
  // We are scaling the glyphs up/down to the size we want so we need to
  // inverse scale the outline widths of those glyphs so they are invariant
  aContext->SetLineWidth(aContext->CurrentLineWidth() / aScale);
  return PR_TRUE;
}

PRInt32
CharacterIterator::NextChar()
{
  if (mInError)
    return -1;

  while (PR_TRUE) {
    if (mCurrentChar >= 0 &&
        (mPositions.IsEmpty() || mPositions[mCurrentChar].draw)) {
      mCurrentAdvance +=
        mSource->mTextRun->GetAdvanceWidth(mCurrentChar, 1, nsnull);
    }
    ++mCurrentChar;

    if (mCurrentChar >= PRInt32(mSource->mTextRun->GetLength()))
      return -1;

    if (mPositions.IsEmpty() || mPositions[mCurrentChar].draw)
      return mCurrentChar;
  }
}

PRBool
CharacterIterator::AdvanceToCharacter(PRInt32 aIndex)
{
  while (NextChar() != -1) {
    if (mCurrentChar == aIndex)
      return PR_TRUE;
  }
  return PR_FALSE;
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
  // We are scaling the glyphs up/down to the size we want so we need to
  // inverse scale the outline widths of those glyphs so they are invariant
  aContext->SetLineWidth(aContext->CurrentLineWidth() / aScale);
}

CharacterPosition
CharacterIterator::GetPositionData()
{
  if (!mPositions.IsEmpty())
    return mPositions[mCurrentChar];

  gfxFloat advance = mCurrentAdvance * mMetricsScale;
  CharacterPosition cp =
    { mSource->mPosition + gfxPoint(advance, 0), 0, PR_TRUE };
  return cp;
}
