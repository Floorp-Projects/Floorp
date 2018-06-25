/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfx2DGlue.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/SVGAnimatedTransformList.h"
#include "mozilla/dom/SVGGraphicsElementBinding.h"
#include "mozilla/dom/SVGTransformableElement.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsSVGDisplayableFrame.h"
#include "mozilla/dom/SVGRect.h"
#include "nsSVGUtils.h"
#include "SVGContentUtils.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

already_AddRefed<SVGAnimatedTransformList>
SVGTransformableElement::Transform()
{
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the SVGAnimatedTransformList if it hasn't already done so:
  return SVGAnimatedTransformList::GetDOMWrapper(
           GetAnimatedTransformList(DO_ALLOCATE), this);

}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGTransformableElement::IsAttributeMapped(const nsAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sGraphicsMap
  };

  return FindAttributeDependence(name, map) ||
    nsSVGElement::IsAttributeMapped(name);
}

nsChangeHint
SVGTransformableElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                                int32_t aModType) const
{
  nsChangeHint retval =
    nsSVGElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::transform ||
      aAttribute == nsGkAtoms::mozAnimateMotionDummyAttr) {
    nsIFrame* frame =
      const_cast<SVGTransformableElement*>(this)->GetPrimaryFrame();
    retval |= nsChangeHint_InvalidateRenderingObservers;
    if (!frame || (frame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
      return retval;
    }

    bool isAdditionOrRemoval = false;
    if (aModType == MutationEvent_Binding::ADDITION ||
        aModType == MutationEvent_Binding::REMOVAL) {
      isAdditionOrRemoval = true;
    } else {
      MOZ_ASSERT(aModType == MutationEvent_Binding::MODIFICATION,
                 "Unknown modification type.");
      if (!mTransforms ||
          !mTransforms->HasTransform()) {
        // New value is empty, treat as removal.
        isAdditionOrRemoval = true;
      } else if (mTransforms->RequiresFrameReconstruction()) {
        // Old value was empty, treat as addition.
        isAdditionOrRemoval = true;
      }
    }

    if (isAdditionOrRemoval) {
      // Reconstruct the frame tree to handle stacking context changes:
      retval |= nsChangeHint_ReconstructFrame;
    } else {
      // We just assume the old and new transforms are different.
      retval |= nsChangeHint_UpdatePostTransformOverflow |
                nsChangeHint_UpdateTransformLayer;
    }
  }
  return retval;
}

bool
SVGTransformableElement::IsEventAttributeNameInternal(nsAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

gfxMatrix
SVGTransformableElement::PrependLocalTransformsTo(const gfxMatrix& aMatrix,
                                                  SVGTransformTypes aWhich) const
{
  if (aWhich == eChildToUserSpace) {
    // We don't have any eUserSpaceToParent transforms. (Sub-classes that do
    // must override this function and handle that themselves.)
    return aMatrix;
  }
  return GetUserToParentTransform(mAnimateMotionTransform, mTransforms) * aMatrix;
}

const gfx::Matrix*
SVGTransformableElement::GetAnimateMotionTransform() const
{
  return mAnimateMotionTransform.get();
}

void
SVGTransformableElement::SetAnimateMotionTransform(const gfx::Matrix* aMatrix)
{
  if ((!aMatrix && !mAnimateMotionTransform) ||
      (aMatrix && mAnimateMotionTransform && aMatrix->FuzzyEquals(*mAnimateMotionTransform))) {
    return;
  }
  bool transformSet = mTransforms && mTransforms->IsExplicitlySet();
  bool prevSet = mAnimateMotionTransform || transformSet;
  mAnimateMotionTransform = aMatrix ? new gfx::Matrix(*aMatrix) : nullptr;
  bool nowSet = mAnimateMotionTransform || transformSet;
  int32_t modType;
  if (prevSet && !nowSet) {
    modType = MutationEvent_Binding::REMOVAL;
  } else if(!prevSet && nowSet) {
    modType = MutationEvent_Binding::ADDITION;
  } else {
    modType = MutationEvent_Binding::MODIFICATION;
  }
  DidAnimateTransformList(modType);
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    // If the result of this transform and any other transforms on this frame
    // is the identity matrix, then DoApplyRenderingChangeToTree won't handle
    // our nsChangeHint_UpdateTransformLayer hint since aFrame->IsTransformed()
    // will return false. That's fine, but we still need to schedule a repaint,
    // and that won't otherwise happen. Since it's cheap to call SchedulePaint,
    // we don't bother to check IsTransformed().
    frame->SchedulePaint();
  }
}

