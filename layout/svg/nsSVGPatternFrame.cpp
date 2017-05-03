/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGPatternFrame.h"

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsSVGDisplayableFrame.h"
#include "nsStyleContext.h"
#include "nsSVGEffects.h"
#include "SVGGeometryFrame.h"
#include "mozilla/dom/SVGPatternElement.h"
#include "nsSVGUtils.h"
#include "nsSVGAnimatedTransformList.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

//----------------------------------------------------------------------
// Implementation

nsSVGPatternFrame::nsSVGPatternFrame(nsStyleContext* aContext)
  : nsSVGPaintServerFrame(aContext, LayoutFrameType::SVGPattern)
  , mLoopFlag(false)
  , mNoHRefURI(false)
{
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGPatternFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

nsresult
nsSVGPatternFrame::AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::patternUnits ||
       aAttribute == nsGkAtoms::patternContentUnits ||
       aAttribute == nsGkAtoms::patternTransform ||
       aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::preserveAspectRatio ||
       aAttribute == nsGkAtoms::viewBox)) {
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }

  if ((aNameSpaceID == kNameSpaceID_XLink ||
       aNameSpaceID == kNameSpaceID_None) &&
      aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    Properties().Delete(nsSVGEffects::HrefAsPaintingProperty());
    mNoHRefURI = false;
    // And update whoever references us
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }

  return nsSVGPaintServerFrame::AttributeChanged(aNameSpaceID,
                                                 aAttribute, aModType);
}

#ifdef DEBUG
void
nsSVGPatternFrame::Init(nsIContent*       aContent,
                        nsContainerFrame* aParent,
                        nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::pattern), "Content is not an SVG pattern");

  nsSVGPaintServerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

// If our GetCanvasTM is getting called, we
// need to return *our current* transformation
// matrix, which depends on our units parameters
// and X, Y, Width, and Height
gfxMatrix
nsSVGPatternFrame::GetCanvasTM()
{
  if (mCTM) {
    return *mCTM;
  }

  // Do we know our rendering parent?
  if (mSource) {
    // Yes, use it!
    return mSource->GetCanvasTM();
  }

  // We get here when geometry in the <pattern> container is updated
  return gfxMatrix();
}

// -------------------------------------------------------------------------
// Helper functions
// -------------------------------------------------------------------------

/** Calculate the maximum expansion of a matrix */
static float
MaxExpansion(const Matrix &aMatrix)
{
  // maximum expansion derivation from
  // http://lists.cairographics.org/archives/cairo/2004-October/001980.html
  // and also implemented in cairo_matrix_transformed_circle_major_axis
  double a = aMatrix._11;
  double b = aMatrix._12;
  double c = aMatrix._21;
  double d = aMatrix._22;
  double f = (a * a + b * b + c * c + d * d) / 2;
  double g = (a * a + b * b - c * c - d * d) / 2;
  double h = a * c + b * d;
  return sqrt(f + sqrt(g * g + h * h));
}

