/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLBRElement_h
#define mozilla_dom_HTMLBRElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

class HTMLBRElement MOZ_FINAL : public nsGenericHTMLElement,
                                public nsIDOMHTMLBRElement
{
public:
  HTMLBRElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual ~HTMLBRElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLBRElement
  NS_DECL_NSIDOMHTMLBRELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  bool Clear()
  {
    return GetBoolAttr(nsGkAtoms::clear);
  }
  void SetClear(const nsAString& aClear, ErrorResult& aError)
  {
    return SetHTMLAttr(nsGkAtoms::clear, aClear, aError);
  }

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);
};

} // namespace dom
} // namespace mozilla

#endif

