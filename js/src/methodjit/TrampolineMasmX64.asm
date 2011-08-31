; -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
; ***** BEGIN LICENSE BLOCK *****
; Version: MPL 1.1/GPL 2.0/LGPL 2.1
;
; The contents of this file are subject to the Mozilla Public License Version
; 1.1 (the "License"); you may not use this file except in compliance with
; the License. You may obtain a copy of the License at
; http://www.mozilla.org/MPL/
;
; Software distributed under the License is distributed on an "AS IS" basis,
; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
; for the specific language governing rights and limitations under the
; License.
;
; The Original Code is mozilla.org code.
;
; The Initial Developer of the Original Code is Mozilla Japan.
; Portions created by the Initial Developer are Copyright (C) 2010
; the Initial Developer. All Rights Reserved.
;
; Contributor(s):
;   Makoto Kato <m_kato@ga2.so-net.ne.jp>
;
; Alternatively, the contents of this file may be used under the terms of
; either the GNU General Public License Version 2 or later (the "GPL"), or
; the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
; in which case the provisions of the GPL or the LGPL are applicable instead
; of those above. If you wish to allow use of your version of this file only
; under the terms of either the GPL or the LGPL, and not to allow others to
; use your version of this file under the terms of the MPL, indicate your
; decision by deleting the provisions above and replace them with the notice
; and other provisions required by the GPL or the LGPL. If you do not delete
; the provisions above, a recipient may use your version of this file under
; the terms of any one of the MPL, the GPL or the LGPL.
;
; ***** END LICENSE BLOCK *****


extern js_InternalThrow:PROC
extern SetVMFrameRegs:PROC
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
    call    SetVMFrameRegs
    lea     rcx, [rsp+20h]
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
