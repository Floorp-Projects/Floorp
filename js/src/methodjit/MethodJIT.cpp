/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MethodJIT.h"
#include "Logging.h"
#include "assembler/jit/ExecutableAllocator.h"
#include "assembler/assembler/RepatchBuffer.h"
#include "gc/Marking.h"
#include "js/MemoryMetrics.h"
#include "BaseAssembler.h"
#include "Compiler.h"
#include "MonoIC.h"
#include "PolyIC.h"
#include "TrampolineCompiler.h"
#include "jscntxtinlines.h"
#include "jscompartment.h"
#include "jsscope.h"

#include "jsgcinlines.h"
#include "jsinterpinlines.h"

using namespace js;
using namespace js::mjit;

#ifdef __GCC_HAVE_DWARF2_CFI_ASM
# define CFI(str) str
#else
# define CFI(str)
#endif

// Put manually-inserted call frame unwinding information into .debug_frame
// rather than .eh_frame, because we compile with -fno-exceptions which might
// discard the .eh_frame section. (See
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43232).
CFI(asm(".cfi_sections .debug_frame");)

js::mjit::CompilerAllocPolicy::CompilerAllocPolicy(JSContext *cx, Compiler &compiler)
: TempAllocPolicy(cx),
  oomFlag(&compiler.oomInVector)
{
}
void
StackFrame::methodjitStaticAsserts()
{
        /* Static assert for x86 trampolines in MethodJIT.cpp. */
#if defined(JS_CPU_X86)
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_)     == 0x18);
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_) + 4 == 0x1C);
        JS_STATIC_ASSERT(offsetof(StackFrame, ncode_)    == 0x14);
        /* ARM uses decimal literals. */
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_)     == 24);
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_) + 4 == 28);
        JS_STATIC_ASSERT(offsetof(StackFrame, ncode_)    == 20);
#elif defined(JS_CPU_X64)
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_)     == 0x30);
        JS_STATIC_ASSERT(offsetof(StackFrame, ncode_)    == 0x28);
#endif
}

/*
 * Explanation of VMFrame activation and various helper thunks below.
 *
 * JaegerTrampoline  - Executes a method JIT-compiled JSFunction. This function
 *    creates a VMFrame on the machine stack and jumps into JIT'd code. The JIT'd
 *    code will eventually jump back to JaegerTrampolineReturn, clean up the
 *    VMFrame and return into C++.
 *
 *  - Called from C++ function EnterMethodJIT.
 *  - Parameters: cx, fp, code, stackLimit
 *
 * JaegerThrowpoline - Calls into an exception handler from JIT'd code, and if a
 *    scripted exception handler is not found, unwinds the VMFrame and returns
 *    to C++.
 *
 *  - To start exception handling, we return from a stub call to the throwpoline.
 *  - On entry to the throwpoline, the normal conditions of the jit-code ABI
 *    are satisfied.
 *  - To do the unwinding and find out where to continue executing, we call
 *    js_InternalThrow.
 *  - js_InternalThrow may return 0, which means the place to continue, if any,
 *    is above this JaegerShot activation, so we just return, in the same way
 *    the trampoline does.
 *  - Otherwise, js_InternalThrow returns a jit-code address to continue execution
 *    at. Because the jit-code ABI conditions are satisfied, we can just jump to
 *    that point.
 *
 * JaegerInterpoline - After returning from a stub or scripted call made by JIT'd
 *    code, calls into Interpret and has it finish execution of the JIT'd script.
 *    If we have to throw away the JIT code for a script for some reason (either
 *    a new trap is added for debug code, or assumptions made by the JIT code
 *    have broken and forced its invalidation), the call returns into the
 *    Interpoline which calls Interpret to finish the JIT frame. The Interpret
 *    call may eventually recompile the script, in which case it will join into
 *    that code with a new VMFrame activation and JaegerTrampoline.
 *
 *  - Returned into from stub calls originally made from JIT code.
 *  - An alternate version, JaegerInterpolineScripted, returns from scripted
 *    calls originally made from JIT code, and fixes up state to match the
 *    stub call ABI.
 */

#ifdef JS_METHODJIT_PROFILE_STUBS
static const size_t STUB_CALLS_FOR_OP_COUNT = 255;
static uint32_t StubCallsForOp[STUB_CALLS_FOR_OP_COUNT];
#endif

// Called from JaegerTrampoline only
extern "C" void JS_FASTCALL
PushActiveVMFrame(VMFrame &f)
{
    f.oldregs = &f.cx->stack.regs();
    f.cx->stack.repointRegs(&f.regs);
    f.cx->jaegerRuntime().pushActiveFrame(&f);
    f.entryfp->setNativeReturnAddress(JS_FUNC_TO_DATA_PTR(void*, JaegerTrampolineReturn));
    f.regs.clearInlined();
}

// Called from JaegerTrampolineReturn, JaegerThrowpoline, JaegerInterpoline
extern "C" void JS_FASTCALL
PopActiveVMFrame(VMFrame &f)
{
    f.cx->jaegerRuntime().popActiveFrame();
    f.cx->stack.repointRegs(f.oldregs);
}

#if defined(__APPLE__) || (defined(XP_WIN) && !defined(JS_CPU_X64)) || defined(XP_OS2)
# define SYMBOL_STRING(name) "_" #name
#else
# define SYMBOL_STRING(name) #name
#endif

JS_STATIC_ASSERT(offsetof(FrameRegs, sp) == 0);

#if defined(__linux__) && defined(JS_CPU_X64)
# define SYMBOL_STRING_RELOC(name) #name "@plt"
#else
# define SYMBOL_STRING_RELOC(name) SYMBOL_STRING(name)
#endif

#if (defined(XP_WIN) || defined(XP_OS2)) && defined(JS_CPU_X86)
# define SYMBOL_STRING_VMFRAME(name) "@" #name "@4"
#else
# define SYMBOL_STRING_VMFRAME(name) SYMBOL_STRING_RELOC(name)
#endif

#if defined(XP_MACOSX)
# define HIDE_SYMBOL(name) ".private_extern _" #name
#elif defined(__linux__)
# define HIDE_SYMBOL(name) ".hidden" #name
#else
# define HIDE_SYMBOL(name)
#endif

/*
 * Notes about DWARF Call Frame Information (CFI) annotations:
 *
 * A .cfi directive placed in assembly code describes how to recover the
 * caller's registers when control is at or after that directive. That is,
 * they describe the states that hold between one instruction and the next,
 * not the instructions themselves. Later directives override earlier
 * directives.
 *
 * In DWARF CFI, each stack frame has a Canonical Frame Address (CFA) that
 * remains constant throughout the frame's lifetime. Exactly where it is is
 * a matter of convention; on the x86 and x86_64, for example, the CFA
 * points just after the end of the current stack frame: the address of the
 * next word after the return address. The CFI annotations describe 1) how
 * to compute the CFA at each point in the function, and 2) given the CFA,
 * where the caller's value of each register has been saved. (CFI specifies
 * saved registers' locations relative to the CFA, instead of the stack
 * pointer, so that when we push or pop the stack, we need only adjust our
 * rule for computing the CFA, not the rule for each saved register.)
 *
 * Quick reference:
 *
 * .cfi_startproc, .cfi_endproc
 *   Put these at the beginning and end of the block of code you're
 *   annotating.
 *
 * (The following directives apply starting at the point they appear until
 * they are overridden or until the .cfi_endproc.)
 *
 * .cfi_def_cfa REGISTER, OFFSET
 *   The CFA is the value of REGISTER plus OFFSET.
 *
 * .cfi_def_cfa_offset OFFSET
 *   The CFA is the value of the same register as before, but now adding OFFSET.
 *
 * .cfi_def_cfa_register REGISTER
 *   The CFA is now the value of REGISTER, adding the same offset as before.
 *
 * .cfi_offset REGISTER, OFFSET
 *   The caller's value of REGISTER is saved at OFFSET from the current CFA.
 *   (This is the directive that actually says something interesting.)
 *
 * There are other directives that compute the CFA, a saved register's address,
 * or a saved register's value, in more complex ways, but the above are the ones
 * we use here.
 *
 * Special rules for JaegerThrowpoline and friends:
 *
 * In ordinary code, return addresses always point directly after a call
 * instruction. When GDB looks up the CFI for a return address it got from the
 * stack (as opposed to the current PC), it uses the CFI just before the return
 * address --- the CFI associated with the call instruction --- to do the
 * unwinding. However, JaegerMonkey uses hacks that edit return addresses to
 * point directly at the first instruction of JaegerThrowpoline,
 * JaegerInterpoline, and their ilk, so GDB ends up trying to use the CFI
 * associated with whatever instruction lies immediately *before* the given
 * entry point.
 *
 * We make sure our CFI covers the code address GDB will actually use, by
 * placing a 'nop' *before* the entry point --- it is never executed --- and
 * having our CFI apply starting at that nop.
 */

