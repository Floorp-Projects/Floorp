/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGCOMPONENTTRANSFERFUNCTIONELEMENT_H_
#define DOM_SVG_SVGCOMPONENTTRANSFERFUNCTIONELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberList.h"
#include "mozilla/dom/SVGFilters.h"

#define NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID \
  {                                                       \
    0xafab106d, 0xbc18, 0x4f7f, {                         \
      0x9e, 0x29, 0xfe, 0xb4, 0xb0, 0x16, 0x5f, 0xf4      \
    }                                                     \
  }

namespace mozilla::dom {

class DOMSVGAnimatedNumberList;

using SVGComponentTransferFunctionElementBase = SVGFilterPrimitiveChildElement;

class SVGComponentTransferFunctionElement
    : public SVGComponentTransferFunctionElementBase {
 protected:
  explicit SVGComponentTransferFunctionElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGComponentTransferFunctionElementBase(std::move(aNodeInfo)) {}

  virtual ~SVGComponentTransferFunctionElement() = default;

 public:
  using ComponentTransferAttributes = gfx::ComponentTransferAttributes;

  // interfaces:
  NS_DECLARE_STATIC_IID_ACCESSOR(
      NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID)

  NS_DECL_ISUPPORTS_INHERITED

  bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                 nsAtom* aAttribute) const override;

  virtual int32_t GetChannel() = 0;

  void ComputeAttributes(int32_t aChannel,
                         ComponentTransferAttributes& aAttributes);

  // WebIDL
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override = 0;
  already_AddRefed<DOMSVGAnimatedEnumeration> Type();
  already_AddRefed<DOMSVGAnimatedNumberList> TableValues();
  already_AddRefed<DOMSVGAnimatedNumber> Slope();
  already_AddRefed<DOMSVGAnimatedNumber> Intercept();
  already_AddRefed<DOMSVGAnimatedNumber> Amplitude();
  already_AddRefed<DOMSVGAnimatedNumber> Exponent();
  already_AddRefed<DOMSVGAnimatedNumber> Offset();

 protected:
  NumberAttributesInfo GetNumberInfo() override;
  EnumAttributesInfo GetEnumInfo() override;
  NumberListAttributesInfo GetNumberListInfo() override;

  enum { TABLEVALUES };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];

  enum { SLOPE, INTERCEPT, AMPLITUDE, EXPONENT, OFFSET };
  SVGAnimatedNumber mNumberAttributes[5];
  static NumberInfo sNumberInfo[5];

  enum { TYPE };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static SVGEnumMapping sTypeMap[];
  static EnumInfo sEnumInfo[1];
};

NS_DEFINE_STATIC_IID_ACCESSOR(SVGComponentTransferFunctionElement,
                              NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID)

}  // namespace mozilla::dom

nsresult NS_NewSVGFEFuncRElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

class SVGFEFuncRElement : public SVGComponentTransferFunctionElement {
  friend nsresult(::NS_NewSVGFEFuncRElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEFuncRElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGComponentTransferFunctionElement(std::move(aNodeInfo)) {}

 public:
  int32_t GetChannel() override { return 0; }

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

nsresult NS_NewSVGFEFuncGElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

class SVGFEFuncGElement : public SVGComponentTransferFunctionElement {
  friend nsresult(::NS_NewSVGFEFuncGElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEFuncGElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGComponentTransferFunctionElement(std::move(aNodeInfo)) {}

 public:
  int32_t GetChannel() override { return 1; }

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

nsresult NS_NewSVGFEFuncBElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

class SVGFEFuncBElement : public SVGComponentTransferFunctionElement {
  friend nsresult(::NS_NewSVGFEFuncBElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEFuncBElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGComponentTransferFunctionElement(std::move(aNodeInfo)) {}

 public:
  int32_t GetChannel() override { return 2; }

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

nsresult NS_NewSVGFEFuncAElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

class SVGFEFuncAElement : public SVGComponentTransferFunctionElement {
  friend nsresult(::NS_NewSVGFEFuncAElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEFuncAElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGComponentTransferFunctionElement(std::move(aNodeInfo)) {}

 public:
  int32_t GetChannel() override { return 3; }

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGCOMPONENTTRANSFERFUNCTIONELEMENT_H_
