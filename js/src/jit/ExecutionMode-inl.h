/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ExecutionMode_inl_h
#define jit_ExecutionMode_inl_h

#include "jit/CompileInfo.h"

#include "jsscriptinlines.h"

namespace js {
namespace jit {

static inline bool
HasIonScript(JSScript *script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->hasIonScript();
      default:;
    }
    MOZ_CRASH("No such execution mode");
}

static inline IonScript *
GetIonScript(JSScript *script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->maybeIonScript();
      default:;
    }
    MOZ_CRASH("No such execution mode");
}

static inline void
SetIonScript(JSContext *cx, JSScript *script, ExecutionMode cmode, IonScript *ionScript)
{
    switch (cmode) {
      case SequentialExecution: script->setIonScript(cx, ionScript); return;
      default:;
    }
    MOZ_CRASH("No such execution mode");
}

static inline size_t
OffsetOfIonInJSScript(ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return JSScript::offsetOfIonScript();
      default:;
    }
    MOZ_CRASH("No such execution mode");
}

static inline bool
CanIonCompile(JSScript *script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->canIonCompile();
      case DefinitePropertiesAnalysis: return true;
      case ArgumentsUsageAnalysis: return true;
      default:;
    }
    MOZ_CRASH("No such execution mode");
}

static inline bool
CompilingOffThread(JSScript *script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->isIonCompilingOffThread();
      default:;
    }
    MOZ_CRASH("No such execution mode");
}

static inline bool
CompilingOffThread(HandleScript script, ExecutionMode cmode)
{
    switch (cmode) {
      case SequentialExecution: return script->isIonCompilingOffThread();
      default:;
    }
    MOZ_CRASH("No such execution mode");
}

} // namespace jit
} // namespace js

#endif /* jit_ExecutionMode_inl_h */
