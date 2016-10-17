/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MacroAssembler_inl_h
#define jit_MacroAssembler_inl_h

#include "jit/MacroAssembler.h"

#include "mozilla/MathAlgorithms.h"

#if defined(JS_CODEGEN_X86)
# include "jit/x86/MacroAssembler-x86-inl.h"
#elif defined(JS_CODEGEN_X64)
# include "jit/x64/MacroAssembler-x64-inl.h"
#elif defined(JS_CODEGEN_ARM)
# include "jit/arm/MacroAssembler-arm-inl.h"
#elif defined(JS_CODEGEN_ARM64)
# include "jit/arm64/MacroAssembler-arm64-inl.h"
#elif defined(JS_CODEGEN_MIPS32)
# include "jit/mips32/MacroAssembler-mips32-inl.h"
#elif defined(JS_CODEGEN_MIPS64)
# include "jit/mips64/MacroAssembler-mips64-inl.h"
#elif !defined(JS_CODEGEN_NONE)
# error "Unknown architecture!"
#endif

namespace js {
namespace jit {

//{{{ check_macroassembler_style
// ===============================================================
// Frame manipulation functions.

uint32_t
MacroAssembler::framePushed() const
{
    return framePushed_;
}

void
MacroAssembler::setFramePushed(uint32_t framePushed)
{
    framePushed_ = framePushed;
}

void
MacroAssembler::adjustFrame(int32_t value)
{
    MOZ_ASSERT_IF(value < 0, framePushed_ >= uint32_t(-value));
    setFramePushed(framePushed_ + value);
}

void
MacroAssembler::implicitPop(uint32_t bytes)
{
    MOZ_ASSERT(bytes % sizeof(intptr_t) == 0);
    MOZ_ASSERT(bytes <= INT32_MAX);
    adjustFrame(-int32_t(bytes));
}

// ===============================================================
// Stack manipulation functions.

CodeOffset
MacroAssembler::PushWithPatch(ImmWord word)
{
    framePushed_ += sizeof(word.value);
    return pushWithPatch(word);
}

CodeOffset
MacroAssembler::PushWithPatch(ImmPtr imm)
{
    return PushWithPatch(ImmWord(uintptr_t(imm.value)));
}

// ===============================================================
// Simple call functions.

void
MacroAssembler::call(const wasm::CallSiteDesc& desc, const Register reg)
{
    CodeOffset l = call(reg);
    append(desc, l, framePushed());
}

void
MacroAssembler::call(const wasm::CallSiteDesc& desc, uint32_t funcDefIndex)
{
    CodeOffset l = callWithPatch();
    append(desc, l, framePushed(), funcDefIndex);
}

void
MacroAssembler::call(const wasm::CallSiteDesc& desc, wasm::Trap trap)
{
    CodeOffset l = callWithPatch();
    append(desc, l, framePushed(), trap);
}

// ===============================================================
// ABI function calls.

void
MacroAssembler::passABIArg(Register reg)
{
    passABIArg(MoveOperand(reg), MoveOp::GENERAL);
}

void
MacroAssembler::passABIArg(FloatRegister reg, MoveOp::Type type)
{
    passABIArg(MoveOperand(reg), type);
}

template <typename T> void
MacroAssembler::callWithABI(const T& fun, MoveOp::Type result)
{
    AutoProfilerCallInstrumentation profiler(*this);
    callWithABINoProfiler(fun, result);
}

void
MacroAssembler::appendSignatureType(MoveOp::Type type)
{
#ifdef JS_SIMULATOR
    signature_ <<= ArgType_Shift;
    switch (type) {
      case MoveOp::GENERAL: signature_ |= ArgType_General; break;
      case MoveOp::DOUBLE:  signature_ |= ArgType_Double;  break;
      case MoveOp::FLOAT32: signature_ |= ArgType_Float32; break;
      default: MOZ_CRASH("Invalid argument type");
    }
#endif
}

ABIFunctionType
MacroAssembler::signature() const
{
#ifdef JS_SIMULATOR
#ifdef DEBUG
    switch (signature_) {
      case Args_General0:
      case Args_General1:
      case Args_General2:
      case Args_General3:
      case Args_General4:
      case Args_General5:
      case Args_General6:
      case Args_General7:
      case Args_General8:
      case Args_Double_None:
      case Args_Int_Double:
      case Args_Float32_Float32:
      case Args_Double_Double:
      case Args_Double_Int:
      case Args_Double_DoubleInt:
      case Args_Double_DoubleDouble:
      case Args_Double_IntDouble:
      case Args_Int_IntDouble:
      case Args_Int_DoubleIntInt:
      case Args_Int_IntDoubleIntInt:
      case Args_Double_DoubleDoubleDouble:
      case Args_Double_DoubleDoubleDoubleDouble:
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }
#endif // DEBUG

    return ABIFunctionType(signature_);
#else
    // No simulator enabled.
    MOZ_CRASH("Only available for making calls within a simulator.");
#endif
}

// ===============================================================
// Jit Frames.

uint32_t
MacroAssembler::callJitNoProfiler(Register callee)
{
#ifdef JS_USE_LINK_REGISTER
    // The return address is pushed by the callee.
    call(callee);
#else
    callAndPushReturnAddress(callee);
#endif
    return currentOffset();
}

uint32_t
MacroAssembler::callJit(Register callee)
{
    AutoProfilerCallInstrumentation profiler(*this);
    uint32_t ret = callJitNoProfiler(callee);
    return ret;
}

uint32_t
MacroAssembler::callJit(JitCode* callee)
{
    AutoProfilerCallInstrumentation profiler(*this);
    call(callee);
    return currentOffset();
}

void
MacroAssembler::makeFrameDescriptor(Register frameSizeReg, FrameType type, uint32_t headerSize)
{
    // See JitFrames.h for a description of the frame descriptor format.
    // The saved-frame bit is zero for new frames. See js::SavedStacks.

    lshiftPtr(Imm32(FRAMESIZE_SHIFT), frameSizeReg);

    headerSize = EncodeFrameHeaderSize(headerSize);
    orPtr(Imm32((headerSize << FRAME_HEADER_SIZE_SHIFT) | type), frameSizeReg);
}

void
MacroAssembler::pushStaticFrameDescriptor(FrameType type, uint32_t headerSize)
{
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), type, headerSize);
    Push(Imm32(descriptor));
}

void
MacroAssembler::PushCalleeToken(Register callee, bool constructing)
{
    if (constructing) {
        orPtr(Imm32(CalleeToken_FunctionConstructing), callee);
        Push(callee);
        andPtr(Imm32(uint32_t(CalleeTokenMask)), callee);
    } else {
        static_assert(CalleeToken_Function == 0, "Non-constructing call requires no tagging");
        Push(callee);
    }
}

void
MacroAssembler::loadFunctionFromCalleeToken(Address token, Register dest)
{
#ifdef DEBUG
    Label ok;
    loadPtr(token, dest);
    andPtr(Imm32(uint32_t(~CalleeTokenMask)), dest);
    branchPtr(Assembler::Equal, dest, Imm32(CalleeToken_Function), &ok);
    branchPtr(Assembler::Equal, dest, Imm32(CalleeToken_FunctionConstructing), &ok);
    assumeUnreachable("Unexpected CalleeToken tag");
    bind(&ok);
#endif
    loadPtr(token, dest);
    andPtr(Imm32(uint32_t(CalleeTokenMask)), dest);
}

uint32_t
MacroAssembler::buildFakeExitFrame(Register scratch)
{
    mozilla::DebugOnly<uint32_t> initialDepth = framePushed();

    pushStaticFrameDescriptor(JitFrame_IonJS, ExitFrameLayout::Size());
    uint32_t retAddr = pushFakeReturnAddress(scratch);

    MOZ_ASSERT(framePushed() == initialDepth + ExitFrameLayout::Size());
    return retAddr;
}

// ===============================================================
// Exit frame footer.

void
MacroAssembler::PushStubCode()
{
    // Make sure that we do not erase an existing self-reference.
    MOZ_ASSERT(!hasSelfReference());
    selfReferencePatch_ = PushWithPatch(ImmWord(-1));
}

void
MacroAssembler::enterExitFrame(const VMFunction* f)
{
    linkExitFrame();
    // Push the JitCode pointer. (Keep the code alive, when on the stack)
    PushStubCode();
    // Push VMFunction pointer, to mark arguments.
    Push(ImmPtr(f));
}

void
MacroAssembler::enterFakeExitFrame(enum ExitFrameTokenValues token)
{
    linkExitFrame();
    Push(Imm32(token));
    Push(ImmPtr(nullptr));
}

