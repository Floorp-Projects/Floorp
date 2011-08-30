/ -*- Mode: C++/ tab-width: 4/ indent-tabs-mode: nil/ c-basic-offset: 4 -*-
/ ***** BEGIN LICENSE BLOCK *****
/ Version: MPL 1.1/GPL 2.0/LGPL 2.1
/
/ The contents of this file are subject to the Mozilla Public License Version
/ 1.1 (the "License")/ you may not use this file except in compliance with
/ the License. You may obtain a copy of the License at
/ http://www.mozilla.org/MPL/
/
/ Software distributed under the License is distributed on an "AS IS" basis,
/ WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
/ for the specific language governing rights and limitations under the
/ License.
/
/ The Original Code is mozilla.org code.
/
/ The Initial Developer of the Original Code is Mozilla Japan.
/ Portions created by the Initial Developer are Copyright (C) 2010
/ the Initial Developer. All Rights Reserved.
/
/ Contributor(s):
/   Leon Sha <leon.sha@sun.com>
/
/ Alternatively, the contents of this file may be used under the terms of
/ either the GNU General Public License Version 2 or later (the "GPL"), or
/ the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
/ in which case the provisions of the GPL or the LGPL are applicable instead
/ of those above. If you wish to allow use of your version of this file only
/ under the terms of either the GPL or the LGPL, and not to allow others to
/ use your version of this file under the terms of the MPL, indicate your
/ decision by deleting the provisions above and replace them with the notice
/ and other provisions required by the GPL or the LGPL. If you do not delete
/ the provisions above, a recipient may use your version of this file under
/ the terms of any one of the MPL, the GPL or the LGPL.
/
/ ***** END LICENSE BLOCK *****

.text

/ JSBool JaegerTrampoline(JSContext *cx, StackFrame *fp, void *code,
/                         Value *stackLimit)
.global JaegerTrampoline
.type   JaegerTrampoline, @function
JaegerTrampoline:
    /* Prologue. */
    pushq %rbp
    movq %rsp, %rbp
    /* Save non-volatile registers. */
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushq %rbx

    /* Load mask registers. */
    movq $0xFFFF800000000000, %r13
    movq $0x00007FFFFFFFFFFF, %r14

    /* Build the JIT frame.
     * rdi = cx
     * rsi = fp
     * rcx = inlineCallCount
     * fp must go into rbx
     */
    pushq $0x0        /* stubRejoin */
    pushq %rsi        /* entryncode */
    pushq %rsi        /* entryfp */
    pushq %rcx        /* inlineCallCount */
    pushq %rdi        /* cx */
    pushq %rsi        /* fp */
    movq  %rsi, %rbx

    /* Space for the rest of the VMFrame. */
    subq  $0x28, %rsp

    /* This is actually part of the VMFrame. */
    pushq %r8

    /* Set cx->regs and set the active frame. Save rdx and align frame in one. */
    pushq %rdx
    movq  %rsp, %rdi
    call SetVMFrameRegs
    movq  %rsp, %rdi
    call PushActiveVMFrame

    /* Jump into into the JIT'd code. */
    jmp *0(%rsp)
.size   JaegerTrampoline, . - JaegerTrampoline

/ void JaegerTrampolineReturn()
.global JaegerTrampolineReturn
.type   JaegerTrampolineReturn, @function
JaegerTrampolineReturn:
    or   %rdi, %rsi
    movq %rsx, 0x30(%rbx)
    movq %rsp, %rdi
    call PopActiveVMFrame

    addq $0x68, %rsp
    popq %rbx
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    movq $1, %rax
    ret
.size   JaegerTrampolineReturn, . - JaegerTrampolineReturn


/ void *JaegerThrowpoline(js::VMFrame *vmFrame)
.global JaegerThrowpoline
.type   JaegerThrowpoline, @function
JaegerThrowpoline:
    movq %rsp, %rdi
    call js_InternalThrow
    testq %rax, %rax
    je   throwpoline_exit
    jmp  *%rax
  throwpoline_exit:
    movq %rsp, %rdi
    call PopActiveVMFrame
    addq $0x68, %rsp
    popq %rbx
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    xorq %rax,%rax
    ret
.size   JaegerThrowpoline, . - JaegerThrowpoline

/ void JaegerInterpoline()
.global JaegerInterpoline
.type   JaegerInterpoline, @function
JaegerInterpoline:
    movq %rsp, %rcx
    movq %rax, %rdx
    call js_InternalInterpret
    movq 0x38(%rsp), %rbx             /* Load frame */
    movq 0x30(%rbx), %rsi             /* Load rval payload */
    and %r14, %rsi                    /* Mask rval payload */
    movq 0x30(%rbx), %rdi             /* Load rval type */
    and %r13, %rdi                    /* Mask rval type */
    movq 0x18(%rsp), %rcx             /* Load scratch -> argc */
    testq %rax, %rax
    je   interpoline_exit
    jmp  *%rax
  interpoline_exit:
    movq %rsp, %rdi
    call PopActiveVMFrame
    addq $0x68, %rsp
    popq %rbx
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    xorq %rax,%rax
    ret
.size   JaegerInterpoline, . - JaegerInterpoline

/ void JaegerInterpolineScripted()
.global JaegerInterpolineScripted
.type   JaegerInterpolineScripted, @function
JaegerInterpolineScripted:
    movq 0x20(%rbx), %rbx             /* load prev */
    movq %rbx, 0x38(%rsp)
    jmp JaegerInterpoline
.size   JaegerInterpolineScripted, . - JaegerInterpolineScripted
