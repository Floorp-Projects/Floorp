/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
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

#ifndef wasm_validate_h
#define wasm_validate_h

#include <type_traits>

#include "ds/Bitmap.h"

#include "wasm/WasmTypes.h"

namespace js {
namespace wasm {

// This struct captures the bytecode offset of a section's payload (so not
// including the header) and the size of the payload.

struct SectionRange {
  uint32_t start;
  uint32_t size;

  uint32_t end() const { return start + size; }
  bool operator==(const SectionRange& rhs) const {
    return start == rhs.start && size == rhs.size;
  }
};

using MaybeSectionRange = Maybe<SectionRange>;

// CompilerEnvironment holds any values that will be needed to compute
// compilation parameters once the module's feature opt-in sections have been
// parsed.
//
// Subsequent to construction a computeParameters() call will compute the final
// compilation parameters, and the object can then be queried for their values.

struct CompileArgs;
class Decoder;

struct CompilerEnvironment {
  // The object starts in one of two "initial" states; computeParameters moves
  // it into the "computed" state.
  enum State { InitialWithArgs, InitialWithModeTierDebug, Computed };

  State state_;
  union {
    // Value if the state_ == InitialWithArgs.
    const CompileArgs* args_;

    // Value in the other two states.
    struct {
      CompileMode mode_;
      Tier tier_;
      OptimizedBackend optimizedBackend_;
      DebugEnabled debug_;
      bool refTypes_;
      bool gcTypes_;
      bool multiValues_;
      bool hugeMemory_;
      bool bigInt_;
    };
  };

 public:
  // Retain a reference to the CompileArgs. A subsequent computeParameters()
  // will compute all parameters from the CompileArgs and additional values.
  explicit CompilerEnvironment(const CompileArgs& args);

  // Save the provided values for mode, tier, and debug, and the initial value
  // for gcTypes/refTypes. A subsequent computeParameters() will compute the
  // final value of gcTypes/refTypes.
  CompilerEnvironment(CompileMode mode, Tier tier,
                      OptimizedBackend optimizedBackend,
                      DebugEnabled debugEnabled, bool multiValueConfigured,
                      bool refTypesConfigured, bool gcTypesConfigured,
                      bool hugeMemory, bool bigIntConfigured);

  // Compute any remaining compilation parameters.
  void computeParameters(Decoder& d);

  // Compute any remaining compilation parameters.  Only use this method if
  // the CompilerEnvironment was created with values for mode, tier, and
  // debug.
  void computeParameters();

  bool isComputed() const { return state_ == Computed; }
  CompileMode mode() const {
    MOZ_ASSERT(isComputed());
    return mode_;
  }
  Tier tier() const {
    MOZ_ASSERT(isComputed());
    return tier_;
  }
  OptimizedBackend optimizedBackend() const {
    MOZ_ASSERT(isComputed());
    return optimizedBackend_;
  }
  DebugEnabled debug() const {
    MOZ_ASSERT(isComputed());
    return debug_;
  }
  bool gcTypes() const {
    MOZ_ASSERT(isComputed());
    return gcTypes_;
  }
  bool refTypes() const {
    MOZ_ASSERT(isComputed());
    return refTypes_;
  }
  bool multiValues() const {
    MOZ_ASSERT(isComputed());
    return multiValues_;
  }
  bool hugeMemory() const {
    MOZ_ASSERT(isComputed());
    return hugeMemory_;
  }
  bool bigInt() const {
    MOZ_ASSERT(isComputed());
    return bigInt_;
  }
};

// ModuleEnvironment contains all the state necessary to process or render
// functions, and all of the state necessary to validate all aspects of the
// functions.
//
// A ModuleEnvironment is created by decoding all the sections before the wasm
// code section and then used immutably during. When compiling a module using a
// ModuleGenerator, the ModuleEnvironment holds state shared between the
// ModuleGenerator thread and background compile threads. All the threads
// are given a read-only view of the ModuleEnvironment, thus preventing race
// conditions.

struct ModuleEnvironment {
  // Constant parameters for the entire compilation:
  const ModuleKind kind;
  const Shareable sharedMemoryEnabled;
  CompilerEnvironment* const compilerEnv;

