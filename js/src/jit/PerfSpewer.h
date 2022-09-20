/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_PerfSpewer_h
#define jit_PerfSpewer_h

#ifdef JS_ION_PERF
#  include <stdio.h>
#endif
#include "jit/CacheIR.h"
#include "jit/JitCode.h"
#include "jit/Label.h"
#include "jit/LIR.h"
#include "js/AllocPolicy.h"
#include "js/Vector.h"
#include "vm/JSScript.h"

namespace {
struct AutoLockPerfSpewer;
}

namespace js {
namespace jit {

#ifdef JS_ION_PERF
void CheckPerf();
#else
inline void CheckPerf() {}
#endif

class MBasicBlock;
class MacroAssembler;

bool PerfEnabled();

class PerfSpewer {
 protected:
  struct OpcodeEntry {
    Label addr;
    unsigned opcode;
  };
  Vector<OpcodeEntry, 1, SystemAllocPolicy> opcodes_;

 public:
  PerfSpewer() = default;

  void saveJitCodeIRInfo(const char* filename, JitCode* code,
                         AutoLockPerfSpewer& lock);
  static void SaveJitCodeSourceInfo(const char* localfile, JSScript* script,
                                    JitCode* code, AutoLockPerfSpewer& lock);

  static void CollectJitCodeInfo(const char* desc, JSScript* script,
                                 JitCode* code, AutoLockPerfSpewer& lock);
  static void CollectJitCodeInfo(UniqueChars& function_name, JitCode* code,
                                 AutoLockPerfSpewer& lock);
  static void CollectJitCodeInfo(UniqueChars& function_name, void* code_addr,
                                 uint64_t code_size, AutoLockPerfSpewer& lock);
};

void CollectPerfSpewerJitCodeProfile(JitCode* code, const char* msg);

void CollectPerfSpewerWasmMap(uintptr_t base, uintptr_t size,
                              const char* filename, const char* annotation);
void CollectPerfSpewerWasmFunctionMap(uintptr_t base, uintptr_t size,
                                      const char* filename, unsigned lineno,
                                      const char* funcName);

class IonPerfSpewer : public PerfSpewer {
  static UniqueChars lirFilename;

 public:
  void recordInstruction(MacroAssembler& masm, LNode::Opcode op);
  void saveProfile(JSScript* script, JitCode* code);
};

class BaselinePerfSpewer : public PerfSpewer {
  static UniqueChars jsopFilename;

 public:
  void recordInstruction(MacroAssembler& masm, JSOp op);
  void saveProfile(JSScript* script, JitCode* code);
};

class InlineCachePerfSpewer : public PerfSpewer {
  static UniqueChars cacheopFilename;

 public:
  void recordInstruction(MacroAssembler& masm, CacheOp op);
  void saveProfile(JitCode* code, const char* name);
};

}  // namespace jit
}  // namespace js

#endif /* jit_PerfSpewer_h */
