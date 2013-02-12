/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGComponentTransferFunctionElement_h
#define mozilla_dom_SVGComponentTransferFunctionElement_h

#include "mozilla/Util.h"

#include "nsSVGElement.h"
#include "nsGkAtoms.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"
#include "nsSVGInteger.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGBoolean.h"
#include "nsIDOMSVGFilters.h"
#include "nsCOMPtr.h"
#include "nsSVGFilterInstance.h"
#include "nsIDOMSVGFilterElement.h"
#include "nsSVGEnum.h"
#include "SVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "nsSVGFilters.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "nsStyleContext.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "imgIContainer.h"
#include "nsNetUtil.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsSVGFilterElement.h"
#include "nsSVGString.h"
#include "nsSVGEffects.h"
#include "gfxUtils.h"
#include "SVGContentUtils.h"
#include <algorithm>
#include "nsContentUtils.h"

#define NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID \
{ 0xafab106d, 0xbc18, 0x4f7f, \
  { 0x9e, 0x29, 0xfe, 0xb4, 0xb0, 0x16, 0x5f, 0xf4 } }

nsresult NS_NewSVGComponentTransferFunctionElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGFEUnstyledElement SVGComponentTransferFunctionElementBase;

class SVGComponentTransferFunctionElement : public SVGComponentTransferFunctionElementBase
{
  friend nsresult (::NS_NewSVGComponentTransferFunctionElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGComponentTransferFunctionElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGComponentTransferFunctionElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  virtual int32_t GetChannel() = 0;
  void GenerateLookupTable(uint8_t* aTable);

protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual NumberListAttributesInfo GetNumberListInfo();

  // nsIDOMSVGComponentTransferFunctionElement properties:
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
                          public nsIDOMSVGFEFuncRElement
{
  friend nsresult (::NS_NewSVGFEFuncRElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFuncRElement(already_AddRefed<nsINodeInfo> aNodeInfo) 
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(SVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCRELEMENT

  virtual int32_t GetChannel() { return 0; }
  
  NS_FORWARD_NSIDOMSVGELEMENT(SVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncGElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncGElement : public SVGComponentTransferFunctionElement,
                          public nsIDOMSVGFEFuncGElement
{
  friend nsresult (::NS_NewSVGFEFuncGElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFuncGElement(already_AddRefed<nsINodeInfo> aNodeInfo) 
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(SVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCGELEMENT

  virtual int32_t GetChannel() { return 1; }

  NS_FORWARD_NSIDOMSVGELEMENT(SVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncBElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncBElement : public SVGComponentTransferFunctionElement,
                          public nsIDOMSVGFEFuncBElement
{
  friend nsresult (::NS_NewSVGFEFuncBElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFuncBElement(already_AddRefed<nsINodeInfo> aNodeInfo) 
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(SVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCBELEMENT

  virtual int32_t GetChannel() { return 2; }

  NS_FORWARD_NSIDOMSVGELEMENT(SVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

} // namespace dom
} // namespace mozilla

nsresult NS_NewSVGFEFuncAElement(
  nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEFuncAElement : public SVGComponentTransferFunctionElement,
                          public nsIDOMSVGFEFuncAElement
{
  friend nsresult (::NS_NewSVGFEFuncAElement(
    nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEFuncAElement(already_AddRefed<nsINodeInfo> aNodeInfo) 
    : SVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(SVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCAELEMENT

  virtual int32_t GetChannel() { return 3; }

  NS_FORWARD_NSIDOMSVGELEMENT(SVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGComponentTransferFunctionElement_h
