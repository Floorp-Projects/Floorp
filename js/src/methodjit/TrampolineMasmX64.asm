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

    add     rsp, 68h+20h
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
    add     rsp, 68h+20h
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
    add     rsp, 68h+20h
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
