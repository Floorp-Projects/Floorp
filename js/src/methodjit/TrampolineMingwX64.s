# -*- Mode: C++# tab-width: 4# indent-tabs-mode: nil# c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


.extern js_InternalThrow
.extern PushActiveVMFrame
.extern PopActiveVMFrame
.extern js_InternalInterpret

.text
.intel_syntax noprefix

# JSBool JaegerTrampoline(JSContext *cx, StackFrame *fp, void *code,
#                         Value *stackLimit, void *safePoint)#
.globl JaegerTrampoline
.def JaegerTrampoline
   .scl 3
   .type 46
.endef
JaegerTrampoline:
    push    rbp
    # .PUSHREG rbp
    mov     rbp, rsp
    # .SETFRAME rbp, 0
    push    r12
    # .PUSHREG r12
    push    r13
    # .PUSHREG r13
    push    r14
    # .PUSHREG r14
    push    r15
    # .PUSHREG r15
    push    rdi
    # .PUSHREG rdi
    push    rsi
    # .PUSHREG rsi
    push    rbx
    # .PUSHREG rbx
    sub     rsp, 16*10+8
    # .ALLOCSTACK 168
    # .SAVEXMM128 only supports 16 byte alignment offset
    movdqa  xmmword ptr [rsp], xmm6
    # .SAVEXMM128 xmm6, 0
    movdqa  xmmword ptr [rsp+16], xmm7
    # .SAVEXMM128 xmm7, 16
    movdqa  xmmword ptr [rsp+16*2], xmm8
    # .SAVEXMM128 xmm8, 32
    movdqa  xmmword ptr [rsp+16*3], xmm9
    # .SAVEXMM128 xmm9, 48
    movdqa  xmmword ptr [rsp+16*4], xmm10
    # .SAVEXMM128 xmm10, 64
    movdqa  xmmword ptr [rsp+16*5], xmm11
    # .SAVEXMM128 xmm11, 80
    movdqa  xmmword ptr [rsp+16*6], xmm12
    # .SAVEXMM128 xmm12, 96
    movdqa  xmmword ptr [rsp+16*7], xmm13
    # .SAVEXMM128 xmm13, 112
    movdqa  xmmword ptr [rsp+16*8], xmm14
    # .SAVEXMM128 xmm14, 128
    movdqa  xmmword ptr [rsp+16*9], xmm15
    # .SAVEXMM128 xmm15, 144
    # stack aligment  for Win64 ABI
    sub     rsp, 8
    # .ALLOCSTACK 8
    # .ENDPROLOG

    # Load mask registers
    mov     r13, 0xffff800000000000
    mov     r14, 0x7fffffffffff

    # Build the JIT frame.
    # rcx = cx
    # rdx = fp
    # r9 = inlineCallCount
    # fp must go into rbx
    push    0       # stubRejoin
    push    rdx     # entryncode
    push    rdx     # entryFp
    push    r9      # inlineCallCount
    push    rcx     # cx
    push    rdx     # fp
    mov     rbx, rdx

    # Space for the rest of the VMFrame.
    sub     rsp, 0x28

    # This is actually part of the VMFrame.
    mov     r10, [rbp+8*5+8]
    push    r10

    # Set cx->regs and set the active frame. Save r8 and align frame in one
    push    r8
    mov     rcx, rsp
    sub     rsp, 0x20
    call    PushActiveVMFrame
    add     rsp, 0x20

    # Jump into the JIT code.
    jmp     qword ptr [rsp]

# void JaegerTrampolineReturn()#
.globl JaegerTrampolineReturn
.def JaegerTrampolineReturn
   .scl 3
   .type 46
.endef
JaegerTrampolineReturn:
    # .ENDPROLOG
    or      rsi, rdi
    mov     qword ptr [rbx + 0x30], rsi
    sub     rsp, 0x20
    lea     rcx, [rsp+0x20]
    call    PopActiveVMFrame

    add     rsp, 0x68+0x20+8+16*10+8
    movdqa  xmm6, xmmword ptr [rsp-16*10-8]
    movdqa  xmm7, xmmword ptr [rsp-16*9-8]
    movdqa  xmm8, xmmword ptr [rsp-16*8-8]
    movdqa  xmm9, xmmword ptr [rsp-16*7-8]
    movdqa  xmm10, xmmword ptr [rsp-16*6-8]
    movdqa  xmm11, xmmword ptr [rsp-16*5-8]
    movdqa  xmm12, xmmword ptr [rsp-16*4-8]
    movdqa  xmm13, xmmword ptr [rsp-16*3-8]
    movdqa  xmm14, xmmword ptr [rsp-16*2-8]
    movdqa  xmm15, xmmword ptr [rsp-16*1-8]
    pop     rbx
    pop     rsi
    pop     rdi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    mov     rax, 1
    ret


