/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"

#include "jstypedarrayinlines.h"

#include "ion/AsmJS.h"
#include "ion/AsmJSModule.h"
#include "assembler/assembler/MacroAssembler.h"

using namespace js;
using namespace js::ion;

#ifdef JS_ASMJS

// Prevent races trying to install the signal handlers.
#ifdef JS_THREADSAFE
# include "jslock.h"

class SignalMutex
{
    PRLock *mutex_;

  public:
    SignalMutex() {
        mutex_ = PR_NewLock();
        if (!mutex_)
            MOZ_CRASH();
    }
    ~SignalMutex() {
        PR_DestroyLock(mutex_);
    }
    class Lock {
        static bool sHandlersInstalled;
      public:
        Lock();
        ~Lock();
        bool handlersInstalled() const { return sHandlersInstalled; }
        void setHandlersInstalled() { sHandlersInstalled = true; }
    };
} signalMutex;

bool SignalMutex::Lock::sHandlersInstalled = false;

SignalMutex::Lock::Lock()
{
    PR_Lock(signalMutex.mutex_);
}

SignalMutex::Lock::~Lock()
{
    PR_Unlock(signalMutex.mutex_);
}
#else
struct SignalMutex
{
    class Lock {
        static bool sHandlersInstalled;
      public:
        Lock() { (void)this; }
        bool handlersInstalled() const { return sHandlersInstalled; }
        void setHandlersInstalled() { sHandlersInstalled = true; }
    };
};

bool SignalMutex::Lock::sHandlersInstalled = false;
#endif

static AsmJSActivation *
InnermostAsmJSActivation()
{
    PerThreadData *threadData = TlsPerThreadData.get();
    if (!threadData)
        return NULL;

    return threadData->asmJSActivationStackFromOwnerThread();
}

static bool
PCIsInModule(const AsmJSModule &module, void *pc)
{
    uint8_t *code = module.functionCode();
    return pc >= code && pc < (code + module.functionBytes());
}

# if defined(JS_CPU_X64)
template <class T>
static void
SetXMMRegToNaN(bool isFloat32, T *xmm_reg)
{
    if (isFloat32) {
        JS_STATIC_ASSERT(sizeof(T) == 4 * sizeof(float));
        float *floats = reinterpret_cast<float*>(xmm_reg);
        floats[0] = js_NaN;
        floats[1] = 0;
        floats[2] = 0;
        floats[3] = 0;
    } else {
        JS_STATIC_ASSERT(sizeof(T) == 2 * sizeof(double));
        double *dbls = reinterpret_cast<double*>(xmm_reg);
        dbls[0] = js_NaN;
        dbls[1] = 0;
    }
}

// Perform a binary search on the projected offsets of the known heap accesses
// in the module.
static const AsmJSHeapAccess *
LookupHeapAccess(const AsmJSModule &module, uint8_t *pc)
{
    JS_ASSERT(PCIsInModule(module, pc));
    size_t targetOffset = pc - module.functionCode();

    if (module.numHeapAccesses() == 0)
        return NULL;

    size_t low = 0;
    size_t high = module.numHeapAccesses() - 1;
    while (high - low >= 2) {
        size_t mid = low + (high - low) / 2;
        uint32_t midOffset = module.heapAccess(mid).offset();
        if (targetOffset == midOffset)
            return &module.heapAccess(mid);
        if (targetOffset < midOffset)
            high = mid;
        else
            low = mid;
    }
    if (targetOffset == module.heapAccess(low).offset())
        return &module.heapAccess(low);
    if (targetOffset == module.heapAccess(high).offset())
        return &module.heapAccess(high);

    return NULL;
}
# endif

# if defined(XP_WIN)
#  include "jswin.h"

static uint8_t **
ContextToPC(PCONTEXT context)
{
#  if defined(JS_CPU_X64)
    JS_STATIC_ASSERT(sizeof(context->Rip) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&context->Rip);
#  else
    JS_STATIC_ASSERT(sizeof(context->Eip) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&context->Eip);
#  endif
}

#  if defined(JS_CPU_X64)
static void
SetRegisterToCoercedUndefined(CONTEXT *context, bool isFloat32, AnyRegister reg)
{
    if (reg.isFloat()) {
        switch (reg.fpu().code()) {
          case JSC::X86Registers::xmm0:  SetXMMRegToNaN(isFloat32, &context->Xmm0); break;
          case JSC::X86Registers::xmm1:  SetXMMRegToNaN(isFloat32, &context->Xmm1); break;
          case JSC::X86Registers::xmm2:  SetXMMRegToNaN(isFloat32, &context->Xmm2); break;
          case JSC::X86Registers::xmm3:  SetXMMRegToNaN(isFloat32, &context->Xmm3); break;
          case JSC::X86Registers::xmm4:  SetXMMRegToNaN(isFloat32, &context->Xmm4); break;
          case JSC::X86Registers::xmm5:  SetXMMRegToNaN(isFloat32, &context->Xmm5); break;
          case JSC::X86Registers::xmm6:  SetXMMRegToNaN(isFloat32, &context->Xmm6); break;
          case JSC::X86Registers::xmm7:  SetXMMRegToNaN(isFloat32, &context->Xmm7); break;
          case JSC::X86Registers::xmm8:  SetXMMRegToNaN(isFloat32, &context->Xmm8); break;
          case JSC::X86Registers::xmm9:  SetXMMRegToNaN(isFloat32, &context->Xmm9); break;
          case JSC::X86Registers::xmm10: SetXMMRegToNaN(isFloat32, &context->Xmm10); break;
          case JSC::X86Registers::xmm11: SetXMMRegToNaN(isFloat32, &context->Xmm11); break;
          case JSC::X86Registers::xmm12: SetXMMRegToNaN(isFloat32, &context->Xmm12); break;
          case JSC::X86Registers::xmm13: SetXMMRegToNaN(isFloat32, &context->Xmm13); break;
          case JSC::X86Registers::xmm14: SetXMMRegToNaN(isFloat32, &context->Xmm14); break;
          case JSC::X86Registers::xmm15: SetXMMRegToNaN(isFloat32, &context->Xmm15); break;
          default: MOZ_CRASH();
        }
    } else {
        switch (reg.gpr().code()) {
          case JSC::X86Registers::eax: context->Rax = 0; break;
          case JSC::X86Registers::ecx: context->Rcx = 0; break;
          case JSC::X86Registers::edx: context->Rdx = 0; break;
          case JSC::X86Registers::ebx: context->Rbx = 0; break;
          case JSC::X86Registers::esp: context->Rsp = 0; break;
          case JSC::X86Registers::ebp: context->Rbp = 0; break;
          case JSC::X86Registers::esi: context->Rsi = 0; break;
          case JSC::X86Registers::edi: context->Rdi = 0; break;
          case JSC::X86Registers::r8:  context->R8  = 0; break;
          case JSC::X86Registers::r9:  context->R9  = 0; break;
          case JSC::X86Registers::r10: context->R10 = 0; break;
          case JSC::X86Registers::r11: context->R11 = 0; break;
          case JSC::X86Registers::r12: context->R12 = 0; break;
          case JSC::X86Registers::r13: context->R13 = 0; break;
          case JSC::X86Registers::r14: context->R14 = 0; break;
          case JSC::X86Registers::r15: context->R15 = 0; break;
          default: MOZ_CRASH();
        }
    }
}
#  endif

static bool
HandleException(PEXCEPTION_POINTERS exception)
{
    EXCEPTION_RECORD *record = exception->ExceptionRecord;
    CONTEXT *context = exception->ContextRecord;

    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return false;

    AsmJSActivation *activation = InnermostAsmJSActivation();
    if (!activation)
        return false;

    uint8_t **ppc = ContextToPC(context);
    uint8_t *pc = *ppc;
	JS_ASSERT(pc == record->ExceptionAddress);

    const AsmJSModule &module = activation->module();
    if (!PCIsInModule(module, pc))
        return false;

	if (record->NumberParameters < 2)
		return false;

    void *faultingAddress = (void*)record->ExceptionInformation[1];

    // If we faulted trying to execute code in 'module', this must be an
    // operation callback (see TriggerOperationCallbackForAsmJSCode). Redirect
    // execution to a trampoline which will call js_HandleExecutionInterrupt.
    // The trampoline will jump to activation->resumePC if execution isn't
    // interrupted.
    if (PCIsInModule(module, faultingAddress)) {
        activation->setResumePC(pc);
        *ppc = module.operationCallbackExit();
        DWORD oldProtect;
        if (!VirtualProtect(module.functionCode(), module.functionBytes(), PAGE_EXECUTE, &oldProtect))
            MOZ_CRASH();
        return true;
    }

# if defined(JS_CPU_X64)
    // These checks aren't necessary, but, since we can, check anyway to make
    // sure we aren't covering up a real bug.
    if (!module.maybeHeap() ||
        faultingAddress < module.maybeHeap() ||
        faultingAddress >= module.maybeHeap() + AsmJSBufferProtectedSize)
    {
        return false;
    }

    const AsmJSHeapAccess *heapAccess = LookupHeapAccess(module, pc);
    if (!heapAccess)
        return false;

    // Also not necessary, but, since we can, do.
    if (heapAccess->isLoad() != !record->ExceptionInformation[0])
        return false;

    // We now know that this is an out-of-bounds access made by an asm.js
    // load/store that we should handle. If this is a load, assign the
    // JS-defined result value to the destination register (ToInt32(undefined)
    // or ToNumber(undefined), determined by the type of the destination
    // register) and set the PC to the next op. Upon return from the handler,
    // execution will resume at this next PC.
    if (heapAccess->isLoad())
        SetRegisterToCoercedUndefined(context, heapAccess->isFloat32Load(), heapAccess->loadedReg());
    *ppc += heapAccess->opLength();
    return true;
# else
    return false;
# endif
}

