/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/LabelEmitter.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "frontend/BytecodeEmitter.h"  // BytecodeEmitter
#include "vm/Opcodes.h"                // JSOP_*

using namespace js;
using namespace js::frontend;

bool LabelEmitter::emitLabel(HandleAtom name) {
  MOZ_ASSERT(state_ == State::Start);

  // Emit a JSOP_LABEL instruction. The operand is the offset to the statement
  // following the labeled statement. The offset is set in emitEnd().
  uint32_t index;
  if (!bce_->makeAtomIndex(name, &index)) {
    return false;
  }
  if (!bce_->emitN(JSOP_LABEL, 4, &top_)) {
    return false;
  }

  controlInfo_.emplace(bce_, name, bce_->bytecodeSection().offset());

#ifdef DEBUG
  state_ = State::Label;
#endif
  return true;
}

bool LabelEmitter::emitEnd() {
  MOZ_ASSERT(state_ == State::Label);

  // Patch the JSOP_LABEL offset.
  jsbytecode* labelpc = bce_->bytecodeSection().code(top_);
  int32_t offset = bce_->bytecodeSection().lastNonJumpTargetOffset() - top_;
  MOZ_ASSERT(*labelpc == JSOP_LABEL);
  SET_CODE_OFFSET(labelpc, offset);

  // Patch the break/continue to this label.
  if (!controlInfo_->patchBreaks(bce_)) {
    return false;
  }

  controlInfo_.reset();

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}
