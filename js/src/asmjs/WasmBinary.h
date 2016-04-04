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

#include "builtin/SIMD.h"

namespace js {
namespace wasm {

static const uint32_t MagicNumber        = 0x6d736100; // "\0asm"
static const uint32_t EncodingVersion    = 0xa;

static const char SignaturesId[]         = "signatures";
static const char ImportTableId[]        = "import_table";
static const char FunctionSignaturesId[] = "function_signatures";
static const char FunctionTableId[]      = "function_table";
static const char MemoryId[]             = "memory";
static const char ExportTableId[]        = "export_table";
static const char FunctionBodiesId[]     = "function_bodies";
static const char DataSegmentsId[]       = "data_segments";

enum class ValType
{
    // 0x00 is reserved for ExprType::Void in the binary encoding. See comment
    // below about ExprType going away.

    I32                                  = 0x01,
    I64                                  = 0x02,
    F32                                  = 0x03,
    F64                                  = 0x04,

    // ------------------------------------------------------------------------
    // The rest of these types are currently only emitted internally when
    // compiling asm.js and are rejected by wasm validation.

    I32x4,
    F32x4,
    B32x4,

    Limit
};

enum class Expr
{
    // Control flow operators
    Nop                                  = 0x00,
    Block                                = 0x01,
    Loop                                 = 0x02,
    If                                   = 0x03,
    IfElse                               = 0x04,
    Select                               = 0x05,
    Br                                   = 0x06,
    BrIf                                 = 0x07,
    BrTable                              = 0x08,
    Return                               = 0x14,
    Unreachable                          = 0x15,

    // Basic operators
    I32Const                             = 0x0a,
    I64Const                             = 0x0b,
    F64Const                             = 0x0c,
    F32Const                             = 0x0d,
    GetLocal                             = 0x0e,
    SetLocal                             = 0x0f,
    Call                                 = 0x12,
    CallIndirect                         = 0x13,
    CallImport                           = 0x1f,

    // Memory-related operators
    I32Load8S                            = 0x20,
    I32Load8U                            = 0x21,
    I32Load16S                           = 0x22,
    I32Load16U                           = 0x23,
    I64Load8S                            = 0x24,
    I64Load8U                            = 0x25,
    I64Load16S                           = 0x26,
    I64Load16U                           = 0x27,
    I64Load32S                           = 0x28,
    I64Load32U                           = 0x29,
    I32Load                              = 0x2a,
    I64Load                              = 0x2b,
    F32Load                              = 0x2c,
    F64Load                              = 0x2d,
    I32Store8                            = 0x2e,
    I32Store16                           = 0x2f,
    I64Store8                            = 0x30,
    I64Store16                           = 0x31,
    I64Store32                           = 0x32,
    I32Store                             = 0x33,
    I64Store                             = 0x34,
    F32Store                             = 0x35,
    F64Store                             = 0x36,
    MemorySize                           = 0x3b,
    GrowMemory                           = 0x39,

    // i32 operators
    I32Add                               = 0x40,
    I32Sub                               = 0x41,
    I32Mul                               = 0x42,
    I32DivS                              = 0x43,
    I32DivU                              = 0x44,
    I32RemS                              = 0x45,
    I32RemU                              = 0x46,
    I32And                               = 0x47,
    I32Or                                = 0x48,
    I32Xor                               = 0x49,
    I32Shl                               = 0x4a,
    I32ShrU                              = 0x4b,
    I32ShrS                              = 0x4c,
    I32Eq                                = 0x4d,
    I32Ne                                = 0x4e,
    I32LtS                               = 0x4f,
    I32LeS                               = 0x50,
    I32LtU                               = 0x51,
    I32LeU                               = 0x52,
    I32GtS                               = 0x53,
    I32GeS                               = 0x54,
    I32GtU                               = 0x55,
    I32GeU                               = 0x56,
    I32Clz                               = 0x57,
    I32Ctz                               = 0x58,
    I32Popcnt                            = 0x59,
    I32Eqz                               = 0x5a,

