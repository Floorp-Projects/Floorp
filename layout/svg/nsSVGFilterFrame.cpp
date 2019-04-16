/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGFilterFrame.h"

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "gfxUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "nsGkAtoms.h"
#include "SVGObserverUtils.h"
#include "SVGElement.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame* NS_NewSVGFilterFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsSVGFilterFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGFilterFrame)

uint16_t nsSVGFilterFrame::GetEnumValue(uint32_t aIndex, nsIContent* aDefault) {
  SVGAnimatedEnumeration& thisEnum =
      static_cast<SVGFilterElement*>(GetContent())->mEnumAttributes[aIndex];

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
    return static_cast<SVGFilterElement*>(aDefault)
        ->mEnumAttributes[aIndex]
        .GetAnimValue();
  }

  nsSVGFilterFrame* next = GetReferencedFilter();

  return next ? next->GetEnumValue(aIndex, aDefault)
              : static_cast<SVGFilterElement*>(aDefault)
                    ->mEnumAttributes[aIndex]
                    .GetAnimValue();
}

const SVGAnimatedLength* nsSVGFilterFrame::GetLengthValue(
    uint32_t aIndex, nsIContent* aDefault) {
  const SVGAnimatedLength* thisLength =
      &static_cast<SVGFilterElement*>(GetContent())->mLengthAttributes[aIndex];

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
    return &static_cast<SVGFilterElement*>(aDefault)->mLengthAttributes[aIndex];
  }

  nsSVGFilterFrame* next = GetReferencedFilter();

  return next ? next->GetLengthValue(aIndex, aDefault)
              : &static_cast<SVGFilterElement*>(aDefault)
                     ->mLengthAttributes[aIndex];
}

const SVGFilterElement* nsSVGFilterFrame::GetFilterContent(
    nsIContent* aDefault) {
  for (nsIContent* child = mContent->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    RefPtr<SVGFE> primitive;
    CallQueryInterface(child, (SVGFE**)getter_AddRefs(primitive));
    if (primitive) {
      return static_cast<SVGFilterElement*>(GetContent());
    }
  }

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGFilterElement*>(aDefault);
  }

  nsSVGFilterFrame* next = GetReferencedFilter();

  return next ? next->GetFilterContent(aDefault)
              : static_cast<SVGFilterElement*>(aDefault);
}

nsSVGFilterFrame* nsSVGFilterFrame::GetReferencedFilter() {
  if (mNoHRefURI) {
    return nullptr;
  }

  auto GetHref = [this](nsAString& aHref) {
    SVGFilterElement* filter = static_cast<SVGFilterElement*>(GetContent());
    if (filter->mStringAttributes[SVGFilterElement::HREF].IsExplicitlySet()) {
      filter->mStringAttributes[SVGFilterElement::HREF].GetAnimValue(aHref,
                                                                     filter);
    } else {
      filter->mStringAttributes[SVGFilterElement::XLINK_HREF].GetAnimValue(
          aHref, filter);
    }
    this->mNoHRefURI = aHref.IsEmpty();
  };

  nsIFrame* tframe = SVGObserverUtils::GetAndObserveTemplate(this, GetHref);
  if (tframe) {
    LayoutFrameType frameType = tframe->Type();
    if (frameType == LayoutFrameType::SVGFilter) {
      return static_cast<nsSVGFilterFrame*>(tframe);
    }
    // We don't call SVGObserverUtils::RemoveTemplateObserver and set
    // `mNoHRefURI = false` here since we want to be invalidated if the ID
    // specified by our href starts resolving to a different/valid element.
  }

  return nullptr;
}

nsresult nsSVGFilterFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::filterUnits ||
       aAttribute == nsGkAtoms::primitiveUnits)) {
    SVGObserverUtils::InvalidateDirectRenderingObservers(this);
  } else if ((aNameSpaceID == kNameSpaceID_XLink ||
              aNameSpaceID == kNameSpaceID_None) &&
             aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    SVGObserverUtils::RemoveTemplateObserver(this);
    mNoHRefURI = false;
    // And update whoever references us
    SVGObserverUtils::InvalidateDirectRenderingObservers(this);
  }
  return nsSVGContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                               aModType);
}

#ifdef DEBUG
void nsSVGFilterFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::filter),
               "Content is not an SVG filter");

  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */
