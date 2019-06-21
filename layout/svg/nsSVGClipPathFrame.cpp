/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGClipPathFrame.h"

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "ImgDrawResult.h"
#include "gfxContext.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/SVGClipPathElement.h"
#include "nsGkAtoms.h"
#include "SVGObserverUtils.h"
#include "SVGGeometryElement.h"
#include "SVGGeometryFrame.h"
#include "nsSVGUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

//----------------------------------------------------------------------
// Implementation

nsIFrame* NS_NewSVGClipPathFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsSVGClipPathFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGClipPathFrame)

void nsSVGClipPathFrame::ApplyClipPath(gfxContext& aContext,
                                       nsIFrame* aClippedFrame,
                                       const gfxMatrix& aMatrix) {
  MOZ_ASSERT(IsTrivial(), "Caller needs to use GetClipMask");

  const DrawTarget* drawTarget = aContext.GetDrawTarget();

  // No need for AutoReferenceChainGuard since simple clip paths by definition
  // don't reference another clip path.

  // Restore current transform after applying clip path:
  gfxContextMatrixAutoSaveRestore autoRestore(&aContext);

  RefPtr<Path> clipPath;

  nsSVGDisplayableFrame* singleClipPathChild = nullptr;
  IsTrivial(&singleClipPathChild);

  if (singleClipPathChild) {
    SVGGeometryFrame* pathFrame = do_QueryFrame(singleClipPathChild);
    if (pathFrame && pathFrame->StyleVisibility()->IsVisible()) {
      SVGGeometryElement* pathElement =
          static_cast<SVGGeometryElement*>(pathFrame->GetContent());

      gfxMatrix toChildsUserSpace =
          nsSVGUtils::GetTransformMatrixInUserSpace(pathFrame) *
          (GetClipPathTransform(aClippedFrame) * aMatrix);

      gfxMatrix newMatrix = aContext.CurrentMatrixDouble()
                                .PreMultiply(toChildsUserSpace)
                                .NudgeToIntegers();
      if (!newMatrix.IsSingular()) {
        aContext.SetMatrixDouble(newMatrix);
        FillRule clipRule =
            nsSVGUtils::ToFillRule(pathFrame->StyleSVG()->mClipRule);
        clipPath = pathElement->GetOrBuildPath(drawTarget, clipRule);
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

already_AddRefed<DrawTarget> nsSVGClipPathFrame::CreateClipMask(
    gfxContext& aReferenceContext, IntPoint& aOffset) {
  IntRect bounds = RoundedOut(
      ToRect(aReferenceContext.GetClipExtents(gfxContext::eDeviceSpace)));
  if (bounds.IsEmpty()) {
    // We don't need to create a mask surface, all drawing is clipped anyway.
    return nullptr;
  }

  DrawTarget* referenceDT = aReferenceContext.GetDrawTarget();
  RefPtr<DrawTarget> maskDT = referenceDT->CreateClippedDrawTarget(
      bounds.Size(), Matrix::Translation(bounds.TopLeft()), SurfaceFormat::A8);

  aOffset = bounds.TopLeft();

  return maskDT.forget();
}

static void ComposeExtraMask(DrawTarget* aTarget, SourceSurface* aExtraMask,
                             const Matrix& aExtraMasksTransform) {
  MOZ_ASSERT(aExtraMask);

  Matrix origin = aTarget->GetTransform();
  aTarget->SetTransform(aExtraMasksTransform * aTarget->GetTransform());
  aTarget->MaskSurface(ColorPattern(Color(0.0, 0.0, 0.0, 1.0)), aExtraMask,
                       Point(0, 0), DrawOptions(1.0, CompositionOp::OP_IN));
  aTarget->SetTransform(origin);
}

void nsSVGClipPathFrame::PaintClipMask(gfxContext& aMaskContext,
                                       nsIFrame* aClippedFrame,
                                       const gfxMatrix& aMatrix,
                                       Matrix* aMaskTransform,
                                       SourceSurface* aExtraMask,
                                       const Matrix& aExtraMasksTransform) {
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;

  // A clipPath can reference another clipPath, creating a chain of clipPaths
  // that must all be applied.  We re-enter this method for each clipPath in a
  // chain, so we need to protect against reference chain related crashes etc.:
  AutoReferenceChainGuard refChainGuard(this, &mIsBeingProcessed,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    return;  // Break reference chain
  }

  DrawTarget* maskDT = aMaskContext.GetDrawTarget();
  MOZ_ASSERT(maskDT->GetFormat() == SurfaceFormat::A8);

  // Paint this clipPath's contents into aMaskDT:
  // We need to set mMatrixForChildren here so that under the PaintSVG calls
  // on our children (below) our GetCanvasTM() method will return the correct
  // transform.
  mMatrixForChildren = GetClipPathTransform(aClippedFrame) * aMatrix;

  // Check if this clipPath is itself clipped by another clipPath:
  nsSVGClipPathFrame* clipPathThatClipsClipPath;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveClipPath(this, &clipPathThatClipsClipPath);
  nsSVGUtils::MaskUsage maskUsage;
  nsSVGUtils::DetermineMaskUsage(this, true, maskUsage);

  if (maskUsage.shouldApplyClipPath) {
    clipPathThatClipsClipPath->ApplyClipPath(aMaskContext, aClippedFrame,
                                             aMatrix);
  } else if (maskUsage.shouldGenerateClipMaskLayer) {
    Matrix maskTransform;
    RefPtr<SourceSurface> maskSurface = clipPathThatClipsClipPath->GetClipMask(
        aMaskContext, aClippedFrame, aMatrix, &maskTransform);
    aMaskContext.PushGroupForBlendBack(gfxContentType::ALPHA, 1.0, maskSurface,
                                       maskTransform);
    // The corresponding PopGroupAndBlend call below will mask the
    // blend using |maskSurface|.
  }

  // Paint our children into the mask:
  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    PaintFrameIntoMask(kid, aClippedFrame, aMaskContext);
  }

  if (maskUsage.shouldGenerateClipMaskLayer) {
    aMaskContext.PopGroupAndBlend();
  } else if (maskUsage.shouldApplyClipPath) {
    aMaskContext.PopClip();
  }

  // Moz2D transforms in the opposite direction to Thebes
  Matrix maskTransfrom = aMaskContext.CurrentMatrix();
  maskTransfrom.Invert();

  if (aExtraMask) {
    ComposeExtraMask(maskDT, aExtraMask, aExtraMasksTransform);
  }

  *aMaskTransform = maskTransfrom;
}

void nsSVGClipPathFrame::PaintFrameIntoMask(nsIFrame* aFrame,
                                            nsIFrame* aClippedFrame,
                                            gfxContext& aTarget) {
  nsSVGDisplayableFrame* frame = do_QueryFrame(aFrame);
  if (!frame) {
    return;
  }

  // The CTM of each frame referencing us can be different.
  frame->NotifySVGChanged(nsSVGDisplayableFrame::TRANSFORM_CHANGED);

  // Children of this clipPath may themselves be clipped.
  nsSVGClipPathFrame* clipPathThatClipsChild;
  // XXX check return value?
  if (SVGObserverUtils::GetAndObserveClipPath(aFrame,
                                              &clipPathThatClipsChild) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return;
  }

  nsSVGUtils::MaskUsage maskUsage;
  nsSVGUtils::DetermineMaskUsage(aFrame, true, maskUsage);
  if (maskUsage.shouldApplyClipPath) {
    clipPathThatClipsChild->ApplyClipPath(aTarget, aClippedFrame,
                                          mMatrixForChildren);
  } else if (maskUsage.shouldGenerateClipMaskLayer) {
    Matrix maskTransform;
    RefPtr<SourceSurface> maskSurface = clipPathThatClipsChild->GetClipMask(
        aTarget, aClippedFrame, mMatrixForChildren, &maskTransform);
    aTarget.PushGroupForBlendBack(gfxContentType::ALPHA, 1.0, maskSurface,
                                  maskTransform);
    // The corresponding PopGroupAndBlend call below will mask the
    // blend using |maskSurface|.
  }

  gfxMatrix toChildsUserSpace = mMatrixForChildren;
  nsIFrame* child = do_QueryFrame(frame);
  nsIContent* childContent = child->GetContent();
  if (childContent->IsSVGElement()) {
    toChildsUserSpace =
        nsSVGUtils::GetTransformMatrixInUserSpace(child) * mMatrixForChildren;
  }

  // clipPath does not result in any image rendering, so we just use a dummy
  // imgDrawingParams instead of requiring our caller to pass one.
  image::imgDrawingParams imgParams;

  // Our children have NS_STATE_SVG_CLIPPATH_CHILD set on them, and
  // SVGGeometryFrame::Render checks for that state bit and paints
  // only the geometry (opaque black) if set.
  frame->PaintSVG(aTarget, toChildsUserSpace, imgParams);

  if (maskUsage.shouldGenerateClipMaskLayer) {
    aTarget.PopGroupAndBlend();
  } else if (maskUsage.shouldApplyClipPath) {
    aTarget.PopClip();
  }
}

already_AddRefed<SourceSurface> nsSVGClipPathFrame::GetClipMask(
    gfxContext& aReferenceContext, nsIFrame* aClippedFrame,
    const gfxMatrix& aMatrix, Matrix* aMaskTransform, SourceSurface* aExtraMask,
    const Matrix& aExtraMasksTransform) {
  IntPoint offset;
  RefPtr<DrawTarget> maskDT = CreateClipMask(aReferenceContext, offset);
  if (!maskDT) {
    return nullptr;
  }

  RefPtr<gfxContext> maskContext = gfxContext::CreateOrNull(maskDT);
  if (!maskContext) {
    gfxCriticalError() << "SVGClipPath context problem " << gfx::hexa(maskDT);
    return nullptr;
  }
  maskContext->SetMatrix(aReferenceContext.CurrentMatrix() *
                         Matrix::Translation(-offset));

  PaintClipMask(*maskContext, aClippedFrame, aMatrix, aMaskTransform,
                aExtraMask, aExtraMasksTransform);

  RefPtr<SourceSurface> surface = maskDT->Snapshot();
  return surface.forget();
}

bool nsSVGClipPathFrame::PointIsInsideClipPath(nsIFrame* aClippedFrame,
                                               const gfxPoint& aPoint) {
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;

  // A clipPath can reference another clipPath, creating a chain of clipPaths
  // that must all be applied.  We re-enter this method for each clipPath in a
  // chain, so we need to protect against reference chain related crashes etc.:
  AutoReferenceChainGuard refChainGuard(this, &mIsBeingProcessed,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    return false;  // Break reference chain
  }

  gfxMatrix matrix = GetClipPathTransform(aClippedFrame);
  if (!matrix.Invert()) {
    return false;
  }
  gfxPoint point = matrix.TransformPoint(aPoint);

  // clipPath elements can themselves be clipped by a different clip path. In
  // that case the other clip path further clips away the element that is being
  // clipped by the original clipPath. If this clipPath is being clipped by a
  // different clip path we need to check if it prevents the original element
  // from receiving events at aPoint:
  nsSVGClipPathFrame* clipPathFrame;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveClipPath(this, &clipPathFrame);
  if (clipPathFrame &&
      !clipPathFrame->PointIsInsideClipPath(aClippedFrame, aPoint)) {
    return false;
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    nsSVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      gfxPoint pointForChild = point;

      gfxMatrix m = nsSVGUtils::GetTransformMatrixInUserSpace(kid);
      if (!m.IsIdentity()) {
        if (!m.Invert()) {
          return false;
        }
        pointForChild = m.TransformPoint(point);
      }
      if (SVGFrame->GetFrameForPoint(pointForChild)) {
        return true;
      }
    }
  }

  return false;
}

bool nsSVGClipPathFrame::IsTrivial(nsSVGDisplayableFrame** aSingleChild) {
  // If the clip path is clipped then it's non-trivial
  if (SVGObserverUtils::GetAndObserveClipPath(this, nullptr) ==
      SVGObserverUtils::eHasRefsAllValid) {
    return false;
  }

  if (aSingleChild) {
    *aSingleChild = nullptr;
  }

  nsSVGDisplayableFrame* foundChild = nullptr;

  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    nsSVGDisplayableFrame* svgChild = do_QueryFrame(kid);
    if (svgChild) {
      // We consider a non-trivial clipPath to be one containing
      // either more than one svg child and/or a svg container
      if (foundChild || svgChild->IsDisplayContainer()) {
        return false;
      }

      // or where the child is itself clipped
      if (SVGObserverUtils::GetAndObserveClipPath(kid, nullptr) ==
          SVGObserverUtils::eHasRefsAllValid) {
        return false;
      }

      foundChild = svgChild;
    }
  }
  if (aSingleChild) {
    *aSingleChild = foundChild;
  }
  return true;
}

bool nsSVGClipPathFrame::IsValid() {
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;

  // A clipPath can reference another clipPath, creating a chain of clipPaths
  // that must all be applied.  We re-enter this method for each clipPath in a
  // chain, so we need to protect against reference chain related crashes etc.:
  AutoReferenceChainGuard refChainGuard(this, &mIsBeingProcessed,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    return false;  // Break reference chain
  }

  if (SVGObserverUtils::GetAndObserveClipPath(this, nullptr) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return false;
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    LayoutFrameType kidType = kid->Type();

    if (kidType == LayoutFrameType::SVGUse) {
      for (nsIFrame* grandKid : kid->PrincipalChildList()) {
        LayoutFrameType grandKidType = grandKid->Type();

        if (grandKidType != LayoutFrameType::SVGGeometry &&
            grandKidType != LayoutFrameType::SVGText) {
          return false;
        }
      }
      continue;
    }

    if (kidType != LayoutFrameType::SVGGeometry &&
        kidType != LayoutFrameType::SVGText) {
      return false;
    }
  }

  return true;
}

nsresult nsSVGClipPathFrame::AttributeChanged(int32_t aNameSpaceID,
                                              nsAtom* aAttribute,
                                              int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::transform) {
      SVGObserverUtils::InvalidateDirectRenderingObservers(this);
      nsSVGUtils::NotifyChildrenOfSVGChange(
          this, nsSVGDisplayableFrame::TRANSFORM_CHANGED);
    }
    if (aAttribute == nsGkAtoms::clipPathUnits) {
      SVGObserverUtils::InvalidateDirectRenderingObservers(this);
    }
  }

  return nsSVGContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                               aModType);
}

void nsSVGClipPathFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                              nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::clipPath),
               "Content is not an SVG clipPath!");

  AddStateBits(NS_STATE_SVG_CLIPPATH_CHILD);
  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

gfxMatrix nsSVGClipPathFrame::GetCanvasTM() { return mMatrixForChildren; }

gfxMatrix nsSVGClipPathFrame::GetClipPathTransform(nsIFrame* aClippedFrame) {
  SVGClipPathElement* content = static_cast<SVGClipPathElement*>(GetContent());

  gfxMatrix tm = content->PrependLocalTransformsTo({}, eChildToUserSpace) *
                 nsSVGUtils::GetTransformMatrixInUserSpace(this);

  SVGAnimatedEnumeration* clipPathUnits =
      &content->mEnumAttributes[SVGClipPathElement::CLIPPATHUNITS];

  uint32_t flags = nsSVGUtils::eBBoxIncludeFillGeometry |
                   (aClippedFrame->StyleBorder()->mBoxDecorationBreak ==
                            StyleBoxDecorationBreak::Clone
                        ? nsSVGUtils::eIncludeOnlyCurrentFrameForNonSVGElement
                        : 0);

  return nsSVGUtils::AdjustMatrixForUnits(tm, clipPathUnits, aClippedFrame,
                                          flags);
}

