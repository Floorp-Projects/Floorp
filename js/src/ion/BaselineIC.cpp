/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineJIT.h"
#include "BaselineCompiler.h"
#include "BaselineHelpers.h"
#include "BaselineIC.h"
#include "IonLinker.h"
#include "IonSpewer.h"
#include "VMFunctions.h"
#include "IonFrames-inl.h"

#include "jsinterpinlines.h"

namespace js {
namespace ion {

ICMonitoredStub::ICMonitoredStub(Kind kind, IonCode *stubCode, ICStub *firstMonitorStub)
  : ICStub(kind, ICStub::Monitored, stubCode),
    firstMonitorStub_(firstMonitorStub)
{
    // If the first monitored stub is a ICTypeMonitor_Fallback stub, then
    // double check that _its_ firstMonitorStub is the same as this one.
    JS_ASSERT_IF(firstMonitorStub_->isTypeMonitor_Fallback(),
                 firstMonitorStub_->toTypeMonitor_Fallback()->firstMonitorStub() ==
                    firstMonitorStub_);
}

bool
ICMonitoredFallbackStub::initMonitoringChain(JSContext *cx)
{
    JS_ASSERT(fallbackMonitorStub_ == NULL);

    ICTypeMonitor_Fallback::Compiler compiler(cx, this);
    ICTypeMonitor_Fallback *stub = compiler.getStub();
    if (!stub)
        return false;
    fallbackMonitorStub_ = stub;
    return true;
}

bool
ICUpdatedStub::initUpdatingChain(JSContext *cx)
{
    JS_ASSERT(firstUpdateStub_ == NULL);

    ICTypeUpdate_Fallback::Compiler compiler(cx);
    ICTypeUpdate_Fallback *stub = compiler.getStub();
    if (!stub)
        return false;

    firstUpdateStub_ = stub;
    return true;
}

IonCode *
ICStubCompiler::getStubCode()
{
    IonCompartment *ion = cx->compartment->ionCompartment();

    // Check for existing cached stubcode.
    uint32_t stubKey = getKey();
    IonCode *stubCode = ion->getStubCode(stubKey);
    if (stubCode)
        return stubCode;

    // Compile new stubcode.
    MacroAssembler masm;
#ifdef JS_CPU_ARM
    masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif

    AutoFlushCache afc("ICStubCompiler::getStubCode", ion);
    if (!generateStubCode(masm))
        return NULL;
    Linker linker(masm);
    Rooted<IonCode *> newStubCode(cx, linker.newCode(cx));
    if (!newStubCode)
        return NULL;

    // Cache newly compiled stubcode.
    if (!ion->putStubCode(stubKey, newStubCode))
        return NULL;

    return newStubCode;
}

bool
ICStubCompiler::tailCallVM(const VMFunction &fun, MacroAssembler &masm)
{
    IonCompartment *ion = cx->compartment->ionCompartment();
    IonCode *code = ion->getVMWrapper(fun);
    if (!code)
        return false;

    uint32_t argSize = fun.explicitStackSlots() * sizeof(void *);
    EmitTailCallVM(code, masm, argSize);
    return true;
}

bool
ICStubCompiler::callVM(const VMFunction &fun, MacroAssembler &masm)
{
    IonCompartment *ion = cx->compartment->ionCompartment();
    IonCode *code = ion->getVMWrapper(fun);
    if (!code)
        return false;

    EmitCallVM(code, masm);
    return true;
}

bool
ICStubCompiler::callTypeUpdateIC(MacroAssembler &masm)
{
    IonCompartment *ion = cx->compartment->ionCompartment();
    IonCode *code = ion->getVMWrapper(DoTypeUpdateFallbackInfo);
    if (!code)
        return false;

    EmitCallTypeUpdateIC(masm, code);
    return true;
}

//
// TypeMonitor_Fallback
//

bool
ICTypeMonitor_Fallback::addMonitorStubForValue(JSContext *cx, HandleValue val)
{
    bool wasEmptyMonitorChain = (numOptimizedMonitorStubs_ == 0);

    if (numOptimizedMonitorStubs_ >= MAX_OPTIMIZED_STUBS) {
        // TODO: if the TypeSet becomes unknown or has the AnyObject type,
        // replace stubs with a single stub to handle these.
        return true;
    }

    if (val.isPrimitive()) {
        JSValueType type = val.isDouble() ? JSVAL_TYPE_DOUBLE : val.extractNonDoubleType();
        ICTypeMonitor_Type::Compiler compiler(cx, type);
        ICStub *stub = compiler.getStub();
        if (!stub)
            return false;
        addOptimizedMonitorStub(stub);
    } else {
        RootedTypeObject type(cx, val.toObject().getType(cx));
        if (!type)
            return false;
        ICTypeMonitor_TypeObject::Compiler compiler(cx, type);
        ICStub *stub = compiler.getStub();
        if (!stub)
            return false;
        addOptimizedMonitorStub(stub);
    }

    bool firstMonitorStubAdded = wasEmptyMonitorChain && (numOptimizedMonitorStubs_ > 0);

    if (firstMonitorStubAdded) {
        // Was an empty monitor chain before, but a new stub was added.  This is the
        // only time that any main stubs' firstMonitorStub fields need to be updated to
        // refer to the newly added monitor stub.
        ICEntry *ent = mainFallbackStub_->icEntry();
        for (ICStub *mainStub = ent->firstStub();
             mainStub != mainFallbackStub_;
             mainStub = mainStub->next())
        {
            // Since we stop at the last stub, all stubs MUST have a valid next stub.
            JS_ASSERT(mainStub->next() != NULL);

            // Since we just added the first optimized monitoring stub, any
            // existing main stub's |firstMonitorStub| MUST be pointing to the fallback
            // monitor stub (i.e. this stub).
            JS_ASSERT(mainStub->toMonitoredStub()->firstMonitorStub() == this);
            mainStub->toMonitoredStub()->updateFirstMonitorStub(firstMonitorStub_);
        }
    }

    return true;
}

static bool
DoTypeMonitorFallback(JSContext *cx, ICTypeMonitor_Fallback *stub, HandleValue value,
                      MutableHandleValue res)
{
    RootedScript script(cx, GetTopIonJSScript(cx));

    // Monitor the pc.
    jsbytecode *pc = stub->mainFallbackStub()->icEntry()->pc(script);
    types::TypeScript::Monitor(cx, script, pc, value);

    if (!stub->addMonitorStubForValue(cx, value))
        return false;

    // Copy input value to res.
    res.set(value);
    return true;
}

typedef bool (*DoTypeMonitorFallbackFn)(JSContext *, ICTypeMonitor_Fallback *, HandleValue,
                                        MutableHandleValue);
static const VMFunction DoTypeMonitorFallbackInfo =
    FunctionInfo<DoTypeMonitorFallbackFn>(DoTypeMonitorFallback);

bool
ICTypeMonitor_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return tailCallVM(DoTypeMonitorFallbackInfo, masm);
}

bool
ICTypeMonitor_Type::Compiler::generateStubCode(MacroAssembler &masm)
{
    Label failure;
    switch (type_) {
      case JSVAL_TYPE_INT32:
        masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
        break;
      case JSVAL_TYPE_DOUBLE:
        // TI double type implies int32.
        masm.branchTestNumber(Assembler::NotEqual, R0, &failure);
        break;
      case JSVAL_TYPE_BOOLEAN:
        masm.branchTestBoolean(Assembler::NotEqual, R0, &failure);
        break;
      case JSVAL_TYPE_STRING:
        masm.branchTestString(Assembler::NotEqual, R0, &failure);
        break;
      case JSVAL_TYPE_NULL:
        masm.branchTestNull(Assembler::NotEqual, R0, &failure);
        break;
      case JSVAL_TYPE_UNDEFINED:
        masm.branchTestUndefined(Assembler::NotEqual, R0, &failure);
        break;
      default:
        JS_NOT_REACHED("Unexpected type");
    }

    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTypeMonitor_TypeObject::Compiler::generateStubCode(MacroAssembler &masm)
{
    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Guard on the object's TypeObject.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(obj, JSObject::offsetOfType()), R1.scratchReg());

    Address expectedType(BaselineStubReg, ICTypeMonitor_TypeObject::offsetOfType());
    masm.branchPtr(Assembler::NotEqual, expectedType, R1.scratchReg(), &failure);

    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// TypeUpdate_Fallback
//

bool
DoTypeUpdateFallback(JSContext *cx, ICUpdatedStub *stub, HandleValue value)
{
    /*
    switch(stub->kind()) {
      case ICStub::SetElem_Dense:
        RootedTypeObject type(cx, stub->type());
        ///// update typeset on |type|
        break;
      default:
        break;
    }
    ///// create and add new type-update stub for |value|
    */
    return true;
}

typedef bool (*DoTypeUpdateFallbackFn)(JSContext *, ICUpdatedStub *, HandleValue);
const VMFunction DoTypeUpdateFallbackInfo =
    FunctionInfo<DoTypeUpdateFallbackFn>(DoTypeUpdateFallback);

bool
ICTypeUpdate_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    // Just store false into R1.scratchReg() and return.
    masm.move32(Imm32(0), R1.scratchReg());
    EmitReturnFromIC(masm);
    return true;
}

//
// Compare_Fallback
//

static bool
DoCompareFallback(JSContext *cx, ICCompare_Fallback *stub, HandleValue lhs, HandleValue rhs,
                  MutableHandleValue ret)
{
    uint8_t *returnAddr;
    RootedScript script(cx, GetTopIonJSScript(cx, NULL, (void **)&returnAddr));

    // Perform the compare operation.
    JSOp op = JSOp(*stub->icEntry()->pc(script));
    switch(op) {
      case JSOP_LT: {
        // Do the less than.
        JSBool out;
        if (!LessThan(cx, lhs, rhs, &out))
            return false;
        ret.setBoolean(out);
        break;
      }
      case JSOP_GT: {
        // Do the less than.
        JSBool out;
        if (!GreaterThan(cx, lhs, rhs, &out))
            return false;
        ret.setBoolean(out);
        break;
      }
      default:
        JS_ASSERT(!"Unhandled baseline compare op");
        return false;
    }


    // Check to see if a new stub should be generated.
    if (stub->numOptimizedStubs() >= ICCompare_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (lhs.isInt32()) {
        if (rhs.isInt32()) {
            ICCompare_Int32::Compiler compilerInt32(cx, op);
            ICStub *int32Stub = compilerInt32.getStub();
            if (!int32Stub)
                return false;

            stub->addNewStub(int32Stub);
        }
    }

    return true;
}

typedef bool (*DoCompareFallbackFn)(JSContext *, ICCompare_Fallback *, HandleValue, HandleValue,
                                    MutableHandleValue);
static const VMFunction DoCompareFallbackInfo = FunctionInfo<DoCompareFallbackFn>(DoCompareFallback);

bool
ICCompare_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return tailCallVM(DoCompareFallbackInfo, masm);
}

//
// ToBool_Fallback
//

static bool
DoToBoolFallback(JSContext *cx, ICToBool_Fallback *stub, HandleValue arg, MutableHandleValue ret)
{
    bool cond = ToBoolean(arg);
    ret.setBoolean(cond);

    // Check to see if a new stub should be generated.
    if (stub->numOptimizedStubs() >= ICToBool_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (arg.isBoolean()) {
        // Attach the new bool-specialized stub.
        ICToBool_Bool::Compiler compilerBool(cx);
        ICStub *boolStub = compilerBool.getStub();
        if (!boolStub)
            return false;

        stub->addNewStub(boolStub);
        return true;
    }

    if (arg.isInt32()) {
        ICToBool_Int32::Compiler compiler(cx);
        ICStub *int32Stub = compiler.getStub();
        if (!int32Stub)
            return false;

        stub->addNewStub(int32Stub);
        return true;
    }

    return true;
}

typedef bool (*pf)(JSContext *, ICToBool_Fallback *, HandleValue, MutableHandleValue);
static const VMFunction fun = FunctionInfo<pf>(DoToBoolFallback);

bool
ICToBool_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    // Call.
    return tailCallVM(fun, masm);
}

//
// ToBool_Bool
//

bool
ICToBool_Bool::Compiler::generateStubCode(MacroAssembler &masm)
{
    // Just guard that R0 is a boolean and leave it be if so.
    Label failure;
    masm.branchTestBoolean(Assembler::NotEqual, R0, &failure);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}

//
// ToBool_Int32
//

bool
ICToBool_Int32::Compiler::generateStubCode(MacroAssembler &masm)
{
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);

    Label ifFalse;
    Assembler::Condition cond = masm.testInt32Truthy(false, R0);
    masm.j(cond, &ifFalse);

    masm.moveValue(BooleanValue(true), R0);
    EmitReturnFromIC(masm);

    masm.bind(&ifFalse);
    masm.moveValue(BooleanValue(false), R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// ToNumber_Fallback
//

static bool
DoToNumberFallback(JSContext *cx, ICToNumber_Fallback *stub, HandleValue arg, MutableHandleValue ret)
{
    ret.set(arg);
    return ToNumber(cx, ret.address());
}

typedef bool (*DoToNumberFallbackFn)(JSContext *, ICToNumber_Fallback *, HandleValue, MutableHandleValue);
static const VMFunction DoToNumberFallbackInfo = FunctionInfo<DoToNumberFallbackFn>(DoToNumberFallback);

bool
ICToNumber_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return tailCallVM(DoToNumberFallbackInfo, masm);
}

//
// BinaryArith_Fallback
//

static bool
DoBinaryArithFallback(JSContext *cx, ICBinaryArith_Fallback *stub, HandleValue lhs,
                      HandleValue rhs, MutableHandleValue ret)
{
    uint8_t *returnAddr;
    RootedScript script(cx, GetTopIonJSScript(cx, NULL, (void **)&returnAddr));

    // Perform the compare operation.
    JSOp op = JSOp(*stub->icEntry()->pc(script));
    switch(op) {
      case JSOP_ADD:
        // Do an add.
        if (!AddValues(cx, script, stub->icEntry()->pc(script), lhs, rhs, ret.address()))
            return false;
        break;
      case JSOP_SUB:
        if (!SubValues(cx, script, stub->icEntry()->pc(script), lhs, rhs, ret.address()))
            return false;
        break;
      case JSOP_MUL:
        if (!MulValues(cx, script, stub->icEntry()->pc(script), lhs, rhs, ret.address()))
            return false;
        break;
      case JSOP_BITOR: {
        int32_t result;
        if (!BitOr(cx, lhs, rhs, &result))
            return false;
        ret.setInt32(result);
        break;
      }
      case JSOP_BITXOR: {
        int32_t result;
        if (!BitXor(cx, lhs, rhs, &result))
            return false;
        ret.setInt32(result);
        break;
      }
      case JSOP_BITAND: {
        int32_t result;
        if (!BitAnd(cx, lhs, rhs, &result))
            return false;
        ret.setInt32(result);
        break;
      }
      case JSOP_LSH: {
        int32_t result;
        if (!BitLsh(cx, lhs, rhs, &result))
            return false;
        ret.setInt32(result);
        break;
      }
      case JSOP_RSH: {
        int32_t result;
        if (!BitRsh(cx, lhs, rhs, &result))
            return false;
        ret.setInt32(result);
        break;
      }
      case JSOP_URSH: {
        if (!UrshOperation(cx, script, stub->icEntry()->pc(script), lhs, rhs, ret.address()))
            return false;
        break;
      }
      default:
        JS_NOT_REACHED("Unhandled baseline arith op");
        return false;
    }

    // Check to see if a new stub should be generated.
    if (stub->numOptimizedStubs() >= ICBinaryArith_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    if (!lhs.isInt32() || !rhs.isInt32())
        return true;

    // Try to generate new stubs.
    // TODO: unlink previous !allowDouble stub.
    bool allowDouble = ret.isDouble();
    ICBinaryArith_Int32::Compiler compilerInt32(cx, op, allowDouble);
    ICStub *int32Stub = compilerInt32.getStub();
    if (!int32Stub)
        return false;
    stub->addNewStub(int32Stub);
    return true;
}

typedef bool (*DoBinaryArithFallbackFn)(JSContext *, ICBinaryArith_Fallback *, HandleValue, HandleValue,
                                        MutableHandleValue);
static const VMFunction DoBinaryArithFallbackInfo =
    FunctionInfo<DoBinaryArithFallbackFn>(DoBinaryArithFallback);

bool
ICBinaryArith_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return tailCallVM(DoBinaryArithFallbackInfo, masm);
}

//
// UnaryArith_Fallback
//

static bool
DoUnaryArithFallback(JSContext *cx, ICUnaryArith_Fallback *stub, HandleValue val,
                     MutableHandleValue res)
{
    RootedScript script(cx, GetTopIonJSScript(cx));
    jsbytecode *pc = stub->icEntry()->pc(script);

    JSOp op = JSOp(*pc);

    switch (op) {
      case JSOP_BITNOT: {
        int32_t result;
        if (!BitNot(cx, val, &result))
            return false;
        res.setInt32(result);
        break;
      }
      case JSOP_NEG:
        if (!NegOperation(cx, script, pc, val, res))
            return false;
        break;
      default:
        JS_NOT_REACHED("Unexpected op");
        return false;
    }

    if (stub->numOptimizedStubs() >= ICUnaryArith_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard/replace stubs.
        return true;
    }

    if (!val.isInt32() || !res.isInt32())
        return true;

    ICUnaryArith_Int32::Compiler compiler(cx, op);
    ICStub *int32Stub = compiler.getStub();
    if (!int32Stub)
        return false;
    stub->addNewStub(int32Stub);
    return true;
}

typedef bool (*DoUnaryArithFallbackFn)(JSContext *, ICUnaryArith_Fallback *, HandleValue,
                                       MutableHandleValue);
static const VMFunction DoUnaryArithFallbackInfo =
    FunctionInfo<DoUnaryArithFallbackFn>(DoUnaryArithFallback);

bool
ICUnaryArith_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return tailCallVM(DoUnaryArithFallbackInfo, masm);
}

//
// GetElem_Fallback
//

static bool
DoGetElemFallback(JSContext *cx, ICGetElem_Fallback *stub, HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    if (!GetElementMonitored(cx, lhs, rhs, res))
        return false;

    if (stub->numOptimizedStubs() >= ICGetElem_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (!lhs.isObject())
        return true;

    RootedObject obj(cx, &lhs.toObject());
    if (obj->isDenseArray() && rhs.isInt32()) {
        // Don't attach multiple dense array stubs.
        if (stub->hasStub(ICStub::GetElem_Dense))
            return true;

        ICGetElem_Dense::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub());
        ICStub *denseStub = compiler.getStub();
        if (!denseStub)
            return false;

        stub->addNewStub(denseStub);
    }

    return true;
}

typedef bool (*DoGetElemFallbackFn)(JSContext *, ICGetElem_Fallback *, HandleValue, HandleValue,
                                    MutableHandleValue);
static const VMFunction DoGetElemFallbackInfo = FunctionInfo<DoGetElemFallbackFn>(DoGetElemFallback);

bool
ICGetElem_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return tailCallVM(DoGetElemFallbackInfo, masm);
}

