/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

class HTMLHRElement final : public nsGenericHTMLElement,
                            public nsIDOMHTMLHRElement
{
public:
  explicit HTMLHRElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLHRElement
  NS_DECL_NSIDOMHTMLHRELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

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
  virtual ~HTMLHRElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    GenericSpecifiedValues* aGenericData);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLHRElement_h
