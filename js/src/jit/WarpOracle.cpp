/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpOracle.h"

#include "jit/JitScript.h"
#include "jit/MIRGenerator.h"
#include "jit/WarpBuilder.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"

#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"

using namespace js;
using namespace js::jit;

WarpOracle::WarpOracle(JSContext* cx, MIRGenerator& mirGen, HandleScript script)
    : cx_(cx), mirGen_(mirGen), alloc_(mirGen.alloc()), script_(script) {}

mozilla::GenericErrorResult<AbortReason> WarpOracle::abort(AbortReason r) {
  auto res = mirGen_.abort(r);
  JitSpew(JitSpew_IonAbort, "aborted @ %s", script_->filename());
  return res;
}

mozilla::GenericErrorResult<AbortReason> WarpOracle::abort(AbortReason r,
                                                           const char* message,
                                                           ...) {
  va_list ap;
  va_start(ap, message);
  auto res = mirGen_.abortFmt(r, message, ap);
  va_end(ap);
  JitSpew(JitSpew_IonAbort, "aborted @ %s", script_->filename());
  return res;
}

AbortReasonOr<WarpSnapshot*> WarpOracle::createSnapshot() {
  WarpScriptSnapshot* scriptSnapshot;
  MOZ_TRY_VAR(scriptSnapshot, createScriptSnapshot(script_));

  auto* snapshot = alloc_.lifoAlloc()->new_<WarpSnapshot>(scriptSnapshot);
  if (!snapshot) {
    return abort(AbortReason::Alloc);
  }

  return snapshot;
}

AbortReasonOr<WarpScriptSnapshot*> WarpOracle::createScriptSnapshot(
    HandleScript script) {
  MOZ_ASSERT(script->hasJitScript());

  if (!script->jitScript()->ensureHasCachedIonData(cx_, script)) {
    return abort(AbortReason::Error);
  }

  // Analyze the bytecode to look for opcodes we can't compile yet. Eventually
  // this loop will also be responsible for copying IC data.
  for (BytecodeLocation loc : AllBytecodesIterable(script)) {
    JSOp op = loc.getOp();
    switch (op) {
#define OP_CASE(OP) case JSOp::OP:
      WARP_OPCODE_LIST(OP_CASE)
#undef OP_CASE
      break;

      default:
#ifdef DEBUG
        return abort(AbortReason::Disable, "Unsupported opcode: %s",
                     CodeName(op));
#else
        return abort(AbortReason::Disable, "Unsupported opcode: %u",
                     uint8_t(op));
#endif
    }
  }

  auto* scriptSnapshot = alloc_.lifoAlloc()->new_<WarpScriptSnapshot>(script);
  if (!scriptSnapshot) {
    return abort(AbortReason::Alloc);
  }

  return scriptSnapshot;
}