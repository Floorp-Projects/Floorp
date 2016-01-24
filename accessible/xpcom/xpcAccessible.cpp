/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsIAccessibleRelation.h"
#include "nsIAccessibleRole.h"
#include "nsAccessibleRelation.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "xpcAccessibleDocument.h"

#include "nsIMutableArray.h"
#include "nsIPersistentProperties2.h"

using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessible::GetParent(nsIAccessible** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nullptr;
  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aParent = ToXPC(Intl()->Parent()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetNextSibling(nsIAccessible** aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  NS_IF_ADDREF(*aNextSibling = ToXPC(Intl()->GetSiblingAtOffset(1, &rv)));
  return rv;
}

NS_IMETHODIMP
xpcAccessible::GetPreviousSibling(nsIAccessible** aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);
  *aPreviousSibling = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  NS_IF_ADDREF(*aPreviousSibling = ToXPC(Intl()->GetSiblingAtOffset(-1, &rv)));
  return rv;
}

NS_IMETHODIMP
xpcAccessible::GetFirstChild(nsIAccessible** aFirstChild)
{
  NS_ENSURE_ARG_POINTER(aFirstChild);
  *aFirstChild = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aFirstChild = ToXPC(Intl()->FirstChild()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetLastChild(nsIAccessible** aLastChild)
{
  NS_ENSURE_ARG_POINTER(aLastChild);
  *aLastChild = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aLastChild = ToXPC(Intl()->LastChild()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildCount(int32_t* aChildCount)
{
  NS_ENSURE_ARG_POINTER(aChildCount);

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aChildCount = Intl()->ChildCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildAt(int32_t aChildIndex, nsIAccessible** aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  // If child index is negative, then return last child.
  // XXX: do we really need this?
  if (aChildIndex < 0)
    aChildIndex = Intl()->ChildCount() - 1;

  Accessible* child = Intl()->GetChildAt(aChildIndex);
  if (!child)
    return NS_ERROR_INVALID_ARG;

  NS_ADDREF(*aChild = ToXPC(child));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildren(nsIArray** aChildren)
{
  NS_ENSURE_ARG_POINTER(aChildren);
  *aChildren = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> children =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t childCount = Intl()->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = Intl()->GetChildAt(childIdx);
    children->AppendElement(static_cast<nsIAccessible*>(ToXPC(child)), false);
  }

  NS_ADDREF(*aChildren = children);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetIndexInParent(int32_t* aIndexInParent)
{
  NS_ENSURE_ARG_POINTER(aIndexInParent);
  *aIndexInParent = -1;

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aIndexInParent = Intl()->IndexInParent();
  return *aIndexInParent != -1 ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
xpcAccessible::GetDOMNode(nsIDOMNode** aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsINode* node = Intl()->GetNode();
  if (node)
    CallQueryInterface(node, aDOMNode);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDocument(nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDocument = ToXPCDocument(Intl()->Document()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRootDocument(nsIAccessibleDocument** aRootDocument)
{
  NS_ENSURE_ARG_POINTER(aRootDocument);
  *aRootDocument = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aRootDocument = ToXPCDocument(Intl()->RootAccessible()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRole(uint32_t* aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);
  *aRole = nsIAccessibleRole::ROLE_NOTHING;

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aRole = Intl()->Role();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetState(uint32_t* aState, uint32_t* aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  if (!Intl())
    nsAccUtils::To32States(states::DEFUNCT, aState, aExtraState);
  else
    nsAccUtils::To32States(Intl()->State(), aState, aExtraState);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetName(nsAString& aName)
{
  aName.Truncate();

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsAutoString name;
  Intl()->Name(name);
  aName.Assign(name);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDescription(nsAString& aDescription)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  nsAutoString desc;
  Intl()->Description(desc);
  aDescription.Assign(desc);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetLanguage(nsAString& aLanguage)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->Language(aLanguage);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetValue(nsAString& aValue)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  nsAutoString value;
  Intl()->Value(value);
  aValue.Assign(value);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetHelp(nsAString& aHelp)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  nsAutoString help;
  Intl()->Help(help);
  aHelp.Assign(help);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetAccessKey(nsAString& aAccessKey)
{
  aAccessKey.Truncate();

  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->AccessKey().ToString(aAccessKey);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetKeyboardShortcut(nsAString& aKeyBinding)
{
  aKeyBinding.Truncate();
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->KeyboardShortcut().ToString(aKeyBinding);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetAttributes(nsIPersistentProperties** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attributes = Intl()->Attributes();
  attributes.swap(*aAttributes);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetBounds(int32_t* aX, int32_t* aY,
                         int32_t* aWidth, int32_t* aHeight)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsIntRect rect = Intl()->Bounds();
  *aX = rect.x;
  *aY = rect.y;
  *aWidth = rect.width;
  *aHeight = rect.height;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GroupPosition(int32_t* aGroupLevel,
                             int32_t* aSimilarItemsInGroup,
                             int32_t* aPositionInGroup)
{
  NS_ENSURE_ARG_POINTER(aGroupLevel);
  *aGroupLevel = 0;

  NS_ENSURE_ARG_POINTER(aSimilarItemsInGroup);
  *aSimilarItemsInGroup = 0;

  NS_ENSURE_ARG_POINTER(aPositionInGroup);
  *aPositionInGroup = 0;

  if (!Intl())
    return NS_ERROR_FAILURE;

  GroupPos groupPos = Intl()->GroupPosition();

  *aGroupLevel = groupPos.level;
  *aSimilarItemsInGroup = groupPos.setSize;
  *aPositionInGroup = groupPos.posInSet;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRelationByType(uint32_t aType,
                                 nsIAccessibleRelation** aRelation)
{
  NS_ENSURE_ARG_POINTER(aRelation);
  *aRelation = nullptr;

  NS_ENSURE_ARG(aType <= static_cast<uint32_t>(RelationType::LAST));

  if (!Intl())
    return NS_ERROR_FAILURE;

  Relation rel = Intl()->RelationByType(static_cast<RelationType>(aType));
  NS_ADDREF(*aRelation = new nsAccessibleRelation(aType, &rel));
  return *aRelation ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
xpcAccessible::GetRelations(nsIArray** aRelations)
{
  NS_ENSURE_ARG_POINTER(aRelations);
  *aRelations = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMutableArray> relations = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(relations, NS_ERROR_OUT_OF_MEMORY);

  static const uint32_t relationTypes[] = {
    nsIAccessibleRelation::RELATION_LABELLED_BY,
    nsIAccessibleRelation::RELATION_LABEL_FOR,
    nsIAccessibleRelation::RELATION_DESCRIBED_BY,
    nsIAccessibleRelation::RELATION_DESCRIPTION_FOR,
    nsIAccessibleRelation::RELATION_NODE_CHILD_OF,
    nsIAccessibleRelation::RELATION_NODE_PARENT_OF,
    nsIAccessibleRelation::RELATION_CONTROLLED_BY,
    nsIAccessibleRelation::RELATION_CONTROLLER_FOR,
    nsIAccessibleRelation::RELATION_FLOWS_TO,
    nsIAccessibleRelation::RELATION_FLOWS_FROM,
    nsIAccessibleRelation::RELATION_MEMBER_OF,
    nsIAccessibleRelation::RELATION_SUBWINDOW_OF,
    nsIAccessibleRelation::RELATION_EMBEDS,
    nsIAccessibleRelation::RELATION_EMBEDDED_BY,
    nsIAccessibleRelation::RELATION_POPUP_FOR,
    nsIAccessibleRelation::RELATION_PARENT_WINDOW_OF,
    nsIAccessibleRelation::RELATION_DEFAULT_BUTTON,
    nsIAccessibleRelation::RELATION_CONTAINING_DOCUMENT,
    nsIAccessibleRelation::RELATION_CONTAINING_TAB_PANE,
    nsIAccessibleRelation::RELATION_CONTAINING_APPLICATION
  };

  for (uint32_t idx = 0; idx < ArrayLength(relationTypes); idx++) {
    nsCOMPtr<nsIAccessibleRelation> relation;
    nsresult rv = GetRelationByType(relationTypes[idx], getter_AddRefs(relation));

    if (NS_SUCCEEDED(rv) && relation) {
      uint32_t targets = 0;
      relation->GetTargetsCount(&targets);
      if (targets)
        relations->AppendElement(relation, false);
    }
  }

  NS_ADDREF(*aRelations = relations);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetFocusedChild(nsIAccessible** aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aChild = ToXPC(Intl()->FocusedChild()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildAtPoint(int32_t aX, int32_t aY,
                               nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aAccessible =
               ToXPC(Intl()->ChildAtPoint(aX, aY, Accessible::eDirectChild)));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDeepestChildAtPoint(int32_t aX, int32_t aY,
                                      nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aAccessible =
               ToXPC(Intl()->ChildAtPoint(aX, aY, Accessible::eDeepestChild)));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::SetSelected(bool aSelect)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->SetSelected(aSelect);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::TakeSelection()
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->TakeSelection();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::TakeFocus()
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->TakeFocus();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionCount(uint8_t* aActionCount)
{
  NS_ENSURE_ARG_POINTER(aActionCount);
  *aActionCount = 0;
  if (!Intl())
    return NS_ERROR_FAILURE;

  *aActionCount = Intl()->ActionCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  if (aIndex >= Intl()->ActionCount())
    return NS_ERROR_INVALID_ARG;

  Intl()->ActionNameAt(aIndex, aName);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionDescription(uint8_t aIndex, nsAString& aDescription)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  if (aIndex >= Intl()->ActionCount())
    return NS_ERROR_INVALID_ARG;

  Intl()->ActionDescriptionAt(aIndex, aDescription);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::DoAction(uint8_t aIndex)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  return Intl()->DoAction(aIndex) ?
    NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessible::ScrollTo(uint32_t aHow)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->ScrollTo(aHow);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ScrollToPoint(uint32_t aCoordinateType, int32_t aX, int32_t aY)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->ScrollToPoint(aCoordinateType, aX, aY);
  return NS_OK;
}