void
MacroAssembler::enterFakeExitFrameForNative(bool isConstructing)
{
    enterFakeExitFrame(isConstructing ? ConstructNativeExitFrameLayoutToken
                                      : CallNativeExitFrameLayoutToken);
}

void
MacroAssembler::leaveExitFrame(size_t extraFrame)
{
    freeStack(ExitFooterFrame::Size() + extraFrame);
}

bool
MacroAssembler::hasSelfReference() const
{
    return selfReferencePatch_.bound();
}

// ===============================================================
// Arithmetic functions

void
MacroAssembler::addPtr(ImmPtr imm, Register dest)
{
    addPtr(ImmWord(uintptr_t(imm.value)), dest);
}

void
MacroAssembler::inc32(RegisterOrInt32Constant* key)
{
    if (key->isRegister())
        add32(Imm32(1), key->reg());
    else
        key->bumpConstant(1);
}

void
MacroAssembler::dec32(RegisterOrInt32Constant* key)
{
    if (key->isRegister())
        add32(Imm32(-1), key->reg());
    else
        key->bumpConstant(-1);
}

// ===============================================================
// Branch functions

void
MacroAssembler::branch32(Condition cond, Register length, const RegisterOrInt32Constant& key,
                         Label* label)
{
    branch32Impl(cond, length, key, label);
}

void
MacroAssembler::branch32(Condition cond, const Address& length, const RegisterOrInt32Constant& key,
                         Label* label)
{
    branch32Impl(cond, length, key, label);
}

template <typename T>
void
MacroAssembler::branch32Impl(Condition cond, const T& length, const RegisterOrInt32Constant& key,
                             Label* label)
{
    if (key.isRegister())
        branch32(cond, length, key.reg(), label);
    else
        branch32(cond, length, Imm32(key.constant()), label);
}

template <class L>
void
MacroAssembler::branchIfFalseBool(Register reg, L label)
{
    // Note that C++ bool is only 1 byte, so ignore the higher-order bits.
    branchTest32(Assembler::Zero, reg, Imm32(0xFF), label);
}

void
MacroAssembler::branchIfTrueBool(Register reg, Label* label)
{
    // Note that C++ bool is only 1 byte, so ignore the higher-order bits.
    branchTest32(Assembler::NonZero, reg, Imm32(0xFF), label);
}

void
MacroAssembler::branchIfRope(Register str, Label* label)
{
    Address flags(str, JSString::offsetOfFlags());
    static_assert(JSString::ROPE_FLAGS == 0, "Rope type flags must be 0");
    branchTest32(Assembler::Zero, flags, Imm32(JSString::TYPE_FLAGS_MASK), label);
}

void
MacroAssembler::branchLatin1String(Register string, Label* label)
{
    branchTest32(Assembler::NonZero, Address(string, JSString::offsetOfFlags()),
                 Imm32(JSString::LATIN1_CHARS_BIT), label);
}

void
MacroAssembler::branchTwoByteString(Register string, Label* label)
{
    branchTest32(Assembler::Zero, Address(string, JSString::offsetOfFlags()),
                 Imm32(JSString::LATIN1_CHARS_BIT), label);
}

void
MacroAssembler::branchIfFunctionHasNoScript(Register fun, Label* label)
{
    // 16-bit loads are slow and unaligned 32-bit loads may be too so
    // perform an aligned 32-bit load and adjust the bitmask accordingly.
    MOZ_ASSERT(JSFunction::offsetOfNargs() % sizeof(uint32_t) == 0);
    MOZ_ASSERT(JSFunction::offsetOfFlags() == JSFunction::offsetOfNargs() + 2);
    Address address(fun, JSFunction::offsetOfNargs());
    int32_t bit = IMM32_16ADJ(JSFunction::INTERPRETED);
    branchTest32(Assembler::Zero, address, Imm32(bit), label);
}

void
MacroAssembler::branchIfInterpreted(Register fun, Label* label)
{
    // 16-bit loads are slow and unaligned 32-bit loads may be too so
    // perform an aligned 32-bit load and adjust the bitmask accordingly.
    MOZ_ASSERT(JSFunction::offsetOfNargs() % sizeof(uint32_t) == 0);
    MOZ_ASSERT(JSFunction::offsetOfFlags() == JSFunction::offsetOfNargs() + 2);
    Address address(fun, JSFunction::offsetOfNargs());
    int32_t bit = IMM32_16ADJ(JSFunction::INTERPRETED);
    branchTest32(Assembler::NonZero, address, Imm32(bit), label);
}

