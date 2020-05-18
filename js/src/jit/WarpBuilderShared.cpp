/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpBuilderShared.h"

#include "jit/MIRBuilderShared.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace js::jit;

WarpBuilderShared::WarpBuilderShared(MIRGenerator& mirGen,
                                     MBasicBlock* current_)
    : mirGen_(mirGen), alloc_(mirGen.alloc()), current(current_) {}

bool WarpBuilderShared::resumeAfter(MInstruction* ins, BytecodeLocation loc) {
  MOZ_ASSERT(ins->isEffectful());

  MResumePoint* resumePoint = MResumePoint::New(
      alloc(), ins->block(), loc.toRawBytecode(), MResumePoint::ResumeAfter);
  if (!resumePoint) {
    return false;
  }

  ins->setResumePoint(resumePoint);
  return true;
}

MConstant* WarpBuilderShared::constant(const Value& v) {
  MOZ_ASSERT_IF(v.isString(), v.toString()->isAtom());
  MOZ_ASSERT_IF(v.isGCThing(), !IsInsideNursery(v.toGCThing()));

  MConstant* cst = MConstant::New(alloc(), v);
  current->add(cst);
  return cst;
}

void WarpBuilderShared::pushConstant(const Value& v) {
  MConstant* cst = constant(v);
  current->push(cst);
}

MCall* WarpBuilderShared::makeCall(CallInfo& callInfo, bool needsThisCheck,
                                   JSFunction* target) {
  MOZ_ASSERT_IF(needsThisCheck, !target);

  // TODO: specialize for known target. Pad missing arguments. Set MCall flags
  // based on this known target.
  uint32_t targetArgs = callInfo.argc();
  bool isDOMCall = false;
  DOMObjectKind objKind = DOMObjectKind::Unknown;

  // TODO: MCall::New initializes WrappedFunction with various flags from
  // the target. This not thread-safe for non-native JSFunctions.
  MOZ_ASSERT_IF(target, target->isNative());
  MCall* call =
      MCall::New(alloc(), target, targetArgs + 1 + callInfo.constructing(),
                 callInfo.argc(), callInfo.constructing(),
                 callInfo.ignoresReturnValue(), isDOMCall, objKind);
  if (!call) {
    return nullptr;
  }

  if (callInfo.constructing()) {
    // Note: setThis should have been done by the caller of makeCall.
    if (needsThisCheck) {
      call->setNeedsThisCheck();
    }
    call->addArg(targetArgs + 1, callInfo.getNewTarget());
  }

  // Add explicit arguments.
  // Skip addArg(0) because it is reserved for |this|.
  for (int32_t i = callInfo.argc() - 1; i >= 0; i--) {
    call->addArg(i + 1, callInfo.getArg(i));
  }

  // Pass |this| and function.
  call->addArg(0, callInfo.thisArg());
  call->initFunction(callInfo.fun());

  return call;
}
