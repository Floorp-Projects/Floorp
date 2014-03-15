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
  SVGComponentTransferFunctionElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : SVGComponentTransferFunctionElementBase(aNodeInfo)
  {
  }

public:
  typedef gfx::AttributeMap AttributeMap;

  // interfaces:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID)

  NS_DECL_ISUPPORTS_INHERITED

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual int32_t GetChannel() = 0;

  AttributeMap ComputeAttributes();

  // WebIDL
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE = 0;
  already_AddRefed<SVGAnimatedEnumeration> Type();
  already_AddRefed<DOMSVGAnimatedNumberList> TableValues();
  already_AddRefed<SVGAnimatedNumber> Slope();
  already_AddRefed<SVGAnimatedNumber> Intercept();
  already_AddRefed<SVGAnimatedNumber> Amplitude();
  already_AddRefed<SVGAnimatedNumber> Exponent();
  already_AddRefed<SVGAnimatedNumber> Offset();

protected:
  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual NumberListAttributesInfo GetNumberListInfo() MOZ_OVERRIDE;

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
    nsIContent** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncRElement : public SVGComponentTransferFunctionElement
{
  friend nsresult (::NS_NewSVGFEFuncRElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo));
protected:
  SVGFEFuncRElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  virtual int32_t GetChannel() MOZ_OVERRIDE { return 0; }

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncGElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncGElement : public SVGComponentTransferFunctionElement
{
  friend nsresult (::NS_NewSVGFEFuncGElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo));
protected:
  SVGFEFuncGElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  virtual int32_t GetChannel() MOZ_OVERRIDE { return 1; }

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncBElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncBElement : public SVGComponentTransferFunctionElement
{
  friend nsresult (::NS_NewSVGFEFuncBElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo));
protected:
  SVGFEFuncBElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  virtual int32_t GetChannel() MOZ_OVERRIDE { return 2; }

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncAElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncAElement : public SVGComponentTransferFunctionElement
{
  friend nsresult (::NS_NewSVGFEFuncAElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo));
protected:
  SVGFEFuncAElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  virtual int32_t GetChannel() MOZ_OVERRIDE { return 3; }

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGComponentTransferFunctionElement_h
