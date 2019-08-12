/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_DebugAPI_inl_h
#define debugger_DebugAPI_inl_h

#include "debugger/DebugAPI.h"

#include "vm/Stack-inl.h"

namespace js {

/* static */
bool DebugAPI::stepModeEnabled(JSScript* script) {
  return script->hasDebugScript() && stepModeEnabledSlow(script);
}

/* static */
bool DebugAPI::hasBreakpointsAt(JSScript* script, jsbytecode* pc) {
  return script->hasDebugScript() && hasBreakpointsAtSlow(script, pc);
}

/* static */
bool DebugAPI::hasAnyBreakpointsOrStepMode(JSScript* script) {
  return script->hasDebugScript();
}

/* static */
void DebugAPI::onNewScript(JSContext* cx, HandleScript script) {
  // We early return in slowPathOnNewScript for self-hosted scripts, so we can
  // ignore those in our assertion here.
  MOZ_ASSERT_IF(!script->realm()->creationOptions().invisibleToDebugger() &&
                    !script->selfHosted(),
                script->realm()->firedOnNewGlobalObject);

  // The script may not be ready to be interrogated by the debugger.
  if (script->hideScriptFromDebugger()) {
    return;
  }

  if (script->realm()->isDebuggee()) {
    slowPathOnNewScript(cx, script);
  }
}

/* static */
void DebugAPI::onNewGlobalObject(JSContext* cx, Handle<GlobalObject*> global) {
  MOZ_ASSERT(!global->realm()->firedOnNewGlobalObject);
#ifdef DEBUG
  global->realm()->firedOnNewGlobalObject = true;
#endif
  if (!cx->runtime()->onNewGlobalObjectWatchers().isEmpty()) {
    slowPathOnNewGlobalObject(cx, global);
  }
}

/* static */
void DebugAPI::notifyParticipatesInGC(GlobalObject* global,
                                      uint64_t majorGCNumber) {
  GlobalObject::DebuggerVector* dbgs = global->getDebuggers();
  if (dbgs && !dbgs->empty()) {
    slowPathNotifyParticipatesInGC(majorGCNumber, *dbgs);
  }
}

/* static */
bool DebugAPI::onLogAllocationSite(JSContext* cx, JSObject* obj,
                                   HandleSavedFrame frame,
                                   mozilla::TimeStamp when) {
  GlobalObject::DebuggerVector* dbgs = cx->global()->getDebuggers();
  if (!dbgs || dbgs->empty()) {
    return true;
  }
  RootedObject hobj(cx, obj);
  return slowPathOnLogAllocationSite(cx, hobj, frame, when, *dbgs);
}

/* static */
bool DebugAPI::onLeaveFrame(JSContext* cx, AbstractFramePtr frame,
                            jsbytecode* pc, bool ok) {
  MOZ_ASSERT_IF(frame.isInterpreterFrame(),
                frame.asInterpreterFrame() == cx->interpreterFrame());
  MOZ_ASSERT_IF(frame.hasScript() && frame.script()->isDebuggee(),
                frame.isDebuggee());
  /* Traps must be cleared from eval frames, see slowPathOnLeaveFrame. */
  mozilla::DebugOnly<bool> evalTraps =
      frame.isEvalFrame() && frame.script()->hasDebugScript();
  MOZ_ASSERT_IF(evalTraps, frame.isDebuggee());
  if (frame.isDebuggee()) {
    ok = slowPathOnLeaveFrame(cx, frame, pc, ok);
  }
  MOZ_ASSERT(!inFrameMaps(frame));
  return ok;
}

/* static */
bool DebugAPI::onNewGenerator(JSContext* cx, AbstractFramePtr frame,
                              Handle<AbstractGeneratorObject*> genObj) {
  if (frame.isDebuggee()) {
    return slowPathOnNewGenerator(cx, frame, genObj);
  }
  return true;
}

/* static */
bool DebugAPI::checkNoExecute(JSContext* cx, HandleScript script) {
  if (!cx->realm()->isDebuggee() || !cx->noExecuteDebuggerTop) {
    return true;
  }
  return slowPathCheckNoExecute(cx, script);
}

/* static */
ResumeMode DebugAPI::onEnterFrame(JSContext* cx, AbstractFramePtr frame) {
  MOZ_ASSERT_IF(frame.hasScript() && frame.script()->isDebuggee(),
                frame.isDebuggee());
  if (!frame.isDebuggee()) {
    return ResumeMode::Continue;
  }
  return slowPathOnEnterFrame(cx, frame);
}

/* static */
ResumeMode DebugAPI::onResumeFrame(JSContext* cx, AbstractFramePtr frame) {
  MOZ_ASSERT_IF(frame.hasScript() && frame.script()->isDebuggee(),
                frame.isDebuggee());
  if (!frame.isDebuggee()) {
    return ResumeMode::Continue;
  }
  return slowPathOnResumeFrame(cx, frame);
}

/* static */
ResumeMode DebugAPI::onDebuggerStatement(JSContext* cx,
                                         AbstractFramePtr frame) {
  if (!cx->realm()->isDebuggee()) {
    return ResumeMode::Continue;
  }
  return slowPathOnDebuggerStatement(cx, frame);
}

/* static */
ResumeMode DebugAPI::onExceptionUnwind(JSContext* cx, AbstractFramePtr frame) {
  if (!cx->realm()->isDebuggee()) {
    return ResumeMode::Continue;
  }
  return slowPathOnExceptionUnwind(cx, frame);
}

/* static */
void DebugAPI::onNewWasmInstance(JSContext* cx,
                                 Handle<WasmInstanceObject*> wasmInstance) {
  if (cx->realm()->isDebuggee()) {
    slowPathOnNewWasmInstance(cx, wasmInstance);
  }
}

/* static */
void DebugAPI::onNewPromise(JSContext* cx, Handle<PromiseObject*> promise) {
  if (MOZ_UNLIKELY(cx->realm()->isDebuggee())) {
    slowPathOnNewPromise(cx, promise);
  }
}

/* static */
void DebugAPI::onPromiseSettled(JSContext* cx, Handle<PromiseObject*> promise) {
  if (MOZ_UNLIKELY(promise->realm()->isDebuggee())) {
    slowPathOnPromiseSettled(cx, promise);
  }
}

/* static */
void DebugAPI::sweepBreakpoints(JSFreeOp* fop, JSScript* script) {
  if (script->hasDebugScript()) {
    sweepBreakpointsSlow(fop, script);
  }
}

}  // namespace js

#endif /* debugger_DebugAPI_inl_h */
