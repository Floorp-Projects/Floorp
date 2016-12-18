/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGClipPathFrame.h"

// Keep others in (case-insensitive) order:
#include "DrawResult.h"
#include "gfxContext.h"
#include "mozilla/dom/SVGClipPathElement.h"
#include "nsGkAtoms.h"
#include "nsSVGEffects.h"
#include "nsSVGPathGeometryElement.h"
#include "nsSVGPathGeometryFrame.h"
#include "nsSVGUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

// Arbitrary number
#define MAX_SVG_CLIP_PATH_REFERENCE_CHAIN_LENGTH int16_t(512)

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGClipPathFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGClipPathFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGClipPathFrame)

void
nsSVGClipPathFrame::ApplyClipPath(gfxContext& aContext,
                                  nsIFrame* aClippedFrame,
                                  const gfxMatrix& aMatrix)
{
  MOZ_ASSERT(IsTrivial(), "Caller needs to use GetClipMask");

  DrawTarget& aDrawTarget = *aContext.GetDrawTarget();

  // No need for AutoReferenceLimiter since simple clip paths can't create
  // a reference loop (they don't reference other clip paths).

  // Restore current transform after applying clip path:
  gfxContextMatrixAutoSaveRestore autoRestore(&aContext);

  RefPtr<Path> clipPath;

  nsISVGChildFrame* singleClipPathChild = nullptr;
  IsTrivial(&singleClipPathChild);

  if (singleClipPathChild) {
    nsSVGPathGeometryFrame* pathFrame = do_QueryFrame(singleClipPathChild);
    if (pathFrame) {
      nsSVGPathGeometryElement* pathElement =
        static_cast<nsSVGPathGeometryElement*>(pathFrame->GetContent());
      gfxMatrix toChildsUserSpace = pathElement->
        PrependLocalTransformsTo(GetClipPathTransform(aClippedFrame) * aMatrix,
                                 eUserSpaceToParent);
      gfxMatrix newMatrix =
        aContext.CurrentMatrix().PreMultiply(toChildsUserSpace).NudgeToIntegers();
      if (!newMatrix.IsSingular()) {
        aContext.SetMatrix(newMatrix);
        FillRule clipRule =
          nsSVGUtils::ToFillRule(pathFrame->StyleSVG()->mClipRule);
        clipPath = pathElement->GetOrBuildPath(aDrawTarget, clipRule);
      }
    }
  }

  if (clipPath) {
    aContext.Clip(clipPath);
  } else {
    // The spec says clip away everything if we have no children or the
    // clipping path otherwise can't be resolved:
    aContext.Clip(Rect());
  }
}

already_AddRefed<DrawTarget>
nsSVGClipPathFrame::CreateClipMask(gfxContext& aReferenceContext,
                                   IntPoint& aOffset)
{
  gfxContextMatrixAutoSaveRestore autoRestoreMatrix(&aReferenceContext);

  aReferenceContext.SetMatrix(gfxMatrix());
  gfxRect rect = aReferenceContext.GetClipExtents();
  IntRect bounds = RoundedOut(ToRect(rect));
  if (bounds.IsEmpty()) {
    // We don't need to create a mask surface, all drawing is clipped anyway.
    return nullptr;
  }

  DrawTarget* referenceDT = aReferenceContext.GetDrawTarget();
  RefPtr<DrawTarget> maskDT =
    referenceDT->CreateSimilarDrawTarget(bounds.Size(), SurfaceFormat::A8);

  aOffset = bounds.TopLeft();

  return maskDT.forget();
}

static void
ComposeExtraMask(DrawTarget* aTarget, const gfxMatrix& aMaskTransfrom,
                 SourceSurface* aExtraMask, const Matrix& aExtraMasksTransform)
{
  MOZ_ASSERT(aExtraMask);

  Matrix origin = aTarget->GetTransform();
  aTarget->SetTransform(aExtraMasksTransform * aTarget->GetTransform());
  aTarget->MaskSurface(ColorPattern(Color(0.0, 0.0, 0.0, 1.0)),
                       aExtraMask,
                       Point(0, 0),
                       DrawOptions(1.0, CompositionOp::OP_IN));
  aTarget->SetTransform(origin);
}

