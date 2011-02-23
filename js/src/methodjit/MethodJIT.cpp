/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "MethodJIT.h"
#include "Logging.h"
#include "assembler/jit/ExecutableAllocator.h"
#include "jstracer.h"
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


js::mjit::CompilerAllocPolicy::CompilerAllocPolicy(JSContext *cx, Compiler &compiler)
: ContextAllocPolicy(cx),
  oomFlag(&compiler.oomInVector)
{
}
void
JSStackFrame::methodjitStaticAsserts()
{
        /* Static assert for x86 trampolines in MethodJIT.cpp. */
#if defined(JS_CPU_X86)
        JS_STATIC_ASSERT(offsetof(JSStackFrame, rval_)     == 0x18);
        JS_STATIC_ASSERT(offsetof(JSStackFrame, rval_) + 4 == 0x1C);
        JS_STATIC_ASSERT(offsetof(JSStackFrame, ncode_)    == 0x14);
        /* ARM uses decimal literals. */
        JS_STATIC_ASSERT(offsetof(JSStackFrame, rval_)     == 24);
        JS_STATIC_ASSERT(offsetof(JSStackFrame, rval_) + 4 == 28);
        JS_STATIC_ASSERT(offsetof(JSStackFrame, ncode_)    == 20);
#elif defined(JS_CPU_X64)
        JS_STATIC_ASSERT(offsetof(JSStackFrame, rval_)     == 0x30);
        JS_STATIC_ASSERT(offsetof(JSStackFrame, ncode_)    == 0x28);
#endif
}

/*
 * Explanation of VMFrame activation and various helper thunks below.
 *
 * JaegerTrampoline  - Executes a method JIT-compiled JSFunction. This function
 *    creates a VMFrame on the machine stack and jumps into JIT'd code. The JIT'd
 *    code will eventually jump back to the VMFrame.
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
 * InjectJaegerReturn - Implements the tail of InlineReturn. This is needed for
 *    tracer integration, where a "return" opcode might not be a safe-point,
 *    and thus the return path must be injected by hijacking the stub return
 *    address.
 *
 *  - Used by RunTracer()
 */

#ifdef JS_METHODJIT_PROFILE_STUBS
static const size_t STUB_CALLS_FOR_OP_COUNT = 255;
static uint32 StubCallsForOp[STUB_CALLS_FOR_OP_COUNT];
#endif

extern "C" void JaegerTrampolineReturn();

extern "C" void JS_FASTCALL
PushActiveVMFrame(VMFrame &f)
{
    f.entryfp->script()->compartment->jaegerCompartment->pushActiveFrame(&f);
    f.regs.fp->setNativeReturnAddress(JS_FUNC_TO_DATA_PTR(void*, JaegerTrampolineReturn));
}

extern "C" void JS_FASTCALL
PopActiveVMFrame(VMFrame &f)
{
    f.entryfp->script()->compartment->jaegerCompartment->popActiveFrame();
}

extern "C" void JS_FASTCALL
SetVMFrameRegs(VMFrame &f)
{
    f.cx->setCurrentRegs(&f.regs);
}

#if defined(__APPLE__) || (defined(XP_WIN) && !defined(JS_CPU_X64)) || defined(XP_OS2)
# define SYMBOL_STRING(name) "_" #name
#else
# define SYMBOL_STRING(name) #name
#endif

JS_STATIC_ASSERT(offsetof(JSFrameRegs, sp) == 0);

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

#if defined(__GNUC__)

/* If this assert fails, you need to realign VMFrame to 16 bytes. */
#ifdef JS_CPU_ARM
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
JS_STATIC_ASSERT(offsetof(VMFrame, savedRBX) == 0x58);
JS_STATIC_ASSERT(offsetof(VMFrame, regs.fp) == 0x38);

JS_STATIC_ASSERT(JSVAL_TAG_MASK == 0xFFFF800000000000LL);
JS_STATIC_ASSERT(JSVAL_PAYLOAD_MASK == 0x00007FFFFFFFFFFFLL);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerTrampoline) "\n"
SYMBOL_STRING(JaegerTrampoline) ":"       "\n"
    /* Prologue. */
    "pushq %rbp"                         "\n"
    "movq %rsp, %rbp"                    "\n"
    /* Save non-volatile registers. */
    "pushq %r12"                         "\n"
    "pushq %r13"                         "\n"
    "pushq %r14"                         "\n"
    "pushq %r15"                         "\n"
    "pushq %rbx"                         "\n"

    /* Load mask registers. */
    "movq $0xFFFF800000000000, %r13"     "\n"
    "movq $0x00007FFFFFFFFFFF, %r14"     "\n"

    /* Build the JIT frame.
     * rdi = cx
     * rsi = fp
     * rcx = inlineCallCount
     * fp must go into rbx
     */
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
    "call " SYMBOL_STRING_VMFRAME(SetVMFrameRegs) "\n"
    "movq  %rsp, %rdi"                   "\n"
    "call " SYMBOL_STRING_VMFRAME(PushActiveVMFrame) "\n"

    /* Jump into the JIT'd code. */
    "jmp *0(%rsp)"                      "\n"
);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerTrampolineReturn) "\n"
SYMBOL_STRING(JaegerTrampolineReturn) ":"       "\n"
    "or   %rdx, %rcx"                    "\n"
    "movq %rcx, 0x30(%rbx)"              "\n"
    "movq %rsp, %rdi"                    "\n"
    "call " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"

    "addq $0x58, %rsp"                   "\n"
    "popq %rbx"                          "\n"
    "popq %r15"                          "\n"
    "popq %r14"                          "\n"
    "popq %r13"                          "\n"
    "popq %r12"                          "\n"
    "popq %rbp"                          "\n"
    "movq $1, %rax"                      "\n"
    "ret"                                "\n"
);

asm volatile (
".text\n"
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
    "addq $0x58, %rsp"                      "\n"
    "popq %rbx"                             "\n"
    "popq %r15"                             "\n"
    "popq %r14"                             "\n"
    "popq %r13"                             "\n"
    "popq %r12"                             "\n"
    "popq %rbp"                             "\n"
    "xorq %rax,%rax"                        "\n"
    "ret"                                   "\n"
);

JS_STATIC_ASSERT(offsetof(VMFrame, regs.fp) == 0x38);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(InjectJaegerReturn)   "\n"
SYMBOL_STRING(InjectJaegerReturn) ":"         "\n"
    "movq 0x30(%rbx), %rcx"                 "\n" /* load fp->rval_ into typeReg */
    "movq 0x28(%rbx), %rax"                 "\n" /* fp->ncode_ */

    /* Reimplementation of PunboxAssembler::loadValueAsComponents() */
    "movq %r14, %rdx"                       "\n" /* payloadReg = payloadMaskReg */
    "andq %rcx, %rdx"                       "\n"
    "xorq %rdx, %rcx"                       "\n"

    "movq 0x38(%rsp), %rbx"                 "\n" /* f.fp */
    "jmp *%rax"                             "\n" /* return. */
);

# elif defined(JS_CPU_X86)

/*
 *    *** DANGER ***
 * If these assertions break, update the constants below. The throwpoline
 * should have the offset of savedEBX plus 4, because it needs to clean
 * up the argument.
 *    *** DANGER ***
 */
JS_STATIC_ASSERT(offsetof(VMFrame, savedEBX) == 0x2c);
JS_STATIC_ASSERT(offsetof(VMFrame, regs.fp) == 0x1C);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerTrampoline) "\n"
SYMBOL_STRING(JaegerTrampoline) ":"       "\n"
    /* Prologue. */
    "pushl %ebp"                         "\n"
    "movl %esp, %ebp"                    "\n"
    /* Save non-volatile registers. */
    "pushl %esi"                         "\n"
    "pushl %edi"                         "\n"
    "pushl %ebx"                         "\n"

    /* Build the JIT frame. Push fields in order, 
     * then align the stack to form esp == VMFrame. */
    "movl  12(%ebp), %ebx"               "\n"   /* load fp */
    "pushl %ebx"                         "\n"   /* entryfp */
    "pushl 20(%ebp)"                     "\n"   /* stackLimit */
    "pushl 8(%ebp)"                      "\n"   /* cx */
    "pushl %ebx"                         "\n"   /* fp */
    "subl $0x1C, %esp"                   "\n"

    /* Jump into the JIT'd code. */
    "movl  %esp, %ecx"                   "\n"
    "call " SYMBOL_STRING_VMFRAME(SetVMFrameRegs) "\n"
    "movl  %esp, %ecx"                   "\n"
    "call " SYMBOL_STRING_VMFRAME(PushActiveVMFrame) "\n"

    "jmp *16(%ebp)"                      "\n"
);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerTrampolineReturn) "\n"
SYMBOL_STRING(JaegerTrampolineReturn) ":" "\n"
    "movl  %edx, 0x18(%ebx)"             "\n"
    "movl  %ecx, 0x1C(%ebx)"             "\n"
    "movl  %esp, %ecx"                   "\n"
    "call " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"

    "addl $0x2C, %esp"                   "\n"
    "popl %ebx"                          "\n"
    "popl %edi"                          "\n"
    "popl %esi"                          "\n"
    "popl %ebp"                          "\n"
    "movl $1, %eax"                      "\n"
    "ret"                                "\n"
);