  // Module fields decoded from the module environment (or initialized while
  // validating an asm.js module) and immutable during compilation:
  Maybe<uint32_t> dataCount;
  MemoryUsage memoryUsage;
  uint32_t minMemoryLength;
  Maybe<uint32_t> maxMemoryLength;
  uint32_t numStructTypes;
  TypeDefVector types;
  FuncTypeWithIdPtrVector funcTypes;
  Uint32Vector funcImportGlobalDataOffsets;
  GlobalDescVector globals;
  TableDescVector tables;
  Uint32Vector asmJSSigToTableIndex;
  ImportVector imports;
  ExportVector exports;
  Maybe<uint32_t> startFuncIndex;
  ElemSegmentVector elemSegments;
  MaybeSectionRange codeSection;
  SparseBitmap validForRefFunc;

  // Fields decoded as part of the wasm module tail:
  DataSegmentEnvVector dataSegments;
  CustomSectionEnvVector customSections;
  Maybe<uint32_t> nameCustomSectionIndex;
  Maybe<Name> moduleName;
  NameVector funcNames;

  explicit ModuleEnvironment(CompilerEnvironment* compilerEnv,
                             Shareable sharedMemoryEnabled,
                             ModuleKind kind = ModuleKind::Wasm)
      : kind(kind),
        sharedMemoryEnabled(sharedMemoryEnabled),
        compilerEnv(compilerEnv),
        memoryUsage(MemoryUsage::None),
        minMemoryLength(0),
        numStructTypes(0) {}

  Tier tier() const { return compilerEnv->tier(); }
  OptimizedBackend optimizedBackend() const {
    return compilerEnv->optimizedBackend();
  }
  CompileMode mode() const { return compilerEnv->mode(); }
  DebugEnabled debug() const { return compilerEnv->debug(); }
  size_t numTables() const { return tables.length(); }
  size_t numTypes() const { return types.length(); }
  size_t numFuncs() const { return funcTypes.length(); }
  size_t numFuncImports() const { return funcImportGlobalDataOffsets.length(); }
  size_t numFuncDefs() const {
    return funcTypes.length() - funcImportGlobalDataOffsets.length();
  }
  bool gcTypesEnabled() const { return compilerEnv->gcTypes(); }
  bool refTypesEnabled() const { return compilerEnv->refTypes(); }
  bool multiValuesEnabled() const { return compilerEnv->multiValues(); }
  bool bigIntEnabled() const { return compilerEnv->bigInt(); }
  bool usesMemory() const { return memoryUsage != MemoryUsage::None; }
  bool usesSharedMemory() const { return memoryUsage == MemoryUsage::Shared; }
  bool isAsmJS() const { return kind == ModuleKind::AsmJS; }
  bool debugEnabled() const {
    return compilerEnv->debug() == DebugEnabled::True;
  }
  bool hugeMemoryEnabled() const {
    return !isAsmJS() && compilerEnv->hugeMemory();
  }
  uint32_t funcMaxResults() const {
    return multiValuesEnabled() ? MaxResults : 1;
  }
  bool funcIsImport(uint32_t funcIndex) const {
    return funcIndex < funcImportGlobalDataOffsets.length();
  }
  bool isRefSubtypeOf(ValType one, ValType two) const {
    MOZ_ASSERT(one.isReference());
    MOZ_ASSERT(two.isReference());
    // Anything's a subtype of itself.
    if (one == two) {
      return true;
    }
    // Anything's a subtype of AnyRef.
    if (two.isAnyRef()) {
      return true;
    }
    // NullRef is a subtype of nullable types.
    if (one.isNullRef()) {
      return two.isNullable();
    }
#if defined(ENABLE_WASM_GC)
    // Struct One is a subtype of struct Two if Two is a prefix of One.
    if (gcTypesEnabled() && isStructType(one) && isStructType(two)) {
      return isStructPrefixOf(two, one);
    }
#endif
    return false;
  }
  bool isStructType(ValType t) const {
    return t.isTypeIndex() && types[t.refType().typeIndex()].isStructType();
  }

