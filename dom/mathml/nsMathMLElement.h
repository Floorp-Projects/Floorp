/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLElement_h
#define nsMathMLElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "nsMappedAttributeElement.h"
#include "nsIDOMElement.h"
#include "Link.h"
#include "mozilla/dom/DOMRect.h"

class nsCSSValue;

typedef nsMappedAttributeElement nsMathMLElementBase;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
} // namespace mozilla

/*
 * The base class for MathML elements.
 */
class nsMathMLElement final : public nsMathMLElementBase,
                              public nsIDOMElement,
                              public mozilla::dom::Link
{
public:
  explicit nsMathMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  explicit nsMathMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // Implementation of nsISupports is inherited from nsMathMLElementBase
  NS_DECL_ISUPPORTS_INHERITED

  // Forward implementations of parent interfaces of nsMathMLElement to
  // our base class
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                      nsIContent* aBindingParent,
                      bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) override;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  enum {
    PARSE_ALLOW_UNITLESS = 0x01, // unitless 0 will be turned into 0px
    PARSE_ALLOW_NEGATIVE = 0x02,
    PARSE_SUPPRESS_WARNINGS = 0x04,
    CONVERT_UNITLESS_TO_PERCENT = 0x08
  };
  static bool ParseNamedSpaceValue(const nsString& aString,
                                   nsCSSValue&     aCSSValue,
                                   uint32_t        aFlags);

  static bool ParseNumericValue(const nsString& aString,
                                nsCSSValue&     aCSSValue,
                                uint32_t        aFlags,
                                nsIDocument*    aDocument);

  static void MapMathMLAttributesInto(const nsMappedAttributes* aAttributes,
                                      mozilla::GenericSpecifiedValues* aGenericData);

  virtual nsresult GetEventTargetParent(
                     mozilla::EventChainPreVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(
                     mozilla::EventChainPostVisitor& aVisitor) override;
  nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                 bool aPreallocateChildren) const override;
  virtual mozilla::EventStates IntrinsicState() const override;
  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  // Set during reflow as necessary. Does a style change notification,
  // aNotify must be true.
  void SetIncrementScriptLevel(bool aIncrementScriptLevel, bool aNotify);
  bool GetIncrementScriptLevel() const {
    return mIncrementScriptLevel;
  }

  virtual bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;

  virtual nsIDOMNode* AsDOMNode() override { return this; }

  virtual void NodeInfoChanged(nsIDocument* aOldDoc) override
  {
    ClearHasPendingLinkUpdate();
    nsMathMLElementBase::NodeInfoChanged(aOldDoc);
  }

protected:
  virtual ~nsMathMLElement() {}

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                bool aNotify) override;

private:
  bool mIncrementScriptLevel;
};

#endif // nsMathMLElement_h
