/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
using namespace mozilla;

#if defined(XP_WIN)
# define XMM_sig(p,i) ((p)->Xmm##i)
# define EIP_sig(p) ((p)->Eip)
# define RIP_sig(p) ((p)->Rip)
# define RAX_sig(p) ((p)->Rax)
# define RCX_sig(p) ((p)->Rcx)
# define RDX_sig(p) ((p)->Rdx)
# define RBX_sig(p) ((p)->Rbx)
# define RSP_sig(p) ((p)->Rsp)
# define RBP_sig(p) ((p)->Rbp)
# define RSI_sig(p) ((p)->Rsi)
# define RDI_sig(p) ((p)->Rdi)
# define R8_sig(p) ((p)->R8)
# define R9_sig(p) ((p)->R9)
# define R10_sig(p) ((p)->R10)
# define R11_sig(p) ((p)->R11)
# define R12_sig(p) ((p)->R12)
# define R13_sig(p) ((p)->R13)
# define R14_sig(p) ((p)->R14)
# define R15_sig(p) ((p)->R15)
#elif defined(__OpenBSD__)
# define XMM_sig(p,i) ((p)->sc_fpstate->fx_xmm[i])
# define EIP_sig(p) ((p)->sc_eip)
# define RIP_sig(p) ((p)->sc_rip)
# define RAX_sig(p) ((p)->sc_rax)
# define RCX_sig(p) ((p)->sc_rcx)
# define RDX_sig(p) ((p)->sc_rdx)
# define RBX_sig(p) ((p)->sc_rbx)
# define RSP_sig(p) ((p)->sc_rsp)
# define RBP_sig(p) ((p)->sc_rbp)
# define RSI_sig(p) ((p)->sc_rsi)
# define RDI_sig(p) ((p)->sc_rdi)
# define R8_sig(p) ((p)->sc_r8)
# define R9_sig(p) ((p)->sc_r9)
# define R10_sig(p) ((p)->sc_r10)
# define R11_sig(p) ((p)->sc_r11)
# define R12_sig(p) ((p)->sc_r12)
# define R13_sig(p) ((p)->sc_r13)
# define R14_sig(p) ((p)->sc_r14)
# define R15_sig(p) ((p)->sc_r15)
#elif defined(__linux__) || defined(SOLARIS)
# if defined(__linux__)
#  define XMM_sig(p,i) ((p)->uc_mcontext.fpregs->_xmm[i])
# else
#  define XMM_sig(p,i) ((p)->uc_mcontext.fpregs.fp_reg_set.fpchip_state.xmm[i])
# endif
# define EIP_sig(p) ((p)->uc_mcontext.gregs[REG_EIP])
# define RIP_sig(p) ((p)->uc_mcontext.gregs[REG_RIP])
# define PC_sig(p) ((p)->uc_mcontext.arm_pc)
# define RAX_sig(p) ((p)->uc_mcontext.gregs[REG_RAX])
# define RCX_sig(p) ((p)->uc_mcontext.gregs[REG_RCX])
# define RDX_sig(p) ((p)->uc_mcontext.gregs[REG_RDX])
# define RBX_sig(p) ((p)->uc_mcontext.gregs[REG_RBX])
# define RSP_sig(p) ((p)->uc_mcontext.gregs[REG_RSP])
# define RBP_sig(p) ((p)->uc_mcontext.gregs[REG_RBP])
# define RSI_sig(p) ((p)->uc_mcontext.gregs[REG_RSI])
# define RDI_sig(p) ((p)->uc_mcontext.gregs[REG_RDI])
# define R8_sig(p) ((p)->uc_mcontext.gregs[REG_R8])
# define R9_sig(p) ((p)->uc_mcontext.gregs[REG_R9])
# define R10_sig(p) ((p)->uc_mcontext.gregs[REG_R10])
# define R11_sig(p) ((p)->uc_mcontext.gregs[REG_R11])
# define R12_sig(p) ((p)->uc_mcontext.gregs[REG_R12])
# define R13_sig(p) ((p)->uc_mcontext.gregs[REG_R13])
# define R14_sig(p) ((p)->uc_mcontext.gregs[REG_R14])
# define R15_sig(p) ((p)->uc_mcontext.gregs[REG_R15])
#elif defined(__NetBSD__)
# define XMM_sig(p,i) (((struct fxsave64 *)(p)->uc_mcontext.__fpregs)->fx_xmm[i])
# define EIP_sig(p) ((p)->uc_mcontext.__gregs[_REG_EIP])
# define RIP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RIP])
# define RAX_sig(p) ((p)->uc_mcontext.__gregs[_REG_RAX])
# define RCX_sig(p) ((p)->uc_mcontext.__gregs[_REG_RCX])
# define RDX_sig(p) ((p)->uc_mcontext.__gregs[_REG_RDX])
# define RBX_sig(p) ((p)->uc_mcontext.__gregs[_REG_RBX])
# define RSP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RSP])
# define RBP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RBP])
# define RSI_sig(p) ((p)->uc_mcontext.__gregs[_REG_RSI])
# define RDI_sig(p) ((p)->uc_mcontext.__gregs[_REG_RDI])
# define R8_sig(p) ((p)->uc_mcontext.__gregs[_REG_R8])
# define R9_sig(p) ((p)->uc_mcontext.__gregs[_REG_R9])
# define R10_sig(p) ((p)->uc_mcontext.__gregs[_REG_R10])
# define R11_sig(p) ((p)->uc_mcontext.__gregs[_REG_R11])
# define R12_sig(p) ((p)->uc_mcontext.__gregs[_REG_R12])
# define R13_sig(p) ((p)->uc_mcontext.__gregs[_REG_R13])
# define R14_sig(p) ((p)->uc_mcontext.__gregs[_REG_R14])
# define R15_sig(p) ((p)->uc_mcontext.__gregs[_REG_R15])
#elif defined(__DragonFly__) || defined(__FreeBSD__)
# if defined(__DragonFly__)
#  define XMM_sig(p,i) (((union savefpu *)(p)->uc_mcontext.mc_fpregs)->sv_xmm.sv_xmm[i])
# else
#  define XMM_sig(p,i) (((struct savefpu *)(p)->uc_mcontext.mc_fpstate)->sv_xmm[i])
# endif
# define EIP_sig(p) ((p)->uc_mcontext.mc_eip)
# define RIP_sig(p) ((p)->uc_mcontext.mc_rip)
# define RAX_sig(p) ((p)->uc_mcontext.mc_rax)
# define RCX_sig(p) ((p)->uc_mcontext.mc_rcx)
# define RDX_sig(p) ((p)->uc_mcontext.mc_rdx)
# define RBX_sig(p) ((p)->uc_mcontext.mc_rbx)
# define RSP_sig(p) ((p)->uc_mcontext.mc_rsp)
# define RBP_sig(p) ((p)->uc_mcontext.mc_rbp)
# define RSI_sig(p) ((p)->uc_mcontext.mc_rsi)
# define RDI_sig(p) ((p)->uc_mcontext.mc_rdi)
# define R8_sig(p) ((p)->uc_mcontext.mc_r8)
# define R9_sig(p) ((p)->uc_mcontext.mc_r9)
# define R10_sig(p) ((p)->uc_mcontext.mc_r10)
# define R11_sig(p) ((p)->uc_mcontext.mc_r11)
# define R12_sig(p) ((p)->uc_mcontext.mc_r12)
# define R13_sig(p) ((p)->uc_mcontext.mc_r13)
# define R14_sig(p) ((p)->uc_mcontext.mc_r14)
# define R15_sig(p) ((p)->uc_mcontext.mc_r15)
#elif defined(XP_MACOSX)
// Mach requires special treatment.
#else
# error "Don't know how to read/write to the thread state via the mcontext_t."
#endif