 private:
  bool isStructPrefixOf(ValType a, ValType b) const {
    const StructType& other = types[a.refType().typeIndex()].structType();
    return types[b.refType().typeIndex()].structType().hasPrefix(other);
  }
};

// ElemSegmentFlags provides methods for decoding and encoding the flags field
// of an element segment. This is needed as the flags field has a non-trivial
// encoding that is effectively split into independent `kind` and `payload`
// enums.
class ElemSegmentFlags {
  enum class Flags : uint32_t {
    Passive = 0x1,
    WithIndexOrDeclared = 0x2,
    ElemExpression = 0x4,
    // Below this line are convenient combinations of flags
    KindMask = Passive | WithIndexOrDeclared,
    PayloadMask = ElemExpression,
    AllFlags = Passive | WithIndexOrDeclared | ElemExpression,
  };
  uint32_t encoded_;

  explicit ElemSegmentFlags(uint32_t encoded) : encoded_(encoded) {}

 public:
  ElemSegmentFlags(ElemSegmentKind kind, ElemSegmentPayload payload) {
    encoded_ = uint32_t(kind) | uint32_t(payload);
  }

  static Maybe<ElemSegmentFlags> construct(uint32_t encoded) {
    if (encoded > uint32_t(Flags::AllFlags)) {
      return Nothing();
    }
    return Some(ElemSegmentFlags(encoded));
  }

  uint32_t encoded() const { return encoded_; }

  ElemSegmentKind kind() const {
    return static_cast<ElemSegmentKind>(encoded_ & uint32_t(Flags::KindMask));
  }
  ElemSegmentPayload payload() const {
    return static_cast<ElemSegmentPayload>(encoded_ &
                                           uint32_t(Flags::PayloadMask));
  }
};

// The Encoder class appends bytes to the Bytes object it is given during
// construction. The client is responsible for the Bytes's lifetime and must
// keep the Bytes alive as long as the Encoder is used.

class Encoder {
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
      if (i != 0) {
        byte |= 0x80;
      }
      if (!bytes_.append(byte)) {
        return false;
      }
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
      if (!done) {
        byte |= 0x80;
      }
      if (!bytes_.append(byte)) {
        return false;
      }
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
    } while (assertBits != 0);
  }

  void patchFixedU7(size_t offset, uint8_t patchBits, uint8_t assertBits) {
    MOZ_ASSERT(patchBits <= uint8_t(INT8_MAX));
    patchFixedU8(offset, patchBits, assertBits);
  }

  void patchFixedU8(size_t offset, uint8_t patchBits, uint8_t assertBits) {
    MOZ_ASSERT(bytes_[offset] == assertBits);
    bytes_[offset] = patchBits;
  }

  uint32_t varU32ByteLength(size_t offset) const {
    size_t start = offset;
    while (bytes_[offset] & 0x80) {
      offset++;
    }
    return offset - start + 1;
  }

 public:
  explicit Encoder(Bytes& bytes) : bytes_(bytes) { MOZ_ASSERT(empty()); }

  size_t currentOffset() const { return bytes_.length(); }
  bool empty() const { return currentOffset() == 0; }

  // Fixed-size encoding operations simply copy the literal bytes (without
  // attempting to align).

  MOZ_MUST_USE bool writeFixedU7(uint8_t i) {
    MOZ_ASSERT(i <= uint8_t(INT8_MAX));
    return writeFixedU8(i);
  }
  MOZ_MUST_USE bool writeFixedU8(uint8_t i) { return write<uint8_t>(i); }
  MOZ_MUST_USE bool writeFixedU32(uint32_t i) { return write<uint32_t>(i); }
  MOZ_MUST_USE bool writeFixedF32(float f) { return write<float>(f); }
  MOZ_MUST_USE bool writeFixedF64(double d) { return write<double>(d); }

  // Variable-length encodings that all use LEB128.