DrawResult
nsSVGClipPathFrame::PaintClipMask(gfxContext& aMaskContext,
                                  nsIFrame* aClippedFrame,
                                  const gfxMatrix& aMatrix,
                                  Matrix* aMaskTransform,
                                  SourceSurface* aExtraMask,
                                  const Matrix& aExtraMasksTransform)
{
  // A clipPath can reference another clipPath.  We re-enter this method for
  // each clipPath in a reference chain, so here we limit chain length:
  static int16_t sRefChainLengthCounter = AutoReferenceLimiter::notReferencing;
  AutoReferenceLimiter
    refChainLengthLimiter(&sRefChainLengthCounter,
                          MAX_SVG_CLIP_PATH_REFERENCE_CHAIN_LENGTH);
  if (!refChainLengthLimiter.Reference()) {
    return DrawResult::SUCCESS; // Reference chain is too long!
  }

  // And to prevent reference loops we check that this clipPath only appears
  // once in the reference chain (if any) that we're currently processing:
  AutoReferenceLimiter refLoopDetector(&mReferencing, 1);
  if (!refLoopDetector.Reference()) {
    return DrawResult::SUCCESS; // Reference loop!
  }

  DrawResult result = DrawResult::SUCCESS;
  DrawTarget* maskDT = aMaskContext.GetDrawTarget();
  MOZ_ASSERT(maskDT->GetFormat() == SurfaceFormat::A8);

  // Paint this clipPath's contents into aMaskDT:
  // We need to set mMatrixForChildren here so that under the PaintSVG calls
  // on our children (below) our GetCanvasTM() method will return the correct
  // transform.
  mMatrixForChildren = GetClipPathTransform(aClippedFrame) * aMatrix;

  // Check if this clipPath is itself clipped by another clipPath:
  nsSVGClipPathFrame* clipPathThatClipsClipPath =
    nsSVGEffects::GetEffectProperties(this).GetClipPathFrame();
  nsSVGUtils::MaskUsage maskUsage;
  nsSVGUtils::DetermineMaskUsage(this, true, maskUsage);

  if (maskUsage.shouldApplyClipPath) {
    clipPathThatClipsClipPath->ApplyClipPath(aMaskContext, aClippedFrame,
                                             aMatrix);
  } else if (maskUsage.shouldGenerateClipMaskLayer) {
    Matrix maskTransform;
    RefPtr<SourceSurface> maskSurface;
    Tie(result, maskSurface) =
      clipPathThatClipsClipPath->GetClipMask(aMaskContext, aClippedFrame,
                                             aMatrix, &maskTransform);
    aMaskContext.PushGroupForBlendBack(gfxContentType::ALPHA, 1.0,
                                       maskSurface, maskTransform);
    // The corresponding PopGroupAndBlend call below will mask the
    // blend using |maskSurface|.
  }

  // Paint our children into the mask:
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    result &= PaintFrameIntoMask(kid, aClippedFrame, aMaskContext, aMatrix);
  }

  if (maskUsage.shouldGenerateClipMaskLayer) {
    aMaskContext.PopGroupAndBlend();
  } else if (maskUsage.shouldApplyClipPath) {
    aMaskContext.PopClip();
  }

  // Moz2D transforms in the opposite direction to Thebes
  gfxMatrix maskTransfrom = aMaskContext.CurrentMatrix();
  maskTransfrom.Invert();

  if (aExtraMask) {
    ComposeExtraMask(maskDT, maskTransfrom, aExtraMask, aExtraMasksTransform);
  }

  *aMaskTransform = ToMatrix(maskTransfrom);
  return result;
}

