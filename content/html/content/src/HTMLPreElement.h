/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

class HTMLPreElement MOZ_FINAL : public nsGenericHTMLElement,
                                 public nsIDOMHTMLPreElement
{
public:
  HTMLPreElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }
  virtual ~HTMLPreElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLPreElement
  NS_IMETHOD GetWidth(int32_t* aWidth) MOZ_OVERRIDE;
  NS_IMETHOD SetWidth(int32_t aWidth) MOZ_OVERRIDE;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

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
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLPreElement_h