void
MacroAssembler::branchFunctionKind(Condition cond, JSFunction::FunctionKind kind, Register fun,
                                   Register scratch, Label* label)
{
    // 16-bit loads are slow and unaligned 32-bit loads may be too so
    // perform an aligned 32-bit load and adjust the bitmask accordingly.
    MOZ_ASSERT(JSFunction::offsetOfNargs() % sizeof(uint32_t) == 0);
    MOZ_ASSERT(JSFunction::offsetOfFlags() == JSFunction::offsetOfNargs() + 2);
    Address address(fun, JSFunction::offsetOfNargs());
    int32_t mask = IMM32_16ADJ(JSFunction::FUNCTION_KIND_MASK);
    int32_t bit = IMM32_16ADJ(kind << JSFunction::FUNCTION_KIND_SHIFT);
    load32(address, scratch);
    and32(Imm32(mask), scratch);
    branch32(cond, scratch, Imm32(bit), label);
}

void
MacroAssembler::branchTestObjClass(Condition cond, Register obj, Register scratch, const js::Class* clasp,
                                   Label* label)
{
    loadObjGroup(obj, scratch);
    branchPtr(cond, Address(scratch, ObjectGroup::offsetOfClasp()), ImmPtr(clasp), label);
}

void
MacroAssembler::branchTestObjShape(Condition cond, Register obj, const Shape* shape, Label* label)
{
    branchPtr(cond, Address(obj, ShapedObject::offsetOfShape()), ImmGCPtr(shape), label);
}

void
MacroAssembler::branchTestObjShape(Condition cond, Register obj, Register shape, Label* label)
{
    branchPtr(cond, Address(obj, ShapedObject::offsetOfShape()), shape, label);
}

void
MacroAssembler::branchTestObjGroup(Condition cond, Register obj, ObjectGroup* group, Label* label)
{
    branchPtr(cond, Address(obj, JSObject::offsetOfGroup()), ImmGCPtr(group), label);
}

void
MacroAssembler::branchTestObjGroup(Condition cond, Register obj, Register group, Label* label)
{
    branchPtr(cond, Address(obj, JSObject::offsetOfGroup()), group, label);
}

void
MacroAssembler::branchTestObjectTruthy(bool truthy, Register objReg, Register scratch,
                                       Label* slowCheck, Label* checked)
{
    // The branches to out-of-line code here implement a conservative version
    // of the JSObject::isWrapper test performed in EmulatesUndefined.  If none
    // of the branches are taken, we can check class flags directly.
    loadObjClass(objReg, scratch);
    Address flags(scratch, Class::offsetOfFlags());

    branchTestClassIsProxy(true, scratch, slowCheck);

    Condition cond = truthy ? Assembler::Zero : Assembler::NonZero;
    branchTest32(cond, flags, Imm32(JSCLASS_EMULATES_UNDEFINED), checked);
}

void
MacroAssembler::branchTestClassIsProxy(bool proxy, Register clasp, Label* label)
{
    branchTest32(proxy ? Assembler::NonZero : Assembler::Zero,
                 Address(clasp, Class::offsetOfFlags()),
                 Imm32(JSCLASS_IS_PROXY), label);
}

void
MacroAssembler::branchTestObjectIsProxy(bool proxy, Register object, Register scratch, Label* label)
{
    loadObjClass(object, scratch);
    branchTestClassIsProxy(proxy, scratch, label);
}

void
MacroAssembler::branchTestProxyHandlerFamily(Condition cond, Register proxy, Register scratch,
                                             const void* handlerp, Label* label)
{
    Address handlerAddr(proxy, ProxyObject::offsetOfHandler());
    loadPtr(handlerAddr, scratch);
    Address familyAddr(scratch, BaseProxyHandler::offsetOfFamily());
    branchPtr(cond, familyAddr, ImmPtr(handlerp), label);
}

template <typename Value>
void
MacroAssembler::branchTestMIRType(Condition cond, const Value& val, MIRType type, Label* label)
{
    switch (type) {
      case MIRType::Null:      return branchTestNull(cond, val, label);
      case MIRType::Undefined: return branchTestUndefined(cond, val, label);
      case MIRType::Boolean:   return branchTestBoolean(cond, val, label);
      case MIRType::Int32:     return branchTestInt32(cond, val, label);
      case MIRType::String:    return branchTestString(cond, val, label);
      case MIRType::Symbol:    return branchTestSymbol(cond, val, label);
      case MIRType::Object:    return branchTestObject(cond, val, label);
      case MIRType::Double:    return branchTestDouble(cond, val, label);
      case MIRType::MagicOptimizedArguments: // Fall through.
      case MIRType::MagicIsConstructing:
      case MIRType::MagicHole: return branchTestMagic(cond, val, label);
      default:
        MOZ_CRASH("Bad MIRType");
    }
}

