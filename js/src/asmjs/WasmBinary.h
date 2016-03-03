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

#include "jsstr.h"

#include "asmjs/WasmTypes.h"
#include "builtin/SIMD.h"

namespace js {

class PropertyName;

namespace wasm {

// Module generator limits
static const unsigned MaxSigs         =   4 * 1024;
static const unsigned MaxFuncs        = 512 * 1024;
static const unsigned MaxImports      =   4 * 1024;
static const unsigned MaxExports      =   4 * 1024;
static const unsigned MaxTableElems   = 128 * 1024;
static const unsigned MaxArgsPerFunc  =   4 * 1024;

// Module header constants
static const uint32_t MagicNumber     = 0x6d736100; // "\0asm"
static const uint32_t EncodingVersion = -1;     // experimental

// Names:
static const char SigLabel[]          = "sig";
static const char ImportLabel[]       = "import";
static const char DeclLabel[]         = "decl";
static const char TableLabel[]        = "table";
static const char MemoryLabel[]       = "memory";
static const char ExportLabel[]       = "export";
static const char FuncLabel[]         = "func";
static const char DataLabel[]         = "data";
static const char SegmentLabel[]      = "segment";
static const char InitialLabel[]      = "initial";
static const char EndLabel[]          = "";

enum class Expr : uint16_t
{
    // Control opcodes
    Nop,
    Block,
    Loop,
    If,
    IfElse,
    Select,
    Br,
    BrIf,
    TableSwitch,
    Return,
    Unreachable,

    // Calls
    Call,
    CallIndirect,
    CallImport,

    // Constants and calls
    I32Const,
    I64Const,
    F64Const,
    F32Const,
    GetLocal,
    SetLocal,
    LoadGlobal,
    StoreGlobal,

    // I32 opcodes
    I32Add,
    I32Sub,
    I32Mul,
    I32DivS,
    I32DivU,
    I32RemS,
    I32RemU,
    I32Or,
    I32And,
    I32Xor,
    I32Shl,
    I32ShrU,
    I32ShrS,
    I32Eq,
    I32Ne,
    I32LtS,
    I32LeS,
    I32LtU,
    I32LeU,
    I32GtS,
    I32GeS,
    I32GtU,
    I32GeU,
    I32Clz,
    I32Ctz,
    I32Popcnt,

    // I64 opcodes
    I64Add,
    I64Sub,
    I64Mul,
    I64DivS,
    I64DivU,
    I64RemS,
    I64RemU,
    I64Or,
    I64And,
    I64Xor,
    I64Shl,
    I64ShrU,
    I64ShrS,
    I64Eq,
    I64Ne,
    I64LtS,
    I64LeS,
    I64LtU,
    I64LeU,
    I64GtS,
    I64GeS,
    I64GtU,
    I64GeU,
    I64Clz,
    I64Ctz,
    I64Popcnt,

    // F32 opcodes
    F32Add,
    F32Sub,
    F32Mul,
    F32Div,
    F32Min,
    F32Max,
    F32Abs,
    F32Neg,
    F32CopySign,
    F32Ceil,
    F32Floor,
    F32Trunc,
    F32Nearest,
    F32Sqrt,
    F32Eq,
    F32Ne,
    F32Lt,
    F32Le,
    F32Gt,
    F32Ge,

    // F64 opcodes
    F64Add,
    F64Sub,
    F64Mul,
    F64Div,
    F64Min,
    F64Max,
    F64Abs,
    F64Neg,
    F64CopySign,
    F64Ceil,
    F64Floor,
    F64Trunc,
    F64Nearest,
    F64Sqrt,
    F64Eq,
    F64Ne,
    F64Lt,
    F64Le,
    F64Gt,
    F64Ge,

