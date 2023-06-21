/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainTime.h"

#include "mozilla/Assertions.h"

#include "jspubtd.h"
#include "NamespaceImports.h"

#include "gc/AllocKind.h"
#include "js/Class.h"
#include "js/PropertySpec.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"
#include "vm/PlainObject.h"

using namespace js;
using namespace js::temporal;

static bool PlainTimeConstructor(JSContext* cx, unsigned argc, Value* vp) {
  MOZ_CRASH("NYI");
}

const JSClass PlainTimeObject::class_ = {
    "Temporal.PlainTime",
    JSCLASS_HAS_RESERVED_SLOTS(PlainTimeObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainTime),
    JS_NULL_CLASS_OPS,
    &PlainTimeObject::classSpec_,
};

const JSClass& PlainTimeObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainTime_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec PlainTime_prototype_methods[] = {
    JS_FS_END,
};

static const JSPropertySpec PlainTime_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec PlainTimeObject::classSpec_ = {
    GenericCreateConstructor<PlainTimeConstructor, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainTimeObject>,
    PlainTime_methods,
    nullptr,
    PlainTime_prototype_methods,
    PlainTime_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
