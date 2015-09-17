/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all types that can be allocated in an nsPresArena, for
   preprocessing */

#ifdef STYLE_STRUCT
#error Sorry nsPresArenaObjectList.h needs to use STYLE_STRUCT!
#endif

#if !defined(PRES_ARENA_OBJECT)
#error Must define PRES_ARENA_OBJECT.
#endif

PRES_ARENA_OBJECT(nsLineBox)
PRES_ARENA_OBJECT(nsRuleNode)
PRES_ARENA_OBJECT(nsStyleContext)
PRES_ARENA_OBJECT(nsInheritedStyleData)
PRES_ARENA_OBJECT(nsResetStyleData)
PRES_ARENA_OBJECT(nsConditionalResetStyleData)
PRES_ARENA_OBJECT(nsConditionalResetStyleDataEntry)
PRES_ARENA_OBJECT(nsFrameList)
PRES_ARENA_OBJECT(CustomCounterStyle)
PRES_ARENA_OBJECT(DependentBuiltinCounterStyle)

#define STYLE_STRUCT(name_, checkdata_cb_) \
  PRES_ARENA_OBJECT(nsStyle##name_)
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
