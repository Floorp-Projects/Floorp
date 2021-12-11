/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGPatternFrame.h"

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "gfxPlatform.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/PresShell.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/SVGGeometryFrame.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/SVGPatternElement.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "mozilla/gfx/2D.h"
#include "nsGkAtoms.h"
#include "nsIFrameInlines.h"
#include "SVGAnimatedTransformList.h"

using namespace mozilla::dom;
using namespace mozilla::dom::SVGUnitTypes_Binding;
using namespace mozilla::gfx;
using namespace mozilla::image;

namespace mozilla {

//----------------------------------------------------------------------
// Implementation

SVGPatternFrame::SVGPatternFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext)
    : SVGPaintServerFrame(aStyle, aPresContext, kClassID),
      mSource(nullptr),
      mLoopFlag(false),
      mNoHRefURI(false) {}

NS_IMPL_FRAMEARENA_HELPERS(SVGPatternFrame)

NS_QUERYFRAME_HEAD(SVGPatternFrame)
  NS_QUERYFRAME_ENTRY(SVGPatternFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGPaintServerFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

nsresult SVGPatternFrame::AttributeChanged(int32_t aNameSpaceID,
                                           nsAtom* aAttribute,
                                           int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::patternUnits ||
       aAttribute == nsGkAtoms::patternContentUnits ||
       aAttribute == nsGkAtoms::patternTransform ||
       aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::preserveAspectRatio ||
       aAttribute == nsGkAtoms::viewBox)) {
    SVGObserverUtils::InvalidateDirectRenderingObservers(this);
  }

  if ((aNameSpaceID == kNameSpaceID_XLink ||
       aNameSpaceID == kNameSpaceID_None) &&
      aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    SVGObserverUtils::RemoveTemplateObserver(this);
    mNoHRefURI = false;
    // And update whoever references us
    SVGObserverUtils::InvalidateDirectRenderingObservers(this);
  }

  return SVGPaintServerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                               aModType);
}

#ifdef DEBUG
void SVGPatternFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                           nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::pattern),
               "Content is not an SVG pattern");

  SVGPaintServerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

//----------------------------------------------------------------------
// SVGContainerFrame methods:

// If our GetCanvasTM is getting called, we
// need to return *our current* transformation
// matrix, which depends on our units parameters
// and X, Y, Width, and Height
gfxMatrix SVGPatternFrame::GetCanvasTM() {
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
static float MaxExpansion(const Matrix& aMatrix) {
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

// The SVG specification says that the 'patternContentUnits' attribute "has no
// effect if attribute ‘viewBox’ is specified". We still need to include a bbox
// scale if the viewBox is specified and _patternUnits_ is set to or defaults to
// objectBoundingBox though, since in that case the viewBox is relative to the
// bbox
static bool IncludeBBoxScale(const SVGAnimatedViewBox& aViewBox,
                             uint32_t aPatternContentUnits,
                             uint32_t aPatternUnits) {
  return (!aViewBox.IsExplicitlySet() &&
          aPatternContentUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) ||
         (aViewBox.IsExplicitlySet() &&
          aPatternUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
}

// Given the matrix for the pattern element's own transform, this returns a
// combined matrix including the transforms applicable to its target.
static Matrix GetPatternMatrix(uint16_t aPatternUnits,
                               const Matrix& patternTransform,
                               const gfxRect& bbox, const gfxRect& callerBBox,
                               const Matrix& callerCTM) {
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

static nsresult GetTargetGeometry(gfxRect* aBBox,
                                  const SVGAnimatedViewBox& aViewBox,
                                  uint16_t aPatternContentUnits,
                                  uint16_t aPatternUnits, nsIFrame* aTarget,
                                  const Matrix& aContextMatrix,
                                  const gfxRect* aOverrideBounds) {
  *aBBox =
      aOverrideBounds
          ? *aOverrideBounds
          : SVGUtils::GetBBox(aTarget, SVGUtils::eUseFrameBoundsForOuterSVG |
                                           SVGUtils::eBBoxIncludeFillGeometry);

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

already_AddRefed<SourceSurface> SVGPatternFrame::PaintPattern(
    const DrawTarget* aDrawTarget, Matrix* patternMatrix,
    const Matrix& aContextMatrix, nsIFrame* aSource,
    StyleSVGPaint nsStyleSVG::*aFillOrStroke, float aGraphicOpacity,
    const gfxRect* aOverrideBounds, imgDrawingParams& aImgParams) {
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

  SVGPatternFrame* patternWithChildren = GetPatternWithChildren();
  if (!patternWithChildren) {
    // Either no kids or a bad reference
    return nullptr;
  }
  nsIFrame* firstKid = patternWithChildren->mFrames.FirstChild();

  const SVGAnimatedViewBox& viewBox = GetViewBox();

  uint16_t patternContentUnits =
      GetEnumValue(SVGPatternElement::PATTERNCONTENTUNITS);
  uint16_t patternUnits = GetEnumValue(SVGPatternElement::PATTERNUNITS);

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
  if (NS_FAILED(GetTargetGeometry(&callerBBox, viewBox, patternContentUnits,
                                  patternUnits, aSource, aContextMatrix,
                                  aOverrideBounds))) {
    return nullptr;
  }

  // Construct the CTM that we will provide to our children when we
  // render them into the tile.
  gfxMatrix ctm = ConstructCTM(viewBox, patternContentUnits, patternUnits,
                               callerBBox, aContextMatrix, aSource);
  if (ctm.IsSingular()) {
    return nullptr;
  }

  if (patternWithChildren->mCTM) {
    *patternWithChildren->mCTM = ctm;
  } else {
    patternWithChildren->mCTM = MakeUnique<gfxMatrix>(ctm);
  }

  // Get the bounding box of the pattern.  This will be used to determine
  // the size of the surface, and will also be used to define the bounding
  // box for the pattern tile.
  gfxRect bbox =
      GetPatternRect(patternUnits, callerBBox, aContextMatrix, aSource);
  if (bbox.Width() <= 0.0 || bbox.Height() <= 0.0) {
    return nullptr;
  }

  // Get the pattern transform
  Matrix patternTransform = ToMatrix(GetPatternTransform());

  // revert the vector effect transform so that the pattern appears unchanged
  if (aFillOrStroke == &nsStyleSVG::mStroke) {
    gfxMatrix userToOuterSVG;
    if (SVGUtils::GetNonScalingStrokeTransform(aSource, &userToOuterSVG)) {
      patternTransform *= ToMatrix(userToOuterSVG);
      if (patternTransform.IsSingular()) {
        NS_WARNING("Singular matrix painting non-scaling-stroke");
        return nullptr;
      }
    }
  }

  // Get the transformation matrix that we will hand to the renderer's pattern
  // routine.
  *patternMatrix = GetPatternMatrix(patternUnits, patternTransform, bbox,
                                    callerBBox, aContextMatrix);
  if (patternMatrix->IsSingular()) {
    return nullptr;
  }

  // Now that we have all of the necessary geometries, we can
  // create our surface.
  gfxRect transformedBBox =
      ThebesRect(patternTransform.TransformBounds(ToRect(bbox)));

  bool resultOverflows;
  IntSize surfaceSize =
      SVGUtils::ConvertToSurfaceSize(transformedBBox.Size(), &resultOverflows);

  // 0 disables rendering, < 0 is an error
  if (surfaceSize.width <= 0 || surfaceSize.height <= 0) {
    return nullptr;
  }

  gfxFloat patternWidth = bbox.Width();
  gfxFloat patternHeight = bbox.Height();

  if (resultOverflows || patternWidth != surfaceSize.width ||
      patternHeight != surfaceSize.height) {
    // scale drawing to pattern surface size
    gfxMatrix tempTM = gfxMatrix(surfaceSize.width / patternWidth, 0.0, 0.0,
                                 surfaceSize.height / patternHeight, 0.0, 0.0);
    patternWithChildren->mCTM->PreMultiply(tempTM);

    // and rescale pattern to compensate
    patternMatrix->PreScale(patternWidth / surfaceSize.width,
                            patternHeight / surfaceSize.height);
  }

  RefPtr<DrawTarget> dt = aDrawTarget->CreateSimilarDrawTargetWithBacking(
      surfaceSize, SurfaceFormat::B8G8R8A8);
  if (!dt || !dt->IsValid()) {
    return nullptr;
  }
  dt->ClearRect(Rect(0, 0, surfaceSize.width, surfaceSize.height));

  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(dt);
  MOZ_ASSERT(ctx);  // already checked the draw target above

  if (aGraphicOpacity != 1.0f) {
    ctx->Save();
    ctx->PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, aGraphicOpacity);
  }

  // OK, now render -- note that we use "firstKid", which
  // we got at the beginning because it takes care of the
  // referenced pattern situation for us

  if (aSource->IsSVGGeometryFrameOrSubclass()) {
    // Set the geometrical parent of the pattern we are rendering
    patternWithChildren->mSource = static_cast<SVGGeometryFrame*>(aSource);
  }

  // Delay checking NS_FRAME_DRAWING_AS_PAINTSERVER bit until here so we can
  // give back a clear surface if there's a loop
  if (!patternWithChildren->HasAnyStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER)) {
    AutoSetRestorePaintServerState paintServer(patternWithChildren);
    for (nsIFrame* kid = firstKid; kid; kid = kid->GetNextSibling()) {
      gfxMatrix tm = *(patternWithChildren->mCTM);

      // The CTM of each frame referencing us can be different
      ISVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
      if (SVGFrame) {
        SVGFrame->NotifySVGChanged(ISVGDisplayableFrame::TRANSFORM_CHANGED);
        tm = SVGUtils::GetTransformMatrixInUserSpace(kid) * tm;
      }

      SVGUtils::PaintFrameWithEffects(kid, *ctx, tm, aImgParams);
    }
  }

  patternWithChildren->mSource = nullptr;

  if (aGraphicOpacity != 1.0f) {
    ctx->PopGroupAndBlend();
    ctx->Restore();
  }

  // caller now owns the surface
  return dt->GetBackingSurface();
}

/* Will probably need something like this... */
// How do we handle the insertion of a new frame?
// We really don't want to rerender this every time,
// do we?
SVGPatternFrame* SVGPatternFrame::GetPatternWithChildren() {
  // Do we have any children ourselves?
  if (!mFrames.IsEmpty()) {
    return this;
  }

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

  SVGPatternFrame* next = GetReferencedPattern();
  if (!next) {
    return nullptr;
  }

  return next->GetPatternWithChildren();
}

uint16_t SVGPatternFrame::GetEnumValue(uint32_t aIndex, nsIContent* aDefault) {
  SVGAnimatedEnumeration& thisEnum =
      static_cast<SVGPatternElement*>(GetContent())->mEnumAttributes[aIndex];

  if (thisEnum.IsExplicitlySet()) {
    return thisEnum.GetAnimValue();
  }

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGPatternElement*>(aDefault)
        ->mEnumAttributes[aIndex]
        .GetAnimValue();
  }

  SVGPatternFrame* next = GetReferencedPattern();
  return next ? next->GetEnumValue(aIndex, aDefault)
              : static_cast<SVGPatternElement*>(aDefault)
                    ->mEnumAttributes[aIndex]
                    .GetAnimValue();
}

SVGAnimatedTransformList* SVGPatternFrame::GetPatternTransformList(
    nsIContent* aDefault) {
  SVGAnimatedTransformList* thisTransformList =
      static_cast<SVGPatternElement*>(GetContent())->GetAnimatedTransformList();

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

  SVGPatternFrame* next = GetReferencedPattern();
  return next ? next->GetPatternTransformList(aDefault)
              : static_cast<SVGPatternElement*>(aDefault)
                    ->mPatternTransform.get();
}

gfxMatrix SVGPatternFrame::GetPatternTransform() {
  SVGAnimatedTransformList* animTransformList =
      GetPatternTransformList(GetContent());
  if (!animTransformList) {
    return gfxMatrix();
  }

  return animTransformList->GetAnimValue().GetConsolidationMatrix();
}

const SVGAnimatedViewBox& SVGPatternFrame::GetViewBox(nsIContent* aDefault) {
  const SVGAnimatedViewBox& thisViewBox =
      static_cast<SVGPatternElement*>(GetContent())->mViewBox;

  if (thisViewBox.IsExplicitlySet()) {
    return thisViewBox;
  }

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGPatternElement*>(aDefault)->mViewBox;
  }

  SVGPatternFrame* next = GetReferencedPattern();
  return next ? next->GetViewBox(aDefault)
              : static_cast<SVGPatternElement*>(aDefault)->mViewBox;
}

const SVGAnimatedPreserveAspectRatio& SVGPatternFrame::GetPreserveAspectRatio(
    nsIContent* aDefault) {
  const SVGAnimatedPreserveAspectRatio& thisPar =
      static_cast<SVGPatternElement*>(GetContent())->mPreserveAspectRatio;

  if (thisPar.IsExplicitlySet()) {
    return thisPar;
  }

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGPatternElement*>(aDefault)->mPreserveAspectRatio;
  }

  SVGPatternFrame* next = GetReferencedPattern();
  return next ? next->GetPreserveAspectRatio(aDefault)
              : static_cast<SVGPatternElement*>(aDefault)->mPreserveAspectRatio;
}

