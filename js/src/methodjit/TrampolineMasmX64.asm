; -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.


extern js_InternalThrow:PROC
extern PushActiveVMFrame:PROC
extern PopActiveVMFrame:PROC
extern js_InternalInterpret:PROC

.CODE

; JSBool JaegerTrampoline(JSContext *cx, StackFrame *fp, void *code,
;                         Value *stackLimit, void *safePoint);
JaegerTrampoline PROC FRAME
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    push    r12
    .PUSHREG r12
    push    r13
    .PUSHREG r13
    push    r14
    .PUSHREG r14
    push    r15
    .PUSHREG r15
    push    rdi
    .PUSHREG rdi
    push    rsi
    .PUSHREG rsi
    push    rbx
    .PUSHREG rbx
    sub     rsp, 16*10+8
    .ALLOCSTACK 168
    ; .SAVEXMM128 only supports 16 byte alignment offset
    movdqa  xmmword ptr [rsp], xmm6
    .SAVEXMM128 xmm6, 0
    movdqa  xmmword ptr [rsp+16], xmm7
    .SAVEXMM128 xmm7, 16
    movdqa  xmmword ptr [rsp+16*2], xmm8
    .SAVEXMM128 xmm8, 32
    movdqa  xmmword ptr [rsp+16*3], xmm9
    .SAVEXMM128 xmm9, 48
    movdqa  xmmword ptr [rsp+16*4], xmm10
    .SAVEXMM128 xmm10, 64
    movdqa  xmmword ptr [rsp+16*5], xmm11
    .SAVEXMM128 xmm11, 80
    movdqa  xmmword ptr [rsp+16*6], xmm12
    .SAVEXMM128 xmm12, 96
    movdqa  xmmword ptr [rsp+16*7], xmm13
    .SAVEXMM128 xmm13, 112
    movdqa  xmmword ptr [rsp+16*8], xmm14
    .SAVEXMM128 xmm14, 128
    movdqa  xmmword ptr [rsp+16*9], xmm15
    .SAVEXMM128 xmm15, 144
    ; stack aligment  for Win64 ABI
    sub     rsp, 8
    .ALLOCSTACK 8
    .ENDPROLOG

    ; Load mask registers
    mov     r13, 0ffff800000000000h
    mov     r14, 7fffffffffffh

    ; Build the JIT frame.
    ; rcx = cx
    ; rdx = fp
    ; r9 = inlineCallCount
    ; fp must go into rbx
    push    0       ; stubRejoin
    push    rdx     ; entryncode
    push    rdx     ; entryFp
    push    r9      ; inlineCallCount
    push    rcx     ; cx
    push    rdx     ; fp
    mov     rbx, rdx

    ; Space for the rest of the VMFrame.
    sub     rsp, 28h

    ; This is actually part of the VMFrame.
    mov     r10, [rbp+8*5+8]
    push    r10

    ; Set cx->regs and set the active frame. Save r8 and align frame in one
    push    r8
    mov     rcx, rsp
    sub     rsp, 20h
    call    PushActiveVMFrame
    add     rsp, 20h

    ; Jump into the JIT code.
    jmp     qword ptr [rsp]
JaegerTrampoline ENDP

; void JaegerTrampolineReturn();
JaegerTrampolineReturn PROC FRAME
    .ENDPROLOG
    or      rsi, rdi
    mov     qword ptr [rbx+30h], rsi
    sub     rsp, 20h
    lea     rcx, [rsp+20h]
    call    PopActiveVMFrame

    add     rsp, 68h+20h+8+16*10+8
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
JaegerTrampolineReturn ENDP


; void JaegerThrowpoline()
JaegerThrowpoline PROC FRAME
    .ENDPROLOG
    ; For Windows x64 stub calls, we pad the stack by 32 before
    ; calling, so we must account for that here. See doStubCall.
    lea     rcx, [rsp+20h]
    call    js_InternalThrow
    test    rax, rax
    je      throwpoline_exit
    add     rsp, 20h
    jmp     rax

throwpoline_exit:
    lea     rcx, [rsp+20h]
    call    PopActiveVMFrame
    add     rsp, 68h+20h+8+16*10+8
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
JaegerThrowpoline ENDP

JaegerInterpoline PROC FRAME
    .ENDPROLOG
    mov     rcx, rdi
    mov     rdx, rsi
    lea     r9, [rsp+20h]
    mov     r8, rax
    call    js_InternalInterpret
    mov     rbx, qword ptr [rsp+38h+20h] ; Load Frame
    mov     rsi, qword ptr [rbx+30h]     ; Load rval payload
    and     rsi, r14                     ; Mask rval payload
    mov     rdi, qword ptr [rbx+30h]     ; Load rval type
    and     rdi, r13                     ; Mask rval type
    mov     rcx, qword ptr [rsp+18h+20h] ; Load scratch -> argc
    test    rax, rax
    je      interpoline_exit
    add     rsp, 20h
    jmp     rax

interpoline_exit:
    lea     rcx, [rsp+20h]
    call    PopActiveVMFrame
    add     rsp, 68h+20h+8+16*10+8
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
JaegerInterpoline ENDP

JaegerInterpolineScripted PROC FRAME
    .ENDPROLOG
    mov     rbx, qword ptr [rbx+20h] ; Load prev
    mov     qword ptr [rsp+38h], rbx ; fp -> regs.fp
    sub     rsp, 20h
    jmp     JaegerInterpoline
JaegerInterpolineScripted ENDP

JaegerInterpolinePatched PROC FRAME
    sub     rsp, 20h
    .ALLOCSTACK 32
    .ENDPROLOG
    jmp     JaegerInterpoline
JaegerInterpolinePatched ENDP


END
