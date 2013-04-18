/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimatedTransformList.h"
#include "mozilla/dom/SVGTransformableElement.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsContentUtils.h"
#include "nsIDOMMutationEvent.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "mozilla/dom/SVGRect.h"
#include "nsSVGUtils.h"
#include "SVGContentUtils.h"

namespace mozilla {
namespace dom {

already_AddRefed<SVGAnimatedTransformList>
SVGTransformableElement::Transform()
{
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the SVGAnimatedTransformList if it hasn't already done so:
  return SVGAnimatedTransformList::GetDOMWrapper(
           GetAnimatedTransformList(DO_ALLOCATE), this).get();

}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGTransformableElement::IsAttributeMapped(const nsIAtom* name) const
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
SVGTransformableElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                                int32_t aModType) const
{
  nsChangeHint retval =
    nsSVGElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::transform ||
      aAttribute == nsGkAtoms::mozAnimateMotionDummyAttr) {
    // We add nsChangeHint_UpdateOverflow so that nsFrame::UpdateOverflow()
    // will be called on us and our ancestors.
    nsIFrame* frame =
      const_cast<SVGTransformableElement*>(this)->GetPrimaryFrame();
    if (!frame || (frame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
      return retval; // no change
    }
    if (aModType == nsIDOMMutationEvent::ADDITION ||
        aModType == nsIDOMMutationEvent::REMOVAL ||
        (aModType == nsIDOMMutationEvent::MODIFICATION &&
         !(mTransforms && mTransforms->HasTransform()))) {
      // Reconstruct the frame tree to handle stacking context changes:
      NS_UpdateHint(retval, nsChangeHint_ReconstructFrame);
    } else {
      NS_ABORT_IF_FALSE(aModType == nsIDOMMutationEvent::MODIFICATION,
                        "Unknown modification type.");
      // We just assume the old and new transforms are different.
      NS_UpdateHint(retval, NS_CombineHint(nsChangeHint_UpdateOverflow,
                                           nsChangeHint_UpdateTransformLayer));
    }
  }
  return retval;
}

bool
SVGTransformableElement::IsEventAttributeName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

gfxMatrix
SVGTransformableElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                                                  TransformTypes aWhich) const
{
  NS_ABORT_IF_FALSE(aWhich != eChildToUserSpace || aMatrix.IsIdentity(),
                    "Skipping eUserSpaceToParent transforms makes no sense");

  gfxMatrix result(aMatrix);

  if (aWhich == eChildToUserSpace) {
    // We don't have anything to prepend.
    // eChildToUserSpace is not the common case, which is why we return
    // 'result' to benefit from NRVO rather than returning aMatrix before
    // creating 'result'.
    return result;
  }

  NS_ABORT_IF_FALSE(aWhich == eAllTransforms || aWhich == eUserSpaceToParent,
                    "Unknown TransformTypes");

  // animateMotion's resulting transform is supposed to apply *on top of*
  // any transformations from the |transform| attribute. So since we're
  // PRE-multiplying, we need to apply the animateMotion transform *first*.
  if (mAnimateMotionTransform) {
    result.PreMultiply(*mAnimateMotionTransform);
  }

  if (mTransforms) {
    result.PreMultiply(mTransforms->GetAnimValue().GetConsolidationMatrix());
  }

  return result;
}

const gfxMatrix*
SVGTransformableElement::GetAnimateMotionTransform() const
{
  return mAnimateMotionTransform.get();
}

void
SVGTransformableElement::SetAnimateMotionTransform(const gfxMatrix* aMatrix)
{
  if ((!aMatrix && !mAnimateMotionTransform) ||
      (aMatrix && mAnimateMotionTransform && *aMatrix == *mAnimateMotionTransform)) {
    return;
  }
  mAnimateMotionTransform = aMatrix ? new gfxMatrix(*aMatrix) : nullptr;
  DidAnimateTransformList();
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
SVGTransformableElement::GetBBox(ErrorResult& rv)
{
  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);

  if (!frame || (frame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsISVGChildFrame* svgframe = do_QueryFrame(frame);
  if (!svgframe) {
    rv.Throw(NS_ERROR_NOT_IMPLEMENTED); // XXX: outer svg
    return nullptr;
  }

  return NS_NewSVGRect(nsSVGUtils::GetBBox(frame));
}

already_AddRefed<SVGMatrix>
SVGTransformableElement::GetCTM()
{
  nsIDocument* currentDoc = GetCurrentDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(Flush_Layout);
  }
  gfxMatrix m = SVGContentUtils::GetCTM(this, false);
  nsRefPtr<SVGMatrix> mat = m.IsSingular() ? nullptr : new SVGMatrix(m);
  return mat.forget();
}

already_AddRefed<SVGMatrix>
SVGTransformableElement::GetScreenCTM()
{
  nsIDocument* currentDoc = GetCurrentDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(Flush_Layout);
  }
  gfxMatrix m = SVGContentUtils::GetCTM(this, true);
  nsRefPtr<SVGMatrix> mat = m.IsSingular() ? nullptr : new SVGMatrix(m);
  return mat.forget();
}

already_AddRefed<SVGMatrix>
SVGTransformableElement::GetTransformToElement(SVGGraphicsElement& aElement,
                                               ErrorResult& rv)
{
  // the easiest way to do this (if likely to increase rounding error):
  nsRefPtr<SVGMatrix> ourScreenCTM = GetScreenCTM();
  nsRefPtr<SVGMatrix> targetScreenCTM = aElement.GetScreenCTM();
  if (!ourScreenCTM || !targetScreenCTM) {
    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  nsRefPtr<SVGMatrix> tmp = targetScreenCTM->Inverse(rv);
  if (rv.Failed()) return nullptr;

  nsRefPtr<SVGMatrix> mat = tmp->Multiply(*ourScreenCTM);
  return mat.forget();
}

} // namespace dom
} // namespace mozilla

