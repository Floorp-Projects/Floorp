/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLTextAccessible.h"

#include "DocAccessible.h"
#include "nsAccUtils.h"
#include "nsIAccessibleRelation.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLHRAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLHRAccessible::
  nsHTMLHRAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsLeafAccessible(aContent, aDoc)
{
}

role
nsHTMLHRAccessible::NativeRole()
{
  return roles::SEPARATOR;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLBRAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLBRAccessible::
  nsHTMLBRAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsLeafAccessible(aContent, aDoc)
{
}

role
nsHTMLBRAccessible::NativeRole()
{
  return roles::WHITESPACE;
}

PRUint64
nsHTMLBRAccessible::NativeState()
{
  return states::READONLY;
}

nsresult
nsHTMLBRAccessible::GetNameInternal(nsAString& aName)
{
  aName = static_cast<PRUnichar>('\n');    // Newline char
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLabelAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLLabelAccessible::
  nsHTMLLabelAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsHyperTextAccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLLabelAccessible, nsHyperTextAccessible)

nsresult
nsHTMLLabelAccessible::GetNameInternal(nsAString& aName)
{
  return nsTextEquivUtils::GetNameFromSubtree(this, aName);
}

role
nsHTMLLabelAccessible::NativeRole()
{
  return roles::LABEL;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLOuputAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLOutputAccessible::
  nsHTMLOutputAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsHyperTextAccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLOutputAccessible, nsHyperTextAccessible)

Relation
nsHTMLOutputAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsAccessibleWrap::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_CONTROLLED_BY)
    rel.AppendIter(new IDRefsIterator(mDoc, mContent, nsGkAtoms::_for));

  return rel;
}

role
nsHTMLOutputAccessible::NativeRole()
{
  return roles::SECTION;
}

nsresult
nsHTMLOutputAccessible::GetAttributesInternal(nsIPersistentProperties* aAttributes)
{
  nsresult rv = nsAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::live,
                         NS_LITERAL_STRING("polite"));
  
  return NS_OK;
}