asm volatile (
".text\n"
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
    "addl $0x2c, %esp"                   "\n"
    "popl %ebx"                          "\n"
    "popl %edi"                          "\n"
    "popl %esi"                          "\n"
    "popl %ebp"                          "\n"
    "xorl %eax, %eax"                    "\n"
    "ret"                                "\n"
);

JS_STATIC_ASSERT(offsetof(VMFrame, regs.fp) == 0x1C);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(InjectJaegerReturn)   "\n"
SYMBOL_STRING(InjectJaegerReturn) ":"         "\n"
    "movl 0x18(%ebx), %edx"                 "\n" /* fp->rval_ data */
    "movl 0x1C(%ebx), %ecx"                 "\n" /* fp->rval_ type */
    "movl 0x14(%ebx), %eax"                 "\n" /* fp->ncode_ */
    "movl 0x1C(%esp), %ebx"                 "\n" /* f.fp */
    "jmp *%eax"                             "\n"
);

# elif defined(JS_CPU_ARM)

JS_STATIC_ASSERT(sizeof(VMFrame) == 80);
JS_STATIC_ASSERT(offsetof(VMFrame, savedLR) ==          (4*19));
JS_STATIC_ASSERT(offsetof(VMFrame, entryfp) ==          (4*10));
JS_STATIC_ASSERT(offsetof(VMFrame, stackLimit) ==       (4*9));
JS_STATIC_ASSERT(offsetof(VMFrame, cx) ==               (4*8));
JS_STATIC_ASSERT(offsetof(VMFrame, regs.fp) ==          (4*7));
JS_STATIC_ASSERT(offsetof(VMFrame, unused) ==           (4*4));
JS_STATIC_ASSERT(offsetof(VMFrame, previous) ==         (4*3));

JS_STATIC_ASSERT(JSFrameReg == JSC::ARMRegisters::r11);
JS_STATIC_ASSERT(JSReturnReg_Data == JSC::ARMRegisters::r1);
JS_STATIC_ASSERT(JSReturnReg_Type == JSC::ARMRegisters::r2);

#ifdef MOZ_THUMB2
#define FUNCTION_HEADER_EXTRA \
  ".align 2\n" \
  ".thumb\n" \
  ".thumb_func\n"
#else
#define FUNCTION_HEADER_EXTRA
#endif

asm volatile (
".text\n"
FUNCTION_HEADER_EXTRA
".globl " SYMBOL_STRING(InjectJaegerReturn) "\n"
SYMBOL_STRING(InjectJaegerReturn) ":"       "\n"
    /* Restore frame regs. */
    "ldr lr, [r11, #20]"                    "\n" /* fp->ncode */
    "ldr r1, [r11, #24]"                    "\n" /* fp->rval data */
    "ldr r2, [r11, #28]"                    "\n" /* fp->rval type */
    "ldr r11, [sp, #28]"                    "\n" /* load f.fp */
    "bx  lr"                                "\n"
);

