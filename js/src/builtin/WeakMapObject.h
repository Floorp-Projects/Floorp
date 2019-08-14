/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_WeakMapObject_h
#define builtin_WeakMapObject_h

#include "gc/WeakMap.h"
#include "vm/NativeObject.h"

namespace js {

class GlobalObject;

// Abstract base class for WeakMapObject and WeakSetObject.
class WeakCollectionObject : public NativeObject {
 public:
  ObjectValueMap* getMap() {
    return static_cast<ObjectValueMap*>(getPrivate());
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
    ObjectValueMap* map = getMap();
    return map ? map->sizeOfIncludingThis(aMallocSizeOf) : 0;
  }

  static MOZ_MUST_USE bool nondeterministicGetKeys(
      JSContext* cx, Handle<WeakCollectionObject*> obj,
      MutableHandleObject ret);

 protected:
  static const JSClassOps classOps_;
};

class WeakMapObject : public WeakCollectionObject {
 public:
  static const Class class_;
  static const Class protoClass_;

 private:
  static const ClassSpec classSpec_;

  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];

  static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

  static MOZ_MUST_USE MOZ_ALWAYS_INLINE bool is(HandleValue v);

  static MOZ_MUST_USE MOZ_ALWAYS_INLINE bool has_impl(JSContext* cx,
                                                      const CallArgs& args);
  static MOZ_MUST_USE bool has(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE MOZ_ALWAYS_INLINE bool get_impl(JSContext* cx,
                                                      const CallArgs& args);
  static MOZ_MUST_USE bool get(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE MOZ_ALWAYS_INLINE bool delete_impl(JSContext* cx,
                                                         const CallArgs& args);
  static MOZ_MUST_USE bool delete_(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE MOZ_ALWAYS_INLINE bool set_impl(JSContext* cx,
                                                      const CallArgs& args);
  static MOZ_MUST_USE bool set(JSContext* cx, unsigned argc, Value* vp);
};

}  // namespace js

#endif /* builtin_WeakMapObject_h */
