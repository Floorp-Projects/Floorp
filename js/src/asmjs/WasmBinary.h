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

// Because WebAssembly allows one to define the payload of a NaN value,
// including the signal/quiet bit (highest order bit of payload), another
// represenation of floating-point values is required: on some platforms (x86
// without SSE2), passing a floating-point argument to a function call may use
// the x87 stack, which has the side-effect of clearing the signal/quiet bit.
// Because the signal/quiet bit must be preserved (by spec), we use the raw
// punned integer representation of floating points instead, in function calls.
//
// When we leave the WebAssembly sandbox back to JS, NaNs are canonicalized, so
// this isn't observable from JS.

template<class T>
class Raw
{
    typedef typename mozilla::FloatingPoint<T>::Bits Bits;
    Bits value_;

  public:
    Raw() : value_(0) {}

    explicit Raw(T value)
      : value_(mozilla::BitwiseCast<Bits>(value))
    {}

    template<class U> MOZ_IMPLICIT Raw(U) = delete;

    static Raw fromBits(Bits bits) { Raw r; r.value_ = bits; return r; }

    Bits bits() const { return value_; }
    T fp() const { return mozilla::BitwiseCast<T>(value_); }
};

using RawF64 = Raw<double>;
using RawF32 = Raw<float>;

// Telemetry sample values for the JS_AOT_USAGE key, indicating whether asm.js
// or WebAssembly is used.

enum class Telemetry {
    ASMJS = 0,
    WASM = 1
};

static const uint32_t MagicNumber        = 0x6d736100; // "\0asm"
static const uint32_t EncodingVersion    = 0x0b;

static const char TypeSectionId[]        = "type";
static const char GlobalSectionId[]      = "global";
static const char ImportSectionId[]      = "import";
static const char FunctionSectionId[]    = "function";
static const char TableSectionId[]       = "table";
static const char MemorySectionId[]      = "memory";
static const char ExportSectionId[]      = "export";
static const char StartSectionId[]       = "start";
static const char CodeSectionId[]        = "code";
static const char ElemSectionId[]        = "elem";
static const char DataSectionId[]        = "data";
static const char NameSectionId[]        = "name";

enum class ValType
{
    I32                                  = 0x01,
    I64                                  = 0x02,
    F32                                  = 0x03,
    F64                                  = 0x04,

    // ------------------------------------------------------------------------
    // The rest of these types are currently only emitted internally when
    // compiling asm.js and are rejected by wasm validation.

    I8x16,
    I16x8,
    I32x4,
    F32x4,
    B8x16,
    B16x8,
    B32x4,

    Limit
};

enum class TypeConstructor
{
    AnyFunc                              = 0x20,
    Function                             = 0x40
};

enum class DefinitionKind
{
    Function                             = 0x00,
    Table                                = 0x01,
    Memory                               = 0x02,
    Global                               = 0x03
};

enum class ResizableFlags
{
    Default                              = 0x1,
    HasMaximum                           = 0x2,
    AllowedMask                          = 0x3
};

enum class GlobalFlags
{
    IsMutable                            = 0x1,
    AllowedMask                          = 0x1
};

enum class Expr
{
    // Control flow operators
    Nop                                  = 0x00,
    Block                                = 0x01,
    Loop                                 = 0x02,
    If                                   = 0x03,
    Else                                 = 0x04,
    Select                               = 0x05,
    Br                                   = 0x06,
    BrIf                                 = 0x07,
    BrTable                              = 0x08,
    Return                               = 0x09,
    Unreachable                          = 0x0a,
    End                                  = 0x0f,

    // Basic operators
    I32Const                             = 0x10,
    I64Const                             = 0x11,
    F64Const                             = 0x12,
    F32Const                             = 0x13,
    GetLocal                             = 0x14,
    SetLocal                             = 0x15,
    Call                                 = 0x16,
    CallIndirect                         = 0x17,
    CallImport                           = 0x18,

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
    CurrentMemory                        = 0x3b,
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

    // Global access.
    GetGlobal                            = 0xc0,
    SetGlobal                            = 0xc1,

    // ------------------------------------------------------------------------
    // The rest of these operators are currently only emitted internally when
    // compiling asm.js and are rejected by wasm validation.