const SVGAnimatedLength* SVGPatternFrame::GetLengthValue(uint32_t aIndex,
                                                         nsIContent* aDefault) {
  const SVGAnimatedLength* thisLength =
      &static_cast<SVGPatternElement*>(GetContent())->mLengthAttributes[aIndex];

  if (thisLength->IsExplicitlySet()) {
    return thisLength;
  }

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return &static_cast<SVGPatternElement*>(aDefault)
                ->mLengthAttributes[aIndex];
  }

  SVGPatternFrame* next = GetReferencedPattern();
  return next ? next->GetLengthValue(aIndex, aDefault)
              : &static_cast<SVGPatternElement*>(aDefault)
                     ->mLengthAttributes[aIndex];
}

// Private (helper) methods

SVGPatternFrame* SVGPatternFrame::GetReferencedPattern() {
  if (mNoHRefURI) {
    return nullptr;
  }

  auto GetHref = [this](nsAString& aHref) {
    SVGPatternElement* pattern =
        static_cast<SVGPatternElement*>(this->GetContent());
    if (pattern->mStringAttributes[SVGPatternElement::HREF].IsExplicitlySet()) {
      pattern->mStringAttributes[SVGPatternElement::HREF].GetAnimValue(aHref,
                                                                       pattern);
    } else {
      pattern->mStringAttributes[SVGPatternElement::XLINK_HREF].GetAnimValue(
          aHref, pattern);
    }
    this->mNoHRefURI = aHref.IsEmpty();
  };

  nsIFrame* tframe = SVGObserverUtils::GetAndObserveTemplate(this, GetHref);
  if (tframe) {
    LayoutFrameType frameType = tframe->Type();
    if (frameType == LayoutFrameType::SVGPattern) {
      return static_cast<SVGPatternFrame*>(tframe);
    }
    // We don't call SVGObserverUtils::RemoveTemplateObserver and set
    // `mNoHRefURI = false` here since we want to be invalidated if the ID
    // specified by our href starts resolving to a different/valid element.
  }

  return nullptr;
}

