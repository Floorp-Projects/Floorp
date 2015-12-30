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
    typedef Vector<uint8_t, 4096, LifoAllocPolicy<Fallible>> Bytecode;

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
    typedef Vector<SourceCoords, 4, LifoAllocPolicy<Fallible>> SourceCoordsVector;

    // Note: this unrooted field assumes AutoKeepAtoms via TokenStream via
    // asm.js compilation.
    PropertyName* name_;
    unsigned line_;
    unsigned column_;

    uint32_t index_;
    const LifoSig* sig_;
    ValTypeVector localVars_;
    SourceCoordsVector callSourceCoords_;
    Bytecode bytecode_;
    unsigned generateTime_;

  public:
    FuncIR(LifoAlloc& alloc, PropertyName* name, unsigned line, unsigned column)
      : name_(name),
        line_(line),
        column_(column),
        index_(UINT_MAX),
        sig_(nullptr),
        localVars_(alloc),
        callSourceCoords_(alloc),
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

    uint8_t        readU8 (size_t* pc) const { return readPrimitive<uint8_t>(pc); }
    int32_t        readI32(size_t* pc) const { return readPrimitive<int32_t>(pc); }
    float          readF32(size_t* pc) const { return readPrimitive<float>(pc); }
    uint32_t       readU32(size_t* pc) const { return readPrimitive<uint32_t>(pc); }
    double         readF64(size_t* pc) const { return readPrimitive<double>(pc); }
    const LifoSig* readSig(size_t* pc) const { return readPrimitive<LifoSig*>(pc); }

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

    // Read-only interface
    PropertyName* name() const { return name_; }
    unsigned line() const { return line_; }
    unsigned column() const { return column_; }
    uint32_t index() const { MOZ_ASSERT(index_ != UINT32_MAX); return index_; }
    size_t size() const { return bytecode_.length(); }
    const LifoSig& sig() const { MOZ_ASSERT(sig_); return *sig_; }
    size_t numLocalVars() const { return localVars_.length(); }
    ValType localVarType(size_t i) const { return localVars_[i]; }
    size_t numLocals() const { return sig_->args().length() + numLocalVars(); }
    unsigned generateTime() const { MOZ_ASSERT(generateTime_ != UINT_MAX); return generateTime_; }
    const SourceCoords& sourceCoords(size_t i) const { return callSourceCoords_[i]; }
};

} // namespace wasm
} // namespace js

#endif // wasm_ir_h
