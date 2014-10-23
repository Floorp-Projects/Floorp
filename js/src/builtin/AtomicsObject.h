/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_AtomicsObject_h
#define builtin_AtomicsObject_h

#include "jsobj.h"

namespace js {

class AtomicsObject : public JSObject
{
  public:
    static const Class class_;
    static JSObject* initClass(JSContext *cx, Handle<GlobalObject *> global);
    static bool toString(JSContext *cx, unsigned int argc, jsval *vp);

    static const int FutexOK = 0;

    // The error values must be negative because APIs such as futexWaitOrRequeue
    // return a value that is either the number of tasks woken or an error code.
    static const int FutexNotequal = -1;
    static const int FutexTimedout = -2;

    // Internal signals; negative for the same reason.
    static const int FutexInterrupted = -1000;
};

void atomics_fullMemoryBarrier();

bool atomics_compareExchange(JSContext *cx, unsigned argc, Value *vp);
bool atomics_load(JSContext *cx, unsigned argc, Value *vp);
bool atomics_store(JSContext *cx, unsigned argc, Value *vp);
bool atomics_fence(JSContext *cx, unsigned argc, Value *vp);
bool atomics_add(JSContext *cx, unsigned argc, Value *vp);
bool atomics_sub(JSContext *cx, unsigned argc, Value *vp);
bool atomics_and(JSContext *cx, unsigned argc, Value *vp);
bool atomics_or(JSContext *cx, unsigned argc, Value *vp);
bool atomics_xor(JSContext *cx, unsigned argc, Value *vp);
bool atomics_futexWait(JSContext *cx, unsigned argc, Value *vp);
bool atomics_futexWake(JSContext *cx, unsigned argc, Value *vp);
bool atomics_futexWakeOrRequeue(JSContext *cx, unsigned argc, Value *vp);

}  /* namespace js */

JSObject *
js_InitAtomicsClass(JSContext *cx, js::HandleObject obj);

#endif /* builtin_AtomicsObject_h */
