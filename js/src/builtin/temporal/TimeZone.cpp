/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/TimeZone.h"

#include "mozilla/Assertions.h"

#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "gc/AllocKind.h"
#include "gc/GCContext.h"
#include "js/Class.h"
#include "js/PropertySpec.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"
#include "vm/PlainObject.h"

using namespace js;
using namespace js::temporal;

static bool TimeZoneConstructor(JSContext* cx, unsigned argc, Value* vp) {
  MOZ_CRASH("NYI");
}

void js::temporal::TimeZoneObject::finalize(JS::GCContext* gcx, JSObject* obj) {
  MOZ_ASSERT(gcx->onMainThread());
}

const JSClassOps TimeZoneObject::classOps_ = {
    nullptr,                   // addProperty
    nullptr,                   // delProperty
    nullptr,                   // enumerate
    nullptr,                   // newEnumerate
    nullptr,                   // resolve
    nullptr,                   // mayResolve
    TimeZoneObject::finalize,  // finalize
    nullptr,                   // call
    nullptr,                   // construct
    nullptr,                   // trace
};

const JSClass TimeZoneObject::class_ = {
    "Temporal.TimeZone",
    JSCLASS_HAS_RESERVED_SLOTS(TimeZoneObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_TimeZone) |
        JSCLASS_FOREGROUND_FINALIZE,
    &TimeZoneObject::classOps_,
    &TimeZoneObject::classSpec_,
};

const JSClass& TimeZoneObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec TimeZone_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec TimeZone_prototype_methods[] = {
    JS_FS_END,
};

static const JSPropertySpec TimeZone_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec TimeZoneObject::classSpec_ = {
    GenericCreateConstructor<TimeZoneConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<TimeZoneObject>,
    TimeZone_methods,
    nullptr,
    TimeZone_prototype_methods,
    TimeZone_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
