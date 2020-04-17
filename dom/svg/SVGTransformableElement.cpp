/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGTransformableElement.h"

#include "DOMSVGAnimatedTransformList.h"
#include "gfx2DGlue.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/SVGGraphicsElementBinding.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGRect.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "SVGTextFrame.h"
#include "SVGContentUtils.h"
#include "nsSVGDisplayableFrame.h"
#include "nsSVGUtils.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

already_AddRefed<DOMSVGAnimatedTransformList>
SVGTransformableElement::Transform() {
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the DOMSVGAnimatedTransformList if it hasn't already done so:
  return DOMSVGAnimatedTransformList::GetDOMWrapper(
      GetAnimatedTransformList(DO_ALLOCATE), this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGTransformableElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {sColorMap, sFillStrokeMap,
                                                    sGraphicsMap};

  return FindAttributeDependence(name, map) ||
         SVGElement::IsAttributeMapped(name);
}

nsChangeHint SVGTransformableElement::GetAttributeChangeHint(
    const nsAtom* aAttribute, int32_t aModType) const {
  nsChangeHint retval =
      SVGElement::GetAttributeChangeHint(aAttribute, aModType);
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
      if (!mTransforms || !mTransforms->HasTransform()) {
        // New value is empty, treat as removal.
        // FIXME: Should we just rely on CreatedOrRemovedOnLastChange?
        isAdditionOrRemoval = true;
      } else if (mTransforms->CreatedOrRemovedOnLastChange()) {
        // Old value was empty, treat as addition.
        isAdditionOrRemoval = true;
      }
    }

    if (isAdditionOrRemoval) {
      retval |= nsChangeHint_ComprehensiveAddOrRemoveTransform;
    } else {
      // We just assume the old and new transforms are different.
      retval |= nsChangeHint_UpdatePostTransformOverflow |
                nsChangeHint_UpdateTransformLayer;
    }
  }
  return retval;
}

