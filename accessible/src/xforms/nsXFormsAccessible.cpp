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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsXFormsAccessible.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsTextEquivUtils.h"

#include "nscore.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIEditor.h"
#include "nsIMutableArray.h"
#include "nsIXFormsUtilityService.h"
#include "nsIPlaintextEditor.h"

////////////////////////////////////////////////////////////////////////////////
// nsXFormsAccessibleBase
////////////////////////////////////////////////////////////////////////////////

nsIXFormsUtilityService *nsXFormsAccessibleBase::sXFormsService = nsnull;

nsXFormsAccessibleBase::nsXFormsAccessibleBase()
{
  if (!sXFormsService) {
    nsresult rv = CallGetService("@mozilla.org/xforms-utility-service;1",
                                 &sXFormsService);
    if (NS_FAILED(rv))
      NS_WARNING("No XForms utility service.");
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsXFormsAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsAccessible::
nsXFormsAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

nsresult
nsXFormsAccessible::GetBoundChildElementValue(const nsAString& aTagName,
                                              nsAString& aValue)
{
  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsINodeList* nodes = mContent->GetChildNodesList();
  NS_ENSURE_STATE(nodes);

  PRUint32 length;
  nsresult rv = nodes->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 index = 0; index < length; index++) {
    nsIContent* content = nodes->GetNodeAt(index);
    if (content->NodeInfo()->Equals(aTagName) &&
        content->NodeInfo()->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS))) {
      nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(content));
      return sXFormsService->GetValue(DOMNode, aValue);
    }
  }

  aValue.Truncate();
  return NS_OK;
}

void
nsXFormsAccessible::CacheSelectChildren(nsIDOMNode *aContainerNode)
{
  nsCOMPtr<nsIDOMNode> container(aContainerNode);
  if (!container)
    container = do_QueryInterface(mContent);

  nsCOMPtr<nsIDOMNodeList> children;
  sXFormsService->GetSelectChildrenFor(container, getter_AddRefs(children));

  if (!children)
    return;

  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));

  PRUint32 length = 0;
  children->GetLength(&length);

  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> DOMChild;
    children->Item(index, getter_AddRefs(DOMChild));
    if (!DOMChild)
      continue;

    nsCOMPtr<nsIContent> child(do_QueryInterface(DOMChild));
    nsRefPtr<nsAccessible> accessible =
      GetAccService()->GetOrCreateAccessible(child, presShell, mWeakShell);
    if (!accessible)
      continue;

    AppendChild(accessible);
  }
}

// nsIAccessible

NS_IMETHODIMP
nsXFormsAccessible::GetValue(nsAString& aValue)
{
  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  return sXFormsService->GetValue(DOMNode, aValue);
}

nsresult
nsXFormsAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = 0;

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  if (aExtraState)
    *aExtraState = 0;

  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  PRBool isRelevant = PR_FALSE;
  nsresult rv = sXFormsService->IsRelevant(DOMNode, &isRelevant);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isReadonly = PR_FALSE;
  rv = sXFormsService->IsReadonly(DOMNode, &isReadonly);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isRequired = PR_FALSE;
  rv = sXFormsService->IsRequired(DOMNode, &isRequired);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isValid = PR_FALSE;
  rv = sXFormsService->IsValid(DOMNode, &isValid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsHyperTextAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isRelevant)
    *aState |= nsIAccessibleStates::STATE_UNAVAILABLE;

  if (isReadonly)
    *aState |= nsIAccessibleStates::STATE_READONLY;

  if (isRequired)
    *aState |= nsIAccessibleStates::STATE_REQUIRED;

  if (!isValid)
    *aState |= nsIAccessibleStates::STATE_INVALID;

  return NS_OK;
}

nsresult
nsXFormsAccessible::GetNameInternal(nsAString& aName)
{
  // search the xforms:label element
  return GetBoundChildElementValue(NS_LITERAL_STRING("label"), aName);
}

NS_IMETHODIMP
nsXFormsAccessible::GetDescription(nsAString& aDescription)
{
  nsAutoString description;
  nsresult rv = nsTextEquivUtils::
    GetTextEquivFromIDRefs(this, nsAccessibilityAtoms::aria_describedby,
                           description);

  if (NS_SUCCEEDED(rv) && !description.IsEmpty()) {
    aDescription = description;
    return NS_OK;
  }

  // search the xforms:hint element
  return GetBoundChildElementValue(NS_LITERAL_STRING("hint"), aDescription);
}

