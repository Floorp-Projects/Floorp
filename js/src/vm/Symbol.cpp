/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Symbol.h"

#include "jscntxt.h"
#include "jscompartment.h"

#include "gc/Rooting.h"

#include "jscompartmentinlines.h"
#include "jsgcinlines.h"

using JS::Symbol;
using namespace js;

Symbol *
Symbol::new_(ExclusiveContext *cx, JSString *description)
{
    RootedAtom atom(cx);
    if (description) {
        atom = AtomizeString(cx, description);
        if (!atom)
            return nullptr;
    }

    // Lock to allocate. If symbol allocation becomes a bottleneck, this can
    // probably be replaced with an assertion that we're on the main thread.
    AutoLockForExclusiveAccess lock(cx);
    AutoCompartment ac(cx, cx->atomsCompartment());

    // Following AtomizeAndCopyChars, we grudgingly forgo last-ditch GC here.
    Symbol *p = gc::AllocateNonObject<Symbol, NoGC>(cx);
    if (!p) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }
    return new (p) Symbol(atom);
}
