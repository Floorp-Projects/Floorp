/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/Assembler-shared.h"

#include "jit/MacroAssembler-inl.h"

using namespace js::jit;

void CodeLocationLabel::repoint(JitCode* code) {
  MOZ_ASSERT(state_ == Relative);
  uintptr_t new_off = uintptr_t(raw_);
  MOZ_ASSERT(new_off < code->instructionsSize());

  raw_ = code->raw() + new_off;
  setAbsolute();
}