PRBool
nsXFormsAccessible::GetAllowsAnonChildAccessibles()
{
  return PR_FALSE;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsContainerAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsContainerAccessible::
  nsXFormsContainerAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

nsresult
nsXFormsContainerAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_GROUPING;
  return NS_OK;
}

PRBool
nsXFormsContainerAccessible::GetAllowsAnonChildAccessibles()
{
  return PR_TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsEditableAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsEditableAccessible::
  nsXFormsEditableAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

nsresult
nsXFormsEditableAccessible::GetStateInternal(PRUint32 *aState,
                                             PRUint32 *aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsresult rv = nsXFormsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  if (!aExtraState)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  PRBool isReadonly = PR_FALSE;
  rv = sXFormsService->IsReadonly(DOMNode, &isReadonly);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isReadonly) {
    PRBool isRelevant = PR_FALSE;
    rv = sXFormsService->IsRelevant(DOMNode, &isRelevant);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isRelevant) {
      *aExtraState |= nsIAccessibleStates::EXT_STATE_EDITABLE |
                      nsIAccessibleStates::EXT_STATE_SELECTABLE_TEXT;
    }
  }

  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);
  PRUint32 flags;
  editor->GetFlags(&flags);
  if (flags & nsIPlaintextEditor::eEditorSingleLineMask)
    *aExtraState |= nsIAccessibleStates::EXT_STATE_SINGLE_LINE;
  else
    *aExtraState |= nsIAccessibleStates::EXT_STATE_MULTI_LINE;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsEditableAccessible::GetAssociatedEditor(nsIEditor **aEditor)
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  return sXFormsService->GetEditor(DOMNode, aEditor);
}

// nsXFormsSelectableAccessible


NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsSelectableAccessible,
                             nsXFormsEditableAccessible,
                             nsIAccessibleSelectable)

nsXFormsSelectableAccessible::
  nsXFormsSelectableAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsEditableAccessible(aContent, aShell), mIsSelect1Element(nsnull)
{
  mIsSelect1Element =
    mContent->NodeInfo()->Equals(nsAccessibilityAtoms::select1);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::GetSelectedChildren(nsIArray **aAccessibles)
{
  NS_ENSURE_ARG_POINTER(aAccessibles);

  *aAccessibles = nsnull;

  nsCOMPtr<nsIMutableArray> accessibles =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(accessibles, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> item;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!item)
      return NS_OK;

    nsCOMPtr<nsIAccessible> accessible;
    GetAccService()->GetAccessibleFor(item, getter_AddRefs(accessible));
    NS_ENSURE_TRUE(accessible, NS_ERROR_FAILURE);

    accessibles->AppendElement(accessible, PR_FALSE);
    NS_ADDREF(*aAccessibles = accessibles);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNodeList> items;
  rv = sXFormsService->GetSelectedItemsForSelect(DOMNode,
                                                 getter_AddRefs(items));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!items)
    return NS_OK;

  PRUint32 length = 0;
  items->GetLength(&length);
  if (!length)
    return NS_OK;

  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> item;
    items->Item(index, getter_AddRefs(item));
    NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

    nsCOMPtr<nsIAccessible> accessible;
    GetAccService()->GetAccessibleFor(item, getter_AddRefs(accessible));
    NS_ENSURE_TRUE(accessible, NS_ERROR_FAILURE);

    accessibles->AppendElement(accessible, PR_FALSE);
  }

  NS_ADDREF(*aAccessibles = accessibles);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::GetSelectionCount(PRInt32 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);

  *aCount = 0;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> item;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (item)
      *aCount = 1;

    return NS_OK;
  }

  nsCOMPtr<nsIDOMNodeList> items;
  rv = sXFormsService->GetSelectedItemsForSelect(DOMNode,
                                                 getter_AddRefs(items));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!items)
    return NS_OK;

  PRUint32 length = 0;
  items->GetLength(&length);
  if (length)
    *aCount = length;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::AddChildToSelection(PRInt32 aIndex)
{
  nsCOMPtr<nsIDOMNode> item = GetItemByIndex(&aIndex);
  if (!item)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element)
    return sXFormsService->SetSelectedItemForSelect1(DOMNode, item);

  return sXFormsService->AddItemToSelectionForSelect(DOMNode, item);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::RemoveChildFromSelection(PRInt32 aIndex)
{
  nsCOMPtr<nsIDOMNode> item = GetItemByIndex(&aIndex);
  if (!item)
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> selitem;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(selitem));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (selitem != item)
      return NS_ERROR_FAILURE;
    return sXFormsService->SetSelectedItemForSelect1(DOMNode, nsnull);
  }

  return sXFormsService->RemoveItemFromSelectionForSelect(DOMNode, item);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::RefSelection(PRInt32 aIndex,
                                           nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element) {
    if (aIndex != 0)
      return NS_OK;

    nsCOMPtr<nsIDOMNode> item;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (item)
      return GetAccService()->GetAccessibleFor(item, aAccessible);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNodeList> items;
  rv = sXFormsService->GetSelectedItemsForSelect(DOMNode,
                                                 getter_AddRefs(items));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!items)
    return NS_OK;

  PRUint32 length = 0;
  items->GetLength(&length);
  if (aIndex < 0 || PRUint32(aIndex) >= length)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> item;
  items->Item(aIndex, getter_AddRefs(item));

  nsCOMPtr<nsIAccessible> accessible;
  return GetAccService()->GetAccessibleFor(item, getter_AddRefs(accessible));
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::IsChildSelected(PRInt32 aIndex,
                                              PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  nsCOMPtr<nsIDOMNode> item = GetItemByIndex(&aIndex);
  if (!item)
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> selitem;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(selitem));
    NS_ENSURE_SUCCESS(rv, rv);

    if (selitem == item)
      *aIsSelected = PR_TRUE;
    return NS_OK;
  }

  return sXFormsService->IsSelectItemSelected(DOMNode, item, aIsSelected);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::ClearSelection()
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element)
    return sXFormsService->SetSelectedItemForSelect1(DOMNode, nsnull);

  return sXFormsService->ClearSelectionForSelect(DOMNode);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::SelectAllSelection(PRBool *aMultipleSelection)
{
  NS_ENSURE_ARG_POINTER(aMultipleSelection);

  if (mIsSelect1Element) {
    *aMultipleSelection = PR_FALSE;
    return NS_OK;
  }

  *aMultipleSelection = PR_TRUE;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  return sXFormsService->SelectAllItemsForSelect(DOMNode);
}