gfxRect SVGPatternFrame::GetPatternRect(uint16_t aPatternUnits,
                                        const gfxRect& aTargetBBox,
                                        const Matrix& aTargetCTM,
                                        nsIFrame* aTarget) {
  // We need to initialize our box
  float x, y, width, height;

  // Get the pattern x,y,width, and height
  const SVGAnimatedLength *tmpX, *tmpY, *tmpHeight, *tmpWidth;
  tmpX = GetLengthValue(SVGPatternElement::ATTR_X);
  tmpY = GetLengthValue(SVGPatternElement::ATTR_Y);
  tmpHeight = GetLengthValue(SVGPatternElement::ATTR_HEIGHT);
  tmpWidth = GetLengthValue(SVGPatternElement::ATTR_WIDTH);

  if (aPatternUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    x = SVGUtils::ObjectSpace(aTargetBBox, tmpX);
    y = SVGUtils::ObjectSpace(aTargetBBox, tmpY);
    width = SVGUtils::ObjectSpace(aTargetBBox, tmpWidth);
    height = SVGUtils::ObjectSpace(aTargetBBox, tmpHeight);
  } else {
    float scale = MaxExpansion(aTargetCTM);
    x = SVGUtils::UserSpace(aTarget, tmpX) * scale;
    y = SVGUtils::UserSpace(aTarget, tmpY) * scale;
    width = SVGUtils::UserSpace(aTarget, tmpWidth) * scale;
    height = SVGUtils::UserSpace(aTarget, tmpHeight) * scale;
  }

  return gfxRect(x, y, width, height);
}

