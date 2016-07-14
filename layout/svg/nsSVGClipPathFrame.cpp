/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGClipPathFrame.h"

// Keep others in (case-insensitive) order:
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

already_AddRefed<SourceSurface>
nsSVGClipPathFrame::GetClipMask(gfxContext& aReferenceContext,
                                nsIFrame* aClippedFrame,
                                const gfxMatrix& aMatrix,
                                Matrix* aMaskTransform,
                                SourceSurface* aExtraMask,
                                const Matrix& aExtraMasksTransform,
                                DrawResult* aResult)
{
  MOZ_ASSERT(!IsTrivial(), "Caller needs to use ApplyClipPath");

  if (aResult) {
    *aResult = DrawResult::SUCCESS;
  }
  DrawTarget& aReferenceDT = *aReferenceContext.GetDrawTarget();

  // A clipPath can reference another clipPath.  We re-enter this method for
  // each clipPath in a reference chain, so here we limit chain length:
  static int16_t sRefChainLengthCounter = AutoReferenceLimiter::notReferencing;
  AutoReferenceLimiter
    refChainLengthLimiter(&sRefChainLengthCounter,
                          MAX_SVG_CLIP_PATH_REFERENCE_CHAIN_LENGTH);
  if (!refChainLengthLimiter.Reference()) {
    return nullptr; // Reference chain is too long!
  }

  // And to prevent reference loops we check that this clipPath only appears
  // once in the reference chain (if any) that we're currently processing:
  AutoReferenceLimiter refLoopDetector(&mReferencing, 1);
  if (!refLoopDetector.Reference()) {
    return nullptr; // Reference loop!
  }

  IntRect devSpaceClipExtents;
  {
    gfxContextMatrixAutoSaveRestore autoRestoreMatrix(&aReferenceContext);

    aReferenceContext.SetMatrix(gfxMatrix());
    gfxRect rect = aReferenceContext.GetClipExtents();
    devSpaceClipExtents = RoundedOut(ToRect(rect));
    if (devSpaceClipExtents.IsEmpty()) {
      // We don't need to create a mask surface, all drawing is clipped anyway.
      return nullptr;
    }
  }

  RefPtr<DrawTarget> maskDT =
    aReferenceDT.CreateSimilarDrawTarget(devSpaceClipExtents.Size(),
                                         SurfaceFormat::A8);

  gfxMatrix mat = aReferenceContext.CurrentMatrix() *
                    gfxMatrix::Translation(-devSpaceClipExtents.TopLeft());

  // Paint this clipPath's contents into maskDT:
  {
    RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(maskDT);
    if (!ctx) {
      gfxCriticalError() << "SVGClipPath context problem " << gfx::hexa(maskDT);
      return nullptr;
    }
    ctx->SetMatrix(mat);

    // We need to set mMatrixForChildren here so that under the PaintSVG calls
    // on our children (below) our GetCanvasTM() method will return the correct
    // transform.
    mMatrixForChildren = GetClipPathTransform(aClippedFrame) * aMatrix;

    // Check if this clipPath is itself clipped by another clipPath:
    nsSVGClipPathFrame* clipPathThatClipsClipPath =
      nsSVGEffects::GetEffectProperties(this).GetClipPathFrame(nullptr);
    bool clippingOfClipPathRequiredMasking;
    if (clipPathThatClipsClipPath) {
      ctx->Save();
      clippingOfClipPathRequiredMasking = !clipPathThatClipsClipPath->IsTrivial();
      if (!clippingOfClipPathRequiredMasking) {
        clipPathThatClipsClipPath->ApplyClipPath(*ctx, aClippedFrame, aMatrix);
      } else {
        Matrix maskTransform;
        RefPtr<SourceSurface> mask =
          clipPathThatClipsClipPath->GetClipMask(*ctx, aClippedFrame,
                                                 aMatrix, &maskTransform);
        ctx->PushGroupForBlendBack(gfxContentType::ALPHA, 1.0,
                                   mask, maskTransform);
        // The corresponding PopGroupAndBlend call below will mask the
        // blend using |mask|.
      }
    }

    // Paint our children into the mask:
    for (nsIFrame* kid = mFrames.FirstChild(); kid;
         kid = kid->GetNextSibling()) {
      nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
      if (SVGFrame) {
        // The CTM of each frame referencing us can be different.
        SVGFrame->NotifySVGChanged(nsISVGChildFrame::TRANSFORM_CHANGED);

        bool isOK = true;
        // Children of this clipPath may themselves be clipped.
        nsSVGClipPathFrame *clipPathThatClipsChild =
          nsSVGEffects::GetEffectProperties(kid).GetClipPathFrame(&isOK);
        if (!isOK) {
          continue;
        }

        bool childsClipPathRequiresMasking;

        if (clipPathThatClipsChild) {
          childsClipPathRequiresMasking = !clipPathThatClipsChild->IsTrivial();
          ctx->Save();
          if (!childsClipPathRequiresMasking) {
            clipPathThatClipsChild->ApplyClipPath(*ctx, aClippedFrame, aMatrix);
          } else {
            Matrix maskTransform;
            RefPtr<SourceSurface> mask =
              clipPathThatClipsChild->GetClipMask(*ctx, aClippedFrame,
                                                  aMatrix, &maskTransform);
            ctx->PushGroupForBlendBack(gfxContentType::ALPHA, 1.0,
                                       mask, maskTransform);
            // The corresponding PopGroupAndBlend call below will mask the
            // blend using |mask|.
          }
        }

        gfxMatrix toChildsUserSpace = mMatrixForChildren;
        nsIFrame* child = do_QueryFrame(SVGFrame);
        nsIContent* childContent = child->GetContent();
        if (childContent->IsSVGElement()) {
          toChildsUserSpace =
            static_cast<const nsSVGElement*>(childContent)->
              PrependLocalTransformsTo(mMatrixForChildren, eUserSpaceToParent);
        }

        // Our children have NS_STATE_SVG_CLIPPATH_CHILD set on them, and
        // nsSVGPathGeometryFrame::Render checks for that state bit and paints
        // only the geometry (opaque black) if set.
        DrawResult result = SVGFrame->PaintSVG(*ctx, toChildsUserSpace);
        if (aResult) {
          *aResult &= result;
        }

        if (clipPathThatClipsChild) {
          if (childsClipPathRequiresMasking) {
            ctx->PopGroupAndBlend();
          }
          ctx->Restore();
        }
      }
    }


    if (clipPathThatClipsClipPath) {
      if (clippingOfClipPathRequiredMasking) {
        ctx->PopGroupAndBlend();
      }
      ctx->Restore();
    }
  }

  // Moz2D transforms in the opposite direction to Thebes
  mat.Invert();

  if (aExtraMask) {
    // We could potentially due this more efficiently with OPERATOR_IN
    // but that operator does not work well on CG or D2D
    RefPtr<SourceSurface> currentMask = maskDT->Snapshot();
    Matrix transform = maskDT->GetTransform();
    maskDT->SetTransform(Matrix());
    maskDT->ClearRect(Rect(0, 0,
                           devSpaceClipExtents.width,
                           devSpaceClipExtents.height));
    maskDT->SetTransform(aExtraMasksTransform * transform);
    // draw currentMask with the inverse of the transform that we just so that
    // it ends up in the same spot with aExtraMask transformed by aExtraMasksTransform
    maskDT->MaskSurface(SurfacePattern(currentMask, ExtendMode::CLAMP, aExtraMasksTransform.Inverse() * ToMatrix(mat)),
                        aExtraMask,
                        Point(0, 0));
  }

  *aMaskTransform = ToMatrix(mat);
  return maskDT->Snapshot();
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
    nsSVGEffects::GetEffectProperties(this).GetClipPathFrame(nullptr);
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
  if (nsSVGEffects::GetEffectProperties(this).GetClipPathFrame(nullptr))
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
      if (nsSVGEffects::GetEffectProperties(kid).GetClipPathFrame(nullptr))
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

  bool isOK = true;
  nsSVGEffects::GetEffectProperties(this).GetClipPathFrame(&isOK);
  if (!isOK) {
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
        bool isOK = true;
        nsSVGClipPathFrame *clipPathFrame = 
                              effectProperties.GetClipPathFrame(&isOK);
        if (clipPathFrame && isOK) {
          tmpBBox = clipPathFrame->GetBBoxForClipPathFrame(tmpBBox, aMatrix);
        } 
        tmpBBox.Intersect(aBBox);
        unionBBox.UnionEdges(tmpBBox);
      }
    }
  }
  nsSVGEffects::EffectProperties props = 
    nsSVGEffects::GetEffectProperties(this);    
  if (props.mClipPath) {
    bool isOK = true;
    nsSVGClipPathFrame *clipPathFrame = props.GetClipPathFrame(&isOK);
    if (clipPathFrame && isOK) {
      tmpBBox = clipPathFrame->GetBBoxForClipPathFrame(aBBox, aMatrix);                                                       
      unionBBox.Intersect(tmpBBox);
    } else if (!isOK) {
      unionBBox = SVGBBox();
    }
  }
  return unionBBox;
}
