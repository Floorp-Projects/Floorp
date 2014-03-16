/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERSELEMENT_H__
#define __NS_SVGFILTERSELEMENT_H__

#include "mozilla/Attributes.h"
#include "nsImageLoadingContent.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "nsSVGElement.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"
#include "FilterSupport.h"
#include "gfxASurface.h"

class nsSVGFilterInstance;
class nsSVGFilterResource;
class nsSVGNumberPair;

struct nsSVGStringInfo {
  nsSVGStringInfo(const nsSVGString* aString,
                  nsSVGElement *aElement) :
    mString(aString), mElement(aElement) {}

  const nsSVGString* mString;
  nsSVGElement* mElement;
};

typedef nsSVGElement nsSVGFEBase;

#define NS_SVG_FE_CID \
{ 0x60483958, 0xd229, 0x4a77, \
  { 0x96, 0xb2, 0x62, 0x3e, 0x69, 0x95, 0x1e, 0x0e } }

/**
 * Base class for filter primitive elements
 * Children of those elements e.g. feMergeNode
 * derive from SVGFEUnstyledElement instead
 */
class nsSVGFE : public nsSVGFEBase
{
  friend class nsSVGFilterInstance;

protected:
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::Size Size;
  typedef mozilla::gfx::IntRect IntRect;
  typedef mozilla::gfx::ColorSpace ColorSpace;
  typedef mozilla::gfx::FilterPrimitiveDescription FilterPrimitiveDescription;

  nsSVGFE(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : nsSVGFEBase(aNodeInfo) {}

public:
  typedef mozilla::gfx::AttributeMap AttributeMap;

  ColorSpace
  GetInputColorSpace(int32_t aInputIndex, ColorSpace aUnchangedInputColorSpace) {
    return OperatesOnSRGB(aInputIndex, aUnchangedInputColorSpace == mozilla::gfx::SRGB) ?
             mozilla::gfx::SRGB : mozilla::gfx::LINEAR_RGB;
  }

  // This is only called for filter primitives without inputs. For primitives
  // with inputs, the output color model is the same as of the first input.
  ColorSpace
  GetOutputColorSpace() {
    return ProducesSRGB() ? mozilla::gfx::SRGB : mozilla::gfx::LINEAR_RGB;
  }

  // See http://www.w3.org/TR/SVG/filters.html#FilterPrimitiveSubRegion
  virtual bool SubregionIsUnionOfRegions() { return true; }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_FE_CID)

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  // nsSVGElement interface
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE = 0;

  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  bool IsNodeOfType(uint32_t aFlags) const MOZ_OVERRIDE
    { return !(aFlags & ~(eCONTENT | eFILTER)); }

  virtual nsSVGString& GetResultImageName() = 0;
  // Return a list of all image names used as sources. Default is to
  // return no sources.
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);

  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            const nsTArray<bool>& aInputsAreTainted,
                            nsTArray<mozilla::RefPtr<SourceSurface>>& aInputImages) = 0;

  // returns true if changes to the attribute should cause us to
  // repaint the filter
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  // Return whether this filter primitive has tainted output. A filter's
  // output is tainted if it depends on things that the web page is not
  // allowed to read from, e.g. the source graphic or cross-origin images.
  // aReferencePrincipal is the node principal of the filtered frame's element.
  virtual bool OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                               nsIPrincipal* aReferencePrincipal);

  static nsIntRect GetMaxRect() {
    // Try to avoid overflow errors dealing with this rect. It will
    // be intersected with some other reasonable-sized rect eventually.
    return nsIntRect(INT32_MIN/2, INT32_MIN/2, INT32_MAX, INT32_MAX);
  }

  operator nsISupports*() { return static_cast<nsIContent*>(this); }

  // WebIDL
  already_AddRefed<mozilla::dom::SVGAnimatedLength> X();
  already_AddRefed<mozilla::dom::SVGAnimatedLength> Y();
  already_AddRefed<mozilla::dom::SVGAnimatedLength> Width();
  already_AddRefed<mozilla::dom::SVGAnimatedLength> Height();
  already_AddRefed<mozilla::dom::SVGAnimatedString> Result();

protected:
  virtual bool OperatesOnSRGB(int32_t aInputIndex, bool aInputIsAlreadySRGB) {
    return StyleIsSetToSRGB();
  }

  // Only called for filter primitives without inputs.
  virtual bool ProducesSRGB() { return StyleIsSetToSRGB(); }

  bool StyleIsSetToSRGB();

  // nsSVGElement specializations:
  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;

  Size GetKernelUnitLength(nsSVGFilterInstance* aInstance,
                          nsSVGNumberPair *aKernelUnitLength);

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

typedef nsSVGElement SVGFEUnstyledElementBase;

class SVGFEUnstyledElement : public SVGFEUnstyledElementBase
{
protected:
  SVGFEUnstyledElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : SVGFEUnstyledElementBase(aNodeInfo) {}

public:
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE = 0;

  // returns true if changes to the attribute should cause us to
  // repaint the filter
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const = 0;
};

//------------------------------------------------------------

typedef nsSVGFE nsSVGFELightingElementBase;

class nsSVGFELightingElement : public nsSVGFELightingElementBase
{
protected:
  nsSVGFELightingElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : nsSVGFELightingElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsSVGString& GetResultImageName() MOZ_OVERRIDE { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) MOZ_OVERRIDE;
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFELightingElementBase::)

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

protected:
  virtual bool OperatesOnSRGB(int32_t aInputIndex,
                              bool aInputIsAlreadySRGB) MOZ_OVERRIDE { return true; }

  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;
  virtual NumberPairAttributesInfo GetNumberPairInfo() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  AttributeMap ComputeLightAttributes(nsSVGFilterInstance* aInstance);

  FilterPrimitiveDescription
    AddLightingAttributes(FilterPrimitiveDescription aDescription,
                          nsSVGFilterInstance* aInstance);

  enum { SURFACE_SCALE, DIFFUSE_CONSTANT, SPECULAR_CONSTANT, SPECULAR_EXPONENT };
  nsSVGNumber2 mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  enum { KERNEL_UNIT_LENGTH };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

namespace mozilla {
namespace dom {

typedef SVGFEUnstyledElement SVGFELightElementBase;

class SVGFELightElement : public SVGFELightElementBase
{
protected:
  SVGFELightElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : SVGFELightElementBase(aNodeInfo) {}

public:
  typedef gfx::AttributeMap AttributeMap;

  virtual AttributeMap
    ComputeLightAttributes(nsSVGFilterInstance* aInstance) = 0;
};

}
}

#endif
