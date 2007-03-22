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

#include "nscore.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIMutableArray.h"
#include "nsIXFormsUtilityService.h"
#include "nsIPlaintextEditor.h"
#include "nsIPersistentProperties2.h"

// nsXFormsAccessibleBase

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

// nsXFormsAccessible

nsXFormsAccessible::
nsXFormsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
  nsHyperTextAccessible(aNode, aShell)
{
}

nsresult
nsXFormsAccessible::GetBoundChildElementValue(const nsAString& aTagName,
                                              nsAString& aValue)
{
  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mDOMNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNodeList> nodes;
  nsresult rv = mDOMNode->GetChildNodes(getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 length;
  rv = nodes->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = nodes->Item(index, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

    if (content->NodeInfo()->Equals(aTagName) &&
        content->NodeInfo()->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS))) {
      return sXFormsService->GetValue(node, aValue);
    }
  }

  aValue.Truncate();
  return NS_OK;
}

void
nsXFormsAccessible::CacheSelectChildren(nsIDOMNode *aContainerNode)
{
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = eChildCountUninitialized;
    return;
  }

  if (mAccChildCount != eChildCountUninitialized)
    return;

  nsIAccessibilityService *accService = GetAccService();
  if (!accService)
    return;

  nsCOMPtr<nsIDOMNode> container(aContainerNode);
  if (!container)
    container = mDOMNode;

  nsCOMPtr<nsIDOMNodeList> children;
  sXFormsService->GetSelectChildrenFor(container, getter_AddRefs(children));

  if (!children)
    return;

  PRUint32 length = 0;
  children->GetLength(&length);

  nsCOMPtr<nsIAccessible> accessible;
  nsCOMPtr<nsPIAccessible> currAccessible;
  nsCOMPtr<nsPIAccessible> prevAccessible;

  PRUint32 childLength = 0;
  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> child;
    children->Item(index, getter_AddRefs(child));
    if (!child)
      continue;

    accService->GetAttachedAccessibleFor(child, getter_AddRefs(accessible));
    currAccessible = do_QueryInterface(accessible);
    if (!currAccessible)
      continue;

    if (childLength == 0)
      SetFirstChild(accessible);

    currAccessible->SetParent(this);
    if (prevAccessible) {
      prevAccessible->SetNextSibling(accessible);
    }
    currAccessible.swap(prevAccessible);
    childLength++;
  }

  mAccChildCount = childLength;
}

// nsIAccessible

NS_IMETHODIMP
nsXFormsAccessible::GetValue(nsAString& aValue)
{
  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);
  return sXFormsService->GetValue(mDOMNode, aValue);
}

NS_IMETHODIMP
nsXFormsAccessible::GetState(PRUint32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = 0;

  NS_ENSURE_TRUE(sXFormsService, NS_ERROR_FAILURE);

  PRBool isRelevant = PR_FALSE;
  nsresult rv = sXFormsService->IsRelevant(mDOMNode, &isRelevant);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isReadonly = PR_FALSE;
  rv = sXFormsService->IsReadonly(mDOMNode, &isReadonly);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isRequired = PR_FALSE;
  rv = sXFormsService->IsRequired(mDOMNode, &isRequired);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isValid = PR_FALSE;
  rv = sXFormsService->IsValid(mDOMNode, &isValid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsHyperTextAccessible::GetState(aState);
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

NS_IMETHODIMP
nsXFormsAccessible::GetName(nsAString& aName)
{
  nsAutoString name;
  nsresult rv = GetTextFromRelationID(nsAccessibilityAtoms::labelledby, name);
  if (NS_SUCCEEDED(rv) && !name.IsEmpty()) {
    aName = name;
    return NS_OK;
  }

  // search the xforms:label element
  return GetBoundChildElementValue(NS_LITERAL_STRING("label"), aName);
}

NS_IMETHODIMP
nsXFormsAccessible::GetDescription(nsAString& aDescription)
{
  nsAutoString description;
  nsresult rv = GetTextFromRelationID(nsAccessibilityAtoms::describedby,
                                      description);

  if (NS_SUCCEEDED(rv) && !description.IsEmpty()) {
    aDescription = description;
    return NS_OK;
  }

  // search the xforms:hint element
  return GetBoundChildElementValue(NS_LITERAL_STRING("hint"), aDescription);
}

NS_IMETHODIMP
nsXFormsAccessible::GetAttributes(nsIPersistentProperties **aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  nsresult rv = nsHyperTextAccessible::GetAttributes(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString name;
  rv = sXFormsService->GetBuiltinTypeName(mDOMNode, name);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString unused;
  return (*aAttributes)->SetStringProperty(NS_LITERAL_CSTRING("datatype"),
                                           name, unused);
}

NS_IMETHODIMP
nsXFormsAccessible::GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren)
{
  NS_ENSURE_ARG_POINTER(aAllowsAnonChildren);

  *aAllowsAnonChildren = PR_FALSE;
  return NS_OK;
}

// nsXFormsContainerAccessible

nsXFormsContainerAccessible::
nsXFormsContainerAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
  nsXFormsAccessible(aNode, aShell)
{
}

NS_IMETHODIMP
nsXFormsContainerAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);

  *aRole = nsIAccessibleRole::ROLE_GROUPING;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsContainerAccessible::GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren)
{
  NS_ENSURE_ARG_POINTER(aAllowsAnonChildren);

  *aAllowsAnonChildren = PR_TRUE;
  return NS_OK;
}

