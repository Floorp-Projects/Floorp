/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmSignalHandlers.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ThreadLocal.h"

#include "vm/Runtime.h"
#include "wasm/WasmInstance.h"

using namespace js;
using namespace js::wasm;

using mozilla::DebugOnly;

// =============================================================================
// This following pile of macros and includes defines the ToRegisterState() and
// the ContextTo{PC,FP,SP,LR}() functions from the (highly) platform-specific
// CONTEXT struct which is provided to the signal handler.
// =============================================================================

#if defined(XP_WIN)
# include "util/Windows.h"
#else
# include <signal.h>
# include <sys/mman.h>
#endif

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
# include <sys/ucontext.h> // for ucontext_t, mcontext_t
#endif

#if defined(__x86_64__)
# if defined(__DragonFly__)
#  include <machine/npx.h> // for union savefpu
# elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || \
       defined(__NetBSD__) || defined(__OpenBSD__)
#  include <machine/fpu.h> // for struct savefpu/fxsave64
# endif
#endif

#if defined(XP_WIN)
# define EIP_sig(p) ((p)->Eip)
# define EBP_sig(p) ((p)->Ebp)
# define ESP_sig(p) ((p)->Esp)
# define RIP_sig(p) ((p)->Rip)
# define RSP_sig(p) ((p)->Rsp)
# define RBP_sig(p) ((p)->Rbp)
# define R11_sig(p) ((p)->R11)
# define R13_sig(p) ((p)->R13)
# define R14_sig(p) ((p)->R14)
# define R15_sig(p) ((p)->R15)
#elif defined(__OpenBSD__)
# define EIP_sig(p) ((p)->sc_eip)
# define EBP_sig(p) ((p)->sc_ebp)
# define ESP_sig(p) ((p)->sc_esp)
# define RIP_sig(p) ((p)->sc_rip)
# define RSP_sig(p) ((p)->sc_rsp)
# define RBP_sig(p) ((p)->sc_rbp)
# define R11_sig(p) ((p)->sc_r11)
# if defined(__arm__)
#  define R13_sig(p) ((p)->sc_usr_sp)
#  define R14_sig(p) ((p)->sc_usr_lr)
#  define R15_sig(p) ((p)->sc_pc)
# else
#  define R13_sig(p) ((p)->sc_r13)
#  define R14_sig(p) ((p)->sc_r14)
#  define R15_sig(p) ((p)->sc_r15)
# endif
# if defined(__aarch64__)
#  define EPC_sig(p) ((p)->sc_elr)
#  define RFP_sig(p) ((p)->sc_x[29])
#  define RLR_sig(p) ((p)->sc_lr)
#  define R31_sig(p) ((p)->sc_sp)
# endif
# if defined(__mips__)
#  define EPC_sig(p) ((p)->sc_pc)
#  define RFP_sig(p) ((p)->sc_regs[30])
# endif
#elif defined(__linux__) || defined(__sun)
# if defined(__linux__)
#  define EIP_sig(p) ((p)->uc_mcontext.gregs[REG_EIP])
#  define EBP_sig(p) ((p)->uc_mcontext.gregs[REG_EBP])
#  define ESP_sig(p) ((p)->uc_mcontext.gregs[REG_ESP])
# else
#  define EIP_sig(p) ((p)->uc_mcontext.gregs[REG_PC])
#  define EBP_sig(p) ((p)->uc_mcontext.gregs[REG_EBP])
#  define ESP_sig(p) ((p)->uc_mcontext.gregs[REG_ESP])
# endif
# define RIP_sig(p) ((p)->uc_mcontext.gregs[REG_RIP])
# define RSP_sig(p) ((p)->uc_mcontext.gregs[REG_RSP])
# define RBP_sig(p) ((p)->uc_mcontext.gregs[REG_RBP])
# if defined(__linux__) && defined(__arm__)
#  define R11_sig(p) ((p)->uc_mcontext.arm_fp)
#  define R13_sig(p) ((p)->uc_mcontext.arm_sp)
#  define R14_sig(p) ((p)->uc_mcontext.arm_lr)
#  define R15_sig(p) ((p)->uc_mcontext.arm_pc)
# else
#  define R11_sig(p) ((p)->uc_mcontext.gregs[REG_R11])
#  define R13_sig(p) ((p)->uc_mcontext.gregs[REG_R13])
#  define R14_sig(p) ((p)->uc_mcontext.gregs[REG_R14])
#  define R15_sig(p) ((p)->uc_mcontext.gregs[REG_R15])
# endif
# if defined(__linux__) && defined(__aarch64__)
#  define EPC_sig(p) ((p)->uc_mcontext.pc)
#  define RFP_sig(p) ((p)->uc_mcontext.regs[29])
#  define RLR_sig(p) ((p)->uc_mcontext.regs[30])
#  define R31_sig(p) ((p)->uc_mcontext.regs[31])
# endif
# if defined(__linux__) && defined(__mips__)
#  define EPC_sig(p) ((p)->uc_mcontext.pc)
#  define RFP_sig(p) ((p)->uc_mcontext.gregs[30])
#  define RSP_sig(p) ((p)->uc_mcontext.gregs[29])
#  define R31_sig(p) ((p)->uc_mcontext.gregs[31])
# endif
# if defined(__linux__) && (defined(__sparc__) && defined(__arch64__))
#  define PC_sig(p) ((p)->uc_mcontext.mc_gregs[MC_PC])
#  define FP_sig(p) ((p)->uc_mcontext.mc_fp)
#  define SP_sig(p) ((p)->uc_mcontext.mc_i7)
# endif
# if defined(__linux__) && \
     (defined(__ppc64__) ||  defined (__PPC64__) || defined(__ppc64le__) || defined (__PPC64LE__))
#  define R01_sig(p) ((p)->uc_mcontext.gp_regs[1])
#  define R32_sig(p) ((p)->uc_mcontext.gp_regs[32])
# endif
#elif defined(__NetBSD__)
# define EIP_sig(p) ((p)->uc_mcontext.__gregs[_REG_EIP])
# define EBP_sig(p) ((p)->uc_mcontext.__gregs[_REG_EBP])
# define ESP_sig(p) ((p)->uc_mcontext.__gregs[_REG_ESP])
# define RIP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RIP])
# define RSP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RSP])
# define RBP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RBP])
# define R11_sig(p) ((p)->uc_mcontext.__gregs[_REG_R11])
# define R13_sig(p) ((p)->uc_mcontext.__gregs[_REG_R13])
# define R14_sig(p) ((p)->uc_mcontext.__gregs[_REG_R14])
# define R15_sig(p) ((p)->uc_mcontext.__gregs[_REG_R15])
# if defined(__aarch64__)
#  define EPC_sig(p) ((p)->uc_mcontext.__gregs[_REG_PC])
#  define RFP_sig(p) ((p)->uc_mcontext.__gregs[_REG_X29])
#  define RLR_sig(p) ((p)->uc_mcontext.__gregs[_REG_X30])
#  define R31_sig(p) ((p)->uc_mcontext.__gregs[_REG_SP])
# endif
# if defined(__mips__)
#  define EPC_sig(p) ((p)->uc_mcontext.__gregs[_REG_EPC])
#  define RFP_sig(p) ((p)->uc_mcontext.__gregs[_REG_S8])
# endif
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
# define EIP_sig(p) ((p)->uc_mcontext.mc_eip)
# define EBP_sig(p) ((p)->uc_mcontext.mc_ebp)
# define ESP_sig(p) ((p)->uc_mcontext.mc_esp)
# define RIP_sig(p) ((p)->uc_mcontext.mc_rip)
# define RSP_sig(p) ((p)->uc_mcontext.mc_rsp)
# define RBP_sig(p) ((p)->uc_mcontext.mc_rbp)
# if defined(__FreeBSD__) && defined(__arm__)
#  define R11_sig(p) ((p)->uc_mcontext.__gregs[_REG_R11])
#  define R13_sig(p) ((p)->uc_mcontext.__gregs[_REG_R13])
#  define R14_sig(p) ((p)->uc_mcontext.__gregs[_REG_R14])
#  define R15_sig(p) ((p)->uc_mcontext.__gregs[_REG_R15])
# else
#  define R11_sig(p) ((p)->uc_mcontext.mc_r11)
#  define R13_sig(p) ((p)->uc_mcontext.mc_r13)
#  define R14_sig(p) ((p)->uc_mcontext.mc_r14)
#  define R15_sig(p) ((p)->uc_mcontext.mc_r15)
# endif
# if defined(__FreeBSD__) && defined(__aarch64__)
#  define EPC_sig(p) ((p)->uc_mcontext.mc_gpregs.gp_elr)
#  define RFP_sig(p) ((p)->uc_mcontext.mc_gpregs.gp_x[29])
#  define RLR_sig(p) ((p)->uc_mcontext.mc_gpregs.gp_lr)
#  define R31_sig(p) ((p)->uc_mcontext.mc_gpregs.gp_sp)
# endif
# if defined(__FreeBSD__) && defined(__mips__)
#  define EPC_sig(p) ((p)->uc_mcontext.mc_pc)
#  define RFP_sig(p) ((p)->uc_mcontext.mc_regs[30])
# endif
#elif defined(XP_DARWIN)
# define EIP_sig(p) ((p)->thread.uts.ts32.__eip)
# define EBP_sig(p) ((p)->thread.uts.ts32.__ebp)
# define ESP_sig(p) ((p)->thread.uts.ts32.__esp)
# define RIP_sig(p) ((p)->thread.__rip)
# define RBP_sig(p) ((p)->thread.__rbp)
# define RSP_sig(p) ((p)->thread.__rsp)
# define R11_sig(p) ((p)->thread.__r[11])
# define R13_sig(p) ((p)->thread.__sp)
# define R14_sig(p) ((p)->thread.__lr)
# define R15_sig(p) ((p)->thread.__pc)
#else
# error "Don't know how to read/write to the thread state via the mcontext_t."
#endif

#if defined(ANDROID)
// Not all versions of the Android NDK define ucontext_t or mcontext_t.
// Detect this and provide custom but compatible definitions. Note that these
// follow the GLibc naming convention to access register values from
// mcontext_t.
//
// See: https://chromiumcodereview.appspot.com/10829122/
// See: http://code.google.com/p/android/issues/detail?id=34784
# if !defined(__BIONIC_HAVE_UCONTEXT_T)
#  if defined(__arm__)

// GLibc on ARM defines mcontext_t has a typedef for 'struct sigcontext'.
// Old versions of the C library <signal.h> didn't define the type.
#   if !defined(__BIONIC_HAVE_STRUCT_SIGCONTEXT)
#    include <asm/sigcontext.h>
#   endif

typedef struct sigcontext mcontext_t;

typedef struct ucontext {
    uint32_t uc_flags;
    struct ucontext* uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    // Other fields are not used so don't define them here.
} ucontext_t;

#  elif defined(__mips__)

typedef struct {
    uint32_t regmask;
    uint32_t status;
    uint64_t pc;
    uint64_t gregs[32];
    uint64_t fpregs[32];
    uint32_t acx;
    uint32_t fpc_csr;
    uint32_t fpc_eir;
    uint32_t used_math;
    uint32_t dsp;
    uint64_t mdhi;
    uint64_t mdlo;
    uint32_t hi1;
    uint32_t lo1;
    uint32_t hi2;
    uint32_t lo2;
    uint32_t hi3;
    uint32_t lo3;
} mcontext_t;

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
#  endif  // defined(__i386__)
# endif  // !defined(__BIONIC_HAVE_UCONTEXT_T)
#endif // defined(ANDROID)

#if defined(XP_DARWIN)
# if defined(__x86_64__)
struct macos_x64_context {
    x86_thread_state64_t thread;
    x86_float_state64_t float_;
};
#  define CONTEXT macos_x64_context
# elif defined(__i386__)
struct macos_x86_context {
    x86_thread_state_t thread;
    x86_float_state_t float_;
};
#  define CONTEXT macos_x86_context
# elif defined(__arm__)
struct macos_arm_context {
    arm_thread_state_t thread;
    arm_neon_state_t float_;
};
#  define CONTEXT macos_arm_context
# else
#  error Unsupported architecture
# endif
#elif !defined(XP_WIN)
# define CONTEXT ucontext_t
#endif

#if defined(_M_X64) || defined(__x86_64__)
# define PC_sig(p) RIP_sig(p)
# define FP_sig(p) RBP_sig(p)
# define SP_sig(p) RSP_sig(p)
#elif defined(_M_IX86) || defined(__i386__)
# define PC_sig(p) EIP_sig(p)
# define FP_sig(p) EBP_sig(p)
# define SP_sig(p) ESP_sig(p)
#elif defined(__arm__)
# define FP_sig(p) R11_sig(p)
# define SP_sig(p) R13_sig(p)
# define LR_sig(p) R14_sig(p)
# define PC_sig(p) R15_sig(p)
#elif defined(__aarch64__)
# define PC_sig(p) EPC_sig(p)
# define FP_sig(p) RFP_sig(p)
# define SP_sig(p) R31_sig(p)
# define LR_sig(p) RLR_sig(p)
#elif defined(__mips__)
# define PC_sig(p) EPC_sig(p)
# define FP_sig(p) RFP_sig(p)
# define SP_sig(p) RSP_sig(p)
# define LR_sig(p) R31_sig(p)
#elif defined(__ppc64__) ||  defined (__PPC64__) || defined(__ppc64le__) || defined (__PPC64LE__)
# define PC_sig(p) R32_sig(p)
# define SP_sig(p) R01_sig(p)
# define FP_sig(p) R01_sig(p)
#endif

static uint8_t**
ContextToPC(CONTEXT* context)
{
#ifdef PC_sig
    return reinterpret_cast<uint8_t**>(&PC_sig(context));
#else
    MOZ_CRASH();
#endif
}

static uint8_t*
ContextToFP(CONTEXT* context)
{
#ifdef FP_sig
    return reinterpret_cast<uint8_t*>(FP_sig(context));
#else
    MOZ_CRASH();
#endif
}

static uint8_t*
ContextToSP(CONTEXT* context)
{
#ifdef SP_sig
    return reinterpret_cast<uint8_t*>(SP_sig(context));
#else
    MOZ_CRASH();
#endif
}

#if defined(__arm__) || defined(__aarch64__) || defined(__mips__)
static uint8_t*
ContextToLR(CONTEXT* context)
{
# ifdef LR_sig
    return reinterpret_cast<uint8_t*>(LR_sig(context));
# else
    MOZ_CRASH();
# endif
}
#endif

static JS::ProfilingFrameIterator::RegisterState
ToRegisterState(CONTEXT* context)
{
    JS::ProfilingFrameIterator::RegisterState state;
    state.fp = ContextToFP(context);
    state.pc = *ContextToPC(context);
    state.sp = ContextToSP(context);
#if defined(__arm__) || defined(__aarch64__) || defined(__mips__)
    state.lr = ContextToLR(context);
#else
    state.lr = (void*)UINTPTR_MAX;
#endif
    return state;
}

// =============================================================================
// All signals/exceptions funnel down to this one trap-handling function which
// tests whether the pc is in a wasm module and, if so, whether there is
// actually a trap expected at this pc. These tests both avoid real bugs being
// silently converted to wasm traps and provides the trapping wasm bytecode
// offset we need to report in the error.
//
// Crashing inside wasm trap handling (due to a bug in trap handling or exposed
// during trap handling) must be reported like a normal crash, not cause the
// crash report to be lost. On Windows and non-Mach Unix, a crash during the
// handler reenters the handler, possibly repeatedly until exhausting the stack,
// and so we prevent recursion with the thread-local sAlreadyHandlingTrap. On
// Mach, the wasm exception handler has its own thread and is installed only on
// the thread-level debugging ports of JSRuntime threads, so a crash on
// exception handler thread will not recurse; it will bubble up to the
// process-level debugging ports (where Breakpad is installed).
// =============================================================================

static MOZ_THREAD_LOCAL(bool) sAlreadyHandlingTrap;

