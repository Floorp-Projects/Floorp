/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
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

#ifndef jit_AsmJSCompile_h
#define jit_AsmJSCompile_h

#include "asmjs/AsmJSCompileInputs.h"
#include "asmjs/AsmJSModule.h"

namespace js {

namespace jit {
    class LIRGraph;
    class MIRGenerator;
}

namespace wasm {

enum NeedsBoundsCheck {
    NO_BOUNDS_CHECK,
    NEEDS_BOUNDS_CHECK
};

// Respresents the type of a general asm.js expression.
class Type
{
  public:
    enum Which {
        Fixnum = AsmJSNumLit::Fixnum,
        Signed = AsmJSNumLit::NegativeInt,
        Unsigned = AsmJSNumLit::BigUnsigned,
        DoubleLit = AsmJSNumLit::Double,
        Float = AsmJSNumLit::Float,
        Int32x4 = AsmJSNumLit::Int32x4,
        Float32x4 = AsmJSNumLit::Float32x4,
        Double,
        MaybeDouble,
        MaybeFloat,
        Floatish,
        Int,
        Intish,
        Void
    };

  private:
    Which which_;

  public:
    Type() : which_(Which(-1)) {}
    static Type Of(const AsmJSNumLit& lit) {
        MOZ_ASSERT(lit.hasType());
        Which which = Type::Which(lit.which());
        MOZ_ASSERT(which >= Fixnum && which <= Float32x4);
        Type t;
        t.which_ = which;
        return t;
    }
    MOZ_IMPLICIT Type(Which w) : which_(w) {}
    Which which() const { return which_; }
    MOZ_IMPLICIT Type(AsmJSSimdType type) {
        switch (type) {
          case AsmJSSimdType_int32x4:
            which_ = Int32x4;
            return;
          case AsmJSSimdType_float32x4:
            which_ = Float32x4;
            return;
        }
        MOZ_CRASH("unexpected AsmJSSimdType");
    }

    bool operator==(Type rhs) const { return which_ == rhs.which_; }
    bool operator!=(Type rhs) const { return which_ != rhs.which_; }

