/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGFilterFrame.h"

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "gfxUtils.h"
#include "nsGkAtoms.h"
#include "nsSVGEffects.h"
#include "nsSVGElement.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame*
NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGFilterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGFilterFrame)

uint16_t
nsSVGFilterFrame::GetEnumValue(uint32_t aIndex, nsIContent *aDefault)
{
  nsSVGEnum& thisEnum =
    static_cast<SVGFilterElement *>(mContent)->mEnumAttributes[aIndex];

  if (thisEnum.IsExplicitlySet())
    return thisEnum.GetAnimValue();

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<SVGFilterElement *>(aDefault)->
    mEnumAttributes[aIndex].GetAnimValue();
  }

  nsSVGFilterFrame *next = GetReferencedFilter();

  return next ? next->GetEnumValue(aIndex, aDefault)
              : static_cast<SVGFilterElement *>(aDefault)->
                  mEnumAttributes[aIndex].GetAnimValue();
}

const nsSVGLength2 *
nsSVGFilterFrame::GetLengthValue(uint32_t aIndex, nsIContent *aDefault)
{
  const nsSVGLength2 *thisLength =
    &static_cast<SVGFilterElement *>(mContent)->mLengthAttributes[aIndex];

  if (thisLength->IsExplicitlySet())
    return thisLength;

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return &static_cast<SVGFilterElement*>(aDefault)->mLengthAttributes[aIndex];
  }

  nsSVGFilterFrame *next = GetReferencedFilter();

  return next ? next->GetLengthValue(aIndex, aDefault)
              : &static_cast<SVGFilterElement *>(aDefault)->mLengthAttributes[aIndex];
}

const SVGFilterElement *
nsSVGFilterFrame::GetFilterContent(nsIContent *aDefault)
{
  for (nsIContent* child = mContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    RefPtr<nsSVGFE> primitive;
    CallQueryInterface(child, (nsSVGFE**)getter_AddRefs(primitive));
    if (primitive) {
      return static_cast<SVGFilterElement *>(mContent);
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

  nsSVGFilterFrame *next = GetReferencedFilter();

  return next ? next->GetFilterContent(aDefault)
              : static_cast<SVGFilterElement*>(aDefault);
}

nsSVGFilterFrame *
nsSVGFilterFrame::GetReferencedFilter()
{
  if (mNoHRefURI)
    return nullptr;

  nsSVGPaintingProperty *property =
    GetProperty(nsSVGEffects::HrefAsPaintingProperty());

  if (!property) {
    // Fetch our Filter element's href or xlink:href attribute
    SVGFilterElement *filter = static_cast<SVGFilterElement *>(mContent);
    nsAutoString href;
    if (filter->mStringAttributes[SVGFilterElement::HREF].IsExplicitlySet()) {
      filter->mStringAttributes[SVGFilterElement::HREF]
        .GetAnimValue(href, filter);
    } else {
      filter->mStringAttributes[SVGFilterElement::XLINK_HREF]
        .GetAnimValue(href, filter);
    }

    if (href.IsEmpty()) {
      mNoHRefURI = true;
      return nullptr; // no URL
    }

    // Convert href to an nsIURI
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetUncomposedDoc(), base);

    property =
      nsSVGEffects::GetPaintingProperty(targetURI, this,
                                        nsSVGEffects::HrefAsPaintingProperty());
    if (!property)
      return nullptr;
  }

  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nullptr;

  LayoutFrameType frameType = result->Type();
  if (frameType != LayoutFrameType::SVGFilter)
    return nullptr;

  return static_cast<nsSVGFilterFrame*>(result);
}

nsresult
nsSVGFilterFrame::AttributeChanged(int32_t  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::filterUnits ||
       aAttribute == nsGkAtoms::primitiveUnits)) {
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  } else if ((aNameSpaceID == kNameSpaceID_XLink ||
              aNameSpaceID == kNameSpaceID_None) &&
             aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    DeleteProperty(nsSVGEffects::HrefAsPaintingProperty());
    mNoHRefURI = false;
    // And update whoever references us
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }
  return nsSVGContainerFrame::AttributeChanged(aNameSpaceID,
                                               aAttribute, aModType);
}

#ifdef DEBUG
void
nsSVGFilterFrame::Init(nsIContent*       aContent,
                       nsContainerFrame* aParent,
                       nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::filter),
               "Content is not an SVG filter");

  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */
