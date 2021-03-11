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

#include "wasm/WasmCompile.h"
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
                      DebugEnabled debugEnabled);

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
  bool debugEnabled() const { return debug() == DebugEnabled::True; }
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
  const FeatureArgs features;

  // Module fields decoded from the module environment (or initialized while
  // validating an asm.js module) and immutable during compilation:
  Maybe<uint32_t> dataCount;
  MemoryUsage memoryUsage;
  uint64_t minMemoryLength;
  Maybe<uint64_t> maxMemoryLength;
  TypeContext types;
  TypeIdDescVector typeIds;
  FuncDescVector funcs;
  Uint32Vector funcImportGlobalDataOffsets;

  GlobalDescVector globals;
#ifdef ENABLE_WASM_EXCEPTIONS
  EventDescVector events;
#endif
  TableDescVector tables;
  Uint32Vector asmJSSigToTableIndex;
  ImportVector imports;
  ExportVector exports;
  Maybe<uint32_t> startFuncIndex;
  ElemSegmentVector elemSegments;
  MaybeSectionRange codeSection;
  SparseBitmap validForRefFunc;
  bool usesDuplicateImports;

  // Fields decoded as part of the wasm module tail:
  DataSegmentEnvVector dataSegments;
  CustomSectionEnvVector customSections;
  Maybe<uint32_t> nameCustomSectionIndex;
  Maybe<Name> moduleName;
  NameVector funcNames;

  explicit ModuleEnvironment(FeatureArgs features,
                             ModuleKind kind = ModuleKind::Wasm)
      : kind(kind),
        features(features),
        memoryUsage(MemoryUsage::None),
        minMemoryLength(0),
        types(features, TypeDefVector()),
        usesDuplicateImports(false) {}

  size_t numTables() const { return tables.length(); }
  size_t numTypes() const { return types.length(); }
  size_t numFuncs() const { return funcs.length(); }
  size_t numFuncImports() const { return funcImportGlobalDataOffsets.length(); }
  size_t numFuncDefs() const {
    return funcs.length() - funcImportGlobalDataOffsets.length();
  }
  Shareable sharedMemoryEnabled() const { return features.sharedMemory; }
  bool refTypesEnabled() const { return features.refTypes; }
  bool functionReferencesEnabled() const { return features.functionReferences; }
  bool gcTypesEnabled() const { return features.gcTypes; }
  bool multiValueEnabled() const { return features.multiValue; }
  bool v128Enabled() const { return features.v128; }
  bool simdWormholeEnabled() const { return features.simdWormhole; }
  bool hugeMemoryEnabled() const { return !isAsmJS() && features.hugeMemory; }
  bool exceptionsEnabled() const { return features.exceptions; }
  bool usesMemory() const { return memoryUsage != MemoryUsage::None; }
  bool usesSharedMemory() const { return memoryUsage == MemoryUsage::Shared; }
  bool isAsmJS() const { return kind == ModuleKind::AsmJS; }

  uint32_t funcMaxResults() const {
    return multiValueEnabled() ? MaxResults : 1;
  }
  bool funcIsImport(uint32_t funcIndex) const {
    return funcIndex < funcImportGlobalDataOffsets.length();
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
  [[nodiscard]] bool write(const T& v) {
    return bytes_.append(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
  }

  template <typename UInt>
  [[nodiscard]] bool writeVarU(UInt i) {
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
  [[nodiscard]] bool writeVarS(SInt i) {
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

  [[nodiscard]] bool writeFixedU7(uint8_t i) {
    MOZ_ASSERT(i <= uint8_t(INT8_MAX));
    return writeFixedU8(i);
  }
  [[nodiscard]] bool writeFixedU8(uint8_t i) { return write<uint8_t>(i); }
  [[nodiscard]] bool writeFixedU32(uint32_t i) { return write<uint32_t>(i); }
  [[nodiscard]] bool writeFixedF32(float f) { return write<float>(f); }
  [[nodiscard]] bool writeFixedF64(double d) { return write<double>(d); }

  // Variable-length encodings that all use LEB128.

  [[nodiscard]] bool writeVarU32(uint32_t i) { return writeVarU<uint32_t>(i); }
  [[nodiscard]] bool writeVarS32(int32_t i) { return writeVarS<int32_t>(i); }
  [[nodiscard]] bool writeVarU64(uint64_t i) { return writeVarU<uint64_t>(i); }
  [[nodiscard]] bool writeVarS64(int64_t i) { return writeVarS<int64_t>(i); }
  [[nodiscard]] bool writeValType(ValType type) {
    static_assert(size_t(TypeCode::Limit) <= UINT8_MAX, "fits");
    if (type.isTypeIndex()) {
      return writeFixedU8(uint8_t(TypeCode::NullableRef)) &&
             writeVarU32(type.refType().typeIndex());
    }
    TypeCode tc = type.packed().typeCode();
    MOZ_ASSERT(size_t(tc) < size_t(TypeCode::Limit));
    return writeFixedU8(uint8_t(tc));
  }
  [[nodiscard]] bool writeOp(Opcode opcode) {
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

  [[nodiscard]] bool writePatchableFixedU7(size_t* offset) {
    *offset = bytes_.length();
    return writeFixedU8(UINT8_MAX);
  }
  void patchFixedU7(size_t offset, uint8_t patchBits) {
    return patchFixedU7(offset, patchBits, UINT8_MAX);
  }

  // Variable-length encodings that allow back-patching.

  [[nodiscard]] bool writePatchableVarU32(size_t* offset) {
    *offset = bytes_.length();
    return writeVarU32(UINT32_MAX);
  }
  void patchVarU32(size_t offset, uint32_t patchBits) {
    return patchVarU32(offset, patchBits, UINT32_MAX);
  }

  // Byte ranges start with an LEB128 length followed by an arbitrary sequence
  // of bytes. When used for strings, bytes are to be interpreted as utf8.

  [[nodiscard]] bool writeBytes(const void* bytes, uint32_t numBytes) {
    return writeVarU32(numBytes) &&
           bytes_.append(reinterpret_cast<const uint8_t*>(bytes), numBytes);
  }

  // A "section" is a contiguous range of bytes that stores its own size so
  // that it may be trivially skipped without examining the payload. Sections
  // require backpatching since the size of the section is only known at the
  // end while the size's varU32 must be stored at the beginning. Immediately
  // after the section length is the string id of the section.

  [[nodiscard]] bool startSection(SectionId id, size_t* offset) {
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
  [[nodiscard]] bool read(T* out) {
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
  [[nodiscard]] bool readVarU(UInt* out) {
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
  [[nodiscard]] bool readVarS(SInt* out) {
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

  [[nodiscard]] bool readFixedU8(uint8_t* i) { return read<uint8_t>(i); }
  [[nodiscard]] bool readFixedU32(uint32_t* u) { return read<uint32_t>(u); }
  [[nodiscard]] bool readFixedF32(float* f) { return read<float>(f); }
  [[nodiscard]] bool readFixedF64(double* d) { return read<double>(d); }
#ifdef ENABLE_WASM_SIMD
  [[nodiscard]] bool readFixedV128(V128* d) {
    for (unsigned i = 0; i < 16; i++) {
      if (!read<uint8_t>(d->bytes + i)) {
        return false;
      }
    }
    return true;
  }
#endif

  // Variable-length encodings that all use LEB128.

  [[nodiscard]] bool readVarU32(uint32_t* out) {
    return readVarU<uint32_t>(out);
  }
  [[nodiscard]] bool readVarS32(int32_t* out) { return readVarS<int32_t>(out); }
  [[nodiscard]] bool readVarU64(uint64_t* out) {
    return readVarU<uint64_t>(out);
  }
  [[nodiscard]] bool readVarS64(int64_t* out) { return readVarS<int64_t>(out); }

  [[nodiscard]] ValType uncheckedReadValType() {
    uint8_t code = uncheckedReadFixedU8();
    switch (code) {
      case uint8_t(TypeCode::FuncRef):
      case uint8_t(TypeCode::ExternRef):
        return RefType::fromTypeCode(TypeCode(code), true);
      case uint8_t(TypeCode::Rtt): {
        uint32_t rttDepth = uncheckedReadVarU32();
        int32_t typeIndex = uncheckedReadVarS32();
        return ValType::fromRtt(typeIndex, rttDepth);
      }
      case uint8_t(TypeCode::Ref):
      case uint8_t(TypeCode::NullableRef): {
        bool nullable = code == uint8_t(TypeCode::NullableRef);

        uint8_t nextByte;
        peekByte(&nextByte);

        if ((nextByte & SLEB128SignMask) == SLEB128SignBit) {
          uint8_t code = uncheckedReadFixedU8();
          return RefType::fromTypeCode(TypeCode(code), nullable);
        }

        int32_t x = uncheckedReadVarS32();
        return RefType::fromTypeIndex(x, nullable);
      }
      default:
        return ValType::fromNonRefTypeCode(TypeCode(code));
    }
  }

  template <class T>
  [[nodiscard]] bool readPackedType(uint32_t numTypes,
                                    const FeatureArgs& features, T* type) {
    static_assert(uint8_t(TypeCode::Limit) <= UINT8_MAX, "fits");
    uint8_t code;
    if (!readFixedU8(&code)) {
      return fail("expected type code");
    }
    switch (code) {
      case uint8_t(TypeCode::V128): {
#ifdef ENABLE_WASM_SIMD
        if (!features.v128) {
          return fail("v128 not enabled");
        }
        *type = T::fromNonRefTypeCode(TypeCode(code));
        return true;
#else
        break;
#endif
      }
      case uint8_t(TypeCode::FuncRef):
      case uint8_t(TypeCode::ExternRef): {
#ifdef ENABLE_WASM_REFTYPES
        if (!features.refTypes) {
          return fail("reference types not enabled");
        }
        *type = RefType::fromTypeCode(TypeCode(code), true);
        return true;
#else
        break;
#endif
      }
      case uint8_t(TypeCode::Ref):
      case uint8_t(TypeCode::NullableRef): {
#ifdef ENABLE_WASM_FUNCTION_REFERENCES
        if (!features.functionReferences) {
          return fail("(ref T) types not enabled");
        }
        bool nullable = code == uint8_t(TypeCode::NullableRef);
        RefType refType;
        if (!readHeapType(numTypes, features, nullable, &refType)) {
          return false;
        }
        *type = refType;
        return true;
#else
        break;
#endif
      }
      case uint8_t(TypeCode::Rtt): {
#ifdef ENABLE_WASM_GC
        if (!features.gcTypes) {
          return fail("gc types not enabled");
        }

        uint32_t rttDepth;
        if (!readVarU32(&rttDepth) || uint32_t(rttDepth) >= MaxRttDepth) {
          return fail("invalid rtt depth");
        }

        RefType heapType;
        if (!readHeapType(numTypes, features, true, &heapType)) {
          return false;
        }

        if (!heapType.isTypeIndex()) {
          return fail("invalid heap type for rtt");
        }

        *type = T::fromRtt(heapType.typeIndex(), rttDepth);
        return true;
#else
        break;
#endif
      }
      case uint8_t(TypeCode::EqRef): {
#ifdef ENABLE_WASM_GC
        if (!features.gcTypes) {
          return fail("gc types not enabled");
        }
        *type = RefType::fromTypeCode(TypeCode(code), true);
        return true;
#else
        break;
#endif
      }
      default: {
        if (!T::isValidTypeCode(TypeCode(code))) {
          break;
        }
        *type = T::fromNonRefTypeCode(TypeCode(code));
        return true;
      }
    }
    return fail("bad type");
  }
  template <class T>
  [[nodiscard]] bool readPackedType(const TypeContext& types,
                                    const FeatureArgs& features, T* type) {
    if (!readPackedType(types.length(), features, type)) {
      return false;
    }
    if (type->isTypeIndex() &&
        !validateTypeIndex(types, features, type->refType())) {
      return false;
    }
    return true;
  }

  [[nodiscard]] bool readValType(uint32_t numTypes, const FeatureArgs& features,
                                 ValType* type) {
    return readPackedType<ValType>(numTypes, features, type);
  }
  [[nodiscard]] bool readValType(const TypeContext& types,
                                 const FeatureArgs& features, ValType* type) {
    return readPackedType<ValType>(types, features, type);
  }

  [[nodiscard]] bool readFieldType(uint32_t numTypes,
                                   const FeatureArgs& features,
                                   FieldType* type) {
    return readPackedType<FieldType>(numTypes, features, type);
  }
  [[nodiscard]] bool readFieldType(const TypeContext& types,
                                   const FeatureArgs& features,
                                   FieldType* type) {
    return readPackedType<FieldType>(types, features, type);
  }

  [[nodiscard]] bool readHeapType(uint32_t numTypes,
                                  const FeatureArgs& features, bool nullable,
                                  RefType* type) {
    uint8_t nextByte;
    if (!peekByte(&nextByte)) {
      return fail("expected heap type code");
    }

    if ((nextByte & SLEB128SignMask) == SLEB128SignBit) {
      uint8_t code;
      if (!readFixedU8(&code)) {
        return false;
      }

      switch (code) {
        case uint8_t(TypeCode::FuncRef):
        case uint8_t(TypeCode::ExternRef):
          *type = RefType::fromTypeCode(TypeCode(code), nullable);
          return true;
#ifdef ENABLE_WASM_GC
        case uint8_t(TypeCode::EqRef):
          if (!features.gcTypes) {
            return fail("gc types not enabled");
          }
          *type = RefType::fromTypeCode(TypeCode(code), nullable);
          return true;
#endif
        default:
          return fail("invalid heap type");
      }
    }

#ifdef ENABLE_WASM_FUNCTION_REFERENCES
    if (features.functionReferences) {
      int32_t x;
      if (!readVarS32(&x) || x < 0 || uint32_t(x) >= numTypes ||
          uint32_t(x) >= MaxTypeIndex) {
        return fail("invalid heap type index");
      }
      *type = RefType::fromTypeIndex(x, nullable);
      return true;
    }
#endif
    return fail("invalid heap type");
  }
  [[nodiscard]] bool readHeapType(const TypeContext& types,
                                  const FeatureArgs& features, bool nullable,
                                  RefType* type) {
    if (!readHeapType(types.length(), features, nullable, type)) {
      return false;
    }

    if (type->isTypeIndex() && !validateTypeIndex(types, features, *type)) {
      return false;
    }
    return true;
  }
  [[nodiscard]] bool readRefType(uint32_t numTypes, const FeatureArgs& features,
                                 RefType* type) {
    ValType valType;
    if (!readValType(numTypes, features, &valType)) {
      return false;
    }
    if (!valType.isReference()) {
      return fail("bad type");
    }
    *type = valType.refType();
    return true;
  }
  [[nodiscard]] bool readRefType(const TypeContext& types,
                                 const FeatureArgs& features, RefType* type) {
    ValType valType;
    if (!readValType(types, features, &valType)) {
      return false;
    }
    if (!valType.isReference()) {
      return fail("bad type");
    }
    *type = valType.refType();
    return true;
  }
  [[nodiscard]] bool validateTypeIndex(const TypeContext& types,
                                       const FeatureArgs& features,
                                       RefType type) {
    MOZ_ASSERT(type.isTypeIndex());

    if (features.gcTypes && (types[type.typeIndex()].isStructType() ||
                             types[type.typeIndex()].isArrayType())) {
      return true;
    }
    return fail("type index references an invalid type");
  }
  [[nodiscard]] bool readOp(OpBytes* op) {
    static_assert(size_t(Op::Limit) == 256, "fits");
    uint8_t u8;
    if (!readFixedU8(&u8)) {
      return false;
    }
    op->b0 = u8;
    if (MOZ_LIKELY(!IsPrefixByte(u8))) {
      return true;
    }
    if (!readVarU32(&op->b1)) {
      return false;
    }
    return true;
  }

  // See writeBytes comment.

  [[nodiscard]] bool readBytes(uint32_t numBytes,
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

  [[nodiscard]] bool readSectionHeader(uint8_t* id, SectionRange* range);

  [[nodiscard]] bool startSection(SectionId id, ModuleEnvironment* env,
                                  MaybeSectionRange* range,
                                  const char* sectionName);
  [[nodiscard]] bool finishSection(const SectionRange& range,
                                   const char* sectionName);

  // Custom sections do not cause validation errors unless the error is in
  // the section header itself.

  [[nodiscard]] bool startCustomSection(const char* expected,
                                        size_t expectedLength,
                                        ModuleEnvironment* env,
                                        MaybeSectionRange* range);

  template <size_t NameSizeWith0>
  [[nodiscard]] bool startCustomSection(const char (&name)[NameSizeWith0],
                                        ModuleEnvironment* env,
                                        MaybeSectionRange* range) {
    MOZ_ASSERT(name[NameSizeWith0 - 1] == '\0');
    return startCustomSection(name, NameSizeWith0 - 1, env, range);
  }

  void finishCustomSection(const char* name, const SectionRange& range);
  void skipAndFinishCustomSection(const SectionRange& range);

  [[nodiscard]] bool skipCustomSection(ModuleEnvironment* env);

  // The Name section has its own optional subsections.

  [[nodiscard]] bool startNameSubsection(NameType nameType,
                                         Maybe<uint32_t>* endOffset);
  [[nodiscard]] bool finishNameSubsection(uint32_t endOffset);
  [[nodiscard]] bool skipNameSubsection();

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

// Shared subtyping function across validation.

[[nodiscard]] bool CheckIsSubtypeOf(Decoder& d, const ModuleEnvironment& env,
                                    size_t opcodeOffset, ValType actual,
                                    ValType expected, TypeCache* cache);

// The local entries are part of function bodies and thus serialized by both
// wasm and asm.js and decoded as part of both validation and compilation.

[[nodiscard]] bool EncodeLocalEntries(Encoder& d, const ValTypeVector& locals);

// This performs no validation; the local entries must already have been
// validated by an earlier pass.

[[nodiscard]] bool DecodeValidatedLocalEntries(Decoder& d,
                                               ValTypeVector* locals);

// This validates the entries.

[[nodiscard]] bool DecodeLocalEntries(Decoder& d, const TypeContext& types,
                                      const FeatureArgs& features,
                                      ValTypeVector* locals);

// Returns whether the given [begin, end) prefix of a module's bytecode starts a
// code section and, if so, returns the SectionRange of that code section.
// Note that, even if this function returns 'false', [begin, end) may actually
// be a valid module in the special case when there are no function defs and the
// code section is not present. Such modules can be valid so the caller must
// handle this special case.

[[nodiscard]] bool StartsCodeSection(const uint8_t* begin, const uint8_t* end,
                                     SectionRange* range);

// Calling DecodeModuleEnvironment decodes all sections up to the code section
// and performs full validation of all those sections. The client must then
// decode the code section itself, reusing ValidateFunctionBody if necessary,
// and finally call DecodeModuleTail to decode all remaining sections after the
// code section (again, performing full validation).

[[nodiscard]] bool DecodeModuleEnvironment(Decoder& d, ModuleEnvironment* env);

[[nodiscard]] bool ValidateFunctionBody(const ModuleEnvironment& env,
                                        uint32_t funcIndex, uint32_t bodySize,
                                        Decoder& d);

[[nodiscard]] bool DecodeModuleTail(Decoder& d, ModuleEnvironment* env);

void ConvertMemoryPagesToBytes(Limits* memory);

// Validate an entire module, returning true if the module was validated
// successfully. If Validate returns false:
//  - if *error is null, the caller should report out-of-memory
//  - otherwise, there was a legitimate error described by *error

[[nodiscard]] bool Validate(JSContext* cx, const ShareableBytes& bytecode,
                            const FeatureOptions& options, UniqueChars* error);

}  // namespace wasm
}  // namespace js

#endif  // namespace wasm_validate_h
