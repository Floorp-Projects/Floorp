/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWrapperCacheInline_h___
#define nsWrapperCacheInline_h___

#include "nsWrapperCache.h"
#include "js/GCAPI.h"
#include "js/TracingAPI.h"

inline JSObject*
nsWrapperCache::GetWrapper() const
{
    JSObject* obj = GetWrapperPreserveColor();
    if (obj) {
      JS::ExposeObjectToActiveJS(obj);
    }
    return obj;
}

inline bool
nsWrapperCache::IsBlack()
{
  JSObject* o = GetWrapperPreserveColor();
  return o && !JS::ObjectIsMarkedGray(o);
}

static void
SearchGray(JS::GCCellPtr aGCThing, const char* aName, void* aClosure)
{
  bool* hasGrayObjects = static_cast<bool*>(aClosure);
  if (!*hasGrayObjects && aGCThing && JS::GCThingIsMarkedGray(aGCThing)) {
    *hasGrayObjects = true;
  }
}

inline bool
nsWrapperCache::HasNothingToTrace(nsISupports* aThis)
{
  nsXPCOMCycleCollectionParticipant* participant = nullptr;
  CallQueryInterface(aThis, &participant);
  bool hasGrayObjects = false;
  participant->Trace(aThis, TraceCallbackFunc(SearchGray), &hasGrayObjects);
  return !hasGrayObjects;
}

inline bool
nsWrapperCache::IsBlackAndDoesNotNeedTracing(nsISupports* aThis)
{
  return IsBlack() && HasNothingToTrace(aThis);
}

inline void
nsWrapperCache::TraceWrapperJSObject(JSTracer* aTrc, const char* aName)
{
  JS_CallObjectTracer(aTrc, &mWrapper, aName);
}

#endif /* nsWrapperCache_h___ */
