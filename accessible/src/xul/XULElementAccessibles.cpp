/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULElementAccessibles.h"

#include "Accessible-inl.h"
#include "BaseAccessibles.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

#include "nsIAccessibleRelation.h"
#include "nsIDOMXULDescriptionElement.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsNetUtil.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULLabelAccessible
////////////////////////////////////////////////////////////////////////////////

XULLabelAccessible::
  XULLabelAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
}

ENameValueFlag
XULLabelAccessible::NativeName(nsString& aName)
{
  // if the value attr doesn't exist, the screen reader must get the accessible text
  // from the accessible text interface or from the children
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, aName);
  return eNameOK;
}

role
XULLabelAccessible::NativeRole()
{
  return roles::LABEL;
}

uint64_t
XULLabelAccessible::NativeState()
{
  // Labels and description have read only state
  // They are not focusable or selectable
  return HyperTextAccessibleWrap::NativeState() | states::READONLY;
}

Relation
XULLabelAccessible::RelationByType(uint32_t aType)
{
  Relation rel = HyperTextAccessibleWrap::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_LABEL_FOR) {
    // Caption is the label for groupbox
    nsIContent *parent = mContent->GetParent();
    if (parent && parent->Tag() == nsGkAtoms::caption) {
      Accessible* parent = Parent();
      if (parent && parent->Role() == roles::GROUPING)
        rel.AppendTarget(parent);
    }
  }

  return rel;
}


////////////////////////////////////////////////////////////////////////////////
// XULTooltipAccessible
////////////////////////////////////////////////////////////////////////////////

XULTooltipAccessible::
  XULTooltipAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

uint64_t
XULTooltipAccessible::NativeState()
{
  return LeafAccessible::NativeState() | states::READONLY;
}

role
XULTooltipAccessible::NativeRole()
{
  return roles::TOOLTIP;
}


////////////////////////////////////////////////////////////////////////////////
// XULLinkAccessible
////////////////////////////////////////////////////////////////////////////////

XULLinkAccessible::
  XULLinkAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
}

// Expose nsIAccessibleHyperLink unconditionally
NS_IMPL_ISUPPORTS_INHERITED1(XULLinkAccessible, HyperTextAccessibleWrap,
                             nsIAccessibleHyperLink)

////////////////////////////////////////////////////////////////////////////////
// XULLinkAccessible. nsIAccessible

void
XULLinkAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::href, aValue);
}

ENameValueFlag
XULLinkAccessible::NativeName(nsString& aName)
{
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, aName);
  if (aName.IsEmpty())
    nsTextEquivUtils::GetNameFromSubtree(this, aName);

  return eNameOK;
}

role
XULLinkAccessible::NativeRole()
{
  return roles::LINK;
}


uint64_t
XULLinkAccessible::NativeLinkState() const
{
  return states::LINKED;
}

uint8_t
XULLinkAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
XULLinkAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();

  if (aIndex != eAction_Jump)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("jump");
  return NS_OK;
}

NS_IMETHODIMP
XULLinkAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Jump)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  DoCommand();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// XULLinkAccessible: HyperLinkAccessible

bool
XULLinkAccessible::IsLink()
{
  // Expose HyperLinkAccessible unconditionally.
  return true;
}

uint32_t
XULLinkAccessible::StartOffset()
{
  // If XUL link accessible is not contained by hypertext accessible then
  // start offset matches index in parent because the parent doesn't contains
  // a text.
  // XXX: accessible parent of XUL link accessible should be a hypertext
  // accessible.
  if (Accessible::IsLink())
    return Accessible::StartOffset();
  return IndexInParent();
}

uint32_t
XULLinkAccessible::EndOffset()
{
  if (Accessible::IsLink())
    return Accessible::EndOffset();
  return IndexInParent() + 1;
}

already_AddRefed<nsIURI>
XULLinkAccessible::AnchorURIAt(uint32_t aAnchorIndex)
{
  if (aAnchorIndex != 0)
    return nullptr;

  nsAutoString href;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);

  nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
  nsIDocument* document = mContent->OwnerDoc();

  nsIURI* anchorURI = nullptr;
  NS_NewURI(&anchorURI, href,
            document->GetDocumentCharacterSet().get(),
            baseURI);

  return anchorURI;
}