#if defined(__GNUC__) && !defined(_WIN64)

/* If this assert fails, you need to realign VMFrame to 16 bytes. */
#if defined(JS_CPU_ARM) || defined(JS_CPU_MIPS) || defined(JS_CPU_SPARC)
JS_STATIC_ASSERT(sizeof(VMFrame) % 8 == 0);
#else
JS_STATIC_ASSERT(sizeof(VMFrame) % 16 == 0);
#endif

# if defined(JS_CPU_X64)

/*
 *    *** DANGER ***
 * If these assertions break, update the constants below.
 *    *** DANGER ***
 */
JS_STATIC_ASSERT(offsetof(VMFrame, savedRBX) == 0x68);
JS_STATIC_ASSERT(offsetof(VMFrame, scratch) == 0x18);
JS_STATIC_ASSERT(VMFrame::offsetOfFp == 0x38);

JS_STATIC_ASSERT(JSVAL_TAG_MASK == 0xFFFF800000000000LL);
JS_STATIC_ASSERT(JSVAL_PAYLOAD_MASK == 0x00007FFFFFFFFFFFLL);

asm (
".text\n"
".globl " SYMBOL_STRING(JaegerTrampoline) "\n"
SYMBOL_STRING(JaegerTrampoline) ":"       "\n"
    /* Prologue. */
    CFI(".cfi_startproc"                 "\n")
    CFI(".cfi_def_cfa rsp, 8"            "\n")
    "pushq %rbp"                         "\n"
    CFI(".cfi_def_cfa_offset 16"         "\n")
    CFI(".cfi_offset rbp, -16"           "\n")
    "movq %rsp, %rbp"                    "\n"
    CFI(".cfi_def_cfa_register rbp"      "\n")
    /* Save non-volatile registers. */
    "pushq %r12"                         "\n"
    CFI(".cfi_offset r12, -24"           "\n")
    "pushq %r13"                         "\n"
    CFI(".cfi_offset r13, -32"           "\n")
    "pushq %r14"                         "\n"
    CFI(".cfi_offset r14, -40"           "\n")
    "pushq %r15"                         "\n"
    CFI(".cfi_offset r15, -48"           "\n")
    "pushq %rbx"                         "\n"
    CFI(".cfi_offset rbx, -56"           "\n")

    /* Load mask registers. */
    "movq $0xFFFF800000000000, %r13"     "\n"
    "movq $0x00007FFFFFFFFFFF, %r14"     "\n"

    /* Build the JIT frame.
     * rdi = cx
     * rsi = fp
     * rcx = inlineCallCount
     * fp must go into rbx
     */
    "pushq $0x0"                         "\n" /* stubRejoin */
    "pushq %rsi"                         "\n" /* entryncode */
    "pushq %rsi"                         "\n" /* entryfp */
    "pushq %rcx"                         "\n" /* inlineCallCount */
    "pushq %rdi"                         "\n" /* cx */
    "pushq %rsi"                         "\n" /* fp */
    "movq  %rsi, %rbx"                   "\n"

    /* Space for the rest of the VMFrame. */
    "subq  $0x28, %rsp"                  "\n"

    /* This is actually part of the VMFrame. */
    "pushq %r8"                          "\n"

    /* Set cx->regs and set the active frame. Save rdx and align frame in one. */
    "pushq %rdx"                         "\n"
    "movq  %rsp, %rdi"                   "\n"
    "call " SYMBOL_STRING_VMFRAME(PushActiveVMFrame) "\n"

    /* Jump into the JIT'd code. */
    "jmp *0(%rsp)"                      "\n"
    CFI(".cfi_endproc"                  "\n")
);

asm (
".text\n"
    /* See "Special rules for JaegerThrowpoline and friends", above. */
    CFI(".cfi_startproc"                 "\n")
    CFI(".cfi_def_cfa rbp, 16"           "\n")
    CFI(".cfi_offset rbp, -16"           "\n")
    CFI(".cfi_offset r12, -24"           "\n")
    CFI(".cfi_offset r13, -32"           "\n")
    CFI(".cfi_offset r14, -40"           "\n")
    CFI(".cfi_offset r15, -48"           "\n")
    CFI(".cfi_offset rbx, -56"           "\n")
    CFI("nop"                            "\n")
".globl " SYMBOL_STRING(JaegerTrampolineReturn) "\n"
SYMBOL_STRING(JaegerTrampolineReturn) ":"       "\n"
    "or   %rdi, %rsi"                    "\n"
    "movq %rsi, 0x30(%rbx)"              "\n"
    "movq %rsp, %rdi"                    "\n"
    "call " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"

    "addq $0x68, %rsp"                   "\n"
    "popq %rbx"                          "\n"
    "popq %r15"                          "\n"
    "popq %r14"                          "\n"
    "popq %r13"                          "\n"
    "popq %r12"                          "\n"
    "popq %rbp"                          "\n"
    CFI(".cfi_def_cfa rsp, 8"            "\n")
    "movq $1, %rax"                      "\n"
    "ret"                                "\n"
    CFI(".cfi_endproc"                   "\n")
);

asm (
".text\n"
    /* See "Special rules for JaegerThrowpoline and friends", above. */
    CFI(".cfi_startproc"                    "\n")
    CFI(".cfi_def_cfa rbp, 16"              "\n")
    CFI(".cfi_offset rbp, -16"              "\n")
    CFI(".cfi_offset r12, -24"              "\n")
    CFI(".cfi_offset r13, -32"              "\n")
    CFI(".cfi_offset r14, -40"              "\n")
    CFI(".cfi_offset r15, -48"              "\n")
    CFI(".cfi_offset rbx, -56"              "\n")
    CFI("nop"                               "\n")
".globl " SYMBOL_STRING(JaegerThrowpoline)  "\n"
SYMBOL_STRING(JaegerThrowpoline) ":"        "\n"
    "movq %rsp, %rdi"                       "\n"
    "call " SYMBOL_STRING_RELOC(js_InternalThrow) "\n"
    "testq %rax, %rax"                      "\n"
    "je   throwpoline_exit"                 "\n"
    "jmp  *%rax"                            "\n"
  "throwpoline_exit:"                       "\n"
    "movq %rsp, %rdi"                       "\n"
    "call " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"
    "addq $0x68, %rsp"                      "\n"
    "popq %rbx"                             "\n"
    "popq %r15"                             "\n"
    "popq %r14"                             "\n"
    "popq %r13"                             "\n"
    "popq %r12"                             "\n"
    "popq %rbp"                             "\n"
    CFI(".cfi_def_cfa rsp, 8"               "\n")
    "xorq %rax,%rax"                        "\n"
    "ret"                                   "\n"
    CFI(".cfi_endproc"                      "\n")
);

asm (
".text\n"
    /* See "Special rules for JaegerThrowpoline and friends", above. */
    CFI(".cfi_startproc"                    "\n")
    CFI(".cfi_def_cfa rbp, 16"              "\n")
    CFI(".cfi_offset rbp, -16"              "\n")
    CFI(".cfi_offset r12, -24"              "\n")
    CFI(".cfi_offset r13, -32"              "\n")
    CFI(".cfi_offset r14, -40"              "\n")
    CFI(".cfi_offset r15, -48"              "\n")
    CFI(".cfi_offset rbx, -56"              "\n")
    CFI("nop"                               "\n")
".globl " SYMBOL_STRING(JaegerInterpoline)  "\n"
SYMBOL_STRING(JaegerInterpoline) ":"        "\n"
    "movq %rsp, %rcx"                       "\n"
    "movq %rax, %rdx"                       "\n"
    "call " SYMBOL_STRING_RELOC(js_InternalInterpret) "\n"
    "movq 0x38(%rsp), %rbx"                 "\n" /* Load frame */
    "movq 0x30(%rbx), %rsi"                 "\n" /* Load rval payload */
    "and %r14, %rsi"                        "\n" /* Mask rval payload */
    "movq 0x30(%rbx), %rdi"                 "\n" /* Load rval type */
    "and %r13, %rdi"                        "\n" /* Mask rval type */
    "movq 0x18(%rsp), %rcx"                 "\n" /* Load scratch -> argc */
    "testq %rax, %rax"                      "\n"
    "je   interpoline_exit"                 "\n"
    "jmp  *%rax"                            "\n"
  "interpoline_exit:"                       "\n"
    "movq %rsp, %rdi"                       "\n"
    "call " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"
    "addq $0x68, %rsp"                      "\n"
    "popq %rbx"                             "\n"
    "popq %r15"                             "\n"
    "popq %r14"                             "\n"
    "popq %r13"                             "\n"
    "popq %r12"                             "\n"
    "popq %rbp"                             "\n"
    CFI(".cfi_def_cfa rsp, 8"               "\n")
    "xorq %rax,%rax"                        "\n"
    "ret"                                   "\n"
    CFI(".cfi_endproc"                      "\n")
);

asm (
".text\n"
    /* See "Special rules for JaegerThrowpoline and friends", above. */
    CFI(".cfi_startproc"                            "\n")
    CFI(".cfi_def_cfa rbp, 16"                      "\n")
    CFI(".cfi_offset rbp, -16"                      "\n")
    CFI(".cfi_offset r12, -24"                      "\n")
    CFI(".cfi_offset r13, -32"                      "\n")
    CFI(".cfi_offset r14, -40"                      "\n")
    CFI(".cfi_offset r15, -48"                      "\n")
    CFI(".cfi_offset rbx, -56"                      "\n")
    CFI("nop"                                       "\n")
".globl " SYMBOL_STRING(JaegerInterpolineScripted)  "\n"
SYMBOL_STRING(JaegerInterpolineScripted) ":"        "\n"
    "movq 0x20(%rbx), %rbx"                         "\n" /* load prev */
    "movq %rbx, 0x38(%rsp)"                         "\n"
    "jmp " SYMBOL_STRING_RELOC(JaegerInterpoline)   "\n"
    CFI(".cfi_endproc"                              "\n")
);

# elif defined(JS_CPU_X86)

/*
 *    *** DANGER ***
 * If these assertions break, update the constants below. The throwpoline
 * should have the offset of savedEBX plus 4, because it needs to clean
 * up the argument.
 *    *** DANGER ***
 */
JS_STATIC_ASSERT(offsetof(VMFrame, savedEBX) == 0x3C);
JS_STATIC_ASSERT(offsetof(VMFrame, scratch) == 0xC);
JS_STATIC_ASSERT(VMFrame::offsetOfFp == 0x1C);

asm (
".text\n"
".globl " SYMBOL_STRING(JaegerTrampoline) "\n"
SYMBOL_STRING(JaegerTrampoline) ":"       "\n"
    /* Prologue. */
    CFI(".cfi_startproc"                 "\n")
    CFI(".cfi_def_cfa esp, 4"            "\n")
    "pushl %ebp"                         "\n"
    CFI(".cfi_def_cfa_offset 8"          "\n")
    CFI(".cfi_offset ebp, -8"            "\n")
    "movl %esp, %ebp"                    "\n"
    CFI(".cfi_def_cfa_register ebp"      "\n")
    /* Save non-volatile registers. */
    "pushl %esi"                         "\n"
    CFI(".cfi_offset esi, -12"           "\n")
    "pushl %edi"                         "\n"
    CFI(".cfi_offset edi, -16"           "\n")
    "pushl %ebx"                         "\n"
    CFI(".cfi_offset ebx, -20"           "\n")

    /* Build the JIT frame. Push fields in order,
     * then align the stack to form esp == VMFrame. */
    "movl  12(%ebp), %ebx"               "\n"   /* load fp */
    "pushl %ebx"                         "\n"   /* unused1 */
    "pushl %ebx"                         "\n"   /* unused0 */
    "pushl $0x0"                         "\n"   /* stubRejoin */
    "pushl %ebx"                         "\n"   /* entryncode */
    "pushl %ebx"                         "\n"   /* entryfp */
    "pushl 20(%ebp)"                     "\n"   /* stackLimit */
    "pushl 8(%ebp)"                      "\n"   /* cx */
    "pushl %ebx"                         "\n"   /* fp */
    "subl $0x1C, %esp"                   "\n"

    /* Jump into the JIT'd code. */
    "movl  %esp, %ecx"                   "\n"
    "call " SYMBOL_STRING_VMFRAME(PushActiveVMFrame) "\n"

    "movl 28(%esp), %ebp"                "\n"   /* load fp for JIT code */
    "jmp *88(%esp)"                      "\n"
    CFI(".cfi_endproc"                   "\n")
);

asm (
".text\n"
    /* See "Special rules for JaegerThrowpoline and friends", above. */
    CFI(".cfi_startproc"                 "\n")
    CFI(".cfi_def_cfa ebp, 8"            "\n")
    CFI(".cfi_offset ebp, -8"            "\n")
    CFI(".cfi_offset esi, -12"           "\n")
    CFI(".cfi_offset edi, -16"           "\n")
    CFI(".cfi_offset ebx, -20"           "\n")
    CFI("nop"                            "\n")
".globl " SYMBOL_STRING(JaegerTrampolineReturn) "\n"
SYMBOL_STRING(JaegerTrampolineReturn) ":" "\n"
    "movl  %esi, 0x18(%ebp)"             "\n"
    "movl  %edi, 0x1C(%ebp)"             "\n"
    "movl  %esp, %ebp"                   "\n"
    "addl  $0x48, %ebp"                  "\n" /* Restore stack at STACK_BASE_DIFFERENCE */
    "movl  %esp, %ecx"                   "\n"
    "call " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"

    "addl $0x3C, %esp"                   "\n"
    "popl %ebx"                          "\n"
    "popl %edi"                          "\n"
    "popl %esi"                          "\n"
    "popl %ebp"                          "\n"
    CFI(".cfi_def_cfa esp, 4"            "\n")
    "movl $1, %eax"                      "\n"
    "ret"                                "\n"
    CFI(".cfi_endproc"                   "\n")
);

