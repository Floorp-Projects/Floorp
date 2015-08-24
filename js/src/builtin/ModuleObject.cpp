/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/ModuleObject.h"

#include "jsobjinlines.h"

using namespace js;

///////////////////////////////////////////////////////////////////////////
// ModuleObject

const Class ModuleObject::class_ = {
    "Module",
    JSCLASS_HAS_RESERVED_SLOTS(ModuleObject::SlotCount) |
    JSCLASS_IS_ANONYMOUS |
    JSCLASS_IMPLEMENTS_BARRIERS,
    nullptr,        /* addProperty */
    nullptr,        /* delProperty */
    nullptr,        /* getProperty */
    nullptr,        /* setProperty */
    nullptr,        /* enumerate   */
    nullptr,        /* resolve     */
    nullptr,        /* mayResolve  */
    nullptr,        /* convert     */
    nullptr,        /* finalize    */
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    ModuleObject::trace
};

/* static */ ModuleObject*
ModuleObject::create(ExclusiveContext* cx)
{
    return NewBuiltinClassInstance<ModuleObject>(cx, TenuredObject);
}

void
ModuleObject::init(HandleScript script)
{
    initReservedSlot(ScriptSlot, PrivateValue(script));
}

JSScript*
ModuleObject::script() const
{
    return static_cast<JSScript*>(getReservedSlot(ScriptSlot).toPrivate());
}

/* static */ void
ModuleObject::trace(JSTracer* trc, JSObject* obj)
{
    ModuleObject& module = obj->as<ModuleObject>();
    JSScript* script = module.script();
    TraceManuallyBarrieredEdge(trc, &script, "Module script");
    module.setReservedSlot(ScriptSlot, PrivateValue(script));
}