static LONG WINAPI
AsmJSExceptionHandler(LPEXCEPTION_POINTERS exception)
{
    if (HandleException(exception))
        return EXCEPTION_CONTINUE_EXECUTION;

    // No need to worry about calling other handlers, the OS does this for us.
    return EXCEPTION_CONTINUE_SEARCH;
}

# else  // If not Windows, assume Unix
#  include <signal.h>
#  include <sys/mman.h>

// Unfortunately, we still need OS-specific code to read/write to the thread
// state via the mcontext_t.
#  if defined(__linux__)
static uint8_t **
ContextToPC(mcontext_t &context)
{
#   if defined(JS_CPU_X86)
    JS_STATIC_ASSERT(sizeof(context.gregs[REG_EIP]) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&context.gregs[REG_EIP]);
#   else
    JS_STATIC_ASSERT(sizeof(context.gregs[REG_RIP]) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&context.gregs[REG_RIP]);
#   endif
}

#   if defined(JS_CPU_X64)
static void
SetRegisterToCoercedUndefined(mcontext_t &context, bool isFloat32, AnyRegister reg)
{
    if (reg.isFloat()) {
        switch (reg.fpu().code()) {
          case JSC::X86Registers::xmm0:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[0]); break;
          case JSC::X86Registers::xmm1:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[1]); break;
          case JSC::X86Registers::xmm2:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[2]); break;
          case JSC::X86Registers::xmm3:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[3]); break;
          case JSC::X86Registers::xmm4:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[4]); break;
          case JSC::X86Registers::xmm5:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[5]); break;
          case JSC::X86Registers::xmm6:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[6]); break;
          case JSC::X86Registers::xmm7:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[7]); break;
          case JSC::X86Registers::xmm8:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[8]); break;
          case JSC::X86Registers::xmm9:  SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[9]); break;
          case JSC::X86Registers::xmm10: SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[10]); break;
          case JSC::X86Registers::xmm11: SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[11]); break;
          case JSC::X86Registers::xmm12: SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[12]); break;
          case JSC::X86Registers::xmm13: SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[13]); break;
          case JSC::X86Registers::xmm14: SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[14]); break;
          case JSC::X86Registers::xmm15: SetXMMRegToNaN(isFloat32, &context.fpregs->_xmm[15]); break;
          default: MOZ_CRASH();
        }
    } else {
        switch (reg.gpr().code()) {
          case JSC::X86Registers::eax: context.gregs[REG_RAX] = 0; break;
          case JSC::X86Registers::ecx: context.gregs[REG_RCX] = 0; break;
          case JSC::X86Registers::edx: context.gregs[REG_RDX] = 0; break;
          case JSC::X86Registers::ebx: context.gregs[REG_RBX] = 0; break;
          case JSC::X86Registers::esp: context.gregs[REG_RSP] = 0; break;
          case JSC::X86Registers::ebp: context.gregs[REG_RBP] = 0; break;
          case JSC::X86Registers::esi: context.gregs[REG_RSI] = 0; break;
          case JSC::X86Registers::edi: context.gregs[REG_RDI] = 0; break;
          case JSC::X86Registers::r8:  context.gregs[REG_R8]  = 0; break;
          case JSC::X86Registers::r9:  context.gregs[REG_R9]  = 0; break;
          case JSC::X86Registers::r10: context.gregs[REG_R10] = 0; break;
          case JSC::X86Registers::r11: context.gregs[REG_R11] = 0; break;
          case JSC::X86Registers::r12: context.gregs[REG_R12] = 0; break;
          case JSC::X86Registers::r13: context.gregs[REG_R13] = 0; break;
          case JSC::X86Registers::r14: context.gregs[REG_R14] = 0; break;
          case JSC::X86Registers::r15: context.gregs[REG_R15] = 0; break;
          default: MOZ_CRASH();
        }
    }
}
#   endif
#  elif defined(XP_MACOSX)
static uint8_t **
ContextToPC(mcontext_t context)
{
#   if defined(JS_CPU_X86)
    JS_STATIC_ASSERT(sizeof(context->__ss.__eip) == sizeof(void*));
    return reinterpret_cast<uint8_t **>(&context->__ss.__eip);
#   else
    JS_STATIC_ASSERT(sizeof(context->__ss.__rip) == sizeof(void*));
    return reinterpret_cast<uint8_t **>(&context->__ss.__rip);
#   endif
}