asm (
".text\n"
    /* See "Special rules for JaegerThrowpoline and friends", above. */
    CFI(".cfi_startproc"                 "\n")
    CFI(".cfi_def_cfa ebp, 8"            "\n")
    CFI(".cfi_offset ebp, -8"            "\n")
    CFI(".cfi_offset esi, -12"           "\n")
    CFI(".cfi_offset edi, -16"           "\n")
    CFI(".cfi_offset ebx, -20"           "\n")
    CFI("nop"                            "\n")
".globl " SYMBOL_STRING(JaegerThrowpoline)  "\n"
SYMBOL_STRING(JaegerThrowpoline) ":"        "\n"
    /* Align the stack to 16 bytes. */
    "pushl %esp"                         "\n"
    "pushl (%esp)"                       "\n"
    "pushl (%esp)"                       "\n"
    "pushl (%esp)"                       "\n"
    "call " SYMBOL_STRING_RELOC(js_InternalThrow) "\n"
    /* Bump the stack by 0x2c, as in the basic trampoline, but
     * also one more word to clean up the stack for js_InternalThrow,
     * and another to balance the alignment above. */
    "addl $0x10, %esp"                   "\n"
    "testl %eax, %eax"                   "\n"
    "je   throwpoline_exit"              "\n"
    "jmp  *%eax"                         "\n"
  "throwpoline_exit:"                    "\n"
    "movl %esp, %ecx"                    "\n"
    "call " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"
    "addl $0x3c, %esp"                   "\n"
    "popl %ebx"                          "\n"
    "popl %edi"                          "\n"
    "popl %esi"                          "\n"
    "popl %ebp"                          "\n"
    CFI(".cfi_def_cfa esp, 4"            "\n")
    "xorl %eax, %eax"                    "\n"
    "ret"                                "\n"
    CFI(".cfi_endproc"                   "\n")
);

asm (
".text\n"
    /* See "Special rules for JaegerThrowpoline and friends", above. */
    CFI(".cfi_startproc"                 "\n")
    CFI(".cfi_def_cfa ebp, 8"            "\n")
    CFI(".cfi_offset ebp, -8"            "\n")
    CFI(".cfi_offset esi, -12"           "\n")
    CFI(".cfi_offset edi, -16"           "\n")
    CFI(".cfi_offset ebx, -20"           "\n")
    CFI("nop"                            "\n")
".globl " SYMBOL_STRING(JaegerInterpoline)  "\n"
SYMBOL_STRING(JaegerInterpoline) ":"        "\n"
    /* Align the stack to 16 bytes. */
    "pushl %esp"                         "\n"
    "pushl %eax"                         "\n"
    "pushl %edi"                         "\n"
    "pushl %esi"                         "\n"
    "call " SYMBOL_STRING_RELOC(js_InternalInterpret) "\n"
    "addl $0x10, %esp"                   "\n"
    "movl 0x1C(%esp), %ebp"              "\n" /* Load frame */
    "movl 0x18(%ebp), %esi"              "\n" /* Load rval payload */
    "movl 0x1C(%ebp), %edi"              "\n" /* Load rval type */
    "movl 0xC(%esp), %ecx"               "\n" /* Load scratch -> argc, for any scripted call */
    "testl %eax, %eax"                   "\n"
    "je   interpoline_exit"              "\n"
    "jmp  *%eax"                         "\n"
  "interpoline_exit:"                    "\n"
    "movl %esp, %ecx"                    "\n"
    "call " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"
    "addl $0x3c, %esp"                   "\n"
    "popl %ebx"                          "\n"
    "popl %edi"                          "\n"
    "popl %esi"                          "\n"
    "popl %ebp"                          "\n"
    CFI(".cfi_def_cfa esp, 4"            "\n")
    "xorl %eax, %eax"                    "\n"
    "ret"                                "\n"
    CFI(".cfi_endproc"                   "\n")
);

asm (
".text\n"
    /* See "Special rules for JaegerThrowpoline and friends", above. */
    CFI(".cfi_startproc"                            "\n")
    CFI(".cfi_def_cfa ebp, 8"                       "\n")
    CFI(".cfi_offset ebp, -8"                       "\n")
    CFI(".cfi_offset esi, -12"                      "\n")
    CFI(".cfi_offset edi, -16"                      "\n")
    CFI(".cfi_offset ebx, -20"                      "\n")
    CFI("nop"                                       "\n")
".globl " SYMBOL_STRING(JaegerInterpolineScripted)  "\n"
SYMBOL_STRING(JaegerInterpolineScripted) ":"        "\n"
    "movl 0x10(%ebp), %ebp"                         "\n" /* load prev. :XXX: STATIC_ASSERT this */
    "movl  %ebp, 0x1C(%esp)"                        "\n"
    "jmp " SYMBOL_STRING_RELOC(JaegerInterpoline)   "\n"
    CFI(".cfi_endproc"                              "\n")
);

# elif defined(JS_CPU_ARM)

JS_STATIC_ASSERT(sizeof(VMFrame) == 88);
JS_STATIC_ASSERT(sizeof(VMFrame)%8 == 0);   /* We need 8-byte stack alignment for EABI. */
JS_STATIC_ASSERT(offsetof(VMFrame, savedLR) ==          (4*21));
JS_STATIC_ASSERT(offsetof(VMFrame, entryfp) ==          (4*10));
JS_STATIC_ASSERT(offsetof(VMFrame, stackLimit) ==       (4*9));
JS_STATIC_ASSERT(offsetof(VMFrame, cx) ==               (4*8));
JS_STATIC_ASSERT(VMFrame::offsetOfFp ==                 (4*7));
JS_STATIC_ASSERT(offsetof(VMFrame, scratch) ==          (4*3));
JS_STATIC_ASSERT(offsetof(VMFrame, previous) ==         (4*2));

JS_STATIC_ASSERT(JSFrameReg == JSC::ARMRegisters::r10);
JS_STATIC_ASSERT(JSReturnReg_Type == JSC::ARMRegisters::r5);
JS_STATIC_ASSERT(JSReturnReg_Data == JSC::ARMRegisters::r4);

#ifdef MOZ_THUMB2
#define FUNCTION_HEADER_EXTRA \
  ".align 2\n" \
  ".thumb\n" \
  ".thumb_func\n"
#else
#define FUNCTION_HEADER_EXTRA
#endif

asm (
".text\n"
FUNCTION_HEADER_EXTRA
".globl " SYMBOL_STRING(JaegerTrampoline)   "\n"
SYMBOL_STRING(JaegerTrampoline) ":"         "\n"
    /*
     * On entry to JaegerTrampoline:
     *         r0 = cx
     *         r1 = fp
     *         r2 = code
     *         r3 = stackLimit
     *
     * The VMFrame for ARM looks like this:
     *  [ lr           ]   \
     *  [ r11          ]   |
     *  [ r10          ]   |
     *  [ r9           ]   | Callee-saved registers.
     *  [ r8           ]   | VFP registers d8-d15 may be required here too, but
     *  [ r7           ]   | unconditionally preserving them might be expensive
     *  [ r6           ]   | considering that we might not use them anyway.
     *  [ r5           ]   |
     *  [ r4           ]   /
     *  [ stubRejoin   ]
     *  [ entryncode   ]
     *  [ entryfp      ]
     *  [ stkLimit     ]
     *  [ cx           ]
     *  [ regs.fp      ]
     *  [ regs.inlined ]
     *  [ regs.pc      ]
     *  [ regs.sp      ]
     *  [ scratch      ]
     *  [ previous     ]
     *  [ args.ptr2    ]  [ dynamicArgc ]  (union)
     *  [ args.ptr     ]  [ lazyArgsObj ]  (union)
     */

    /* Push callee-saved registers. */
"   push    {r4-r11,lr}"                        "\n"
    /* Push interesting VMFrame content. */
"   mov     ip, #0"                             "\n"
"   push    {ip}"                               "\n"    /* stubRejoin */
"   push    {r1}"                               "\n"    /* entryncode */
"   push    {r1}"                               "\n"    /* entryfp */
"   push    {r3}"                               "\n"    /* stackLimit */
"   push    {r0}"                               "\n"    /* cx */
"   push    {r1}"                               "\n"    /* regs.fp */
    /* Remaining fields are set elsewhere, but we need to leave space for them. */
"   sub     sp, sp, #(4*7)"                     "\n"

    /* Preserve 'code' (r2) in an arbitrary callee-saved register. */
"   mov     r4, r2"                             "\n"
    /* Preserve 'fp' (r1) in r10 (JSFrameReg). */
"   mov     r10, r1"                            "\n"

"   mov     r0, sp"                             "\n"
"   blx  " SYMBOL_STRING_VMFRAME(PushActiveVMFrame)"\n"

    /* Call the compiled JavaScript function. */
"   bx     r4"                                  "\n"
);

