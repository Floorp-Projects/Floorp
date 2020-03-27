/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the NativeRegExpMacroAssembler interface for
// SpiderMonkey. It provides the same interface as each of V8's
// architecture-specific implementations.

#ifndef RegexpMacroAssemblerArch_h
#define RegexpMacroAssemblerArch_h

#include "jit/MacroAssembler.h"
#include "new-regexp/regexp-macro-assembler.h"

namespace v8 {
namespace internal {

class SMRegExpMacroAssembler final : public NativeRegExpMacroAssembler {
 public:
  SMRegExpMacroAssembler(JSContext* cx, Isolate* isolate,
                         js::jit::StackMacroAssembler& masm, Zone* zone,
                         Mode mode, uint32_t num_capture_registers);
  virtual ~SMRegExpMacroAssembler() {} // Nothing to do here

  virtual int stack_limit_slack();
  virtual IrregexpImplementation Implementation();


private:
  JSContext* cx_;
  js::jit::StackMacroAssembler& masm_;

  Mode mode_;
  int num_registers_;
  int num_capture_registers_;
};

}  // namespace internal
}  // namespace v8

#endif  // RegexpMacroAssemblerArch_h
