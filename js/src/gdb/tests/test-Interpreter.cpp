/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gdb-tests.h"

#include "vm/Stack.h"

namespace js {

void GDBTestInitInterpreterRegs(InterpreterRegs& regs,
                                js::InterpreterFrame* fp_, JS::Value* sp,
                                uint8_t* pc) {
  regs.fp_ = fp_;
  regs.sp = sp;
  regs.pc = pc;
}

void GDBTestInitAbstractFramePtr(AbstractFramePtr& frame,
                                 InterpreterFrame* ptr) {
  MOZ_ASSERT((uintptr_t(ptr) & AbstractFramePtr::TagMask) == 0);
  frame.ptr_ = uintptr_t(ptr) | AbstractFramePtr::Tag_InterpreterFrame;
}

void GDBTestInitAbstractFramePtr(AbstractFramePtr& frame,
                                 jit::BaselineFrame* ptr) {
  MOZ_ASSERT((uintptr_t(ptr) & AbstractFramePtr::TagMask) == 0);
  frame.ptr_ = uintptr_t(ptr) | AbstractFramePtr::Tag_BaselineFrame;
}

void GDBTestInitAbstractFramePtr(AbstractFramePtr& frame,
                                 jit::RematerializedFrame* ptr) {
  MOZ_ASSERT((uintptr_t(ptr) & AbstractFramePtr::TagMask) == 0);
  frame.ptr_ = uintptr_t(ptr) | AbstractFramePtr::Tag_RematerializedFrame;
}

void GDBTestInitAbstractFramePtr(AbstractFramePtr& frame,
                                 wasm::DebugFrame* ptr) {
  MOZ_ASSERT((uintptr_t(ptr) & AbstractFramePtr::TagMask) == 0);
  frame.ptr_ = uintptr_t(ptr) | AbstractFramePtr::Tag_WasmDebugFrame;
}

}  // namespace js

FRAGMENT(Interpreter, Regs) {
  struct FakeFrame {
    js::InterpreterFrame frame;
    JS::Value slot0;
    JS::Value slot1;
    JS::Value slot2;
  } fakeFrame;
  uint8_t fakeOpcode = uint8_t(JSOp::True);

  js::InterpreterRegs regs;
  js::GDBTestInitInterpreterRegs(regs, &fakeFrame.frame, &fakeFrame.slot2,
                                 &fakeOpcode);

  breakpoint();

  use(regs);
}

FRAGMENT(Interpreter, AbstractFramePtr) {
  js::AbstractFramePtr ifptr;
  GDBTestInitAbstractFramePtr(ifptr,
                              (js::InterpreterFrame*)uintptr_t(0x8badf00));

  js::AbstractFramePtr bfptr;
  GDBTestInitAbstractFramePtr(bfptr,
                              (js::jit::BaselineFrame*)uintptr_t(0xbadcafe0));

  js::AbstractFramePtr rfptr;
  GDBTestInitAbstractFramePtr(
      rfptr, (js::jit::RematerializedFrame*)uintptr_t(0xdabbad00));

  js::AbstractFramePtr sfptr;
  GDBTestInitAbstractFramePtr(sfptr,
                              (js::wasm::DebugFrame*)uintptr_t(0xcb98ad00));

  breakpoint();

  use(ifptr);
  use(bfptr);
  use(rfptr);
  use(sfptr);
}
