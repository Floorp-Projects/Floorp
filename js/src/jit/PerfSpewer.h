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
struct AutoFileLock;
}

namespace js {
namespace jit {

class MBasicBlock;
class MacroAssembler;

#ifdef JS_ION_PERF
void CheckPerf();
bool PerfIREnabled();
bool PerfSrcEnabled();
bool PerfFuncEnabled();
static inline bool PerfEnabled() {
  return PerfSrcEnabled() || PerfIREnabled() || PerfFuncEnabled();
}
#else
static inline void CheckPerf() {}
static inline bool PerfSrcEnabled() { return false; }
static inline bool PerfIREnabled() { return false; }
static inline bool PerfFuncEnabled() { return false; }
static inline bool PerfEnabled() { return false; }
#endif

#ifdef JS_ION_PERF

class PerfSpewer {
 protected:
  struct OpcodeEntry {
    Label addr;
    unsigned lineno;
  };
  Vector<OpcodeEntry, 1, SystemAllocPolicy> opcodes_;

 public:
  void writeJitDumpIRInfo(const char* filename, JitCode* code,
                          AutoFileLock& lock);
  static void WriteJitDumpEntry(const char* desc, JSScript* script,
                                JitCode* code, AutoFileLock& lock);
  static void WriteJitDumpLoadRecord(char* function_name, JitCode* code,
                                     AutoFileLock& lock);
  static void WriteJitDumpLoadRecord(char* function_name, void* code_addr,
                                     uint64_t code_size, AutoFileLock& lock);
  static void WriteJitDumpSourceInfo(const char* localfile, JSScript* script,
                                     JitCode* code, AutoFileLock& lock);
  static void WriteJitDumpDebugEntry(uint64_t addr, const char* filename,
                                     uint32_t lineno, uint32_t colno,
                                     AutoFileLock& lock);
};

void writePerfSpewerJitCodeProfile(JitCode* code, const char* msg);

void writePerfSpewerWasmMap(uintptr_t base, uintptr_t size,
                            const char* filename, const char* annotation);
void writePerfSpewerWasmFunctionMap(uintptr_t base, uintptr_t size,
                                    const char* filename, unsigned lineno,
                                    const char* funcName);

class IonPerfSpewer : public PerfSpewer {
  static UniqueChars lirFilename;

 public:
  void recordInstruction(MacroAssembler& masm, LNode::Opcode op);
  void writeProfile(JSScript* script, JitCode* code);
};

class BaselinePerfSpewer : public PerfSpewer {
  static UniqueChars jsopFilename;

 public:
  void recordInstruction(MacroAssembler& masm, JSOp op);
  void writeProfile(JSScript* script, JitCode* code);
};

class InlineCachePerfSpewer : public PerfSpewer {
  static UniqueChars cacheopFilename;

 public:
  void recordInstruction(MacroAssembler& masm, CacheOp op);
  void writeProfile(JitCode* code, const char* name);
};

#endif  // JS_ION_PERF

}  // namespace jit
}  // namespace js

#endif /* jit_PerfSpewer_h */
