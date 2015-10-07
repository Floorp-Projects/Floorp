/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGFilterFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxUtils.h"
#include "nsGkAtoms.h"
#include "nsSVGEffects.h"
#include "nsSVGElement.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "nsContentUtils.h"

using namespace mozilla::dom;

nsIFrame*
NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGFilterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGFilterFrame)

class MOZ_RAII nsSVGFilterFrame::AutoFilterReferencer
{
public:
  explicit AutoFilterReferencer(nsSVGFilterFrame *aFrame MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mFrame(aFrame)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    // Reference loops should normally be detected in advance and handled, so
    // we're not expecting to encounter them here
    MOZ_ASSERT(!mFrame->mLoopFlag, "Undetected reference loop!");
    mFrame->mLoopFlag = true;
  }
  ~AutoFilterReferencer() {
    mFrame->mLoopFlag = false;
  }
private:
  nsSVGFilterFrame *mFrame;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

uint16_t
nsSVGFilterFrame::GetEnumValue(uint32_t aIndex, nsIContent *aDefault)
{
  nsSVGEnum& thisEnum =
    static_cast<SVGFilterElement *>(mContent)->mEnumAttributes[aIndex];

  if (thisEnum.IsExplicitlySet())
    return thisEnum.GetAnimValue();

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetEnumValue(aIndex, aDefault) :
    static_cast<SVGFilterElement *>(aDefault)->
      mEnumAttributes[aIndex].GetAnimValue();
}

const nsSVGLength2 *
nsSVGFilterFrame::GetLengthValue(uint32_t aIndex, nsIContent *aDefault)
{
  const nsSVGLength2 *thisLength =
    &static_cast<SVGFilterElement *>(mContent)->mLengthAttributes[aIndex];

  if (thisLength->IsExplicitlySet())
    return thisLength;

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetLengthValue(aIndex, aDefault) :
    &static_cast<SVGFilterElement *>(aDefault)->mLengthAttributes[aIndex];
}

const SVGFilterElement *
nsSVGFilterFrame::GetFilterContent(nsIContent *aDefault)
{
  for (nsIContent* child = mContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    nsRefPtr<nsSVGFE> primitive;
    CallQueryInterface(child, (nsSVGFE**)getter_AddRefs(primitive));
    if (primitive) {
      return static_cast<SVGFilterElement *>(mContent);
    }
  }

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetFilterContent(aDefault) :
    static_cast<SVGFilterElement *>(aDefault);
}

nsSVGFilterFrame *
nsSVGFilterFrame::GetReferencedFilter()
{
  if (mNoHRefURI)
    return nullptr;

  nsSVGPaintingProperty *property = static_cast<nsSVGPaintingProperty*>
    (Properties().Get(nsSVGEffects::HrefProperty()));

  if (!property) {
    // Fetch our Filter element's xlink:href attribute
    SVGFilterElement *filter = static_cast<SVGFilterElement *>(mContent);
    nsAutoString href;
    filter->mStringAttributes[SVGFilterElement::HREF].GetAnimValue(href, filter);
    if (href.IsEmpty()) {
      mNoHRefURI = true;
      return nullptr; // no URL
    }

    // Convert href to an nsIURI
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetCurrentDoc(), base);

    property =
      nsSVGEffects::GetPaintingProperty(targetURI, this, nsSVGEffects::HrefProperty());
    if (!property)
      return nullptr;
  }

  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nullptr;

  nsIAtom* frameType = result->GetType();
  if (frameType != nsGkAtoms::svgFilterFrame)
    return nullptr;

  return static_cast<nsSVGFilterFrame*>(result);
}

nsSVGFilterFrame *
nsSVGFilterFrame::GetReferencedFilterIfNotInUse()
{
  nsSVGFilterFrame *referenced = GetReferencedFilter();
  if (!referenced)
    return nullptr;

  if (referenced->mLoopFlag) {
    // XXXjwatt: we should really send an error to the JavaScript Console here:
    NS_WARNING("Filter reference loop detected while inheriting attribute!");
    return nullptr;
  }

  return referenced;
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
  } else if (aNameSpaceID == kNameSpaceID_XLink &&
             aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    Properties().Delete(nsSVGEffects::HrefProperty());
    mNoHRefURI = false;
    // And update whoever references us
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }
  return nsSVGFilterFrameBase::AttributeChanged(aNameSpaceID,
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

  nsSVGFilterFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGFilterFrame::GetType() const
{
  return nsGkAtoms::svgFilterFrame;
}
