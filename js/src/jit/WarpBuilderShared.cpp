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
