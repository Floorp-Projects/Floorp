/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSharedListElement_h
#define mozilla_dom_HTMLSharedListElement_h

#include "mozilla/Attributes.h"

#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLSharedListElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLSharedListElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  bool Reversed() const { return GetBoolAttr(nsGkAtoms::reversed); }
  void SetReversed(bool aReversed, mozilla::ErrorResult& rv) {
    SetHTMLBoolAttr(nsGkAtoms::reversed, aReversed, rv);
  }
  int32_t Start() const { return GetIntAttr(nsGkAtoms::start, 1); }
  void SetStart(int32_t aStart, mozilla::ErrorResult& rv) {
    SetHTMLIntAttr(nsGkAtoms::start, aStart, rv);
  }
  void GetType(DOMString& aType) { GetHTMLAttr(nsGkAtoms::type, aType); }
  void SetType(const nsAString& aType, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::type, aType, rv);
  }
  bool Compact() const { return GetBoolAttr(nsGkAtoms::compact); }
  void SetCompact(bool aCompact, mozilla::ErrorResult& rv) {
    SetHTMLBoolAttr(nsGkAtoms::compact, aCompact, rv);
  }

 protected:
  virtual ~HTMLSharedListElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);
  static void MapOLAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                      MappedDeclarations&);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLSharedListElement_h
