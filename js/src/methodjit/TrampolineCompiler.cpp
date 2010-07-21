
#include "TrampolineCompiler.h"
#include "StubCalls.h"
#include "assembler/assembler/LinkBuffer.h"

namespace js {
namespace mjit {

#define CHECK_RESULT(x) if (!(x)) return false
#define COMPILE(which, pool, how) CHECK_RESULT(compileTrampoline((void **)(&(which)), &pool, how))
#define RELEASE(which, pool) JS_BEGIN_MACRO \
    which = NULL;                           \
    if (pool)                               \
        pool->release();                    \
    pool = NULL;                            \
JS_END_MACRO

#ifdef JS_CPU_X86
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type = JSC::X86Registers::ecx;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data = JSC::X86Registers::edx;
#elif defined(JS_CPU_ARM)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type = JSC::ARMRegisters::r2;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data = JSC::ARMRegisters::r1;
#endif

bool
TrampolineCompiler::compile()
{
#ifdef JS_METHODJIT_SPEW
    JMCheckLogging();
#endif

    COMPILE(trampolines->forceReturn, trampolines->forceReturnPool, generateForceReturn);

    return true;
}

void
TrampolineCompiler::release(Trampolines *tramps)
{
    RELEASE(tramps->forceReturn, tramps->forceReturnPool);
}

bool
TrampolineCompiler::compileTrampoline(void **where, JSC::ExecutablePool **pool,
                                      TrampolineGenerator generator)
{
    Assembler masm;

    Label entry = masm.label();
    CHECK_RESULT(generator(masm));
    JS_ASSERT(entry.isValid());

    *pool = execPool->poolForSize(masm.size());
    if (!*pool)
        return false;

    JSC::LinkBuffer buffer(&masm, *pool);
    uint8 *result = (uint8*)buffer.finalizeCodeAddendum().dataLocation();
    masm.finalize(result);
    *where = result + masm.distanceOf(entry);

    return true;
}

/*
 * This is shamelessly copied from emitReturn, but with several changes:
 * - There was always at least one inline call.
 * - We don't know if there is a call object, so we always check.
 * - We don't know where we came from, so we don't know frame depth or PC.
 * - There is no stub buffer.
 */
bool
TrampolineCompiler::generateForceReturn(Assembler &masm)
{
    /* if (!callobj) stubs::PutCallObject */
    Jump noCallObj = masm.branchPtr(Assembler::Equal,
                                    Address(JSFrameReg, offsetof(JSStackFrame, callobj)),
                                    ImmPtr(0));
    masm.stubCall(stubs::PutCallObject, NULL, 0);
    noCallObj.linkTo(masm.label(), &masm);

    /* if (arguments) stubs::PutArgsObject */
    Jump noArgsObj = masm.branchPtr(Assembler::Equal,
                                    Address(JSFrameReg, offsetof(JSStackFrame, argsobj)),
                                    ImmIntPtr(0));
    masm.stubCall(stubs::PutArgsObject, NULL, 0);
    noArgsObj.linkTo(masm.label(), &masm);

    /*
     * r = fp->down
     * a1 = f.cx
     * f.fp = r
     * cx->fp = r
     */
    masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, down)), Registers::ReturnReg);
    masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), Registers::ArgReg1);
    masm.storePtr(Registers::ReturnReg, FrameAddress(offsetof(VMFrame, fp)));
    masm.storePtr(Registers::ReturnReg, Address(Registers::ArgReg1, offsetof(JSContext, fp)));
    masm.subPtr(ImmIntPtr(1), FrameAddress(offsetof(VMFrame, inlineCallCount)));

    Address rval(JSFrameReg, offsetof(JSStackFrame, rval));
    masm.load32(masm.payloadOf(rval), JSReturnReg_Data);
    masm.load32(masm.tagOf(rval), JSReturnReg_Type);
    masm.move(Registers::ReturnReg, JSFrameReg);
    masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, ncode)), Registers::ReturnReg);
#ifdef DEBUG
    masm.storePtr(ImmPtr(JSStackFrame::sInvalidPC),
                  Address(JSFrameReg, offsetof(JSStackFrame, savedPC)));
#endif

#if defined(JS_CPU_ARM)
    masm.loadPtr(FrameAddress(offsetof(VMFrame, scriptedReturn)), JSC::ARMRegisters::lr);
#endif

    masm.ret();
    return true;
}

}
}
