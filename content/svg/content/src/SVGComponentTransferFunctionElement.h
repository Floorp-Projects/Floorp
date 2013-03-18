/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGComponentTransferFunctionElement_h
#define mozilla_dom_SVGComponentTransferFunctionElement_h

#include "nsSVGEnum.h"
#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"
#include "SVGAnimatedNumberList.h"


#define NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID \
{ 0xafab106d, 0xbc18, 0x4f7f, \
  { 0x9e, 0x29, 0xfe, 0xb4, 0xb0, 0x16, 0x5f, 0xf4 } }

namespace mozilla {

class DOMSVGAnimatedNumberList;

namespace dom {

typedef SVGFEUnstyledElement SVGComponentTransferFunctionElementBase;

class SVGComponentTransferFunctionElement : public SVGComponentTransferFunctionElementBase
{
protected:
  SVGComponentTransferFunctionElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGComponentTransferFunctionElementBase(aNodeInfo)
  {
    SetIsDOMBinding();
  }

public:
  // interfaces:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID)

  NS_DECL_ISUPPORTS_INHERITED

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  virtual int32_t GetChannel() = 0;
  bool GenerateLookupTable(uint8_t* aTable);

  // WebIDL
  virtual JSObject*
  WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE = 0;
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> Type();
  already_AddRefed<DOMSVGAnimatedNumberList> TableValues();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Slope();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Intercept();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Amplitude();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Exponent();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Offset();

protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual NumberListAttributesInfo GetNumberListInfo();

  enum { TABLEVALUES };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];

  enum { SLOPE, INTERCEPT, AMPLITUDE, EXPONENT, OFFSET };
  nsSVGNumber2 mNumberAttributes[5];
  static NumberInfo sNumberInfo[5];

  enum { TYPE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sTypeMap[];
  static EnumInfo sEnumInfo[1];
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncRElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncRElement : public SVGComponentTransferFunctionElement,
                          public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFEFuncRElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFuncRElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t GetChannel() { return 0; }

  NS_FORWARD_NSIDOMSVGELEMENT(SVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual JSObject*
  WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncGElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncGElement : public SVGComponentTransferFunctionElement,
                          public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFEFuncGElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFuncGElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t GetChannel() { return 1; }

  NS_FORWARD_NSIDOMSVGELEMENT(SVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual JSObject*
  WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncBElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncBElement : public SVGComponentTransferFunctionElement,
                          public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFEFuncBElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFuncBElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t GetChannel() { return 2; }

  NS_FORWARD_NSIDOMSVGELEMENT(SVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual JSObject*
  WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncAElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncAElement : public SVGComponentTransferFunctionElement,
                          public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFEFuncAElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFuncAElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t GetChannel() { return 3; }

  NS_FORWARD_NSIDOMSVGELEMENT(SVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual JSObject*
  WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGComponentTransferFunctionElement_h
