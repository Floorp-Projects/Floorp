/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERSELEMENT_H__
#define __NS_SVGFILTERSELEMENT_H__

#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGElement.h"
#include "FilterSupport.h"
#include "nsImageLoadingContent.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberPair.h"
#include "SVGAnimatedString.h"

class nsSVGFilterInstance;

namespace mozilla {
namespace dom {

struct SVGStringInfo {
  SVGStringInfo(const SVGAnimatedString* aString, SVGElement* aElement)
      : mString(aString), mElement(aElement) {}

  const SVGAnimatedString* mString;
  SVGElement* mElement;
};

typedef SVGElement SVGFEBase;

#define NS_SVG_FE_CID                                \
  {                                                  \
    0x60483958, 0xd229, 0x4a77, {                    \
      0x96, 0xb2, 0x62, 0x3e, 0x69, 0x95, 0x1e, 0x0e \
    }                                                \
  }

/**
 * Base class for filter primitive elements
 * Children of those elements e.g. feMergeNode
 * derive from SVGFEUnstyledElement instead
 */
class SVGFE : public SVGFEBase {
  friend class ::nsSVGFilterInstance;

 protected:
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::Size Size;
  typedef mozilla::gfx::IntRect IntRect;
  typedef mozilla::gfx::ColorSpace ColorSpace;
  typedef mozilla::gfx::FilterPrimitiveDescription FilterPrimitiveDescription;

  explicit SVGFE(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEBase(std::move(aNodeInfo)) {}
  virtual ~SVGFE() = default;

 public:
  typedef mozilla::gfx::PrimitiveAttributes PrimitiveAttributes;

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

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_FE_CID)

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  // SVGElement interface
  virtual nsresult Clone(mozilla::dom::NodeInfo*,
                         nsINode** aResult) const override = 0;

  virtual bool HasValidDimensions() const override;

  bool IsNodeOfType(uint32_t aFlags) const override {
    return !(aFlags & ~eFILTER);
  }

  virtual SVGAnimatedString& GetResultImageName() = 0;
  // Return a list of all image names used as sources. Default is to
  // return no sources.
  virtual void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources);

  virtual FilterPrimitiveDescription GetPrimitiveDescription(
      nsSVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
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
  already_AddRefed<mozilla::dom::DOMSVGAnimatedLength> X();
  already_AddRefed<mozilla::dom::DOMSVGAnimatedLength> Y();
  already_AddRefed<mozilla::dom::DOMSVGAnimatedLength> Width();
  already_AddRefed<mozilla::dom::DOMSVGAnimatedLength> Height();
  already_AddRefed<mozilla::dom::DOMSVGAnimatedString> Result();

 protected:
  virtual bool OperatesOnSRGB(int32_t aInputIndex, bool aInputIsAlreadySRGB) {
    return StyleIsSetToSRGB();
  }

  // Only called for filter primitives without inputs.
  virtual bool ProducesSRGB() { return StyleIsSetToSRGB(); }

  bool StyleIsSetToSRGB();

  // SVGElement specializations:
  virtual LengthAttributesInfo GetLengthInfo() override;

  Size GetKernelUnitLength(nsSVGFilterInstance* aInstance,
                           SVGAnimatedNumberPair* aKernelUnitLength);

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

NS_DEFINE_STATIC_IID_ACCESSOR(SVGFE, NS_SVG_FE_CID)

typedef SVGElement SVGFEUnstyledElementBase;

class SVGFEUnstyledElement : public SVGFEUnstyledElementBase {
 protected:
  explicit SVGFEUnstyledElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEUnstyledElementBase(std::move(aNodeInfo)) {}

 public:
  virtual nsresult Clone(mozilla::dom::NodeInfo*,
                         nsINode** aResult) const override = 0;

  // returns true if changes to the attribute should cause us to
  // repaint the filter
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const = 0;
};

//------------------------------------------------------------

typedef SVGFE SVGFELightingElementBase;

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

  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const override;
  virtual SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }
  virtual void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

 protected:
  virtual bool OperatesOnSRGB(int32_t aInputIndex,
                              bool aInputIsAlreadySRGB) override {
    return true;
  }

  virtual NumberAttributesInfo GetNumberInfo() override;
  virtual NumberPairAttributesInfo GetNumberPairInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  mozilla::gfx::LightType ComputeLightAttributes(
      nsSVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes);

  bool AddLightingAttributes(
      mozilla::gfx::DiffuseLightingAttributes* aAttributes,
      nsSVGFilterInstance* aInstance);

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

typedef SVGFEUnstyledElement SVGFELightElementBase;

class SVGFELightElement : public SVGFELightElementBase {
 protected:
  explicit SVGFELightElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFELightElementBase(std::move(aNodeInfo)) {}

 public:
  typedef gfx::PrimitiveAttributes PrimitiveAttributes;

  virtual mozilla::gfx::LightType ComputeLightAttributes(
      nsSVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes) = 0;
};

}  // namespace dom
}  // namespace mozilla

#endif