asm volatile (
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
     *  [ lr        ]   \
     *  [ r11       ]   |
     *  [ r10       ]   |
     *  [ r9        ]   | Callee-saved registers.                             
     *  [ r8        ]   | VFP registers d8-d15 may be required here too, but  
     *  [ r7        ]   | unconditionally preserving them might be expensive
     *  [ r6        ]   | considering that we might not use them anyway.
     *  [ r5        ]   |
     *  [ r4        ]   /
     *  [ entryfp   ]
     *  [ stkLimit  ]
     *  [ cx        ]
     *  [ regs.fp   ]
     *  [ regs.pc   ]
     *  [ regs.sp   ]
     *  [ unused    ]
     *  [ previous  ]
     *  [ args.ptr3 ]
     *  [ args.ptr2 ]
     *  [ args.ptr  ]
     */
    
    /* Push callee-saved registers. */
"   push    {r4-r11,lr}"                        "\n"
    /* Push interesting VMFrame content. */
"   push    {r1}"                               "\n"    /* entryfp */
"   push    {r3}"                               "\n"    /* stackLimit */
"   push    {r0}"                               "\n"    /* cx */
"   push    {r1}"                               "\n"    /* regs.fp */
    /* Remaining fields are set elsewhere, but we need to leave space for them. */
"   sub     sp, sp, #(4*7)"                     "\n"

    /* Preserve 'code' (r2) in an arbitrary callee-saved register. */
"   mov     r4, r2"                             "\n"
    /* Preserve 'fp' (r1) in r11 (JSFrameReg). */
"   mov     r11, r1"                            "\n"

"   mov     r0, sp"                             "\n"
"   blx  " SYMBOL_STRING_VMFRAME(SetVMFrameRegs)   "\n"
"   mov     r0, sp"                             "\n"
"   blx  " SYMBOL_STRING_VMFRAME(PushActiveVMFrame)"\n"

    /* Call the compiled JavaScript function. */
"   bx     r4"                                  "\n"
);

asm volatile (
".text\n"
FUNCTION_HEADER_EXTRA
".globl " SYMBOL_STRING(JaegerTrampolineReturn)   "\n"
SYMBOL_STRING(JaegerTrampolineReturn) ":"         "\n"
"   str r1, [r11, #24]"                    "\n" /* fp->rval data */
"   str r2, [r11, #28]"                    "\n" /* fp->rval type */

    /* Tidy up. */
"   mov     r0, sp"                             "\n"
"   blx  " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"

    /* Skip past the parameters we pushed (such as cx and the like). */
"   add     sp, sp, #(4*7 + 4*4)"               "\n"

    /* Set a 'true' return value to indicate successful completion. */
"   mov     r0, #1"                         "\n"
"   pop     {r4-r11,pc}"                    "\n"
);

asm volatile (
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
"   mov     r0, sp"                             "\n"
"   blx  " SYMBOL_STRING_VMFRAME(PopActiveVMFrame) "\n"
"   add     sp, sp, #(4*7 + 4*4)"               "\n"
"   mov     r0, #0"                         "\n"
"   pop     {r4-r11,pc}"                    "\n"
);

asm volatile (
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

# else
#  error "Unsupported CPU!"
# endif
#elif defined(_MSC_VER)

#if defined(JS_CPU_X86)

/*
 *    *** DANGER ***
 * If these assertions break, update the constants below. The throwpoline
 * should have the offset of savedEBX plus 4, because it needs to clean
 * up the argument.
 *    *** DANGER ***
 */
JS_STATIC_ASSERT(offsetof(VMFrame, savedEBX) == 0x2c);
JS_STATIC_ASSERT(offsetof(VMFrame, regs.fp) == 0x1C);

extern "C" {

    __declspec(naked) void InjectJaegerReturn()
    {
        __asm {
            mov edx, [ebx + 0x18];
            mov ecx, [ebx + 0x1C];
            mov eax, [ebx + 0x14];
            mov ebx, [esp + 0x1C];
            jmp eax;
        }
    }

    __declspec(naked) JSBool JaegerTrampoline(JSContext *cx, JSStackFrame *fp, void *code,
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
            push [ebp + 20];
            push [ebp + 8];
            push ebx;
            sub  esp, 0x1C;

            /* Jump into into the JIT'd code. */
            mov  ecx, esp;
            call SetVMFrameRegs;
            mov  ecx, esp;
            call PushActiveVMFrame;

            jmp dword ptr [ebp + 16];
        }
    }

    __declspec(naked) void JaegerTrampolineReturn()
    {
        __asm {
            mov [ebx + 0x18], edx;
            mov [ebx + 0x1C], ecx;
            mov  ecx, esp;
            call PopActiveVMFrame;

            add esp, 0x2C;

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
            add esp, 0x2c;
            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            xor eax, eax
            ret;
        }
    }
}

