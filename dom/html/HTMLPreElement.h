/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLPreElement_h
#define mozilla_dom_HTMLPreElement_h

#include "mozilla/Attributes.h"

#include "nsIDOMHTMLPreElement.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLPreElement final : public nsGenericHTMLElement,
                             public nsIDOMHTMLPreElement
{
public:
  explicit HTMLPreElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLPreElement
  NS_IMETHOD GetWidth(int32_t* aWidth) override;
  NS_IMETHOD SetWidth(int32_t aWidth) override;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  // WebIDL API
  int32_t Width() const
  {
    return GetIntAttr(nsGkAtoms::width, 0);
  }
  void SetWidth(int32_t aWidth, mozilla::ErrorResult& rv)
  {
    rv = SetIntAttr(nsGkAtoms::width, aWidth);
  }

protected:
  virtual ~HTMLPreElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    GenericSpecifiedValues* aGenericData);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLPreElement_h
