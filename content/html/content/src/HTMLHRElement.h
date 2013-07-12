/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_dom_HTMLHRElement_h
#define mozilla_dom_HTMLHRElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLHRElement.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsRuleData.h"

namespace mozilla {
namespace dom {

class HTMLHRElement MOZ_FINAL : public nsGenericHTMLElement,
                                public nsIDOMHTMLHRElement
{
public:
  HTMLHRElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLHRElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLHRElement
  NS_DECL_NSIDOMHTMLHRELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

  // WebIDL API
  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }

  // The XPCOM GetColor is OK for us
  void SetColor(const nsAString& aColor, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::color, aColor, aError);
  }

  bool NoShade() const
  {
   return GetBoolAttr(nsGkAtoms::noshade);
  }
  void SetNoShade(bool aNoShade, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::noshade, aNoShade, aError);
  }

  // The XPCOM GetSize is OK for us
  void SetSize(const nsAString& aSize, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::size, aSize, aError);
  }

  // The XPCOM GetWidth is OK for us
  void SetWidth(const nsAString& aWidth, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::width, aWidth, aError);
  }

protected:
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLHRElement_h
