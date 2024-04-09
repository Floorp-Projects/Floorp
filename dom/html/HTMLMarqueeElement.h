/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HTMLMarqueeElement_h___
#define HTMLMarqueeElement_h___

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsContentUtils.h"

namespace mozilla::dom {
class HTMLMarqueeElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLMarqueeElement(already_AddRefed<dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLMarqueeElement, marquee);

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(UnbindContext&) override;

  static const int kDefaultLoop = -1;
  static const int kDefaultScrollAmount = 6;
  static const int kDefaultScrollDelayMS = 85;

  void GetBehavior(nsAString& aValue);
  void SetBehavior(const nsAString& aValue, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::behavior, aValue, aError);
  }

  void GetDirection(nsAString& aValue);
  void SetDirection(const nsAString& aValue, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::direction, aValue, aError);
  }

  void GetBgColor(DOMString& aBgColor) {
    GetHTMLAttr(nsGkAtoms::bgcolor, aBgColor);
  }
  void SetBgColor(const nsAString& aBgColor, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::bgcolor, aBgColor, aError);
  }
  void GetHeight(DOMString& aHeight) {
    GetHTMLAttr(nsGkAtoms::height, aHeight);
  }
  void SetHeight(const nsAString& aHeight, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::height, aHeight, aError);
  }
  uint32_t Hspace() {
    return GetDimensionAttrAsUnsignedInt(nsGkAtoms::hspace, 0);
  }
  void SetHspace(uint32_t aValue, ErrorResult& aError) {
    SetUnsignedIntAttr(nsGkAtoms::hspace, aValue, 0, aError);
  }
  int32_t Loop() {
    int loop = GetIntAttr(nsGkAtoms::loop, kDefaultLoop);
    if (loop <= 0) {
      loop = -1;
    }

    return loop;
  }
  void SetLoop(int32_t aValue, ErrorResult& aError) {
    if (aValue == -1 || aValue > 0) {
      SetHTMLIntAttr(nsGkAtoms::loop, aValue, aError);
    }
  }
  uint32_t ScrollAmount() {
    return GetUnsignedIntAttr(nsGkAtoms::scrollamount, kDefaultScrollAmount);
  }
  void SetScrollAmount(uint32_t aValue, ErrorResult& aError) {
    SetUnsignedIntAttr(nsGkAtoms::scrollamount, aValue, kDefaultScrollAmount,
                       aError);
  }
  uint32_t ScrollDelay() {
    return GetUnsignedIntAttr(nsGkAtoms::scrolldelay, kDefaultScrollDelayMS);
  }
  void SetScrollDelay(uint32_t aValue, ErrorResult& aError) {
    SetUnsignedIntAttr(nsGkAtoms::scrolldelay, aValue, kDefaultScrollDelayMS,
                       aError);
  }
  bool TrueSpeed() const { return GetBoolAttr(nsGkAtoms::truespeed); }
  void SetTrueSpeed(bool aValue, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::truespeed, aValue, aError);
  }
  void GetWidth(DOMString& aWidth) { GetHTMLAttr(nsGkAtoms::width, aWidth); }
  void SetWidth(const nsAString& aWidth, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::width, aWidth, aError);
  }
  uint32_t Vspace() {
    return GetDimensionAttrAsUnsignedInt(nsGkAtoms::vspace, 0);
  }
  void SetVspace(uint32_t aValue, ErrorResult& aError) {
    SetUnsignedIntAttr(nsGkAtoms::vspace, aValue, 0, aError);
  }

  void Start();
  void Stop();

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

 protected:
  virtual ~HTMLMarqueeElement();

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 private:
  static void MapAttributesIntoRule(MappedDeclarationsBuilder&);

  void DispatchEventToShadowRoot(const nsAString& aEventTypeArg);
};

}  // namespace mozilla::dom

#endif /* HTMLMarqueeElement_h___ */
