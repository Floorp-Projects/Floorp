/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
  // If adding new types, please update nsPresArena::ClearArenaRefPtrWithoutDeregistering
  // as well
  static_assert(IsSame<T, GeckoStyleContext>::value || IsSame<T, nsStyleContext>::value,
                 "ArenaRefPtr<T> template parameter T must be declared in "
                 "nsPresArenaObjectList and explicitly handled in"
                 "nsPresArena.cpp");
}

} // namespace mozilla

template<typename T>
void
nsPresArena::RegisterArenaRefPtr(mozilla::ArenaRefPtr<T>* aPtr)
{
  MOZ_ASSERT(!mArenaRefPtrs.Contains(aPtr));
  mArenaRefPtrs.Put(aPtr, T::ArenaObjectID());
}


#endif