DrawResult
nsSVGClipPathFrame::PaintFrameIntoMask(nsIFrame *aFrame,
                                       nsIFrame* aClippedFrame,
                                       gfxContext& aTarget,
                                       const gfxMatrix& aMatrix)
{
  nsISVGChildFrame* frame = do_QueryFrame(aFrame);
  if (!frame) {
    return DrawResult::SUCCESS;
  }

  // The CTM of each frame referencing us can be different.
  frame->NotifySVGChanged(nsISVGChildFrame::TRANSFORM_CHANGED);

  // Children of this clipPath may themselves be clipped.
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(aFrame);
  if (effectProperties.HasInvalidClipPath()) {
    return DrawResult::SUCCESS;
  }
  nsSVGClipPathFrame *clipPathThatClipsChild =
    effectProperties.GetClipPathFrame();

  nsSVGUtils::MaskUsage maskUsage;
  nsSVGUtils::DetermineMaskUsage(aFrame, true, maskUsage);
  DrawResult result = DrawResult::SUCCESS;
  if (maskUsage.shouldApplyClipPath) {
    clipPathThatClipsChild->ApplyClipPath(aTarget, aClippedFrame, aMatrix);
  } else if (maskUsage.shouldGenerateClipMaskLayer) {
    Matrix maskTransform;
    RefPtr<SourceSurface> maskSurface;
    Tie(result, maskSurface) =
      clipPathThatClipsChild->GetClipMask(aTarget, aClippedFrame,
                                          aMatrix, &maskTransform);
    aTarget.PushGroupForBlendBack(gfxContentType::ALPHA, 1.0,
                                  maskSurface, maskTransform);
    // The corresponding PopGroupAndBlend call below will mask the
    // blend using |maskSurface|.
  }

  gfxMatrix toChildsUserSpace = mMatrixForChildren;
  nsIFrame* child = do_QueryFrame(frame);
  nsIContent* childContent = child->GetContent();
  if (childContent->IsSVGElement()) {
    toChildsUserSpace =
      static_cast<const nsSVGElement*>(childContent)->
        PrependLocalTransformsTo(mMatrixForChildren, eUserSpaceToParent);
  }

  // Our children have NS_STATE_SVG_CLIPPATH_CHILD set on them, and
  // nsSVGPathGeometryFrame::Render checks for that state bit and paints
  // only the geometry (opaque black) if set.
  result &= frame->PaintSVG(aTarget, toChildsUserSpace);

  if (maskUsage.shouldGenerateClipMaskLayer) {
    aTarget.PopGroupAndBlend();
  } else if (maskUsage.shouldApplyClipPath) {
    aTarget.PopClip();
  }

  return result;
}

mozilla::Pair<DrawResult, RefPtr<SourceSurface>>
nsSVGClipPathFrame::GetClipMask(gfxContext& aReferenceContext,
                                nsIFrame* aClippedFrame,
                                const gfxMatrix& aMatrix,
                                Matrix* aMaskTransform,
                                SourceSurface* aExtraMask,
                                const Matrix& aExtraMasksTransform)
{
  MOZ_ASSERT(!IsTrivial(), "Caller needs to use ApplyClipPath");

  IntPoint offset;
  RefPtr<DrawTarget> maskDT = CreateClipMask(aReferenceContext, offset);
  if (!maskDT) {
    return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
  }

  RefPtr<gfxContext> maskContext = gfxContext::CreateOrNull(maskDT);
  if (!maskContext) {
    gfxCriticalError() << "SVGClipPath context problem " << gfx::hexa(maskDT);
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }
  maskContext->SetMatrix(aReferenceContext.CurrentMatrix() *
                         gfxMatrix::Translation(-offset));

  DrawResult result = PaintClipMask(*maskContext, aClippedFrame, aMatrix,
                                    aMaskTransform, aExtraMask,
                                    aExtraMasksTransform);

  RefPtr<SourceSurface> surface = maskDT->Snapshot();
  return MakePair(result, Move(surface));
}