  MOZ_MUST_USE bool writeVarU32(uint32_t i) { return writeVarU<uint32_t>(i); }
  MOZ_MUST_USE bool writeVarS32(int32_t i) { return writeVarS<int32_t>(i); }
  MOZ_MUST_USE bool writeVarU64(uint64_t i) { return writeVarU<uint64_t>(i); }
  MOZ_MUST_USE bool writeVarS64(int64_t i) { return writeVarS<int64_t>(i); }
  MOZ_MUST_USE bool writeValType(ValType type) {
    static_assert(size_t(TypeCode::Limit) <= UINT8_MAX, "fits");
    if (type.isTypeIndex()) {
      return writeFixedU8(uint8_t(TypeCode::OptRef)) &&
             writeVarU32(type.refType().typeIndex());
    }
    TypeCode tc = UnpackTypeCodeType(type.packed());
    MOZ_ASSERT(size_t(tc) < size_t(TypeCode::Limit));
    return writeFixedU8(uint8_t(tc));
  }
  MOZ_MUST_USE bool writeOp(Opcode opcode) {
    // The Opcode constructor has asserted that `opcode` is meaningful, so no
    // further correctness checking is necessary here.
    uint32_t bits = opcode.bits();
    if (!writeFixedU8(bits & 255)) {
      return false;
    }
    if (opcode.isOp()) {
      return true;
    }
    return writeVarU32(bits >> 8);
  }

  // Fixed-length encodings that allow back-patching.

  MOZ_MUST_USE bool writePatchableFixedU7(size_t* offset) {
    *offset = bytes_.length();
    return writeFixedU8(UINT8_MAX);
  }
  void patchFixedU7(size_t offset, uint8_t patchBits) {
    return patchFixedU7(offset, patchBits, UINT8_MAX);
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
  // that it may be trivially skipped without examining the payload. Sections
  // require backpatching since the size of the section is only known at the
  // end while the size's varU32 must be stored at the beginning. Immediately
  // after the section length is the string id of the section.

  MOZ_MUST_USE bool startSection(SectionId id, size_t* offset) {
    MOZ_ASSERT(uint32_t(id) < 128);
    return writeVarU32(uint32_t(id)) && writePatchableVarU32(offset);
  }
  void finishSection(size_t offset) {
    return patchVarU32(offset,
                       bytes_.length() - offset - varU32ByteLength(offset));
  }
};

// The Decoder class decodes the bytes in the range it is given during
// construction. The client is responsible for keeping the byte range alive as
// long as the Decoder is used.

class Decoder {
  const uint8_t* const beg_;
  const uint8_t* const end_;
  const uint8_t* cur_;
  const size_t offsetInModule_;
  UniqueChars* error_;
  UniqueCharsVector* warnings_;
  bool resilientMode_;

  template <class T>
  MOZ_MUST_USE bool read(T* out) {
    if (bytesRemain() < sizeof(T)) {
      return false;
    }
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
    DebugOnly<const uint8_t*> before = cur_;
    const unsigned numBits = sizeof(UInt) * CHAR_BIT;
    const unsigned remainderBits = numBits % 7;
    const unsigned numBitsInSevens = numBits - remainderBits;
    UInt u = 0;
    uint8_t byte;
    UInt shift = 0;
    do {
      if (!readFixedU8(&byte)) {
        return false;
      }
      if (!(byte & 0x80)) {
        *out = u | UInt(byte) << shift;
        return true;
      }
      u |= UInt(byte & 0x7F) << shift;
      shift += 7;
    } while (shift != numBitsInSevens);
    if (!readFixedU8(&byte) || (byte & (unsigned(-1) << remainderBits))) {
      return false;
    }
    *out = u | (UInt(byte) << numBitsInSevens);
    MOZ_ASSERT_IF(sizeof(UInt) == 4,
                  unsigned(cur_ - before) <= MaxVarU32DecodedBytes);
    return true;
  }

  template <typename SInt>
  MOZ_MUST_USE bool readVarS(SInt* out) {
    using UInt = std::make_unsigned_t<SInt>;
    const unsigned numBits = sizeof(SInt) * CHAR_BIT;
    const unsigned remainderBits = numBits % 7;
    const unsigned numBitsInSevens = numBits - remainderBits;
    SInt s = 0;
    uint8_t byte;
    unsigned shift = 0;
    do {
      if (!readFixedU8(&byte)) {
        return false;
      }
      s |= SInt(byte & 0x7f) << shift;
      shift += 7;
      if (!(byte & 0x80)) {
        if (byte & 0x40) {
          s |= UInt(-1) << shift;
        }
        *out = s;
        return true;
      }
    } while (shift < numBitsInSevens);
    if (!remainderBits || !readFixedU8(&byte) || (byte & 0x80)) {
      return false;
    }
    uint8_t mask = 0x7f & (uint8_t(-1) << remainderBits);
    if ((byte & mask) != ((byte & (1 << (remainderBits - 1))) ? mask : 0)) {
      return false;
    }
    *out = s | UInt(byte) << shift;
    return true;
  }

