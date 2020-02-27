/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpBuilder_h
#define jit_WarpBuilder_h

#include "jit/JitContext.h"

#include "jit/MIR.h"

namespace js {
namespace jit {

// JS bytecode ops supported by WarpBuilder. Once we support most opcodes
// this should be replaced with the FOR_EACH_OPCODE macro.
#define WARP_OPCODE_LIST(_) \
  _(Nop)                    \
  _(NopDestructuring)       \
  _(TryDestructuring)       \
  _(Lineno)                 \
  _(Undefined)              \
  _(Void)                   \
  _(Null)                   \
  _(Hole)                   \
  _(Uninitialized)          \
  _(False)                  \
  _(True)                   \
  _(Zero)                   \
  _(One)                    \
  _(Pop)                    \
  _(PopN)                   \
  _(Dup)                    \
  _(Dup2)                   \
  _(Swap)                   \
  _(GetLocal)               \
  _(SetLocal)               \
  _(InitLexical)            \
  _(Return)                 \
  _(RetRval)

class MIRGenerator;
class MIRGraph;
class WarpSnapshot;

// WarpBuilder builds a MIR graph from WarpSnapshot. Unlike WarpOracle,
// WarpBuilder can run off-thread.
class MOZ_STACK_CLASS WarpBuilder {
  WarpSnapshot& input_;
  MIRGenerator& mirGen_;
  MIRGraph& graph_;
  TempAllocator& alloc_;
  const CompileInfo& info_;
  JSScript* script_;
  MBasicBlock* current = nullptr;

  TempAllocator& alloc() { return alloc_; }
  MIRGraph& graph() { return graph_; }
  const CompileInfo& info() const { return info_; }
  WarpSnapshot& input() const { return input_; }

  BytecodeSite* newBytecodeSite(jsbytecode* pc);

  bool startNewBlock(size_t stackDepth, jsbytecode* pc,
                     MBasicBlock* maybePredecessor = nullptr);

  bool hasTerminatedBlock() const { return current == nullptr; }
  void setTerminatedBlock() { current = nullptr; }

  MConstant* constant(const Value& v);
  void pushConstant(const Value& v);

  MOZ_MUST_USE bool buildPrologue();
  MOZ_MUST_USE bool buildBody();
  MOZ_MUST_USE bool buildEpilogue();

#define BUILD_OP(OP) bool build_##OP(BytecodeLocation loc);
  WARP_OPCODE_LIST(BUILD_OP)
#undef BUILD_OP

 public:
  WarpBuilder(WarpSnapshot& input, MIRGenerator& mirGen);

  MOZ_MUST_USE bool build();
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpBuilder_h */