// nsXFormsEditableAccessible

nsXFormsEditableAccessible::
  nsXFormsEditableAccessible(nsIDOMNode *aNode, nsIWeakReference *aShell):
  nsXFormsAccessible(aNode, aShell)
{
}

NS_IMETHODIMP
nsXFormsEditableAccessible::GetExtState(PRUint32 *aExtState)
{
  NS_ENSURE_ARG_POINTER(aExtState);

  *aExtState = 0;

  nsresult rv = nsXFormsAccessible::GetExtState(aExtState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mEditor)
    return NS_OK;

  PRBool isReadonly = PR_FALSE;
  rv = sXFormsService->IsReadonly(mDOMNode, &isReadonly);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isReadonly) {
    PRBool isRelevant = PR_FALSE;
    rv = sXFormsService->IsRelevant(mDOMNode, &isRelevant);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isRelevant) {
      *aExtState |= nsIAccessibleStates::EXT_STATE_EDITABLE |
                    nsIAccessibleStates::EXT_STATE_SELECTABLE_TEXT;
    }
  }

  PRUint32 flags;
  mEditor->GetFlags(&flags);
  if (flags & nsIPlaintextEditor::eEditorSingleLineMask)
    *aExtState |= nsIAccessibleStates::EXT_STATE_SINGLE_LINE;
  else
    *aExtState |= nsIAccessibleStates::EXT_STATE_MULTI_LINE;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsEditableAccessible::Init()
{
  nsCOMPtr<nsIEditor> editor;
  sXFormsService->GetEditor(mDOMNode, getter_AddRefs(editor));
  SetEditor(editor);

  return nsXFormsAccessible::Init();
}

NS_IMETHODIMP
nsXFormsEditableAccessible::Shutdown()
{
  SetEditor(nsnull);
  return nsXFormsAccessible::Shutdown();
}

already_AddRefed<nsIEditor>
nsXFormsEditableAccessible::GetEditor()
{
  nsIEditor *editor = mEditor;
  NS_IF_ADDREF(editor);
  return editor;
}

void
nsXFormsEditableAccessible::SetEditor(nsIEditor *aEditor)
{
  if (mEditor)
    mEditor->RemoveEditActionListener(this);

  mEditor = aEditor;
  if (mEditor)
    mEditor->AddEditActionListener(this);
}

// nsXFormsSelectableAccessible


NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsSelectableAccessible,
                             nsXFormsEditableAccessible,
                             nsIAccessibleSelectable)

