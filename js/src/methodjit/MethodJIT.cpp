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
#include "MonoIC.h"
#include "PolyIC.h"

using namespace js;
using namespace js::mjit;

#ifdef JS_METHODJIT_PROFILE_STUBS
static uint32 StubCallsForOp[255];
#endif

extern "C" void JS_FASTCALL
SetVMFrameRegs(VMFrame &f)
{
    f.oldRegs = f.cx->regs;
    f.cx->setCurrentRegs(&f.regs);
}

extern "C" void JS_FASTCALL
UnsetVMFrameRegs(VMFrame &f)
{
    *f.oldRegs = f.regs;
    f.cx->setCurrentRegs(f.oldRegs);
}

#if defined(__APPLE__) || defined(XP_WIN)
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
JS_STATIC_ASSERT(offsetof(VMFrame, savedRBX) == 0x48);
JS_STATIC_ASSERT(offsetof(VMFrame, fp) == 0x30);

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

    /* Build the JIT frame.
     * rdi = cx
     * rsi = fp
     * rcx = inlineCallCount
     * fp must go into rbx
     */
    "pushq %rcx"                         "\n"
    "pushq %rdi"                         "\n"
    "pushq %rsi"                         "\n"
    "movq  %rsi, %rbx"                   "\n"

    /* Space for the rest of the VMFrame. */
    "subq  $0x28, %rsp"                  "\n"

    /* Set cx->regs (requires saving rdx). */
    "pushq %rdx"                         "\n"
    "movq  %rsp, %rdi"                   "\n"
    "call " SYMBOL_STRING_RELOC(SetVMFrameRegs) "\n"
    "popq  %rdx"                         "\n"

    /*
     * Jump into into the JIT'd code. The call implicitly fills in
     * the precious f.scriptedReturn member of VMFrame.
     */
    "call *%rdx"                         "\n"
    "leaq -8(%rsp), %rdi"                "\n"
    "call " SYMBOL_STRING_RELOC(UnsetVMFrameRegs) "\n"

    "addq $0x40, %rsp"                   "\n"
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
    "addq $0x48, %rsp"                      "\n"
    "popq %rbx"                             "\n"
    "popq %r15"                             "\n"
    "popq %r14"                             "\n"
    "popq %r13"                             "\n"
    "popq %r12"                             "\n"
    "popq %rbp"                             "\n"
    "ret"                                   "\n"
);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerFromTracer)   "\n"
SYMBOL_STRING(JaegerFromTracer) ":"         "\n"
    /* Restore fp reg. */
    "movq 0x30(%rsp), %rbx"                 "\n"
    "jmp *%rax"                             "\n"
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
JS_STATIC_ASSERT(offsetof(VMFrame, fp) == 0x20);

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
    "pushl 20(%ebp)"                     "\n"
    "pushl 8(%ebp)"                      "\n"
    "pushl 12(%ebp)"                     "\n"
    "movl  12(%ebp), %ebx"               "\n"
    "subl $0x1c, %esp"                   "\n"

    /* Jump into the JIT'd code. */
    "pushl 16(%ebp)"                     "\n"
    "movl  %esp, %ecx"                   "\n"
    "call " SYMBOL_STRING_RELOC(SetVMFrameRegs) "\n"
    "popl  %edx"                         "\n"

    "call  *%edx"                        "\n"
    "leal  -4(%esp), %ecx"               "\n"
    "call " SYMBOL_STRING_RELOC(UnsetVMFrameRegs) "\n"

    "addl $0x28, %esp"                   "\n"
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
    "addl $0x2c, %esp"                   "\n"
    "popl %ebx"                          "\n"
    "popl %edi"                          "\n"
    "popl %esi"                          "\n"
    "popl %ebp"                          "\n"
    "xorl %eax, %eax"                    "\n"
    "ret"                                "\n"
);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerFromTracer)   "\n"
SYMBOL_STRING(JaegerFromTracer) ":"         "\n"
    "movl 0x28(%ebx), %edx"                 "\n"
    "movl 0x2C(%ebx), %ecx"                 "\n"
    "movl 0x3C(%ebx), %eax"                 "\n"
    "movl 0x20(%esp), %ebx"                 "\n"
    "ret"                                   "\n"
);

