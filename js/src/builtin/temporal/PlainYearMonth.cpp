/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainYearMonth.h"

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

static bool PlainYearMonthConstructor(JSContext* cx, unsigned argc, Value* vp) {
  MOZ_CRASH("NYI");
}

const JSClass PlainYearMonthObject::class_ = {
    "Temporal.PlainYearMonth",
    JSCLASS_HAS_RESERVED_SLOTS(PlainYearMonthObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainYearMonth),
    JS_NULL_CLASS_OPS,
    &PlainYearMonthObject::classSpec_,
};

const JSClass& PlainYearMonthObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainYearMonth_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec PlainYearMonth_prototype_methods[] = {
    JS_FS_END,
};

static const JSPropertySpec PlainYearMonth_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec PlainYearMonthObject::classSpec_ = {
    GenericCreateConstructor<PlainYearMonthConstructor, 2,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainYearMonthObject>,
    PlainYearMonth_methods,
    nullptr,
    PlainYearMonth_prototype_methods,
    PlainYearMonth_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
