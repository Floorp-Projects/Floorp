/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessible.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsIAccessibleRelation.h"
#include "nsIAccessibleRole.h"
#include "nsAccessibleRelation.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"

#include "nsIMutableArray.h"
#include "nsIPersistentProperties2.h"

using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessible::GetParent(nsIAccessible** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aParent = static_cast<Accessible*>(this)->Parent());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetNextSibling(nsIAccessible** aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  NS_IF_ADDREF(*aNextSibling = static_cast<Accessible*>(this)->GetSiblingAtOffset(1, &rv));
  return rv;
}

NS_IMETHODIMP
xpcAccessible::GetPreviousSibling(nsIAccessible** aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);
  *aPreviousSibling = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  NS_IF_ADDREF(*aPreviousSibling = static_cast<Accessible*>(this)->GetSiblingAtOffset(-1, &rv));
  return rv;
}

NS_IMETHODIMP
xpcAccessible::GetFirstChild(nsIAccessible** aFirstChild)
{
  NS_ENSURE_ARG_POINTER(aFirstChild);
  *aFirstChild = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aFirstChild = static_cast<Accessible*>(this)->FirstChild());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetLastChild(nsIAccessible** aLastChild)
{
  NS_ENSURE_ARG_POINTER(aLastChild);
  *aLastChild = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aLastChild = static_cast<Accessible*>(this)->LastChild());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildCount(int32_t* aChildCount)
{
  NS_ENSURE_ARG_POINTER(aChildCount);

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  *aChildCount = static_cast<Accessible*>(this)->ChildCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ScriptableGetChildAt(int32_t aChildIndex, nsIAccessible** aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  // If child index is negative, then return last child.
  // XXX: do we really need this?
  if (aChildIndex < 0)
    aChildIndex = static_cast<Accessible*>(this)->ChildCount() - 1;

  Accessible* child = static_cast<Accessible*>(this)->GetChildAt(aChildIndex);
  if (!child)
    return NS_ERROR_INVALID_ARG;

  NS_ADDREF(*aChild = child);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildren(nsIArray** aChildren)
{
  NS_ENSURE_ARG_POINTER(aChildren);
  *aChildren = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> children =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t childCount = static_cast<Accessible*>(this)->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    nsIAccessible* child = static_cast<Accessible*>(this)->GetChildAt(childIdx);
    children->AppendElement(child, false);
  }

  NS_ADDREF(*aChildren = children);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetIndexInParent(int32_t* aIndexInParent)
{
  NS_ENSURE_ARG_POINTER(aIndexInParent);

  *aIndexInParent = static_cast<Accessible*>(this)->IndexInParent();
  return *aIndexInParent != -1 ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
xpcAccessible::GetDOMNode(nsIDOMNode** aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nullptr;

  nsINode *node = static_cast<Accessible*>(this)->GetNode();
  if (node)
    CallQueryInterface(node, aDOMNode);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDocument(nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  NS_IF_ADDREF(*aDocument = static_cast<Accessible*>(this)->Document());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRootDocument(nsIAccessibleDocument** aRootDocument)
{
  NS_ENSURE_ARG_POINTER(aRootDocument);

  NS_IF_ADDREF(*aRootDocument = static_cast<Accessible*>(this)->RootAccessible());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRole(uint32_t* aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);
  *aRole = nsIAccessibleRole::ROLE_NOTHING;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  *aRole = static_cast<Accessible*>(this)->Role();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetState(uint32_t* aState, uint32_t* aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsAccUtils::To32States(static_cast<Accessible*>(this)->State(),
                         aState, aExtraState);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetName(nsAString& aName)
{
  aName.Truncate();

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString name;
  static_cast<Accessible*>(this)->Name(name);
  aName.Assign(name);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDescription(nsAString& aDescription)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString desc;
  static_cast<Accessible*>(this)->Description(desc);
  aDescription.Assign(desc);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetLanguage(nsAString& aLanguage)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<Accessible*>(this)->Language(aLanguage);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetValue(nsAString& aValue)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString value;
  static_cast<Accessible*>(this)->Value(value);
  aValue.Assign(value);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetHelp(nsAString& aHelp)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
xpcAccessible::GetAccessKey(nsAString& aAccessKey)
{
  aAccessKey.Truncate();

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<Accessible*>(this)->AccessKey().ToString(aAccessKey);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetKeyboardShortcut(nsAString& aKeyBinding)
{
  aKeyBinding.Truncate();
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<Accessible*>(this)->KeyboardShortcut().ToString(aKeyBinding);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetAttributes(nsIPersistentProperties** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attributes =
    static_cast<Accessible*>(this)->Attributes();
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

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsIntRect rect = static_cast<Accessible*>(this)->Bounds();
  *aX = rect.x;
  *aY = rect.y;
  *aWidth = rect.width;
  *aHeight = rect.height;;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ScriptableGroupPosition(int32_t* aGroupLevel,
                                       int32_t* aSimilarItemsInGroup,
                                       int32_t* aPositionInGroup)
{
  NS_ENSURE_ARG_POINTER(aGroupLevel);
  *aGroupLevel = 0;

  NS_ENSURE_ARG_POINTER(aSimilarItemsInGroup);
  *aSimilarItemsInGroup = 0;

  NS_ENSURE_ARG_POINTER(aPositionInGroup);
  *aPositionInGroup = 0;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  GroupPos groupPos = static_cast<Accessible*>(this)->GroupPosition();

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

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  Relation rel = static_cast<Accessible*>(this)->RelationByType(static_cast<RelationType>(aType));
  NS_ADDREF(*aRelation = new nsAccessibleRelation(aType, &rel));
  return *aRelation ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
xpcAccessible::GetRelations(nsIArray** aRelations)
{
  NS_ENSURE_ARG_POINTER(aRelations);
  *aRelations = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
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

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aChild = static_cast<Accessible*>(this)->FocusedChild());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildAtPoint(int32_t aX, int32_t aY,
                               nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aAccessible =
               static_cast<Accessible*>(this)->ChildAtPoint(aX, aY,
                                                            Accessible::eDirectChild));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDeepestChildAtPoint(int32_t aX, int32_t aY,
                                      nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aAccessible =
               static_cast<Accessible*>(this)->ChildAtPoint(aX, aY,
                                                            Accessible::eDeepestChild));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ScriptableSetSelected(bool aSelect)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<Accessible*>(this)->SetSelected(aSelect);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ExtendSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
xpcAccessible::ScriptableTakeSelection()
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<Accessible*>(this)->TakeSelection();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ScriptableTakeFocus()
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<Accessible*>(this)->TakeFocus();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionCount(uint8_t* aActionCount)
{
  NS_ENSURE_ARG_POINTER(aActionCount);
  *aActionCount = 0;
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  *aActionCount = static_cast<Accessible*>(this)->ActionCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  if (aIndex >= static_cast<Accessible*>(this)->ActionCount())
    return NS_ERROR_INVALID_ARG;

  static_cast<Accessible*>(this)->ActionNameAt(aIndex, aName);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionDescription(uint8_t aIndex, nsAString& aDescription)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  if (aIndex >= static_cast<Accessible*>(this)->ActionCount())
    return NS_ERROR_INVALID_ARG;

  static_cast<Accessible*>(this)->ActionDescriptionAt(aIndex, aDescription);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ScriptableDoAction(uint8_t aIndex)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  return static_cast<Accessible*>(this)->DoAction(aIndex) ?
    NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessible::ScriptableScrollTo(uint32_t aHow)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<Accessible*>(this)->ScrollTo(aHow);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ScriptableScrollToPoint(uint32_t aCoordinateType,
                                       int32_t aX, int32_t aY)
{
  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<Accessible*>(this)->ScrollToPoint(aCoordinateType, aX, aY);
  return NS_OK;
}