# elif defined(JS_CPU_ARM)

JS_STATIC_ASSERT(offsetof(VMFrame, savedLR) == 76);
JS_STATIC_ASSERT(offsetof(VMFrame, fp) == 32);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerFromTracer)   "\n"
SYMBOL_STRING(JaegerFromTracer) ":"         "\n"
    /* Restore frame regs. */
    "ldr r11, [sp, #32]"                    "\n"
    "bx  r0"                                "\n"
);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerTrampoline)   "\n"
SYMBOL_STRING(JaegerTrampoline) ":"         "\n"
    /* The trampoline for ARM looks like this:
     *  [ lr        ]   \
     *  [ r11       ]   |
     *  [ r10       ]   |
     *  [ r9        ]   | Callee-saved registers.                             
     *  [ r8        ]   | VFP registers d8-d15 may be required here too, but  
     *  [ r7        ]   | unconditionally preserving them might be expensive
     *  [ r6        ]   | considering that we might not use them anyway.
     *  [ r5        ]   |
     *  [ r4        ]   /
     *  [ regs      ]   \
     *  [ ICallCnt  ]   | Parameters for the compiled code (and subsequently-called routines).
     *  [ cx        ]   |
     *  [ fp        ]   |
     *  [ sp        ]   /
     *  [ args      ]
     *  [ ...       ]
     *  [ padding   ]
     *  [ exc. ret  ]  <- sp
     */
    
    /* We push these in short groups (rather than using one big push operation) because it
     * supposedly benefits Cortex-A9. TODO: Check that this is actually a benefit. */
"   push    {r10,r11,lr}"                   "\n"
"   push    {r7-r9}"                        "\n"
"   push    {r4-r6}"                        "\n"
"   mov     r11, #0"                        "\n"   /* r11 = inlineCallCount */
"   push    {r11}"                          "\n"   /* inlineCallCount   */
"   push    {r0}"                           "\n"   /* cx                */
"   push    {r1}"                           "\n"   /* fp                */
"   mov     r11, r1"                        "\n"   /* JSFrameReg        */

    /* Leave space for the VMFrame arguments. The largest slot appears to be 8 bytes for 32-bit
     * architectures, though hard-coding this doesn't seem sensible. TODO: Use sizeof here and for
     * the other targets. */

"   sub     sp, sp, #(8*4)"                 "\n"

"   mov     r0, sp"                         "\n"
"   push    {r2}"                           "\n"
"   blx " SYMBOL_STRING_RELOC(SetVMFrameRegs) "\n"
"   pop     {r2}"                           "\n"

    /* Call the compiled JavaScript function using r2 ('code'). */
"   bl  " SYMBOL_STRING_RELOC(JaegerTrampVeneer) "\n"

    /* --------
     * Tidy up: Unwind the stack pointer, restore the callee-saved registers and then return.
     */

"   mov     r0, sp"                         "\n"
"   blx " SYMBOL_STRING_RELOC(UnsetVMFrameRegs) "\n"

    /* Skip past the parameters we pushed (such as cx and the like). */
"   add     sp, sp, #(11*4)"           "\n"

    /* Set a 'true' return value to indicate successful completion. */
"   mov     r0, #1"                         "\n"

    /* We pop these in short groups (rather than using one big push operation) because it
     * supposedly benefits Cortex-A9. TODO: Check that this is actually a benefit. */
"   pop     {r4-r6}"                        "\n"
"   pop     {r7-r9}"                        "\n"
"   pop     {r10,r11,pc}"                   "\n"    /* Pop lr directly into the pc to return quickly. */
);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerThrowpoline)  "\n"
SYMBOL_STRING(JaegerThrowpoline) ":"        "\n"
    /* Restore 'f', as it will have been clobbered. */
"   mov     r0, sp"                         "\n"

    /* Call the utility function that sets up the internal throw routine. */
