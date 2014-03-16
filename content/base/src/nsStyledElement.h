/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
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
#include "nsString.h"
#include "mozilla/dom/Element.h"

namespace mozilla {
namespace css {
class StyleRule;
}
}

typedef mozilla::dom::Element nsStyledElementBase;

class nsStyledElementNotElementCSSInlineStyle : public nsStyledElementBase
{

protected:

  inline nsStyledElementNotElementCSSInlineStyle(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : nsStyledElementBase(aNodeInfo)
  {}

public:
  // nsIContent interface methods
  virtual nsIAtom* GetClassAttributeName() const MOZ_OVERRIDE;
  virtual nsIAtom* GetIDAttributeName() const MOZ_OVERRIDE;
  virtual nsIAtom* DoGetID() const MOZ_OVERRIDE;
  virtual const nsAttrValue* DoGetClasses() const MOZ_OVERRIDE;

  virtual mozilla::css::StyleRule* GetInlineStyleRule();
  virtual nsresult SetInlineStyleRule(mozilla::css::StyleRule* aStyleRule,
                                      const nsAString* aSerialized,
                                      bool aNotify) MOZ_OVERRIDE;

  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  nsICSSDeclaration* Style();

protected:

  /**
   * Parse a style attr value into a CSS rulestruct (or, if there is no
   * document, leave it as a string) and return as nsAttrValue.
   *
   * @param aValue the value to parse
   * @param aResult the resulting HTMLValue [OUT]
   */
  void ParseStyleAttribute(const nsAString& aValue,
                           nsAttrValue& aResult,
                           bool aForceInDataDoc);

  virtual bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                                const nsAString& aValue, nsAttrValue& aResult) MOZ_OVERRIDE;

  friend class mozilla::dom::Element;

  /**
   * Create the style struct from the style attr.  Used when an element is
   * first put into a document.  Only has an effect if the old value is a
   * string.  If aForceInDataDoc is true, will reparse even if we're in a data
   * document.
   */
  nsresult  ReparseStyleAttribute(bool aForceInDataDoc);
};

class nsStyledElement : public nsStyledElementNotElementCSSInlineStyle {
protected:
  inline nsStyledElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : nsStyledElementNotElementCSSInlineStyle(aNodeInfo)
  {}
};

#endif // __NS_STYLEDELEMENT_H_