//
// GetElem_Dense
//

bool
ICGetElem_Dense::Compiler::generateStubCode(MacroAssembler &masm)
{
    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    RootedShape shape(cx, GetDenseArrayShape(cx, cx->global()));
    if (!shape)
        return false;

    // Unbox R0 and guard it's a dense array.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.branchTestObjShape(Assembler::NotEqual, obj, shape, &failure);

    // Load obj->elements in scratchReg.
    GeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();
    masm.loadPtr(Address(obj, JSObject::offsetOfElements()), scratchReg);

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    // Bounds check.
    Address initLength(scratchReg, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, key, &failure);

    // Hole check and load value.
    BaseIndex element(scratchReg, key, TimesEight);
    masm.branchTestMagic(Assembler::Equal, element, &failure);
    masm.loadValue(element, R0);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// SetElem_Fallback
//

static bool
DoSetElemFallback(JSContext *cx, ICSetElem_Fallback *stub, HandleValue rhs, HandleValue objv,
                  HandleValue index)
{
    RootedObject obj(cx, ToObject(cx, objv));
    if (!obj)
        return false;

    RootedScript script(cx, GetTopIonJSScript(cx));
    if (!SetObjectElement(cx, obj, index, rhs, script->strictModeCode))
        return false;

    if (stub->numOptimizedStubs() >= ICSetElem_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (obj->isDenseArray() && index.isInt32()) {
        RootedTypeObject type(cx, obj->getType(cx));
        ICSetElem_Dense::Compiler compiler(cx, type);
        ICStub *denseStub = compiler.getStub();
        if (!denseStub)
            return false;

        stub->addNewStub(denseStub);
    }

    return true;
}

typedef bool (*DoSetElemFallbackFn)(JSContext *, ICSetElem_Fallback *, HandleValue, HandleValue,
                                    HandleValue);
static const VMFunction DoSetElemFallbackInfo = FunctionInfo<DoSetElemFallbackFn>(DoSetElemFallback);

bool
ICSetElem_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    // Push key and object.
    masm.pushValue(R1);
    masm.pushValue(R0);

    // Push RHS. On x86 and ARM two push instructions are emitted so use a
    // separate register to store the old stack pointer.
    masm.mov(BaselineStackReg, R0.scratchReg());
    masm.pushValue(Address(R0.scratchReg(), 2 * sizeof(Value)));

    masm.push(BaselineStubReg);

    return tailCallVM(DoSetElemFallbackInfo, masm);
}

//
// SetElem_Dense
//

bool
ICSetElem_Dense::Compiler::generateStubCode(MacroAssembler &masm)
{
    // R0 = object
    // R1 = key
    // Stack = { ... rhs-value, <return-addr>? }
    Label failure;
    Label failureUnstow;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    RootedShape shape(cx, GetDenseArrayShape(cx, cx->global()));
    if (!shape)
        return false;

    // Unbox R0 and guard it's a dense array.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.branchTestObjShape(Assembler::NotEqual, obj, shape, &failure);

    // Stow both R0 and R1 (object and key)
    // But R0 and R1 still hold their values.
    EmitStowICValues(masm, 2);

    // We may need to free up some registers
    GeneralRegisterSet regs(availableGeneralRegs(0));
    regs.take(R0);

    // Guard that the type object matches.
    Register typeReg = regs.takeAny();
    masm.loadPtr(Address(BaselineStubReg, ICSetElem_Dense::offsetOfType()), typeReg);
    masm.branchPtr(Assembler::NotEqual, Address(obj, JSObject::offsetOfType()), typeReg,
                   &failureUnstow);

    // Stack is now: { ..., rhs-value, object-value, key-value, maybe?-RET-ADDR }
    // Load rhs-value in to R0
    masm.loadValue(Address(BaselineStackReg, 2 * sizeof(Value) + ICStackValueOffset), R0);

    // Call the type-update stub.
    if (!callTypeUpdateIC(masm))
        return false;

    // Unstow R0 and R1 (object and key)
    EmitUnstowICValues(masm, 2);

    // Load obj->elements in scratchReg.
    regs = availableGeneralRegs(2);
    Register scratchReg = regs.takeAny();
    masm.loadPtr(Address(obj, JSObject::offsetOfElements()), scratchReg);

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    // Bounds check.
    Address initLength(scratchReg, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, key, &failure);

    // Hole check.
    BaseIndex element(scratchReg, key, TimesEight);
    masm.branchTestMagic(Assembler::Equal, element, &failure);

    // It's safe to overwrite R0 now.
    masm.loadValue(Address(BaselineStackReg, ICStackValueOffset), R0);
    masm.storeValue(R0, element);
    EmitReturnFromIC(masm);

    // Failure case - fail but first unstow R0 and R1
    masm.bind(&failureUnstow);
    EmitUnstowICValues(masm, 2);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// GetName_Fallback
//

static bool
DoGetNameFallback(JSContext *cx, ICGetName_Fallback *stub, HandleObject scopeChain, MutableHandleValue res)
{
    RootedScript script(cx, GetTopIonJSScript(cx));
    jsbytecode *pc = stub->icEntry()->pc(script);

    JS_ASSERT(JSOp(*pc) == JSOP_GETGNAME || JSOp(*pc) == JSOP_CALLGNAME);

    RootedPropertyName name(cx, script->getName(pc));

    if (JSOp(pc[JSOP_GETGNAME_LENGTH]) == JSOP_TYPEOF) {
        if (!GetScopeNameForTypeOf(cx, scopeChain, name, res))
            return false;
    } else {
        if (!GetScopeName(cx, scopeChain, name, res))
            return false;
    }

    types::TypeScript::Monitor(cx, script, pc, res);

    return true;
}

typedef bool (*DoGetNameFallbackFn)(JSContext *, ICGetName_Fallback *, HandleObject, MutableHandleValue);
static const VMFunction DoGetNameFallbackInfo = FunctionInfo<DoGetNameFallbackFn>(DoGetNameFallback);

bool
ICGetName_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    masm.push(R0.scratchReg());
    masm.push(BaselineStubReg);

    return tailCallVM(DoGetNameFallbackInfo, masm);
}

//
// Call_Fallback
//

static bool
DoCallFallback(JSContext *cx, ICCall_Fallback *stub, uint32_t argc, Value *vp, MutableHandleValue res)
{
    RootedValue callee(cx, vp[0]);
    RootedValue thisv(cx, vp[1]);

    Value *args = vp + 2;

    RootedScript script(cx, GetTopIonJSScript(cx));
    JSOp op = JSOp(*stub->icEntry()->pc(script));

    if (op == JSOP_NEW) {
        if (!InvokeConstructor(cx, callee, argc, args, res.address()))
            return false;
    } else {
        JS_ASSERT(op == JSOP_CALL || op == JSOP_FUNCALL || op == JSOP_FUNAPPLY);
        if (!Invoke(cx, thisv, callee, argc, args, res.address()))
            return false;
    }

    types::TypeScript::Monitor(cx, res);

    // Attach new stub.
    if (stub->numOptimizedStubs() >= ICCall_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with generic call stub.
        return true;
    }

    if (op == JSOP_NEW || !callee.isObject())
        return true;

    RootedObject obj(cx, &callee.toObject());
    if (!obj->isFunction())
        return true;

    RootedFunction fun(cx, obj->toFunction());
    if (obj->toFunction()->hasScript()) {
        RootedScript script(cx, fun->nonLazyScript());
        if (!script->hasBaselineScript() && !script->hasIonScript())
            return true;

        ICCall_Scripted::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(), fun);
        ICStub *newStub = compiler.getStub();
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        return true;
    }

    return true;
}