    // Conversions
    I32WrapI64,
    I64ExtendSI32,
    I64ExtendUI32,
    I32TruncSF32,
    I32TruncSF64,
    I32TruncUF32,
    I32TruncUF64,
    I64TruncSF32,
    I64TruncSF64,
    I64TruncUF32,
    I64TruncUF64,
    F32ConvertSI32,
    F32ConvertUI32,
    F64ConvertSI32,
    F64ConvertUI32,
    F32ConvertSI64,
    F32ConvertUI64,
    F64ConvertSI64,
    F64ConvertUI64,
    F32DemoteF64,
    F64PromoteF32,
    I32ReinterpretF32,
    F32ReinterpretI32,
    I64ReinterpretF64,
    F64ReinterpretI64,

    // Load/store operations
    I32LoadMem8S,
    I32LoadMem8U,
    I32LoadMem16S,
    I32LoadMem16U,
    I32LoadMem,
    I64LoadMem8S,
    I64LoadMem8U,
    I64LoadMem16S,
    I64LoadMem16U,
    I64LoadMem32S,
    I64LoadMem32U,
    I64LoadMem,
    F32LoadMem,
    F64LoadMem,

    I32StoreMem8,
    I32StoreMem16,
    I64StoreMem8,
    I64StoreMem16,
    I64StoreMem32,
    I32StoreMem,
    I64StoreMem,
    F32StoreMem,
    F64StoreMem,

    // asm.js specific
    While,          // all CFG ops to be deleted in favor of Loop/Br/BrIf
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

    Id,

    I32Min,
    I32Max,

    // Atomics
    AtomicsFence,
    I32AtomicsCompareExchange,
    I32AtomicsExchange,
    I32AtomicsLoad,
    I32AtomicsStore,
    I32AtomicsBinOp,

    // SIMD
#define SIMD_OPCODE(TYPE, OP) TYPE##OP,
#define _(OP) SIMD_OPCODE(I32x4, OP)
    FORALL_INT32X4_ASMJS_OP(_)
    I32x4Constructor,
    I32x4Const,
#undef _
    // Unsigned I32x4 operations. These are the SIMD.Uint32x4 operations that
    // behave differently from their SIMD.Int32x4 counterparts.
    I32x4shiftRightByScalarU,
    I32x4lessThanU,
    I32x4lessThanOrEqualU,
    I32x4greaterThanU,
    I32x4greaterThanOrEqualU,
    I32x4fromFloat32x4U,

#define _(OP) SIMD_OPCODE(F32x4, OP)
    FORALL_FLOAT32X4_ASMJS_OP(_)
    F32x4Constructor,
    F32x4Const,
#undef _
#define _(OP) SIMD_OPCODE(B32x4, OP)
    FORALL_BOOL_SIMD_OP(_)
    B32x4Constructor,
    B32x4Const,
#undef _
#undef OPCODE

    // I32 asm.js opcodes
    I32Not,
    I32Neg,
    I32BitNot,
    I32Abs,

    // F32 asm.js opcodes
    F32StoreMemF64,

    // F64 asm.js opcodes
    F64Mod,

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

    F64StoreMemF32,

    Limit
};

typedef Vector<uint8_t, 0, SystemAllocPolicy> Bytecode;
typedef UniquePtr<Bytecode> UniqueBytecode;

// The Encoder class appends bytes to the Bytecode object it is given during
// construction. The client is responsible for the Bytecode's lifetime and must
// keep the Bytecode alive as long as the Encoder is used.
class Encoder
{
    Bytecode& bytecode_;

