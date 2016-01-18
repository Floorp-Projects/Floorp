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

enum class Expr : uint8_t
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

    GetLocal,
    SetLocal,

    CallInternal,
    CallIndirect,
    CallImport,

    AtomicsFence,

    I32Expr,
    F32Expr,
    F64Expr,
    I32X4Expr,
    F32X4Expr,
    B32X4Expr,

    // asm.js specific
    Id,
    Noop,

    LoadGlobal,
    StoreGlobal,

    InterruptCheckHead,
    InterruptCheckLoop,

    DebugCheckPoint,
    Bad,

    // I32 opcodes
    I32CallInternal,
    I32CallIndirect,
    I32CallImport,

    I32Conditional,
    I32Comma,

    I32Literal,

    // Binary arith opcodes
    I32Add,
    I32Sub,
    I32Mul,
    I32SDiv,
    I32SMod,
    I32UDiv,
    I32UMod,
    I32Min,
    I32Max,

    // Unary arith opcodes
    I32Not,
    I32Neg,

    // Bitwise opcodes
    I32BitOr,
    I32BitAnd,
    I32BitXor,
    I32BitNot,

    I32Lsh,
    I32ArithRsh,
    I32LogicRsh,

    // Conversion opcodes
    I32FromF32,
    I32FromF64,

    // Math builtin opcodes
    I32Clz,
    I32Abs,

    // Comparison opcodes
    // Ordering matters (EmitComparison expects signed opcodes to be placed
    // before unsigned opcodes)
    I32EqI32,
    I32NeI32,
    I32SLtI32,
    I32SLeI32,
    I32SGtI32,
    I32SGeI32,
    I32ULtI32,
    I32ULeI32,
    I32UGtI32,
    I32UGeI32,

    I32EqF32,
    I32NeF32,
    I32LtF32,
    I32LeF32,
    I32GtF32,
    I32GeF32,

    I32EqF64,
    I32NeF64,
    I32LtF64,
    I32LeF64,
    I32GtF64,
    I32GeF64,

    // Heap accesses opcodes
    I32SLoad8,
    I32SLoad16,
    I32SLoad32,
    I32ULoad8,
    I32ULoad16,
    I32ULoad32,
    I32Store8,
    I32Store16,
    I32Store32,

    // Atomics opcodes
    I32AtomicsCompareExchange,
    I32AtomicsExchange,
    I32AtomicsLoad,
    I32AtomicsStore,
    I32AtomicsBinOp,

    // SIMD opcodes
    I32I32X4ExtractLane,
    I32B32X4ExtractLane,
    I32B32X4AllTrue,
    I32B32X4AnyTrue,

    // Specific to AsmJS
    I32Id,

    // F32 opcdoes
    // Common opcodes
    F32CallInternal,
    F32CallIndirect,
    F32CallImport,

    F32Conditional,
    F32Comma,

    F32Literal,

    // Binary arith opcodes
    F32Add,
    F32Sub,
    F32Mul,
    F32Div,
    F32Min,
    F32Max,
    F32Neg,

    // Math builtin opcodes
    F32Abs,
    F32Sqrt,
    F32Ceil,
    F32Floor,

    // Conversion opcodes
    F32FromF64,
    F32FromS32,
    F32FromU32,

    // Heap accesses opcodes
    F32Load,
    F32StoreF32,
    F32StoreF64,

    // SIMD opcodes
    F32F32X4ExtractLane,

    // asm.js specific
    F32Id,

    // F64 opcodes
    // Common opcodes
    F64CallInternal,
    F64CallIndirect,
    F64CallImport,

    F64Conditional,
    F64Comma,

    F64Literal,

    // Binary arith opcodes
    F64Add,
    F64Sub,
    F64Mul,
    F64Div,
    F64Min,
    F64Max,
    F64Mod,
    F64Neg,

    // Math builtin opcodes
    F64Abs,
    F64Sqrt,
    F64Ceil,
    F64Floor,
    F64Sin,
    F64Cos,
    F64Tan,
    F64Asin,
    F64Acos,
    F64Atan,
    F64Exp,
    F64Log,
    F64Pow,
    F64Atan2,

    // Conversions opcodes
    F64FromF32,
    F64FromS32,
    F64FromU32,

    // Heap accesses opcodes
    F64Load,
    F64StoreF32,
    F64StoreF64,

    // asm.js specific
    F64Id,

    // I32X4 opcodes
    // Common opcodes
    I32X4CallInternal,
    I32X4CallIndirect,
    I32X4CallImport,

    I32X4Conditional,
    I32X4Comma,

    I32X4Literal,

    // Specific opcodes
    I32X4Ctor,

    I32X4Unary,

    I32X4Binary,
    I32X4BinaryBitwise,
    I32X4BinaryShift,

    I32X4ReplaceLane,

    I32X4FromF32X4,
    I32X4FromF32X4Bits,

    I32X4Swizzle,
    I32X4Shuffle,
    I32X4Select,
    I32X4Splat,

    I32X4Load,
    I32X4Store,

    // asm.js specific
    I32X4Id,

    // F32X4 opcodes
    // Common opcodes
    F32X4CallInternal,
    F32X4CallIndirect,
    F32X4CallImport,

    F32X4Conditional,
    F32X4Comma,

    F32X4Literal,

    // Specific opcodes
    F32X4Ctor,

    F32X4Unary,

    F32X4Binary,

    F32X4ReplaceLane,

    F32X4FromI32X4,
    F32X4FromI32X4Bits,
    F32X4Swizzle,
    F32X4Shuffle,
    F32X4Select,
    F32X4Splat,

    F32X4Load,
    F32X4Store,

    // asm.js specific
    F32X4Id,

    // B32X4 opcodes
    // Common opcodes
    B32X4CallInternal,
    B32X4CallIndirect,
    B32X4CallImport,

    B32X4Conditional,
    B32X4Comma,

    B32X4Literal,

    // Specific opcodes
    B32X4Ctor,

    B32X4Unary,

    B32X4Binary,
    B32X4BinaryCompI32X4,
    B32X4BinaryCompF32X4,
    B32X4BinaryBitwise,

    B32X4ReplaceLane,

    B32X4Splat,

    // asm.js specific
    B32X4Id
};