asm (
".text\n"
FUNCTION_HEADER_EXTRA
".globl " SYMBOL_STRING(JaegerTrampolineReturn)   "\n"
SYMBOL_STRING(JaegerTrampolineReturn) ":"         "\n"
"   strd    r4, r5, [r10, #24]"             "\n" /* fp->rval type,data */

    /* Tidy up. */
"   mov     r0, sp"                         "\n"
"   blx  " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"

    /* Skip past the parameters we pushed (such as cx and the like). */
"   add     sp, sp, #(4*7 + 4*6)"           "\n"

    /* Set a 'true' return value to indicate successful completion. */
"   mov     r0, #1"                         "\n"
"   pop     {r4-r11,pc}"                    "\n"
);

asm (
".text\n"
FUNCTION_HEADER_EXTRA
".globl " SYMBOL_STRING(JaegerThrowpoline)  "\n"
SYMBOL_STRING(JaegerThrowpoline) ":"        "\n"
    /* Find the VMFrame pointer for js_InternalThrow. */
"   mov     r0, sp"                         "\n"

    /* Call the utility function that sets up the internal throw routine. */
"   blx  " SYMBOL_STRING_RELOC(js_InternalThrow) "\n"

    /* If js_InternalThrow found a scripted handler, jump to it. Otherwise, tidy
     * up and return. */
"   cmp     r0, #0"                         "\n"
"   it      ne"                             "\n"
"   bxne    r0"                             "\n"

    /* Tidy up, then return '0' to represent an unhandled exception. */
"   mov     r0, sp"                         "\n"
"   blx  " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"
"   add     sp, sp, #(4*7 + 4*6)"           "\n"
"   mov     r0, #0"                         "\n"
"   pop     {r4-r11,pc}"                    "\n"
);

asm (
".text\n"
FUNCTION_HEADER_EXTRA
".globl " SYMBOL_STRING(JaegerInterpolineScripted)  "\n"
SYMBOL_STRING(JaegerInterpolineScripted) ":"        "\n"
    /* The only difference between JaegerInterpoline and JaegerInpolineScripted is that the
     * scripted variant has to walk up to the previous StackFrame first. */
"   ldr     r10, [r10, #(4*4)]"             "\n"    /* Load f->prev_ */
"   str     r10, [sp, #(4*7)]"              "\n"    /* Update f->regs->fp_ */
    /* Fall through into JaegerInterpoline. */

FUNCTION_HEADER_EXTRA
".globl " SYMBOL_STRING(JaegerInterpoline)  "\n"
SYMBOL_STRING(JaegerInterpoline) ":"        "\n"
"   mov     r3, sp"                         "\n"    /* f */
"   mov     r2, r0"                         "\n"    /* returnReg */
"   mov     r1, r5"                         "\n"    /* returnType */
"   mov     r0, r4"                         "\n"    /* returnData */
"   blx  " SYMBOL_STRING_RELOC(js_InternalInterpret) "\n"
"   cmp     r0, #0"                         "\n"
"   ldr     r10, [sp, #(4*7)]"              "\n"    /* Load (StackFrame*)f->regs->fp_ */
"   ldrd    r4, r5, [r10, #(4*6)]"          "\n"    /* Load rval payload and type. */
"   ldr     r1, [sp, #(4*3)]"               "\n"    /* Load scratch. */
"   it      ne"                             "\n"
"   bxne    r0"                             "\n"
    /* Tidy up, then return 0. */
"   mov     r0, sp"                         "\n"
"   blx  " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"
"   add     sp, sp, #(4*7 + 4*6)"           "\n"
"   mov     r0, #0"                         "\n"
"   pop     {r4-r11,pc}"                    "\n"
);

asm (
".text\n"
FUNCTION_HEADER_EXTRA
".globl " SYMBOL_STRING(JaegerStubVeneer)   "\n"
SYMBOL_STRING(JaegerStubVeneer) ":"         "\n"
    /* We enter this function as a veneer between a compiled method and one of the js_ stubs. We
     * need to store the LR somewhere (so it can be modified in case on an exception) and then
     * branch to the js_ stub as if nothing had happened.
     * The arguments are identical to those for js_* except that the target function should be in
     * 'ip'. */
"   push    {ip,lr}"                        "\n"
"   blx     ip"                             "\n"
"   pop     {ip,pc}"                        "\n"
);

# elif defined(JS_CPU_SPARC)
# elif defined(JS_CPU_MIPS)
# else
#  error "Unsupported CPU!"
# endif
#elif defined(_MSC_VER) && defined(JS_CPU_X86)

/*
 *    *** DANGER ***
 * If these assertions break, update the constants below. The throwpoline
 * should have the offset of savedEBX plus 4, because it needs to clean
 * up the argument.
 *    *** DANGER ***
 */
JS_STATIC_ASSERT(offsetof(VMFrame, savedEBX) == 0x3C);
JS_STATIC_ASSERT(offsetof(VMFrame, scratch) == 0xC);
JS_STATIC_ASSERT(VMFrame::offsetOfFp == 0x1C);

extern "C" {

    __declspec(naked) JSBool JaegerTrampoline(JSContext *cx, StackFrame *fp, void *code,
                                              Value *stackLimit)
    {
        __asm {
            /* Prologue. */
            push ebp;
            mov ebp, esp;
            /* Save non-volatile registers. */
            push esi;
            push edi;
            push ebx;

            /* Build the JIT frame. Push fields in order,
             * then align the stack to form esp == VMFrame. */
            mov  ebx, [ebp + 12];
            push ebx;
            push ebx;
            push 0x0;
            push ebx;
            push ebx;
            push [ebp + 20];
            push [ebp + 8];
            push ebx;
            sub  esp, 0x1C;

            /* Jump into into the JIT'd code. */
            mov  ecx, esp;
            call PushActiveVMFrame;

            mov ebp, [esp + 28];  /* load fp for JIT code */
            jmp dword ptr [esp + 88];
        }
    }

    __declspec(naked) void JaegerTrampolineReturn()
    {
        __asm {
            mov [ebp + 0x18], esi;
            mov [ebp + 0x1C], edi;
            mov  ebp, esp;
            add  ebp, 0x48; /* Restore stack at STACK_BASE_DIFFERENCE */
            mov  ecx, esp;
            call PopActiveVMFrame;

            add esp, 0x3C;

            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            mov eax, 1;
            ret;
        }
    }

    extern "C" void *js_InternalThrow(js::VMFrame &f);

    __declspec(naked) void *JaegerThrowpoline(js::VMFrame *vmFrame) {
        __asm {
            /* Align the stack to 16 bytes. */
            push esp;
            push [esp];
            push [esp];
            push [esp];
            call js_InternalThrow;
            /* Bump the stack by 0x2c, as in the basic trampoline, but
             * also one more word to clean up the stack for js_InternalThrow,
             * and another to balance the alignment above. */
            add esp, 0x10;
            test eax, eax;
            je throwpoline_exit;
            jmp eax;
        throwpoline_exit:
            mov ecx, esp;
            call PopActiveVMFrame;
            add esp, 0x3c;
            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            xor eax, eax
            ret;
        }
    }

    extern "C" void *
    js_InternalInterpret(void *returnData, void *returnType, void *returnReg, js::VMFrame &f);

    __declspec(naked) void JaegerInterpoline() {
        __asm {
            /* Align the stack to 16 bytes. */
            push esp;
            push eax;
            push edi;
            push esi;
            call js_InternalInterpret;
            add esp, 0x10;
            mov ebp, [esp + 0x1C];  /* Load frame */
            mov esi, [ebp + 0x18];  /* Load rval payload */
            mov edi, [ebp + 0x1C];  /* Load rval type */
            mov ecx, [esp + 0xC];   /* Load scratch -> argc */
            test eax, eax;
            je interpoline_exit;
            jmp eax;
        interpoline_exit:
            mov ecx, esp;
            call PopActiveVMFrame;
            add esp, 0x3c;
            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            xor eax, eax
            ret;
        }
    }

    __declspec(naked) void JaegerInterpolineScripted() {
        __asm {
            mov ebp, [ebp + 0x10];  /* Load prev */
            mov [esp + 0x1C], ebp;  /* fp -> regs.fp */
            jmp JaegerInterpoline;
        }
    }
}

