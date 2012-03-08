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
#include "nsDocAccessible.h"
#include "nsTextEquivUtils.h"
#include "Role.h"
#include "States.h"
#include "Statistics.h"

#include "nscore.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIEditor.h"
#include "nsIMutableArray.h"
#include "nsIXFormsUtilityService.h"
#include "nsIPlaintextEditor.h"
#include "nsINodeList.h"

using namespace mozilla::a11y;

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
  statistics::XFormsAccessibleUsed();
}

////////////////////////////////////////////////////////////////////////////////
// nsXFormsAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsAccessible::
  nsXFormsAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsHyperTextAccessibleWrap(aContent, aDoc)
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

  PRUint32 length = 0;
  children->GetLength(&length);

  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> DOMChild;
    children->Item(index, getter_AddRefs(DOMChild));
    if (!DOMChild)
      continue;

    nsCOMPtr<nsIContent> child(do_QueryInterface(DOMChild));
    nsAccessible* accessible =
      GetAccService()->GetOrCreateAccessible(child, mDoc);
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

PRUint64
nsXFormsAccessible::NativeState()
{
  NS_ENSURE_TRUE(sXFormsService, 0);

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  bool isRelevant = false;
  nsresult rv = sXFormsService->IsRelevant(DOMNode, &isRelevant);
  NS_ENSURE_SUCCESS(rv, 0);

  bool isReadonly = false;
  rv = sXFormsService->IsReadonly(DOMNode, &isReadonly);
  NS_ENSURE_SUCCESS(rv, 0);

  bool isRequired = false;
  rv = sXFormsService->IsRequired(DOMNode, &isRequired);
  NS_ENSURE_SUCCESS(rv, 0);

  bool isValid = false;
  rv = sXFormsService->IsValid(DOMNode, &isValid);
  NS_ENSURE_SUCCESS(rv, 0);

  PRUint64 states = nsHyperTextAccessibleWrap::NativeState();

  if (!isRelevant)
    states |= states::UNAVAILABLE;

  if (isReadonly)
    states |= states::READONLY;

  if (isRequired)
    states |= states::REQUIRED;

  if (!isValid)
    states |= states::INVALID;

  return states;
}

nsresult
nsXFormsAccessible::GetNameInternal(nsAString& aName)
{
  // search the xforms:label element
  return GetBoundChildElementValue(NS_LITERAL_STRING("label"), aName);
}

void
nsXFormsAccessible::Description(nsString& aDescription)
{
  nsTextEquivUtils::
    GetTextEquivFromIDRefs(this, nsGkAtoms::aria_describedby,
                           aDescription);

  if (aDescription.IsEmpty())
    GetBoundChildElementValue(NS_LITERAL_STRING("hint"), aDescription);
}

