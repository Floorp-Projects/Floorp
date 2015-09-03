/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_WeakMapObject_h
#define vm_WeakMapObject_h

#include "jsobj.h"
#include "jsweakmap.h"

namespace js {

class ObjectValueMap : public WeakMap<PreBarrieredObject, RelocatableValue>
{
  public:
    ObjectValueMap(JSContext* cx, JSObject* obj)
      : WeakMap<PreBarrieredObject, RelocatableValue>(cx, obj) {}

    virtual bool findZoneEdges();
};

class WeakMapObject : public NativeObject
{
  public:
    static const Class class_;

    ObjectValueMap* getMap() { return static_cast<ObjectValueMap*>(getPrivate()); }
};

// Generic weak map for mapping objects to other objects.
class ObjectWeakMap
{
  private:
    ObjectValueMap map;
    typedef gc::HashKeyRef<ObjectValueMap, JSObject*> StoreBufferRef;

  public:
    explicit ObjectWeakMap(JSContext* cx);
    bool init();
    ~ObjectWeakMap();

    JSObject* lookup(const JSObject* obj);
    bool add(JSContext* cx, JSObject* obj, JSObject* target);
    void clear();

    void trace(JSTracer* trc);
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
        return mallocSizeOf(this) + sizeOfExcludingThis(mallocSizeOf);
    }

#ifdef JSGC_HASH_TABLE_CHECKS
    void checkAfterMovingGC();
#endif
};

} // namespace js

#endif /* vm_WeakMapObject_h */
