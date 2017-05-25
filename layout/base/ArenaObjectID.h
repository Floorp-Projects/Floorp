/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* enum type for objects that can be allocated by an nsPresArena */

#ifndef mozilla_ArenaObjectID_h
#define mozilla_ArenaObjectID_h

#include "nsQueryFrame.h"

namespace mozilla {

enum ArenaObjectID {
  eArenaObjectID_DummyBeforeFirstObjectID = nsQueryFrame::NON_FRAME_MARKER - 1,

#define PRES_ARENA_OBJECT(name_) \
  eArenaObjectID_##name_,
#include "nsPresArenaObjectList.h"
#undef PRES_ARENA_OBJECT

  eArenaObjectID_COUNT
};

};

#endif