// The SVG specification says that the 'patternContentUnits' attribute "has no effect if
// attribute ‘viewBox’ is specified". We still need to include a bbox scale
// if the viewBox is specified and _patternUnits_ is set to or defaults to
// objectBoundingBox though, since in that case the viewBox is relative to the bbox
static bool
IncludeBBoxScale(const nsSVGViewBox& aViewBox,
                 uint32_t aPatternContentUnits, uint32_t aPatternUnits)
{
  return (!aViewBox.IsExplicitlySet() &&
          aPatternContentUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) ||
         (aViewBox.IsExplicitlySet() &&
          aPatternUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
}

// Given the matrix for the pattern element's own transform, this returns a
// combined matrix including the transforms applicable to its target.
static Matrix
GetPatternMatrix(uint16_t aPatternUnits,
                 const Matrix &patternTransform,
                 const gfxRect &bbox,
                 const gfxRect &callerBBox,
                 const Matrix &callerCTM)
{
  // We really want the pattern matrix to handle translations
  gfxFloat minx = bbox.X();
  gfxFloat miny = bbox.Y();

  if (aPatternUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    minx += callerBBox.X();
    miny += callerBBox.Y();
  }

  float scale = 1.0f / MaxExpansion(callerCTM);
  Matrix patternMatrix = patternTransform;
  patternMatrix.PreScale(scale, scale);
  patternMatrix.PreTranslate(minx, miny);

  return patternMatrix;
}

static nsresult
GetTargetGeometry(gfxRect *aBBox,
                  const nsSVGViewBox &aViewBox,
                  uint16_t aPatternContentUnits,
                  uint16_t aPatternUnits,
                  nsIFrame *aTarget,
                  const Matrix &aContextMatrix,
                  const gfxRect *aOverrideBounds)
{
  *aBBox =
    aOverrideBounds
      ? *aOverrideBounds
      : nsSVGUtils::GetBBox(aTarget, nsSVGUtils::eUseFrameBoundsForOuterSVG |
                                     nsSVGUtils::eBBoxIncludeFillGeometry);

  // Sanity check
  if (IncludeBBoxScale(aViewBox, aPatternContentUnits, aPatternUnits) &&
      (aBBox->Width() <= 0 || aBBox->Height() <= 0)) {
    return NS_ERROR_FAILURE;
  }

  // OK, now fix up the bounding box to reflect user coordinates
  // We handle device unit scaling in pattern matrix
  float scale = MaxExpansion(aContextMatrix);
  if (scale <= 0) {
    return NS_ERROR_FAILURE;
  }
  aBBox->Scale(scale);
  return NS_OK;
}

mozilla::Pair<DrawResult, RefPtr<SourceSurface>>
nsSVGPatternFrame::PaintPattern(const DrawTarget* aDrawTarget,
                                Matrix* patternMatrix,
                                const Matrix &aContextMatrix,
                                nsIFrame *aSource,
                                nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                float aGraphicOpacity,
                                const gfxRect *aOverrideBounds,
                                uint32_t aFlags)
{
  /*
   * General approach:
   *    Set the content geometry stuff
   *    Calculate our bbox (using x,y,width,height & patternUnits &
   *                        patternTransform)
   *    Create the surface
   *    Calculate the content transformation matrix
   *    Get our children (we may need to get them from another Pattern)
   *    Call SVGPaint on all of our children
   *    Return
   */

  nsSVGPatternFrame* patternWithChildren = GetPatternWithChildren();
  if (!patternWithChildren) {
    // Either no kids or a bad reference
    return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
  }
  nsIFrame* firstKid = patternWithChildren->mFrames.FirstChild();

  const nsSVGViewBox& viewBox = GetViewBox();

  uint16_t patternContentUnits =
    GetEnumValue(SVGPatternElement::PATTERNCONTENTUNITS);
  uint16_t patternUnits =
    GetEnumValue(SVGPatternElement::PATTERNUNITS);

  /*
   * Get the content geometry information.  This is a little tricky --
   * our parent is probably a <defs>, but we are rendering in the context
   * of some geometry source.  Our content geometry information needs to
   * come from our rendering parent as opposed to our content parent.  We
   * get that information from aSource, which is passed to us from the
   * backend renderer.
   *
   * There are three "geometries" that we need:
   *   1) The bounding box for the pattern.  We use this to get the
   *      width and height for the surface, and as the return to
   *      GetBBox.
   *   2) The transformation matrix for the pattern.  This is not *quite*
   *      the same as the canvas transformation matrix that we will
   *      provide to our rendering children since we "fudge" it a little
   *      to get the renderer to handle the translations correctly for us.
   *   3) The CTM that we return to our children who make up the pattern.
   */

  // Get all of the information we need from our "caller" -- i.e.
  // the geometry that is being rendered with a pattern
  gfxRect callerBBox;
  if (NS_FAILED(GetTargetGeometry(&callerBBox,
                                  viewBox,
                                  patternContentUnits, patternUnits,
                                  aSource,
                                  aContextMatrix,
                                  aOverrideBounds))) {
    return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
  }

  // Construct the CTM that we will provide to our children when we
  // render them into the tile.
  gfxMatrix ctm = ConstructCTM(viewBox, patternContentUnits, patternUnits,
                               callerBBox, aContextMatrix, aSource);
  if (ctm.IsSingular()) {
    return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
  }

  if (patternWithChildren->mCTM) {
    *patternWithChildren->mCTM = ctm;
  } else {
    patternWithChildren->mCTM = new gfxMatrix(ctm);
  }

  // Get the bounding box of the pattern.  This will be used to determine
  // the size of the surface, and will also be used to define the bounding
  // box for the pattern tile.
  gfxRect bbox = GetPatternRect(patternUnits, callerBBox, aContextMatrix, aSource);
  if (bbox.Width() <= 0.0 || bbox.Height() <= 0.0) {
    return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
  }

  // Get the pattern transform
  Matrix patternTransform = ToMatrix(GetPatternTransform());

  // revert the vector effect transform so that the pattern appears unchanged
  if (aFillOrStroke == &nsStyleSVG::mStroke) {
    gfxMatrix userToOuterSVG;
    if (nsSVGUtils::GetNonScalingStrokeTransform(aSource, &userToOuterSVG)) {
      patternTransform *= ToMatrix(userToOuterSVG);
      if (patternTransform.IsSingular()) {
        NS_WARNING("Singular matrix painting non-scaling-stroke");
        return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
      }
    }
  }

  // Get the transformation matrix that we will hand to the renderer's pattern
  // routine.
  *patternMatrix = GetPatternMatrix(patternUnits, patternTransform,
                                    bbox, callerBBox, aContextMatrix);
  if (patternMatrix->IsSingular()) {
    return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
  }

  // Now that we have all of the necessary geometries, we can
  // create our surface.
  gfxRect transformedBBox = ThebesRect(patternTransform.TransformBounds(ToRect(bbox)));

  bool resultOverflows;
  IntSize surfaceSize =
    nsSVGUtils::ConvertToSurfaceSize(
      transformedBBox.Size(), &resultOverflows);

  // 0 disables rendering, < 0 is an error
  if (surfaceSize.width <= 0 || surfaceSize.height <= 0) {
    return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
  }

  gfxFloat patternWidth = bbox.Width();
  gfxFloat patternHeight = bbox.Height();

  if (resultOverflows ||
      patternWidth != surfaceSize.width ||
      patternHeight != surfaceSize.height) {
    // scale drawing to pattern surface size
    gfxMatrix tempTM =
      gfxMatrix(surfaceSize.width / patternWidth, 0.0,
                0.0, surfaceSize.height / patternHeight,
                0.0, 0.0);
    patternWithChildren->mCTM->PreMultiply(tempTM);

    // and rescale pattern to compensate
    patternMatrix->PreScale(patternWidth / surfaceSize.width,
                            patternHeight / surfaceSize.height);
  }

  RefPtr<DrawTarget> dt =
    aDrawTarget->CreateSimilarDrawTarget(surfaceSize, SurfaceFormat::B8G8R8A8);
  if (!dt || !dt->IsValid()) {
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }
  dt->ClearRect(Rect(0, 0, surfaceSize.width, surfaceSize.height));

  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(dt);
  MOZ_ASSERT(ctx); // already checked the draw target above

  if (aGraphicOpacity != 1.0f) {
    ctx->Save();
    ctx->PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, aGraphicOpacity);
  }

  // OK, now render -- note that we use "firstKid", which
  // we got at the beginning because it takes care of the
  // referenced pattern situation for us

  if (aSource->IsFrameOfType(nsIFrame::eSVGGeometry)) {
    // Set the geometrical parent of the pattern we are rendering
    patternWithChildren->mSource = static_cast<SVGGeometryFrame*>(aSource);
  }

  // Delay checking NS_FRAME_DRAWING_AS_PAINTSERVER bit until here so we can
  // give back a clear surface if there's a loop
  DrawResult result = DrawResult::SUCCESS;
  if (!(patternWithChildren->GetStateBits() & NS_FRAME_DRAWING_AS_PAINTSERVER)) {
    AutoSetRestorePaintServerState paintServer(patternWithChildren);
    for (nsIFrame* kid = firstKid; kid;
         kid = kid->GetNextSibling()) {
      // The CTM of each frame referencing us can be different
      nsSVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
      if (SVGFrame) {
        SVGFrame->NotifySVGChanged(nsSVGDisplayableFrame::TRANSFORM_CHANGED);
      }
      gfxMatrix tm = *(patternWithChildren->mCTM);
      if (kid->GetContent()->IsSVGElement()) {
        tm = static_cast<nsSVGElement*>(kid->GetContent())->
               PrependLocalTransformsTo(tm, eUserSpaceToParent);
      }

      result &= nsSVGUtils::PaintFrameWithEffects(kid, *ctx, tm, nullptr,
                                                  aFlags);
    }
  }

  patternWithChildren->mSource = nullptr;

  if (aGraphicOpacity != 1.0f) {
    ctx->PopGroupAndBlend();
    ctx->Restore();
  }

  // caller now owns the surface
  RefPtr<SourceSurface> surf = dt->Snapshot();
  return MakePair(result, Move(surf));
}

