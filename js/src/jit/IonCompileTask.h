/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonCompileTask_h
#define jit_IonCompileTask_h

#include "mozilla/LinkedList.h"

#include "jit/MIRGenerator.h"

#include "js/Utility.h"

namespace js {

class CompilerConstraintList;

namespace jit {

class CodeGenerator;
class MRootList;

// IonCompileTask represents a single off-thread Ion compilation task.
class IonCompileTask final : public RunnableTask,
                             public mozilla::LinkedListElement<IonCompileTask> {
  MIRGenerator& mirGen_;

  // If off thread compilation is successful, the final code generator is
  // attached here. Code has been generated, but not linked (there is not yet
  // an IonScript). This is heap allocated, and must be explicitly destroyed,
  // performed by FinishOffThreadTask().
  CodeGenerator* backgroundCodegen_ = nullptr;

  CompilerConstraintList* constraints_ = nullptr;
  MRootList* rootList_ = nullptr;
  WarpSnapshot* snapshot_ = nullptr;

  // script->hasIonScript() at the start of the compilation. Used to avoid
  // calling hasIonScript() from background compilation threads.
  bool scriptHasIonScript_;

 public:
  explicit IonCompileTask(MIRGenerator& mirGen, bool scriptHasIonScript,
                          CompilerConstraintList* constraints,
                          WarpSnapshot* snapshot);

  JSScript* script() { return mirGen_.outerInfo().script(); }
  MIRGenerator& mirGen() { return mirGen_; }
  TempAllocator& alloc() { return mirGen_.alloc(); }
  bool scriptHasIonScript() const { return scriptHasIonScript_; }
  CompilerConstraintList* constraints() { return constraints_; }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
  void trace(JSTracer* trc);

  void setRootList(MRootList& rootList) {
    MOZ_ASSERT(!rootList_);
    rootList_ = &rootList;
  }
  CodeGenerator* backgroundCodegen() const { return backgroundCodegen_; }
  void setBackgroundCodegen(CodeGenerator* codegen) {
    backgroundCodegen_ = codegen;
  }

  void runTaskLocked(AutoLockHelperThreadState& locked);

  ThreadType threadType() override { return THREAD_TYPE_ION; }
  void runTask() override;
};

void AttachFinishedCompilations(JSContext* cx);
void FinishOffThreadTask(JSRuntime* runtime, IonCompileTask* task,
                         const AutoLockHelperThreadState& lock);
void FreeIonCompileTask(IonCompileTask* task);

MOZ_MUST_USE bool CreateMIRRootList(IonCompileTask& task);

}  // namespace jit
}  // namespace js

#endif /* jit_IonCompileTask_h */
