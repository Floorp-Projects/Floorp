/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectGroup_inl_h
#define vm_ObjectGroup_inl_h

#include "vm/ObjectGroup.h"

#include "gc/Zone.h"

namespace js {

inline bool ObjectGroup::needsSweep() {
  // Note: this can be called off thread during compacting GCs, in which case
  // nothing will be running on the main thread.
  MOZ_ASSERT(!TlsContext.get()->inUnsafeCallWithABI);
  return generation() != zoneFromAnyThread()->types.generation;
}

/* static */ inline ObjectGroup* ObjectGroup::lazySingletonGroup(
    JSContext* cx, ObjectGroup* oldGroup, const JSClass* clasp,
    TaggedProto proto) {
  ObjectGroupRealm& realm = oldGroup ? ObjectGroupRealm::get(oldGroup)
                                     : ObjectGroupRealm::getForNewObject(cx);
  JS::Realm* objectRealm = oldGroup ? oldGroup->realm() : cx->realm();
  return lazySingletonGroup(cx, realm, objectRealm, clasp, proto);
}

}  // namespace js

/* static */ inline bool JSObject::setSingleton(JSContext* cx,
                                                js::HandleObject obj) {
  MOZ_ASSERT(!IsInsideNursery(obj));
  MOZ_ASSERT(!obj->isSingleton());

  js::ObjectGroup* group = js::ObjectGroup::lazySingletonGroup(
      cx, obj->groupRaw(), obj->getClass(), obj->taggedProto());
  if (!group) {
    return false;
  }

  obj->setGroupRaw(group);
  return true;
}

#endif /* vm_ObjectGroup_inl_h */