"   blx " SYMBOL_STRING_RELOC(js_InternalThrow) "\n"
    
    /* If 0 was returned, just bail out as normal. Otherwise, we have a 'catch' or 'finally' clause
     * to execute. */
"   cmp     r0, #0"                         "\n"
"   bxne    r0"                             "\n"

    /* Skip past the parameters we pushed (such as cx and the like). */
"   add     sp, sp, #(11*4)"           "\n"

    /* We pop these in short groups (rather than using one big push operation) because it
     * supposedly benefits Cortex-A9. TODO: Check that this is actually a benefit. */
"   pop     {r4-r6}"                        "\n"
"   pop     {r7-r9}"                        "\n"
"   pop     {r10,r11,pc}"                   "\n"    /* Pop lr directly into the pc to return quickly. */
);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerStubVeneer)   "\n"
SYMBOL_STRING(JaegerStubVeneer) ":"         "\n"
    /* We enter this function as a veneer between a compiled method and one of the js_ stubs. We
     * need to store the LR somewhere (so it can be modified in case on an exception) and then
     * branch to the js_ stub as if nothing had happened.
     * The arguments are identical to those for js_* except that the target function should be in
     * 'ip'. TODO: This is not ABI-compliant, though it is convenient for now. I should work out
     * which register to use to do this properly; r1 is likely because r0 gets VMFrame &f. */
"   str     lr, [sp]"                       "\n"    /* VMFrame->veneerReturn */
"   blx     ip"                             "\n"
"   ldr     pc, [sp]"                       "\n"    /* This should stack-predict, but only if 'sp' is used. */
);

