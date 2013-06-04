/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsobjinlines.h"
#include "builtin/Module.h"

using namespace js;

Class js::ModuleClass = {
    "Module",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,        /* addProperty */
    JS_DeletePropertyStub,  /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

inline void
Module::setAtom(JSAtom *atom)
{
    setReservedSlot(ATOM_SLOT, StringValue(atom));
}

inline void
Module::setScript(JSScript *script)
{
    setReservedSlot(SCRIPT_SLOT, PrivateValue(script));
}

Module *
Module::create(JSContext *cx, HandleAtom atom)
{
    RootedObject object(cx, NewBuiltinClassInstance(cx, &ModuleClass));
    if (!object)
        return NULL;
    RootedModule module(cx, &object->asModule());
    module->setAtom(atom);
    module->setScript(NULL);
    return module;
}