SVGBBox nsSVGClipPathFrame::GetBBoxForClipPathFrame(const SVGBBox& aBBox,
                                                    const gfxMatrix& aMatrix,
                                                    uint32_t aFlags) {
  nsSVGClipPathFrame* clipPathThatClipsClipPath;
  if (SVGObserverUtils::GetAndObserveClipPath(this,
                                              &clipPathThatClipsClipPath) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return SVGBBox();
  }

  nsIContent* node = GetContent()->GetFirstChild();
  SVGBBox unionBBox, tmpBBox;
  for (; node; node = node->GetNextSibling()) {
    SVGElement* svgNode = static_cast<SVGElement*>(node);
    nsIFrame* frame = svgNode->GetPrimaryFrame();
    if (frame) {
      nsSVGDisplayableFrame* svg = do_QueryFrame(frame);
      if (svg) {
        gfxMatrix matrix =
            nsSVGUtils::GetTransformMatrixInUserSpace(frame) * aMatrix;
        tmpBBox = svg->GetBBoxContribution(mozilla::gfx::ToMatrix(matrix),
                                           nsSVGUtils::eBBoxIncludeFill);
        nsSVGClipPathFrame* clipPathFrame;
        if (SVGObserverUtils::GetAndObserveClipPath(frame, &clipPathFrame) !=
                SVGObserverUtils::eHasRefsSomeInvalid &&
            clipPathFrame) {
          tmpBBox =
              clipPathFrame->GetBBoxForClipPathFrame(tmpBBox, aMatrix, aFlags);
        }
        if (!(aFlags & nsSVGUtils::eDoNotClipToBBoxOfContentInsideClipPath)) {
          tmpBBox.Intersect(aBBox);
        }
        unionBBox.UnionEdges(tmpBBox);
      }
    }
  }

  if (clipPathThatClipsClipPath) {
    tmpBBox = clipPathThatClipsClipPath->GetBBoxForClipPathFrame(aBBox, aMatrix,
                                                                 aFlags);
    unionBBox.Intersect(tmpBBox);
  }
  return unionBBox;
}

bool nsSVGClipPathFrame::IsSVGTransformed(Matrix* aOwnTransforms,
                                          Matrix* aFromParentTransforms) const {
  auto e = static_cast<SVGElement const*>(GetContent());
  Matrix m = ToMatrix(e->PrependLocalTransformsTo({}, eUserSpaceToParent));

  if (m.IsIdentity()) {
    return false;
  }

  if (aOwnTransforms) {
    *aOwnTransforms = m;
  }

  return true;
}
