/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGClipPathFrame.h"

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "ImgDrawResult.h"
#include "gfxContext.h"
#include "mozilla/PresShell.h"
#include "mozilla/SVGGeometryFrame.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/SVGClipPathElement.h"
#include "mozilla/dom/SVGGeometryElement.h"
#include "nsGkAtoms.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

//----------------------------------------------------------------------
// Implementation

nsIFrame* NS_NewSVGClipPathFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGClipPathFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGClipPathFrame)

void SVGClipPathFrame::ApplyClipPath(gfxContext& aContext,
                                     nsIFrame* aClippedFrame,
                                     const gfxMatrix& aMatrix) {
  MOZ_ASSERT(IsTrivial(), "Caller needs to use GetClipMask");

  const DrawTarget* drawTarget = aContext.GetDrawTarget();

  // No need for AutoReferenceChainGuard since simple clip paths by definition
  // don't reference another clip path.

  // Restore current transform after applying clip path:
  gfxContextMatrixAutoSaveRestore autoRestore(&aContext);

  RefPtr<Path> clipPath;

  ISVGDisplayableFrame* singleClipPathChild = nullptr;
  IsTrivial(&singleClipPathChild);

  if (singleClipPathChild) {
    SVGGeometryFrame* pathFrame = do_QueryFrame(singleClipPathChild);
    if (pathFrame && pathFrame->StyleVisibility()->IsVisible()) {
      SVGGeometryElement* pathElement =
          static_cast<SVGGeometryElement*>(pathFrame->GetContent());

      gfxMatrix toChildsUserSpace =
          SVGUtils::GetTransformMatrixInUserSpace(pathFrame) *
          (GetClipPathTransform(aClippedFrame) * aMatrix);

      gfxMatrix newMatrix = aContext.CurrentMatrixDouble()
                                .PreMultiply(toChildsUserSpace)
                                .NudgeToIntegers();
      if (!newMatrix.IsSingular()) {
        aContext.SetMatrixDouble(newMatrix);
        FillRule clipRule =
            SVGUtils::ToFillRule(pathFrame->StyleSVG()->mClipRule);
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

static void ComposeExtraMask(DrawTarget* aTarget, SourceSurface* aExtraMask) {
  MOZ_ASSERT(aExtraMask);

  Matrix origin = aTarget->GetTransform();
  aTarget->SetTransform(Matrix());
  aTarget->MaskSurface(ColorPattern(DeviceColor(0.0, 0.0, 0.0, 1.0)),
                       aExtraMask, Point(0, 0),
                       DrawOptions(1.0, CompositionOp::OP_IN));
  aTarget->SetTransform(origin);
}

void SVGClipPathFrame::PaintClipMask(gfxContext& aMaskContext,
                                     nsIFrame* aClippedFrame,
                                     const gfxMatrix& aMatrix,
                                     SourceSurface* aExtraMask) {
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
  SVGClipPathFrame* clipPathThatClipsClipPath;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveClipPath(this, &clipPathThatClipsClipPath);
  SVGUtils::MaskUsage maskUsage;
  SVGUtils::DetermineMaskUsage(this, true, maskUsage);

  if (maskUsage.shouldApplyClipPath) {
    clipPathThatClipsClipPath->ApplyClipPath(aMaskContext, aClippedFrame,
                                             aMatrix);
  } else if (maskUsage.shouldGenerateClipMaskLayer) {
    RefPtr<SourceSurface> maskSurface = clipPathThatClipsClipPath->GetClipMask(
        aMaskContext, aClippedFrame, aMatrix);
    // We want the mask to be untransformed so use the inverse of the current
    // transform as the maskTransform to compensate.
    Matrix maskTransform = aMaskContext.CurrentMatrix();
    maskTransform.Invert();
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

  if (aExtraMask) {
    ComposeExtraMask(maskDT, aExtraMask);
  }
}

void SVGClipPathFrame::PaintFrameIntoMask(nsIFrame* aFrame,
                                          nsIFrame* aClippedFrame,
                                          gfxContext& aTarget) {
  ISVGDisplayableFrame* frame = do_QueryFrame(aFrame);
  if (!frame) {
    return;
  }

  // The CTM of each frame referencing us can be different.
  frame->NotifySVGChanged(ISVGDisplayableFrame::TRANSFORM_CHANGED);

  // Children of this clipPath may themselves be clipped.
  SVGClipPathFrame* clipPathThatClipsChild;
  // XXX check return value?
  if (SVGObserverUtils::GetAndObserveClipPath(aFrame,
                                              &clipPathThatClipsChild) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return;
  }

  SVGUtils::MaskUsage maskUsage;
  SVGUtils::DetermineMaskUsage(aFrame, true, maskUsage);
  if (maskUsage.shouldApplyClipPath) {
    clipPathThatClipsChild->ApplyClipPath(aTarget, aClippedFrame,
                                          mMatrixForChildren);
  } else if (maskUsage.shouldGenerateClipMaskLayer) {
    RefPtr<SourceSurface> maskSurface = clipPathThatClipsChild->GetClipMask(
        aTarget, aClippedFrame, mMatrixForChildren);

    // We want the mask to be untransformed so use the inverse of the current
    // transform as the maskTransform to compensate.
    Matrix maskTransform = aTarget.CurrentMatrix();
    maskTransform.Invert();
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
        SVGUtils::GetTransformMatrixInUserSpace(child) * mMatrixForChildren;
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

already_AddRefed<SourceSurface> SVGClipPathFrame::GetClipMask(
    gfxContext& aReferenceContext, nsIFrame* aClippedFrame,
    const gfxMatrix& aMatrix, SourceSurface* aExtraMask) {
  RefPtr<DrawTarget> maskDT =
      aReferenceContext.GetDrawTarget()->CreateClippedDrawTarget(
          Rect(), SurfaceFormat::A8);
  if (!maskDT) {
    return nullptr;
  }

  RefPtr<gfxContext> maskContext =
      gfxContext::CreatePreservingTransformOrNull(maskDT);
  if (!maskContext) {
    gfxCriticalError() << "SVGClipPath context problem " << gfx::hexa(maskDT);
    return nullptr;
  }

  PaintClipMask(*maskContext, aClippedFrame, aMatrix, aExtraMask);

  RefPtr<SourceSurface> surface = maskDT->Snapshot();
  return surface.forget();
}

bool SVGClipPathFrame::PointIsInsideClipPath(nsIFrame* aClippedFrame,
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
  SVGClipPathFrame* clipPathFrame;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveClipPath(this, &clipPathFrame);
  if (clipPathFrame &&
      !clipPathFrame->PointIsInsideClipPath(aClippedFrame, aPoint)) {
    return false;
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    ISVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      gfxPoint pointForChild = point;

      gfxMatrix m = SVGUtils::GetTransformMatrixInUserSpace(kid);
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

bool SVGClipPathFrame::IsTrivial(ISVGDisplayableFrame** aSingleChild) {
  // If the clip path is clipped then it's non-trivial
  if (SVGObserverUtils::GetAndObserveClipPath(this, nullptr) ==
      SVGObserverUtils::eHasRefsAllValid) {
    return false;
  }

  if (aSingleChild) {
    *aSingleChild = nullptr;
  }

  ISVGDisplayableFrame* foundChild = nullptr;

  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    ISVGDisplayableFrame* svgChild = do_QueryFrame(kid);
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

bool SVGClipPathFrame::IsValid() {
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

nsresult SVGClipPathFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::transform) {
      SVGObserverUtils::InvalidateDirectRenderingObservers(this);
      SVGUtils::NotifyChildrenOfSVGChange(
          this, ISVGDisplayableFrame::TRANSFORM_CHANGED);
    }
    if (aAttribute == nsGkAtoms::clipPathUnits) {
      SVGObserverUtils::InvalidateDirectRenderingObservers(this);
    }
  }

  return SVGContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
}

void SVGClipPathFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::clipPath),
               "Content is not an SVG clipPath!");

  AddStateBits(NS_STATE_SVG_CLIPPATH_CHILD);
  AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
  SVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

gfxMatrix SVGClipPathFrame::GetCanvasTM() { return mMatrixForChildren; }

gfxMatrix SVGClipPathFrame::GetClipPathTransform(nsIFrame* aClippedFrame) {
  SVGClipPathElement* content = static_cast<SVGClipPathElement*>(GetContent());

  gfxMatrix tm = content->PrependLocalTransformsTo({}, eChildToUserSpace) *
                 SVGUtils::GetTransformMatrixInUserSpace(this);

  SVGAnimatedEnumeration* clipPathUnits =
      &content->mEnumAttributes[SVGClipPathElement::CLIPPATHUNITS];

  uint32_t flags = SVGUtils::eBBoxIncludeFillGeometry |
                   (aClippedFrame->StyleBorder()->mBoxDecorationBreak ==
                            StyleBoxDecorationBreak::Clone
                        ? SVGUtils::eIncludeOnlyCurrentFrameForNonSVGElement
                        : 0);

  return SVGUtils::AdjustMatrixForUnits(tm, clipPathUnits, aClippedFrame,
                                        flags);
}

SVGBBox SVGClipPathFrame::GetBBoxForClipPathFrame(const SVGBBox& aBBox,
                                                  const gfxMatrix& aMatrix,
                                                  uint32_t aFlags) {
  SVGClipPathFrame* clipPathThatClipsClipPath;
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
      ISVGDisplayableFrame* svg = do_QueryFrame(frame);
      if (svg) {
        gfxMatrix matrix =
            SVGUtils::GetTransformMatrixInUserSpace(frame) * aMatrix;
        tmpBBox = svg->GetBBoxContribution(gfx::ToMatrix(matrix),
                                           SVGUtils::eBBoxIncludeFill);
        SVGClipPathFrame* clipPathFrame;
        if (SVGObserverUtils::GetAndObserveClipPath(frame, &clipPathFrame) !=
                SVGObserverUtils::eHasRefsSomeInvalid &&
            clipPathFrame) {
          tmpBBox =
              clipPathFrame->GetBBoxForClipPathFrame(tmpBBox, aMatrix, aFlags);
        }
        if (!(aFlags & SVGUtils::eDoNotClipToBBoxOfContentInsideClipPath)) {
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

bool SVGClipPathFrame::IsSVGTransformed(Matrix* aOwnTransforms,
                                        Matrix* aFromParentTransforms) const {
  const auto* e = static_cast<SVGElement const*>(GetContent());
  Matrix m = ToMatrix(e->PrependLocalTransformsTo({}, eUserSpaceToParent));

  if (m.IsIdentity()) {
    return false;
  }

  if (aOwnTransforms) {
    *aOwnTransforms = m;
  }

  return true;
}

}  // namespace mozilla
