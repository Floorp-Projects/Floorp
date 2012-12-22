/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWrapperCacheInline_h___
#define nsWrapperCacheInline_h___

#include "nsWrapperCache.h"
#include "xpcpublic.h"

// We want to encode 3 bits into mWrapperPtrBits, so anything we store in it
// needs to be aligned on 8 byte boundaries.
// JS arenas are aligned on 4k boundaries and padded so that the array of
// JSObjects ends on the end of the arena. If the size of a JSObject is a
// multiple of 8 then the start of every JSObject in an arena should be aligned
// on 8 byte boundaries.
MOZ_STATIC_ASSERT(sizeof(js::shadow::Object) % 8 == 0 && sizeof(JS::Value) == 8,
                  "We want to rely on JSObject being aligned on 8 byte "
                  "boundaries.");

inline JSObject*
nsWrapperCache::GetWrapper() const
{
    JSObject* obj = GetWrapperPreserveColor();
    xpc_UnmarkGrayObject(obj);
    return obj;
}

inline bool
nsWrapperCache::IsBlack()
{
  JSObject* o = GetWrapperPreserveColor();
  return o && !xpc_IsGrayGCThing(o);
}

static void
SearchGray(void* aGCThing, const char* aName, void* aClosure)
{
  bool* hasGrayObjects = static_cast<bool*>(aClosure);
  if (!*hasGrayObjects && aGCThing && xpc_IsGrayGCThing(aGCThing)) {
    *hasGrayObjects = true;
  }
}

inline bool
nsWrapperCache::IsBlackAndDoesNotNeedTracing(nsISupports* aThis)
{
  if (IsBlack()) {
    nsXPCOMCycleCollectionParticipant* participant = nullptr;
    CallQueryInterface(aThis, &participant);
    bool hasGrayObjects = false;
    participant->Trace(aThis, SearchGray, &hasGrayObjects);
    return !hasGrayObjects;
  }
  return false;
}

#endif /* nsWrapperCache_h___ */