void
MacroAssembler::branchTestNeedsIncrementalBarrier(Condition cond, Label* label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero);
    CompileZone* zone = GetJitContext()->compartment->zone();
    AbsoluteAddress needsBarrierAddr(zone->addressOfNeedsIncrementalBarrier());
    branchTest32(cond, needsBarrierAddr, Imm32(0x1), label);
}

void
MacroAssembler::branchTestMagicValue(Condition cond, const ValueOperand& val, JSWhyMagic why,
                                     Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    branchTestValue(cond, val, MagicValue(why), label);
}

void
MacroAssembler::branchDoubleNotInInt64Range(Address src, Register temp, Label* fail)
{
    // Tests if double is in [INT64_MIN; INT64_MAX] range
    uint32_t EXPONENT_MASK = 0x7ff00000;
    uint32_t EXPONENT_SHIFT = FloatingPoint<double>::kExponentShift - 32;
    uint32_t TOO_BIG_EXPONENT = (FloatingPoint<double>::kExponentBias + 63) << EXPONENT_SHIFT;

    load32(Address(src.base, src.offset + sizeof(int32_t)), temp);
    and32(Imm32(EXPONENT_MASK), temp);
    branch32(Assembler::GreaterThanOrEqual, temp, Imm32(TOO_BIG_EXPONENT), fail);
}

void
MacroAssembler::branchDoubleNotInUInt64Range(Address src, Register temp, Label* fail)
{
    // Note: returns failure on -0.0
    // Tests if double is in [0; UINT64_MAX] range
    // Take the sign also in the equation. That way we can compare in one test?
    uint32_t EXPONENT_MASK = 0xfff00000;
    uint32_t EXPONENT_SHIFT = FloatingPoint<double>::kExponentShift - 32;
    uint32_t TOO_BIG_EXPONENT = (FloatingPoint<double>::kExponentBias + 64) << EXPONENT_SHIFT;

    load32(Address(src.base, src.offset + sizeof(int32_t)), temp);
    and32(Imm32(EXPONENT_MASK), temp);
    branch32(Assembler::AboveOrEqual, temp, Imm32(TOO_BIG_EXPONENT), fail);
}

void
MacroAssembler::branchFloat32NotInInt64Range(Address src, Register temp, Label* fail)
{
    // Tests if float is in [INT64_MIN; INT64_MAX] range
    uint32_t EXPONENT_MASK = 0x7f800000;
    uint32_t EXPONENT_SHIFT = FloatingPoint<float>::kExponentShift;
    uint32_t TOO_BIG_EXPONENT = (FloatingPoint<float>::kExponentBias + 63) << EXPONENT_SHIFT;

    load32(src, temp);
    and32(Imm32(EXPONENT_MASK), temp);
    branch32(Assembler::GreaterThanOrEqual, temp, Imm32(TOO_BIG_EXPONENT), fail);
}

void
MacroAssembler::branchFloat32NotInUInt64Range(Address src, Register temp, Label* fail)
{
    // Note: returns failure on -0.0
    // Tests if float is in [0; UINT64_MAX] range
    // Take the sign also in the equation. That way we can compare in one test?
    uint32_t EXPONENT_MASK = 0xff800000;
    uint32_t EXPONENT_SHIFT = FloatingPoint<float>::kExponentShift;
    uint32_t TOO_BIG_EXPONENT = (FloatingPoint<float>::kExponentBias + 64) << EXPONENT_SHIFT;

    load32(src, temp);
    and32(Imm32(EXPONENT_MASK), temp);
    branch32(Assembler::AboveOrEqual, temp, Imm32(TOO_BIG_EXPONENT), fail);
}

// ========================================================================
// Canonicalization primitives.
void
MacroAssembler::canonicalizeFloat(FloatRegister reg)
{
    Label notNaN;
    branchFloat(DoubleOrdered, reg, reg, &notNaN);
    loadConstantFloat32(float(JS::GenericNaN()), reg);
    bind(&notNaN);
}