// For platforms where the signal/exception handler runs on the same
// thread/stack as the victim (Unix and Windows), we can use TLS to find any
// currently executing asm.js code.
#if !defined(XP_MACOSX)
static AsmJSActivation *
InnermostAsmJSActivation()
{
    PerThreadData *threadData = TlsPerThreadData.get();
    if (!threadData)
        return NULL;

    return threadData->asmJSActivationStackFromOwnerThread();
}
#endif

// For platforms that install a single, process-wide signal handler (Unix and
// Windows), the InstallSignalHandlersMutex prevents races between JSRuntimes
// installing signal handlers.
#if !defined(XP_MACOSX)
# ifdef JS_THREADSAFE
#  include "jslock.h"

class InstallSignalHandlersMutex
{
    PRLock *mutex_;

  public:
    InstallSignalHandlersMutex() {
        mutex_ = PR_NewLock();
        if (!mutex_)
            MOZ_CRASH();
    }
    ~InstallSignalHandlersMutex() {
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

bool InstallSignalHandlersMutex::Lock::sHandlersInstalled = false;

InstallSignalHandlersMutex::Lock::Lock()
{
    PR_Lock(signalMutex.mutex_);
}

InstallSignalHandlersMutex::Lock::~Lock()
{
    PR_Unlock(signalMutex.mutex_);
}
# else  // JS_THREADSAFE
struct InstallSignalHandlersMutex
{
    class Lock {
        static bool sHandlersInstalled;
      public:
        Lock() { (void)this; }
        bool handlersInstalled() const { return sHandlersInstalled; }
        void setHandlersInstalled() { sHandlersInstalled = true; }
    };
};

bool InstallSignalHandlersMutex::Lock::sHandlersInstalled = false;
# endif  // JS_THREADSAFE
#endif   // !XP_MACOSX

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
    JS_ASSERT(module.containsPC(pc));
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
# else
#  include <signal.h>
#  include <sys/mman.h>
# endif

# if defined(__FreeBSD__)
#  include <sys/ucontext.h> // for ucontext_t, mcontext_t
# endif

# if defined(JS_CPU_X64)
#  if defined(__DragonFly__)
#   include <machine/npx.h> // for union savefpu
#  elif defined(__FreeBSD__) || defined(__OpenBSD__)
#   include <machine/fpu.h> // for struct savefpu/fxsave64
#  endif
# endif

// Not all versions of the Android NDK define ucontext_t or mcontext_t.
// Detect this and provide custom but compatible definitions. Note that these
// follow the GLibc naming convention to access register values from
// mcontext_t.
//
// See: https://chromiumcodereview.appspot.com/10829122/
// See: http://code.google.com/p/android/issues/detail?id=34784
# if (defined(ANDROID)) && !defined(__BIONIC_HAVE_UCONTEXT_T)
#  if defined(__arm__)
// GLibc on ARM defines mcontext_t has a typedef for 'struct sigcontext'.
// Old versions of the C library <signal.h> didn't define the type.
#if !defined(__BIONIC_HAVE_STRUCT_SIGCONTEXT)
#include <asm/sigcontext.h>
#endif

typedef struct sigcontext mcontext_t;

typedef struct ucontext {
    uint32_t uc_flags;
    struct ucontext* uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    // Other fields are not used so don't define them here.
} ucontext_t;

#  elif defined(__i386__)
// x86 version for Android.
typedef struct {
    uint32_t gregs[19];
    void* fpregs;
    uint32_t oldmask;
    uint32_t cr2;
} mcontext_t;

typedef uint32_t kernel_sigset_t[2];  // x86 kernel uses 64-bit signal masks
typedef struct ucontext {
    uint32_t uc_flags;
    struct ucontext* uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    // Other fields are not used by V8, don't define them here.
} ucontext_t;
enum { REG_EIP = 14 };
#  endif
# endif  // defined(__ANDROID__) && !defined(__BIONIC_HAVE_UCONTEXT_T)


# if !defined(XP_WIN)
#  define CONTEXT ucontext_t
# endif

# if !defined(XP_MACOSX)
static uint8_t **
ContextToPC(CONTEXT *context)
{
#  if defined(JS_CPU_X64)
    JS_STATIC_ASSERT(sizeof(RIP_sig(context)) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&RIP_sig(context));
#  elif defined(JS_CPU_X86)
    JS_STATIC_ASSERT(sizeof(EIP_sig(context)) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&EIP_sig(context));
#  elif defined(JS_CPU_ARM)
    JS_STATIC_ASSERT(sizeof(PC_sig(context)) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&PC_sig(context));
#  endif
}

#  if defined(JS_CPU_X64)
static void
SetRegisterToCoercedUndefined(CONTEXT *context, bool isFloat32, AnyRegister reg)
{
    if (reg.isFloat()) {
        switch (reg.fpu().code()) {
          case JSC::X86Registers::xmm0:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 0)); break;
          case JSC::X86Registers::xmm1:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 1)); break;
          case JSC::X86Registers::xmm2:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 2)); break;
          case JSC::X86Registers::xmm3:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 3)); break;
          case JSC::X86Registers::xmm4:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 4)); break;
          case JSC::X86Registers::xmm5:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 5)); break;
          case JSC::X86Registers::xmm6:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 6)); break;
          case JSC::X86Registers::xmm7:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 7)); break;
          case JSC::X86Registers::xmm8:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 8)); break;
          case JSC::X86Registers::xmm9:  SetXMMRegToNaN(isFloat32, &XMM_sig(context, 9)); break;
          case JSC::X86Registers::xmm10: SetXMMRegToNaN(isFloat32, &XMM_sig(context, 10)); break;
          case JSC::X86Registers::xmm11: SetXMMRegToNaN(isFloat32, &XMM_sig(context, 11)); break;
          case JSC::X86Registers::xmm12: SetXMMRegToNaN(isFloat32, &XMM_sig(context, 12)); break;
          case JSC::X86Registers::xmm13: SetXMMRegToNaN(isFloat32, &XMM_sig(context, 13)); break;
          case JSC::X86Registers::xmm14: SetXMMRegToNaN(isFloat32, &XMM_sig(context, 14)); break;
          case JSC::X86Registers::xmm15: SetXMMRegToNaN(isFloat32, &XMM_sig(context, 15)); break;
          default: MOZ_CRASH();
        }
    } else {
        switch (reg.gpr().code()) {
          case JSC::X86Registers::eax: RAX_sig(context) = 0; break;
          case JSC::X86Registers::ecx: RCX_sig(context) = 0; break;
          case JSC::X86Registers::edx: RDX_sig(context) = 0; break;
          case JSC::X86Registers::ebx: RBX_sig(context) = 0; break;
          case JSC::X86Registers::esp: RSP_sig(context) = 0; break;
          case JSC::X86Registers::ebp: RBP_sig(context) = 0; break;
          case JSC::X86Registers::esi: RSI_sig(context) = 0; break;
          case JSC::X86Registers::edi: RDI_sig(context) = 0; break;
          case JSC::X86Registers::r8:  R8_sig(context)  = 0; break;
          case JSC::X86Registers::r9:  R9_sig(context)  = 0; break;
          case JSC::X86Registers::r10: R10_sig(context) = 0; break;
          case JSC::X86Registers::r11: R11_sig(context) = 0; break;
          case JSC::X86Registers::r12: R12_sig(context) = 0; break;
          case JSC::X86Registers::r13: R13_sig(context) = 0; break;
          case JSC::X86Registers::r14: R14_sig(context) = 0; break;
          case JSC::X86Registers::r15: R15_sig(context) = 0; break;
          default: MOZ_CRASH();
        }
    }
}
#  endif  // JS_CPU_X64
# endif   // !XP_MACOSX