#elif defined(JS_CPU_X64)

/*
 *    *** DANGER ***
 * If these assertions break, update the constants below.
 *    *** DANGER ***
 */
JS_STATIC_ASSERT(offsetof(VMFrame, savedRBX) == 0x58);
JS_STATIC_ASSERT(offsetof(VMFrame, regs.fp) == 0x38);
JS_STATIC_ASSERT(JSVAL_TAG_MASK == 0xFFFF800000000000LL);
JS_STATIC_ASSERT(JSVAL_PAYLOAD_MASK == 0x00007FFFFFFFFFFFLL);

// Windows x64 uses assembler version since compiler doesn't support
// inline assembler
#else
#  error "Unsupported CPU!"
#endif

#endif                   /* _MSC_VER */

bool
JaegerCompartment::Initialize()
{
    execAlloc = JSC::ExecutableAllocator::create();
    if (!execAlloc)
        return false;
    
    TrampolineCompiler tc(execAlloc, &trampolines);
    if (!tc.compile()) {
        delete execAlloc;
        return false;
    }

#ifdef JS_METHODJIT_PROFILE_STUBS
    for (size_t i = 0; i < STUB_CALLS_FOR_OP_COUNT; ++i)
        StubCallsForOp[i] = 0;
#endif

    activeFrame_ = NULL;

    return true;
}