nsSVGAnimatedTransformList*
SVGTransformableElement::GetAnimatedTransformList(uint32_t aFlags)
{
  if (!mTransforms && (aFlags & DO_ALLOCATE)) {
    mTransforms = new nsSVGAnimatedTransformList();
  }
  return mTransforms;
}

nsSVGElement*
SVGTransformableElement::GetNearestViewportElement()
{
  return SVGContentUtils::GetNearestViewportElement(this);
}

nsSVGElement*
SVGTransformableElement::GetFarthestViewportElement()
{
  return SVGContentUtils::GetOuterSVGElement(this);
}

already_AddRefed<SVGIRect>
SVGTransformableElement::GetBBox(const SVGBoundingBoxOptions& aOptions,
                                 ErrorResult& rv)
{
  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);

  if (!frame || (frame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsSVGDisplayableFrame* svgframe = do_QueryFrame(frame);
  if (!svgframe) {
    rv.Throw(NS_ERROR_NOT_IMPLEMENTED); // XXX: outer svg
    return nullptr;
  }

  if (!NS_SVGNewGetBBoxEnabled()) {
    return NS_NewSVGRect(this, ToRect(nsSVGUtils::GetBBox(frame,
                                      nsSVGUtils::eBBoxIncludeFillGeometry |
                                      nsSVGUtils::eUseUserSpaceOfUseElement)));
  } else {
    uint32_t flags = 0;
    if (aOptions.mFill) {
      flags |= nsSVGUtils::eBBoxIncludeFill;
    }
    if (aOptions.mStroke) {
      flags |= nsSVGUtils::eBBoxIncludeStroke;
    }
    if (aOptions.mMarkers) {
      flags |= nsSVGUtils::eBBoxIncludeMarkers;
    }
    if (aOptions.mClipped) {
      flags |= nsSVGUtils::eBBoxIncludeClipped;
    }
    if (flags == 0) {
      return NS_NewSVGRect(this,0,0,0,0);
    }
    if (flags == nsSVGUtils::eBBoxIncludeMarkers ||
        flags == nsSVGUtils::eBBoxIncludeClipped) {
      flags |= nsSVGUtils::eBBoxIncludeFill;
    }
    flags |= nsSVGUtils::eUseUserSpaceOfUseElement;
    return NS_NewSVGRect(this, ToRect(nsSVGUtils::GetBBox(frame, flags)));
  }
}

already_AddRefed<SVGMatrix>
SVGTransformableElement::GetCTM()
{
  nsIDocument* currentDoc = GetComposedDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(FlushType::Layout);
  }
  gfx::Matrix m = SVGContentUtils::GetCTM(this, false);
  RefPtr<SVGMatrix> mat = m.IsSingular() ? nullptr : new SVGMatrix(ThebesMatrix(m));
  return mat.forget();
}

already_AddRefed<SVGMatrix>
SVGTransformableElement::GetScreenCTM()
{
  nsIDocument* currentDoc = GetComposedDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(FlushType::Layout);
  }
  gfx::Matrix m = SVGContentUtils::GetCTM(this, true);
  RefPtr<SVGMatrix> mat = m.IsSingular() ? nullptr : new SVGMatrix(ThebesMatrix(m));
  return mat.forget();
}

already_AddRefed<SVGMatrix>
SVGTransformableElement::GetTransformToElement(SVGGraphicsElement& aElement,
                                               ErrorResult& rv)
{
  // the easiest way to do this (if likely to increase rounding error):
  RefPtr<SVGMatrix> ourScreenCTM = GetScreenCTM();
  RefPtr<SVGMatrix> targetScreenCTM = aElement.GetScreenCTM();
  if (!ourScreenCTM || !targetScreenCTM) {
    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  RefPtr<SVGMatrix> tmp = targetScreenCTM->Inverse(rv);
  if (rv.Failed()) return nullptr;

  RefPtr<SVGMatrix> mat = tmp->Multiply(*ourScreenCTM);
  return mat.forget();
}

/* static */ gfxMatrix
SVGTransformableElement::GetUserToParentTransform(
                           const gfx::Matrix* aAnimateMotionTransform,
                           const nsSVGAnimatedTransformList* aTransforms)
{
  gfxMatrix result;

  if (aAnimateMotionTransform) {
    result.PreMultiply(ThebesMatrix(*aAnimateMotionTransform));
  }

  if (aTransforms) {
    result.PreMultiply(aTransforms->GetAnimValue().GetConsolidationMatrix());
  }

  return result;
}

} // namespace dom
} // namespace mozilla

