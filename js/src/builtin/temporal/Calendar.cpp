/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Calendar.h"

#include "mozilla/Assertions.h"

#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "gc/AllocKind.h"
#include "js/Class.h"
#include "js/PropertySpec.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"
#include "vm/PlainObject.h"

using namespace js;
using namespace js::temporal;

static bool CalendarConstructor(JSContext* cx, unsigned argc, Value* vp) {
  MOZ_CRASH("NYI");
}

const JSClass CalendarObject::class_ = {
    "Temporal.Calendar",
    JSCLASS_HAS_RESERVED_SLOTS(CalendarObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Calendar),
    JS_NULL_CLASS_OPS,
    &CalendarObject::classSpec_,
};

const JSClass& CalendarObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec Calendar_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec Calendar_prototype_methods[] = {
    JS_FS_END,
};

static const JSPropertySpec Calendar_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec CalendarObject::classSpec_ = {
    GenericCreateConstructor<CalendarConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<CalendarObject>,
    Calendar_methods,
    nullptr,
    Calendar_prototype_methods,
    Calendar_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