nsXFormsSelectableAccessible::
  nsXFormsSelectableAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell) :
  nsXFormsEditableAccessible(aNode, aShell)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content)
    return;

  mIsSelect1Element =
    content->NodeInfo()->Equals(nsAccessibilityAtoms::select1);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::GetSelectedChildren(nsIArray **aAccessibles)
{
  NS_ENSURE_ARG_POINTER(aAccessibles);

  *aAccessibles = nsnull;

  nsCOMPtr<nsIMutableArray> accessibles =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(accessibles, NS_ERROR_OUT_OF_MEMORY);

  nsIAccessibilityService* accService = GetAccService();
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  nsresult rv;

  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> item;
    rv = sXFormsService->GetSelectedItemForSelect1(mDOMNode,
                                                   getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!item)
      return NS_OK;

    nsCOMPtr<nsIAccessible> accessible;
    accService->GetAccessibleFor(item, getter_AddRefs(accessible));
    NS_ENSURE_TRUE(accessible, NS_ERROR_FAILURE);

    accessibles->AppendElement(accessible, PR_FALSE);
    NS_ADDREF(*aAccessibles = accessibles);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNodeList> items;
  rv = sXFormsService->GetSelectedItemsForSelect(mDOMNode,
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
    accService->GetAccessibleFor(item, getter_AddRefs(accessible));
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
  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> item;
    rv = sXFormsService->GetSelectedItemForSelect1(mDOMNode,
                                                   getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (item)
      *aCount = 1;

    return NS_OK;
  }

  nsCOMPtr<nsIDOMNodeList> items;
  rv = sXFormsService->GetSelectedItemsForSelect(mDOMNode,
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

  if (mIsSelect1Element)
    return sXFormsService->SetSelectedItemForSelect1(mDOMNode, item);

  return sXFormsService->AddItemToSelectionForSelect(mDOMNode, item);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::RemoveChildFromSelection(PRInt32 aIndex)
{
  nsCOMPtr<nsIDOMNode> item = GetItemByIndex(&aIndex);
  if (!item)
    return NS_OK;

  nsresult rv;
  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> selitem;
    rv = sXFormsService->GetSelectedItemForSelect1(mDOMNode,
                                                   getter_AddRefs(selitem));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (selitem != item)
      return NS_ERROR_FAILURE;
    return sXFormsService->SetSelectedItemForSelect1(mDOMNode, nsnull);
  }

  return sXFormsService->RemoveItemFromSelectionForSelect(mDOMNode, item);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::RefSelection(PRInt32 aIndex,
                                           nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  nsIAccessibilityService* accService = GetAccService();
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  nsresult rv;
  if (mIsSelect1Element) {
    if (aIndex != 0)
      return NS_OK;

    nsCOMPtr<nsIDOMNode> item;
    rv = sXFormsService->GetSelectedItemForSelect1(mDOMNode,
                                                   getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    if (item)
      return accService->GetAccessibleFor(item, aAccessible);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNodeList> items;
  rv = sXFormsService->GetSelectedItemsForSelect(mDOMNode,
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
  return accService->GetAccessibleFor(item, getter_AddRefs(accessible));
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
  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> selitem;
    rv = sXFormsService->GetSelectedItemForSelect1(mDOMNode,
                                                   getter_AddRefs(selitem));
    NS_ENSURE_SUCCESS(rv, rv);

    if (selitem == item)
      *aIsSelected = PR_TRUE;
    return NS_OK;
  }

  return sXFormsService->IsSelectItemSelected(mDOMNode, item, aIsSelected);
}

NS_IMETHODIMP
nsXFormsSelectableAccessible::ClearSelection()
{
  if (mIsSelect1Element)
    return sXFormsService->SetSelectedItemForSelect1(mDOMNode, nsnull);

  return sXFormsService->ClearSelectionForSelect(mDOMNode);
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
  return sXFormsService->SelectAllItemsForSelect(mDOMNode);
}

already_AddRefed<nsIDOMNode>
nsXFormsSelectableAccessible::GetItemByIndex(PRInt32 *aIndex,
                                             nsIAccessible *aAccessible)
{
  nsCOMPtr<nsIAccessible> accessible(aAccessible ? aAccessible : this);

  nsCOMPtr<nsIAccessible> curAccChild;
  accessible->GetFirstChild(getter_AddRefs(curAccChild));

  while (curAccChild) {
    nsCOMPtr<nsIAccessNode> curAccNodeChild(do_QueryInterface(curAccChild));
    if (curAccNodeChild) {
      nsCOMPtr<nsIDOMNode> curChildNode;
      curAccNodeChild->GetDOMNode(getter_AddRefs(curChildNode));
      nsCOMPtr<nsIContent> curChildContent(do_QueryInterface(curChildNode));
      if (curChildContent) {
        nsCOMPtr<nsINodeInfo> nodeInfo = curChildContent->NodeInfo();
        if (nodeInfo->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS))) {
          if (nodeInfo->Equals(nsAccessibilityAtoms::item)) {
            if (!*aIndex) {
              nsIDOMNode *itemNode = nsnull;
              curChildNode.swap(itemNode);
              return itemNode;
            }
            --*aIndex;
          } else if (nodeInfo->Equals(nsAccessibilityAtoms::choices)) {
            nsIDOMNode *itemNode = GetItemByIndex(aIndex, curAccChild).get();
            if (itemNode)
              return itemNode;
          }
        }
      }
    }

    nsCOMPtr<nsIAccessible> nextAccChild;
    curAccChild->GetNextSibling(getter_AddRefs(nextAccChild));
    curAccChild.swap(nextAccChild);
  }

  return nsnull;
}


// nsXFormsSelectableItemAccessible

nsXFormsSelectableItemAccessible::
  nsXFormsSelectableItemAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell) :
  nsXFormsAccessible(aNode, aShell)
{
}

NS_IMETHODIMP
nsXFormsSelectableItemAccessible::GetValue(nsAString& aValue)
{
  return sXFormsService->GetValue(mDOMNode, aValue);
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

  return DoCommand();
}

PRBool
nsXFormsSelectableItemAccessible::IsItemSelected()
{
  nsresult rv;

  nsCOMPtr<nsINode> parent = do_QueryInterface(mDOMNode);
  while (parent = parent->GetNodeParent()) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(parent));
    if (!content)
      return PR_FALSE;

    nsCOMPtr<nsINodeInfo> nodeinfo = content->NodeInfo();
    if (!nodeinfo->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS)))
      continue;

    nsCOMPtr<nsIDOMNode> select(do_QueryInterface(parent));
    if (!select)
      continue;

    if (nodeinfo->Equals(nsAccessibilityAtoms::select)) {
      PRBool isSelected = PR_FALSE;
      rv = sXFormsService->IsSelectItemSelected(select, mDOMNode, &isSelected);
      return NS_SUCCEEDED(rv) && isSelected;
    }

    if (nodeinfo->Equals(nsAccessibilityAtoms::select1)) {
      nsCOMPtr<nsIDOMNode> selitem;
      rv = sXFormsService->GetSelectedItemForSelect1(select,
                                                     getter_AddRefs(selitem));
      return NS_SUCCEEDED(rv) && (selitem == mDOMNode);
    }
  }

  return PR_FALSE;
}

