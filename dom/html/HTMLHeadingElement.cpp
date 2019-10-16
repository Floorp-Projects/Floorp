/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLHeadingElement.h"
#include "mozilla/dom/HTMLHeadingElementBinding.h"

#include "mozilla/MappedDeclarations.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "mozAutoDocUpdate.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Heading)

namespace mozilla {
namespace dom {

HTMLHeadingElement::~HTMLHeadingElement() {}

NS_IMPL_ELEMENT_CLONE(HTMLHeadingElement)

JSObject* HTMLHeadingElement::WrapNode(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return HTMLHeadingElement_Binding::Wrap(aCx, this, aGivenProto);
}

int32_t HTMLHeadingElement::AccessibilityLevel() const {
  int32_t level = BaseAccessibilityLevel();
  if (!StaticPrefs::accessibility_heading_element_level_changes_enabled()) {
    return level;
  }

  // What follows is an implementation of
  // the proposal in https://github.com/whatwg/html/issues/5002.
  //
  // FIXME(emilio): Should this really use the light tree rather than the flat
  // tree?
  nsAtom* name = NodeInfo()->NameAtom();
  nsINode* parent = GetParentNode();
  const bool considerAncestors =
      name == nsGkAtoms::h1 ||
      (parent && parent->IsHTMLElement(nsGkAtoms::hgroup));
  if (!considerAncestors) {
    return level;
  }

  for (; parent; parent = parent->GetParentNode()) {
    if (parent->IsAnyOfHTMLElements(nsGkAtoms::section, nsGkAtoms::article,
                                    nsGkAtoms::aside, nsGkAtoms::nav)) {
      level++;
    }
  }
  return level;
}

bool HTMLHeadingElement::ParseAttribute(int32_t aNamespaceID,
                                        nsAtom* aAttribute,
                                        const nsAString& aValue,
                                        nsIPrincipal* aMaybeScriptedPrincipal,
                                        nsAttrValue& aResult) {
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return ParseDivAlignValue(aValue, aResult);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLHeadingElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLHeadingElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry* const map[] = {sDivAlignAttributeMap,
                                                    sCommonAttributeMap};

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLHeadingElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

}  // namespace dom
}  // namespace mozilla
