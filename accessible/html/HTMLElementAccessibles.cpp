/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLElementAccessibles.h"

#include "DocAccessible.h"
#include "nsAccUtils.h"
#include "nsIPersistentProperties2.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

#include "mozilla/dom/HTMLLabelElement.h"
#include "mozilla/dom/HTMLDetailsElement.h"
#include "mozilla/dom/HTMLSummaryElement.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLHRAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLHRAccessible::NativeRole() const { return roles::SEPARATOR; }

////////////////////////////////////////////////////////////////////////////////
// HTMLBRAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLBRAccessible::NativeRole() const { return roles::WHITESPACE; }

uint64_t HTMLBRAccessible::NativeState() const { return states::READONLY; }

ENameValueFlag HTMLBRAccessible::NativeName(nsString& aName) const {
  aName = static_cast<char16_t>('\n');  // Newline char
  return eNameOK;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLabelAccessible
////////////////////////////////////////////////////////////////////////////////

ENameValueFlag HTMLLabelAccessible::NativeName(nsString& aName) const {
  nsTextEquivUtils::GetNameFromSubtree(this, aName);
  return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
}

Relation HTMLLabelAccessible::RelationByType(RelationType aType) const {
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType == RelationType::LABEL_FOR) {
    dom::HTMLLabelElement* label = dom::HTMLLabelElement::FromNode(mContent);
    rel.AppendTarget(mDoc, label->GetControl());
  }

  return rel;
}

uint8_t HTMLLabelAccessible::ActionCount() const {
  return nsCoreUtils::IsLabelWithControl(mContent) ? 1 : 0;
}

void HTMLLabelAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == 0) {
    if (nsCoreUtils::IsLabelWithControl(mContent)) aName.AssignLiteral("click");
  }
}

bool HTMLLabelAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != 0) return false;

  DoCommand();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLOuputAccessible
////////////////////////////////////////////////////////////////////////////////

Relation HTMLOutputAccessible::RelationByType(RelationType aType) const {
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType == RelationType::CONTROLLED_BY)
    rel.AppendIter(new IDRefsIterator(mDoc, mContent, nsGkAtoms::_for));

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSummaryAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLSummaryAccessible::HTMLSummaryAccessible(nsIContent* aContent,
                                             DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {
  mGenericTypes |= eButton;
}

uint8_t HTMLSummaryAccessible::ActionCount() const { return 1; }

void HTMLSummaryAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex != eAction_Click) {
    return;
  }

  dom::HTMLSummaryElement* summary =
      dom::HTMLSummaryElement::FromNode(mContent);
  if (!summary) {
    return;
  }

  dom::HTMLDetailsElement* details = summary->GetDetails();
  if (!details) {
    return;
  }

  if (details->Open()) {
    aName.AssignLiteral("collapse");
  } else {
    aName.AssignLiteral("expand");
  }
}

bool HTMLSummaryAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != eAction_Click) return false;

  DoCommand();
  return true;
}

uint64_t HTMLSummaryAccessible::NativeState() const {
  uint64_t state = HyperTextAccessibleWrap::NativeState();

  dom::HTMLSummaryElement* summary =
      dom::HTMLSummaryElement::FromNode(mContent);
  if (!summary) {
    return state;
  }

  dom::HTMLDetailsElement* details = summary->GetDetails();
  if (!details) {
    return state;
  }

  if (details->Open()) {
    state |= states::EXPANDED;
  } else {
    state |= states::COLLAPSED;
  }

  return state;
}

HTMLSummaryAccessible* HTMLSummaryAccessible::FromDetails(Accessible* details) {
  if (!dom::HTMLDetailsElement::FromNodeOrNull(details->GetContent())) {
    return nullptr;
  }

  HTMLSummaryAccessible* summaryAccessible = nullptr;
  for (uint32_t i = 0; i < details->ChildCount(); i++) {
    // Iterate through the children of our details accessible to locate main
    // summary. This iteration includes the anonymous summary if the details
    // element was not explicitly created with one.
    Accessible* child = details->GetChildAt(i);
    auto* summary =
        mozilla::dom::HTMLSummaryElement::FromNodeOrNull(child->GetContent());
    if (summary && summary->IsMainSummary()) {
      summaryAccessible = static_cast<HTMLSummaryAccessible*>(child);
      break;
    }
  }

  MOZ_ASSERT(summaryAccessible,
             "Details objects should have at least one summary");
  return summaryAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSummaryAccessible: Widgets

bool HTMLSummaryAccessible::IsWidget() const { return true; }

////////////////////////////////////////////////////////////////////////////////
// HTMLHeaderOrFooterAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLHeaderOrFooterAccessible::NativeRole() const {
  // Only map header and footer if they are direct descendants of the body tag.
  // If other sectioning or sectioning root elements, they become sections.
  nsIContent* parent = mContent->GetParent();
  while (parent) {
    if (parent->IsAnyOfHTMLElements(
            nsGkAtoms::article, nsGkAtoms::aside, nsGkAtoms::nav,
            nsGkAtoms::section, nsGkAtoms::main, nsGkAtoms::blockquote,
            nsGkAtoms::details, nsGkAtoms::dialog, nsGkAtoms::fieldset,
            nsGkAtoms::figure, nsGkAtoms::td)) {
      break;
    }
    parent = parent->GetParent();
  }

  // No sectioning or sectioning root elements found.
  if (!parent) {
    return roles::LANDMARK;
  }

  return roles::SECTION;
}

nsAtom* HTMLHeaderOrFooterAccessible::LandmarkRole() const {
  if (!HasOwnContent()) return nullptr;

  a11y::role r = const_cast<HTMLHeaderOrFooterAccessible*>(this)->Role();
  if (r == roles::LANDMARK) {
    if (mContent->IsHTMLElement(nsGkAtoms::header)) {
      return nsGkAtoms::banner;
    }

    if (mContent->IsHTMLElement(nsGkAtoms::footer)) {
      return nsGkAtoms::contentinfo;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSectionAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLSectionAccessible::NativeRole() const {
  nsAutoString name;
  const_cast<HTMLSectionAccessible*>(this)->Name(name);
  return name.IsEmpty() ? roles::SECTION : roles::REGION;
}

nsAtom* HTMLSectionAccessible::LandmarkRole() const {
  if (!HasOwnContent()) {
    return nullptr;
  }

  // Only return xml-roles "region" if the section has an accessible name.
  nsAutoString name;
  const_cast<HTMLSectionAccessible*>(this)->Name(name);
  return name.IsEmpty() ? nullptr : nsGkAtoms::region;
}
