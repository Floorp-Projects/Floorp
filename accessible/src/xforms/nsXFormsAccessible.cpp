/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXFormsAccessible.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
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

nsIXFormsUtilityService *nsXFormsAccessibleBase::sXFormsService = nullptr;

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
  nsXFormsAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
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
    Accessible* accessible =
      GetAccService()->GetOrCreateAccessible(child, mDoc);
    if (!accessible)
      continue;

    AppendChild(accessible);
  }
}

PRUint64
nsXFormsAccessible::NativeState()
{
  NS_ENSURE_TRUE(sXFormsService, 0);

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  bool isReadonly = false;
  nsresult rv = sXFormsService->IsReadonly(DOMNode, &isReadonly);
  NS_ENSURE_SUCCESS(rv, 0);

  bool isRequired = false;
  rv = sXFormsService->IsRequired(DOMNode, &isRequired);
  NS_ENSURE_SUCCESS(rv, 0);

  bool isValid = false;
  rv = sXFormsService->IsValid(DOMNode, &isValid);
  NS_ENSURE_SUCCESS(rv, 0);

  PRUint64 states = HyperTextAccessibleWrap::NativeState();

  if (NativelyUnavailable())
    states |= states::UNAVAILABLE;

  if (isReadonly)
    states |= states::READONLY;

  if (isRequired)
    states |= states::REQUIRED;

  if (!isValid)
    states |= states::INVALID;

  return states;
}

bool
nsXFormsAccessible::NativelyUnavailable() const
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  bool isRelevant = false;
  sXFormsService->IsRelevant(DOMNode, &isRelevant);
  return !isRelevant;
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

void
nsXFormsAccessible::Value(nsString& aValue)
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  sXFormsService->GetValue(DOMNode, aValue);
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
  nsXFormsContainerAccessible(nsIContent* aContent, DocAccessible* aDoc) :
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
  nsXFormsEditableAccessible(nsIContent* aContent, DocAccessible* aDoc) :
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
  nsXFormsSelectableAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsEditableAccessible(aContent, aDoc), mIsSelect1Element(nullptr)
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
    return nullptr;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));

  if (mIsSelect1Element) {
    nsCOMPtr<nsIDOMNode> itemDOMNode;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(itemDOMNode));
    if (NS_FAILED(rv) || !itemDOMNode || !mDoc)
      return nullptr;

    nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemDOMNode));
    nsIAccessible* item = mDoc->GetAccessible(itemNode);
    if (item)
      selectedItems->AppendElement(item, false);

    nsIMutableArray* items = nullptr;
    selectedItems.forget(&items);
    return items;
  }

  nsCOMPtr<nsIDOMNodeList> itemNodeList;
  rv = sXFormsService->GetSelectedItemsForSelect(DOMNode,
                                                 getter_AddRefs(itemNodeList));
  if (NS_FAILED(rv) || !itemNodeList || !mDoc)
    return nullptr;

  PRUint32 length = 0;
  itemNodeList->GetLength(&length);

  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> itemDOMNode;
    itemNodeList->Item(index, getter_AddRefs(itemDOMNode));
    if (!itemDOMNode)
      return nullptr;

    nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemDOMNode));
    nsIAccessible* item = mDoc->GetAccessible(itemNode);
    if (item)
      selectedItems->AppendElement(item, false);
  }

  nsIMutableArray* items = nullptr;
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
      sXFormsService->SetSelectedItemForSelect1(DOMNode, nullptr);

    return true;
  }

  sXFormsService->RemoveItemFromSelectionForSelect(DOMNode, itemDOMNode);
  return true;
}

Accessible*
nsXFormsSelectableAccessible::GetSelectedItem(PRUint32 aIndex)
{
  if (!mDoc)
    return nullptr;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  if (mIsSelect1Element) {
    if (aIndex != 0)
      return nullptr;

    nsCOMPtr<nsIDOMNode> itemDOMNode;
    rv = sXFormsService->GetSelectedItemForSelect1(DOMNode,
                                                   getter_AddRefs(itemDOMNode));
    if (NS_SUCCEEDED(rv) && itemDOMNode) {
      nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemDOMNode));
      return mDoc->GetAccessible(itemNode);
    }
    return nullptr;
  }

  nsCOMPtr<nsIDOMNodeList> itemNodeList;
  rv = sXFormsService->GetSelectedItemsForSelect(DOMNode,
                                                 getter_AddRefs(itemNodeList));
  if (NS_FAILED(rv) || !itemNodeList)
    return nullptr;

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
    sXFormsService->SetSelectedItemForSelect1(DOMNode, nullptr);

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
                                             Accessible* aAccessible)
{
  Accessible* accessible = aAccessible ? aAccessible : this;
  PRUint32 childCount = accessible->ChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = accessible->GetChildAt(childIdx);
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

  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectableItemAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectableItemAccessible::
  nsXFormsSelectableItemAccessible(nsIContent* aContent,
                                   DocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

void
nsXFormsSelectableItemAccessible::Value(nsString& aValue)
{
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  sXFormsService->GetValue(DOMNode, aValue);
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

