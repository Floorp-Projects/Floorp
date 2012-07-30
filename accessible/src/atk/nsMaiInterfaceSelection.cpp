/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "AccessibleWrap.h"
#include "nsMai.h"

#include <atk/atk.h>

extern "C" {

static gboolean
addSelectionCB(AtkSelection *aSelection, gint i)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (!accWrap || !accWrap->IsSelect())
    return FALSE;

  return accWrap->AddItemToSelection(i);
}

static gboolean
clearSelectionCB(AtkSelection *aSelection)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (!accWrap || !accWrap->IsSelect())
    return FALSE;

  return accWrap->UnselectAll();
}

static AtkObject*
refSelectionCB(AtkSelection *aSelection, gint i)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (!accWrap || !accWrap->IsSelect())
    return nullptr;

  Accessible* selectedItem = accWrap->GetSelectedItem(i);
  if (!selectedItem)
    return nullptr;

  AtkObject* atkObj = AccessibleWrap::GetAtkObject(selectedItem);
  if (atkObj)
    g_object_ref(atkObj);

  return atkObj;
}

static gint
getSelectionCountCB(AtkSelection *aSelection)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (!accWrap || !accWrap->IsSelect())
    return -1;

  return accWrap->SelectedItemCount();
}

static gboolean
isChildSelectedCB(AtkSelection *aSelection, gint i)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (!accWrap || !accWrap->IsSelect())
    return FALSE;

  return accWrap->IsItemSelected(i);
}

static gboolean
removeSelectionCB(AtkSelection *aSelection, gint i)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (!accWrap || !accWrap->IsSelect())
    return FALSE;

  return accWrap->RemoveItemFromSelection(i);
}

static gboolean
selectAllSelectionCB(AtkSelection *aSelection)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aSelection));
  if (!accWrap || !accWrap->IsSelect())
    return FALSE;

  return accWrap->SelectAll();
}
}

void
selectionInterfaceInitCB(AtkSelectionIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (NS_UNLIKELY(!aIface))
    return;

  aIface->add_selection = addSelectionCB;
  aIface->clear_selection = clearSelectionCB;
  aIface->ref_selection = refSelectionCB;
  aIface->get_selection_count = getSelectionCountCB;
  aIface->is_child_selected = isChildSelectedCB;
  aIface->remove_selection = removeSelectionCB;
  aIface->select_all_selection = selectAllSelectionCB;
}
