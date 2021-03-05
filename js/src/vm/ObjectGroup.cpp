/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ObjectGroup.h"

#include "mozilla/Maybe.h"
#include "mozilla/Unused.h"

#include "jsexn.h"

#include "builtin/DataViewObject.h"
#include "gc/FreeOp.h"
#include "gc/HashUtil.h"
#include "gc/Policy.h"
#include "gc/StoreBuffer.h"
#include "js/CharacterEncoding.h"
#include "js/friend/WindowProxy.h"  // js::IsWindow
#include "js/UniquePtr.h"
#include "vm/ArrayObject.h"
#include "vm/ErrorObject.h"
#include "vm/GlobalObject.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/RegExpObject.h"
#include "vm/Shape.h"
#include "vm/TaggedProto.h"

#include "gc/Marking-inl.h"
#include "gc/ObjectKind-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

/////////////////////////////////////////////////////////////////////
// GlobalObject
/////////////////////////////////////////////////////////////////////

bool GlobalObject::shouldSplicePrototype() {
  // During bootstrapping, we need to make sure not to splice a new prototype in
  // for the global object if its __proto__ had previously been set to non-null,
  // as this will change the prototype for all other objects with the same type.
  return staticPrototype() == nullptr;
}

/* static */
bool GlobalObject::splicePrototype(JSContext* cx, Handle<GlobalObject*> global,
                                   Handle<TaggedProto> proto) {
  MOZ_ASSERT(cx->realm() == global->realm());

  // Windows may not appear on prototype chains.
  MOZ_ASSERT_IF(proto.isObject(), !IsWindow(proto.toObject()));

  if (proto.isObject()) {
    RootedObject protoObj(cx, proto.toObject());
    if (!JSObject::setDelegate(cx, protoObj)) {
      return false;
    }
  }

  return JSObject::setProtoUnchecked(cx, global, proto);
}

static bool AddPlainObjectProperties(JSContext* cx, HandlePlainObject obj,
                                     IdValuePair* properties,
                                     size_t nproperties) {
  RootedId propid(cx);
  RootedValue value(cx);

  for (size_t i = 0; i < nproperties; i++) {
    propid = properties[i].id;
    value = properties[i].value;
    if (!NativeDefineDataProperty(cx, obj, propid, value, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  return true;
}

PlainObject* js::NewPlainObjectWithProperties(JSContext* cx,
                                              IdValuePair* properties,
                                              size_t nproperties,
                                              NewObjectKind newKind) {
  gc::AllocKind allocKind = gc::GetGCObjectKind(nproperties);
  RootedPlainObject obj(
      cx, NewBuiltinClassInstance<PlainObject>(cx, allocKind, newKind));
  if (!obj || !AddPlainObjectProperties(cx, obj, properties, nproperties)) {
    return nullptr;
  }
  return obj;
}
