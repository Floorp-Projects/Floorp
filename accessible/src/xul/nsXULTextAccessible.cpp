/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// NOTE: groups are alphabetically ordered
#include "nsXULTextAccessible.h"

#include "nsAccessibilityAtoms.h"
#include "nsAccUtils.h"
#include "nsBaseWidgetAccessible.h"
#include "nsCoreUtils.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "States.h"

#include "nsIAccessibleRelation.h"
#include "nsIDOMXULDescriptionElement.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsNetUtil.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXULTextAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTextAccessible::
  nsXULTextAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

nsresult
nsXULTextAccessible::GetNameInternal(nsAString& aName)
{
  // if the value attr doesn't exist, the screen reader must get the accessible text
  // from the accessible text interface or from the children
  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value, aName);
  return NS_OK;
}

PRUint32
nsXULTextAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LABEL;
}

PRUint64
nsXULTextAccessible::NativeState()
{
  // Labels and description have read only state
  // They are not focusable or selectable
  return nsHyperTextAccessibleWrap::NativeState() | states::READONLY;
}

Relation
nsXULTextAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsHyperTextAccessibleWrap::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_LABEL_FOR) {
    // Caption is the label for groupbox
    nsIContent *parent = mContent->GetParent();
    if (parent && parent->Tag() == nsAccessibilityAtoms::caption) {
      nsAccessible* parent = Parent();
      if (parent && parent->Role() == nsIAccessibleRole::ROLE_GROUPING)
        rel.AppendTarget(parent);
    }
  }

  return rel;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULTooltipAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTooltipAccessible::
  nsXULTooltipAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsLeafAccessible(aContent, aShell)
{
}

PRUint64
nsXULTooltipAccessible::NativeState()
{
  PRUint64 states = nsLeafAccessible::NativeState();

  states &= ~states::FOCUSABLE;
  states |= states::READONLY;
  return states;
}

PRUint32
nsXULTooltipAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_TOOLTIP;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULLinkAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULLinkAccessible::
  nsXULLinkAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

// Expose nsIAccessibleHyperLink unconditionally
NS_IMPL_ISUPPORTS_INHERITED1(nsXULLinkAccessible, nsHyperTextAccessibleWrap,
                             nsIAccessibleHyperLink)

////////////////////////////////////////////////////////////////////////////////
// nsXULLinkAccessible. nsIAccessible

NS_IMETHODIMP
nsXULLinkAccessible::GetValue(nsAString& aValue)
{
  aValue.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::href, aValue);
  return NS_OK;
}

nsresult
nsXULLinkAccessible::GetNameInternal(nsAString& aName)
{
  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value, aName);
  if (!aName.IsEmpty())
    return NS_OK;

  return nsTextEquivUtils::GetNameFromSubtree(this, aName);
}

PRUint32
nsXULLinkAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LINK;
}


PRUint64
nsXULLinkAccessible::NativeState()
{
  return nsHyperTextAccessible::NativeState() | states::LINKED;
}

PRUint8
nsXULLinkAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXULLinkAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  if (aIndex != eAction_Jump)
    return NS_ERROR_INVALID_ARG;
  
  aName.AssignLiteral("jump");
  return NS_OK;
}

NS_IMETHODIMP
nsXULLinkAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Jump)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  DoCommand();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULLinkAccessible: HyperLinkAccessible

bool
nsXULLinkAccessible::IsLink()
{
  // Expose HyperLinkAccessible unconditionally.
  return true;
}

PRUint32
nsXULLinkAccessible::StartOffset()
{
  // If XUL link accessible is not contained by hypertext accessible then
  // start offset matches index in parent because the parent doesn't contains
  // a text.
  // XXX: accessible parent of XUL link accessible should be a hypertext
  // accessible.
  if (nsAccessible::IsLink())
    return nsAccessible::StartOffset();
  return IndexInParent();
}

PRUint32
nsXULLinkAccessible::EndOffset()
{
  if (nsAccessible::IsLink())
    return nsAccessible::EndOffset();
  return IndexInParent() + 1;
}

already_AddRefed<nsIURI>
nsXULLinkAccessible::AnchorURIAt(PRUint32 aAnchorIndex)
{
  if (aAnchorIndex != 0)
    return nsnull;

  nsAutoString href;
  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::href, href);

  nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
  nsIDocument* document = mContent->GetOwnerDoc();

  nsIURI* anchorURI = nsnull;
  NS_NewURI(&anchorURI, href,
            document ? document->GetDocumentCharacterSet().get() : nsnull,
            baseURI);

  return anchorURI;
}
