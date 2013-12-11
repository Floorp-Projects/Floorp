/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEBlendElement.h"
#include "mozilla/dom/SVGFEBlendElementBinding.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEBlend)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEBlendElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEBlendElementBinding::Wrap(aCx, aScope, this);
}

nsSVGEnumMapping SVGFEBlendElement::sModeMap[] = {
  {&nsGkAtoms::normal, SVG_FEBLEND_MODE_NORMAL},
  {&nsGkAtoms::multiply, SVG_FEBLEND_MODE_MULTIPLY},
  {&nsGkAtoms::screen, SVG_FEBLEND_MODE_SCREEN},
  {&nsGkAtoms::darken, SVG_FEBLEND_MODE_DARKEN},
  {&nsGkAtoms::lighten, SVG_FEBLEND_MODE_LIGHTEN},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFEBlendElement::sEnumInfo[1] =
{
  { &nsGkAtoms::mode,
    sModeMap,
    SVG_FEBLEND_MODE_NORMAL
  }
};

nsSVGElement::StringInfo SVGFEBlendElement::sStringInfo[3] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true },
  { &nsGkAtoms::in2, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEBlendElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEBlendElement methods

already_AddRefed<SVGAnimatedString>
SVGFEBlendElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedString>
SVGFEBlendElement::In2()
{
  return mStringAttributes[IN2].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFEBlendElement::Mode()
{
  return mEnumAttributes[MODE].ToDOMAnimatedEnum(this);
}

FilterPrimitiveDescription
SVGFEBlendElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                           const IntRect& aFilterSubregion,
                                           nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  uint32_t mode = mEnumAttributes[MODE].GetAnimValue();
  FilterPrimitiveDescription descr(FilterPrimitiveDescription::eBlend);
  descr.Attributes().Set(eBlendBlendmode, mode);
  return descr;
}

bool
SVGFEBlendElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                             nsIAtom* aAttribute) const
{
  return SVGFEBlendElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::in2 ||
           aAttribute == nsGkAtoms::mode));
}

void
SVGFEBlendElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN2], this));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
SVGFEBlendElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEBlendElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