    // i64 operators
    I64Add                               = 0x5b,
    I64Sub                               = 0x5c,
    I64Mul                               = 0x5d,
    I64DivS                              = 0x5e,
    I64DivU                              = 0x5f,
    I64RemS                              = 0x60,
    I64RemU                              = 0x61,
    I64And                               = 0x62,
    I64Or                                = 0x63,
    I64Xor                               = 0x64,
    I64Shl                               = 0x65,
    I64ShrU                              = 0x66,
    I64ShrS                              = 0x67,
    I64Eq                                = 0x68,
    I64Ne                                = 0x69,
    I64LtS                               = 0x6a,
    I64LeS                               = 0x6b,
    I64LtU                               = 0x6c,
    I64LeU                               = 0x6d,
    I64GtS                               = 0x6e,
    I64GeS                               = 0x6f,
    I64GtU                               = 0x70,
    I64GeU                               = 0x71,
    I64Clz                               = 0x72,
    I64Ctz                               = 0x73,
    I64Popcnt                            = 0x74,

    // f32 opcodes
    F32Add                               = 0x75,
    F32Sub                               = 0x76,
    F32Mul                               = 0x77,
    F32Div                               = 0x78,
    F32Min                               = 0x79,
    F32Max                               = 0x7a,
    F32Abs                               = 0x7b,
    F32Neg                               = 0x7c,
    F32CopySign                          = 0x7d,
    F32Ceil                              = 0x7e,
    F32Floor                             = 0x7f,
    F32Trunc                             = 0x80,
    F32Nearest                           = 0x81,
    F32Sqrt                              = 0x82,
    F32Eq                                = 0x83,
    F32Ne                                = 0x84,
    F32Lt                                = 0x85,
    F32Le                                = 0x86,
    F32Gt                                = 0x87,
    F32Ge                                = 0x88,

    // f64 opcodes
    F64Add                               = 0x89,
    F64Sub                               = 0x8a,
    F64Mul                               = 0x8b,
    F64Div                               = 0x8c,
    F64Min                               = 0x8d,
    F64Max                               = 0x8e,
    F64Abs                               = 0x8f,
    F64Neg                               = 0x90,
    F64CopySign                          = 0x91,
    F64Ceil                              = 0x92,
    F64Floor                             = 0x93,
    F64Trunc                             = 0x94,
    F64Nearest                           = 0x95,
    F64Sqrt                              = 0x96,
    F64Eq                                = 0x97,
    F64Ne                                = 0x98,
    F64Lt                                = 0x99,
    F64Le                                = 0x9a,
    F64Gt                                = 0x9b,
    F64Ge                                = 0x9c,

    // Conversions
    I32TruncSF32                         = 0x9d,
    I32TruncSF64                         = 0x9e,
    I32TruncUF32                         = 0x9f,
    I32TruncUF64                         = 0xa0,
    I32WrapI64                           = 0xa1,
    I64TruncSF32                         = 0xa2,
    I64TruncSF64                         = 0xa3,
    I64TruncUF32                         = 0xa4,
    I64TruncUF64                         = 0xa5,
    I64ExtendSI32                        = 0xa6,
    I64ExtendUI32                        = 0xa7,
    F32ConvertSI32                       = 0xa8,
    F32ConvertUI32                       = 0xa9,
    F32ConvertSI64                       = 0xaa,
    F32ConvertUI64                       = 0xab,
    F32DemoteF64                         = 0xac,
    F32ReinterpretI32                    = 0xad,
    F64ConvertSI32                       = 0xae,
    F64ConvertUI32                       = 0xaf,
    F64ConvertSI64                       = 0xb0,
    F64ConvertUI64                       = 0xb1,
    F64PromoteF32                        = 0xb2,
    F64ReinterpretI64                    = 0xb3,
    I32ReinterpretF32                    = 0xb4,
    I64ReinterpretF64                    = 0xb5,

    // Bitwise rotates.
    I32Rotr                              = 0xb6,
    I32Rotl                              = 0xb7,
    I64Rotr                              = 0xb8,
    I64Rotl                              = 0xb9,

    // i64.eqz.
    I64Eqz                               = 0xba,

    // ------------------------------------------------------------------------
    // The rest of these operators are currently only emitted internally when
    // compiling asm.js and are rejected by wasm validation.