struct AutoHandlingTrap
{
    AutoHandlingTrap() {
        MOZ_ASSERT(!sAlreadyHandlingTrap.get());
        sAlreadyHandlingTrap.set(true);
    }

    ~AutoHandlingTrap() {
        MOZ_ASSERT(sAlreadyHandlingTrap.get());
        sAlreadyHandlingTrap.set(false);
    }
};

static MOZ_MUST_USE bool
HandleTrap(CONTEXT* context, JSContext* cx)
{
    MOZ_ASSERT(sAlreadyHandlingTrap.get());

    uint8_t* pc = *ContextToPC(context);
    const CodeSegment* codeSegment = LookupCodeSegment(pc);
    if (!codeSegment || !codeSegment->isModule()) {
        return false;
    }

    const ModuleSegment& segment = *codeSegment->asModule();

    Trap trap;
    BytecodeOffset bytecode;
    if (!segment.code().lookupTrap(pc, &trap, &bytecode)) {
        return false;
    }

    // We have a safe, expected wasm trap. Call startWasmTrap() to store enough
    // register state at the point of the trap to allow stack unwinding or
    // resumption, both of which will call finishWasmTrap().
    jit::JitActivation* activation = cx->activation()->asJit();
    activation->startWasmTrap(trap, bytecode.offset(), ToRegisterState(context));
    *ContextToPC(context) = segment.trapCode();
    return true;
}

// =============================================================================
// The following platform specific signal/exception handlers are installed by
// wasm::EnsureSignalHandlers() and funnel all potential wasm traps into
// HandleTrap() above.
// =============================================================================

#if defined(XP_WIN)