/* Will probably need something like this... */
// How do we handle the insertion of a new frame?
// We really don't want to rerender this every time,
// do we?
nsSVGPatternFrame*
nsSVGPatternFrame::GetPatternWithChildren()
{
  // Do we have any children ourselves?
  if (!mFrames.IsEmpty())
    return this;

  // No, see if we chain to someone who does

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return nullptr;
  }

  nsSVGPatternFrame* next = GetReferencedPattern();
  if (!next)
    return nullptr;

  return next->GetPatternWithChildren();
}

uint16_t
nsSVGPatternFrame::GetEnumValue(uint32_t aIndex, nsIContent *aDefault)
{
  nsSVGEnum& thisEnum =
    static_cast<SVGPatternElement *>(mContent)->mEnumAttributes[aIndex];

  if (thisEnum.IsExplicitlySet())
    return thisEnum.GetAnimValue();

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGPatternElement *>(aDefault)->
             mEnumAttributes[aIndex].GetAnimValue();
  }

  nsSVGPatternFrame *next = GetReferencedPattern();
  return next ? next->GetEnumValue(aIndex, aDefault)
              : static_cast<SVGPatternElement*>(aDefault)->
                  mEnumAttributes[aIndex].GetAnimValue();
}

nsSVGAnimatedTransformList*
nsSVGPatternFrame::GetPatternTransformList(nsIContent* aDefault)
{
  nsSVGAnimatedTransformList *thisTransformList =
    static_cast<SVGPatternElement *>(mContent)->GetAnimatedTransformList();

  if (thisTransformList && thisTransformList->IsExplicitlySet())
    return thisTransformList;

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGPatternElement*>(aDefault)->mPatternTransform.get();
  }

  nsSVGPatternFrame *next = GetReferencedPattern();
  return next ? next->GetPatternTransformList(aDefault)
              : static_cast<SVGPatternElement*>(aDefault)->mPatternTransform.get();
}