#   if defined(JS_CPU_X64)
static void
SetRegisterToCoercedUndefined(mcontext_t &context, bool isFloat32, AnyRegister reg)
{
    if (reg.isFloat()) {
        switch (reg.fpu().code()) {
          case JSC::X86Registers::xmm0:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm0); break;
          case JSC::X86Registers::xmm1:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm1); break;
          case JSC::X86Registers::xmm2:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm2); break;
          case JSC::X86Registers::xmm3:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm3); break;
          case JSC::X86Registers::xmm4:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm4); break;
          case JSC::X86Registers::xmm5:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm5); break;
          case JSC::X86Registers::xmm6:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm6); break;
          case JSC::X86Registers::xmm7:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm7); break;
          case JSC::X86Registers::xmm8:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm8); break;
          case JSC::X86Registers::xmm9:  SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm9); break;
          case JSC::X86Registers::xmm10: SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm10); break;
          case JSC::X86Registers::xmm11: SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm11); break;
          case JSC::X86Registers::xmm12: SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm12); break;
          case JSC::X86Registers::xmm13: SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm13); break;
          case JSC::X86Registers::xmm14: SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm14); break;
          case JSC::X86Registers::xmm15: SetXMMRegToNaN(isFloat32, &context->__fs.__fpu_xmm15); break;
          default: MOZ_CRASH();
        }
    } else {
        switch (reg.gpr().code()) {
          case JSC::X86Registers::eax: context->__ss.__rax = 0; break;
          case JSC::X86Registers::ecx: context->__ss.__rcx = 0; break;
          case JSC::X86Registers::edx: context->__ss.__rdx = 0; break;
          case JSC::X86Registers::ebx: context->__ss.__rbx = 0; break;
          case JSC::X86Registers::esp: context->__ss.__rsp = 0; break;
          case JSC::X86Registers::ebp: context->__ss.__rbp = 0; break;
          case JSC::X86Registers::esi: context->__ss.__rsi = 0; break;
          case JSC::X86Registers::edi: context->__ss.__rdi = 0; break;
          case JSC::X86Registers::r8:  context->__ss.__r8  = 0; break;
          case JSC::X86Registers::r9:  context->__ss.__r9  = 0; break;
          case JSC::X86Registers::r10: context->__ss.__r10 = 0; break;
          case JSC::X86Registers::r11: context->__ss.__r11 = 0; break;
          case JSC::X86Registers::r12: context->__ss.__r12 = 0; break;
          case JSC::X86Registers::r13: context->__ss.__r13 = 0; break;
          case JSC::X86Registers::r14: context->__ss.__r14 = 0; break;
          case JSC::X86Registers::r15: context->__ss.__r15 = 0; break;
          default: MOZ_CRASH();
        }
    }
}
#   endif
#  endif  // end of OS-specific mcontext accessors