static LONG WINAPI
WasmTrapHandler(LPEXCEPTION_POINTERS exception)
{
    if (sAlreadyHandlingTrap.get()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    AutoHandlingTrap aht;

    EXCEPTION_RECORD* record = exception->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
        record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (!HandleTrap(exception->ContextRecord, TlsContext.get())) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    return EXCEPTION_CONTINUE_EXECUTION;
}

#elif defined(XP_DARWIN)
# include <mach/exc.h>

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
HandleMachException(JSContext* cx, const ExceptionRequest& request)
{
    // Get the port of the JSContext's thread from the message.
    mach_port_t cxThread = request.body.thread.name;

    // Read out the JSRuntime thread's register state.
    CONTEXT context;
# if defined(__x86_64__)
    unsigned int thread_state_count = x86_THREAD_STATE64_COUNT;
    unsigned int float_state_count = x86_FLOAT_STATE64_COUNT;
    int thread_state = x86_THREAD_STATE64;
    int float_state = x86_FLOAT_STATE64;
# elif defined(__i386__)
    unsigned int thread_state_count = x86_THREAD_STATE_COUNT;
    unsigned int float_state_count = x86_FLOAT_STATE_COUNT;
    int thread_state = x86_THREAD_STATE;
    int float_state = x86_FLOAT_STATE;
# elif defined(__arm__)
    unsigned int thread_state_count = ARM_THREAD_STATE_COUNT;
    unsigned int float_state_count = ARM_NEON_STATE_COUNT;
    int thread_state = ARM_THREAD_STATE;
    int float_state = ARM_NEON_STATE;
# else
#  error Unsupported architecture
# endif
    kern_return_t kret;
    kret = thread_get_state(cxThread, thread_state,
                            (thread_state_t)&context.thread, &thread_state_count);
    if (kret != KERN_SUCCESS) {
        return false;
    }
    kret = thread_get_state(cxThread, float_state,
                            (thread_state_t)&context.float_, &float_state_count);
    if (kret != KERN_SUCCESS) {
        return false;
    }

    if (request.body.exception != EXC_BAD_ACCESS &&
        request.body.exception != EXC_BAD_INSTRUCTION)
    {
        return false;
    }

    {
        AutoNoteSingleThreadedRegion anstr;
        AutoHandlingTrap aht;
        if (!HandleTrap(&context, cx)) {
            return false;
        }
    }

    // Update the thread state with the new pc and register values.
    kret = thread_set_state(cxThread, float_state, (thread_state_t)&context.float_, float_state_count);
    if (kret != KERN_SUCCESS) {
        return false;
    }
    kret = thread_set_state(cxThread, thread_state, (thread_state_t)&context.thread, thread_state_count);
    if (kret != KERN_SUCCESS) {
        return false;
    }

    return true;
}

// Taken from mach_exc in /usr/include/mach/mach_exc.defs.
static const mach_msg_id_t sExceptionId = 2405;

// The choice of id here is arbitrary, the only constraint is that sQuitId != sExceptionId.
static const mach_msg_id_t sQuitId = 42;

static void
MachExceptionHandlerThread(JSContext* cx)
{
    mach_port_t port = cx->wasmMachExceptionHandler.port();
    kern_return_t kret;

    while(true) {
        ExceptionRequest request;
        kret = mach_msg(&request.body.Head, MACH_RCV_MSG, 0, sizeof(request),
                        port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

        // If we fail even receiving the message, we can't even send a reply!
        // Rather than hanging the faulting thread (hanging the browser), crash.
        if (kret != KERN_SUCCESS) {
            fprintf(stderr, "MachExceptionHandlerThread: mach_msg failed with %d\n", (int)kret);
            MOZ_CRASH();
        }

        // There are only two messages we should be receiving: an exception
        // message that occurs when the runtime's thread faults and the quit
        // message sent when the runtime is shutting down.
        if (request.body.Head.msgh_id == sQuitId) {
            break;
        }
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
        bool handled = HandleMachException(cx, request);
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
}

MachExceptionHandler::MachExceptionHandler()
  : installed_(false),
    thread_(),
    port_(MACH_PORT_NULL)
{}

void
MachExceptionHandler::uninstall()
{
    if (installed_) {
        thread_port_t thread = mach_thread_self();
        kern_return_t kret = thread_set_exception_ports(thread,
                                                        EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION,
                                                        MACH_PORT_NULL,
                                                        EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                                        THREAD_STATE_NONE);
        mach_port_deallocate(mach_task_self(), thread);
        if (kret != KERN_SUCCESS) {
            MOZ_CRASH();
        }
        installed_ = false;
    }
    if (thread_.joinable()) {
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
            fprintf(stderr, "MachExceptionHandler: failed to send quit message: %d\n", (int)kret);
            MOZ_CRASH();
        }

        // Wait for the handler thread to complete before deallocating the port.
        thread_.join();
    }
    if (port_ != MACH_PORT_NULL) {
        DebugOnly<kern_return_t> kret = mach_port_destroy(mach_task_self(), port_);
        MOZ_ASSERT(kret == KERN_SUCCESS);
        port_ = MACH_PORT_NULL;
    }
}

bool
MachExceptionHandler::install(JSContext* cx)
{
    MOZ_ASSERT(!installed());
    kern_return_t kret;
    mach_port_t thread;

    auto onFailure = mozilla::MakeScopeExit([&] {
        uninstall();
    });

    // Get a port which can send and receive data.
    kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port_);
    if (kret != KERN_SUCCESS) {
        return false;
    }
    kret = mach_port_insert_right(mach_task_self(), port_, port_, MACH_MSG_TYPE_MAKE_SEND);
    if (kret != KERN_SUCCESS) {
        return false;
    }

    // Create a thread to block on reading port_.
    if (!thread_.init(MachExceptionHandlerThread, cx)) {
        return false;
    }

    // Direct exceptions on this thread to port_ (and thus our handler thread).
    // Note: we are totally clobbering any existing *thread* exception ports and
    // not even attempting to forward. Breakpad and gdb both use the *process*
    // exception ports which are only called if the thread doesn't handle the
    // exception, so we should be fine.
    thread = mach_thread_self();
    kret = thread_set_exception_ports(thread,
                                      EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION,
                                      port_,
                                      EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                      THREAD_STATE_NONE);
    mach_port_deallocate(mach_task_self(), thread);
    if (kret != KERN_SUCCESS) {
        return false;
    }

    installed_ = true;
    onFailure.release();
    return true;
}

#else  // If not Windows or Mac, assume Unix

#ifdef __mips__
static const uint32_t kWasmTrapSignal = SIGFPE;
#else
static const uint32_t kWasmTrapSignal = SIGILL;
#endif

static struct sigaction sPrevSEGVHandler;
static struct sigaction sPrevSIGBUSHandler;
static struct sigaction sPrevWasmTrapHandler;

static void
WasmTrapHandler(int signum, siginfo_t* info, void* context)
{
    if (!sAlreadyHandlingTrap.get()) {
        AutoHandlingTrap aht;
        MOZ_RELEASE_ASSERT(signum == SIGSEGV || signum == SIGBUS || signum == kWasmTrapSignal);
        if (HandleTrap((CONTEXT*)context, TlsContext.get())) {
            return;
        }
    }

    struct sigaction* previousSignal = nullptr;
    switch (signum) {
      case SIGSEGV: previousSignal = &sPrevSEGVHandler; break;
      case SIGBUS: previousSignal = &sPrevSIGBUSHandler; break;
      case kWasmTrapSignal: previousSignal = &sPrevWasmTrapHandler; break;
    }
    MOZ_ASSERT(previousSignal);

    // This signal is not for any asm.js code we expect, so we need to forward
    // the signal to the next handler. If there is no next handler (SIG_IGN or
    // SIG_DFL), then it's time to crash. To do this, we set the signal back to
    // its original disposition and return. This will cause the faulting op to
    // be re-executed which will crash in the normal way. The advantage of
    // doing this to calling _exit() is that we remove ourselves from the crash
    // stack which improves crash reports. If there is a next handler, call it.
    // It will either crash synchronously, fix up the instruction so that
    // execution can continue and return, or trigger a crash by returning the
    // signal to it's original disposition and returning.
    //
    // Note: the order of these tests matter.
    if (previousSignal->sa_flags & SA_SIGINFO) {
        previousSignal->sa_sigaction(signum, info, context);
    } else if (previousSignal->sa_handler == SIG_DFL || previousSignal->sa_handler == SIG_IGN) {
        sigaction(signum, previousSignal, nullptr);
    } else {
        previousSignal->sa_handler(signum);
    }
}
# endif // XP_WIN || XP_DARWIN || assume unix

#if defined(ANDROID) && defined(MOZ_LINKER)
extern "C" MFBT_API bool IsSignalHandlingBroken();
#endif

static bool sTriedInstallSignalHandlers = false;
static bool sHaveSignalHandlers = false;

static bool
ProcessHasSignalHandlers()
{
    // We assume that there are no races creating the first JSRuntime of the process.
    if (sTriedInstallSignalHandlers) {
        return sHaveSignalHandlers;
    }
    sTriedInstallSignalHandlers = true;

#if defined (JS_CODEGEN_NONE)
    // If there is no JIT, then there should be no Wasm signal handlers.
    return false;
#endif

    // Signal handlers are currently disabled when recording or replaying.
    if (mozilla::recordreplay::IsRecordingOrReplaying()) {
        return false;
    }

#if defined(ANDROID) && defined(MOZ_LINKER)
    // Signal handling is broken on some android systems.
    if (IsSignalHandlingBroken()) {
        return false;
    }
#endif

    // Initalize ThreadLocal flag used by WasmTrapHandler
    sAlreadyHandlingTrap.infallibleInit();

    // Install a SIGSEGV handler to handle safely-out-of-bounds asm.js heap
    // access and/or unaligned accesses.
#if defined(XP_WIN)
# if defined(MOZ_ASAN)
    // Under ASan we need to let the ASan runtime's ShadowExceptionHandler stay
    // in the first handler position. This requires some coordination with
    // MemoryProtectionExceptionHandler::isDisabled().
    const bool firstHandler = false;
# else
    // Otherwise, WasmTrapHandler needs to go first, so that we can recover
    // from wasm faults and continue execution without triggering handlers
    // such as MemoryProtectionExceptionHandler that assume we are crashing.
    const bool firstHandler = true;
# endif
    if (!AddVectoredExceptionHandler(firstHandler, WasmTrapHandler)) {
        return false;
    }
#elif defined(XP_DARWIN)
    // OSX handles seg faults via the Mach exception handler above, so don't
    // install WasmTrapHandler.
#else
    // SA_NODEFER allows us to reenter the signal handler if we crash while
    // handling the signal, and fall through to the Breakpad handler by testing
    // handlingSegFault.

    // Allow handling OOB with signals on all architectures
    struct sigaction faultHandler;
    faultHandler.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
    faultHandler.sa_sigaction = WasmTrapHandler;
    sigemptyset(&faultHandler.sa_mask);
    if (sigaction(SIGSEGV, &faultHandler, &sPrevSEGVHandler)) {
        MOZ_CRASH("unable to install segv handler");
    }

# if defined(JS_CODEGEN_ARM)
    // On Arm Handle Unaligned Accesses
    struct sigaction busHandler;
    busHandler.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
    busHandler.sa_sigaction = WasmTrapHandler;
    sigemptyset(&busHandler.sa_mask);
    if (sigaction(SIGBUS, &busHandler, &sPrevSIGBUSHandler)) {
        MOZ_CRASH("unable to install sigbus handler");
    }
# endif

    // Install a handler to handle the instructions that are emitted to implement
    // wasm traps.
    struct sigaction wasmTrapHandler;
    wasmTrapHandler.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
    wasmTrapHandler.sa_sigaction = WasmTrapHandler;
    sigemptyset(&wasmTrapHandler.sa_mask);
    if (sigaction(kWasmTrapSignal, &wasmTrapHandler, &sPrevWasmTrapHandler)) {
        MOZ_CRASH("unable to install wasm trap handler");
    }
#endif

    sHaveSignalHandlers = true;
    return true;
}

bool
wasm::EnsureSignalHandlers(JSContext* cx)
{
    // Nothing to do if the platform doesn't support it.
    if (!ProcessHasSignalHandlers()) {
        return true;
    }

#if defined(XP_DARWIN)
    // On OSX, each JSContext which runs wasm gets its own handler thread.
    if (!cx->wasmMachExceptionHandler.installed() && !cx->wasmMachExceptionHandler.install(cx)) {
        return false;
    }
#endif

    return true;
}

bool
wasm::HaveSignalHandlers()
{
    MOZ_ASSERT(sTriedInstallSignalHandlers);
    return sHaveSignalHandlers;
}

bool
wasm::MemoryAccessTraps(const RegisterState& regs, uint8_t* addr, uint32_t numBytes, uint8_t** newPC)
{
    const wasm::CodeSegment* codeSegment = wasm::LookupCodeSegment(regs.pc);
    if (!codeSegment || !codeSegment->isModule()) {
        return false;
    }

    const wasm::ModuleSegment& segment = *codeSegment->asModule();

    Trap trap;
    BytecodeOffset bytecode;
    if (!segment.code().lookupTrap(regs.pc, &trap, &bytecode) || trap != Trap::OutOfBounds) {
        return false;
    }

    Instance& instance = *reinterpret_cast<Frame*>(regs.fp)->tls->instance;
    MOZ_ASSERT(&instance.code() == &segment.code());

    if (!instance.memoryAccessInGuardRegion((uint8_t*)addr, numBytes)) {
        return false;
    }

    jit::JitActivation* activation = TlsContext.get()->activation()->asJit();
    activation->startWasmTrap(Trap::OutOfBounds, bytecode.offset(), regs);
    *newPC = segment.trapCode();
    return true;
}

bool
wasm::HandleIllegalInstruction(const RegisterState& regs, uint8_t** newPC)
{
    const wasm::CodeSegment* codeSegment = wasm::LookupCodeSegment(regs.pc);
    if (!codeSegment || !codeSegment->isModule()) {
        return false;
    }

    const wasm::ModuleSegment& segment = *codeSegment->asModule();

    Trap trap;
    BytecodeOffset bytecode;
    if (!segment.code().lookupTrap(regs.pc, &trap, &bytecode)) {
        return false;
    }

    jit::JitActivation* activation = TlsContext.get()->activation()->asJit();
    activation->startWasmTrap(trap, bytecode.offset(), regs);
    *newPC = segment.trapCode();
    return true;
}
