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

#ifndef wasm_binary_h
#define wasm_binary_h

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

typedef Vector<uint8_t, 0, SystemAllocPolicy> Bytecode;
typedef UniquePtr<Bytecode, JS::DeletePolicy<Bytecode>> UniqueBytecode;

// The Encoder class recycles (through its constructor) or creates a new Bytecode (through its
// init() method). Its Bytecode is released when it's done building the wasm IR in finish().
class Encoder
{
    UniqueBytecode bytecode_;
    mozilla::DebugOnly<bool> done_;

    template<class T>
    MOZ_WARN_UNUSED_RESULT
    bool write(T v, size_t* offset) {
        if (offset)
            *offset = bytecode_->length();
        return bytecode_->append(reinterpret_cast<uint8_t*>(&v), sizeof(T));
    }

  public:
    Encoder()
      : bytecode_(nullptr),
        done_(false)
    {}

    bool init(UniqueBytecode bytecode) {
        if (bytecode) {
            bytecode_ = mozilla::Move(bytecode);
            bytecode_->clear();
            return true;
        }
        bytecode_.reset(js_new<Bytecode>());
        return !!bytecode_;
    }

    size_t bytecodeOffset() const { return bytecode_->length(); }
    bool empty() const { return bytecodeOffset() == 0; }

    UniqueBytecode finish() {
        MOZ_ASSERT(!done_);
        done_ = true;
        return mozilla::Move(bytecode_);
    }

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
            patchable &= Stmt((*bytecode_)[pc]) == Stmt::Bad;
        return patchable;
    }
#endif

    void patchU8(size_t pc, uint8_t i) {
        MOZ_ASSERT(pcIsPatchable(pc, sizeof(uint8_t)));
        (*bytecode_)[pc] = i;
    }

    template<class T>
    void patch32(size_t pc, T i) {
        static_assert(sizeof(T) == sizeof(uint32_t),
                      "patch32 must be used with 32-bits wide types");
        MOZ_ASSERT(pcIsPatchable(pc, sizeof(uint32_t)));
        memcpy(&(*bytecode_)[pc], &i, sizeof(uint32_t));
    }

    void patchSig(size_t pc, const LifoSig* ptr) {
        MOZ_ASSERT(pcIsPatchable(pc, sizeof(LifoSig*)));
        memcpy(&(*bytecode_)[pc], &ptr, sizeof(LifoSig*));
    }
};

class Decoder
{
    const Bytecode& bytecode_;
    size_t cur_;

    template<class T>
    MOZ_WARN_UNUSED_RESULT
    bool read(T* out) {
        if (uintptr_t(bytecode_.length() - cur_) < sizeof(T))
            return false;
        memcpy((void*)out, &bytecode_[cur_], sizeof(T));
        cur_ += sizeof(T);
        return true;
    }

    template<class T>
    T uncheckedRead() {
        MOZ_ASSERT(uintptr_t(bytecode_.length() - cur_) >= sizeof(T));
        T ret;
        memcpy(&ret, &bytecode_[cur_], sizeof(T));
        cur_ += sizeof(T);
        return ret;
    }

  public:
    explicit Decoder(const Bytecode& bytecode)
      : bytecode_(bytecode),
        cur_(0)
    {}

    bool done() const { return cur_ == bytecode_.length(); }
    void assertCurrentIs(const DebugOnly<size_t> offset) const {
        MOZ_ASSERT(offset == cur_);
    }

    // The fallible unpacking API should be used when we're not assuming
    // anything about the bytecode, in particular if it is well-formed.
    MOZ_WARN_UNUSED_RESULT bool readU8 (uint8_t* i)         { return read(i); }
    MOZ_WARN_UNUSED_RESULT bool readI32(int32_t* i)         { return read(i); }
    MOZ_WARN_UNUSED_RESULT bool readF32(float* f)           { return read(f); }
    MOZ_WARN_UNUSED_RESULT bool readU32(uint32_t* u)        { return read(u); }
    MOZ_WARN_UNUSED_RESULT bool readF64(double* d)          { return read(d); }
    MOZ_WARN_UNUSED_RESULT bool readSig(const LifoSig* sig) { return read(sig); }

