/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGGraphicElement.h"
#include "nsSVGSVGElement.h"
#include "DOMSVGAnimatedTransformList.h"
#include "DOMSVGMatrix.h"
#include "nsGkAtoms.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMutationEvent.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "nsSVGUtils.h"
#include "nsError.h"
#include "nsSVGRect.h"
#include "nsContentUtils.h"
#include "mozilla/dom/SVGGraphicsElementBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

JSObject*
nsSVGGraphicElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGGraphicsElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGGraphicElement, nsSVGGraphicElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGGraphicElement, nsSVGGraphicElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGGraphicElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTests)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGraphicElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGGraphicElement::nsSVGGraphicElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGGraphicElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGGraphicElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sGraphicsMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGGraphicElementBase::IsAttributeMapped(name);
}

nsChangeHint
nsSVGGraphicElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            int32_t aModType) const
{
  nsChangeHint retval =
    nsSVGGraphicElementBase::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::transform ||
      aAttribute == nsGkAtoms::mozAnimateMotionDummyAttr) {
    // We add nsChangeHint_UpdateOverflow so that nsFrame::UpdateOverflow()
    // will be called on us and our ancestors.
    nsIFrame* frame =
      const_cast<nsSVGGraphicElement*>(this)->GetPrimaryFrame();
    if (!frame || (frame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
      return retval; // no change
    }
    if (aModType == nsIDOMMutationEvent::ADDITION ||
        aModType == nsIDOMMutationEvent::REMOVAL) {
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
nsSVGGraphicElement::IsEventAttributeName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

gfxMatrix
nsSVGGraphicElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
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
nsSVGGraphicElement::GetAnimateMotionTransform() const
{
  return mAnimateMotionTransform.get();
}

void
nsSVGGraphicElement::SetAnimateMotionTransform(const gfxMatrix* aMatrix)
{
  if ((!aMatrix && !mAnimateMotionTransform) ||
      (aMatrix && mAnimateMotionTransform && *aMatrix == *mAnimateMotionTransform)) {
    return;
  }
  mAnimateMotionTransform = aMatrix ? new gfxMatrix(*aMatrix) : nullptr;
  DidAnimateTransformList();
}

SVGAnimatedTransformList*
nsSVGGraphicElement::GetAnimatedTransformList(uint32_t aFlags)
{
  if (!mTransforms && (aFlags & DO_ALLOCATE)) {
    mTransforms = new SVGAnimatedTransformList();
  }
  return mTransforms;
}