gfxMatrix
nsSVGPatternFrame::GetPatternTransform()
{
  nsSVGAnimatedTransformList* animTransformList =
    GetPatternTransformList(mContent);
  if (!animTransformList)
    return gfxMatrix();

  return animTransformList->GetAnimValue().GetConsolidationMatrix();
}

const nsSVGViewBox &
nsSVGPatternFrame::GetViewBox(nsIContent* aDefault)
{
  const nsSVGViewBox &thisViewBox =
    static_cast<SVGPatternElement *>(mContent)->mViewBox;

  if (thisViewBox.IsExplicitlySet())
    return thisViewBox;

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGPatternElement *>(aDefault)->mViewBox;
  }

  nsSVGPatternFrame *next = GetReferencedPattern();
  return next ? next->GetViewBox(aDefault)
              : static_cast<SVGPatternElement *>(aDefault)->mViewBox;
}

const SVGAnimatedPreserveAspectRatio &
nsSVGPatternFrame::GetPreserveAspectRatio(nsIContent *aDefault)
{
  const SVGAnimatedPreserveAspectRatio &thisPar =
    static_cast<SVGPatternElement *>(mContent)->mPreserveAspectRatio;

  if (thisPar.IsExplicitlySet())
    return thisPar;

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGPatternElement *>(aDefault)->mPreserveAspectRatio;
  }

  nsSVGPatternFrame *next = GetReferencedPattern();
  return next ? next->GetPreserveAspectRatio(aDefault)
              : static_cast<SVGPatternElement *>(aDefault)->mPreserveAspectRatio;
}

