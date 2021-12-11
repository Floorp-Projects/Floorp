/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MathMLElement_h_
#define mozilla_dom_MathMLElement_h_

#include "mozilla/Attributes.h"
#include "nsMappedAttributeElement.h"
#include "Link.h"

class nsCSSValue;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

typedef nsMappedAttributeElement MathMLElementBase;

/*
 * The base class for MathML elements.
 */
class MathMLElement final : public MathMLElementBase,
                            public mozilla::dom::Link {
 public:
  explicit MathMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  explicit MathMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // Implementation of nsISupports is inherited from MathMLElementBase
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMNODE(MathMLElement, kNameSpaceID_MathML)

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;

  enum {
    PARSE_ALLOW_UNITLESS = 0x01,  // unitless 0 will be turned into 0px
    PARSE_ALLOW_NEGATIVE = 0x02,
    PARSE_SUPPRESS_WARNINGS = 0x04,
    CONVERT_UNITLESS_TO_PERCENT = 0x08
  };
  static bool ParseNamedSpaceValue(const nsString& aString,
                                   nsCSSValue& aCSSValue, uint32_t aFlags,
                                   const Document& aDocument);

  static bool ParseNumericValue(const nsString& aString, nsCSSValue& aCSSValue,
                                uint32_t aFlags, Document* aDocument);

  static void MapMathMLAttributesInto(const nsMappedAttributes* aAttributes,
                                      mozilla::MappedDeclarations&);

  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;
  MOZ_CAN_RUN_SCRIPT
  nsresult PostHandleEvent(mozilla::EventChainPostVisitor& aVisitor) override;
  nsresult Clone(mozilla::dom::NodeInfo*, nsINode** aResult) const override;
  virtual mozilla::EventStates IntrinsicState() const override;
  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  // Set during reflow as necessary. Does a style change notification,
  // aNotify must be true.
  void SetIncrementScriptLevel(bool aIncrementScriptLevel, bool aNotify);
  bool GetIncrementScriptLevel() const { return mIncrementScriptLevel; }

  int32_t TabIndexDefault() final;
  virtual bool IsFocusableInternal(int32_t* aTabIndex,
                                   bool aWithMouse) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;

  virtual void NodeInfoChanged(Document* aOldDoc) override {
    ClearHasPendingLinkUpdate();
    MathMLElementBase::NodeInfoChanged(aOldDoc);
  }

  void RecompileScriptEventListeners() final;
  bool IsEventAttributeNameInternal(nsAtom* aName) final;

 protected:
  virtual ~MathMLElement() = default;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  nsresult BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                         const nsAttrValueOrString* aValue, bool aNotify) final;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

 private:
  bool mIncrementScriptLevel;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MathMLElement_h_
