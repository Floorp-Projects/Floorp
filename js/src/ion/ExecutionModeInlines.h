/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_compilemode_h__
#define jsion_compilemode_h__

namespace js {
namespace ion {

static inline bool HasIonScript(RawScript script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->hasIonScript();
      case ParallelExecution: return script->hasParallelIonScript();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline IonScript *GetIonScript(RawScript script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->ion;
      case ParallelExecution: return script->parallelIon;
    }
    JS_NOT_REACHED("No such execution mode");
    return NULL;
}

static inline void SetIonScript(RawScript script, ExecutionMode cmode, IonScript *ionScript)
{
    switch (cmode) {
      case SequentialExecution: script->ion = ionScript; return;
      case ParallelExecution: script->parallelIon = ionScript; return;
    }
    JS_NOT_REACHED("No such execution mode");
}

static inline size_t OffsetOfIonInJSScript(ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return offsetof(JSScript, ion);
      case ParallelExecution: return offsetof(JSScript, parallelIon);
    }
    JS_NOT_REACHED("No such execution mode");
}

static inline bool CanIonCompile(RawScript script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->canIonCompile();
      case ParallelExecution: return script->canParallelIonCompile();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline bool CanIonCompile(RawFunction fun, ExecutionMode cmode)
{
    return fun->isInterpreted() && CanIonCompile(fun->nonLazyScript(), cmode);
}

static inline bool CompilingOffThread(RawScript script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->isIonCompilingOffThread();
      case ParallelExecution: return script->isParallelIonCompilingOffThread();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline bool CompilingOffThread(HandleScript script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->isIonCompilingOffThread();
      case ParallelExecution: return script->isParallelIonCompilingOffThread();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline bool Disabled(RawScript script, ExecutionMode cmode) {
    switch (cmode) {
      case SequentialExecution: return script->isIonCompilingOffThread();
      case ParallelExecution: return script->isParallelIonCompilingOffThread();
    }
    JS_NOT_REACHED("No such execution mode");
    return false;
}

static inline types::CompilerOutput::Kind CompilerOutputKind(ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return types::CompilerOutput::Ion;
      case ParallelExecution: return types::CompilerOutput::ParallelIon;
    }
    JS_NOT_REACHED("No such execution mode");
    return types::CompilerOutput::Ion;
}

}
}

#endif
