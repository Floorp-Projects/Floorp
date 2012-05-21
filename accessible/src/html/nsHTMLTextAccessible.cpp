/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLTextAccessible.h"

#include "nsDocAccessible.h"
#include "nsAccUtils.h"
#include "nsIAccessibleRelation.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTextAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLTextAccessible::
  nsHTMLTextAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsTextAccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLTextAccessible, nsTextAccessible)

ENameValueFlag
nsHTMLTextAccessible::Name(nsString& aName)
{
  // Text node, ARIA can't be used.
  aName = mText;
  return eNameOK;
}

role
nsHTMLTextAccessible::NativeRole()
{
  nsIFrame *frame = GetFrame();
  // Don't return on null frame -- we still return a role
  // after accessible is shutdown/DEFUNCT
  if (frame && frame->IsGeneratedContentFrame()) 
    return roles::STATICTEXT;

  return nsTextAccessible::NativeRole();
}

PRUint64
nsHTMLTextAccessible::NativeState()
{
  PRUint64 state = nsTextAccessible::NativeState();

  nsDocAccessible* docAccessible = Document();
  if (docAccessible) {
     PRUint64 docState = docAccessible->State();
     if (0 == (docState & states::EDITABLE)) {
       state |= states::READONLY; // Links not focusable in editor
     }
  }

  return state;
}

nsresult
nsHTMLTextAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (NativeRole() == roles::STATICTEXT) {
    nsAutoString oldValueUnused;
    aAttributes->SetStringProperty(NS_LITERAL_CSTRING("auto-generated"),
                                  NS_LITERAL_STRING("true"), oldValueUnused);
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLHRAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLHRAccessible::
  nsHTMLHRAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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
  nsHTMLBRAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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
  nsHTMLLabelAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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
  nsHTMLOutputAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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

