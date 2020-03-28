/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BytecodeAnalysis.h"

#include "jit/JitSpewer.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"
#include "vm/BytecodeUtil.h"

#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"
#include "vm/BytecodeUtil-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

BytecodeAnalysis::BytecodeAnalysis(TempAllocator& alloc, JSScript* script)
    : script_(script), infos_(alloc), hasTryFinally_(false) {}

// Bytecode range containing only catch or finally code.
struct CatchFinallyRange {
  uint32_t start;  // Inclusive.
  uint32_t end;    // Exclusive.

  CatchFinallyRange(uint32_t start, uint32_t end) : start(start), end(end) {
    MOZ_ASSERT(end > start);
  }

  bool contains(uint32_t offset) const {
    return start <= offset && offset < end;
  }
};

bool BytecodeAnalysis::init(TempAllocator& alloc) {
  if (!infos_.growByUninitialized(script_->length())) {
    return false;
  }

  // Clear all BytecodeInfo.
  mozilla::PodZero(infos_.begin(), infos_.length());
  infos_[0].init(/*stackDepth=*/0);

  Vector<CatchFinallyRange, 0, JitAllocPolicy> catchFinallyRanges(alloc);

  for (const BytecodeLocation& it : AllBytecodesIterable(script_)) {
    JSOp op = it.getOp();
    uint32_t offset = it.bytecodeToOffset(script_);

    JitSpew(JitSpew_BaselineOp, "Analyzing op @ %u (end=%u): %s",
            unsigned(offset), unsigned(script_->length()), CodeName(op));

    // If this bytecode info has not yet been initialized, it's not reachable.
    if (!infos_[offset].initialized) {
      continue;
    }

    uint32_t stackDepth = infos_[offset].stackDepth;

#ifdef DEBUG
    size_t endOffset = offset + it.length();
    for (size_t checkOffset = offset + 1; checkOffset < endOffset;
         checkOffset++) {
      MOZ_ASSERT(!infos_[checkOffset].initialized);
    }
#endif
    uint32_t nuses = it.useCount();
    uint32_t ndefs = it.defCount();

    MOZ_ASSERT(stackDepth >= nuses);
    stackDepth -= nuses;
    stackDepth += ndefs;

    // If stack depth exceeds max allowed by analysis, fail fast.
    MOZ_ASSERT(stackDepth <= BytecodeInfo::MAX_STACK_DEPTH);

    switch (op) {
      case JSOp::TableSwitch: {
        uint32_t defaultOffset = it.getTableSwitchDefaultOffset(script_);
        int32_t low = it.getTableSwitchLow();
        int32_t high = it.getTableSwitchHigh();

        infos_[defaultOffset].init(stackDepth);
        infos_[defaultOffset].jumpTarget = true;

        uint32_t ncases = high - low + 1;

        for (uint32_t i = 0; i < ncases; i++) {
          uint32_t targetOffset = it.tableSwitchCaseOffset(script_, i);
          if (targetOffset != defaultOffset) {
            infos_[targetOffset].init(stackDepth);
            infos_[targetOffset].jumpTarget = true;
          }
        }
        break;
      }

      case JSOp::Try: {
        for (const TryNote& tn : script_->trynotes()) {
          if (tn.start == offset + JSOpLength_Try &&
              (tn.kind() == TryNoteKind::Catch ||
               tn.kind() == TryNoteKind::Finally)) {
            uint32_t catchOrFinallyOffset = tn.start + tn.length;
            infos_[catchOrFinallyOffset].init(stackDepth);
            infos_[catchOrFinallyOffset].jumpTarget = true;
          }
        }

        // Get the pc of the last instruction in the try block. It's a
        // JSOp::Goto to jump over the catch/finally blocks.
        BytecodeLocation endOfTryLoc(script_,
                                     it.toRawBytecode() + it.codeOffset());
        MOZ_ASSERT(endOfTryLoc.is(JSOp::Goto));

        BytecodeLocation afterTryLoc(
            script_, endOfTryLoc.toRawBytecode() + endOfTryLoc.jumpOffset());
        MOZ_ASSERT(afterTryLoc > endOfTryLoc);

        // Ensure the code following the try-block is always marked as
        // reachable, to simplify Ion's ControlFlowGenerator.
        uint32_t afterTryOffset = afterTryLoc.bytecodeToOffset(script_);
        infos_[afterTryOffset].init(stackDepth);
        infos_[afterTryOffset].jumpTarget = true;

        // Pop CatchFinallyRanges that are no longer needed.
        while (!catchFinallyRanges.empty() &&
               catchFinallyRanges.back().end <= offset) {
          catchFinallyRanges.popBack();
        }

        CatchFinallyRange range(endOfTryLoc.bytecodeToOffset(script_),
                                afterTryLoc.bytecodeToOffset(script_));
        if (!catchFinallyRanges.append(range)) {
          return false;
        }
        break;
      }

      case JSOp::LoopHead:
        for (size_t i = 0; i < catchFinallyRanges.length(); i++) {
          if (catchFinallyRanges[i].contains(offset)) {
            infos_[offset].loopHeadInCatchOrFinally = true;
          }
        }
        break;

      default:
        break;
    }

    bool jump = it.isJump();
    if (jump) {
      // Case instructions do not push the lvalue back when branching.
      uint32_t newStackDepth = stackDepth;
      if (it.is(JSOp::Case)) {
        newStackDepth--;
      }

      uint32_t targetOffset = it.getJumpTargetOffset(script_);

      // If this is a backedge, the target JSOp::LoopHead must have been
      // analyzed already.
      MOZ_ASSERT_IF(targetOffset < offset, infos_[targetOffset].initialized);

      infos_[targetOffset].init(newStackDepth);
      infos_[targetOffset].jumpTarget = true;
    }
    // Handle any fallthrough from this opcode.
    if (it.fallsThrough()) {
      BytecodeLocation fallthroughLoc = it.next();
      MOZ_ASSERT(fallthroughLoc.isInBounds(script_));
      uint32_t fallthroughOffset = fallthroughLoc.bytecodeToOffset(script_);

      infos_[fallthroughOffset].init(stackDepth);

      // Treat the fallthrough of a branch instruction as a jump target.
      if (jump) {
        infos_[fallthroughOffset].jumpTarget = true;
      }
    }
  }

  // Flag (reachable) resume offset instructions.
  for (uint32_t offset : script_->resumeOffsets()) {
    BytecodeInfo& info = infos_[offset];
    if (info.initialized) {
      info.hasResumeOffset = true;
    }
  }

  return true;
}

