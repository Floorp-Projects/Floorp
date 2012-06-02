/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLElementAccessibles.h"

#include "DocAccessible.h"
#include "nsAccUtils.h"
#include "nsIAccessibleRelation.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLHRAccessible
////////////////////////////////////////////////////////////////////////////////

role
HTMLHRAccessible::NativeRole()
{
  return roles::SEPARATOR;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLBRAccessible
////////////////////////////////////////////////////////////////////////////////

role
HTMLBRAccessible::NativeRole()
{
  return roles::WHITESPACE;
}

PRUint64
HTMLBRAccessible::NativeState()
{
  return states::READONLY;
}

nsresult
HTMLBRAccessible::GetNameInternal(nsAString& aName)
{
  aName = static_cast<PRUnichar>('\n');    // Newline char
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLabelAccessible
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(HTMLLabelAccessible, HyperTextAccessible)

nsresult
HTMLLabelAccessible::GetNameInternal(nsAString& aName)
{
  return nsTextEquivUtils::GetNameFromSubtree(this, aName);
}

role
HTMLLabelAccessible::NativeRole()
{
  return roles::LABEL;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLOuputAccessible
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(HTMLOutputAccessible, HyperTextAccessible)

Relation
HTMLOutputAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_CONTROLLED_BY)
    rel.AppendIter(new IDRefsIterator(mDoc, mContent, nsGkAtoms::_for));

  return rel;
}

role
HTMLOutputAccessible::NativeRole()
{
  return roles::SECTION;
}

nsresult
HTMLOutputAccessible::GetAttributesInternal(nsIPersistentProperties* aAttributes)
{
  nsresult rv = AccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::live,
                         NS_LITERAL_STRING("polite"));

  return NS_OK;
}