# void JaegerThrowpoline()
.globl JaegerThrowpoline
.def JaegerTrampoline
   .scl 3
   .type 46
.endef
JaegerThrowpoline:
    # .ENDPROLOG
    # For Windows x64 stub calls, we pad the stack by 32 before
    # calling, so we must account for that here. See doStubCall.
    lea     rcx, [rsp+0x20]
    call    js_InternalThrow
    test    rax, rax
    je      throwpoline_exit
    add     rsp, 0x20
    jmp     rax

throwpoline_exit:
    lea     rcx, [rsp+0x20]
    call    PopActiveVMFrame
    add     rsp, 0x68+0x20+8+16*10+8
    movdqa  xmm6, xmmword ptr [rsp-16*10-8]
    movdqa  xmm7, xmmword ptr [rsp-16*9-8]
    movdqa  xmm8, xmmword ptr [rsp-16*8-8]
    movdqa  xmm9, xmmword ptr [rsp-16*7-8]
    movdqa  xmm10, xmmword ptr [rsp-16*6-8]
    movdqa  xmm11, xmmword ptr [rsp-16*5-8]
    movdqa  xmm12, xmmword ptr [rsp-16*4-8]
    movdqa  xmm13, xmmword ptr [rsp-16*3-8]
    movdqa  xmm14, xmmword ptr [rsp-16*2-8]
    movdqa  xmm15, xmmword ptr [rsp-16*1-8]
    pop     rbx
    pop     rsi
    pop     rdi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    xor     rax, rax
    ret


.globl JaegerInterpoline
.def JaegerInterpoline
   .scl 3
   .type 46
.endef
JaegerInterpoline:
    #.ENDPROLOG
    mov     rcx, rdi
    mov     rdx, rsi
    lea     r9, [rsp+0x20]
    mov     r8, rax
    call    js_InternalInterpret
    mov     rbx, qword ptr [rsp+0x38+0x20] # Load Frame
    mov     rsi, qword ptr [rbx+0x30]      # Load rval payload
    and     rsi, r14                       # Mask rval payload
    mov     rdi, qword ptr [rbx+0x30]      # Load rval type
    and     rdi, r13                       # Mask rval type
    mov     rcx, qword ptr [rsp+0x18+0x20] # Load scratch -> argc
    test    rax, rax
    je      interpoline_exit
    add     rsp, 0x20
    jmp     rax

interpoline_exit:
    lea     rcx, [rsp+0x20]
    call    PopActiveVMFrame
    add     rsp, 0x68+0x20+8+16*10+8
    movdqa  xmm6, xmmword ptr [rsp-16*10-8]
    movdqa  xmm7, xmmword ptr [rsp-16*9-8]
    movdqa  xmm8, xmmword ptr [rsp-16*8-8]
    movdqa  xmm9, xmmword ptr [rsp-16*7-8]
    movdqa  xmm10, xmmword ptr [rsp-16*6-8]
    movdqa  xmm11, xmmword ptr [rsp-16*5-8]
    movdqa  xmm12, xmmword ptr [rsp-16*4-8]
    movdqa  xmm13, xmmword ptr [rsp-16*3-8]
    movdqa  xmm14, xmmword ptr [rsp-16*2-8]
    movdqa  xmm15, xmmword ptr [rsp-16*1-8]
    pop     rbx
    pop     rsi
    pop     rdi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    xor     rax, rax
    ret

.globl JaegerInterpolineScripted
.def JaegerInterpolineScripted
   .scl 3
   .type 46
.endef
JaegerInterpolineScripted:
    #.ENDPROLOG
    mov     rbx, qword ptr [rbx+0x20] # Load prev
    mov     qword ptr [rsp+0x38], rbx # fp -> regs.fp
    sub     rsp, 0x20
    jmp     JaegerInterpoline

.globl JaegerInterpolinePatched
.def JaegerInterpolinePatched
   .scl 3
   .type 46
.endef
JaegerInterpolinePatched:
    sub     rsp, 0x20
    #.ALLOCSTACK 32
    #.ENDPROLOG
    jmp     JaegerInterpoline