bool SVGTransformableElement::IsEventAttributeNameInternal(nsAtom* aName) {
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

//----------------------------------------------------------------------
// SVGElement overrides

gfxMatrix SVGTransformableElement::PrependLocalTransformsTo(
    const gfxMatrix& aMatrix, SVGTransformTypes aWhich) const {
  if (aWhich == eChildToUserSpace) {
    // We don't have any eUserSpaceToParent transforms. (Sub-classes that do
    // must override this function and handle that themselves.)
    return aMatrix;
  }
  return GetUserToParentTransform(mAnimateMotionTransform.get(),
                                  mTransforms.get()) *
         aMatrix;
}

const gfx::Matrix* SVGTransformableElement::GetAnimateMotionTransform() const {
  return mAnimateMotionTransform.get();
}

void SVGTransformableElement::SetAnimateMotionTransform(
    const gfx::Matrix* aMatrix) {
  if ((!aMatrix && !mAnimateMotionTransform) ||
      (aMatrix && mAnimateMotionTransform &&
       aMatrix->FuzzyEquals(*mAnimateMotionTransform))) {
    return;
  }
  bool transformSet = mTransforms && mTransforms->IsExplicitlySet();
  bool prevSet = mAnimateMotionTransform || transformSet;
  mAnimateMotionTransform =
      aMatrix ? MakeUnique<gfx::Matrix>(*aMatrix) : nullptr;
  bool nowSet = mAnimateMotionTransform || transformSet;
  int32_t modType;
  if (prevSet && !nowSet) {
    modType = MutationEvent_Binding::REMOVAL;
  } else if (!prevSet && nowSet) {
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

SVGAnimatedTransformList* SVGTransformableElement::GetAnimatedTransformList(
    uint32_t aFlags) {
  if (!mTransforms && (aFlags & DO_ALLOCATE)) {
    mTransforms = MakeUnique<SVGAnimatedTransformList>();
  }
  return mTransforms.get();
}

SVGElement* SVGTransformableElement::GetNearestViewportElement() {
  return SVGContentUtils::GetNearestViewportElement(this);
}

SVGElement* SVGTransformableElement::GetFarthestViewportElement() {
  return SVGContentUtils::GetOuterSVGElement(this);
}

static already_AddRefed<SVGRect> ZeroBBox(SVGTransformableElement& aOwner) {
  return MakeAndAddRef<SVGRect>(&aOwner, Rect {0, 0, 0, 0});
}

already_AddRefed<SVGRect> SVGTransformableElement::GetBBox(
    const SVGBoundingBoxOptions& aOptions) {
  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);

  if (!frame || (frame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
    return ZeroBBox(*this);
  }
  nsSVGDisplayableFrame* svgframe = do_QueryFrame(frame);

  if (!svgframe) {
    if (!nsSVGUtils::IsInSVGTextSubtree(frame)) {
      return ZeroBBox(*this);
    }

    // For <tspan>, <textPath>, the frame is an nsInlineFrame or
    // nsBlockFrame, |svgframe| will be a nullptr.
    // We implement their getBBox directly here instead of in
    // nsSVGUtils::GetBBox, because nsSVGUtils::GetBBox is more
    // or less used for other purpose elsewhere. e.g. gradient
    // code assumes GetBBox of <tspan> returns the bbox of the
    // outer <text>.
    // TODO: cleanup this sort of usecase of nsSVGUtils::GetBBox,
    // then move this code nsSVGUtils::GetBBox.
    SVGTextFrame* text =
        static_cast<SVGTextFrame*>(nsLayoutUtils::GetClosestFrameOfType(
            frame->GetParent(), LayoutFrameType::SVGText));

    if (text->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
      return ZeroBBox(*this);
    }

    gfxRect rec = text->TransformFrameRectFromTextChild(
        frame->GetRectRelativeToSelf(), frame);

    // Should also add the |x|, |y| of the SVGTextFrame itself, since
    // the result obtained by TransformFrameRectFromTextChild doesn't
    // include them.
    rec.x += float(text->GetPosition().x) / AppUnitsPerCSSPixel();
    rec.y += float(text->GetPosition().y) / AppUnitsPerCSSPixel();

    return do_AddRef(new SVGRect(this, ToRect(rec)));
  }

  if (!NS_SVGNewGetBBoxEnabled()) {
    return do_AddRef(new SVGRect(
        this, ToRect(nsSVGUtils::GetBBox(
                  frame, nsSVGUtils::eBBoxIncludeFillGeometry |
                             nsSVGUtils::eUseUserSpaceOfUseElement))));
  }
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
    return do_AddRef(new SVGRect(this, gfx::Rect()));
  }
  if (flags == nsSVGUtils::eBBoxIncludeMarkers ||
      flags == nsSVGUtils::eBBoxIncludeClipped) {
    flags |= nsSVGUtils::eBBoxIncludeFill;
  }
  flags |= nsSVGUtils::eUseUserSpaceOfUseElement;
  return do_AddRef(
      new SVGRect(this, ToRect(nsSVGUtils::GetBBox(frame, flags))));
}

already_AddRefed<SVGMatrix> SVGTransformableElement::GetCTM() {
  Document* currentDoc = GetComposedDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(FlushType::Layout);
  }
  gfx::Matrix m = SVGContentUtils::GetCTM(this, false);
  RefPtr<SVGMatrix> mat =
      m.IsSingular() ? nullptr : new SVGMatrix(ThebesMatrix(m));
  return mat.forget();
}

already_AddRefed<SVGMatrix> SVGTransformableElement::GetScreenCTM() {
  Document* currentDoc = GetComposedDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(FlushType::Layout);
  }
  gfx::Matrix m = SVGContentUtils::GetCTM(this, true);
  RefPtr<SVGMatrix> mat =
      m.IsSingular() ? nullptr : new SVGMatrix(ThebesMatrix(m));
  return mat.forget();
}

already_AddRefed<SVGMatrix> SVGTransformableElement::GetTransformToElement(
    SVGGraphicsElement& aElement, ErrorResult& rv) {
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

/* static */
gfxMatrix SVGTransformableElement::GetUserToParentTransform(
    const gfx::Matrix* aAnimateMotionTransform,
    const SVGAnimatedTransformList* aTransforms) {
  gfxMatrix result;

  if (aAnimateMotionTransform) {
    result.PreMultiply(ThebesMatrix(*aAnimateMotionTransform));
  }

  if (aTransforms) {
    result.PreMultiply(aTransforms->GetAnimValue().GetConsolidationMatrix());
  }

  return result;
}

}  // namespace dom
}  // namespace mozilla