void
ICCallStubCompiler::pushCallArguments(MacroAssembler &masm, GeneralRegisterSet regs, Register argcReg)
{
    JS_ASSERT(!regs.has(argcReg));

    // Push the callee and |this| too.
    Register count = regs.takeAny();
    masm.mov(argcReg, count);
    masm.add32(Imm32(2), count);

    // argPtr initially points to the last argument.
    Register argPtr = regs.takeAny();
    masm.mov(BaselineStackReg, argPtr);

    // Skip 4 pointers pushed on top of the arguments: the frame descriptor,
    // return address, old frame pointer and stub reg.
    masm.addPtr(Imm32(sizeof(void *) * 4), argPtr);

    // Push all values, starting at the last one.
    Label loop, done;
    masm.bind(&loop);
    masm.branchTest32(Assembler::Zero, count, count, &done);
    {
        masm.pushValue(Address(argPtr, 0));
        masm.addPtr(Imm32(sizeof(Value)), argPtr);

        masm.sub32(Imm32(1), count);
        masm.jump(&loop);
    }
    masm.bind(&done);
}

typedef bool (*DoCallFallbackFn)(JSContext *, ICCall_Fallback *, uint32_t, Value *, MutableHandleValue);
static const VMFunction DoCallFallbackInfo = FunctionInfo<DoCallFallbackFn>(DoCallFallback);