already_AddRefed<nsIDOMNode>
nsXFormsSelectableAccessible::GetItemByIndex(PRInt32 *aIndex,
                                             nsIAccessible *aAccessible)
{
  nsRefPtr<nsAccessible> accessible(do_QueryObject(aAccessible));
  if (!accessible)
    accessible = this;

  PRInt32 childCount = accessible->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible *child = accessible->GetChildAt(childIdx);

    nsCOMPtr<nsIDOMNode> childNode(child->GetDOMNode());
    nsCOMPtr<nsIContent> childContent(do_QueryInterface(childNode));
    if (!childContent)
      continue;

    nsINodeInfo *nodeInfo = childContent->NodeInfo();
    if (nodeInfo->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS))) {
      if (nodeInfo->Equals(nsAccessibilityAtoms::item)) {
        if (!*aIndex)
          return childNode.forget();

        --*aIndex;
      } else if (nodeInfo->Equals(nsAccessibilityAtoms::choices)) {
        nsIDOMNode *itemNode = GetItemByIndex(aIndex, child).get();
        if (itemNode)
          return itemNode;
      }
    }
  }

  return nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectableItemAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectableItemAccessible::
  nsXFormsSelectableItemAccessible(nsIContent *aContent,
                                   nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

NS_IMETHODIMP
nsXFormsSelectableItemAccessible::GetValue(nsAString& aValue)
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  return sXFormsService->GetValue(DOMNode, aValue);
}

NS_IMETHODIMP
nsXFormsSelectableItemAccessible::GetNumActions(PRUint8 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);

  *aCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectableItemAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

PRBool
nsXFormsSelectableItemAccessible::IsItemSelected()
{
  nsresult rv;

  nsINode* parent = mContent;
  while ((parent = parent->GetNodeParent())) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(parent));
    if (!content)
      return PR_FALSE;

    nsCOMPtr<nsINodeInfo> nodeinfo = content->NodeInfo();
    if (!nodeinfo->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS)))
      continue;

    nsCOMPtr<nsIDOMNode> select(do_QueryInterface(parent));
    if (!select)
      continue;

    nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
    if (nodeinfo->Equals(nsAccessibilityAtoms::select)) {
      PRBool isSelected = PR_FALSE;
      rv = sXFormsService->IsSelectItemSelected(select, DOMNode, &isSelected);
      return NS_SUCCEEDED(rv) && isSelected;
    }

    if (nodeinfo->Equals(nsAccessibilityAtoms::select1)) {
      nsCOMPtr<nsIDOMNode> selitem;
      rv = sXFormsService->GetSelectedItemForSelect1(select,
                                                     getter_AddRefs(selitem));
      return NS_SUCCEEDED(rv) && (selitem == DOMNode);
    }
  }

  return PR_FALSE;
}