 public:
  Decoder(const uint8_t* begin, const uint8_t* end, size_t offsetInModule,
          UniqueChars* error, UniqueCharsVector* warnings = nullptr,
          bool resilientMode = false)
      : beg_(begin),
        end_(end),
        cur_(begin),
        offsetInModule_(offsetInModule),
        error_(error),
        warnings_(warnings),
        resilientMode_(resilientMode) {
    MOZ_ASSERT(begin <= end);
  }
  explicit Decoder(const Bytes& bytes, size_t offsetInModule = 0,
                   UniqueChars* error = nullptr,
                   UniqueCharsVector* warnings = nullptr)
      : beg_(bytes.begin()),
        end_(bytes.end()),
        cur_(bytes.begin()),
        offsetInModule_(offsetInModule),
        error_(error),
        warnings_(warnings),
        resilientMode_(false) {}

  // These convenience functions use currentOffset() as the errorOffset.
  bool fail(const char* msg) { return fail(currentOffset(), msg); }
  bool failf(const char* msg, ...) MOZ_FORMAT_PRINTF(2, 3);
  void warnf(const char* msg, ...) MOZ_FORMAT_PRINTF(2, 3);

  // Report an error at the given offset (relative to the whole module).
  bool fail(size_t errorOffset, const char* msg);

  UniqueChars* error() { return error_; }

  void clearError() {
    if (error_) {
      error_->reset();
    }
  }

  bool done() const {
    MOZ_ASSERT(cur_ <= end_);
    return cur_ == end_;
  }
  bool resilientMode() const { return resilientMode_; }

  size_t bytesRemain() const {
    MOZ_ASSERT(end_ >= cur_);
    return size_t(end_ - cur_);
  }
  // pos must be a value previously returned from currentPosition.
  void rollbackPosition(const uint8_t* pos) { cur_ = pos; }
  const uint8_t* currentPosition() const { return cur_; }
  size_t currentOffset() const { return offsetInModule_ + (cur_ - beg_); }
  const uint8_t* begin() const { return beg_; }
  const uint8_t* end() const { return end_; }

  // Peek at the next byte, if it exists, without advancing the position.

  bool peekByte(uint8_t* byte) {
    if (done()) {
      return false;
    }
    *byte = *cur_;
    return true;
  }

  // Fixed-size encoding operations simply copy the literal bytes (without
  // attempting to align).

  MOZ_MUST_USE bool readFixedU8(uint8_t* i) { return read<uint8_t>(i); }
  MOZ_MUST_USE bool readFixedU32(uint32_t* u) { return read<uint32_t>(u); }
  MOZ_MUST_USE bool readFixedF32(float* f) { return read<float>(f); }
  MOZ_MUST_USE bool readFixedF64(double* d) { return read<double>(d); }

  // Variable-length encodings that all use LEB128.

  MOZ_MUST_USE bool readVarU32(uint32_t* out) {
    return readVarU<uint32_t>(out);
  }
  MOZ_MUST_USE bool readVarS32(int32_t* out) { return readVarS<int32_t>(out); }
  MOZ_MUST_USE bool readVarU64(uint64_t* out) {
    return readVarU<uint64_t>(out);
  }
  MOZ_MUST_USE bool readVarS64(int64_t* out) { return readVarS<int64_t>(out); }

