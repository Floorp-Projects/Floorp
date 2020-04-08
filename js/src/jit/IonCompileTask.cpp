/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonCompileTask.h"

#include "jit/CodeGenerator.h"
#include "jit/JitScript.h"
#include "jit/WarpSnapshot.h"
#include "vm/JSScript.h"

#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

void IonCompileTask::runTask() {
  // This is the entry point when ion compiles are run offthread.
  TraceLoggerThread* logger = TraceLoggerForCurrentThread();
  TraceLoggerEvent event(TraceLogger_AnnotateScripts, script());
  AutoTraceLog logScript(logger, event);
  AutoTraceLog logCompile(logger, TraceLogger_IonCompilation);

  jit::JitContext jctx(mirGen_.realm->runtime(), mirGen_.realm, &alloc());
  setBackgroundCodegen(jit::CompileBackEnd(&mirGen_, snapshot_));
}

void IonCompileTask::trace(JSTracer* trc) {
  if (!mirGen_.runtime->runtimeMatches(trc->runtime())) {
    return;
  }

  if (JitOptions.warpBuilder) {
    MOZ_ASSERT(snapshot_);
    MOZ_ASSERT(!rootList_);
    snapshot_->trace(trc);
  } else {
    MOZ_ASSERT(!snapshot_);
    MOZ_ASSERT(rootList_);
    rootList_->trace(trc);
  }
}

IonCompileTask::IonCompileTask(MIRGenerator& mirGen, bool scriptHasIonScript,
                               CompilerConstraintList* constraints,
                               WarpSnapshot* snapshot)
    : mirGen_(mirGen),
      constraints_(constraints),
      snapshot_(snapshot),
      scriptHasIonScript_(scriptHasIonScript) {}

size_t IonCompileTask::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  // See js::jit::FreeIonCompileTask.
  // The IonCompileTask and most of its contents live in the LifoAlloc we point
  // to.

  size_t result = alloc().lifoAlloc()->sizeOfIncludingThis(mallocSizeOf);

  if (backgroundCodegen_) {
    result += mallocSizeOf(backgroundCodegen_);
  }

  return result;
}

static inline bool TooManyUnlinkedTasks(JSRuntime* rt) {
  static const size_t MaxUnlinkedTasks = 100;
  return rt->jitRuntime()->ionLazyLinkListSize() > MaxUnlinkedTasks;
}

static void MoveFinishedTasksToLazyLinkList(
    JSRuntime* rt, const AutoLockHelperThreadState& lock) {
  // Incorporate any off thread compilations for the runtime which have
  // finished, failed or have been cancelled.

  GlobalHelperThreadState::IonCompileTaskVector& finished =
      HelperThreadState().ionFinishedList(lock);

  for (size_t i = 0; i < finished.length(); i++) {
    // Find a finished task for the runtime.
    IonCompileTask* task = finished[i];
    if (task->script()->runtimeFromAnyThread() != rt) {
      continue;
    }

    HelperThreadState().remove(finished, &i);
    rt->jitRuntime()->numFinishedOffThreadTasksRef(lock)--;

    JSScript* script = task->script();
    MOZ_ASSERT(script->hasBaselineScript());
    script->baselineScript()->setPendingIonCompileTask(rt, script, task);
    rt->jitRuntime()->ionLazyLinkListAdd(rt, task);
  }
}

static void EagerlyLinkExcessTasks(JSContext* cx,
                                   AutoLockHelperThreadState& lock) {
  JSRuntime* rt = cx->runtime();
  MOZ_ASSERT(TooManyUnlinkedTasks(rt));

  do {
    jit::IonCompileTask* task = rt->jitRuntime()->ionLazyLinkList(rt).getLast();
    RootedScript script(cx, task->script());

    AutoUnlockHelperThreadState unlock(lock);
    AutoRealm ar(cx, script);
    jit::LinkIonScript(cx, script);
  } while (TooManyUnlinkedTasks(rt));
}

void jit::AttachFinishedCompilations(JSContext* cx) {
  JSRuntime* rt = cx->runtime();
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

  if (!rt->jitRuntime() || !rt->jitRuntime()->numFinishedOffThreadTasks()) {
    return;
  }

  AutoLockHelperThreadState lock;

  while (true) {
    MoveFinishedTasksToLazyLinkList(rt, lock);

    if (!TooManyUnlinkedTasks(rt)) {
      break;
    }

    EagerlyLinkExcessTasks(cx, lock);

    // Linking releases the lock so we must now check the finished list
    // again.
  }

  MOZ_ASSERT(!rt->jitRuntime()->numFinishedOffThreadTasks());
}

void jit::FreeIonCompileTask(IonCompileTask* task) {
  // The task is allocated into its LifoAlloc, so destroying that will
  // destroy the task and all other data accumulated during compilation,
  // except any final codegen (which includes an assembler and needs to be
  // explicitly destroyed).
  js_delete(task->backgroundCodegen());
  js_delete(task->alloc().lifoAlloc());
}

void jit::FinishOffThreadTask(JSRuntime* runtime, IonCompileTask* task,
                              const AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(runtime);

  JSScript* script = task->script();

  // Clean the references to the pending IonCompileTask, if we just finished it.
  if (script->baselineScript()->hasPendingIonCompileTask() &&
      script->baselineScript()->pendingIonCompileTask() == task) {
    script->baselineScript()->removePendingIonCompileTask(runtime, script);
  }

  // If the task is still in one of the helper thread lists, then remove it.
  if (task->isInList()) {
    runtime->jitRuntime()->ionLazyLinkListRemove(runtime, task);
  }

  // Clear the recompiling flag of the old ionScript, since we continue to
  // use the old ionScript if recompiling fails.
  if (script->hasIonScript()) {
    script->ionScript()->clearRecompiling();
  }

  // Clean up if compilation did not succeed.
  if (script->isIonCompilingOffThread()) {
    script->jitScript()->clearIsIonCompilingOffThread(script);

    AbortReasonOr<Ok> status = task->mirGen().getOffThreadStatus();
    if (status.isErr() && status.unwrapErr() == AbortReason::Disable) {
      script->disableIon();
    }
  }

  // Free Ion LifoAlloc off-thread. Free on the main thread if this OOMs.
  if (!StartOffThreadIonFree(task, locked)) {
    FreeIonCompileTask(task);
  }
}

MOZ_MUST_USE bool jit::CreateMIRRootList(IonCompileTask& task) {
  MOZ_ASSERT(!task.mirGen().outerInfo().isAnalysis());

  TempAllocator& alloc = task.alloc();
  MIRGraph& graph = task.mirGen().graph();

  MRootList* roots = new (alloc.fallible()) MRootList(alloc);
  if (!roots) {
    return false;
  }

  JSScript* prevScript = nullptr;

  for (ReversePostorderIterator block(graph.rpoBegin());
       block != graph.rpoEnd(); block++) {
    JSScript* script = block->info().script();
    if (script != prevScript) {
      if (!roots->append(script)) {
        return false;
      }
      prevScript = script;
    }

    for (MInstructionIterator iter(block->begin()), end(block->end());
         iter != end; iter++) {
      if (!iter->appendRoots(*roots)) {
        return false;
      }
    }
  }

  task.setRootList(*roots);
  return true;
}
