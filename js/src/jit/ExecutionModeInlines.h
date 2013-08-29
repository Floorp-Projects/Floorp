/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ExecutionModeInlines_h
#define jit_ExecutionModeInlines_h

#ifdef JS_ION

namespace js {
namespace jit {

static inline bool
HasIonScript(JSScript *script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->hasIonScript();
      case ParallelExecution: return script->hasParallelIonScript();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline IonScript *
GetIonScript(JSScript *script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->maybeIonScript();
      case ParallelExecution: return script->maybeParallelIonScript();
    }
    JS_NOT_REACHED("No such execution mode");
    return NULL;
}

static inline void
SetIonScript(JSScript *script, ExecutionMode cmode, IonScript *ionScript)
{
    switch (cmode) {
      case SequentialExecution: script->setIonScript(ionScript); return;
      case ParallelExecution: script->setParallelIonScript(ionScript); return;
    }
    JS_NOT_REACHED("No such execution mode");
}

static inline size_t
OffsetOfIonInJSScript(ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return JSScript::offsetOfIonScript();
      case ParallelExecution: return JSScript::offsetOfParallelIonScript();
    }
    JS_NOT_REACHED("No such execution mode");
}

static inline bool
CanIonCompile(JSScript *script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->canIonCompile();
      case ParallelExecution: return script->canParallelIonCompile();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline bool
CanIonCompile(JSFunction *fun, ExecutionMode cmode)
{
    return fun->isInterpreted() && CanIonCompile(fun->nonLazyScript(), cmode);
}

static inline bool
CompilingOffThread(JSScript *script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->isIonCompilingOffThread();
      case ParallelExecution: return script->isParallelIonCompilingOffThread();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline bool
CompilingOffThread(HandleScript script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->isIonCompilingOffThread();
      case ParallelExecution: return script->isParallelIonCompilingOffThread();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline types::CompilerOutput::Kind
CompilerOutputKind(ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return types::CompilerOutput::Ion;
      case ParallelExecution: return types::CompilerOutput::ParallelIon;
    }
    JS_NOT_REACHED("No such execution mode");
    return types::CompilerOutput::Ion;
}

} // namespace jit
} // namespace js

#endif  // JS_ION

#endif /* jit_ExecutionModeInlines_h */