void
MacroAssembler::canonicalizeFloatIfDeterministic(FloatRegister reg)
{
#ifdef JS_MORE_DETERMINISTIC
    // See the comment in TypedArrayObjectTemplate::getIndexValue.
    canonicalizeFloat(reg);
#endif // JS_MORE_DETERMINISTIC
}

void
MacroAssembler::canonicalizeDouble(FloatRegister reg)
{
    Label notNaN;
    branchDouble(DoubleOrdered, reg, reg, &notNaN);
    loadConstantDouble(JS::GenericNaN(), reg);
    bind(&notNaN);
}

void
MacroAssembler::canonicalizeDoubleIfDeterministic(FloatRegister reg)
{
#ifdef JS_MORE_DETERMINISTIC
    // See the comment in TypedArrayObjectTemplate::getIndexValue.
    canonicalizeDouble(reg);
#endif // JS_MORE_DETERMINISTIC
}

// ========================================================================
// Memory access primitives.
template<class T> void
MacroAssembler::storeDouble(FloatRegister src, const T& dest)
{
    canonicalizeDoubleIfDeterministic(src);
    storeUncanonicalizedDouble(src, dest);
}

template void MacroAssembler::storeDouble(FloatRegister src, const Address& dest);
template void MacroAssembler::storeDouble(FloatRegister src, const BaseIndex& dest);

template<class T> void
MacroAssembler::storeFloat32(FloatRegister src, const T& dest)
{
    canonicalizeFloatIfDeterministic(src);
    storeUncanonicalizedFloat32(src, dest);
}

template void MacroAssembler::storeFloat32(FloatRegister src, const Address& dest);
template void MacroAssembler::storeFloat32(FloatRegister src, const BaseIndex& dest);

//}}} check_macroassembler_style
// ===============================================================

#ifndef JS_CODEGEN_ARM64

template <typename T>
void
MacroAssembler::branchTestStackPtr(Condition cond, T t, Label* label)
{
    branchTestPtr(cond, getStackPointer(), t, label);
}

template <typename T>
void
MacroAssembler::branchStackPtr(Condition cond, T rhs, Label* label)
{
    branchPtr(cond, getStackPointer(), rhs, label);
}

template <typename T>
void
MacroAssembler::branchStackPtrRhs(Condition cond, T lhs, Label* label)
{
    branchPtr(cond, lhs, getStackPointer(), label);
}

template <typename T> void
MacroAssembler::addToStackPtr(T t)
{
    addPtr(t, getStackPointer());
}

template <typename T> void
MacroAssembler::addStackPtrTo(T t)
{
    addPtr(getStackPointer(), t);
}

#endif // !JS_CODEGEN_ARM64

template <typename T>
void
MacroAssembler::storeObjectOrNull(Register src, const T& dest)
{
    Label notNull, done;
    branchTestPtr(Assembler::NonZero, src, src, &notNull);
    storeValue(NullValue(), dest);
    jump(&done);
    bind(&notNull);
    storeValue(JSVAL_TYPE_OBJECT, src, dest);
    bind(&done);
}

void
MacroAssembler::assertStackAlignment(uint32_t alignment, int32_t offset /* = 0 */)
{
#ifdef DEBUG
    Label ok, bad;
    MOZ_ASSERT(mozilla::IsPowerOfTwo(alignment));

    // Wrap around the offset to be a non-negative number.
    offset %= alignment;
    if (offset < 0)
        offset += alignment;

    // Test if each bit from offset is set.
    uint32_t off = offset;
    while (off) {
        uint32_t lowestBit = 1 << mozilla::CountTrailingZeroes32(off);
        branchTestStackPtr(Assembler::Zero, Imm32(lowestBit), &bad);
        off ^= lowestBit;
    }

    // Check that all remaining bits are zero.
    branchTestStackPtr(Assembler::Zero, Imm32((alignment - 1) ^ offset), &ok);

    bind(&bad);
    breakpoint();
    bind(&ok);
#endif
}

void
MacroAssembler::storeCallResultValue(AnyRegister dest)
{
    unboxValue(JSReturnOperand, dest);
}

void
MacroAssembler::storeCallResultValue(TypedOrValueRegister dest)
{
    if (dest.hasValue())
        storeCallResultValue(dest.valueReg());
    else
        storeCallResultValue(dest.typedReg());
}

} // namespace jit
} // namespace js

#endif /* jit_MacroAssembler_inl_h */
