/ -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
/ This Source Code Form is subject to the terms of the Mozilla Public
/ License, v. 2.0. If a copy of the MPL was not distributed with this
/ file, You can obtain one at http://mozilla.org/MPL/2.0/.

.text

/ JSBool JaegerTrampoline(JSContext *cx, StackFrame *fp, void *code,
/                         Value *stackLimit)
.global JaegerTrampoline
.type   JaegerTrampoline, @function
JaegerTrampoline:
    /* Prologue. */
    pushl %ebp
    movl %esp, %ebp
    /* Save non-volatile registers. */
    pushl %esi
    pushl %edi
    pushl %ebx

    /* Build the JIT frame. Push fields in order, */
    /* then align the stack to form esp == VMFrame. */
    movl  12(%ebp), %ebx                       /* load fp */
    pushl %ebx                                 /* unused1 */
    pushl %ebx                                 /* unused0 */
    pushl $0x0                                 /* stubRejoin */
    pushl %ebx                                 /* entryncode */
    pushl %ebx                                 /* entryfp */
    pushl 20(%ebp)                             /* stackLimit */
    pushl 8(%ebp)                              /* cx */
    pushl %ebx                                 /* fp */
    subl $0x1C, %esp

    /* Jump into the JIT'd code. */
    /* No fastcall for sunstudio. */
    pushl %esp
    call PushActiveVMFrame
    popl  %edx

    movl 28(%esp), %ebp                       /* load fp for JIT code */
    jmp  *88(%esp)
.size   JaegerTrampoline, . - JaegerTrampoline

/ void JaegerTrampolineReturn()
.global JaegerTrampolineReturn
.type   JaegerTrampolineReturn, @function
JaegerTrampolineReturn:
    movl  %esi, 0x18(%ebp)
    movl  %edi, 0x1C(%ebp)
    movl  %esp, %ebp
    addl  $0x48, %ebp
    pushl %esp
    call PopActiveVMFrame

    addl $0x40, %esp
    popl %ebx
    popl %edi
    popl %esi
    popl %ebp
    movl $1, %eax
    ret
.size   JaegerTrampolineReturn, . - JaegerTrampolineReturn


/ void *JaegerThrowpoline(js::VMFrame *vmFrame)
.global JaegerThrowpoline
.type   JaegerThrowpoline, @function
JaegerThrowpoline:
    /* For Sun Studio there is no fast call. */
    /* We add the stack by 16 before. */
    addl $0x10, %esp
    /* Align the stack to 16 bytes. */
    pushl %esp
    pushl (%esp)
    pushl (%esp)
    pushl (%esp)
    call js_InternalThrow
    /* Bump the stack by 0x2c, as in the basic trampoline, but */
    /* also one more word to clean up the stack for jsl_InternalThrow,*/
    /* and another to balance the alignment above. */
    addl $0x10, %esp
    testl %eax, %eax
    je   throwpoline_exit
    jmp  *%eax
throwpoline_exit:
    pushl %esp
    call PopActiveVMFrame
    addl $0x40, %esp
    popl %ebx
    popl %edi
    popl %esi
    popl %ebp
    xorl %eax, %eax
    ret
.size   JaegerThrowpoline, . - JaegerThrowpoline

/ void JaegerInterpoline()
.global JaegerInterpoline
.type   JaegerInterpoline, @function
JaegerInterpoline:
    /* For Sun Studio there is no fast call. */
    /* We add the stack by 16 before. */
    addl $0x10, %esp
    /* Align the stack to 16 bytes. */
    pushl %esp
    pushl %eax
    pushl %edi
    pushl %esi
    call js_InternalInterpret
    addl $0x10, %esp
    movl 0x1C(%esp), %ebp    /* Load frame */
    movl 0x18(%ebp), %esi    /* Load rval payload */
    movl 0x1C(%ebp), %edi    /* Load rval type */
    movl 0xC(%esp), %ecx     /* Load scratch -> argc, for any scripted call */
    testl %eax, %eax
    je   interpoline_exit
    jmp  *%eax
interpoline_exit:
    pushl %esp
    call PopActiveVMFrame
    addl $0x40, %esp
    popl %ebx
    popl %edi
    popl %esi
    popl %ebp
    xorl %eax, %eax
    ret
.size   JaegerInterpoline, . - JaegerInterpoline

/ void JaegerInterpolineScripted()
.global JaegerInterpolineScripted
.type   JaegerInterpolineScripted, @function
JaegerInterpolineScripted:
    movl 0x10(%ebp), %ebp
    movl %ebp, 0x1C(%esp)
    subl $0x10, %esp
    jmp JaegerInterpoline
.size   JaegerInterpolineScripted, . - JaegerInterpolineScripted

/ void JaegerInterpolinePatched()
.global JaegerInterpolinePatched
.type   JaegerInterpolinePatched, @function
JaegerInterpolinePatched:
    subl $0x10, %esp
    jmp JaegerInterpoline
.size   JaegerInterpolinePatched, . - JaegerInterpolinePatched