    MOZ_WARN_UNUSED_RESULT bool readI32X4(jit::SimdConstant* c) {
        int32_t v[4] = { 0, 0, 0, 0 };
        for (size_t i = 0; i < 4; i++) {
            if (!readI32(&v[i]))
                return false;
        }
        *c = jit::SimdConstant::CreateX4(v[0], v[1], v[2], v[3]);
        return true;
    }
    MOZ_WARN_UNUSED_RESULT bool readF32X4(jit::SimdConstant* c) {
        float v[4] = { 0., 0., 0., 0. };
        for (size_t i = 0; i < 4; i++) {
            if (!readF32(&v[i]))
                return false;
        }
        *c = jit::SimdConstant::CreateX4(v[0], v[1], v[2], v[3]);
        return true;
    }

    // The unfallible unpacking API should be used when we are sure that the
    // bytecode is well-formed.
    uint8_t        uncheckedReadU8 () { return uncheckedRead<uint8_t>(); }
    int32_t        uncheckedReadI32() { return uncheckedRead<int32_t>(); }
    float          uncheckedReadF32() { return uncheckedRead<float>(); }
    uint32_t       uncheckedReadU32() { return uncheckedRead<uint32_t>(); }
    double         uncheckedReadF64() { return uncheckedRead<double>(); }
    const LifoSig* uncheckedReadSig() { return uncheckedRead<const LifoSig*>(); }

    jit::SimdConstant uncheckedReadI32X4() {
        int32_t v[4] = { 0, 0, 0, 0 };
        for (size_t i = 0; i < 4; i++)
            v[i] = uncheckedReadI32();
        return jit::SimdConstant::CreateX4(v[0], v[1], v[2], v[3]);
    }
    jit::SimdConstant uncheckedReadF32X4() {
        float v[4] = { 0., 0., 0., 0. };
        for (size_t i = 0; i < 4; i++)
            v[i] = uncheckedReadF32();
        return jit::SimdConstant::CreateX4(v[0], v[1], v[2], v[3]);
    }
};

// Source coordinates for a call site. As they're read sequentially, we
// don't need to store the call's bytecode offset, unless we want to
// check its correctness in debug mode.
struct SourceCoords {
    DebugOnly<size_t> offset; // after call opcode
    uint32_t line;
    uint32_t column;
};

typedef Vector<SourceCoords, 0, SystemAllocPolicy> SourceCoordsVector;
typedef Vector<ValType, 0, SystemAllocPolicy> ValTypeVector;

// The FuncBytecode class contains the intermediate representation of a
// parsed/decoded and validated asm.js/WebAssembly function. The FuncBytecode
// lives only until it is fully compiled.
class FuncBytecode
{
    // Note: this unrooted field assumes AutoKeepAtoms via TokenStream via
    // asm.js compilation.
    PropertyName* name_;
    unsigned line_;
    unsigned column_;

    SourceCoordsVector callSourceCoords_;

    uint32_t index_;
    const LifoSig& sig_;
    UniqueBytecode bytecode_;
    ValTypeVector localVars_;
    unsigned generateTime_;

  public:
    FuncBytecode(PropertyName* name,
                 unsigned line,
                 unsigned column,
                 SourceCoordsVector&& sourceCoords,
                 uint32_t index,
                 const LifoSig& sig,
                 UniqueBytecode bytecode,
                 ValTypeVector&& localVars,
                 unsigned generateTime)
      : name_(name),
        line_(line),
        column_(column),
        callSourceCoords_(mozilla::Move(sourceCoords)),
        index_(index),
        sig_(sig),
        bytecode_(mozilla::Move(bytecode)),
        localVars_(mozilla::Move(localVars)),
        generateTime_(generateTime)
    {}

    UniqueBytecode recycleBytecode() { return mozilla::Move(bytecode_); }

    PropertyName* name() const { return name_; }
    unsigned line() const { return line_; }
    unsigned column() const { return column_; }
    const SourceCoords& sourceCoords(size_t i) const { return callSourceCoords_[i]; }

    uint32_t index() const { return index_; }
    const LifoSig& sig() const { return sig_; }
    const Bytecode& bytecode() const { return *bytecode_; }

    size_t numLocalVars() const { return localVars_.length(); }
    ValType localVarType(size_t i) const { return localVars_[i]; }
    size_t numLocals() const { return sig_.args().length() + numLocalVars(); }

    unsigned generateTime() const { return generateTime_; }
};

typedef mozilla::UniquePtr<FuncBytecode, JS::DeletePolicy<FuncBytecode>> UniqueFuncBytecode;

} // namespace wasm
} // namespace js

#endif // wasm_binary_h