    // asm.js-specific operators
    Id                                   = 0xc0,
    LoadGlobal,
    StoreGlobal,
    I32Min,
    I32Max,
    I32Neg,
    I32BitNot,
    I32Abs,
    F32StoreF64,
    F64StoreF32,
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

    // Atomics
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

    Limit
};

// The ExprType enum represents the type of a WebAssembly expression or return
// value and may either be a value type or void. Soon, expression types will be
// generalized to a list of ValType and this enum will go away, replaced,
// wherever it is used, by a varU32 + list of ValType.

enum class ExprType
{
    Void  = 0x00,
    I32   = uint8_t(ValType::I32),
    I64   = uint8_t(ValType::I64),
    F32   = uint8_t(ValType::F32),
    F64   = uint8_t(ValType::F64),
    I32x4 = uint8_t(ValType::I32x4),
    F32x4 = uint8_t(ValType::F32x4),
    B32x4 = uint8_t(ValType::B32x4),

    Limit
};

typedef int32_t I32x4[4];
typedef float F32x4[4];
typedef Vector<uint8_t, 0, SystemAllocPolicy> Bytes;

// The Encoder class appends bytes to the Bytes object it is given during
// construction. The client is responsible for the Bytes's lifetime and must
// keep the Bytes alive as long as the Encoder is used.

class Encoder
{
    Bytes& bytes_;

    template <class T>
    MOZ_WARN_UNUSED_RESULT bool write(const T& v) {
        return bytes_.append(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
    }

    template <typename UInt>
    MOZ_WARN_UNUSED_RESULT bool writeVarU(UInt i) {
        do {
            uint8_t byte = i & 0x7f;
            i >>= 7;
            if (i != 0)
                byte |= 0x80;
            if (!bytes_.append(byte))
                return false;
        } while (i != 0);
        return true;
    }

    template <typename SInt>
    MOZ_WARN_UNUSED_RESULT bool writeVarS(SInt i) {
        bool done;
        do {
            uint8_t byte = i & 0x7f;
            i >>= 7;
            done = ((i == 0) && !(byte & 0x40)) || ((i == -1) && (byte & 0x40));
            if (!done)
                byte |= 0x80;
            if (!bytes_.append(byte))
                return false;
        } while (!done);
        return true;
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
            MOZ_ASSERT(assertByte == bytes_[offset]);
            bytes_[offset] = patchByte;
            offset++;
        } while(assertBits != 0);
    }

    uint32_t varU32ByteLength(size_t offset) const {
        size_t start = offset;
        while (bytes_[offset] & 0x80)
            offset++;
        return offset - start + 1;
    }

    static const size_t ExprLimit = 2 * UINT8_MAX - 1;

  public:
    explicit Encoder(Bytes& bytes)
      : bytes_(bytes)
    {
        MOZ_ASSERT(empty());
    }

    size_t currentOffset() const { return bytes_.length(); }
    bool empty() const { return currentOffset() == 0; }

    // Fixed-size encoding operations simply copy the literal bytes (without
    // attempting to align).