    inline bool operator<=(Type rhs) const {
        switch (rhs.which_) {
          case Signed:      return isSigned();
          case Unsigned:    return isUnsigned();
          case DoubleLit:   return isDoubleLit();
          case Double:      return isDouble();
          case Float:       return isFloat();
          case Int32x4:     return isInt32x4();
          case Float32x4:   return isFloat32x4();
          case MaybeDouble: return isMaybeDouble();
          case MaybeFloat:  return isMaybeFloat();
          case Floatish:    return isFloatish();
          case Int:         return isInt();
          case Intish:      return isIntish();
          case Fixnum:      return isFixnum();
          case Void:        return isVoid();
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected this type");
    }

    bool isFixnum() const {
        return which_ == Fixnum;
    }

    bool isSigned() const {
        return which_ == Signed || which_ == Fixnum;
    }

    bool isUnsigned() const {
        return which_ == Unsigned || which_ == Fixnum;
    }

    bool isInt() const {
        return isSigned() || isUnsigned() || which_ == Int;
    }

    bool isIntish() const {
        return isInt() || which_ == Intish;
    }

    bool isDoubleLit() const {
        return which_ == DoubleLit;
    }

    bool isDouble() const {
        return isDoubleLit() || which_ == Double;
    }

    bool isMaybeDouble() const {
        return isDouble() || which_ == MaybeDouble;
    }

    bool isFloat() const {
        return which_ == Float;
    }

    bool isMaybeFloat() const {
        return isFloat() || which_ == MaybeFloat;
    }

    bool isFloatish() const {
        return isMaybeFloat() || which_ == Floatish;
    }

    bool isVoid() const {
        return which_ == Void;
    }

    bool isExtern() const {
        return isDouble() || isSigned();
    }

    bool isInt32x4() const {
        return which_ == Int32x4;
    }

    bool isFloat32x4() const {
        return which_ == Float32x4;
    }

    bool isSimd() const {
        return isInt32x4() || isFloat32x4();
    }

    bool isVarType() const {
        return isInt() || isDouble() || isFloat() || isSimd();
    }

    jit::MIRType toMIRType() const {
        switch (which_) {
          case Double:
          case DoubleLit:
          case MaybeDouble:
            return jit::MIRType_Double;
          case Float:
          case Floatish:
          case MaybeFloat:
            return jit::MIRType_Float32;
          case Fixnum:
          case Int:
          case Signed:
          case Unsigned:
          case Intish:
            return jit::MIRType_Int32;
          case Int32x4:
            return jit::MIRType_Int32x4;
          case Float32x4:
            return jit::MIRType_Float32x4;
          case Void:
            return jit::MIRType_None;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Invalid Type");
    }

    AsmJSSimdType simdType() const {
        MOZ_ASSERT(isSimd());
        switch (which_) {
          case Int32x4:
            return AsmJSSimdType_int32x4;
          case Float32x4:
            return AsmJSSimdType_float32x4;
          // Scalar types
          case Double:
          case DoubleLit:
          case MaybeDouble:
          case Float:
          case MaybeFloat:
          case Floatish:
          case Fixnum:
          case Int:
          case Signed:
          case Unsigned:
          case Intish:
          case Void:
            break;
        }
        MOZ_CRASH("not a SIMD Type");
    }

    const char* toChars() const {
        switch (which_) {
          case Double:      return "double";
          case DoubleLit:   return "doublelit";
          case MaybeDouble: return "double?";
          case Float:       return "float";
          case Floatish:    return "floatish";
          case MaybeFloat:  return "float?";
          case Fixnum:      return "fixnum";
          case Int:         return "int";
          case Signed:      return "signed";
          case Unsigned:    return "unsigned";
          case Intish:      return "intish";
          case Int32x4:     return "int32x4";
          case Float32x4:   return "float32x4";
          case Void:        return "void";
        }
        MOZ_CRASH("Invalid Type");
    }
};

// Represents the subset of Type that can be used as a variable or
// argument's type. Note: AsmJSCoercion and VarType are kept separate to
// make very clear the signed/int distinction: a coercion may explicitly sign
// an *expression* but, when stored as a variable, this signedness information
// is explicitly thrown away by the asm.js type system. E.g., in
//
//   function f(i) {
//     i = i | 0;             (1)
//     if (...)
//         i = foo() >>> 0;
//     else
//         i = bar() | 0;
//     return i | 0;          (2)
//   }
//
// the AsmJSCoercion of (1) is Signed (since | performs ToInt32) but, when
// translated to a VarType, the result is a plain Int since, as shown, it
// is legal to assign both Signed and Unsigned (or some other Int) values to
// it. For (2), the AsmJSCoercion is also Signed but, when translated to an
// RetType, the result is Signed since callers (asm.js and non-asm.js) can
// rely on the return value being Signed.
class VarType
{
  public:
    enum Which {
        Int = Type::Int,
        Double = Type::Double,
        Float = Type::Float,
        Int32x4 = Type::Int32x4,
        Float32x4 = Type::Float32x4
    };

  private:
    Which which_;

  public:
    VarType()
      : which_(Which(-1)) {}
    MOZ_IMPLICIT VarType(Which w)
      : which_(w) {}
    MOZ_IMPLICIT VarType(AsmJSCoercion coercion) {
        switch (coercion) {
          case AsmJS_ToInt32: which_ = Int; break;
          case AsmJS_ToNumber: which_ = Double; break;
          case AsmJS_FRound: which_ = Float; break;
          case AsmJS_ToInt32x4: which_ = Int32x4; break;
          case AsmJS_ToFloat32x4: which_ = Float32x4; break;
        }
    }
    static VarType Of(const AsmJSNumLit& lit) {
        MOZ_ASSERT(lit.hasType());
        switch (lit.which()) {
          case AsmJSNumLit::Fixnum:
          case AsmJSNumLit::NegativeInt:
          case AsmJSNumLit::BigUnsigned:
            return Int;
          case AsmJSNumLit::Double:
            return Double;
          case AsmJSNumLit::Float:
            return Float;
          case AsmJSNumLit::Int32x4:
            return Int32x4;
          case AsmJSNumLit::Float32x4:
            return Float32x4;
          case AsmJSNumLit::OutOfRangeInt:
            MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("can't be out of range int");
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected literal type");
    }

    Which which() const {
        return which_;
    }
    Type toType() const {
        return Type::Which(which_);
    }
    jit::MIRType toMIRType() const {
        switch(which_) {
          case Int:       return jit::MIRType_Int32;
          case Double:    return jit::MIRType_Double;
          case Float:     return jit::MIRType_Float32;
          case Int32x4:   return jit::MIRType_Int32x4;
          case Float32x4: return jit::MIRType_Float32x4;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("VarType can only be Int, SIMD, Double or Float");
    }
    AsmJSCoercion toCoercion() const {
        switch(which_) {
          case Int:       return AsmJS_ToInt32;
          case Double:    return AsmJS_ToNumber;
          case Float:     return AsmJS_FRound;
          case Int32x4:   return AsmJS_ToInt32x4;
          case Float32x4: return AsmJS_ToFloat32x4;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("VarType can only be Int, SIMD, Double or Float");
    }
    static VarType FromCheckedType(Type type) {
        MOZ_ASSERT(type.isInt() || type.isMaybeDouble() || type.isFloatish() || type.isSimd());
        if (type.isMaybeDouble())
            return Double;
        else if (type.isFloatish())
            return Float;
        else if (type.isInt())
            return Int;
        else if (type.isInt32x4())
            return Int32x4;
        else if (type.isFloat32x4())
            return Float32x4;
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unknown type in FromCheckedType");
    }
    bool operator==(VarType rhs) const { return which_ == rhs.which_; }
    bool operator!=(VarType rhs) const { return which_ != rhs.which_; }
};

// Represents the subset of Type that can be used as the return type of a
// function.
class RetType
{
  public:
    enum Which {
        Void      = Type::Void,
        Signed    = Type::Signed,
        Double    = Type::Double,
        Float     = Type::Float,
        Int32x4   = Type::Int32x4,
        Float32x4 = Type::Float32x4
    };

  private:
    Which which_;

  public:
    RetType() : which_(Which(-1)) {}
    MOZ_IMPLICIT RetType(Which w) : which_(w) {}
    MOZ_IMPLICIT RetType(AsmJSCoercion coercion) {
        which_ = Which(-1);  // initialize to silence GCC warning
        switch (coercion) {
          case AsmJS_ToInt32:     which_ = Signed;    break;
          case AsmJS_ToNumber:    which_ = Double;    break;
          case AsmJS_FRound:      which_ = Float;     break;
          case AsmJS_ToInt32x4:   which_ = Int32x4;   break;
          case AsmJS_ToFloat32x4: which_ = Float32x4; break;
        }
    }
    Which which() const {
        return which_;
    }
    Type toType() const {
        return Type::Which(which_);
    }
    AsmJSModule::ReturnType toModuleReturnType() const {
        switch (which_) {
          case Void:      return AsmJSModule:: Return_Void;
          case Signed:    return AsmJSModule:: Return_Int32;
          case Float: // will be converted to a Double
          case Double:    return AsmJSModule:: Return_Double;
          case Int32x4:   return AsmJSModule:: Return_Int32x4;
          case Float32x4: return AsmJSModule:: Return_Float32x4;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected return type");
    }
    jit::MIRType toMIRType() const {
        switch (which_) {
          case Void:      return jit::MIRType_None;
          case Signed:    return jit::MIRType_Int32;
          case Double:    return jit::MIRType_Double;
          case Float:     return jit::MIRType_Float32;
          case Int32x4:   return jit::MIRType_Int32x4;
          case Float32x4: return jit::MIRType_Float32x4;
        }
        MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected return type");
    }
    bool operator==(RetType rhs) const { return which_ == rhs.which_; }
    bool operator!=(RetType rhs) const { return which_ != rhs.which_; }
};

inline jit::MIRType ToMIRType(jit::MIRType t) { return t; }
inline jit::MIRType ToMIRType(VarType t) { return t.toMIRType(); }

template <class VecT>
class ABIArgIter
{
    jit::ABIArgGenerator gen_;
    const VecT& types_;
    unsigned i_;

    void settle() { if (!done()) gen_.next(ToMIRType(types_[i_])); }

  public:
    explicit ABIArgIter(const VecT& types) : types_(types), i_(0) { settle(); }
    void operator++(int) { MOZ_ASSERT(!done()); i_++; settle(); }
    bool done() const { return i_ == types_.length(); }

    jit::ABIArg* operator->() { MOZ_ASSERT(!done()); return &gen_.current(); }
    jit::ABIArg& operator*() { MOZ_ASSERT(!done()); return gen_.current(); }

    unsigned index() const { MOZ_ASSERT(!done()); return i_; }
    jit::MIRType mirType() const { MOZ_ASSERT(!done()); return ToMIRType(types_[i_]); }
    uint32_t stackBytesConsumedSoFar() const { return gen_.stackBytesConsumedSoFar(); }
};

typedef Vector<jit::MIRType, 8> MIRTypeVector;
typedef ABIArgIter<MIRTypeVector> ABIArgMIRTypeIter;

typedef Vector<VarType, 8, LifoAllocPolicy<Fallible>> VarTypeVector;
typedef ABIArgIter<VarTypeVector> ABIArgTypeIter;

class Signature
{
    VarTypeVector argTypes_;
    RetType retType_;

  public:
    explicit Signature(LifoAlloc& alloc)
      : argTypes_(alloc) {}
    Signature(LifoAlloc& alloc, RetType retType)
      : argTypes_(alloc), retType_(retType) {}
    Signature(VarTypeVector&& argTypes, RetType retType)
      : argTypes_(Move(argTypes)), retType_(Move(retType)) {}
    Signature(Signature&& rhs)
      : argTypes_(Move(rhs.argTypes_)), retType_(Move(rhs.retType_)) {}

    bool copy(const Signature& rhs) {
        if (!argTypes_.resize(rhs.argTypes_.length()))
            return false;
        for (unsigned i = 0; i < argTypes_.length(); i++)
            argTypes_[i] = rhs.argTypes_[i];
        retType_ = rhs.retType_;
        return true;
    }

    bool appendArg(VarType type) { return argTypes_.append(type); }
    VarType arg(unsigned i) const { return argTypes_[i]; }
    const VarTypeVector& args() const { return argTypes_; }
    VarTypeVector&& extractArgs() { return Move(argTypes_); }

    RetType retType() const { return retType_; }
};

// Signature that can be only allocated with a LifoAlloc.
class LifoSignature : public Signature
{
    explicit LifoSignature(Signature&& rhs)
      : Signature(Move(rhs))
    {}

    LifoSignature(const LifoSignature&) = delete;
    LifoSignature(const LifoSignature&&) = delete;
    LifoSignature& operator=(const LifoSignature&) = delete;
    LifoSignature& operator=(const LifoSignature&&) = delete;

  public:
    static LifoSignature* new_(LifoAlloc& lifo, Signature&& sig) {
        void* mem = lifo.alloc(sizeof(LifoSignature));
        if (!mem)
            return nullptr;
        return new (mem) LifoSignature(Move(sig));
    }
};

enum class Stmt : uint8_t {
    Ret,

    Block,

    IfThen,
    IfElse,
    Switch,

    While,
    DoWhile,

    ForInitInc,
    ForInitNoInc,
    ForNoInitNoInc,
    ForNoInitInc,

    Label,
    Continue,
    ContinueLabel,
    Break,
    BreakLabel,

    CallInternal,
    CallIndirect,
    CallImport,

    AtomicsFence,

    // asm.js specific
    // Expression statements (to be removed in the future)
    I32Expr,
    F32Expr,
    F64Expr,
    I32X4Expr,
    F32X4Expr,

    Id,
    Noop,
    InterruptCheckHead,
    InterruptCheckLoop,

    DebugCheckPoint,

    Bad
};

enum class I32 : uint8_t {
    // Common opcodes
    GetLocal,
    SetLocal,
    GetGlobal,
    SetGlobal,

    CallInternal,
    CallIndirect,
    CallImport,

    Conditional,
    Comma,

    Literal,

    // Binary arith opcodes
    Add,
    Sub,
    Mul,
    SDiv,
    SMod,
    UDiv,
    UMod,
    Min,
    Max,

    // Unary arith opcodes
    Not,
    Neg,

    // Bitwise opcodes
    BitOr,
    BitAnd,
    BitXor,
    BitNot,

    Lsh,
    ArithRsh,
    LogicRsh,

    // Conversion opcodes
    FromF32,
    FromF64,

    // Math builtin opcodes
    Clz,
    Abs,

    // Comparison opcodes
    // Ordering matters (EmitComparison expects signed opcodes to be placed
    // before unsigned opcodes)
    EqI32,
    NeI32,
    SLtI32,
    SLeI32,
    SGtI32,
    SGeI32,
    ULtI32,
    ULeI32,
    UGtI32,
    UGeI32,

    EqF32,
    NeF32,
    LtF32,
    LeF32,
    GtF32,
    GeF32,

    EqF64,
    NeF64,
    LtF64,
    LeF64,
    GtF64,
    GeF64,

    // Heap accesses opcodes
    SLoad8,
    SLoad16,
    SLoad32,
    ULoad8,
    ULoad16,
    ULoad32,
    Store8,
    Store16,
    Store32,

    // Atomics opcodes
    AtomicsCompareExchange,
    AtomicsExchange,
    AtomicsLoad,
    AtomicsStore,
    AtomicsBinOp,

    // SIMD opcodes
    I32X4SignMask,
    F32X4SignMask,

    I32X4ExtractLane,

    // Specific to AsmJS
    Id,

    Bad
};

enum class F32 : uint8_t {
    // Common opcodes
    GetLocal,
    SetLocal,
    GetGlobal,
    SetGlobal,

    CallInternal,
    CallIndirect,
    CallImport,

    Conditional,
    Comma,

    Literal,

    // Binary arith opcodes
    Add,
    Sub,
    Mul,
    Div,
    Min,
    Max,
    Neg,

    // Math builtin opcodes
    Abs,
    Sqrt,
    Ceil,
    Floor,

    // Conversion opcodes
    FromF64,
    FromS32,
    FromU32,

    // Heap accesses opcodes
    Load,
    StoreF32,
    StoreF64,

    // SIMD opcodes
    F32X4ExtractLane,

    // asm.js specific
    Id,
    Bad
};

enum class F64 : uint8_t {
    // Common opcodes
    GetLocal,
    SetLocal,
    GetGlobal,
    SetGlobal,

    CallInternal,
    CallIndirect,
    CallImport,

    Conditional,
    Comma,

    Literal,

    // Binary arith opcodes
    Add,
    Sub,
    Mul,
    Div,
    Min,
    Max,
    Mod,
    Neg,

    // Math builtin opcodes
    Abs,
    Sqrt,
    Ceil,
    Floor,
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Exp,
    Log,
    Pow,
    Atan2,

    // Conversions opcodes
    FromF32,
    FromS32,
    FromU32,

    // Heap accesses opcodes
    Load,
    StoreF32,
    StoreF64,

    // asm.js specific
    Id,
    Bad
};

enum class I32X4 : uint8_t {
    // Common opcodes
    GetLocal,
    SetLocal,

    GetGlobal,
    SetGlobal,

    CallInternal,
    CallIndirect,
    CallImport,

    Conditional,
    Comma,

    Literal,

    // Specific opcodes
    Ctor,

    Unary,

    Binary,
    BinaryCompI32X4,
    BinaryCompF32X4,
    BinaryBitwise,
    BinaryShift,

    ReplaceLane,

    FromF32X4,
    FromF32X4Bits,

    Swizzle,
    Shuffle,
    Select,
    BitSelect,
    Splat,

    Load,
    Store,

    // asm.js specific
    Id,
    Bad
};

enum class F32X4 : uint8_t {
    // Common opcodes
    GetLocal,
    SetLocal,

    GetGlobal,
    SetGlobal,

    CallInternal,
    CallIndirect,
    CallImport,

    Conditional,
    Comma,

    Literal,

    // Specific opcodes
    Ctor,

    Unary,

    Binary,
    BinaryBitwise,

    ReplaceLane,

    FromI32X4,
    FromI32X4Bits,
    Swizzle,
    Shuffle,
    Select,
    BitSelect,
    Splat,

    Load,
    Store,

    // asm.js specific
    Id,
    Bad
};

} // namespace wasm

class AsmFunction
{
  public:
    typedef Vector<AsmJSNumLit, 8, LifoAllocPolicy<Fallible>> VarInitializerVector;

  private:
    typedef Vector<uint8_t, 4096, LifoAllocPolicy<Fallible>> Bytecode;

    VarInitializerVector varInitializers_;
    Bytecode bytecode_;

    wasm::VarTypeVector argTypes_;
    wasm::RetType returnedType_;

    PropertyName* name_;

    unsigned funcIndex_;
    unsigned srcBegin_;
    unsigned lineno_;
    unsigned column_;
    unsigned parseTime_;

  public:
    explicit AsmFunction(LifoAlloc& alloc)
      : varInitializers_(alloc),
        bytecode_(alloc),
        argTypes_(alloc),
        returnedType_(wasm::RetType::Which(-1)),
        name_(nullptr),
        funcIndex_(-1),
        srcBegin_(-1),
        lineno_(-1),
        column_(-1),
        parseTime_(-1)
    {}

    bool init(const wasm::VarTypeVector& args) {
        if (!argTypes_.initCapacity(args.length()))
            return false;
        for (size_t i = 0; i < args.length(); i++)
            argTypes_.append(args[i]);
        return true;
    }

    bool finish(const wasm::VarTypeVector& args, PropertyName* name, unsigned funcIndex,
                unsigned srcBegin, unsigned lineno, unsigned column, unsigned parseTime)
    {
        if (!argTypes_.initCapacity(args.length()))
            return false;
        for (size_t i = 0; i < args.length(); i++)
            argTypes_.infallibleAppend(args[i]);

        MOZ_ASSERT(name_ == nullptr);
        name_ = name;

        MOZ_ASSERT(funcIndex_ == unsigned(-1));
        funcIndex_ = funcIndex;

        MOZ_ASSERT(srcBegin_ == unsigned(-1));
        srcBegin_ = srcBegin;

        MOZ_ASSERT(lineno_ == unsigned(-1));
        lineno_ = lineno;

        MOZ_ASSERT(column_ == unsigned(-1));
        column_ = column;

        MOZ_ASSERT(parseTime_ == unsigned(-1));
        parseTime_ = parseTime;
        return true;
    }

  private:
    AsmFunction(const AsmFunction&) = delete;
    AsmFunction(AsmFunction&& other) = delete;
    AsmFunction& operator=(const AsmFunction&) = delete;

    // Helper functions
    template<class T> size_t writePrimitive(T v) {
        size_t writeAt = bytecode_.length();
        if (!bytecode_.append(reinterpret_cast<uint8_t*>(&v), sizeof(T)))
            return -1;
        return writeAt;
    }

    template<class T> T readPrimitive(size_t* pc) const {
        MOZ_ASSERT(*pc + sizeof(T) <= bytecode_.length());
        T ret;
        memcpy(&ret, &bytecode_[*pc], sizeof(T));
        *pc += sizeof(T);
        return ret;
    }

  public:
    size_t writeU8(uint8_t i)   { return writePrimitive<uint8_t>(i); }
    size_t writeI32(int32_t i)  { return writePrimitive<int32_t>(i); }
    size_t writeU32(uint32_t i) { return writePrimitive<uint32_t>(i); }
    size_t writeF32(float f)    { return writePrimitive<float>(f); }
    size_t writeF64(double d)   { return writePrimitive<double>(d); }

    size_t writeI32X4(const int32_t* i4) {
        size_t pos = bytecode_.length();
        for (size_t i = 0; i < 4; i++)
            writePrimitive<int32_t>(i4[i]);
        return pos;
    }
    size_t writeF32X4(const float* f4) {
        size_t pos = bytecode_.length();
        for (size_t i = 0; i < 4; i++)
            writePrimitive<float>(f4[i]);
        return pos;
    }

    uint8_t  readU8 (size_t* pc) const { return readPrimitive<uint8_t>(pc); }
    int32_t  readI32(size_t* pc) const { return readPrimitive<int32_t>(pc); }
    float    readF32(size_t* pc) const { return readPrimitive<float>(pc); }
    uint32_t readU32(size_t* pc) const { return readPrimitive<uint32_t>(pc); }
    double   readF64(size_t* pc) const { return readPrimitive<double>(pc); }
    wasm::LifoSignature* readSignature(size_t* pc) const { return readPrimitive<wasm::LifoSignature*>(pc); }

    jit::SimdConstant readI32X4(size_t* pc) const {
        int32_t x = readI32(pc);
        int32_t y = readI32(pc);
        int32_t z = readI32(pc);
        int32_t w = readI32(pc);
        return jit::SimdConstant::CreateX4(x, y, z, w);
    }
    jit::SimdConstant readF32X4(size_t* pc) const {
        float x = readF32(pc);
        float y = readF32(pc);
        float z = readF32(pc);
        float w = readF32(pc);
        return jit::SimdConstant::CreateX4(x, y, z, w);
    }

#ifdef DEBUG
    bool pcIsPatchable(size_t pc, unsigned size) const {
        bool patchable = true;
        for (unsigned i = 0; patchable && i < size; i++)
            patchable &= wasm::Stmt(bytecode_[pc]) == wasm::Stmt::Bad;
        return patchable;
    }
#endif // DEBUG

    void patchU8(size_t pc, uint8_t i) {
        MOZ_ASSERT(pcIsPatchable(pc, sizeof(uint8_t)));
        bytecode_[pc] = i;
    }

    template<class T>
    void patch32(size_t pc, T i) {
        static_assert(sizeof(T) == sizeof(uint32_t),
                      "patch32 must be used with 32-bits wide types");
        MOZ_ASSERT(pcIsPatchable(pc, sizeof(uint32_t)));
        memcpy(&bytecode_[pc], &i, sizeof(uint32_t));
    }

    void patchSignature(size_t pc, const wasm::LifoSignature* ptr) {
        MOZ_ASSERT(pcIsPatchable(pc, sizeof(wasm::LifoSignature*)));
        memcpy(&bytecode_[pc], &ptr, sizeof(wasm::LifoSignature*));
    }

    // Setters
    bool addVariable(const AsmJSNumLit& init) {
        return varInitializers_.append(init);
    }
    void setReturnedType(wasm::RetType retType) {
        MOZ_ASSERT(returnedType_ == wasm::RetType::Which(-1));
        returnedType_ = retType;
    }

    // Read-only interface
    PropertyName* name() const { return name_; }
    unsigned funcIndex() const { return funcIndex_; }
    unsigned srcBegin() const { return srcBegin_; }
    unsigned lineno() const { return lineno_; }
    unsigned column() const { return column_; }
    unsigned parseTime() const { return parseTime_; }

    size_t size() const { return bytecode_.length(); }

    const wasm::VarTypeVector& argTypes() const { return argTypes_; }

    const VarInitializerVector& varInitializers() const { return varInitializers_; }
    size_t numLocals() const { return argTypes_.length() + varInitializers_.length(); }
    wasm::RetType returnedType() const {
        MOZ_ASSERT(returnedType_ != wasm::RetType::Which(-1));
        return returnedType_;
    }
};

class FunctionCompileResults
{
    jit::MacroAssembler masm_;

    jit::IonScriptCounts* counts_;
    AsmJSModule::FunctionCodeRange codeRange_;
    unsigned compileTime_;

  public:
    FunctionCompileResults()
      : masm_(jit::MacroAssembler::AsmJSToken()),
        counts_(nullptr),
        codeRange_(),
        compileTime_(-1)
    {}

    const jit::MacroAssembler& masm() const { return masm_; }
    jit::MacroAssembler& masm() { return masm_; }

    const AsmJSModule::FunctionCodeRange& codeRange() const { return codeRange_; }
    jit::IonScriptCounts* counts() const { return counts_; }

    void finishCodegen(AsmJSModule::FunctionCodeRange codeRange, jit::IonScriptCounts& counts) {
        codeRange_ = codeRange;
        counts_ = &counts;
    }
    void setCompileTime(unsigned ms) {
        MOZ_ASSERT(compileTime_ == unsigned(-1));
        compileTime_ = ms;
    }
    unsigned compileTime() const {
        return compileTime_;
    }
};

struct AsmJSParallelTask
{
    mozilla::Maybe<FunctionCompileResults> results;
    LifoAlloc lifo;                         // Provider of all heap memory used for compilation.
    ModuleCompileInputs inputs;
    JSRuntime* runtime;                     // Associated runtime.
    AsmFunction* func;

    AsmJSParallelTask(size_t defaultChunkSize, js::ModuleCompileInputs inputs)
      : results(),
        lifo(defaultChunkSize),
        inputs(inputs),
        runtime(nullptr),
        func(nullptr)
    { }

    void init(JSRuntime* rt, js::AsmFunction* func) {
        this->runtime = rt;
        this->func = func;
        results.emplace();
    }
};

bool CompileAsmFunction(LifoAlloc& lifo, ModuleCompileInputs inputs, const AsmFunction& func,
                        FunctionCompileResults* results);

} // namespace js

#endif //jit_AsmJSCompile_h
