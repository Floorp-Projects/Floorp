/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/TemporalNow.h"

#include "mozilla/Assertions.h"

#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "js/Class.h"
#include "js/PropertySpec.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"

#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::temporal;

const JSClass TemporalNowObject::class_ = {
    "Temporal.Now",
    JSCLASS_HAS_CACHED_PROTO(JSProto_TemporalNow),
    JS_NULL_CLASS_OPS,
    &TemporalNowObject::classSpec_,
};

static const JSFunctionSpec TemporalNow_methods[] = {
    JS_FS_END,
};

static const JSPropertySpec TemporalNow_properties[] = {
    JS_PS_END,
};

static JSObject* CreateTemporalNowObject(JSContext* cx, JSProtoKey key) {
  RootedObject proto(cx, &cx->global()->getObjectPrototype());
  return NewTenuredObjectWithGivenProto(cx, &TemporalNowObject::class_, proto);
}

const ClassSpec TemporalNowObject::classSpec_ = {
    CreateTemporalNowObject,
    nullptr,
    TemporalNow_methods,
    TemporalNow_properties,
    nullptr,
    nullptr,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
