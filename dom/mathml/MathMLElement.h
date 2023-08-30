/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MathMLElement_h_
#define mozilla_dom_MathMLElement_h_

#include "mozilla/Attributes.h"
#include "nsStyledElement.h"
#include "Link.h"

class nsCSSValue;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

using MathMLElementBase = nsStyledElement;

/*
 * The base class for MathML elements.
 */
class MathMLElement final : public MathMLElementBase, public Link {
 public:
  explicit MathMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  explicit MathMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // Implementation of nsISupports is inherited from MathMLElementBase
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMNODE(MathMLElement, kNameSpaceID_MathML)

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(bool aNullParent = true) override;

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  enum {
    PARSE_ALLOW_NEGATIVE = 0x02,
    PARSE_SUPPRESS_WARNINGS = 0x04,
  };
  static bool ParseNamedSpaceValue(const nsString& aString,
                                   nsCSSValue& aCSSValue, uint32_t aFlags,
                                   const Document& aDocument);

  static bool ParseNumericValue(const nsString& aString, nsCSSValue& aCSSValue,
                                uint32_t aFlags, Document* aDocument);

  static void MapGlobalMathMLAttributesInto(
      mozilla::MappedDeclarationsBuilder&);
  static void MapMiAttributesInto(mozilla::MappedDeclarationsBuilder&);
  static void MapMTableAttributesInto(mozilla::MappedDeclarationsBuilder&);

  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;
  MOZ_CAN_RUN_SCRIPT
  nsresult PostHandleEvent(mozilla::EventChainPostVisitor& aVisitor) override;
  nsresult Clone(mozilla::dom::NodeInfo*, nsINode** aResult) const override;

  // Set during reflow as necessary. Does a style change notification,
  // aNotify must be true.
  void SetIncrementScriptLevel(bool aIncrementScriptLevel, bool aNotify);
  bool GetIncrementScriptLevel() const {
    return Element::State().HasState(ElementState::INCREMENT_SCRIPT_LEVEL);
  }

  int32_t TabIndexDefault() final;

  bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) override;
  already_AddRefed<nsIURI> GetHrefURI() const override;

  void NodeInfoChanged(Document* aOldDoc) override {
    ClearHasPendingLinkUpdate();
    MathMLElementBase::NodeInfoChanged(aOldDoc);
  }

  bool IsEventAttributeNameInternal(nsAtom* aName) final;

  bool Autofocus() const { return GetBoolAttr(nsGkAtoms::autofocus); }
  void SetAutofocus(bool aAutofocus, ErrorResult& aRv) {
    if (aAutofocus) {
      SetAttr(nsGkAtoms::autofocus, u""_ns, aRv);
    } else {
      UnsetAttr(nsGkAtoms::autofocus, aRv);
    }
  }

 protected:
  virtual ~MathMLElement() = default;

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  void BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                     const nsAttrValue* aValue, bool aNotify) final;
  void AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MathMLElement_h_