bool
nsSVGClipPathFrame::PointIsInsideClipPath(nsIFrame* aClippedFrame,
                                          const gfxPoint &aPoint)
{
  // A clipPath can reference another clipPath.  We re-enter this method for
  // each clipPath in a reference chain, so here we limit chain length:
  static int16_t sRefChainLengthCounter = AutoReferenceLimiter::notReferencing;
  AutoReferenceLimiter
    refChainLengthLimiter(&sRefChainLengthCounter,
                          MAX_SVG_CLIP_PATH_REFERENCE_CHAIN_LENGTH);
  if (!refChainLengthLimiter.Reference()) {
    return false; // Reference chain is too long!
  }

  // And to prevent reference loops we check that this clipPath only appears
  // once in the reference chain (if any) that we're currently processing:
  AutoReferenceLimiter refLoopDetector(&mReferencing, 1);
  if (!refLoopDetector.Reference()) {
    return true; // Reference loop!
  }

  gfxMatrix matrix = GetClipPathTransform(aClippedFrame);
  if (!matrix.Invert()) {
    return false;
  }
  gfxPoint point = matrix.Transform(aPoint);

  // clipPath elements can themselves be clipped by a different clip path. In
  // that case the other clip path further clips away the element that is being
  // clipped by the original clipPath. If this clipPath is being clipped by a
  // different clip path we need to check if it prevents the original element
  // from recieving events at aPoint:
  nsSVGClipPathFrame *clipPathFrame =
    nsSVGEffects::GetEffectProperties(this).GetClipPathFrame();
  if (clipPathFrame &&
      !clipPathFrame->PointIsInsideClipPath(aClippedFrame, aPoint)) {
    return false;
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      gfxPoint pointForChild = point;
      gfxMatrix m = static_cast<nsSVGElement*>(kid->GetContent())->
        PrependLocalTransformsTo(gfxMatrix(), eUserSpaceToParent);
      if (!m.IsIdentity()) {
        if (!m.Invert()) {
          return false;
        }
        pointForChild = m.Transform(point);
      }
      if (SVGFrame->GetFrameForPoint(pointForChild)) {
        return true;
      }
    }
  }

  return false;
}

bool
nsSVGClipPathFrame::IsTrivial(nsISVGChildFrame **aSingleChild)
{
  // If the clip path is clipped then it's non-trivial
  if (nsSVGEffects::GetEffectProperties(this).GetClipPathFrame())
    return false;

  if (aSingleChild) {
    *aSingleChild = nullptr;
  }

  nsISVGChildFrame *foundChild = nullptr;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame *svgChild = do_QueryFrame(kid);
    if (svgChild) {
      // We consider a non-trivial clipPath to be one containing
      // either more than one svg child and/or a svg container
      if (foundChild || svgChild->IsDisplayContainer())
        return false;

      // or where the child is itself clipped
      if (nsSVGEffects::GetEffectProperties(kid).GetClipPathFrame())
        return false;

      foundChild = svgChild;
    }
  }
  if (aSingleChild) {
    *aSingleChild = foundChild;
  }
  return true;
}

