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

role
HTMLHRAccessible::NativeRole() const
{
  return roles::SEPARATOR;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLBRAccessible
////////////////////////////////////////////////////////////////////////////////

role
HTMLBRAccessible::NativeRole() const
{
  return roles::WHITESPACE;
}

uint64_t
HTMLBRAccessible::NativeState()
{
  return states::READONLY;
}

ENameValueFlag
HTMLBRAccessible::NativeName(nsString& aName)
{
  aName = static_cast<char16_t>('\n');    // Newline char
  return eNameOK;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLabelAccessible
////////////////////////////////////////////////////////////////////////////////

ENameValueFlag
HTMLLabelAccessible::NativeName(nsString& aName)
{
  nsTextEquivUtils::GetNameFromSubtree(this, aName);
  return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
}

Relation
HTMLLabelAccessible::RelationByType(RelationType aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType == RelationType::LABEL_FOR) {
    dom::HTMLLabelElement* label = dom::HTMLLabelElement::FromNode(mContent);
    rel.AppendTarget(mDoc, label->GetControl());
  }

  return rel;
}

uint8_t
HTMLLabelAccessible::ActionCount()
{
  return nsCoreUtils::IsLabelWithControl(mContent) ? 1 : 0;
}

void
HTMLLabelAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == 0) {
    if (nsCoreUtils::IsLabelWithControl(mContent))
      aName.AssignLiteral("click");
  }
}

bool
HTMLLabelAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != 0)
    return false;

  DoCommand();
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLOuputAccessible
////////////////////////////////////////////////////////////////////////////////

Relation
HTMLOutputAccessible::RelationByType(RelationType aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType == RelationType::CONTROLLED_BY)
    rel.AppendIter(new IDRefsIterator(mDoc, mContent, nsGkAtoms::_for));

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSummaryAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLSummaryAccessible::
  HTMLSummaryAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
  mGenericTypes |= eButton;
}

uint8_t
HTMLSummaryAccessible::ActionCount()
{
  return 1;
}

void
HTMLSummaryAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click) {
    return;
  }

  dom::HTMLSummaryElement* summary = dom::HTMLSummaryElement::FromNode(mContent);
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

bool
HTMLSummaryAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click)
    return false;

  DoCommand();
  return true;
}

uint64_t
HTMLSummaryAccessible::NativeState()
{
  uint64_t state = HyperTextAccessibleWrap::NativeState();

  dom::HTMLSummaryElement* summary = dom::HTMLSummaryElement::FromNode(mContent);
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

////////////////////////////////////////////////////////////////////////////////
// HTMLSummaryAccessible: Widgets

bool
HTMLSummaryAccessible::IsWidget() const
{
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLHeaderOrFooterAccessible
////////////////////////////////////////////////////////////////////////////////

role
HTMLHeaderOrFooterAccessible::NativeRole() const
{
  // Only map header and footer if they are direct descendants of the body tag.
  // If other sectioning or sectioning root elements, they become sections.
  nsIContent* parent = mContent->GetParent();
  while (parent) {
    if (parent->IsAnyOfHTMLElements(nsGkAtoms::article, nsGkAtoms::aside,
                             nsGkAtoms::nav, nsGkAtoms::section,
                             nsGkAtoms::blockquote, nsGkAtoms::details,
                             nsGkAtoms::dialog, nsGkAtoms::fieldset,
                             nsGkAtoms::figure, nsGkAtoms::td)) {
      break;
    }
    parent = parent->GetParent();
  }

  // No sectioning or sectioning root elements found.
  if (!parent) {
    if (mContent->IsHTMLElement(nsGkAtoms::header)) {
      return roles::HEADER;
    }

    if (mContent->IsHTMLElement(nsGkAtoms::footer)) {
      return roles::FOOTER;
    }
  }

  return roles::SECTION;
}

nsAtom*
HTMLHeaderOrFooterAccessible::LandmarkRole() const
{
  if (!HasOwnContent())
    return nullptr;

  a11y::role r = const_cast<HTMLHeaderOrFooterAccessible*>(this)->Role();
  if (r == roles::HEADER) {
    return nsGkAtoms::banner;
  }

  if (r == roles::FOOTER) {
    return nsGkAtoms::contentinfo;
  }

  return nullptr;
}
