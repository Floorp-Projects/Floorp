/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Debugger_inl_h
#define vm_Debugger_inl_h

#include "vm/Debugger.h"

#include "builtin/Promise.h"
#include "vm/GeneratorObject.h"

#include "gc/WeakMap-inl.h"
#include "vm/Stack-inl.h"

/* static */ inline bool js::Debugger::onLeaveFrame(JSContext* cx,
                                                    AbstractFramePtr frame,
                                                    jsbytecode* pc, bool ok) {
  MOZ_ASSERT_IF(frame.isInterpreterFrame(),
                frame.asInterpreterFrame() == cx->interpreterFrame());
  MOZ_ASSERT_IF(frame.hasScript() && frame.script()->isDebuggee(),
                frame.isDebuggee());
  /* Traps must be cleared from eval frames, see slowPathOnLeaveFrame. */
  mozilla::DebugOnly<bool> evalTraps =
      frame.isEvalFrame() && frame.script()->hasAnyBreakpointsOrStepMode();
  MOZ_ASSERT_IF(evalTraps, frame.isDebuggee());
  if (frame.isDebuggee()) {
    ok = slowPathOnLeaveFrame(cx, frame, pc, ok);
  }
  MOZ_ASSERT(!inFrameMaps(frame));
  return ok;
}

/* static */ inline bool js::Debugger::onNewGenerator(
    JSContext* cx, AbstractFramePtr frame,
    Handle<AbstractGeneratorObject*> genObj) {
  if (frame.isDebuggee()) {
    return slowPathOnNewGenerator(cx, frame, genObj);
  }
  return true;
}

/* static */ inline js::Debugger* js::Debugger::fromJSObject(
    const JSObject* obj) {
  MOZ_ASSERT(obj->getClass() == &class_);
  return (Debugger*)obj->as<NativeObject>().getPrivate();
}

/* static */ inline bool js::Debugger::checkNoExecute(JSContext* cx,
                                                      HandleScript script) {
  if (!cx->realm()->isDebuggee() || !cx->noExecuteDebuggerTop) {
    return true;
  }
  return slowPathCheckNoExecute(cx, script);
}

/* static */ inline js::ResumeMode js::Debugger::onEnterFrame(JSContext* cx,
                                                       AbstractFramePtr frame) {
  MOZ_ASSERT_IF(frame.hasScript() && frame.script()->isDebuggee(),
                frame.isDebuggee());
  if (!frame.isDebuggee()) {
    return ResumeMode::Continue;
  }
  return slowPathOnEnterFrame(cx, frame);
}

/* static */ inline js::ResumeMode js::Debugger::onResumeFrame(
    JSContext* cx, AbstractFramePtr frame) {
  MOZ_ASSERT_IF(frame.hasScript() && frame.script()->isDebuggee(),
                frame.isDebuggee());
  if (!frame.isDebuggee()) {
    return ResumeMode::Continue;
  }
  return slowPathOnResumeFrame(cx, frame);
}

/* static */ inline js::ResumeMode js::Debugger::onDebuggerStatement(
    JSContext* cx, AbstractFramePtr frame) {
  if (!cx->realm()->isDebuggee()) {
    return ResumeMode::Continue;
  }
  return slowPathOnDebuggerStatement(cx, frame);
}

/* static */ inline js::ResumeMode js::Debugger::onExceptionUnwind(
    JSContext* cx, AbstractFramePtr frame) {
  if (!cx->realm()->isDebuggee()) {
    return ResumeMode::Continue;
  }
  return slowPathOnExceptionUnwind(cx, frame);
}

/* static */ inline void js::Debugger::onNewWasmInstance(
    JSContext* cx, Handle<WasmInstanceObject*> wasmInstance) {
  if (cx->realm()->isDebuggee()) {
    slowPathOnNewWasmInstance(cx, wasmInstance);
  }
}

/* static */ inline void js::Debugger::onNewPromise(JSContext* cx,
                                             Handle<PromiseObject*> promise) {
  if (MOZ_UNLIKELY(cx->realm()->isDebuggee())) {
    slowPathPromiseHook(cx, Debugger::OnNewPromise, promise);
  }
}

/* static */ inline void js::Debugger::onPromiseSettled(
    JSContext* cx, Handle<PromiseObject*> promise) {
  if (MOZ_UNLIKELY(promise->realm()->isDebuggee())) {
    slowPathPromiseHook(cx, Debugger::OnPromiseSettled, promise);
  }
}

inline js::Debugger* js::DebuggerEnvironment::owner() const {
  JSObject* dbgobj = &getReservedSlot(OWNER_SLOT).toObject();
  return Debugger::fromJSObject(dbgobj);
}

inline js::Debugger* js::DebuggerObject::owner() const {
  JSObject* dbgobj = &getReservedSlot(OWNER_SLOT).toObject();
  return Debugger::fromJSObject(dbgobj);
}

inline js::PromiseObject* js::DebuggerObject::promise() const {
  MOZ_ASSERT(isPromise());

  JSObject* referent = this->referent();
  if (IsCrossCompartmentWrapper(referent)) {
    // We know we have a Promise here, so CheckedUnwrapStatic is fine.
    referent = CheckedUnwrapStatic(referent);
    MOZ_ASSERT(referent);
  }

  return &referent->as<PromiseObject>();
}

#endif /* vm_Debugger_inl_h */