asm volatile (
".text\n"
".globl " SYMBOL_STRING(JaegerTrampVeneer)   "\n"
SYMBOL_STRING(JaegerTrampVeneer) ":"         "\n"
    /* This is needed to store the initial scriptedReturn value, which won't
     * get stored when invoking JaegerShot() into the middle of methods.
     */
"   str     lr, [sp, #4]"                   "\n"    /* VMFrame->scriptedReturn */
"   bx      r2"                             "\n"
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
JS_STATIC_ASSERT(offsetof(VMFrame, fp) == 0x20);

extern "C" {

    __declspec(naked) void JaegerFromTracer()
    {
        __asm {
            mov edx, [ebx + 0x28];
            mov ecx, [ebx + 0x2C];
            mov eax, [ebx + 0x3C];
            mov ebx, [esp + 0x20];
            ret;
        }
    }

    __declspec(naked) JSBool JaegerTrampoline(JSContext *cx, JSStackFrame *fp, void *code,
                                              uintptr_t inlineCallCount)
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
            push [ebp+20];
            push [ebp+8];
            push [ebp+12];
            mov  ebx, [ebp+12];
            sub  esp, 0x1c;

            /* Jump into into the JIT'd code. */
            push [ebp+16];
            mov  ecx, esp;
            call SetVMFrameRegs;
            pop  edx;

            call edx;
            lea  ecx, [esp-4];
            call UnsetVMFrameRegs;

            add esp, 0x28

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
JS_STATIC_ASSERT(offsetof(VMFrame, savedRBX) == 0x48);
JS_STATIC_ASSERT(offsetof(VMFrame, fp) == 0x30);

// Windows x64 uses assembler version since compiler doesn't support
// inline assembler
#else
#  error "Unsupported CPU!"
#endif

#endif                   /* _MSC_VER */

bool
ThreadData::Initialize()
{
    execPool = new JSC::ExecutableAllocator();
    if (!execPool)
        return false;
    
    if (!picScripts.init()) {
        delete execPool;
        return false;
    }

    return true;
}

void
ThreadData::Finish()
{
    delete execPool;
#ifdef JS_METHODJIT_PROFILE_STUBS
    FILE *fp = fopen("/tmp/stub-profiling", "wt");
# define OPDEF(op,val,name,image,length,nuses,ndefs,prec,format) \
    fprintf(fp, "%03d %s %d\n", val, #op, StubCallsForOp[val]);
# include "jsopcode.tbl"
# undef OPDEF
    fclose(fp);
#endif
}

bool
ThreadData::addScript(JSScript *script)
{
    ScriptSet::AddPtr p = picScripts.lookupForAdd(script);
    if (p)
        return true;
    return picScripts.add(p, script);
}

void
ThreadData::removeScript(JSScript *script)
{
    ScriptSet::Ptr p = picScripts.lookup(script);
    if (p)
        picScripts.remove(p);
}

void
ThreadData::purge(JSContext *cx)
{
    if (!cx->runtime->gcRegenShapes)
        return;

    for (ThreadData::ScriptSet::Enum e(picScripts); !e.empty(); e.popFront()) {
#if defined JS_POLYIC
        JSScript *script = e.front();
        ic::PurgePICs(cx, script);
#endif
#if defined JS_MONOIC
        //PurgeMICs(cs, script);
#endif
    }

    picScripts.clear();
}


extern "C" JSBool JaegerTrampoline(JSContext *cx, JSStackFrame *fp, void *code,
                                   uintptr_t inlineCallCount);

JSBool
mjit::JaegerShot(JSContext *cx)
{
    JS_ASSERT(cx->regs);

    JS_CHECK_RECURSION(cx, return JS_FALSE;);

    void *code;
    jsbytecode *pc = cx->regs->pc;
    JSStackFrame *fp = cx->fp;
    JSScript *script = fp->script;
    uintptr_t inlineCallCount = 0;

    JS_ASSERT(script->ncode && script->ncode != JS_UNJITTABLE_METHOD);

#ifdef JS_TRACER
    if (TRACE_RECORDER(cx))
        AbortRecording(cx, "attempt to enter method JIT while recording");
#endif

    if (pc == script->code)
        code = script->nmap[-1];
    else
        code = script->nmap[pc - script->code];

    JS_ASSERT(code);

#ifdef JS_METHODJIT_SPEW
    Profiler prof;

    JaegerSpew(JSpew_Prof, "entering jaeger script: %s, line %d\n", fp->script->filename,
               fp->script->lineno);
    prof.start();
#endif

#ifdef DEBUG
    JSStackFrame *checkFp = fp;
#endif
#if 0
    uintptr_t iCC = inlineCallCount;
    while (iCC--)
        checkFp = checkFp->down;
#endif

    JSAutoResolveFlags rf(cx, JSRESOLVE_INFER);
    JSBool ok = JaegerTrampoline(cx, fp, code, inlineCallCount);

    JS_ASSERT(checkFp == cx->fp);

#ifdef JS_METHODJIT_SPEW
    prof.stop();
    JaegerSpew(JSpew_Prof, "script run took %d ms\n", prof.time_ms());
#endif

    return ok;
}

template <typename T>
static inline void Destroy(T &t)
{
    t.~T();
}

void
mjit::ReleaseScriptCode(JSContext *cx, JSScript *script)
{
    if (script->execPool) {
        script->execPool->release();
        script->execPool = NULL;
        // Releasing the execPool takes care of releasing the code.
        script->ncode = NULL;
#ifdef DEBUG
        script->jitLength = 0;
#endif
        
#if defined JS_POLYIC
        if (script->pics) {
            uint32 npics = script->numPICs();
            for (uint32 i = 0; i < npics; i++) {
                script->pics[i].releasePools();
                Destroy(script->pics[i].execPools);
            }
            JS_METHODJIT_DATA(cx).removeScript(script);
            cx->free((uint8*)script->pics - sizeof(uint32));
        }
#endif
    }

    if (script->nmap) {
        cx->free(script->nmap - 1);
        script->nmap = NULL;
    }
#if defined JS_MONOIC
    if (script->mics) {
        cx->free(script->mics);
        script->mics = NULL;
    }
#endif

# if 0 /* def JS_TRACER */
    if (script->trees) {
        cx->free(script->trees);
        script->trees = NULL;
    }
# endif
}

#ifdef JS_METHODJIT_PROFILE_STUBS
void JS_FASTCALL
mjit::ProfileStubCall(VMFrame &f)
{
    JSOp op = JSOp(*f.regs.pc);
    StubCallsForOp[op]++;
}
#endif

