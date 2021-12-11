/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nsStyledElement is the base for elements supporting styling via the
 * id/class/style attributes; it is a common base for their support in HTML,
 * SVG and MathML.
 */

#ifndef __NS_STYLEDELEMENT_H_
#define __NS_STYLEDELEMENT_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "nsString.h"

namespace mozilla {
class DeclarationBlock;
struct MutationClosureData;
}  // namespace mozilla

// IID for nsStyledElement interface
#define NS_STYLED_ELEMENT_IID                        \
  {                                                  \
    0xacbd9ea6, 0x15aa, 0x4f37, {                    \
      0x8c, 0xe0, 0x35, 0x1e, 0xd7, 0x21, 0xca, 0xe9 \
    }                                                \
  }

using nsStyledElementBase = mozilla::dom::Element;

class nsStyledElement : public nsStyledElementBase {
 protected:
  inline explicit nsStyledElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsStyledElementBase(std::move(aNodeInfo)) {}

 public:
  // We don't want to implement AddRef/Release because that would add an extra
  // function call for those on pretty much all elements.  But we do need QI, so
  // we can QI to nsStyledElement.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  // Element interface methods
  virtual void InlineStyleDeclarationWillChange(
      mozilla::MutationClosureData& aData) override;
  virtual nsresult SetInlineStyleDeclaration(
      mozilla::DeclarationBlock& aDeclaration,
      mozilla::MutationClosureData& aData) override;

  nsICSSDeclaration* Style();

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_STYLED_ELEMENT_IID)
  NS_IMPL_FROMNODE_HELPER(nsStyledElement, IsStyledElement());

  bool IsStyledElement() const final { return true; }

 protected:
  nsICSSDeclaration* GetExistingStyle();

  /**
   * Parse a style attr value into a CSS rulestruct (or, if there is no
   * document, leave it as a string) and return as nsAttrValue.
   *
   * @param aValue the value to parse
   * @param aMaybeScriptedPrincipal if available, the scripted principal
   *        responsible for this attribute value, as passed to
   *        Element::ParseAttribute.
   * @param aResult the resulting HTMLValue [OUT]
   */
  void ParseStyleAttribute(const nsAString& aValue,
                           nsIPrincipal* aMaybeScriptedPrincipal,
                           nsAttrValue& aResult, bool aForceInDataDoc);

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  friend class mozilla::dom::Element;

  /**
   * Create the style struct from the style attr.  Used when an element is
   * first put into a document.  Only has an effect if the old value is a
   * string.  If aForceInDataDoc is true, will reparse even if we're in a data
   * document.
   */
  nsresult ReparseStyleAttribute(bool aForceInDataDoc);

  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsStyledElement, NS_STYLED_ELEMENT_IID)
#endif  // __NS_STYLEDELEMENT_H_