gfxMatrix SVGPatternFrame::ConstructCTM(const SVGAnimatedViewBox& aViewBox,
                                        uint16_t aPatternContentUnits,
                                        uint16_t aPatternUnits,
                                        const gfxRect& callerBBox,
                                        const Matrix& callerCTM,
                                        nsIFrame* aTarget) {
  SVGViewportElement* ctx = nullptr;
  nsIContent* targetContent = aTarget->GetContent();
  gfxFloat scaleX, scaleY;

  // The objectBoundingBox conversion must be handled in the CTM:
  if (IncludeBBoxScale(aViewBox, aPatternContentUnits, aPatternUnits)) {
    scaleX = callerBBox.Width();
    scaleY = callerBBox.Height();
  } else {
    if (targetContent->IsSVGElement()) {
      ctx = static_cast<SVGElement*>(targetContent)->GetCtx();
    }
    scaleX = scaleY = MaxExpansion(callerCTM);
  }

  if (!aViewBox.IsExplicitlySet()) {
    return gfxMatrix(scaleX, 0.0, 0.0, scaleY, 0.0, 0.0);
  }
  const SVGViewBox& viewBox = aViewBox.GetAnimValue();

  if (viewBox.height <= 0.0f || viewBox.width <= 0.0f) {
    return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);  // singular
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
    return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);  // singular
  }

  Matrix tm = SVGContentUtils::GetViewBoxTransform(
      viewportWidth * scaleX, viewportHeight * scaleY, viewBox.x, viewBox.y,
      viewBox.width, viewBox.height, GetPreserveAspectRatio());

  return ThebesMatrix(tm);
}

//----------------------------------------------------------------------
// SVGPaintServerFrame methods:
already_AddRefed<gfxPattern> SVGPatternFrame::GetPaintServerPattern(
    nsIFrame* aSource, const DrawTarget* aDrawTarget,
    const gfxMatrix& aContextMatrix, StyleSVGPaint nsStyleSVG::*aFillOrStroke,
    float aGraphicOpacity, imgDrawingParams& aImgParams,
    const gfxRect* aOverrideBounds) {
  if (aGraphicOpacity == 0.0f) {
    return do_AddRef(new gfxPattern(DeviceColor()));
  }

  // Paint it!
  Matrix pMatrix;
  RefPtr<SourceSurface> surface =
      PaintPattern(aDrawTarget, &pMatrix, ToMatrix(aContextMatrix), aSource,
                   aFillOrStroke, aGraphicOpacity, aOverrideBounds, aImgParams);

  if (!surface) {
    return nullptr;
  }

  RefPtr<gfxPattern> pattern = new gfxPattern(surface, pMatrix);

  if (!pattern) {
    return nullptr;
  }

  pattern->SetExtend(ExtendMode::REPEAT);
  return pattern.forget();
}

}  // namespace mozilla

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* NS_NewSVGPatternFrame(mozilla::PresShell* aPresShell,
                                mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGPatternFrame(aStyle, aPresShell->GetPresContext());
}
