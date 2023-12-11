/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFILTERS_H_
#define DOM_SVG_SVGFILTERS_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGElement.h"
#include "FilterDescription.h"
#include "nsImageLoadingContent.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberPair.h"
#include "SVGAnimatedString.h"

namespace mozilla {
class SVGFilterInstance;

namespace dom {

struct SVGStringInfo {
  SVGStringInfo(const SVGAnimatedString* aString, SVGElement* aElement)
      : mString(aString), mElement(aElement) {}

  const SVGAnimatedString* mString;
  SVGElement* mElement;
};

using SVGFilterPrimitiveElementBase = SVGElement;

/**
 * Base class for filter primitive elements
 * Children of those elements e.g. feMergeNode
 * derive from SVGFilterPrimitiveChildElement instead
 */
class SVGFilterPrimitiveElement : public SVGFilterPrimitiveElementBase {
  friend class mozilla::SVGFilterInstance;

 protected:
  using SourceSurface = mozilla::gfx::SourceSurface;
  using Size = mozilla::gfx::Size;
  using IntRect = mozilla::gfx::IntRect;
  using ColorSpace = mozilla::gfx::ColorSpace;
  using FilterPrimitiveDescription = mozilla::gfx::FilterPrimitiveDescription;

  explicit SVGFilterPrimitiveElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFilterPrimitiveElementBase(std::move(aNodeInfo)) {}
  virtual ~SVGFilterPrimitiveElement() = default;

 public:
  using PrimitiveAttributes = mozilla::gfx::PrimitiveAttributes;

  NS_IMPL_FROMNODE_HELPER(SVGFilterPrimitiveElement,
                          IsSVGFilterPrimitiveElement())

  ColorSpace GetInputColorSpace(int32_t aInputIndex,
                                ColorSpace aUnchangedInputColorSpace) {
    return OperatesOnSRGB(aInputIndex,
                          aUnchangedInputColorSpace == ColorSpace::SRGB)
               ? ColorSpace::SRGB
               : ColorSpace::LinearRGB;
  }

  // This is only called for filter primitives without inputs. For primitives
  // with inputs, the output color model is the same as of the first input.
  ColorSpace GetOutputColorSpace() {
    return ProducesSRGB() ? ColorSpace::SRGB : ColorSpace::LinearRGB;
  }

  // See http://www.w3.org/TR/SVG/filters.html#FilterPrimitiveSubRegion
  virtual bool SubregionIsUnionOfRegions() { return true; }

  bool IsSVGFilterPrimitiveElement() const final { return true; }

  // SVGElement interface
  nsresult Clone(mozilla::dom::NodeInfo*, nsINode** aResult) const override = 0;

  bool HasValidDimensions() const override;

  virtual SVGAnimatedString& GetResultImageName() = 0;
  // Return a list of all image names used as sources. Default is to
  // return no sources.
  virtual void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources);

  virtual FilterPrimitiveDescription GetPrimitiveDescription(
      SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) = 0;

  // returns true if changes to the attribute should cause us to
  // repaint the filter
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const;

  // Return whether this filter primitive has tainted output. A filter's
  // output is tainted if it depends on things that the web page is not
  // allowed to read from, e.g. the source graphic or cross-origin images.
  // aReferencePrincipal is the node principal of the filtered frame's element.
  virtual bool OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                               nsIPrincipal* aReferencePrincipal);

  static nsIntRect GetMaxRect() {
    // Try to avoid overflow errors dealing with this rect. It will
    // be intersected with some other reasonable-sized rect eventually.
    return nsIntRect(INT32_MIN / 2, INT32_MIN / 2, INT32_MAX, INT32_MAX);
  }

  operator nsISupports*() { return static_cast<nsIContent*>(this); }

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();
  already_AddRefed<DOMSVGAnimatedString> Result();

 protected:
  virtual bool OperatesOnSRGB(int32_t aInputIndex, bool aInputIsAlreadySRGB) {
    return StyleIsSetToSRGB();
  }

  // Only called for filter primitives without inputs.
  virtual bool ProducesSRGB() { return StyleIsSetToSRGB(); }

  bool StyleIsSetToSRGB();

  // SVGElement specializations:
  LengthAttributesInfo GetLengthInfo() override;

  Size GetKernelUnitLength(SVGFilterInstance* aInstance,
                           SVGAnimatedNumberPair* aKernelUnitLength);

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

using SVGFilterPrimitiveChildElementBase = SVGElement;

class SVGFilterPrimitiveChildElement
    : public SVGFilterPrimitiveChildElementBase {
 protected:
  explicit SVGFilterPrimitiveChildElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFilterPrimitiveChildElementBase(std::move(aNodeInfo)) {}

 public:
  NS_IMPL_FROMNODE_HELPER(SVGFilterPrimitiveChildElement,
                          IsSVGFilterPrimitiveChildElement())

  bool IsSVGFilterPrimitiveChildElement() const final { return true; }

  nsresult Clone(mozilla::dom::NodeInfo*, nsINode** aResult) const override = 0;

  // returns true if changes to the attribute should cause us to
  // repaint the filter
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const = 0;
};

//------------------------------------------------------------

using SVGFELightingElementBase = SVGFilterPrimitiveElement;

class SVGFELightingElement : public SVGFELightingElementBase {
 protected:
  explicit SVGFELightingElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFELightingElementBase(std::move(aNodeInfo)) {}

  virtual ~SVGFELightingElement() = default;

 public:
  // interfaces:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(SVGFELightingElement,
                                       SVGFELightingElementBase)

  bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                 nsAtom* aAttribute) const override;
  SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }

  bool OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                       nsIPrincipal* aReferencePrincipal) override;

  void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) override;

 protected:
  bool OperatesOnSRGB(int32_t aInputIndex, bool aInputIsAlreadySRGB) override {
    return true;
  }

  NumberAttributesInfo GetNumberInfo() override;
  NumberPairAttributesInfo GetNumberPairInfo() override;
  StringAttributesInfo GetStringInfo() override;

  mozilla::gfx::LightType ComputeLightAttributes(
      SVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes);

  bool AddLightingAttributes(
      mozilla::gfx::DiffuseLightingAttributes* aAttributes,
      SVGFilterInstance* aInstance);

  enum {
    SURFACE_SCALE,
    DIFFUSE_CONSTANT,
    SPECULAR_CONSTANT,
    SPECULAR_EXPONENT
  };
  SVGAnimatedNumber mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  enum { KERNEL_UNIT_LENGTH };
  SVGAnimatedNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { RESULT, IN1 };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

using SVGFELightElementBase = SVGFilterPrimitiveChildElement;

class SVGFELightElement : public SVGFELightElementBase {
 protected:
  explicit SVGFELightElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFELightElementBase(std::move(aNodeInfo)) {}

 public:
  using PrimitiveAttributes = gfx::PrimitiveAttributes;

  virtual mozilla::gfx::LightType ComputeLightAttributes(
      SVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes) = 0;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGFILTERS_H_