    template <class T>
    MOZ_WARN_UNUSED_RESULT bool write(const T& v) {
        return bytecode_.append(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
    }

    template <typename UInt>
    MOZ_WARN_UNUSED_RESULT bool writeVarU(UInt i) {
        do {
            uint8_t byte = i & 0x7F;
            i >>= 7;
            if (i != 0)
                byte |= 0x80;
            if (!bytecode_.append(byte))
                return false;
        } while(i != 0);
        return true;
    }

    template <class T>
    MOZ_WARN_UNUSED_RESULT bool writeEnum(T v) {
        static_assert(uint32_t(T::Limit) <= UINT32_MAX, "fits");
        MOZ_ASSERT(uint32_t(v) < uint32_t(T::Limit));
        return writeVarU32(uint32_t(v));
    }

    void patchVarU32(size_t offset, uint32_t patchBits, uint32_t assertBits) {
        do {
            uint8_t assertByte = assertBits & 0x7f;
            uint8_t patchByte = patchBits & 0x7f;
            assertBits >>= 7;
            patchBits >>= 7;
            if (assertBits != 0) {
                assertByte |= 0x80;
                patchByte |= 0x80;
            }
            MOZ_ASSERT(assertByte == bytecode_[offset]);
            bytecode_[offset] = patchByte;
            offset++;
        } while(assertBits != 0);
    }

    uint32_t varU32ByteLength(size_t offset) const {
        size_t start = offset;
        while (bytecode_[offset] & 0x80)
            offset++;
        return offset - start + 1;
    }
    static const uint32_t EnumSentinel = 0x3fff;

    template <class T>
    MOZ_WARN_UNUSED_RESULT bool writePatchableEnum(size_t* offset) {
        static_assert(uint32_t(T::Limit) <= EnumSentinel, "reserve enough bits");
        *offset = bytecode_.length();
        return writeVarU32(EnumSentinel);
    }

    template <class T>
    void patchEnum(size_t offset, T v) {
        static_assert(uint32_t(T::Limit) <= UINT32_MAX, "fits");
        MOZ_ASSERT(uint32_t(v) < uint32_t(T::Limit));
        return patchVarU32(offset, uint32_t(v), EnumSentinel);
    }

  public:
    explicit Encoder(Bytecode& bytecode)
      : bytecode_(bytecode)
    {
        MOZ_ASSERT(empty());
    }

    size_t bytecodeOffset() const { return bytecode_.length(); }
    bool empty() const { return bytecodeOffset() == 0; }

    // Fixed-size encoding operations simply copy the literal bytes (without
    // attempting to align).

    MOZ_WARN_UNUSED_RESULT bool writeFixedU32(uint32_t i) {
        return write<uint32_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedF32(float f) {
        return write<float>(f);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedF64(double d) {
        return write<double>(d);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedI32x4(const Val::I32x4& i32x4) {
        return write<Val::I32x4>(i32x4);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedF32x4(const Val::F32x4& f32x4) {
        return write<Val::F32x4>(f32x4);
    }

    // Variable-length encodings that all use LEB128.

    MOZ_WARN_UNUSED_RESULT bool writeVarU32(uint32_t i) {
        return writeVarU<uint32_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeVarU64(uint64_t i) {
        return writeVarU<uint64_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeExpr(Expr expr) {
        return writeEnum(expr);
    }
    MOZ_WARN_UNUSED_RESULT bool writeValType(ValType type) {
        return writeEnum(type);
    }
    MOZ_WARN_UNUSED_RESULT bool writeExprType(ExprType type) {
        return writeEnum(type);
    }

    // C-strings are written in UTF8 and null-terminated while raw data can
    // contain nulls and instead has an explicit byte length.

    MOZ_WARN_UNUSED_RESULT bool writeCString(const char* cstr) {
        return bytecode_.append(reinterpret_cast<const uint8_t*>(cstr), strlen(cstr) + 1);
    }
    MOZ_WARN_UNUSED_RESULT bool writeRawData(const uint8_t* bytes, uint32_t numBytes) {
        return bytecode_.append(bytes, numBytes);
    }

    // A "section" is a contiguous region of bytes that stores its own size so
    // that it may be trivially skipped without examining the contents. Sections
    // require backpatching since the size of the section is only known at the
    // end while the size's uint32 must be stored at the beginning.

    MOZ_WARN_UNUSED_RESULT bool startSection(size_t* offset) {
        return writePatchableVarU32(offset);
    }
    void finishSection(size_t offset) {
        return patchVarU32(offset, bytecode_.length() - offset - varU32ByteLength(offset));
    }

    // Patching is necessary due to the combination of a preorder encoding and a
    // single-pass algorithm that only knows the precise opcode after visiting
    // children. Switching to a postorder encoding will remove these methods:

    MOZ_WARN_UNUSED_RESULT bool writePatchableVarU32(size_t* offset) {
        *offset = bytecode_.length();
        return writeVarU32(UINT32_MAX);
    }
    void patchVarU32(size_t offset, uint32_t patchBits) {
        return patchVarU32(offset, patchBits, UINT32_MAX);
    }

    MOZ_WARN_UNUSED_RESULT bool writePatchableVarU8(size_t* offset) {
        *offset = bytecode_.length();
        return writeU8(UINT8_MAX);
    }
    void patchVarU8(size_t offset, uint8_t patchBits) {
        MOZ_ASSERT(patchBits < 0x80);
        return patchU8(offset, patchBits);
    }

    MOZ_WARN_UNUSED_RESULT bool writePatchableExpr(size_t* offset) {
        return writePatchableEnum<Expr>(offset);
    }
    void patchExpr(size_t offset, Expr expr) {
        patchEnum(offset, expr);
    }

    // Temporary encoding forms which should be removed as part of the
    // conversion to wasm:

    MOZ_WARN_UNUSED_RESULT bool writeU8(uint8_t i) {
        return write<uint8_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writePatchableU8(size_t* offset) {
        *offset = bytecode_.length();
        return bytecode_.append(0xff);
    }
    void patchU8(size_t offset, uint8_t i) {
        MOZ_ASSERT(bytecode_[offset] == 0xff);
        bytecode_[offset] = i;
    }
};

class Decoder
{
    const uint8_t* const beg_;
    const uint8_t* const end_;
    const uint8_t* cur_;

    uintptr_t bytesRemain() const {
        MOZ_ASSERT(end_ >= cur_);
        return uintptr_t(end_ - cur_);
    }

    template <class T>
    MOZ_WARN_UNUSED_RESULT bool read(T* out) {
        if (bytesRemain() < sizeof(T))
            return false;
        if (out)
            memcpy((void*)out, cur_, sizeof(T));
        cur_ += sizeof(T);
        return true;
    }

    template <class T>
    MOZ_WARN_UNUSED_RESULT bool readEnum(T* out) {
        static_assert(uint32_t(T::Limit) <= UINT32_MAX, "fits");
        uint32_t u32;
        if (!readVarU32(&u32) || u32 >= uint32_t(T::Limit))
            return false;
        if (out)
            *out = T(u32);
        return true;
    }

    template <class T>
    T uncheckedRead() {
        MOZ_ASSERT(bytesRemain() >= sizeof(T));
        T ret;
        memcpy(&ret, cur_, sizeof(T));
        cur_ += sizeof(T);
        return ret;
    }

    template <class T>
    T uncheckedReadEnum() {
        static_assert(uint32_t(T::Limit) <= UINT32_MAX, "fits");
        return (T)uncheckedReadVarU32();
    }

    template <typename UInt>
    MOZ_WARN_UNUSED_RESULT bool readVarU(UInt* out = nullptr) {
        const unsigned numBits = sizeof(UInt) * CHAR_BIT;
        const unsigned remainderBits = numBits % 7;
        const unsigned numBitsInSevens = numBits - remainderBits;
        UInt u = 0;
        uint8_t byte;
        UInt shift = 0;
        do {
            if (!readFixedU8(&byte))
                return false;
            if (!(byte & 0x80)) {
                if (out)
                    *out = u | UInt(byte) << shift;
                return true;
            }
            u |= UInt(byte & 0x7F) << shift;
            shift += 7;
        } while (shift != numBitsInSevens);
        if (!readFixedU8(&byte) || (byte & (unsigned(-1) << remainderBits)))
            return false;
        if (out)
            *out = u | UInt(byte) << numBitsInSevens;
        return true;
    }

  public:
    Decoder(const uint8_t* begin, const uint8_t* end)
      : beg_(begin),
        end_(end),
        cur_(begin)
    {
        MOZ_ASSERT(begin <= end);
    }
    explicit Decoder(const Bytecode& bytecode)
      : beg_(bytecode.begin()),
        end_(bytecode.end()),
        cur_(bytecode.begin())
    {}

    bool done() const {
        MOZ_ASSERT(cur_ <= end_);
        return cur_ == end_;
    }

    const uint8_t* currentPosition() const {
        return cur_;
    }
    size_t currentOffset() const {
        return cur_ - beg_;
    }
    void assertCurrentIs(const DebugOnly<size_t> offset) const {
        MOZ_ASSERT(currentOffset() == offset);
    }

    // Fixed-size encoding operations simply copy the literal bytes (without
    // attempting to align).

    MOZ_WARN_UNUSED_RESULT bool readFixedU32(uint32_t* u = nullptr) {
        return read<uint32_t>(u);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedF32(float* f = nullptr) {
        return read<float>(f);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedF64(double* d = nullptr) {
        return read<double>(d);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedI32x4(Val::I32x4* i32x4 = nullptr) {
        return read<Val::I32x4>(i32x4);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedF32x4(Val::F32x4* f32x4 = nullptr) {
        return read<Val::F32x4>(f32x4);
    }

    // Variable-length encodings that all use LEB128.

    MOZ_WARN_UNUSED_RESULT bool readVarU32(uint32_t* out = nullptr) {
        return readVarU<uint32_t>(out);
    }
    MOZ_WARN_UNUSED_RESULT bool readVarU64(uint64_t* out = nullptr) {
        return readVarU<uint64_t>(out);
    }
    MOZ_WARN_UNUSED_RESULT bool readExpr(Expr* expr = nullptr) {
        return readEnum(expr);
    }
    MOZ_WARN_UNUSED_RESULT bool readValType(ValType* type = nullptr) {
        return readEnum(type);
    }
    MOZ_WARN_UNUSED_RESULT bool readExprType(ExprType* type = nullptr) {
        return readEnum(type);
    }

    // C-strings are written in UTF8 and null-terminated while raw data can
    // contain nulls and instead has an explicit byte length.

    MOZ_WARN_UNUSED_RESULT UniqueChars readCString() {
        const char* begin = reinterpret_cast<const char*>(cur_);
        for (; cur_ != end_; cur_++) {
            if (!*cur_) {
                cur_++;
                return UniqueChars(DuplicateString(begin));
            }
        }
        return nullptr;
    }
    MOZ_WARN_UNUSED_RESULT bool readCStringIf(const char* tag) {
        for (const uint8_t* p = cur_; p != end_; p++, tag++) {
            if (*p != *tag)
                return false;
            if (!*p) {
                cur_ = p + 1;
                return true;
            }
        }
        return false;
    }
    MOZ_WARN_UNUSED_RESULT bool readRawData(uint32_t numBytes, const uint8_t** bytes = nullptr) {
        if (bytes)
            *bytes = cur_;
        if (bytesRemain() < numBytes)
            return false;
        cur_ += numBytes;
        return true;
    }

    // See "section" description in Encoder.

    MOZ_WARN_UNUSED_RESULT bool startSection(uint32_t* startOffset) {
        *startOffset = currentOffset();
        uint32_t unused;
        return readVarU32(&unused);
    }
    MOZ_WARN_UNUSED_RESULT bool finishSection(uint32_t startOffset) {
        uint32_t currentOffset = cur_ - beg_;
        cur_ = beg_ + startOffset;
        uint32_t numBytes = uncheckedReadVarU32();
        uint32_t afterNumBytes = cur_ - beg_;
        cur_ = beg_ + currentOffset;
        return numBytes == (currentOffset - afterNumBytes);
    }
    MOZ_WARN_UNUSED_RESULT bool skipSection() {
        uint32_t numBytes;
        if (!readVarU32(&numBytes) || bytesRemain() < numBytes)
            return false;
        cur_ += numBytes;
        return true;
    }

    // The infallible "unchecked" decoding functions can be used when we are
    // sure that the bytecode is well-formed (by construction or due to previous
    // validation).

    uint32_t uncheckedReadFixedU32() {
        return uncheckedRead<uint32_t>();
    }
    float uncheckedReadFixedF32() {
        return uncheckedRead<float>();
    }
    double uncheckedReadFixedF64() {
        return uncheckedRead<double>();
    }
    template <typename UInt>
    UInt uncheckedReadVarU() {
        static const unsigned numBits = sizeof(UInt) * CHAR_BIT;
        static const unsigned remainderBits = numBits % 7;
        static const unsigned numBitsInSevens = numBits - remainderBits;
        UInt decoded = 0;
        uint32_t shift = 0;
        do {
            uint8_t byte = *cur_++;
            if (!(byte & 0x80))
                return decoded | (UInt(byte) << shift);
            decoded |= UInt(byte & 0x7f) << shift;
            shift += 7;
        } while (shift != numBitsInSevens);
        uint8_t byte = *cur_++;
        MOZ_ASSERT(!(byte & 0xf0));
        return decoded | (UInt(byte) << numBitsInSevens);
    }
    uint32_t uncheckedReadVarU32() {
        return uncheckedReadVarU<uint32_t>();
    }
    uint64_t uncheckedReadVarU64() {
        return uncheckedReadVarU<uint64_t>();
    }
    Expr uncheckedReadExpr() {
        return uncheckedReadEnum<Expr>();
    }
    Expr uncheckedPeekExpr() {
        const uint8_t* before = cur_;
        Expr ret = uncheckedReadEnum<Expr>();
        cur_ = before;
        return ret;
    }

    // Temporary encoding forms which should be removed as part of the
    // conversion to wasm:

    MOZ_WARN_UNUSED_RESULT bool readFixedU8(uint8_t* i = nullptr) {
        return read<uint8_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedI32(int32_t* i = nullptr) {
        return read<int32_t>(i);
    }
    uint8_t uncheckedReadFixedU8() {
        return uncheckedRead<uint8_t>();
    }
};

// The FuncBytecode class contains the intermediate representation of a
// parsed/decoded and validated asm.js/WebAssembly function. The FuncBytecode
// lives only until it is fully compiled.
class FuncBytecode
{
    // Function metadata
    const DeclaredSig& sig_;
    ValTypeVector locals_;
    uint32_t lineOrBytecode_;
    Uint32Vector callSiteLineNums_;

    // Compilation bookkeeping
    uint32_t index_;
    unsigned generateTime_;

    UniqueBytecode bytecode_;

  public:
    FuncBytecode(uint32_t index,
                 const DeclaredSig& sig,
                 UniqueBytecode bytecode,
                 ValTypeVector&& locals,
                 uint32_t lineOrBytecode,
                 Uint32Vector&& callSiteLineNums,
                 unsigned generateTime)
      : sig_(sig),
        locals_(Move(locals)),
        lineOrBytecode_(lineOrBytecode),
        callSiteLineNums_(Move(callSiteLineNums)),
        index_(index),
        generateTime_(generateTime),
        bytecode_(Move(bytecode))
    {}

    UniqueBytecode recycleBytecode() { return Move(bytecode_); }

    uint32_t lineOrBytecode() const { return lineOrBytecode_; }
    const Uint32Vector& callSiteLineNums() const { return callSiteLineNums_; }
    uint32_t index() const { return index_; }
    const DeclaredSig& sig() const { return sig_; }
    const Bytecode& bytecode() const { return *bytecode_; }

    size_t numLocals() const { return locals_.length(); }
    ValType localType(size_t i) const { return locals_[i]; }

    unsigned generateTime() const { return generateTime_; }
};

typedef UniquePtr<FuncBytecode> UniqueFuncBytecode;

} // namespace wasm
} // namespace js

#endif // wasm_binary_h