void
JaegerCompartment::Finish()
{
    TrampolineCompiler::release(&trampolines);
    js_delete(execAlloc);
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
JaegerTrampoline(JSContext *cx, JSStackFrame *fp, void *code, Value *stackLimit);

JSBool
mjit::EnterMethodJIT(JSContext *cx, JSStackFrame *fp, void *code, Value *stackLimit)
{
#ifdef JS_METHODJIT_SPEW
    Profiler prof;
    JSScript *script = fp->script();

    JaegerSpew(JSpew_Prof, "%s jaeger script, line %d\n",
               script->filename, script->lineno);
    prof.start();
#endif

    JS_ASSERT(cx->regs->fp  == fp);
    JSFrameRegs *oldRegs = cx->regs;

    JSBool ok;
    {
        AssertCompartmentUnchanged pcc(cx);
        JSAutoResolveFlags rf(cx, JSRESOLVE_INFER);
        ok = JaegerTrampoline(cx, fp, code, stackLimit);
    }

    cx->setCurrentRegs(oldRegs);
    JS_ASSERT(fp == cx->fp());

    /* The trampoline wrote the return value but did not set the HAS_RVAL flag. */
    fp->markReturnValue();

#ifdef JS_METHODJIT_SPEW
    prof.stop();
    JaegerSpew(JSpew_Prof, "script run took %d ms\n", prof.time_ms());
#endif

    return ok;
}

static inline JSBool
CheckStackAndEnterMethodJIT(JSContext *cx, JSStackFrame *fp, void *code)
{
    bool ok;
    Value *stackLimit;

    JS_CHECK_RECURSION(cx, goto error;);

    stackLimit = cx->stack().getStackLimit(cx);
    if (!stackLimit)
        goto error;

    ok = EnterMethodJIT(cx, fp, code, stackLimit);
    JS_ASSERT_IF(!fp->isYielding() && !(fp->isEvalFrame() && !fp->script()->strictModeCode),
                 !fp->hasCallObj() && !fp->hasArgsObj());
    return ok;

  error:
    js::PutOwnedActivationObjects(cx, fp);
    return false;
}

JSBool
mjit::JaegerShot(JSContext *cx)
{
    JSStackFrame *fp = cx->fp();
    JSScript *script = fp->script();
    JITScript *jit = script->getJIT(fp->isConstructing());

#ifdef JS_TRACER
    if (TRACE_RECORDER(cx))
        AbortRecording(cx, "attempt to enter method JIT while recording");
#endif

    JS_ASSERT(cx->regs->pc == script->code);

    return CheckStackAndEnterMethodJIT(cx, cx->fp(), jit->invokeEntry);
}

JSBool
js::mjit::JaegerShotAtSafePoint(JSContext *cx, void *safePoint)
{
#ifdef JS_TRACER
    JS_ASSERT(!TRACE_RECORDER(cx));
#endif

    return CheckStackAndEnterMethodJIT(cx, cx->fp(), safePoint);
}

NativeMapEntry *
JITScript::nmap() const
{
    return (NativeMapEntry *)((char*)this + sizeof(JITScript));
}

char *
JITScript::nmapSectionLimit() const
{
    return (char *)nmap() + sizeof(NativeMapEntry) * nNmapPairs;
}

#ifdef JS_MONOIC
ic::GetGlobalNameIC *
JITScript::getGlobalNames() const
{
    return (ic::GetGlobalNameIC *)nmapSectionLimit();
}

ic::SetGlobalNameIC *
JITScript::setGlobalNames() const
{
    return (ic::SetGlobalNameIC *)((char *)nmapSectionLimit() +
            sizeof(ic::GetGlobalNameIC) * nGetGlobalNames);
}

ic::CallICInfo *
JITScript::callICs() const
{
    return (ic::CallICInfo *)((char *)setGlobalNames() +
            sizeof(ic::SetGlobalNameIC) * nSetGlobalNames);
}

ic::EqualityICInfo *
JITScript::equalityICs() const
{
    return (ic::EqualityICInfo *)((char *)callICs() + sizeof(ic::CallICInfo) * nCallICs);
}

ic::TraceICInfo *
JITScript::traceICs() const
{
    return (ic::TraceICInfo *)((char *)equalityICs() + sizeof(ic::EqualityICInfo) * nEqualityICs);
}

char *
JITScript::monoICSectionsLimit() const
{
    return (char *)traceICs() + sizeof(ic::TraceICInfo) * nTraceICs;
}
#else   // JS_MONOIC
char *
JITScript::monoICSectionsLimit() const
{
    return nmapSectionsLimit();
}
#endif  // JS_MONOIC

#ifdef JS_POLYIC
ic::GetElementIC *
JITScript::getElems() const
{
    return (ic::GetElementIC *)monoICSectionsLimit();
}

ic::SetElementIC *
JITScript::setElems() const
{
    return (ic::SetElementIC *)((char *)getElems() + sizeof(ic::GetElementIC) * nGetElems);
}

ic::PICInfo *
JITScript::pics() const
{
    return (ic::PICInfo *)((char *)setElems() + sizeof(ic::SetElementIC) * nSetElems);
}

char *
JITScript::polyICSectionsLimit() const
{
    return (char *)pics() + sizeof(ic::PICInfo) * nPICs;
}
#else   // JS_POLYIC
char *
JITScript::polyICSectionsLimit() const
{
    return monoICSectionsLimit();
}
#endif  // JS_POLYIC

js::mjit::CallSite *
JITScript::callSites() const
{
    return (js::mjit::CallSite *)polyICSectionsLimit();
}

template <typename T>
static inline void Destroy(T &t)
{
    t.~T();
}

mjit::JITScript::~JITScript()
{
#if defined DEBUG && (defined JS_CPU_X86 || defined JS_CPU_X64) 
    void *addr = code.m_code.executableAddress();
    memset(addr, 0xcc, code.m_size);
#endif

    code.m_executablePool->release();

#if defined JS_POLYIC
    ic::GetElementIC *getElems_ = getElems();
    ic::SetElementIC *setElems_ = setElems();
    ic::PICInfo *pics_ = pics();
    for (uint32 i = 0; i < nGetElems; i++)
        Destroy(getElems_[i]);
    for (uint32 i = 0; i < nSetElems; i++)
        Destroy(setElems_[i]);
    for (uint32 i = 0; i < nPICs; i++)
        Destroy(pics_[i]);
#endif

#if defined JS_MONOIC
    for (JSC::ExecutablePool **pExecPool = execPools.begin();
         pExecPool != execPools.end();
         ++pExecPool)
    {
        (*pExecPool)->release();
    }
    
    ic::CallICInfo *callICs_ = callICs();
    for (uint32 i = 0; i < nCallICs; i++)
        callICs_[i].releasePools();
#endif
}

/* Please keep in sync with Compiler::finishThisUp! */
size_t
mjit::JITScript::scriptDataSize()
{
    return sizeof(JITScript) +
        sizeof(NativeMapEntry) * nNmapPairs +
#if defined JS_MONOIC
        sizeof(ic::GetGlobalNameIC) * nGetGlobalNames +
        sizeof(ic::SetGlobalNameIC) * nSetGlobalNames +
        sizeof(ic::CallICInfo) * nCallICs +
        sizeof(ic::EqualityICInfo) * nEqualityICs +
        sizeof(ic::TraceICInfo) * nTraceICs +
#endif
#if defined JS_POLYIC
        sizeof(ic::PICInfo) * nPICs +
        sizeof(ic::GetElementIC) * nGetElems +
        sizeof(ic::SetElementIC) * nSetElems +
#endif
        sizeof(CallSite) * nCallSites;
}

void
mjit::ReleaseScriptCode(JSContext *cx, JSScript *script)
{
    // NB: The recompiler may call ReleaseScriptCode, in which case it
    // will get called again when the script is destroyed, so we
    // must protect against calling ReleaseScriptCode twice.
    JITScript *jscr;

    if ((jscr = script->jitNormal)) {
        cx->runtime->mjitMemoryUsed -= jscr->scriptDataSize() + jscr->mainCodeSize();

        jscr->~JITScript();
        cx->free(jscr);
        script->jitNormal = NULL;
        script->jitArityCheckNormal = NULL;
    }

    if ((jscr = script->jitCtor)) {
        cx->runtime->mjitMemoryUsed -= jscr->scriptDataSize() + jscr->mainCodeSize();

        jscr->~JITScript();
        cx->free(jscr);
        script->jitCtor = NULL;
        script->jitArityCheckCtor = NULL;
    }
}

#ifdef JS_METHODJIT_PROFILE_STUBS
void JS_FASTCALL
mjit::ProfileStubCall(VMFrame &f)
{
    JSOp op = JSOp(*f.regs.pc);
    StubCallsForOp[op]++;
}
#endif

#ifdef JS_POLYIC
static int
PICPCComparator(const void *key, const void *entry)
{
    const jsbytecode *pc = (const jsbytecode *)key;
    const ic::PICInfo *pic = (const ic::PICInfo *)entry;

    if (ic::PICInfo::CALL != pic->kind)
        return ic::PICInfo::CALL - pic->kind;

    /*
     * We can't just return |pc - pic->pc| because the pointers may be
     * far apart and an int (or even a ptrdiff_t) may not be large
     * enough to hold the difference. C says that pointer subtraction
     * is only guaranteed to work for two pointers into the same array.
     */
    if (pc < pic->pc)
        return -1;
    else if (pc == pic->pc)
        return 0;
    else
        return 1;
}

uintN
mjit::GetCallTargetCount(JSScript *script, jsbytecode *pc)
{
    ic::PICInfo *pic;
    
    if (mjit::JITScript *jit = script->getJIT(false)) {
        pic = (ic::PICInfo *)bsearch(pc, jit->pics(), jit->nPICs, sizeof(ic::PICInfo),
                                     PICPCComparator);
        if (pic)
            return pic->stubsGenerated + 1; /* Add 1 for the inline path. */
    }
    
    if (mjit::JITScript *jit = script->getJIT(true)) {
        pic = (ic::PICInfo *)bsearch(pc, jit->pics(), jit->nPICs, sizeof(ic::PICInfo),
                                     PICPCComparator);
        if (pic)
            return pic->stubsGenerated + 1; /* Add 1 for the inline path. */
    }

    return 1;
}
#else
uintN
mjit::GetCallTargetCount(JSScript *script, jsbytecode *pc)
{
    return 1;
}
#endif

jsbytecode *
JITScript::nativeToPC(void *returnAddress) const
{
    size_t low = 0;
    size_t high = nCallICs;
    js::mjit::ic::CallICInfo *callICs_ = callICs();
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

    JS_ASSERT((uint8*)ic.funGuard.executableAddress() + ic.joinPointOffset == returnAddress);
    return ic.pc;
}

