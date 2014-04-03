/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLElement_h
#define nsMathMLElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ElementInlines.h"
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
class nsMathMLElement MOZ_FINAL : public nsMathMLElementBase,
                                  public nsIDOMElement,
                                  public mozilla::dom::Link
{
public:
  nsMathMLElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  nsMathMLElement(already_AddRefed<nsINodeInfo>&& aNodeInfo);

  // Implementation of nsISupports is inherited from nsMathMLElementBase
  NS_DECL_ISUPPORTS_INHERITED

  // Forward implementations of parent interfaces of nsMathMLElement to 
  // our base class
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                      nsIContent* aBindingParent,
                      bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;

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
                                      nsRuleData* aRuleData);
  
  virtual nsresult PreHandleEvent(
                     mozilla::EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult PostHandleEvent(
                     mozilla::EventChainPostVisitor& aVisitor) MOZ_OVERRIDE;
  nsresult Clone(nsINodeInfo*, nsINode**) const MOZ_OVERRIDE;
  virtual mozilla::EventStates IntrinsicState() const MOZ_OVERRIDE;
  virtual bool IsNodeOfType(uint32_t aFlags) const MOZ_OVERRIDE;

  // Set during reflow as necessary. Does a style change notification,
  // aNotify must be true.
  void SetIncrementScriptLevel(bool aIncrementScriptLevel, bool aNotify);
  bool GetIncrementScriptLevel() const {
    return mIncrementScriptLevel;
  }

  virtual bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) MOZ_OVERRIDE;
  virtual bool IsLink(nsIURI** aURI) const MOZ_OVERRIDE;
  virtual void GetLinkTarget(nsAString& aTarget) MOZ_OVERRIDE;
  virtual already_AddRefed<nsIURI> GetHrefURI() const MOZ_OVERRIDE;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;

  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

protected:
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

private:
  bool mIncrementScriptLevel;
};

#endif // nsMathMLElement_h