bool
nsXFormsAccessible::CanHaveAnonChildren()
{
  return false;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsContainerAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsContainerAccessible::
  nsXFormsContainerAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsContainerAccessible::NativeRole()
{
  return roles::GROUPING;
}

bool
nsXFormsContainerAccessible::CanHaveAnonChildren()
{
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsEditableAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsEditableAccessible::
  nsXFormsEditableAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

PRUint64
nsXFormsEditableAccessible::NativeState()
{
  PRUint64 state = nsXFormsAccessible::NativeState();

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  bool isReadonly = false;
  nsresult rv = sXFormsService->IsReadonly(DOMNode, &isReadonly);
  NS_ENSURE_SUCCESS(rv, state);

  if (!isReadonly) {
    bool isRelevant = false;
    rv = sXFormsService->IsRelevant(DOMNode, &isRelevant);
    NS_ENSURE_SUCCESS(rv, state);
    if (isRelevant) {
      state |= states::EDITABLE | states::SELECTABLE_TEXT;
    }
  }

  nsCOMPtr<nsIEditor> editor = GetEditor();
  NS_ENSURE_TRUE(editor, state);
  PRUint32 flags;
  editor->GetFlags(&flags);
  if (flags & nsIPlaintextEditor::eEditorSingleLineMask)
    state |= states::SINGLE_LINE;
  else
    state |= states::MULTI_LINE;

  return state;
}

already_AddRefed<nsIEditor>
nsXFormsEditableAccessible::GetEditor() const
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  nsCOMPtr<nsIEditor> editor;
  sXFormsService->GetEditor(DOMNode, getter_AddRefs(editor));
  return editor.forget();
}

////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectableAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectableAccessible::
  nsXFormsSelectableAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsEditableAccessible(aContent, aDoc), mIsSelect1Element(nsnull)
{
  mIsSelect1Element =
    mContent->NodeInfo()->Equals(nsGkAtoms::select1);
}

bool
nsXFormsSelectableAccessible::IsSelect()
{
  return true;
}

already_AddRefed<nsIArray>
nsXFormsSelectableAccessible::SelectedItems()
{
  nsCOMPtr<nsIMutableArray> selectedItems =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!selectedItems)
    return nsnull;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> itemDOMNode;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(itemDOMNode));
    if (NS_FAILED(rv) || !itemDOMNode || !mDoc)
      return nsnull;

    nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemDOMNode));
    nsIAccessible* item = mDoc->GetAccessible(itemNode);
    if (item)
      selectedItems->AppendElement(item, false);

    nsIMutableArray* items = nsnull;
    selectedItems.forget(&items);
    return items;
  }

  nsCOMPtr<nsIDOMNodeList> itemNodeList;
  rv = sXFormsService->GetSelectedItemsForSelect(DOMNode,
                                                 getter_AddRefs(itemNodeList));
  if (NS_FAILED(rv) || !itemNodeList || !mDoc)
    return nsnull;

  PRUint32 length = 0;
  itemNodeList->GetLength(&length);

  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> itemDOMNode;
    itemNodeList->Item(index, getter_AddRefs(itemDOMNode));
    if (!itemDOMNode)
      return nsnull;

    nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemDOMNode));
    nsIAccessible* item = mDoc->GetAccessible(itemNode);
    if (item)
      selectedItems->AppendElement(item, false);
  }

  nsIMutableArray* items = nsnull;
  selectedItems.forget(&items);
  return items;
}

PRUint32
nsXFormsSelectableAccessible::SelectedItemCount()
{
  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> item;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(item));
    return NS_SUCCEEDED(rv) && item ? 1 : 0;
  }

  nsCOMPtr<nsIDOMNodeList> itemNodeList;
  rv = sXFormsService->GetSelectedItemsForSelect(DOMNode,
                                                 getter_AddRefs(itemNodeList));
  if (NS_FAILED(rv) || !itemNodeList)
    return 0;

  PRUint32 length = 0;
  itemNodeList->GetLength(&length);
  return length;
}

bool
nsXFormsSelectableAccessible::AddItemToSelection(PRUint32 aIndex)
{
  nsCOMPtr<nsIDOMNode> itemDOMNode(do_QueryInterface(GetItemByIndex(&aIndex)));
  if (!itemDOMNode)
    return false;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element)
    sXFormsService->SetSelectedItemForSelect1(DOMNode, itemDOMNode);
  else
    sXFormsService->AddItemToSelectionForSelect(DOMNode, itemDOMNode);

  return true;
}

bool
nsXFormsSelectableAccessible::RemoveItemFromSelection(PRUint32 aIndex)
{
  nsCOMPtr<nsIDOMNode> itemDOMNode(do_QueryInterface(GetItemByIndex(&aIndex)));
  if (!itemDOMNode)
    return false;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> selItemDOMNode;
    sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                              getter_AddRefs(selItemDOMNode));
    if (selItemDOMNode == itemDOMNode)
      sXFormsService->SetSelectedItemForSelect1(DOMNode, nsnull);

    return true;
  }

  sXFormsService->RemoveItemFromSelectionForSelect(DOMNode, itemDOMNode);
  return true;
}

