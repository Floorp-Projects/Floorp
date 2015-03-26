/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "Accessible-inl.h"
#include "AccessibleWrap.h"
#include "nsMai.h"
#include "ProxyAccessible.h"
#include "mozilla/Likely.h"

#include <atk/atk.h>

using namespace mozilla::a11y;

extern "C" {

static gboolean
addSelectionCB(AtkSelection *aSelection, gint i)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (accWrap && accWrap->IsSelect()) {
    return accWrap->AddItemToSelection(i);
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aSelection))) {
    return proxy->AddItemToSelection(i);
  }

  return FALSE;
}

static gboolean
clearSelectionCB(AtkSelection *aSelection)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (accWrap && accWrap->IsSelect()) {
    return accWrap->UnselectAll();
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aSelection))) {
    return proxy->UnselectAll();
  }

  return FALSE;
}

static AtkObject*
refSelectionCB(AtkSelection *aSelection, gint i)
{
  AtkObject* atkObj = nullptr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (accWrap && accWrap->IsSelect()) {
    Accessible* selectedItem = accWrap->GetSelectedItem(i);
    if (!selectedItem) {
      return nullptr;
    }

    atkObj = AccessibleWrap::GetAtkObject(selectedItem);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aSelection))) {
    ProxyAccessible* selectedItem = proxy->GetSelectedItem(i);
    if (selectedItem) {
      atkObj = GetWrapperFor(selectedItem);
    }
  }

  if (atkObj) {
    g_object_ref(atkObj);
  }

  return atkObj;
}

static gint
getSelectionCountCB(AtkSelection *aSelection)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (accWrap && accWrap->IsSelect()) {
    return accWrap->SelectedItemCount();
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aSelection))) {
    return proxy->SelectedItemCount();
  }

  return -1;
}

static gboolean
isChildSelectedCB(AtkSelection *aSelection, gint i)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (accWrap && accWrap->IsSelect()) {
    return accWrap->IsItemSelected(i);
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aSelection))) {
    return proxy->IsItemSelected(i);
  }

  return FALSE;
}

static gboolean
removeSelectionCB(AtkSelection *aSelection, gint i)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (accWrap && accWrap->IsSelect()) {
    return accWrap->RemoveItemFromSelection(i);
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aSelection))) {
    return proxy->RemoveItemFromSelection(i);
  }

  return FALSE;
}

static gboolean
selectAllSelectionCB(AtkSelection *aSelection)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (accWrap && accWrap->IsSelect()) {
    return accWrap->SelectAll();
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aSelection))) {
    return proxy->SelectAll();
  }

  return FALSE;
}
}

void
selectionInterfaceInitCB(AtkSelectionIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface))
    return;

  aIface->add_selection = addSelectionCB;
  aIface->clear_selection = clearSelectionCB;
  aIface->ref_selection = refSelectionCB;
  aIface->get_selection_count = getSelectionCountCB;
  aIface->is_child_selected = isChildSelectedCB;
  aIface->remove_selection = removeSelectionCB;
  aIface->select_all_selection = selectAllSelectionCB;
}