# if defined(XP_WIN)

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
    if (!module.containsPC(pc))
        return false;

	if (record->NumberParameters < 2)
		return false;

    void *faultingAddress = (void*)record->ExceptionInformation[1];

    // If we faulted trying to execute code in 'module', this must be an
    // operation callback (see TriggerOperationCallbackForAsmJSCode). Redirect
    // execution to a trampoline which will call js_HandleExecutionInterrupt.
    // The trampoline will jump to activation->resumePC if execution isn't
    // interrupted.
    if (module.containsPC(faultingAddress)) {
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

# elif defined(XP_MACOSX)
#  include <mach/exc.h>

static uint8_t **
ContextToPC(x86_thread_state_t &state)
{
#  if defined(JS_CPU_X64)
    JS_STATIC_ASSERT(sizeof(state.uts.ts64.__rip) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&state.uts.ts64.__rip);
#  else
    JS_STATIC_ASSERT(sizeof(state.uts.ts32.__eip) == sizeof(void*));
    return reinterpret_cast<uint8_t**>(&state.uts.ts32.__eip);
#  endif
}

#  if defined(JS_CPU_X64)
static bool
SetRegisterToCoercedUndefined(mach_port_t rtThread, x86_thread_state64_t &state,
                              const AsmJSHeapAccess &heapAccess)
{
    if (heapAccess.loadedReg().isFloat()) {
        kern_return_t kret;

        x86_float_state64_t fstate;
        unsigned int count = x86_FLOAT_STATE64_COUNT;
        kret = thread_get_state(rtThread, x86_FLOAT_STATE64, (thread_state_t) &fstate, &count);
        if (kret != KERN_SUCCESS)
            return false;

        bool f32 = heapAccess.isFloat32Load();
        switch (heapAccess.loadedReg().fpu().code()) {
          case JSC::X86Registers::xmm0:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm0); break;
          case JSC::X86Registers::xmm1:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm1); break;
          case JSC::X86Registers::xmm2:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm2); break;
          case JSC::X86Registers::xmm3:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm3); break;
          case JSC::X86Registers::xmm4:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm4); break;
          case JSC::X86Registers::xmm5:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm5); break;
          case JSC::X86Registers::xmm6:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm6); break;
          case JSC::X86Registers::xmm7:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm7); break;
          case JSC::X86Registers::xmm8:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm8); break;
          case JSC::X86Registers::xmm9:  SetXMMRegToNaN(f32, &fstate.__fpu_xmm9); break;
          case JSC::X86Registers::xmm10: SetXMMRegToNaN(f32, &fstate.__fpu_xmm10); break;
          case JSC::X86Registers::xmm11: SetXMMRegToNaN(f32, &fstate.__fpu_xmm11); break;
          case JSC::X86Registers::xmm12: SetXMMRegToNaN(f32, &fstate.__fpu_xmm12); break;
          case JSC::X86Registers::xmm13: SetXMMRegToNaN(f32, &fstate.__fpu_xmm13); break;
          case JSC::X86Registers::xmm14: SetXMMRegToNaN(f32, &fstate.__fpu_xmm14); break;
          case JSC::X86Registers::xmm15: SetXMMRegToNaN(f32, &fstate.__fpu_xmm15); break;
          default: MOZ_CRASH();
        }

        kret = thread_set_state(rtThread, x86_FLOAT_STATE64, (thread_state_t)&fstate, x86_FLOAT_STATE64_COUNT);
        if (kret != KERN_SUCCESS)
            return false;
    } else {
        switch (heapAccess.loadedReg().gpr().code()) {
          case JSC::X86Registers::eax: state.__rax = 0; break;
          case JSC::X86Registers::ecx: state.__rcx = 0; break;
          case JSC::X86Registers::edx: state.__rdx = 0; break;
          case JSC::X86Registers::ebx: state.__rbx = 0; break;
          case JSC::X86Registers::esp: state.__rsp = 0; break;
          case JSC::X86Registers::ebp: state.__rbp = 0; break;
          case JSC::X86Registers::esi: state.__rsi = 0; break;
          case JSC::X86Registers::edi: state.__rdi = 0; break;
          case JSC::X86Registers::r8:  state.__r8  = 0; break;
          case JSC::X86Registers::r9:  state.__r9  = 0; break;
          case JSC::X86Registers::r10: state.__r10 = 0; break;
          case JSC::X86Registers::r11: state.__r11 = 0; break;
          case JSC::X86Registers::r12: state.__r12 = 0; break;
          case JSC::X86Registers::r13: state.__r13 = 0; break;
          case JSC::X86Registers::r14: state.__r14 = 0; break;
          case JSC::X86Registers::r15: state.__r15 = 0; break;
          default: MOZ_CRASH();
        }
    }
    return true;
}
#  endif

