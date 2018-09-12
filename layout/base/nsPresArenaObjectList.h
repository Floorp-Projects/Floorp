/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all types that can be allocated in an nsPresArena, for
   preprocessing */

// These are objects that can be stored in the pres arena

PRES_ARENA_OBJECT(GeckoComputedStyle)

PRES_ARENA_OBJECT(nsLineBox)
PRES_ARENA_OBJECT(DisplayItemData)
PRES_ARENA_OBJECT(nsFrameList)
PRES_ARENA_OBJECT(CustomCounterStyle)
PRES_ARENA_OBJECT(DependentBuiltinCounterStyle)
PRES_ARENA_OBJECT(nsCallbackEventRequest)
PRES_ARENA_OBJECT(nsIntervalSet_Interval)
PRES_ARENA_OBJECT(CellData)
PRES_ARENA_OBJECT(BCCellData)