// Windows x64 uses assembler version since compiler doesn't support
// inline assembler
#elif defined(_WIN64)

/*
 *    *** DANGER ***
 * If these assertions break, update the constants below.
 *    *** DANGER ***
 */
JS_STATIC_ASSERT(offsetof(VMFrame, savedRBX) == 0x68);
JS_STATIC_ASSERT(offsetof(VMFrame, scratch) == 0x18);
JS_STATIC_ASSERT(VMFrame::offsetOfFp == 0x38);
JS_STATIC_ASSERT(JSVAL_TAG_MASK == 0xFFFF800000000000LL);
JS_STATIC_ASSERT(JSVAL_PAYLOAD_MASK == 0x00007FFFFFFFFFFFLL);

#endif                   /* _WIN64 */

JaegerRuntime::JaegerRuntime()
    : orphanedNativeFrames(SystemAllocPolicy()), orphanedNativePools(SystemAllocPolicy())
{}

bool
JaegerRuntime::init(JSContext *cx)
{
    JSC::ExecutableAllocator *execAlloc = cx->runtime->getExecAlloc(cx);
    if (!execAlloc)
        return false;

    TrampolineCompiler tc(execAlloc, &trampolines);
    if (!tc.compile())
        return false;

#ifdef JS_METHODJIT_PROFILE_STUBS
    for (size_t i = 0; i < STUB_CALLS_FOR_OP_COUNT; ++i)
        StubCallsForOp[i] = 0;
#endif

    activeFrame_ = NULL;
    lastUnfinished_ = (JaegerStatus) 0;

    return true;
}