nsAccessible*
nsXFormsSelectableAccessible::GetSelectedItem(PRUint32 aIndex)
{
  if (!mDoc)
    return nsnull;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element) {
    if (aIndex != 0)
      return nsnull;

    nsCOMPtr<nsIDOMNode> itemDOMNode;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(itemDOMNode));
    if (NS_SUCCEEDED(rv) && itemDOMNode) {
      nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemDOMNode));
      return mDoc->GetAccessible(itemNode);
    }
    return nsnull;
  }

  nsCOMPtr<nsIDOMNodeList> itemNodeList;
  rv = sXFormsService->GetSelectedItemsForSelect(DOMNode,
                                                 getter_AddRefs(itemNodeList));
  if (NS_FAILED(rv) || !itemNodeList)
    return nsnull;

  nsCOMPtr<nsIDOMNode> itemDOMNode;
  itemNodeList->Item(aIndex, getter_AddRefs(itemDOMNode));

  nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemDOMNode));
  return mDoc->GetAccessible(itemNode);
}

bool
nsXFormsSelectableAccessible::IsItemSelected(PRUint32 aIndex)
{
  nsCOMPtr<nsIDOMNode> itemDOMNode(do_QueryInterface(GetItemByIndex(&aIndex)));
  if (!itemDOMNode)
    return false;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> selItemDOMNode;
    sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                              getter_AddRefs(selItemDOMNode));
    return selItemDOMNode == itemDOMNode;
  }

  bool isSelected = false;
  sXFormsService->IsSelectItemSelected(DOMNode, itemDOMNode, &isSelected);
  return isSelected;
}

bool
nsXFormsSelectableAccessible::UnselectAll()
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element)
    sXFormsService->SetSelectedItemForSelect1(DOMNode, nsnull);

  sXFormsService->ClearSelectionForSelect(DOMNode);
  return true;
}

bool
nsXFormsSelectableAccessible::SelectAll()
{
  if (mIsSelect1Element)
    return false;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  sXFormsService->SelectAllItemsForSelect(DOMNode);
  return true;
}

nsIContent*
nsXFormsSelectableAccessible::GetItemByIndex(PRUint32* aIndex,
                                             nsAccessible* aAccessible)
{
  nsAccessible* accessible = aAccessible ? aAccessible : this;
  PRInt32 childCount = accessible->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible *child = accessible->GetChildAt(childIdx);
    nsIContent* childContent = child->GetContent();
    nsINodeInfo *nodeInfo = childContent->NodeInfo();
    if (nodeInfo->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS))) {
      if (nodeInfo->Equals(nsGkAtoms::item)) {
        if (!*aIndex)
          return childContent;

        --*aIndex;
      } else if (nodeInfo->Equals(nsGkAtoms::choices)) {
        nsIContent* itemContent = GetItemByIndex(aIndex, child);
        if (itemContent)
          return itemContent;
      }
    }
  }

  return nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectableItemAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectableItemAccessible::
  nsXFormsSelectableItemAccessible(nsIContent* aContent,
                                   nsDocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

NS_IMETHODIMP
nsXFormsSelectableItemAccessible::GetValue(nsAString& aValue)
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  return sXFormsService->GetValue(DOMNode, aValue);
}

PRUint8
nsXFormsSelectableItemAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXFormsSelectableItemAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

bool
nsXFormsSelectableItemAccessible::IsSelected()
{
  nsresult rv;

  nsINode* parent = mContent;
  while ((parent = parent->GetNodeParent())) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(parent));
    if (!content)
      return false;

    nsCOMPtr<nsINodeInfo> nodeinfo = content->NodeInfo();
    if (!nodeinfo->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS)))
      continue;

    nsCOMPtr<nsIDOMNode> select(do_QueryInterface(parent));
    if (!select)
      continue;

    nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
    if (nodeinfo->Equals(nsGkAtoms::select)) {
      bool isSelected = false;
      rv = sXFormsService->IsSelectItemSelected(select, DOMNode, &isSelected);
      return NS_SUCCEEDED(rv) && isSelected;
    }

    if (nodeinfo->Equals(nsGkAtoms::select1)) {
      nsCOMPtr<nsIDOMNode> selitem;
      rv = sXFormsService->GetSelectedItemForSelect1(select,
                                                     getter_AddRefs(selitem));
      return NS_SUCCEEDED(rv) && (selitem == DOMNode);
    }
  }

  return false;
}

