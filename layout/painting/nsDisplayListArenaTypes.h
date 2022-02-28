/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all types that can be allocated in the display list's nsPresArena,
   for preprocessing */

#define DECLARE_DISPLAY_ITEM_TYPE(name_, ...) DISPLAY_LIST_ARENA_OBJECT(name_)
DISPLAY_LIST_ARENA_OBJECT(UNUSED)  // DisplayItemType::TYPE_ZERO
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE
DISPLAY_LIST_ARENA_OBJECT(CLIPCHAIN)
DISPLAY_LIST_ARENA_OBJECT(LISTNODE)