// This definition was generated by mig (the Mach Interface Generator) for the
// routine 'exception_raise' (exc.defs).
#pragma pack(4)
typedef struct {
    mach_msg_header_t Head;
    /* start of the kernel processed data */
    mach_msg_body_t msgh_body;
    mach_msg_port_descriptor_t thread;
    mach_msg_port_descriptor_t task;
    /* end of the kernel processed data */
    NDR_record_t NDR;
    exception_type_t exception;
    mach_msg_type_number_t codeCnt;
    int64_t code[2];
} Request__mach_exception_raise_t;
#pragma pack()

// The full Mach message also includes a trailer.
struct ExceptionRequest
{
    Request__mach_exception_raise_t body;
    mach_msg_trailer_t trailer;
};

static bool
HandleMachException(JSRuntime *rt, const ExceptionRequest &request)
{
    // Get the port of the JSRuntime's thread from the message.
    mach_port_t rtThread = request.body.thread.name;

    // Read out the JSRuntime thread's register state.
    x86_thread_state_t state;
    unsigned int count = x86_THREAD_STATE_COUNT;
    kern_return_t kret;
    kret = thread_get_state(rtThread, x86_THREAD_STATE, (thread_state_t)&state, &count);
    if (kret != KERN_SUCCESS)
        return false;

    AsmJSActivation *activation = rt->mainThread.asmJSActivationStackFromAnyThread();
    if (!activation)
        return false;

    uint8_t **ppc = ContextToPC(state);
    uint8_t *pc = *ppc;

    const AsmJSModule &module = activation->module();
    if (!module.containsPC(pc))
        return false;

    if (request.body.exception != EXC_BAD_ACCESS || request.body.codeCnt != 2)
        return false;

    void *faultingAddress = (void*)request.body.code[1];

    // If we faulted trying to execute code in 'module', this must be an
    // operation callback (see TriggerOperationCallbackForAsmJSCode). Redirect
    // execution to a trampoline which will call js_HandleExecutionInterrupt.
    // The trampoline will jump to activation->resumePC if execution isn't
    // interrupted.
    if (module.containsPC(faultingAddress)) {
        activation->setResumePC(pc);
        *ppc = module.operationCallbackExit();
        mprotect(module.functionCode(), module.functionBytes(), PROT_EXEC);

        // Update the thread state with the new pc.
        kret = thread_set_state(rtThread, x86_THREAD_STATE, (thread_state_t)&state, x86_THREAD_STATE_COUNT);
        return kret == KERN_SUCCESS;
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
    if (heapAccess->isLoad()) {
        if (!SetRegisterToCoercedUndefined(rtThread, state.uts.ts64, *heapAccess))
            return false;
    }
    *ppc += heapAccess->opLength();

    // Update the thread state with the new pc.
    kret = thread_set_state(rtThread, x86_THREAD_STATE, (thread_state_t)&state, x86_THREAD_STATE_COUNT);
    if (kret != KERN_SUCCESS)
        return false;

    return true;
#  else
    return false;
#  endif
}

// Taken from mach_exc in /usr/include/mach/mach_exc.defs.
static const mach_msg_id_t sExceptionId = 2405;

// The choice of id here is arbitrary, the only constraint is that sQuitId != sExceptionId.
static const mach_msg_id_t sQuitId = 42;

void *
AsmJSMachExceptionHandlerThread(void *threadArg)
{
    JSRuntime *rt = reinterpret_cast<JSRuntime*>(threadArg);
    mach_port_t port = rt->asmJSMachExceptionHandler.port();
    kern_return_t kret;

    while(true) {
        ExceptionRequest request;
        kret = mach_msg(&request.body.Head, MACH_RCV_MSG, 0, sizeof(request),
                        port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

        // If we fail even receiving the message, we can't even send a reply!
        // Rather than hanging the faulting thread (hanging the browser), crash.
        if (kret != KERN_SUCCESS) {
            fprintf(stderr, "AsmJSMachExceptionHandlerThread: mach_msg failed with %d\n", (int)kret);
            MOZ_CRASH();
        }

        // There are only two messages we should be receiving: an exception
        // message that occurs when the runtime's thread faults and the quit
        // message sent when the runtime is shutting down.
        if (request.body.Head.msgh_id == sQuitId)
            break;
        if (request.body.Head.msgh_id != sExceptionId) {
            fprintf(stderr, "Unexpected msg header id %d\n", (int)request.body.Head.msgh_bits);
            MOZ_CRASH();
        }

        // Some thread just commited an EXC_BAD_ACCESS and has been suspended by
        // the kernel. The kernel is waiting for us to reply with instructions.
        // Our default is the "not handled" reply (by setting the RetCode field
        // of the reply to KERN_FAILURE) which tells the kernel to continue
        // searching at the process and system level. If this is an asm.js
        // expected exception, we handle it and return KERN_SUCCESS.
        bool handled = HandleMachException(rt, request);
        kern_return_t replyCode = handled ? KERN_SUCCESS : KERN_FAILURE;

        // This magic incantation to send a reply back to the kernel was derived
        // from the exc_server generated by 'mig -v /usr/include/mach/mach_exc.defs'.
        __Reply__exception_raise_t reply;
        reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request.body.Head.msgh_bits), 0);
        reply.Head.msgh_size = sizeof(reply);
        reply.Head.msgh_remote_port = request.body.Head.msgh_remote_port;
        reply.Head.msgh_local_port = MACH_PORT_NULL;
        reply.Head.msgh_id = request.body.Head.msgh_id + 100;
        reply.NDR = NDR_record;
        reply.RetCode = replyCode;
        mach_msg(&reply.Head, MACH_SEND_MSG, sizeof(reply), 0, MACH_PORT_NULL,
                 MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    }

    return NULL;
}

AsmJSMachExceptionHandler::AsmJSMachExceptionHandler()
  : installed_(false),
    thread_(NULL),
    port_(MACH_PORT_NULL)
{}

void
AsmJSMachExceptionHandler::release()
{
    if (installed_) {
        clearCurrentThread();
        installed_ = false;
    }
    if (thread_ != NULL) {
        // Break the handler thread out of the mach_msg loop.
        mach_msg_header_t msg;
        msg.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
        msg.msgh_size = sizeof(msg);
        msg.msgh_remote_port = port_;
        msg.msgh_local_port = MACH_PORT_NULL;
        msg.msgh_reserved = 0;
        msg.msgh_id = sQuitId;
        kern_return_t kret = mach_msg(&msg, MACH_SEND_MSG, sizeof(msg), 0, MACH_PORT_NULL,
                                      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (kret != KERN_SUCCESS) {
            fprintf(stderr, "AsmJSMachExceptionHandler: failed to send quit message: %d\n", (int)kret);
            MOZ_CRASH();
        }

        // Wait for the handler thread to complete before deallocating the port.
        pthread_join(thread_, NULL);
        thread_ = NULL;
    }
    if (port_ != MACH_PORT_NULL) {
        DebugOnly<kern_return_t> kret = mach_port_destroy(mach_task_self(), port_);
        JS_ASSERT(kret == KERN_SUCCESS);
        port_ = MACH_PORT_NULL;
    }
}

void
AsmJSMachExceptionHandler::clearCurrentThread()
{
    if (!installed_)
        return;

    thread_port_t thread = mach_thread_self();
    kern_return_t kret = thread_set_exception_ports(thread,
                                                    EXC_MASK_BAD_ACCESS,
                                                    MACH_PORT_NULL,
                                                    EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                                    THREAD_STATE_NONE);
    mach_port_deallocate(mach_task_self(), thread);
    if (kret != KERN_SUCCESS)
        MOZ_CRASH();
}

void
AsmJSMachExceptionHandler::setCurrentThread()
{
    if (!installed_)
        return;

    thread_port_t thread = mach_thread_self();
    kern_return_t kret = thread_set_exception_ports(thread,
                                                    EXC_MASK_BAD_ACCESS,
                                                    port_,
                                                    EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                                    THREAD_STATE_NONE);
    mach_port_deallocate(mach_task_self(), thread);
    if (kret != KERN_SUCCESS)
        MOZ_CRASH();
}

bool
AsmJSMachExceptionHandler::install(JSRuntime *rt)
{
    JS_ASSERT(!installed());
    kern_return_t kret;
    mach_port_t thread;

    // Get a port which can send and receive data.
    kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port_);
    if (kret != KERN_SUCCESS)
        goto error;
    kret = mach_port_insert_right(mach_task_self(), port_, port_, MACH_MSG_TYPE_MAKE_SEND);
    if (kret != KERN_SUCCESS)
        goto error;

    // Create a thread to block on reading port_.
    if (pthread_create(&thread_, NULL, AsmJSMachExceptionHandlerThread, rt))
        goto error;

    // Direct exceptions on this thread to port_ (and thus our handler thread).
    // Note: we are totally clobbering any existing *thread* exception ports and
    // not even attempting to forward. Breakpad and gdb both use the *process*
    // exception ports which are only called if the thread doesn't handle the
    // exception, so we should be fine.
    thread = mach_thread_self();
    kret = thread_set_exception_ports(thread,
                                      EXC_MASK_BAD_ACCESS,
                                      port_,
                                      EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                      THREAD_STATE_NONE);
    mach_port_deallocate(mach_task_self(), thread);
    if (kret != KERN_SUCCESS)
        goto error;

    installed_ = true;
    return true;

  error:
    release();
    return false;
}

# else  // If not Windows or Mac, assume Unix

// Be very cautious and default to not handling; we don't want to accidentally
// silence real crashes from real bugs.
static bool
HandleSignal(int signum, siginfo_t *info, void *ctx)
{
    AsmJSActivation *activation = InnermostAsmJSActivation();
    if (!activation)
        return false;

    CONTEXT *context = (CONTEXT *)ctx;
    uint8_t **ppc = ContextToPC(context);
    uint8_t *pc = *ppc;

    const AsmJSModule &module = activation->module();
    if (!module.containsPC(pc))
        return false;

    void *faultingAddress = info->si_addr;

    // If we faulted trying to execute code in 'module', this must be an
    // operation callback (see TriggerOperationCallbackForAsmJSCode). Redirect
    // execution to a trampoline which will call js_HandleExecutionInterrupt.
    // The trampoline will jump to activation->resumePC if execution isn't
    // interrupted.
    if (module.containsPC(faultingAddress)) {
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

static struct sigaction sPrevSegvHandler;
static struct sigaction sPrevBusHandler;

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
    struct sigaction* prevHandler = NULL;
    if (signum == SIGSEGV)
        prevHandler = &sPrevSegvHandler;
    else {
	JS_ASSERT(signum == SIGBUS);
        prevHandler = &sPrevBusHandler;
    }

    if (prevHandler->sa_flags & SA_SIGINFO) {
        prevHandler->sa_sigaction(signum, info, context);
        exit(signum);  // backstop
    } else if (prevHandler->sa_handler == SIG_DFL || prevHandler->sa_handler == SIG_IGN) {
        sigaction(signum, prevHandler, NULL);
    } else {
        prevHandler->sa_handler(signum);
        exit(signum);  // backstop
    }
}
# endif

bool
EnsureAsmJSSignalHandlersInstalled(JSRuntime *rt)
{
#if defined(XP_MACOSX)
    // On OSX, each JSRuntime gets its own handler.
    return rt->asmJSMachExceptionHandler.installed() || rt->asmJSMachExceptionHandler.install(rt);
#else
    // Assume Windows or Unix. For these platforms, there is a single,
    // process-wide signal handler installed. Take care to only install it once.
    InstallSignalHandlersMutex::Lock lock;
    if (lock.handlersInstalled())
        return true;

# if defined(XP_WIN)
    if (!AddVectoredExceptionHandler(/* FirstHandler = */true, AsmJSExceptionHandler))
        return false;
# else  // assume Unix
    struct sigaction sigAction;
    sigAction.sa_sigaction = &AsmJSFaultHandler;
    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &sigAction, &sPrevSegvHandler))
        return false;
    if (sigaction(SIGBUS, &sigAction, &sPrevBusHandler))
        return false;
# endif

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
    JS_ASSERT(rt->currentThreadOwnsOperationCallbackLock());

    AsmJSActivation *activation = rt->mainThread.asmJSActivationStackFromAnyThread();
    if (!activation)
        return;

    const AsmJSModule &module = activation->module();

#if defined(XP_WIN)
    DWORD oldProtect;
    if (!VirtualProtect(module.functionCode(), module.functionBytes(), PAGE_NOACCESS, &oldProtect))
        MOZ_CRASH();
#else  // assume Unix
    if (mprotect(module.functionCode(), module.functionBytes(), PROT_NONE))
        MOZ_CRASH();
#endif
}

#ifdef MOZ_ASAN
// When running with asm.js under AddressSanitizer, we need to explicitely
// tell AddressSanitizer to allow custom signal handlers because it will 
// otherwise trigger ASan's SIGSEGV handler for the internal SIGSEGVs that 
// asm.js would otherwise handle.
extern "C" MOZ_ASAN_BLACKLIST
const char* __asan_default_options() {
    return "allow_user_segv_handler=1";
}
#endif
