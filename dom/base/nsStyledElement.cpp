/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStyledElement.h"
#include "nsGkAtoms.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/InternalMutationEvent.h"
#include "nsDOMCSSDeclaration.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocument.h"
#include "mozilla/css/StyleRule.h"
#include "nsCSSParser.h"
#include "mozilla/css/Loader.h"
#include "nsIDOMMutationEvent.h"
#include "nsXULElement.h"
#include "nsContentUtils.h"
#include "nsStyleUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
// nsIContent methods

bool
nsStyledElementNotElementCSSInlineStyle::ParseAttribute(int32_t aNamespaceID,
                                                        nsIAtom* aAttribute,
                                                        const nsAString& aValue,
                                                        nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::style && aNamespaceID == kNameSpaceID_None) {
    SetMayHaveStyle();
    ParseStyleAttribute(aValue, aResult, false);
    return true;
  }

  return nsStyledElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                             aResult);
}

nsresult
nsStyledElementNotElementCSSInlineStyle::SetInlineStyleRule(css::StyleRule* aStyleRule,
                                                            const nsAString* aSerialized,
                                                            bool aNotify)
{
  SetMayHaveStyle();
  bool modification = false;
  nsAttrValue oldValue;

  bool hasListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

  // There's no point in comparing the stylerule pointers since we're always
  // getting a new stylerule here. And we can't compare the stringvalues of
  // the old and the new rules since both will point to the same declaration
  // and thus will be the same.
  if (hasListeners) {
    // save the old attribute so we can set up the mutation event properly
    // XXXbz if the old rule points to the same declaration as the new one,
    // this is getting the new attr value, not the old one....
    nsAutoString oldValueStr;
    modification = GetAttr(kNameSpaceID_None, nsGkAtoms::style,
                           oldValueStr);
    if (modification) {
      oldValue.SetTo(oldValueStr);
    }
  }
  else if (aNotify && IsInDoc()) {
    modification = !!mAttrsAndChildren.GetAttr(nsGkAtoms::style);
  }

  nsAttrValue attrValue(aStyleRule, aSerialized);

  // XXXbz do we ever end up with ADDITION here?  I doubt it.
  uint8_t modType = modification ?
    static_cast<uint8_t>(nsIDOMMutationEvent::MODIFICATION) :
    static_cast<uint8_t>(nsIDOMMutationEvent::ADDITION);

  return SetAttrAndNotify(kNameSpaceID_None, nsGkAtoms::style, nullptr,
                          oldValue, attrValue, modType, hasListeners,
                          aNotify, kDontCallAfterSetAttr);
}

css::StyleRule*
nsStyledElementNotElementCSSInlineStyle::GetInlineStyleRule()
{
  if (!MayHaveStyle()) {
    return nullptr;
  }
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);

  if (attrVal && attrVal->Type() == nsAttrValue::eCSSStyleRule) {
    return attrVal->GetCSSStyleRuleValue();
  }

  return nullptr;
}

// ---------------------------------------------------------------
// Others and helpers

nsICSSDeclaration*
nsStyledElementNotElementCSSInlineStyle::Style()
{
  Element::nsDOMSlots *slots = DOMSlots();

  if (!slots->mStyle) {
    // Just in case...
    ReparseStyleAttribute(true);

    slots->mStyle = new nsDOMCSSAttributeDeclaration(this, false);
    SetMayHaveStyle();
  }

  return slots->mStyle;
}

nsresult
nsStyledElementNotElementCSSInlineStyle::ReparseStyleAttribute(bool aForceInDataDoc)
{
  if (!MayHaveStyle()) {
    return NS_OK;
  }
  const nsAttrValue* oldVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);
  
  if (oldVal && oldVal->Type() != nsAttrValue::eCSSStyleRule) {
    nsAttrValue attrValue;
    nsAutoString stringValue;
    oldVal->ToString(stringValue);
    ParseStyleAttribute(stringValue, attrValue, aForceInDataDoc);
    // Don't bother going through SetInlineStyleRule, we don't want to fire off
    // mutation events or document notifications anyway
    nsresult rv = mAttrsAndChildren.SetAndTakeAttr(nsGkAtoms::style, attrValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  return NS_OK;
}

void
nsStyledElementNotElementCSSInlineStyle::ParseStyleAttribute(const nsAString& aValue,
                                                             nsAttrValue& aResult,
                                                             bool aForceInDataDoc)
{
  nsIDocument* doc = OwnerDoc();

  if (!nsStyleUtil::CSPAllowsInlineStyle(nullptr, NodePrincipal(),
                                         doc->GetDocumentURI(), 0, aValue,
                                         nullptr))
    return;

  if (aForceInDataDoc ||
      !doc->IsLoadedAsData() ||
      doc->IsStaticDocument()) {
    bool isCSS = true; // assume CSS until proven otherwise

    if (!IsInNativeAnonymousSubtree()) {  // native anonymous content
                                          // always assumes CSS
      nsAutoString styleType;
      doc->GetHeaderData(nsGkAtoms::headerContentStyleType, styleType);
      if (!styleType.IsEmpty()) {
        static const char textCssStr[] = "text/css";
        isCSS = (styleType.EqualsIgnoreCase(textCssStr, sizeof(textCssStr) - 1));
      }
    }

    if (isCSS && aResult.ParseStyleAttribute(aValue, this)) {
      return;
    }
  }

  aResult.SetTo(aValue);
}