  MOZ_MUST_USE ValType uncheckedReadValType() {
    uint8_t code = uncheckedReadFixedU8();
    switch (code) {
      case uint8_t(TypeCode::OptRef):
        return RefType::fromTypeIndex(uncheckedReadVarU32());
      case uint8_t(TypeCode::AnyRef):
      case uint8_t(TypeCode::FuncRef):
      case uint8_t(TypeCode::NullRef):
        return RefType::fromTypeCode(TypeCode(code));
      default:
        return ValType::fromNonRefTypeCode(TypeCode(code));
    }
  }
  MOZ_MUST_USE bool readValType(uint32_t numTypes, bool refTypesEnabled,
                                bool gcTypesEnabled, ValType* type) {
    static_assert(uint8_t(TypeCode::Limit) <= UINT8_MAX, "fits");
    uint8_t code;
    if (!readFixedU8(&code)) {
      return false;
    }
    switch (code) {
      case uint8_t(TypeCode::I32):
      case uint8_t(TypeCode::F32):
      case uint8_t(TypeCode::F64):
      case uint8_t(TypeCode::I64):
        *type = ValType::fromNonRefTypeCode(TypeCode(code));
        return true;
#ifdef ENABLE_WASM_REFTYPES
      case uint8_t(TypeCode::FuncRef):
      case uint8_t(TypeCode::AnyRef):
      case uint8_t(TypeCode::NullRef):
        if (!refTypesEnabled) {
          return fail("reference types not enabled");
        }
        *type = RefType::fromTypeCode(TypeCode(code));
        return true;
#  ifdef ENABLE_WASM_GC
      case uint8_t(TypeCode::OptRef): {
        if (!gcTypesEnabled) {
          return fail("(optref T) types not enabled");
        }
        uint32_t typeIndex;
        if (!readVarU32(&typeIndex)) {
          return false;
        }
        if (typeIndex >= numTypes) {
          return fail("ref index out of range");
        }
        *type = RefType::fromTypeIndex(typeIndex);
        return true;
      }
#  endif
#endif
      default:
        return fail("bad type");
    }
  }
  MOZ_MUST_USE bool readValType(const TypeDefVector& types,
                                bool refTypesEnabled, bool gcTypesEnabled,
                                ValType* type) {
    if (!readValType(types.length(), refTypesEnabled, gcTypesEnabled, type)) {
      return false;
    }
    if (type->isTypeIndex() &&
        !types[type->refType().typeIndex()].isStructType()) {
      return fail("type index does not reference a struct type");
    }
    return true;
  }
  MOZ_MUST_USE bool readOp(OpBytes* op) {
    static_assert(size_t(Op::Limit) == 256, "fits");
    uint8_t u8;
    if (!readFixedU8(&u8)) {
      return false;
    }
    op->b0 = u8;
    if (MOZ_LIKELY(!IsPrefixByte(u8))) {
      return true;
    }
    if (!readFixedU8(&u8)) {
      op->b1 = 0;  // Make it sane
      return false;
    }
    op->b1 = u8;
    return true;
  }

  // See writeBytes comment.

  MOZ_MUST_USE bool readBytes(uint32_t numBytes,
                              const uint8_t** bytes = nullptr) {
    if (bytes) {
      *bytes = cur_;
    }
    if (bytesRemain() < numBytes) {
      return false;
    }
    cur_ += numBytes;
    return true;
  }

  // See "section" description in Encoder.

  MOZ_MUST_USE bool readSectionHeader(uint8_t* id, SectionRange* range);

  MOZ_MUST_USE bool startSection(SectionId id, ModuleEnvironment* env,
                                 MaybeSectionRange* range,
                                 const char* sectionName);
  MOZ_MUST_USE bool finishSection(const SectionRange& range,
                                  const char* sectionName);

  // Custom sections do not cause validation errors unless the error is in
  // the section header itself.

  MOZ_MUST_USE bool startCustomSection(const char* expected,
                                       size_t expectedLength,
                                       ModuleEnvironment* env,
                                       MaybeSectionRange* range);

  template <size_t NameSizeWith0>
  MOZ_MUST_USE bool startCustomSection(const char (&name)[NameSizeWith0],
                                       ModuleEnvironment* env,
                                       MaybeSectionRange* range) {
    MOZ_ASSERT(name[NameSizeWith0 - 1] == '\0');
    return startCustomSection(name, NameSizeWith0 - 1, env, range);
  }

  void finishCustomSection(const char* name, const SectionRange& range);
  void skipAndFinishCustomSection(const SectionRange& range);

  MOZ_MUST_USE bool skipCustomSection(ModuleEnvironment* env);

  // The Name section has its own optional subsections.

  MOZ_MUST_USE bool startNameSubsection(NameType nameType,
                                        Maybe<uint32_t>* endOffset);
  MOZ_MUST_USE bool finishNameSubsection(uint32_t endOffset);
  MOZ_MUST_USE bool skipNameSubsection();

  // The infallible "unchecked" decoding functions can be used when we are
  // sure that the bytes are well-formed (by construction or due to previous
  // validation).