    // asm.js-specific operators
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
#define _(OP) SIMD_OPCODE(I8x16, OP)
    FORALL_INT8X16_ASMJS_OP(_)
    I8x16Constructor,
    I8x16Const,
#undef _
    // Unsigned I8x16 operations. These are the SIMD.Uint8x16 operations that
    // behave differently from their SIMD.Int8x16 counterparts.
    I8x16extractLaneU,
    I8x16addSaturateU,
    I8x16subSaturateU,
    I8x16shiftRightByScalarU,
    I8x16lessThanU,
    I8x16lessThanOrEqualU,
    I8x16greaterThanU,
    I8x16greaterThanOrEqualU,

#define SIMD_OPCODE(TYPE, OP) TYPE##OP,
#define _(OP) SIMD_OPCODE(I16x8, OP)
    FORALL_INT16X8_ASMJS_OP(_)
    I16x8Constructor,
    I16x8Const,
#undef _
    // Unsigned I16x8 operations. These are the SIMD.Uint16x8 operations that
    // behave differently from their SIMD.Int16x8 counterparts.
    I16x8extractLaneU,
    I16x8addSaturateU,
    I16x8subSaturateU,
    I16x8shiftRightByScalarU,
    I16x8lessThanU,
    I16x8lessThanOrEqualU,
    I16x8greaterThanU,
    I16x8greaterThanOrEqualU,

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

#define _(OP) SIMD_OPCODE(B8x16, OP)
    FORALL_BOOL_SIMD_OP(_)
    B8x16Constructor,
    B8x16Const,
#undef _
#undef OPCODE

#define _(OP) SIMD_OPCODE(B16x8, OP)
    FORALL_BOOL_SIMD_OP(_)
    B16x8Constructor,
    B16x8Const,
#undef _
#undef OPCODE

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
    I8x16 = uint8_t(ValType::I8x16),
    I16x8 = uint8_t(ValType::I16x8),
    I32x4 = uint8_t(ValType::I32x4),
    F32x4 = uint8_t(ValType::F32x4),
    B8x16 = uint8_t(ValType::B8x16),
    B16x8 = uint8_t(ValType::B16x8),
    B32x4 = uint8_t(ValType::B32x4),

    Limit
};

