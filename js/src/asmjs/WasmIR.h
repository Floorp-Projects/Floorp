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

#ifndef wasm_ir_h
#define wasm_ir_h

#include "asmjs/WasmTypes.h"

namespace js {

class PropertyName;

namespace wasm {

enum class Stmt : uint8_t
{
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
    B32X4Expr,

    Id,
    Noop,
    InterruptCheckHead,
    InterruptCheckLoop,

    DebugCheckPoint,

    Bad
};

enum class I32 : uint8_t
{
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
    I32X4ExtractLane,
    B32X4ExtractLane,
    B32X4AllTrue,
    B32X4AnyTrue,

    // Specific to AsmJS
    Id,

    Bad
};

enum class F32 : uint8_t
{
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

enum class F64 : uint8_t
{
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

enum class I32X4 : uint8_t
{
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
    BinaryShift,

    ReplaceLane,

    FromF32X4,
    FromF32X4Bits,

    Swizzle,
    Shuffle,
    Select,
    Splat,

    Load,
    Store,

    // asm.js specific
    Id,
    Bad
};

enum class F32X4 : uint8_t
{
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

    ReplaceLane,

    FromI32X4,
    FromI32X4Bits,
    Swizzle,
    Shuffle,
    Select,
    Splat,

    Load,
    Store,

    // asm.js specific
    Id,
    Bad
};

enum class B32X4 : uint8_t
{
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

    ReplaceLane,

    Splat,

    // asm.js specific
    Id,
    Bad
};

enum NeedsBoundsCheck : uint8_t
{
    NO_BOUNDS_CHECK,
    NEEDS_BOUNDS_CHECK
};

// The FuncIR class contains the intermediate representation of a parsed/decoded
// and validated asm.js/WebAssembly function. The FuncIR lives only until it
// is fully compiled. Its contents are assumed to be well-formed; all validation
// of untrusted content must happen before FuncIR generation. A FuncIR object is
// associated with a LifoAlloc allocation which contains all the memory
// referenced by the FuncIR.
class FuncIR
{
    typedef Vector<ValType, 4, LifoAllocPolicy<Fallible>> ValTypeVector;

  public:
    // Source coordinates for a call site. As they're read sequentially, we
    // don't need to store the call's bytecode offset, unless we want to
    // check its consistency in debug mode.
    struct SourceCoords {
        DebugOnly<uint32_t> offset; // after call opcode
        uint32_t line;
        uint32_t column;
    };

  private:
    typedef Vector<uint8_t, 4096, LifoAllocPolicy<Fallible>> Bytecode;
    typedef Vector<SourceCoords, 4, LifoAllocPolicy<Fallible>> SourceCoordsVector;

    // Note: this unrooted field assumes AutoKeepAtoms via TokenStream via
    // asm.js compilation.
    PropertyName* name_;
    unsigned line_;
    unsigned column_;
    SourceCoordsVector callSourceCoords_;

    uint32_t index_;
    const LifoSig* sig_;
    ValTypeVector localVars_;
    Bytecode bytecode_;
    unsigned generateTime_;

  public:
    FuncIR(LifoAlloc& alloc, PropertyName* name, unsigned line, unsigned column)
      : name_(name),
        line_(line),
        column_(column),
        callSourceCoords_(alloc),
        index_(UINT_MAX),
        sig_(nullptr),
        localVars_(alloc),
        bytecode_(alloc),
        generateTime_(UINT_MAX)
    {}

    bool addVariable(ValType v) {
        return localVars_.append(v);
    }
    bool addSourceCoords(uint32_t line, uint32_t column) {
        SourceCoords sc = { bytecode_.length(), line, column };
        return callSourceCoords_.append(sc);
    }
    void finish(uint32_t funcIndex, const LifoSig& sig, unsigned generateTime) {
        MOZ_ASSERT(index_ == UINT_MAX);
        MOZ_ASSERT(!sig_);
        MOZ_ASSERT(generateTime_ == UINT_MAX);
        index_ = funcIndex;
        sig_ = &sig;
        generateTime_ = generateTime;
    }

  private:
    template<class T>
    MOZ_WARN_UNUSED_RESULT
    bool write(T v, size_t* offset) {
        if (offset)
            *offset = bytecode_.length();
        return bytecode_.append(reinterpret_cast<uint8_t*>(&v), sizeof(T));
    }

    template<class T>
    MOZ_WARN_UNUSED_RESULT
    bool read(size_t* pc, T* out) const {
        if (size_t(bytecode_.length() - *pc) >= sizeof(T))
            return false;
        memcpy((void*)out, &bytecode_[*pc], sizeof(T));
        *pc += sizeof(T);
        return true;
    }

    template<class T>
    T uncheckedRead(size_t* pc) const {
        MOZ_ASSERT(size_t(bytecode_.length() - *pc) >= sizeof(T));
        T ret;
        memcpy(&ret, &bytecode_[*pc], sizeof(T));
        *pc += sizeof(T);
        return ret;
    }

  public:
    // Packing interface
    MOZ_WARN_UNUSED_RESULT bool
    writeU8(uint8_t i, size_t* offset = nullptr)   { return write<uint8_t>(i, offset); }
    MOZ_WARN_UNUSED_RESULT bool
    writeI32(int32_t i, size_t* offset = nullptr)  { return write<int32_t>(i, offset); }
    MOZ_WARN_UNUSED_RESULT bool
    writeU32(uint32_t i, size_t* offset = nullptr) { return write<uint32_t>(i, offset); }
    MOZ_WARN_UNUSED_RESULT bool
    writeF32(float f, size_t* offset = nullptr)    { return write<float>(f, offset); }
    MOZ_WARN_UNUSED_RESULT bool
    writeF64(double d, size_t* offset = nullptr)   { return write<double>(d, offset); }