bool
nsSVGClipPathFrame::IsValid()
{
  // A clipPath can reference another clipPath.  We re-enter this method for
  // each clipPath in a reference chain, so here we limit chain length:
  static int16_t sRefChainLengthCounter = AutoReferenceLimiter::notReferencing;
  AutoReferenceLimiter
    refChainLengthLimiter(&sRefChainLengthCounter,
                          MAX_SVG_CLIP_PATH_REFERENCE_CHAIN_LENGTH);
  if (!refChainLengthLimiter.Reference()) {
    return false; // Reference chain is too long!
  }

  // And to prevent reference loops we check that this clipPath only appears
  // once in the reference chain (if any) that we're currently processing:
  AutoReferenceLimiter refLoopDetector(&mReferencing, 1);
  if (!refLoopDetector.Reference()) {
    return false; // Reference loop!
  }

  if (nsSVGEffects::GetEffectProperties(this).HasInvalidClipPath()) {
    return false;
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {

    nsIAtom* kidType = kid->GetType();

    if (kidType == nsGkAtoms::svgUseFrame) {
      for (nsIFrame* grandKid : kid->PrincipalChildList()) {

        nsIAtom* grandKidType = grandKid->GetType();

        if (grandKidType != nsGkAtoms::svgPathGeometryFrame &&
            grandKidType != nsGkAtoms::svgTextFrame) {
          return false;
        }
      }
      continue;
    }

    if (kidType != nsGkAtoms::svgPathGeometryFrame &&
        kidType != nsGkAtoms::svgTextFrame) {
      return false;
    }
  }

  return true;
}

nsresult
nsSVGClipPathFrame::AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::transform) {
      nsSVGEffects::InvalidateDirectRenderingObservers(this);
      nsSVGUtils::NotifyChildrenOfSVGChange(this,
                                            nsISVGChildFrame::TRANSFORM_CHANGED);
    }
    if (aAttribute == nsGkAtoms::clipPathUnits) {
      nsSVGEffects::InvalidateDirectRenderingObservers(this);
    }
  }

  return nsSVGContainerFrame::AttributeChanged(aNameSpaceID,
                                               aAttribute, aModType);
}

void
nsSVGClipPathFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::clipPath),
               "Content is not an SVG clipPath!");

  AddStateBits(NS_STATE_SVG_CLIPPATH_CHILD);
  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

nsIAtom *
nsSVGClipPathFrame::GetType() const
{
  return nsGkAtoms::svgClipPathFrame;
}

gfxMatrix
nsSVGClipPathFrame::GetCanvasTM()
{
  return mMatrixForChildren;
}

gfxMatrix
nsSVGClipPathFrame::GetClipPathTransform(nsIFrame* aClippedFrame)
{
  SVGClipPathElement *content = static_cast<SVGClipPathElement*>(mContent);

  gfxMatrix tm = content->PrependLocalTransformsTo(gfxMatrix());

  nsSVGEnum* clipPathUnits =
    &content->mEnumAttributes[SVGClipPathElement::CLIPPATHUNITS];

  return nsSVGUtils::AdjustMatrixForUnits(tm, clipPathUnits, aClippedFrame);
}

SVGBBox
nsSVGClipPathFrame::GetBBoxForClipPathFrame(const SVGBBox &aBBox,
                                            const gfxMatrix &aMatrix)
{
  nsIContent* node = GetContent()->GetFirstChild();
  SVGBBox unionBBox, tmpBBox;
  for (; node; node = node->GetNextSibling()) {
    nsIFrame *frame =
      static_cast<nsSVGElement*>(node)->GetPrimaryFrame();
    if (frame) {
      nsISVGChildFrame *svg = do_QueryFrame(frame);
      if (svg) {
        tmpBBox = svg->GetBBoxContribution(mozilla::gfx::ToMatrix(aMatrix),
                                           nsSVGUtils::eBBoxIncludeFill);
        nsSVGEffects::EffectProperties effectProperties =
                              nsSVGEffects::GetEffectProperties(frame);
        if (effectProperties.HasNoOrValidClipPath()) {
          nsSVGClipPathFrame *clipPathFrame =
            effectProperties.GetClipPathFrame();
          if (clipPathFrame) {
            tmpBBox = clipPathFrame->GetBBoxForClipPathFrame(tmpBBox, aMatrix);
          }
        }
        tmpBBox.Intersect(aBBox);
        unionBBox.UnionEdges(tmpBBox);
      }
    }
  }

  nsSVGEffects::EffectProperties props =
    nsSVGEffects::GetEffectProperties(this);
  if (props.mClipPath) {
    if (props.HasInvalidClipPath()) {
      unionBBox = SVGBBox();
    } else  {
      nsSVGClipPathFrame *clipPathFrame = props.GetClipPathFrame();
      if (clipPathFrame) {
        tmpBBox = clipPathFrame->GetBBoxForClipPathFrame(aBBox, aMatrix);
        unionBBox.Intersect(tmpBBox);
      }
    }
  }
  return unionBBox;
}