typedef int8_t I8x16[16];
typedef int16_t I16x8[8];
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
    MOZ_MUST_USE bool write(const T& v) {
        return bytes_.append(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
    }

    template <typename UInt>
    MOZ_MUST_USE bool writeVarU(UInt i) {
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
    MOZ_MUST_USE bool writeVarS(SInt i) {
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

    MOZ_MUST_USE bool writeFixedU8(uint8_t i) {
        return write<uint8_t>(i);
    }
    MOZ_MUST_USE bool writeFixedU32(uint32_t i) {
        return write<uint32_t>(i);
    }
    MOZ_MUST_USE bool writeFixedF32(RawF32 f) {
        return write<uint32_t>(f.bits());
    }
    MOZ_MUST_USE bool writeFixedF64(RawF64 d) {
        return write<uint64_t>(d.bits());
    }
    MOZ_MUST_USE bool writeFixedI8x16(const I8x16& i8x16) {
        return write<I8x16>(i8x16);
    }
    MOZ_MUST_USE bool writeFixedI16x8(const I16x8& i16x8) {
        return write<I16x8>(i16x8);
    }
    MOZ_MUST_USE bool writeFixedI32x4(const I32x4& i32x4) {
        return write<I32x4>(i32x4);
    }
    MOZ_MUST_USE bool writeFixedF32x4(const F32x4& f32x4) {
        return write<F32x4>(f32x4);
    }

    // Variable-length encodings that all use LEB128.

    MOZ_MUST_USE bool writeVarU32(uint32_t i) {
        return writeVarU<uint32_t>(i);
    }
    MOZ_MUST_USE bool writeVarS32(int32_t i) {
        return writeVarS<int32_t>(i);
    }
    MOZ_MUST_USE bool writeVarU64(uint64_t i) {
        return writeVarU<uint64_t>(i);
    }
    MOZ_MUST_USE bool writeVarS64(int64_t i) {
        return writeVarS<int64_t>(i);
    }
    MOZ_MUST_USE bool writeValType(ValType type) {
        static_assert(size_t(ValType::Limit) <= INT8_MAX, "fits");
        return writeFixedU8(size_t(type));
    }
    MOZ_MUST_USE bool writeExpr(Expr expr) {
        static_assert(size_t(Expr::Limit) <= ExprLimit, "fits");
        if (size_t(expr) < UINT8_MAX)
            return writeFixedU8(uint8_t(expr));
        return writeFixedU8(UINT8_MAX) &&
               writeFixedU8(size_t(expr) - UINT8_MAX);
    }

    // Variable-length encodings that allow back-patching.

    MOZ_MUST_USE bool writePatchableVarU32(size_t* offset) {
        *offset = bytes_.length();
        return writeVarU32(UINT32_MAX);
    }
    void patchVarU32(size_t offset, uint32_t patchBits) {
        return patchVarU32(offset, patchBits, UINT32_MAX);
    }

    // Byte ranges start with an LEB128 length followed by an arbitrary sequence
    // of bytes. When used for strings, bytes are to be interpreted as utf8.

    MOZ_MUST_USE bool writeBytes(const void* bytes, uint32_t numBytes) {
        return writeVarU32(numBytes) &&
               bytes_.append(reinterpret_cast<const uint8_t*>(bytes), numBytes);
    }

    // A "section" is a contiguous range of bytes that stores its own size so
    // that it may be trivially skipped without examining the contents. Sections
    // require backpatching since the size of the section is only known at the
    // end while the size's varU32 must be stored at the beginning. Immediately
    // after the section length is the string id of the section.

    template <size_t IdSizeWith0>
    MOZ_MUST_USE bool startSection(const char (&id)[IdSizeWith0], size_t* offset) {
        static const size_t IdSize = IdSizeWith0 - 1;
        MOZ_ASSERT(id[IdSize] == '\0');
        return writeVarU32(IdSize) &&
               bytes_.append(reinterpret_cast<const uint8_t*>(id), IdSize) &&
               writePatchableVarU32(offset);
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
    UniqueChars* error_;

    template <class T>
    MOZ_MUST_USE bool read(T* out) {
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

    template <class T>
    void uncheckedRead(T* ret) {
        MOZ_ASSERT(bytesRemain() >= sizeof(T));
        memcpy(ret, cur_, sizeof(T));
        cur_ += sizeof(T);
    }

    template <typename UInt>
    MOZ_MUST_USE bool readVarU(UInt* out) {
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
    MOZ_MUST_USE bool readVarS(SInt* out) {
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
    Decoder(const uint8_t* begin, const uint8_t* end, UniqueChars* error = nullptr)
      : beg_(begin),
        end_(end),
        cur_(begin),
        error_(error)
    {
        MOZ_ASSERT(begin <= end);
    }
    explicit Decoder(const Bytes& bytes, UniqueChars* error = nullptr)
      : beg_(bytes.begin()),
        end_(bytes.end()),
        cur_(bytes.begin()),
        error_(error)
    {}

    bool fail(const char* msg) {
        error_->reset(strdup(msg));
        return false;
    }
    bool fail(UniqueChars msg) {
        *error_ = Move(msg);
        return false;
    }
    void clearError() {
        if (error_)
            error_->reset();
    }

    bool done() const {
        MOZ_ASSERT(cur_ <= end_);
        return cur_ == end_;
    }

    size_t bytesRemain() const {
        MOZ_ASSERT(end_ >= cur_);
        return size_t(end_ - cur_);
    }
    const uint8_t* currentPosition() const {
        return cur_;
    }
    size_t currentOffset() const {
        return cur_ - beg_;
    }

    // Fixed-size encoding operations simply copy the literal bytes (without
    // attempting to align).

    MOZ_MUST_USE bool readFixedU8(uint8_t* i) {
        return read<uint8_t>(i);
    }
    MOZ_MUST_USE bool readFixedU32(uint32_t* u) {
        return read<uint32_t>(u);
    }
    MOZ_MUST_USE bool readFixedF32(RawF32* f) {
        uint32_t u;
        if (!read<uint32_t>(&u))
            return false;
        *f = RawF32::fromBits(u);
        return true;
    }
    MOZ_MUST_USE bool readFixedF64(RawF64* d) {
        uint64_t u;
        if (!read<uint64_t>(&u))
            return false;
        *d = RawF64::fromBits(u);
        return true;
    }
    MOZ_MUST_USE bool readFixedI8x16(I8x16* i8x16) {
        return read<I8x16>(i8x16);
    }
    MOZ_MUST_USE bool readFixedI16x8(I16x8* i16x8) {
        return read<I16x8>(i16x8);
    }
    MOZ_MUST_USE bool readFixedI32x4(I32x4* i32x4) {
        return read<I32x4>(i32x4);
    }
    MOZ_MUST_USE bool readFixedF32x4(F32x4* f32x4) {
        return read<F32x4>(f32x4);
    }

    // Variable-length encodings that all use LEB128.

    MOZ_MUST_USE bool readVarU32(uint32_t* out) {
        return readVarU<uint32_t>(out);
    }
    MOZ_MUST_USE bool readVarS32(int32_t* out) {
        return readVarS<int32_t>(out);
    }
    MOZ_MUST_USE bool readVarU64(uint64_t* out) {
        return readVarU<uint64_t>(out);
    }
    MOZ_MUST_USE bool readVarS64(int64_t* out) {
        return readVarS<int64_t>(out);
    }
    MOZ_MUST_USE bool readValType(ValType* type) {
        static_assert(uint8_t(ValType::Limit) <= INT8_MAX, "fits");
        uint8_t u8;
        if (!readFixedU8(&u8))
            return false;
        *type = (ValType)u8;
        return true;
    }
    MOZ_MUST_USE bool readExpr(Expr* expr) {
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

    MOZ_MUST_USE bool readBytes(uint32_t numBytes, const uint8_t** bytes = nullptr) {
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
    MOZ_MUST_USE bool startSection(const char (&id)[IdSizeWith0],
                                   uint32_t* startOffset,
                                   uint32_t* size) {
        static const size_t IdSize = IdSizeWith0 - 1;
        MOZ_ASSERT(id[IdSize] == '\0');
        const uint8_t* before = cur_;
        uint32_t idSize;
        if (!readVarU32(&idSize))
            goto backup;
        if (bytesRemain() < idSize)
            return false;
        if (idSize != IdSize || !!memcmp(cur_, id, IdSize))
            goto backup;
        cur_ += IdSize;
        if (!readVarU32(size))
            goto backup;
        if (bytesRemain() < *size)
            return false;
        *startOffset = cur_ - beg_;
        return true;
      backup:
        cur_ = before;
        *startOffset = NotStarted;
        return true;
    }
    MOZ_MUST_USE bool finishSection(uint32_t startOffset, uint32_t size) {
        return size == (cur_ - beg_) - startOffset;
    }
    void ignoreSection(uint32_t startOffset, uint32_t size) {
        cur_ = (beg_ + startOffset) + size;
        MOZ_ASSERT(cur_ <= end_);
    }
    MOZ_MUST_USE bool skipSection() {
        uint32_t idSize;
        if (!readVarU32(&idSize) || bytesRemain() < idSize)
            return false;
        cur_ += idSize;
        uint32_t size;
        if (!readVarU32(&size) || bytesRemain() < size)
            return false;
        cur_ += size;
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
    RawF32 uncheckedReadFixedF32() {
        return RawF32::fromBits(uncheckedRead<uint32_t>());
    }
    RawF64 uncheckedReadFixedF64() {
        return RawF64::fromBits(uncheckedRead<uint64_t>());
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
        int32_t i32 = 0;
        MOZ_ALWAYS_TRUE(readVarS32(&i32));
        return i32;
    }
    uint64_t uncheckedReadVarU64() {
        return uncheckedReadVarU<uint64_t>();
    }
    int64_t uncheckedReadVarS64() {
        int64_t i64 = 0;
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
    void uncheckedReadFixedI8x16(I8x16* i8x16) {
        struct T { I8x16 v; };
        T t = uncheckedRead<T>();
        memcpy(i8x16, &t, sizeof(t));
    }
    void uncheckedReadFixedI16x8(I16x8* i16x8) {
        struct T { I16x8 v; };
        T t = uncheckedRead<T>();
        memcpy(i16x8, &t, sizeof(t));
    }
    void uncheckedReadFixedI32x4(I32x4* i32x4) {
        struct T { I32x4 v; };
        T t = uncheckedRead<T>();
        memcpy(i32x4, &t, sizeof(t));
    }
    void uncheckedReadFixedF32x4(F32x4* f32x4) {
        struct T { F32x4 v; };
        T t = uncheckedRead<T>();
        memcpy(f32x4, &t, sizeof(t));
    }
};

// Reusable macro encoding/decoding functions reused by both the two
// encoders (AsmJS/WasmText) and decoders (Wasm/WasmIonCompile).

typedef Vector<ValType, 8, SystemAllocPolicy> ValTypeVector;

MOZ_MUST_USE bool
EncodeLocalEntries(Encoder& d, const ValTypeVector& locals);

MOZ_MUST_USE bool
DecodeLocalEntries(Decoder& d, ValTypeVector* locals);

} // namespace wasm
} // namespace js

#endif // wasm_binary_h