// Be very cautious and default to not handling; we don't want to accidentally
// silence real crashes from real bugs.
static bool
HandleSignal(int signum, siginfo_t *info, void *ctx)
{
    AsmJSActivation *activation = InnermostAsmJSActivation();
    if (!activation)
        return false;

    mcontext_t &context = reinterpret_cast<ucontext_t*>(ctx)->uc_mcontext;
    uint8_t **ppc = ContextToPC(context);
    uint8_t *pc = *ppc;

    const AsmJSModule &module = activation->module();
    if (!PCIsInModule(module, pc))
        return false;

    void *faultingAddress = info->si_addr;

    // If we faulted trying to execute code in 'module', this must be an
    // operation callback (see TriggerOperationCallbackForAsmJSCode). Redirect
    // execution to a trampoline which will call js_HandleExecutionInterrupt.
    // The trampoline will jump to activation->resumePC if execution isn't
    // interrupted.
    if (PCIsInModule(module, faultingAddress)) {
        activation->setResumePC(pc);
        *ppc = module.operationCallbackExit();
        mprotect(module.functionCode(), module.functionBytes(), PROT_EXEC);
        return true;
    }

#  if defined(JS_CPU_X64)
    // These checks aren't necessary, but, since we can, check anyway to make
    // sure we aren't covering up a real bug.
    if (!module.maybeHeap() ||
        faultingAddress < module.maybeHeap() ||
        faultingAddress >= module.maybeHeap() + AsmJSBufferProtectedSize)
    {
        return false;
    }

    const AsmJSHeapAccess *heapAccess = LookupHeapAccess(module, pc);
    if (!heapAccess)
        return false;

    // We now know that this is an out-of-bounds access made by an asm.js
    // load/store that we should handle. If this is a load, assign the
    // JS-defined result value to the destination register (ToInt32(undefined)
    // or ToNumber(undefined), determined by the type of the destination
    // register) and set the PC to the next op. Upon return from the handler,
    // execution will resume at this next PC.
    if (heapAccess->isLoad())
        SetRegisterToCoercedUndefined(context, heapAccess->isFloat32Load(), heapAccess->loadedReg());
    *ppc += heapAccess->opLength();
    return true;
#  else
    return false;
#  endif
}

static struct sigaction sPrevHandler;

static void
AsmJSFaultHandler(int signum, siginfo_t *info, void *context)
{
    if (HandleSignal(signum, info, context))
        return;

    // This signal is not for any asm.js code we expect, so we need to forward
    // the signal to the next handler. If there is no next handler (SIG_IGN or
    // SIG_DFL), then it's time to crash. To do this, we set the signal back to
    // it's previous disposition and return. This will cause the faulting op to
    // be re-executed which will crash in the normal way. The advantage to
    // doing this is that we remove ourselves from the crash stack which
    // simplifies crash reports. Note: the order of these tests matter.
    if (sPrevHandler.sa_flags & SA_SIGINFO) {
        sPrevHandler.sa_sigaction(signum, info, context);
        exit(signum);  // backstop
    } else if (sPrevHandler.sa_handler == SIG_DFL || sPrevHandler.sa_handler == SIG_IGN) {
        sigaction(signum, &sPrevHandler, NULL);
    } else {
        sPrevHandler.sa_handler(signum);
        exit(signum);  // backstop
    }
}
# endif
#endif // JS_ASMJS

bool
EnsureAsmJSSignalHandlersInstalled()
{
#if defined(JS_ASMJS)
    SignalMutex::Lock lock;
    if (lock.handlersInstalled())
        return true;

#if defined(XP_WIN)
    if (!AddVectoredExceptionHandler(/* FirstHandler = */true, AsmJSExceptionHandler))
        return false;
#else
    struct sigaction sigAction;
    sigAction.sa_sigaction = &AsmJSFaultHandler;
    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &sigAction, &sPrevHandler))
        return false;
    if (sigaction(SIGBUS, &sigAction, &sPrevHandler))
        return false;
#endif

    lock.setHandlersInstalled();
#endif
    return true;
}

// To interrupt execution of a JSRuntime, any thread may call
// JS_TriggerOperationCallback (JSRuntime::triggerOperationCallback from inside
// the engine). Normally, this sets some state that is polled at regular
// intervals (function prologues, loop headers), even from jit-code. For tight
// loops, this poses non-trivial overhead. For asm.js, we can do better: when
// another thread triggers the operation callback, we simply mprotect all of
// the innermost asm.js module activation's code. This will trigger a SIGSEGV,
// taking us into AsmJSFaultHandler. From there, we can manually redirect
// execution to call js_HandleExecutionInterrupt. The memory is un-protected
// from the signal handler after control flow is redirected.
void
js::TriggerOperationCallbackForAsmJSCode(JSRuntime *rt)
{
#if defined(JS_ASMJS)
    PerThreadData::AsmJSActivationStackLock lock(rt->mainThread);

    AsmJSActivation *activation = rt->mainThread.asmJSActivationStackFromAnyThread();
    if (!activation)
        return;

    const AsmJSModule &module = activation->module();

# if defined(XP_WIN)
    DWORD oldProtect;
    if (!VirtualProtect(module.functionCode(), 4096, PAGE_NOACCESS, &oldProtect))
        MOZ_CRASH();
# else
    if (mprotect(module.functionCode(), module.functionBytes(), PROT_NONE))
        MOZ_CRASH();
# endif
#endif
}