void
JaegerRuntime::finish()
{
    TrampolineCompiler::release(&trampolines);
#ifdef JS_METHODJIT_PROFILE_STUBS
    FILE *fp = fopen("/tmp/stub-profiling", "wt");
# define OPDEF(op,val,name,image,length,nuses,ndefs,prec,format) \
    fprintf(fp, "%03d %s %d\n", val, #op, StubCallsForOp[val]);
# include "jsopcode.tbl"
# undef OPDEF
    fclose(fp);
#endif
}

extern "C" JSBool
JaegerTrampoline(JSContext *cx, StackFrame *fp, void *code, Value *stackLimit);

JaegerStatus
mjit::EnterMethodJIT(JSContext *cx, StackFrame *fp, void *code, Value *stackLimit, bool partial)
{
#ifdef JS_METHODJIT_SPEW
    Profiler prof;
    JSScript *script = fp->script();

    JaegerSpew(JSpew_Prof, "%s jaeger script, line %d\n",
               script->filename, script->lineno);
    prof.start();
#endif

    JS_ASSERT(cx->fp() == fp);

    JSBool ok;
    {
        AssertCompartmentUnchanged pcc(cx);
        JSAutoResolveFlags rf(cx, RESOLVE_INFER);
        ok = JaegerTrampoline(cx, fp, code, stackLimit);
    }

#ifdef JS_METHODJIT_SPEW
    prof.stop();
    JaegerSpew(JSpew_Prof, "script run took %d ms\n", prof.time_ms());
#endif

    JaegerStatus status = cx->jaegerRuntime().lastUnfinished();
    if (status) {
        if (partial) {
            /*
             * Being called from the interpreter, which will resume execution
             * where the JIT left off.
             */
            return status;
        }

        /*
         * Call back into the interpreter to finish the initial frame. This may
         * invoke EnterMethodJIT again, but will allow partial execution for
         * that recursive invocation, so we can have at most two VM frames for
         * a range of inline frames.
         */
        InterpMode mode = (status == Jaeger_UnfinishedAtTrap)
            ? JSINTERP_SKIP_TRAP
            : JSINTERP_REJOIN;
        ok = Interpret(cx, fp, mode);

        return ok ? Jaeger_Returned : Jaeger_Throwing;
    }

    cx->regs().refreshFramePointer(fp);
    cx->regs().setToEndOfScript();

    /* The entry frame should have finished. */
    JS_ASSERT(fp == cx->fp());

    if (ok) {
        /* The trampoline wrote the return value but did not set the HAS_RVAL flag. */
        fp->markReturnValue();
    }

    /* See comment in mjit::Compiler::emitReturn. */
    if (fp->isFunctionFrame())
        fp->updateEpilogueFlags();

    return ok ? Jaeger_Returned : Jaeger_Throwing;
}

static inline JaegerStatus
CheckStackAndEnterMethodJIT(JSContext *cx, StackFrame *fp, void *code, bool partial)
{
    JS_CHECK_RECURSION(cx, return Jaeger_ThrowBeforeEnter);

    JS_ASSERT(!cx->compartment->activeAnalysis);
    JS_ASSERT(code);

    Value *stackLimit = cx->stack.space().getStackLimit(cx, REPORT_ERROR);
    if (!stackLimit)
        return Jaeger_ThrowBeforeEnter;

    return EnterMethodJIT(cx, fp, code, stackLimit, partial);
}

JaegerStatus
mjit::JaegerShot(JSContext *cx, bool partial)
{
    StackFrame *fp = cx->fp();
    JSScript *script = fp->script();
    JITScript *jit = script->getJIT(fp->isConstructing(), cx->compartment->needsBarrier());

    JS_ASSERT(cx->regs().pc == script->code);

    return CheckStackAndEnterMethodJIT(cx, cx->fp(), jit->invokeEntry, partial);
}

JaegerStatus
js::mjit::JaegerShotAtSafePoint(JSContext *cx, void *safePoint, bool partial)
{
    return CheckStackAndEnterMethodJIT(cx, cx->fp(), safePoint, partial);
}

NativeMapEntry *
JITChunk::nmap() const
{
    return (NativeMapEntry *)((char*)this + sizeof(*this));
}

js::mjit::InlineFrame *
JITChunk::inlineFrames() const
{
    return (js::mjit::InlineFrame *)((char *)nmap() + sizeof(NativeMapEntry) * nNmapPairs);
}

js::mjit::CallSite *
JITChunk::callSites() const
{
    return (js::mjit::CallSite *)&inlineFrames()[nInlineFrames];
}

JSObject **
JITChunk::rootedTemplates() const
{
    return (JSObject **)&callSites()[nCallSites];
}

RegExpShared **
JITChunk::rootedRegExps() const
{
    return (RegExpShared **)&rootedTemplates()[nRootedTemplates];
}

char *
JITChunk::commonSectionLimit() const
{
    return (char *)&rootedRegExps()[nRootedRegExps];
}

#ifdef JS_MONOIC
ic::GetGlobalNameIC *
JITChunk::getGlobalNames() const
{
    return (ic::GetGlobalNameIC *) commonSectionLimit();
}

ic::SetGlobalNameIC *
JITChunk::setGlobalNames() const
{
    return (ic::SetGlobalNameIC *)((char *)getGlobalNames() +
            sizeof(ic::GetGlobalNameIC) * nGetGlobalNames);
}

ic::CallICInfo *
JITChunk::callICs() const
{
    return (ic::CallICInfo *)&setGlobalNames()[nSetGlobalNames];
}

ic::EqualityICInfo *
JITChunk::equalityICs() const
{
    return (ic::EqualityICInfo *)&callICs()[nCallICs];
}

char *
JITChunk::monoICSectionsLimit() const
{
    return (char *)&equalityICs()[nEqualityICs];
}
#else   // JS_MONOIC
char *
JITChunk::monoICSectionsLimit() const
{
    return commonSectionLimit();
}
#endif  // JS_MONOIC

#ifdef JS_POLYIC
ic::GetElementIC *
JITChunk::getElems() const
{
    return (ic::GetElementIC *)monoICSectionsLimit();
}

ic::SetElementIC *
JITChunk::setElems() const
{
    return (ic::SetElementIC *)((char *)getElems() + sizeof(ic::GetElementIC) * nGetElems);
}

ic::PICInfo *
JITChunk::pics() const
{
    return (ic::PICInfo *)((char *)setElems() + sizeof(ic::SetElementIC) * nSetElems);
}

char *
JITChunk::polyICSectionsLimit() const
{
    return (char *)pics() + sizeof(ic::PICInfo) * nPICs;
}
#else   // JS_POLYIC
char *
JITChunk::polyICSectionsLimit() const
{
    return monoICSectionsLimit();
}
#endif  // JS_POLYIC

void
JITScript::patchEdge(const CrossChunkEdge &edge, void *label)
{
    if (edge.sourceJump1 || edge.sourceJump2) {
        JITChunk *sourceChunk = chunk(script->code + edge.source);
        ic::Repatcher repatch(sourceChunk);

#ifdef JS_CPU_X64
        JS_ASSERT(edge.sourceTrampoline);

        static const uint32_t JUMP_LENGTH = 10;

        if (edge.sourceJump1) {
            JSC::CodeLocationLabel targetLabel(VerifyRange(edge.sourceJump1, JUMP_LENGTH, label, 0)
                                               ? label
                                               : edge.sourceTrampoline);
            repatch.relink(JSC::CodeLocationJump(edge.sourceJump1), targetLabel);
        }
        if (edge.sourceJump2) {
            JSC::CodeLocationLabel targetLabel(VerifyRange(edge.sourceJump2, JUMP_LENGTH, label, 0)
                                               ? label
                                               : edge.sourceTrampoline);
            repatch.relink(JSC::CodeLocationJump(edge.sourceJump2), targetLabel);
        }
        JSC::CodeLocationDataLabelPtr sourcePatch((char*)edge.sourceTrampoline + JUMP_LENGTH);
        repatch.repatch(sourcePatch, label);
#else
        JSC::CodeLocationLabel targetLabel(label);
        if (edge.sourceJump1)
            repatch.relink(JSC::CodeLocationJump(edge.sourceJump1), targetLabel);
        if (edge.sourceJump2)
            repatch.relink(JSC::CodeLocationJump(edge.sourceJump2), targetLabel);
#endif
    }
    if (edge.jumpTableEntries) {
        for (unsigned i = 0; i < edge.jumpTableEntries->length(); i++)
            *(*edge.jumpTableEntries)[i] = label;
    }
}

JITChunk::~JITChunk()
{
    purgeCaches();
    code.release();

    for (size_t i = 0; i < nRootedRegExps; i++)
        rootedRegExps()[i]->decRef();

    if (pcLengths)
        Foreground::free_(pcLengths);
}

void
JITScript::destroy(FreeOp *fop)
{
    for (unsigned i = 0; i < nchunks; i++)
        destroyChunk(fop, i);

    if (shimPool)
        shimPool->release();
}

void
JITScript::destroyChunk(FreeOp *fop, unsigned chunkIndex, bool resetUses)
{
    ChunkDescriptor &desc = chunkDescriptor(chunkIndex);

    if (desc.chunk) {
        /*
         * Write barrier: Before we destroy the chunk, trace through the objects
         * it holds.
         */
        if (script->compartment()->needsBarrier())
            desc.chunk->trace(script->compartment()->barrierTracer());

        Probes::discardMJITCode(fop, this, desc.chunk, desc.chunk->code.m_code.executableAddress());
        fop->delete_(desc.chunk);
        desc.chunk = NULL;

        CrossChunkEdge *edges = this->edges();
        for (unsigned i = 0; i < nedges; i++) {
            CrossChunkEdge &edge = edges[i];
            if (edge.source >= desc.begin && edge.source < desc.end) {
                edge.sourceJump1 = edge.sourceJump2 = NULL;
#ifdef JS_CPU_X64
                edge.sourceTrampoline = NULL;
#endif
                if (edge.jumpTableEntries) {
                    fop->delete_(edge.jumpTableEntries);
                    edge.jumpTableEntries = NULL;
                }
            } else if (edge.target >= desc.begin && edge.target < desc.end) {
                edge.targetLabel = NULL;
                patchEdge(edge, edge.shimLabel);
            }
        }
    }

    if (resetUses)
        desc.counter = 0;

    if (chunkIndex == 0) {
        if (argsCheckPool) {
            argsCheckPool->release();
            argsCheckPool = NULL;
        }

        invokeEntry = NULL;
        fastEntry = NULL;
        argsCheckEntry = NULL;
        arityCheckEntry = NULL;

        // Fixup any ICs still referring to this chunk.
        while (!JS_CLIST_IS_EMPTY(&callers)) {
            JS_STATIC_ASSERT(offsetof(ic::CallICInfo, links) == 0);
            ic::CallICInfo *ic = (ic::CallICInfo *) callers.next;

            uint8_t *start = (uint8_t *)ic->funGuard.executableAddress();
            JSC::RepatchBuffer repatch(JSC::JITCode(start - 32, 64));

            repatch.repatch(ic->funGuard, NULL);
            repatch.relink(ic->funJump, ic->slowPathStart);
            ic->purgeGuardedObject();
        }
    }
}

void
JITScript::trace(JSTracer *trc)
{
    for (unsigned i = 0; i < nchunks; i++) {
        ChunkDescriptor &desc = chunkDescriptor(i);
        if (desc.chunk)
            desc.chunk->trace(trc);
    }
}

void
JITScript::purgeCaches()
{
    for (unsigned i = 0; i < nchunks; i++) {
        ChunkDescriptor &desc = chunkDescriptor(i);
        if (desc.chunk)
            desc.chunk->purgeCaches();
    }
}

const js::mjit::JITScript *JSScript::JITScriptHandle::UNJITTABLE =
    reinterpret_cast<js::mjit::JITScript *>(1);

void
JSScript::JITScriptHandle::staticAsserts()
{
    // JITScriptHandle's memory layout must match that of JITScript *.
    JS_STATIC_ASSERT(sizeof(JSScript::JITScriptHandle) == sizeof(js::mjit::JITScript *));
    JS_STATIC_ASSERT(JS_ALIGNMENT_OF(JSScript::JITScriptHandle) ==
                     JS_ALIGNMENT_OF(js::mjit::JITScript *));
    JS_STATIC_ASSERT(offsetof(JSScript::JITScriptHandle, value) == 0);
}

size_t
JSScript::sizeOfJitScripts(JSMallocSizeOfFun mallocSizeOf)
{
    size_t n = 0;
    for (int constructing = 0; constructing <= 1; constructing++) {
        for (int barriers = 0; barriers <= 1; barriers++) {
            JITScript *jit = getJIT((bool) constructing, (bool) barriers);
            if (jit)
                n += jit->sizeOfIncludingThis(mallocSizeOf);
        }
    }
    return n;
}

size_t
mjit::JITScript::sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf)
{
    size_t n = mallocSizeOf(this);
    for (unsigned i = 0; i < nchunks; i++) {
        const ChunkDescriptor &desc = chunkDescriptor(i);
        if (desc.chunk)
            n += desc.chunk->sizeOfIncludingThis(mallocSizeOf);
    }
    return n;
}

