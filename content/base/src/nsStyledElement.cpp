/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStyledElement.h"
#include "nsGkAtoms.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/MutationEvent.h"
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

nsIAtom*
nsStyledElementNotElementCSSInlineStyle::GetClassAttributeName() const
{
  return nsGkAtoms::_class;
}

nsIAtom*
nsStyledElementNotElementCSSInlineStyle::GetIDAttributeName() const
{
  return nsGkAtoms::id;
}

nsIAtom*
nsStyledElementNotElementCSSInlineStyle::DoGetID() const
{
  NS_ASSERTION(HasID(), "Unexpected call");

  // The nullcheck here is needed because Element::UnsetAttr calls
  // out to various code between removing the attribute and we get a chance to
  // ClearHasID().

  const nsAttrValue* attr = mAttrsAndChildren.GetAttr(nsGkAtoms::id);

  return attr ? attr->GetAtomValue() : nullptr;
}

const nsAttrValue*
nsStyledElementNotElementCSSInlineStyle::DoGetClasses() const
{
  NS_ASSERTION(HasFlag(NODE_MAY_HAVE_CLASS), "Unexpected call");
  return mAttrsAndChildren.GetAttr(nsGkAtoms::_class);
}

bool
nsStyledElementNotElementCSSInlineStyle::ParseAttribute(int32_t aNamespaceID,
                                                        nsIAtom* aAttribute,
                                                        const nsAString& aValue,
                                                        nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::style) {
      SetMayHaveStyle();
      ParseStyleAttribute(aValue, aResult, false);
      return true;
    }
    if (aAttribute == nsGkAtoms::_class) {
      SetFlags(NODE_MAY_HAVE_CLASS);
      aResult.ParseAtomArray(aValue);
      return true;
    }
    if (aAttribute == nsGkAtoms::id) {
      // Store id as an atom.  id="" means that the element has no id,
      // not that it has an emptystring as the id.
      RemoveFromIdTable();
      if (aValue.IsEmpty()) {
        ClearHasID();
        return false;
      }
      aResult.ParseAtom(aValue);
      SetHasID();
      AddToIdTable(aResult.GetAtomValue());
      return true;
    }
  }

  return nsStyledElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                             aResult);
}

nsresult
nsStyledElementNotElementCSSInlineStyle::UnsetAttr(int32_t aNameSpaceID,
                                                   nsIAtom* aAttribute,
                                                   bool aNotify)
{
  nsAutoScriptBlocker scriptBlocker;
  if (aAttribute == nsGkAtoms::id && aNameSpaceID == kNameSpaceID_None) {
    // Have to do this before clearing flag. See RemoveFromIdTable
    RemoveFromIdTable();
  }

  return Element::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

nsresult
nsStyledElementNotElementCSSInlineStyle::AfterSetAttr(int32_t aNamespaceID,
                                                      nsIAtom* aAttribute,
                                                      const nsAttrValue* aValue,
                                                      bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None && !aValue &&
      aAttribute == nsGkAtoms::id) {
    // The id has been removed when calling UnsetAttr but we kept it because
    // the id is used for some layout stuff between UnsetAttr and AfterSetAttr.
    // Now. the id is really removed so it would not be safe to keep this flag.
    ClearHasID();
  }

  return Element::AfterSetAttr(aNamespaceID, aAttribute, aValue, aNotify);
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