    MOZ_WARN_UNUSED_RESULT bool writeFixedU8(uint8_t i) {
        return write<uint8_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedU32(uint32_t i) {
        return write<uint32_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedF32(float f) {
        return write<float>(f);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedF64(double d) {
        return write<double>(d);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedI32x4(const I32x4& i32x4) {
        return write<I32x4>(i32x4);
    }
    MOZ_WARN_UNUSED_RESULT bool writeFixedF32x4(const F32x4& f32x4) {
        return write<F32x4>(f32x4);
    }

    // Variable-length encodings that all use LEB128.

    MOZ_WARN_UNUSED_RESULT bool writeVarU32(uint32_t i) {
        return writeVarU<uint32_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeVarS32(int32_t i) {
        return writeVarS<int32_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeVarU64(uint64_t i) {
        return writeVarU<uint64_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeVarS64(int64_t i) {
        return writeVarS<int64_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool writeValType(ValType type) {
        static_assert(size_t(ValType::Limit) <= INT8_MAX, "fits");
        return writeFixedU8(size_t(type));
    }
    MOZ_WARN_UNUSED_RESULT bool writeExprType(ExprType type) {
        static_assert(size_t(ExprType::Limit) <= INT8_MAX, "fits");
        return writeFixedU8(uint8_t(type));
    }
    MOZ_WARN_UNUSED_RESULT bool writeExpr(Expr expr) {
        static_assert(size_t(Expr::Limit) <= ExprLimit, "fits");
        if (size_t(expr) < UINT8_MAX)
            return writeFixedU8(uint8_t(expr));
        return writeFixedU8(UINT8_MAX) &&
               writeFixedU8(size_t(expr) - UINT8_MAX);
    }

    // Variable-length encodings that allow back-patching.

    MOZ_WARN_UNUSED_RESULT bool writePatchableFixedU8(size_t* offset) {
        *offset = bytes_.length();
        return bytes_.append(0xff);
    }
    void patchFixedU8(size_t offset, uint8_t i) {
        MOZ_ASSERT(bytes_[offset] == 0xff);
        bytes_[offset] = i;
    }

    MOZ_WARN_UNUSED_RESULT bool writePatchableVarU32(size_t* offset) {
        *offset = bytes_.length();
        return writeVarU32(UINT32_MAX);
    }
    void patchVarU32(size_t offset, uint32_t patchBits) {
        return patchVarU32(offset, patchBits, UINT32_MAX);
    }

    MOZ_WARN_UNUSED_RESULT bool writePatchableOneByteExpr(size_t* offset) {
        *offset = bytes_.length();
        return writeFixedU8(0xff);
    }
    void patchOneByteExpr(size_t offset, Expr expr) {
        MOZ_ASSERT(size_t(expr) < UINT8_MAX);
        MOZ_ASSERT(bytes_[offset] == 0xff);
        bytes_[offset] = uint8_t(expr);
    }

    // Byte ranges start with an LEB128 length followed by an arbitrary sequence
    // of bytes. When used for strings, bytes are to be interpreted as utf8.

    MOZ_WARN_UNUSED_RESULT bool writeBytes(const void* bytes, uint32_t numBytes) {
        return writeVarU32(numBytes) &&
               bytes_.append(reinterpret_cast<const uint8_t*>(bytes), numBytes);
    }

    // A "section" is a contiguous range of bytes that stores its own size so
    // that it may be trivially skipped without examining the contents. Sections
    // require backpatching since the size of the section is only known at the
    // end while the size's varU32 must be stored at the beginning. Immediately
    // after the section length is the string id of the section.

    template <size_t IdSizeWith0>
    MOZ_WARN_UNUSED_RESULT bool startSection(const char (&id)[IdSizeWith0], size_t* offset) {
        static const size_t IdSize = IdSizeWith0 - 1;
        MOZ_ASSERT(id[IdSize] == '\0');
        return writePatchableVarU32(offset) &&
               writeVarU32(IdSize) &&
               bytes_.append(reinterpret_cast<const uint8_t*>(id), IdSize);
    }
    void finishSection(size_t offset) {
        return patchVarU32(offset, bytes_.length() - offset - varU32ByteLength(offset));
    }
};

// The Decoder class decodes the bytes in the range it is given during
// construction. The client is responsible for keeping the byte range alive as
// long as the Decoder is used.

class Decoder
{
    const uint8_t* const beg_;
    const uint8_t* const end_;
    const uint8_t* cur_;

    template <class T>
    MOZ_WARN_UNUSED_RESULT bool read(T* out) {
        if (bytesRemain() < sizeof(T))
            return false;
        memcpy((void*)out, cur_, sizeof(T));
        cur_ += sizeof(T);
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

    template <typename UInt>
    MOZ_WARN_UNUSED_RESULT bool readVarU(UInt* out) {
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
                *out = u | UInt(byte) << shift;
                return true;
            }
            u |= UInt(byte & 0x7F) << shift;
            shift += 7;
        } while (shift != numBitsInSevens);
        if (!readFixedU8(&byte) || (byte & (unsigned(-1) << remainderBits)))
            return false;
        *out = u | (UInt(byte) << numBitsInSevens);
        return true;
    }

    template <typename SInt>
    MOZ_WARN_UNUSED_RESULT bool readVarS(SInt* out) {
        const unsigned numBits = sizeof(SInt) * CHAR_BIT;
        const unsigned remainderBits = numBits % 7;
        const unsigned numBitsInSevens = numBits - remainderBits;
        SInt s = 0;
        uint8_t byte;
        unsigned shift = 0;
        do {
            if (!readFixedU8(&byte))
                return false;
            s |= SInt(byte & 0x7f) << shift;
            shift += 7;
            if (!(byte & 0x80)) {
                if (byte & 0x40)
                    s |= SInt(-1) << shift;
                *out = s;
                return true;
            }
        } while (shift < numBitsInSevens);
        if (!remainderBits || !readFixedU8(&byte) || (byte & 0x80))
            return false;
        uint8_t mask = 0x7f & (uint8_t(-1) << remainderBits);
        if ((byte & mask) != ((byte & (1 << (remainderBits - 1))) ? mask : 0))
            return false;
        *out = s | SInt(byte) << shift;
        return true;
    }

    static const size_t ExprLimit = 2 * UINT8_MAX - 1;

  public:
    Decoder(const uint8_t* begin, const uint8_t* end)
      : beg_(begin),
        end_(end),
        cur_(begin)
    {
        MOZ_ASSERT(begin <= end);
    }
    explicit Decoder(const Bytes& bytes)
      : beg_(bytes.begin()),
        end_(bytes.end()),
        cur_(bytes.begin())
    {}

    bool done() const {
        MOZ_ASSERT(cur_ <= end_);
        return cur_ == end_;
    }

    uintptr_t bytesRemain() const {
        MOZ_ASSERT(end_ >= cur_);
        return uintptr_t(end_ - cur_);
    }
    const uint8_t* currentPosition() const {
        return cur_;
    }
    size_t currentOffset() const {
        return cur_ - beg_;
    }

    // Fixed-size encoding operations simply copy the literal bytes (without
    // attempting to align).

    MOZ_WARN_UNUSED_RESULT bool readFixedU8(uint8_t* i) {
        return read<uint8_t>(i);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedU32(uint32_t* u) {
        return read<uint32_t>(u);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedF32(float* f) {
        return read<float>(f);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedF64(double* d) {
        return read<double>(d);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedI32x4(I32x4* i32x4) {
        return read<I32x4>(i32x4);
    }
    MOZ_WARN_UNUSED_RESULT bool readFixedF32x4(F32x4* f32x4) {
        return read<F32x4>(f32x4);
    }

    // Variable-length encodings that all use LEB128.

    MOZ_WARN_UNUSED_RESULT bool readVarU32(uint32_t* out) {
        return readVarU<uint32_t>(out);
    }
    MOZ_WARN_UNUSED_RESULT bool readVarS32(int32_t* out) {
        return readVarS<int32_t>(out);
    }
    MOZ_WARN_UNUSED_RESULT bool readVarU64(uint64_t* out) {
        return readVarU<uint64_t>(out);
    }
    MOZ_WARN_UNUSED_RESULT bool readVarS64(int64_t* out) {
        return readVarS<int64_t>(out);
    }
    MOZ_WARN_UNUSED_RESULT bool readValType(ValType* type) {
        static_assert(uint8_t(ValType::Limit) <= INT8_MAX, "fits");
        uint8_t u8;
        if (!readFixedU8(&u8))
            return false;
        *type = (ValType)u8;
        return true;
    }
    MOZ_WARN_UNUSED_RESULT bool readExprType(ExprType* type) {
        static_assert(uint8_t(ExprType::Limit) <= INT8_MAX, "fits");
        uint8_t u8;
        if (!readFixedU8(&u8))
            return false;
        *type = (ExprType)u8;
        return true;
    }
    MOZ_WARN_UNUSED_RESULT bool readExpr(Expr* expr) {
        static_assert(size_t(Expr::Limit) <= ExprLimit, "fits");
        uint8_t u8;
        if (!readFixedU8(&u8))
            return false;
        if (u8 != UINT8_MAX) {
            *expr = Expr(u8);
            return true;
        }
        if (!readFixedU8(&u8))
            return false;
        if (u8 == UINT8_MAX)
            return false;
        *expr = Expr(u8 + UINT8_MAX);
        return true;
    }

    // See writeBytes comment.

    MOZ_WARN_UNUSED_RESULT bool readBytes(Bytes* bytes) {
        uint32_t numBytes;
        if (!readVarU32(&numBytes))
            return false;
        if (bytesRemain() < numBytes)
            return false;
        if (!bytes->resize(numBytes))
            return false;
        memcpy(bytes->begin(), cur_, numBytes);
        cur_ += numBytes;
        return true;
    }
    MOZ_WARN_UNUSED_RESULT bool readBytesRaw(uint32_t numBytes, const uint8_t** bytes) {
        if (bytes)
            *bytes = cur_;
        if (bytesRemain() < numBytes)
            return false;
        cur_ += numBytes;
        return true;
    }

    // See "section" description in Encoder.

    static const uint32_t NotStarted = UINT32_MAX;

    template <size_t IdSizeWith0>
    MOZ_WARN_UNUSED_RESULT bool startSection(const char (&id)[IdSizeWith0], uint32_t* startOffset) {
        static const size_t IdSize = IdSizeWith0 - 1;
        MOZ_ASSERT(id[IdSize] == '\0');
        const uint8_t* before = cur_;
        uint32_t size;
        if (!readVarU32(&size))
            goto backup;
        if (bytesRemain() < size)
            return false;
        uint32_t idSize;
        if (!readVarU32(&idSize))
            goto backup;
        if (bytesRemain() < idSize)
            return false;
        if (idSize != IdSize || !!memcmp(cur_, id, IdSize))
            goto backup;
        cur_ += IdSize;
        *startOffset = before - beg_;
        return true;
      backup:
        cur_ = before;
        *startOffset = NotStarted;
        return true;
    }
    MOZ_WARN_UNUSED_RESULT bool finishSection(uint32_t startOffset) {
        uint32_t currentOffset = cur_ - beg_;
        cur_ = beg_ + startOffset;
        uint32_t size = uncheckedReadVarU32();
        uint32_t afterSize = cur_ - beg_;
        cur_ = beg_ + currentOffset;
        return size == (currentOffset - afterSize);
    }
    MOZ_WARN_UNUSED_RESULT bool skipSection() {
        uint32_t size;
        if (!readVarU32(&size) || bytesRemain() < size)
            return false;
        const uint8_t* begin = cur_;
        uint32_t idSize;
        if (!readVarU32(&idSize) || bytesRemain() < idSize)
            return false;
        if (uint32_t(cur_ - begin) > size)
            return false;
        cur_ = begin + size;
        return true;
    }

    // The infallible "unchecked" decoding functions can be used when we are
    // sure that the bytes are well-formed (by construction or due to previous
    // validation).

    uint8_t uncheckedReadFixedU8() {
        return uncheckedRead<uint8_t>();
    }
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
    int32_t uncheckedReadVarS32() {
        int32_t i32;
        MOZ_ALWAYS_TRUE(readVarS32(&i32));
        return i32;
    }
    uint64_t uncheckedReadVarU64() {
        return uncheckedReadVarU<uint64_t>();
    }
    int64_t uncheckedReadVarS64() {
        int64_t i64;
        MOZ_ALWAYS_TRUE(readVarS64(&i64));
        return i64;
    }
    ValType uncheckedReadValType() {
        return (ValType)uncheckedReadFixedU8();
    }
    Expr uncheckedReadExpr() {
        static_assert(size_t(Expr::Limit) <= ExprLimit, "fits");
        uint8_t u8 = uncheckedReadFixedU8();
        return u8 != UINT8_MAX
               ? Expr(u8)
               : Expr(uncheckedReadFixedU8() + UINT8_MAX);
    }
    Expr uncheckedPeekExpr() {
        static_assert(size_t(Expr::Limit) <= ExprLimit, "fits");
        uint8_t u8 = cur_[0];
        return u8 != UINT8_MAX
               ? Expr(u8)
               : Expr(cur_[1] + UINT8_MAX);
    }
};

// Reusable macro encoding/decoding functions reused by both the two
// encoders (AsmJS/WasmText) and decoders (Wasm/WasmIonCompile).

typedef Vector<ValType, 8, SystemAllocPolicy> ValTypeVector;

bool
EncodeLocalEntries(Encoder& d, const ValTypeVector& locals);

bool
DecodeLocalEntries(Decoder& d, ValTypeVector* locals);

} // namespace wasm
} // namespace js

#endif // wasm_binary_h