/* Please keep in sync with Compiler::finishThisUp! */
size_t
mjit::JITChunk::computedSizeOfIncludingThis()
{
    return sizeof(JITChunk) +
           sizeof(NativeMapEntry) * nNmapPairs +
           sizeof(InlineFrame) * nInlineFrames +
           sizeof(CallSite) * nCallSites +
           sizeof(JSObject*) * nRootedTemplates +
           sizeof(RegExpShared*) * nRootedRegExps +
#if defined JS_MONOIC
           sizeof(ic::GetGlobalNameIC) * nGetGlobalNames +
           sizeof(ic::SetGlobalNameIC) * nSetGlobalNames +
           sizeof(ic::CallICInfo) * nCallICs +
           sizeof(ic::EqualityICInfo) * nEqualityICs +
#endif
#if defined JS_POLYIC
           sizeof(ic::PICInfo) * nPICs +
           sizeof(ic::GetElementIC) * nGetElems +
           sizeof(ic::SetElementIC) * nSetElems +
#endif
           0;
}

/* Please keep in sync with Compiler::finishThisUp! */
size_t
mjit::JITChunk::sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf)
{
    return mallocSizeOf(this);
}

void
JSScript::ReleaseCode(FreeOp *fop, JITScriptHandle *jith)
{
    // NB: The recompiler may call ReleaseScriptCode, in which case it
    // will get called again when the script is destroyed, so we
    // must protect against calling ReleaseScriptCode twice.

    JITScript *jit = jith->getValid();
    jit->destroy(fop);
    fop->free_(jit);
    jith->setEmpty();
}

#ifdef JS_METHODJIT_PROFILE_STUBS
void JS_FASTCALL
mjit::ProfileStubCall(VMFrame &f)
{
    JSOp op = JSOp(*f.regs.pc);
    StubCallsForOp[op]++;
}
#endif

JITChunk *
JITScript::findCodeChunk(void *addr)
{
    for (unsigned i = 0; i < nchunks; i++) {
        ChunkDescriptor &desc = chunkDescriptor(i);
        if (desc.chunk && desc.chunk->isValidCode(addr))
            return desc.chunk;
    }
    return NULL;
}

jsbytecode *
JITScript::nativeToPC(void *returnAddress, CallSite **pinline)
{
    JITChunk *chunk = findCodeChunk(returnAddress);
    JS_ASSERT(chunk);

    JS_ASSERT(chunk->isValidCode(returnAddress));

    size_t low = 0;
    size_t high = chunk->nCallICs;
    js::mjit::ic::CallICInfo *callICs_ = chunk->callICs();
    while (high > low + 1) {
        /* Could overflow here on a script with 2 billion calls. Oh well. */
        size_t mid = (high + low) / 2;
        void *entry = callICs_[mid].funGuard.executableAddress();

        /*
         * Use >= here as the return address of the call is likely to be
         * the start address of the next (possibly IC'ed) operation.
         */
        if (entry >= returnAddress)
            high = mid;
        else
            low = mid;
    }

    js::mjit::ic::CallICInfo &ic = callICs_[low];
    JS_ASSERT((uint8_t*)ic.funGuard.executableAddress() + ic.joinPointOffset == returnAddress);

    if (ic.call->inlineIndex != UINT32_MAX) {
        if (pinline)
            *pinline = ic.call;
        InlineFrame *frame = &chunk->inlineFrames()[ic.call->inlineIndex];
        while (frame && frame->parent)
            frame = frame->parent;
        return frame->parentpc;
    }

    if (pinline)
        *pinline = NULL;
    return script->code + ic.call->pcOffset;
}

jsbytecode *
mjit::NativeToPC(JITScript *jit, void *ncode, mjit::CallSite **pinline)
{
    return jit->nativeToPC(ncode, pinline);
}

/* static */ const double mjit::Assembler::oneDouble = 1.0;

void
JITChunk::trace(JSTracer *trc)
{
    JSObject **rootedTemplates_ = rootedTemplates();
    for (size_t i = 0; i < nRootedTemplates; i++) {
        /* We use a manual write barrier in destroyChunk. */
        MarkObjectUnbarriered(trc, &rootedTemplates_[i], "jitchunk_template");
    }
}

void
JITChunk::purgeCaches()
{
    ic::Repatcher repatch(this);

#if defined JS_MONOIC
    uint32_t releasedExecPools = 0;

    ic::EqualityICInfo *equalityICs_ = equalityICs();
    for (uint32_t i = 0; i < nEqualityICs; i++) {
        ic::EqualityICInfo &ic = equalityICs_[i];
        if (!ic.generated)
            continue;

        JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, ic::Equality));
        repatch.relink(ic.stubCall, fptr);
        repatch.relink(ic.jumpToStub, ic.stubEntry);

        ic.generated = false;
        releasedExecPools++;
    }

    JS_ASSERT(releasedExecPools == execPools.length());
    for (JSC::ExecutablePool **pExecPool = execPools.begin();
         pExecPool != execPools.end();
         ++pExecPool)
    {
        (*pExecPool)->release();
    }
    execPools.clear();

    for (unsigned i = 0; i < nativeCallStubs.length(); i++) {
        JSC::ExecutablePool *pool = nativeCallStubs[i].pool;
        if (pool)
            pool->release();
    }
    nativeCallStubs.clear();

    ic::GetGlobalNameIC *getGlobalNames_ = getGlobalNames();
    for (uint32_t i = 0; i < nGetGlobalNames; i++) {
        ic::GetGlobalNameIC &ic = getGlobalNames_[i];
        repatch.repatch(ic.fastPathStart.dataLabelPtrAtOffset(ic.shapeOffset), NULL);
    }

    ic::SetGlobalNameIC *setGlobalNames_ = setGlobalNames();
    for (uint32_t i = 0; i < nSetGlobalNames; i++) {
        ic::SetGlobalNameIC &ic = setGlobalNames_[i];
        ic.patchInlineShapeGuard(repatch, NULL);
    }

    ic::CallICInfo *callICs_ = callICs();
    for (uint32_t i = 0; i < nCallICs; i++)
        callICs_[i].reset(repatch);
#endif

#if defined JS_POLYIC
    ic::GetElementIC *getElems_ = getElems();
    ic::SetElementIC *setElems_ = setElems();
    ic::PICInfo *pics_ = pics();
    for (uint32_t i = 0; i < nGetElems; i++)
        getElems_[i].purge(repatch);
    for (uint32_t i = 0; i < nSetElems; i++)
        setElems_[i].purge(repatch);
    for (uint32_t i = 0; i < nPICs; i++)
        pics_[i].purge(repatch);
#endif
}
