/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gdb-tests.h"
#include "jsapi.h"

#include "vm/Stack.h"

namespace js {

void
GDBTestInitInterpreterRegs(InterpreterRegs& regs,
                           js::InterpreterFrame* fp_,
                           JS::Value* sp,
                           uint8_t* pc)
{
    regs.fp_ = fp_;
    regs.sp = sp;
    regs.pc = pc;
}

void
GDBTestInitAbstractFramePtr(AbstractFramePtr& frame, void* ptr)
{
    MOZ_ASSERT((uintptr_t(ptr) &  AbstractFramePtr::TagMask) == 0);
    frame.ptr_ = uintptr_t(ptr) | AbstractFramePtr::Tag_ScriptFrameIterData;
}

void
GDBTestInitAbstractFramePtr(AbstractFramePtr& frame, InterpreterFrame* ptr)
{
    MOZ_ASSERT((uintptr_t(ptr) & AbstractFramePtr::TagMask) == 0);
    frame.ptr_ = uintptr_t(ptr) | AbstractFramePtr::Tag_InterpreterFrame;
}

void
GDBTestInitAbstractFramePtr(AbstractFramePtr& frame, jit::BaselineFrame* ptr)
{
    MOZ_ASSERT((uintptr_t(ptr) & AbstractFramePtr::TagMask) == 0);
    frame.ptr_ = uintptr_t(ptr) | AbstractFramePtr::Tag_BaselineFrame;
}

void
GDBTestInitAbstractFramePtr(AbstractFramePtr& frame, jit::RematerializedFrame* ptr)
{
    MOZ_ASSERT((uintptr_t(ptr) & AbstractFramePtr::TagMask) == 0);
    frame.ptr_ = uintptr_t(ptr) | AbstractFramePtr::Tag_RematerializedFrame;
}

} // namespace js

FRAGMENT(Interpreter, Regs) {
  struct FakeFrame {
    js::InterpreterFrame frame;
    JS::Value slot0;
    JS::Value slot1;
    JS::Value slot2;
  } fakeFrame;
  uint8_t fakeOpcode = JSOP_IFEQ;

  js::InterpreterRegs regs;
  js::GDBTestInitInterpreterRegs(regs, &fakeFrame.frame, &fakeFrame.slot2, &fakeOpcode);

  breakpoint();

  (void) regs;
}

FRAGMENT(Interpreter, AbstractFramePtr) {

    js::AbstractFramePtr sfidptr;
    GDBTestInitAbstractFramePtr(sfidptr, (js::ScriptFrameIter::Data*) uintptr_t(0xdeeb0));

    js::AbstractFramePtr ifptr;
    GDBTestInitAbstractFramePtr(ifptr, (js::InterpreterFrame*) uintptr_t(0x8badf00));

    js::AbstractFramePtr bfptr;
    GDBTestInitAbstractFramePtr(bfptr, (js::jit::BaselineFrame*) uintptr_t(0xbadcafe0));

    js::AbstractFramePtr rfptr;
    GDBTestInitAbstractFramePtr(rfptr, (js::jit::RematerializedFrame*) uintptr_t(0xdabbad00));

    breakpoint();

    (void) sfidptr;
    (void) ifptr;
    (void) bfptr;
    (void) rfptr;
}