const nsSVGLength2 *
nsSVGPatternFrame::GetLengthValue(uint32_t aIndex, nsIContent *aDefault)
{
  const nsSVGLength2 *thisLength =
    &static_cast<SVGPatternElement *>(mContent)->mLengthAttributes[aIndex];

  if (thisLength->IsExplicitlySet())
    return thisLength;

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return &static_cast<SVGPatternElement *>(aDefault)->mLengthAttributes[aIndex];
  }

  nsSVGPatternFrame *next = GetReferencedPattern();
  return next ? next->GetLengthValue(aIndex, aDefault)
              : &static_cast<SVGPatternElement *>(aDefault)->mLengthAttributes[aIndex];
}

// Private (helper) methods
nsSVGPatternFrame *
nsSVGPatternFrame::GetReferencedPattern()
{
  if (mNoHRefURI)
    return nullptr;

  nsSVGPaintingProperty *property =
    Properties().Get(nsSVGEffects::HrefAsPaintingProperty());

  if (!property) {
    // Fetch our pattern element's href or xlink:href attribute
    SVGPatternElement *pattern = static_cast<SVGPatternElement *>(mContent);
    nsAutoString href;
    if (pattern->mStringAttributes[SVGPatternElement::HREF].IsExplicitlySet()) {
      pattern->mStringAttributes[SVGPatternElement::HREF]
        .GetAnimValue(href, pattern);
    } else {
      pattern->mStringAttributes[SVGPatternElement::XLINK_HREF]
        .GetAnimValue(href, pattern);
    }

    if (href.IsEmpty()) {
      mNoHRefURI = true;
      return nullptr; // no URL
    }

    // Convert href to an nsIURI
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetUncomposedDoc(), base);

    property =
      nsSVGEffects::GetPaintingProperty(targetURI, this,
                                        nsSVGEffects::HrefAsPaintingProperty());
    if (!property)
      return nullptr;
  }

  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nullptr;

  LayoutFrameType frameType = result->Type();
  if (frameType != LayoutFrameType::SVGPattern)
    return nullptr;

  return static_cast<nsSVGPatternFrame*>(result);
}