IonBytecodeInfo js::jit::AnalyzeBytecodeForIon(JSContext* cx,
                                               JSScript* script) {
  IonBytecodeInfo result;

  if (script->isModule() || script->initialEnvironmentShape() ||
      (script->function() &&
       script->function()->needsSomeEnvironmentObject())) {
    result.usesEnvironmentChain = true;
  }

  jsbytecode const* pcEnd = script->codeEnd();
  for (jsbytecode* pc = script->code(); pc < pcEnd; pc = GetNextPc(pc)) {
    JSOp op = JSOp(*pc);
    switch (op) {
      case JSOp::SetArg:
        result.modifiesArguments = true;
        break;

      case JSOp::GetName:
      case JSOp::BindName:
      case JSOp::BindVar:
      case JSOp::SetName:
      case JSOp::StrictSetName:
      case JSOp::DelName:
      case JSOp::GetAliasedVar:
      case JSOp::SetAliasedVar:
      case JSOp::Lambda:
      case JSOp::LambdaArrow:
      case JSOp::DefFun:
      case JSOp::DefVar:
      case JSOp::DefLet:
      case JSOp::DefConst:
      case JSOp::PushLexicalEnv:
      case JSOp::PopLexicalEnv:
      case JSOp::ImplicitThis:
      case JSOp::FunWithProto:
        result.usesEnvironmentChain = true;
        break;

      case JSOp::GetGName:
      case JSOp::SetGName:
      case JSOp::StrictSetGName:
      case JSOp::GImplicitThis:
        if (script->hasNonSyntacticScope()) {
          result.usesEnvironmentChain = true;
        }
        break;

      case JSOp::Finally:
        result.hasTryFinally = true;
        break;

      default:
        break;
    }
  }

  return result;
}