enum NeedsBoundsCheck : uint8_t
{
    NO_BOUNDS_CHECK,
    NEEDS_BOUNDS_CHECK
};

typedef Vector<uint8_t, 0, SystemAllocPolicy> Bytecode;
typedef UniquePtr<Bytecode> UniqueBytecode;

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

    bool init(UniqueBytecode bytecode = UniqueBytecode()) {
        if (bytecode) {
            bytecode_ = mozilla::Move(bytecode);
            bytecode_->clear();
            return true;
        }
        bytecode_ = MakeUnique<Bytecode>();
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
    writeVarU32(uint32_t i) {
        do {
            uint8_t byte = i & 0x7F;
            i >>= 7;
            if (i != 0)
                byte |= 0x80;
            if (!writeU8(byte))
                return false;
        } while(i != 0);
        return true;
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
            patchable &= Expr((*bytecode_)[pc]) == Expr::Bad;
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
    const uint8_t* const beg_;
    const uint8_t* const end_;
    const uint8_t* cur_;

    template<class T>
    MOZ_WARN_UNUSED_RESULT bool
    read(T* out) {
        if (uintptr_t(end_ - cur_) < sizeof(T))
            return false;
        memcpy((void*)out, cur_, sizeof(T));
        cur_ += sizeof(T);
        return true;
    }

    template<class T>
    T uncheckedRead() {
        MOZ_ASSERT(uintptr_t(end_ - cur_) >= sizeof(T));
        T ret;
        memcpy(&ret, cur_, sizeof(T));
        cur_ += sizeof(T);
        return ret;
    }

  public:
    explicit Decoder(const Bytecode& bytecode)
      : beg_(bytecode.begin()),
        end_(bytecode.end()),
        cur_(bytecode.begin())
    {}

    bool done() const {
        MOZ_ASSERT(cur_ <= end_);
        return cur_ == end_;
    }

    void assertCurrentIs(const DebugOnly<size_t> offset) const {
        MOZ_ASSERT(size_t(cur_ - beg_) == offset);
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

    MOZ_WARN_UNUSED_RESULT bool readVarU32(uint32_t* decoded) {
        *decoded = 0;
        uint8_t byte;
        uint32_t shift = 0;
        do {
            if (!readU8(&byte))
                return false;
            if (!(byte & 0x80)) {
                *decoded |= uint32_t(byte & 0x7F) << shift;
                return true;
            }
            *decoded |= uint32_t(byte & 0x7F) << shift;
            shift += 7;
        } while (shift != 28);
        if (!readU8(&byte) || (byte & 0xF0))
            return false;
        *decoded |= uint32_t(byte) << 28;
        return true;
    }

    // The infallible unpacking API should be used when we are sure that the
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

    uint32_t uncheckedReadVarU32() {
        uint32_t decoded = 0;
        uint32_t shift = 0;
        uint8_t byte;
        do {
            byte = uncheckedReadU8();
            if (!(byte & 0x80)) {
                decoded |= uint32_t(byte & 0x7F) << shift;
                return decoded;
            }
            decoded |= uint32_t(byte & 0x7F) << shift;
            shift += 7;
        } while (shift != 28);
        byte = uncheckedReadU8();
        MOZ_ASSERT(!(byte & 0xF0));
        decoded |= uint32_t(byte) << 28;
        return decoded;
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

typedef UniquePtr<FuncBytecode> UniqueFuncBytecode;

} // namespace wasm
} // namespace js

#endif // wasm_binary_h