bool
ICCall_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Push a stub frame so that we can perform a non-tail call.
    EmitEnterStubFrame(masm, R1.scratchReg());

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.
    // |this| and callee are pushed last.

    GeneralRegisterSet regs(availableGeneralRegs(0));
    regs.take(R0.scratchReg()); // argc.

    pushCallArguments(masm, regs, R0.scratchReg());

    masm.push(BaselineStackReg);
    masm.push(R0.scratchReg());
    masm.push(BaselineStubReg);

    if (!callVM(DoCallFallbackInfo, masm))
        return false;

    EmitLeaveStubFrame(masm);

    EmitReturnFromIC(masm);
    return true;
}

bool
ICCall_Scripted::Compiler::generateStubCode(MacroAssembler &masm)
{
    Label failure;
    GeneralRegisterSet regs(availableGeneralRegs(0));
    bool canUseTailCallReg = regs.has(BaselineTailCallReg);

    Register argcReg = R0.scratchReg();
    JS_ASSERT(argcReg != ArgumentsRectifierReg);

    regs.take(argcReg);
    regs.take(ArgumentsRectifierReg);
    if (regs.has(BaselineTailCallReg))
        regs.take(BaselineTailCallReg);

    // Load the callee in R1.
    BaseIndex calleeSlot(BaselineStackReg, argcReg, TimesEight, ICStackValueOffset + sizeof(Value));
    masm.loadValue(calleeSlot, R1);
    regs.take(R1);

    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    // Ensure callee matches this stub's callee.
    Register callee = masm.extractObject(R1, ExtractTemp0);
    Address expectedCallee(BaselineStubReg, ICCall_Scripted::offsetOfCallee());
    masm.branchPtr(Assembler::NotEqual, expectedCallee, callee, &failure);

    // If the previous check succeeded, the function must be non-native and
    // have a valid, non-lazy script.
    masm.loadPtr(Address(callee, offsetof(JSFunction, u.i.script_)), callee);

    // Call IonScript or BaselineScript.
    masm.loadBaselineOrIonCode(callee);

    // Load the start of the target IonCode.
    Register code = regs.takeAny();
    masm.loadPtr(Address(callee, IonCode::offsetOfCode()), code);

    // We no longer need R1.
    regs.add(R1);

    // Push a stub frame so that we can perform a non-tail call.
    Register scratch = regs.takeAny();
    EmitEnterStubFrame(masm, scratch);
    if (canUseTailCallReg)
        regs.add(BaselineTailCallReg);

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.
    // |this| and callee are pushed last.
    pushCallArguments(masm, regs, argcReg);

    // The callee is on top of the stack. Pop and unbox it.
    ValueOperand val = regs.takeAnyValue();
    masm.popValue(val);
    callee = masm.extractObject(val, ExtractTemp0);

    EmitCreateStubFrameDescriptor(masm, scratch);

    // Note that we use Push, not push, so that callIon will align the stack
    // properly on ARM.
    masm.Push(argcReg);
    masm.Push(callee);
    masm.Push(scratch);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(callee, offsetof(JSFunction, nargs)), callee);
    masm.branch32(Assembler::AboveOrEqual, argcReg, callee, &noUnderflow);
    {
        // Call the arguments rectifier.
        JS_ASSERT(ArgumentsRectifierReg != code);
        JS_ASSERT(ArgumentsRectifierReg != argcReg);

        IonCode *argumentsRectifier = cx->compartment->ionCompartment()->getArgumentsRectifier();

        masm.movePtr(ImmGCPtr(argumentsRectifier), code);
        masm.loadPtr(Address(code, IonCode::offsetOfCode()), code);
        masm.mov(argcReg, ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callIon(code);

    EmitLeaveStubFrame(masm, true);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

} // namespace ion
} // namespace js