gfxRect
nsSVGPatternFrame::GetPatternRect(uint16_t aPatternUnits,
                                  const gfxRect &aTargetBBox,
                                  const Matrix &aTargetCTM,
                                  nsIFrame *aTarget)
{
  // We need to initialize our box
  float x,y,width,height;

  // Get the pattern x,y,width, and height
  const nsSVGLength2 *tmpX, *tmpY, *tmpHeight, *tmpWidth;
  tmpX = GetLengthValue(SVGPatternElement::ATTR_X);
  tmpY = GetLengthValue(SVGPatternElement::ATTR_Y);
  tmpHeight = GetLengthValue(SVGPatternElement::ATTR_HEIGHT);
  tmpWidth = GetLengthValue(SVGPatternElement::ATTR_WIDTH);

  if (aPatternUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    x = nsSVGUtils::ObjectSpace(aTargetBBox, tmpX);
    y = nsSVGUtils::ObjectSpace(aTargetBBox, tmpY);
    width = nsSVGUtils::ObjectSpace(aTargetBBox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(aTargetBBox, tmpHeight);
  } else {
    float scale = MaxExpansion(aTargetCTM);
    x = nsSVGUtils::UserSpace(aTarget, tmpX) * scale;
    y = nsSVGUtils::UserSpace(aTarget, tmpY) * scale;
    width = nsSVGUtils::UserSpace(aTarget, tmpWidth) * scale;
    height = nsSVGUtils::UserSpace(aTarget, tmpHeight) * scale;
  }

  return gfxRect(x, y, width, height);
}

gfxMatrix
nsSVGPatternFrame::ConstructCTM(const nsSVGViewBox& aViewBox,
                                uint16_t aPatternContentUnits,
                                uint16_t aPatternUnits,
                                const gfxRect &callerBBox,
                                const Matrix &callerCTM,
                                nsIFrame *aTarget)
{
  SVGSVGElement *ctx = nullptr;
  nsIContent* targetContent = aTarget->GetContent();
  gfxFloat scaleX, scaleY;

  // The objectBoundingBox conversion must be handled in the CTM:
  if (IncludeBBoxScale(aViewBox, aPatternContentUnits, aPatternUnits)) {
    scaleX = callerBBox.Width();
    scaleY = callerBBox.Height();
  } else {
    if (targetContent->IsSVGElement()) {
      ctx = static_cast<nsSVGElement*>(targetContent)->GetCtx();
    }
    scaleX = scaleY = MaxExpansion(callerCTM);
  }

  if (!aViewBox.IsExplicitlySet()) {
    return gfxMatrix(scaleX, 0.0, 0.0, scaleY, 0.0, 0.0);
  }
  const nsSVGViewBoxRect viewBoxRect = aViewBox.GetAnimValue();

  if (viewBoxRect.height <= 0.0f || viewBoxRect.width <= 0.0f) {
    return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }

  float viewportWidth, viewportHeight;
  if (targetContent->IsSVGElement()) {
    // If we're dealing with an SVG target only retrieve the context once.
    // Calling the nsIFrame* variant of GetAnimValue would look it up on
    // every call.
    viewportWidth =
      GetLengthValue(SVGPatternElement::ATTR_WIDTH)->GetAnimValue(ctx);
    viewportHeight =
      GetLengthValue(SVGPatternElement::ATTR_HEIGHT)->GetAnimValue(ctx);
  } else {
    // No SVG target, call the nsIFrame* variant of GetAnimValue.
    viewportWidth =
      GetLengthValue(SVGPatternElement::ATTR_WIDTH)->GetAnimValue(aTarget);
    viewportHeight =
      GetLengthValue(SVGPatternElement::ATTR_HEIGHT)->GetAnimValue(aTarget);
  }

  if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
    return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }

  Matrix tm = SVGContentUtils::GetViewBoxTransform(
    viewportWidth * scaleX, viewportHeight * scaleY,
    viewBoxRect.x, viewBoxRect.y,
    viewBoxRect.width, viewBoxRect.height,
    GetPreserveAspectRatio());

  return ThebesMatrix(tm);
}

//----------------------------------------------------------------------
// nsSVGPaintServerFrame methods:
mozilla::Pair<DrawResult, RefPtr<gfxPattern>>
nsSVGPatternFrame::GetPaintServerPattern(nsIFrame *aSource,
                                         const DrawTarget* aDrawTarget,
                                         const gfxMatrix& aContextMatrix,
                                         nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                         float aGraphicOpacity,
                                         const gfxRect *aOverrideBounds,
                                         uint32_t aFlags)
{
  if (aGraphicOpacity == 0.0f) {
    RefPtr<gfxPattern> pattern = new gfxPattern(Color());
    return MakePair(DrawResult::SUCCESS, Move(pattern));
  }

  // Paint it!
  Matrix pMatrix;
  RefPtr<SourceSurface> surface;
  DrawResult result = DrawResult::SUCCESS;
  Tie(result, surface) =
    PaintPattern(aDrawTarget, &pMatrix, ToMatrix(aContextMatrix), aSource,
                 aFillOrStroke, aGraphicOpacity, aOverrideBounds, aFlags);

  if (!surface) {
    return MakePair(result, RefPtr<gfxPattern>());
  }

  RefPtr<gfxPattern> pattern = new gfxPattern(surface, pMatrix);

  if (!pattern) {
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<gfxPattern>());
  }

  pattern->SetExtend(ExtendMode::REPEAT);
  return MakePair(result, Move(pattern));
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* NS_NewSVGPatternFrame(nsIPresShell*   aPresShell,
                                nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGPatternFrame(aContext);
}