  uint8_t uncheckedReadFixedU8() { return uncheckedRead<uint8_t>(); }
  uint32_t uncheckedReadFixedU32() { return uncheckedRead<uint32_t>(); }
  void uncheckedReadFixedF32(float* out) { uncheckedRead<float>(out); }
  void uncheckedReadFixedF64(double* out) { uncheckedRead<double>(out); }
  template <typename UInt>
  UInt uncheckedReadVarU() {
    static const unsigned numBits = sizeof(UInt) * CHAR_BIT;
    static const unsigned remainderBits = numBits % 7;
    static const unsigned numBitsInSevens = numBits - remainderBits;
    UInt decoded = 0;
    uint32_t shift = 0;
    do {
      uint8_t byte = *cur_++;
      if (!(byte & 0x80)) {
        return decoded | (UInt(byte) << shift);
      }
      decoded |= UInt(byte & 0x7f) << shift;
      shift += 7;
    } while (shift != numBitsInSevens);
    uint8_t byte = *cur_++;
    MOZ_ASSERT(!(byte & 0xf0));
    return decoded | (UInt(byte) << numBitsInSevens);
  }
  uint32_t uncheckedReadVarU32() { return uncheckedReadVarU<uint32_t>(); }
  int32_t uncheckedReadVarS32() {
    int32_t i32 = 0;
    MOZ_ALWAYS_TRUE(readVarS32(&i32));
    return i32;
  }
  uint64_t uncheckedReadVarU64() { return uncheckedReadVarU<uint64_t>(); }
  int64_t uncheckedReadVarS64() {
    int64_t i64 = 0;
    MOZ_ALWAYS_TRUE(readVarS64(&i64));
    return i64;
  }
  Op uncheckedReadOp() {
    static_assert(size_t(Op::Limit) == 256, "fits");
    uint8_t u8 = uncheckedReadFixedU8();
    return u8 != UINT8_MAX ? Op(u8) : Op(uncheckedReadFixedU8() + UINT8_MAX);
  }
};

// The local entries are part of function bodies and thus serialized by both
// wasm and asm.js and decoded as part of both validation and compilation.

MOZ_MUST_USE bool EncodeLocalEntries(Encoder& d, const ValTypeVector& locals);

// This performs no validation; the local entries must already have been
// validated by an earlier pass.

MOZ_MUST_USE bool DecodeValidatedLocalEntries(Decoder& d,
                                              ValTypeVector* locals);

// This validates the entries.

MOZ_MUST_USE bool DecodeLocalEntries(Decoder& d, const TypeDefVector& types,
                                     bool refTypesEnabled, bool gcTypesEnabled,
                                     ValTypeVector* locals);

// Returns whether the given [begin, end) prefix of a module's bytecode starts a
// code section and, if so, returns the SectionRange of that code section.
// Note that, even if this function returns 'false', [begin, end) may actually
// be a valid module in the special case when there are no function defs and the
// code section is not present. Such modules can be valid so the caller must
// handle this special case.

MOZ_MUST_USE bool StartsCodeSection(const uint8_t* begin, const uint8_t* end,
                                    SectionRange* range);

// Calling DecodeModuleEnvironment decodes all sections up to the code section
// and performs full validation of all those sections. The client must then
// decode the code section itself, reusing ValidateFunctionBody if necessary,
// and finally call DecodeModuleTail to decode all remaining sections after the
// code section (again, performing full validation).

MOZ_MUST_USE bool DecodeModuleEnvironment(Decoder& d, ModuleEnvironment* env);

MOZ_MUST_USE bool ValidateFunctionBody(const ModuleEnvironment& env,
                                       uint32_t funcIndex, uint32_t bodySize,
                                       Decoder& d);

MOZ_MUST_USE bool DecodeModuleTail(Decoder& d, ModuleEnvironment* env);

void ConvertMemoryPagesToBytes(Limits* memory);

// Validate an entire module, returning true if the module was validated
// successfully. If Validate returns false:
//  - if *error is null, the caller should report out-of-memory
//  - otherwise, there was a legitimate error described by *error

MOZ_MUST_USE bool Validate(JSContext* cx, const ShareableBytes& bytecode,
                           UniqueChars* error);

}  // namespace wasm
}  // namespace js

#endif  // namespace wasm_validate_h
