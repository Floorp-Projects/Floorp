/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* inline methods that belong in ArenaRefPtr.h, except that they require
   the inclusion of headers for all types that ArenaRefPtr can handle */

#ifndef mozilla_ArenaRefPtrInlines_h
#define mozilla_ArenaRefPtrInlines_h

#include "mozilla/ArenaObjectID.h"
#include "mozilla/Assertions.h"
#include "nsStyleStruct.h"
#include "nsStyleContext.h"

namespace mozilla {

template<typename T>
void
ArenaRefPtr<T>::AssertValidType()
{
#ifdef DEBUG
  bool ok =
#define PRES_ARENA_OBJECT_WITH_ARENAREFPTR_SUPPORT(name_) \
    T::ArenaObjectID() == eArenaObjectID_##name_ ||
#include "nsPresArenaObjectList.h"
#undef PRES_ARENA_OBJECT_WITH_ARENAREFPTR_SUPPORT
    false;
  MOZ_ASSERT(ok, "ArenaRefPtr<T> template parameter T must be declared in "
                 "nsPresArenaObjectList with "
                 "PRES_ARENA_OBJECT_WITH_ARENAREFPTR_SUPPORT");
#endif
}

template<>
struct ArenaRefPtrTraits<nsStyleContext>
{
  static bool UsesArena(nsStyleContext* aPtr) {
    MOZ_ASSERT(aPtr);
    return aPtr->IsGecko();
  }
};

} // namespace mozilla

template<typename T>
void
nsPresArena::RegisterArenaRefPtr(mozilla::ArenaRefPtr<T>* aPtr)
{
  MOZ_ASSERT(!mArenaRefPtrs.Contains(aPtr));
  mArenaRefPtrs.Put(aPtr, T::ArenaObjectID());
}


#endif