    MOZ_WARN_UNUSED_RESULT bool
    writeI32X4(const int32_t* i4, size_t* offset = nullptr) {
        if (!writeI32(i4[0], offset))
            return false;
        for (size_t i = 1; i < 4; i++) {
            if (!writeI32(i4[i]))
                return false;
        }
        return true;
    }
    MOZ_WARN_UNUSED_RESULT bool
    writeF32X4(const float* f4, size_t* offset = nullptr) {
        if (!writeF32(f4[0], offset))
            return false;
        for (size_t i = 1; i < 4; i++) {
            if (!writeF32(f4[i]))
                return false;
        }
        return true;
    }

#ifdef DEBUG
    bool pcIsPatchable(size_t pc, unsigned size) const {
        bool patchable = true;
        for (unsigned i = 0; patchable && i < size; i++)
            patchable &= Stmt(bytecode_[pc]) == Stmt::Bad;
        return patchable;
    }
#endif

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

    void patchSig(size_t pc, const LifoSig* ptr) {
        MOZ_ASSERT(pcIsPatchable(pc, sizeof(LifoSig*)));
        memcpy(&bytecode_[pc], &ptr, sizeof(LifoSig*));
    }

    // The fallible unpacking API should be used when we're not assuming
    // anything about the bytecode, in particular if it is well-formed.
    MOZ_WARN_UNUSED_RESULT bool
    readU8 (size_t* pc, uint8_t* i)         const { return read(pc, i); }
    MOZ_WARN_UNUSED_RESULT bool
    readI32(size_t* pc, int32_t* i)         const { return read(pc, i); }
    MOZ_WARN_UNUSED_RESULT bool
    readF32(size_t* pc, float* f)           const { return read(pc, f); }
    MOZ_WARN_UNUSED_RESULT bool
    readU32(size_t* pc, uint32_t* u)        const { return read(pc, u); }
    MOZ_WARN_UNUSED_RESULT bool
    readF64(size_t* pc, double* d)          const { return read(pc, d); }
    MOZ_WARN_UNUSED_RESULT bool
    readSig(size_t* pc, const LifoSig* sig) const { return read(pc, sig); }

    MOZ_WARN_UNUSED_RESULT bool
    readI32X4(size_t* pc, jit::SimdConstant* c) const {
        int32_t v[4] = { 0, 0, 0, 0 };
        for (size_t i = 0; i < 4; i++) {
            if (!readI32(pc, &v[i]))
                return false;
        }
        *c = jit::SimdConstant::CreateX4(v[0], v[1], v[2], v[3]);
        return true;
    }
    MOZ_WARN_UNUSED_RESULT bool
    readF32X4(size_t* pc, jit::SimdConstant* c) const {
        float v[4] = { 0., 0., 0., 0. };
        for (size_t i = 0; i < 4; i++) {
            if (!readF32(pc, &v[i]))
                return false;
        }
        *c = jit::SimdConstant::CreateX4(v[0], v[1], v[2], v[3]);
        return true;
    }

    // The unfallible unpacking API should be used when we are sure that the
    // bytecode is well-formed.
    uint8_t        uncheckedReadU8 (size_t* pc) const { return uncheckedRead<uint8_t>(pc); }
    int32_t        uncheckedReadI32(size_t* pc) const { return uncheckedRead<int32_t>(pc); }
    float          uncheckedReadF32(size_t* pc) const { return uncheckedRead<float>(pc); }
    uint32_t       uncheckedReadU32(size_t* pc) const { return uncheckedRead<uint32_t>(pc); }
    double         uncheckedReadF64(size_t* pc) const { return uncheckedRead<double>(pc); }
    const LifoSig* uncheckedReadSig(size_t* pc) const { return uncheckedRead<const LifoSig*>(pc); }

    jit::SimdConstant uncheckedReadI32X4(size_t* pc) const {
        int32_t v[4] = { 0, 0, 0, 0 };
        for (size_t i = 0; i < 4; i++)
            v[i] = uncheckedReadI32(pc);
        return jit::SimdConstant::CreateX4(v[0], v[1], v[2], v[3]);
    }
    jit::SimdConstant uncheckedReadF32X4(size_t* pc) const {
        float v[4] = { 0., 0., 0., 0. };
        for (size_t i = 0; i < 4; i++)
            v[i] = uncheckedReadF32(pc);
        return jit::SimdConstant::CreateX4(v[0], v[1], v[2], v[3]);
    }

    // Read-only interface
    PropertyName* name() const { return name_; }
    unsigned line() const { return line_; }
    unsigned column() const { return column_; }
    const SourceCoords& sourceCoords(size_t i) const { return callSourceCoords_[i]; }

    uint32_t index() const { MOZ_ASSERT(index_ != UINT32_MAX); return index_; }
    const LifoSig& sig() const { MOZ_ASSERT(sig_); return *sig_; }

    size_t numLocalVars() const { return localVars_.length(); }
    ValType localVarType(size_t i) const { return localVars_[i]; }
    size_t numLocals() const { return sig_->args().length() + numLocalVars(); }

    unsigned generateTime() const { MOZ_ASSERT(generateTime_ != UINT_MAX); return generateTime_; }

    size_t size() const { return bytecode_.length(); }
};

} // namespace wasm
} // namespace js

#endif // wasm_ir_h
