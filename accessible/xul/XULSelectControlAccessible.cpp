/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULSelectControlAccessible.h"

#include "nsAccessibilityService.h"
#include "DocAccessible.h"

#include "nsIDOMXULContainerElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIMutableArray.h"
#include "nsIServiceManager.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyboardEventBinding.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULSelectControlAccessible
////////////////////////////////////////////////////////////////////////////////

XULSelectControlAccessible::
  XULSelectControlAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  mGenericTypes |= eSelect;
  mSelectControl = do_QueryInterface(aContent);
}

////////////////////////////////////////////////////////////////////////////////
// XULSelectControlAccessible: Accessible

void
XULSelectControlAccessible::Shutdown()
{
  mSelectControl = nullptr;
  AccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// XULSelectControlAccessible: SelectAccessible

void
XULSelectControlAccessible::SelectedItems(nsTArray<Accessible*>* aItems)
{
  // For XUL multi-select control
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> xulMultiSelect =
    do_QueryInterface(mSelectControl);
  if (xulMultiSelect) {
    int32_t length = 0;
    xulMultiSelect->GetSelectedCount(&length);
    for (int32_t index = 0; index < length; index++) {
      nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm;
      xulMultiSelect->MultiGetSelectedItem(index, getter_AddRefs(itemElm));
      nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemElm));
      Accessible* item = mDoc->GetAccessible(itemNode);
      if (item)
        aItems->AppendElement(item);
    }
  } else {  // Single select?
    nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm;
    mSelectControl->GetSelectedItem(getter_AddRefs(itemElm));
    nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemElm));
    if (itemNode) {
      Accessible* item = mDoc->GetAccessible(itemNode);
      if (item)
        aItems->AppendElement(item);
    }
  }
}

Accessible*
XULSelectControlAccessible::GetSelectedItem(uint32_t aIndex)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm;
  if (multiSelectControl)
    multiSelectControl->MultiGetSelectedItem(aIndex, getter_AddRefs(itemElm));
  else if (aIndex == 0)
    mSelectControl->GetSelectedItem(getter_AddRefs(itemElm));

  nsCOMPtr<nsINode> itemNode(do_QueryInterface(itemElm));
  return itemNode && mDoc ? mDoc->GetAccessible(itemNode) : nullptr;
}

uint32_t
XULSelectControlAccessible::SelectedItemCount()
{
  // For XUL multi-select control
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  if (multiSelectControl) {
    int32_t count = 0;
    multiSelectControl->GetSelectedCount(&count);
    return count;
  }

  // For XUL single-select control/menulist
  int32_t index;
  mSelectControl->GetSelectedIndex(&index);
  return (index >= 0) ? 1 : 0;
}

bool
XULSelectControlAccessible::AddItemToSelection(uint32_t aIndex)
{
  Accessible* item = GetChildAt(aIndex);
  if (!item)
    return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
    do_QueryInterface(item->GetContent());
  if (!itemElm)
    return false;

  bool isItemSelected = false;
  itemElm->GetSelected(&isItemSelected);
  if (isItemSelected)
    return true;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);

  if (multiSelectControl)
    multiSelectControl->AddItemToSelection(itemElm);
  else
    mSelectControl->SetSelectedItem(itemElm);

  return true;
}

bool
XULSelectControlAccessible::RemoveItemFromSelection(uint32_t aIndex)
{
  Accessible* item = GetChildAt(aIndex);
  if (!item)
    return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
      do_QueryInterface(item->GetContent());
  if (!itemElm)
    return false;

  bool isItemSelected = false;
  itemElm->GetSelected(&isItemSelected);
  if (!isItemSelected)
    return true;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);

  if (multiSelectControl)
    multiSelectControl->RemoveItemFromSelection(itemElm);
  else
    mSelectControl->SetSelectedItem(nullptr);

  return true;
}

bool
XULSelectControlAccessible::IsItemSelected(uint32_t aIndex)
{
  Accessible* item = GetChildAt(aIndex);
  if (!item)
    return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
    do_QueryInterface(item->GetContent());
  if (!itemElm)
    return false;

  bool isItemSelected = false;
  itemElm->GetSelected(&isItemSelected);
  return isItemSelected;
}

bool
XULSelectControlAccessible::UnselectAll()
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  multiSelectControl ?
    multiSelectControl->ClearSelection() : mSelectControl->SetSelectedIndex(-1);

  return true;
}

bool
XULSelectControlAccessible::SelectAll()
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  if (multiSelectControl) {
    multiSelectControl->SelectAll();
    return true;
  }

  // otherwise, don't support this method
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULSelectControlAccessible: Widgets

Accessible*
XULSelectControlAccessible::CurrentItem()
{
  if (!mSelectControl)
    return nullptr;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> currentItemElm;
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  if (multiSelectControl)
    multiSelectControl->GetCurrentItem(getter_AddRefs(currentItemElm));
  else
    mSelectControl->GetSelectedItem(getter_AddRefs(currentItemElm));

  nsCOMPtr<nsINode> DOMNode;
  if (currentItemElm)
    DOMNode = do_QueryInterface(currentItemElm);

  if (DOMNode) {
    DocAccessible* document = Document();
    if (document)
      return document->GetAccessible(DOMNode);
  }

  return nullptr;
}

void
XULSelectControlAccessible::SetCurrentItem(Accessible* aItem)
{
  if (!mSelectControl)
    return;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
    do_QueryInterface(aItem->GetContent());
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
    do_QueryInterface(mSelectControl);
  if (multiSelectControl)
    multiSelectControl->SetCurrentItem(itemElm);
  else
    mSelectControl->SetSelectedItem(itemElm);
}
