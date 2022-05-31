/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "LocalAccessible-inl.h"
#include "AccessibleWrap.h"
#include "nsMai.h"
#include "mozilla/Likely.h"

#include <atk/atkobject.h>
#include <atk/atkselection.h>

using namespace mozilla::a11y;

extern "C" {

static gboolean addSelectionCB(AtkSelection* aSelection, gint i) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aSelection));
  if (acc && acc->IsSelect()) {
    return acc->AddItemToSelection(i);
  }

  return FALSE;
}

static gboolean clearSelectionCB(AtkSelection* aSelection) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aSelection));
  if (acc && acc->IsSelect()) {
    return acc->UnselectAll();
  }

  return FALSE;
}

static AtkObject* refSelectionCB(AtkSelection* aSelection, gint i) {
  AtkObject* atkObj = nullptr;
  Accessible* acc = GetInternalObj(ATK_OBJECT(aSelection));
  Accessible* selectedItem = acc->GetSelectedItem(i);
  if (selectedItem) {
    atkObj = GetWrapperFor(selectedItem);
  }

  if (atkObj) {
    g_object_ref(atkObj);
  }

  return atkObj;
}

static gint getSelectionCountCB(AtkSelection* aSelection) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aSelection));
  if (acc && acc->IsSelect()) {
    return acc->SelectedItemCount();
  }

  return -1;
}

static gboolean isChildSelectedCB(AtkSelection* aSelection, gint i) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aSelection));
  if (acc && acc->IsSelect()) {
    return acc->IsItemSelected(i);
  }

  return FALSE;
}

static gboolean removeSelectionCB(AtkSelection* aSelection, gint i) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aSelection));
  if (acc && acc->IsSelect()) {
    return acc->RemoveItemFromSelection(i);
  }

  return FALSE;
}

static gboolean selectAllSelectionCB(AtkSelection* aSelection) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aSelection));
  if (acc && acc->IsSelect()) {
    return acc->SelectAll();
  }

  return FALSE;
}
}

void selectionInterfaceInitCB(AtkSelectionIface* aIface) {
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface)) return;

  aIface->add_selection = addSelectionCB;
  aIface->clear_selection = clearSelectionCB;
  aIface->ref_selection = refSelectionCB;
  aIface->get_selection_count = getSelectionCountCB;
  aIface->is_child_selected = isChildSelectedCB;
  aIface->remove_selection = removeSelectionCB;
  aIface->select_all_selection = selectAllSelectionCB;
}
