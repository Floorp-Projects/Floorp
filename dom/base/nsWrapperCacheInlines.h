/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWrapperCacheInline_h___
#define nsWrapperCacheInline_h___

#include "nsWrapperCache.h"
#include "xpcpublic.h"

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
    participant->Trace(aThis, TraceCallbackFunc(SearchGray), &hasGrayObjects);
    return !hasGrayObjects;
  }
  return false;
}

inline void
nsWrapperCache::TraceWrapperJSObject(JSTracer* aTrc, const char* aName)
{
  JS_CallHeapObjectTracer(aTrc, &mWrapper, aName);
}

#endif /* nsWrapperCache_h___ */
