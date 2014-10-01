/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Debugger_inl_h
#define vm_Debugger_inl_h

#include "vm/Debugger.h"

#include "vm/Stack-inl.h"

inline bool
js::Debugger::onLeaveFrame(JSContext *cx, AbstractFramePtr frame, bool ok)
{
    /* Traps must be cleared from eval frames, see slowPathOnLeaveFrame. */
    bool evalTraps = frame.isEvalFrame() &&
                     frame.script()->hasAnyBreakpointsOrStepMode();
    if (cx->compartment()->debugMode() || evalTraps)
        ok = slowPathOnLeaveFrame(cx, frame, ok);
    return ok;
}

/* static */ inline js::Debugger *
js::Debugger::fromJSObject(JSObject *obj)
{
    MOZ_ASSERT(js::GetObjectClass(obj) == &jsclass);
    return (Debugger *) obj->getPrivate();
}

#endif /* vm_Debugger_inl_h */
