/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2021 Mozilla Foundation
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

#ifndef wasm_type_def_h
#define wasm_type_def_h

#include "mozilla/CheckedInt.h"

#include "wasm/WasmCodegenConstants.h"
#include "wasm/WasmCompileArgs.h"
#include "wasm/WasmConstants.h"
#include "wasm/WasmSerialize.h"
#include "wasm/WasmUtility.h"
#include "wasm/WasmValType.h"

namespace js {
namespace wasm {

using mozilla::CheckedInt32;
using mozilla::MallocSizeOf;

// The FuncType class represents a WebAssembly function signature which takes a
// list of value types and returns an expression type. The engine uses two
// in-memory representations of the argument Vector's memory (when elements do
// not fit inline): normal malloc allocation (via SystemAllocPolicy) and
// allocation in a LifoAlloc (via LifoAllocPolicy). The former FuncType objects
// can have any lifetime since they own the memory. The latter FuncType objects
// must not outlive the associated LifoAlloc mark/release interval (which is
// currently the duration of module validation+compilation). Thus, long-lived
// objects like WasmModule must use malloced allocation.

class FuncType {
  ValTypeVector args_;
  ValTypeVector results_;

  // Entry from JS to wasm via the JIT is currently unimplemented for
  // functions that return multiple values.
  bool temporarilyUnsupportedResultCountForJitEntry() const {
    return results().length() > MaxResultsForJitEntry;
  }
  // Calls out from wasm to JS that return multiple values is currently
  // unsupported.
  bool temporarilyUnsupportedResultCountForJitExit() const {
    return results().length() > MaxResultsForJitExit;
  }
  // For JS->wasm jit entries, temporarily disallow certain types until the
  // stubs generator is improved.
  //   * ref params may be nullable externrefs
  //   * ref results may not be type indices
  // V128 types are excluded per spec but are guarded against separately.
  bool temporarilyUnsupportedReftypeForEntry() const {
    for (ValType arg : args()) {
      if (arg.isRefType() && (!arg.isExternRef() || !arg.isNullable())) {
        return true;
      }
    }
    for (ValType result : results()) {
      if (result.isTypeIndex()) {
        return true;
      }
    }
    return false;
  }
  // For wasm->JS jit exits, temporarily disallow certain types until
  // the stubs generator is improved.
  //   * ref results may be nullable externrefs
  // Unexposable types must be guarded against separately.
  bool temporarilyUnsupportedReftypeForExit() const {
    for (ValType result : results()) {
      if (result.isRefType() &&
          (!result.isExternRef() || !result.isNullable())) {
        return true;
      }
    }
    return false;
  }

 public:
  FuncType() : args_(), results_() {}
  FuncType(ValTypeVector&& args, ValTypeVector&& results)
      : args_(std::move(args)), results_(std::move(results)) {}

  [[nodiscard]] bool clone(const FuncType& src) {
    MOZ_ASSERT(args_.empty());
    MOZ_ASSERT(results_.empty());
    return args_.appendAll(src.args_) && results_.appendAll(src.results_);
  }

  ValType arg(unsigned i) const { return args_[i]; }
  const ValTypeVector& args() const { return args_; }
  ValType result(unsigned i) const { return results_[i]; }
  const ValTypeVector& results() const { return results_; }

  HashNumber hash() const {
    HashNumber hn = 0;
    for (const ValType& vt : args_) {
      hn = mozilla::AddToHash(hn, HashNumber(vt.packed().bits()));
    }
    for (const ValType& vt : results_) {
      hn = mozilla::AddToHash(hn, HashNumber(vt.packed().bits()));
    }
    return hn;
  }
  bool operator==(const FuncType& rhs) const {
    return EqualContainers(args(), rhs.args()) &&
           EqualContainers(results(), rhs.results());
  }
  bool operator!=(const FuncType& rhs) const { return !(*this == rhs); }

  bool canHaveJitEntry() const;
  bool canHaveJitExit() const;

  bool hasInt64Arg() const {
    for (ValType arg : args()) {
      if (arg.kind() == ValType::Kind::I64) {
        return true;
      }
    }
    return false;
  }

  bool hasUnexposableArgOrRet() const {
    for (ValType arg : args()) {
      if (!arg.isExposable()) {
        return true;
      }
    }
    for (ValType result : results()) {
      if (!result.isExposable()) {
        return true;
      }
    }
    return false;
  }

#ifdef WASM_PRIVATE_REFTYPES
  bool exposesTypeIndex() const {
    for (const ValType& arg : args()) {
      if (arg.isTypeIndex()) {
        return true;
      }
    }
    for (const ValType& result : results()) {
      if (result.isTypeIndex()) {
        return true;
      }
    }
    return false;
  }
#endif

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  WASM_DECLARE_FRIEND_SERIALIZE(FuncType);
};

struct FuncTypeHashPolicy {
  using Lookup = const FuncType&;
  static HashNumber hash(Lookup ft) { return ft.hash(); }
  static bool match(const FuncType* lhs, Lookup rhs) { return *lhs == rhs; }
};

// Structure type.
//
// The Module owns a dense array of StructType values that represent the
// structure types that the module knows about.  It is created from the sparse
// array of types in the ModuleEnvironment when the Module is created.

struct StructField {
  FieldType type;
  uint32_t offset;
  bool isMutable;

  WASM_CHECK_CACHEABLE_POD(type, offset, isMutable);
};

WASM_DECLARE_CACHEABLE_POD(StructField);

using StructFieldVector = Vector<StructField, 0, SystemAllocPolicy>;

class StructType {
 public:
  StructFieldVector fields_;  // Field type, offset, and mutability
  uint32_t size_;             // The size of the type in bytes.

 public:
  StructType() : fields_(), size_(0) {}

  explicit StructType(StructFieldVector&& fields)
      : fields_(std::move(fields)), size_(0) {}

  StructType(StructType&&) = default;
  StructType& operator=(StructType&&) = default;

  [[nodiscard]] bool clone(const StructType& src) {
    if (!fields_.appendAll(src.fields_)) {
      return false;
    }
    size_ = src.size_;
    return true;
  }

  bool isDefaultable() const {
    for (auto& field : fields_) {
      if (!field.type.isDefaultable()) {
        return false;
      }
    }
    return true;
  }
  [[nodiscard]] bool computeLayout();

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  WASM_DECLARE_FRIEND_SERIALIZE(StructType);
};

using StructTypeVector = Vector<StructType, 0, SystemAllocPolicy>;
using StructTypePtrVector = Vector<const StructType*, 0, SystemAllocPolicy>;

// Utility for computing field offset and alignments, and total size for
// structs and tags.  This is complicated by fact that a WasmStructObject has
// an inline area, which is used first, and if that fills up an optional
// C++-heap-allocated outline area is used.  We need to be careful not to
// split any data item across the boundary.  This is ensured as follows:
//
// (1) the possible field sizes are 1, 2, 4, 8 and 16 only.
// (2) each field is "naturally aligned" -- aligned to its size.
// (3) MaxInlineBytes (the size of the inline area) % 16 == 0.
//
// From (1) and (2), it follows that all fields are placed so that their first
// and last bytes fall within the same 16-byte chunk.  That is,
// offset_of_first_byte_of_field / 16 == offset_of_last_byte_of_field / 16.
//
// Given that, it follows from (3) that all fields fall completely within
// either the inline or outline areas; no field crosses the boundary.
class StructLayout {
  CheckedInt32 sizeSoFar = 0;
  uint32_t structAlignment = 1;

 public:
  // The field adders return the offset of the the field.
  CheckedInt32 addField(FieldType type);

  // The close method rounds up the structure size to the appropriate
  // alignment and returns that size.
  CheckedInt32 close();
};

// Array type

class ArrayType {
 public:
  FieldType elementType_;  // field type
  bool isMutable_;         // mutability

  WASM_CHECK_CACHEABLE_POD(elementType_, isMutable_);

 public:
  ArrayType(FieldType elementType, bool isMutable)
      : elementType_(elementType), isMutable_(isMutable) {}

  ArrayType(const ArrayType&) = default;
  ArrayType& operator=(const ArrayType&) = default;

  ArrayType(ArrayType&&) = default;
  ArrayType& operator=(ArrayType&&) = default;

  [[nodiscard]] bool clone(const ArrayType& src) {
    elementType_ = src.elementType_;
    isMutable_ = src.isMutable_;
    return true;
  }

  bool isDefaultable() const { return elementType_.isDefaultable(); }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

WASM_DECLARE_CACHEABLE_POD(ArrayType);

using ArrayTypeVector = Vector<ArrayType, 0, SystemAllocPolicy>;
using ArrayTypePtrVector = Vector<const ArrayType*, 0, SystemAllocPolicy>;

// A tagged container for the various types that can be present in a wasm
// module's type section.

enum class TypeDefKind : uint8_t {
  None = 0,
  Func,
  Struct,
  Array,
};

class TypeDef {
  TypeDefKind kind_;
  union {
    FuncType funcType_;
    StructType structType_;
    ArrayType arrayType_;
  };

 public:
  TypeDef() : kind_(TypeDefKind::None) {}

  explicit TypeDef(FuncType&& funcType)
      : kind_(TypeDefKind::Func), funcType_(std::move(funcType)) {}

  explicit TypeDef(StructType&& structType)
      : kind_(TypeDefKind::Struct), structType_(std::move(structType)) {}

  explicit TypeDef(ArrayType&& arrayType)
      : kind_(TypeDefKind::Array), arrayType_(std::move(arrayType)) {}

  TypeDef(TypeDef&& td) noexcept : kind_(td.kind_) {
    switch (kind_) {
      case TypeDefKind::Func:
        new (&funcType_) FuncType(std::move(td.funcType_));
        break;
      case TypeDefKind::Struct:
        new (&structType_) StructType(std::move(td.structType_));
        break;
      case TypeDefKind::Array:
        new (&arrayType_) ArrayType(std::move(td.arrayType_));
        break;
      case TypeDefKind::None:
        break;
    }
  }

  ~TypeDef() {
    switch (kind_) {
      case TypeDefKind::Func:
        funcType_.~FuncType();
        break;
      case TypeDefKind::Struct:
        structType_.~StructType();
        break;
      case TypeDefKind::Array:
        arrayType_.~ArrayType();
        break;
      case TypeDefKind::None:
        break;
    }
  }

  TypeDef& operator=(TypeDef&& that) noexcept {
    MOZ_ASSERT(isNone());
    switch (that.kind_) {
      case TypeDefKind::Func:
        new (&funcType_) FuncType(std::move(that.funcType_));
        break;
      case TypeDefKind::Struct:
        new (&structType_) StructType(std::move(that.structType_));
        break;
      case TypeDefKind::Array:
        new (&arrayType_) ArrayType(std::move(that.arrayType_));
        break;
      case TypeDefKind::None:
        break;
    }
    kind_ = that.kind_;
    return *this;
  }

  [[nodiscard]] bool clone(const TypeDef& src) {
    MOZ_ASSERT(isNone());
    kind_ = src.kind_;
    switch (src.kind_) {
      case TypeDefKind::Func:
        new (&funcType_) FuncType();
        return funcType_.clone(src.funcType());
      case TypeDefKind::Struct:
        new (&structType_) StructType();
        return structType_.clone(src.structType());
      case TypeDefKind::Array:
        new (&arrayType_) ArrayType(src.arrayType());
        return true;
      case TypeDefKind::None:
        break;
    }
    MOZ_ASSERT_UNREACHABLE();
    return false;
  }

  TypeDefKind kind() const { return kind_; }

  bool isNone() const { return kind_ == TypeDefKind::None; }

  bool isFuncType() const { return kind_ == TypeDefKind::Func; }

  bool isStructType() const { return kind_ == TypeDefKind::Struct; }

  bool isArrayType() const { return kind_ == TypeDefKind::Array; }

  const FuncType& funcType() const {
    MOZ_ASSERT(isFuncType());
    return funcType_;
  }

  FuncType& funcType() {
    MOZ_ASSERT(isFuncType());
    return funcType_;
  }

  const StructType& structType() const {
    MOZ_ASSERT(isStructType());
    return structType_;
  }

  StructType& structType() {
    MOZ_ASSERT(isStructType());
    return structType_;
  }

  const ArrayType& arrayType() const {
    MOZ_ASSERT(isArrayType());
    return arrayType_;
  }

  ArrayType& arrayType() {
    MOZ_ASSERT(isArrayType());
    return arrayType_;
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  WASM_DECLARE_FRIEND_SERIALIZE(TypeDef);
};

using TypeDefVector = Vector<TypeDef, 0, SystemAllocPolicy>;

// A type cache maintains a cache of equivalence and subtype relations between
// wasm types. This is required for the computation of equivalence and subtyping
// on recursive types.
//
// This class is not thread-safe and so must exist separately from TypeContext,
// which may be shared between multiple threads.

class TypeCache {
  using TypeIndex = uint32_t;
  using TypePair = uint64_t;
  using TypeSet = HashSet<TypePair, DefaultHasher<TypePair>, SystemAllocPolicy>;

  // Generates a hash key for the ordered pair (a, b).
  static constexpr TypePair makeOrderedPair(TypeIndex a, TypeIndex b) {
    return (TypePair(a) << 32) | TypePair(b);
  }

  // Generates a hash key for the unordered pair (a, b).
  static constexpr TypePair makeUnorderedPair(TypeIndex a, TypeIndex b) {
    if (a < b) {
      return (TypePair(a) << 32) | TypePair(b);
    }
    return (TypePair(b) << 32) | TypePair(a);
  }

  TypeSet equivalence_;
  TypeSet subtype_;

 public:
  TypeCache() = default;

  // Mark `a` as equivalent to `b` in the equivalence cache.
  [[nodiscard]] bool markEquivalent(TypeIndex a, TypeIndex b) {
    return equivalence_.put(makeUnorderedPair(a, b));
  }
  // Unmark `a` as equivalent to `b` in the equivalence cache
  void unmarkEquivalent(TypeIndex a, TypeIndex b) {
    equivalence_.remove(makeUnorderedPair(a, b));
  }

  // Check if `a` is equivalent to `b` in the equivalence cache
  bool isEquivalent(TypeIndex a, TypeIndex b) {
    return equivalence_.has(makeUnorderedPair(a, b));
  }

  // Mark `a` as a subtype of `b` in the subtype cache
  [[nodiscard]] bool markSubtypeOf(TypeIndex a, TypeIndex b) {
    return subtype_.put(makeOrderedPair(a, b));
  }
  // Unmark `a` as a subtype of `b` in the subtype cache
  void unmarkSubtypeOf(TypeIndex a, TypeIndex b) {
    subtype_.remove(makeOrderedPair(a, b));
  }
  // Check if `a` is a subtype of `b` in the subtype cache
  bool isSubtypeOf(TypeIndex a, TypeIndex b) {
    return subtype_.has(makeOrderedPair(a, b));
  }
};

// The result of an equivalence or subtyping check between types.
enum class TypeResult {
  True,
  False,
  OOM,
};

// A type context maintains an index space for TypeDef's that can be used to
// give ValType's meaning. It is used during compilation for modules, and
// during runtime for all instances.

class TypeContext : public AtomicRefCounted<TypeContext> {
  FeatureArgs features_;
  TypeDefVector types_;

 public:
  TypeContext() = default;
  TypeContext(const FeatureArgs& features, TypeDefVector&& types)
      : features_(features), types_(std::move(types)) {}

  [[nodiscard]] bool clone(const TypeDefVector& source) {
    MOZ_ASSERT(types_.length() == 0);
    if (!types_.resize(source.length())) {
      return false;
    }
    for (uint32_t i = 0; i < source.length(); i++) {
      if (!types_[i].clone(source[i])) {
        return false;
      }
    }
    return true;
  }

  size_t sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
    return types_.sizeOfExcludingThis(mallocSizeOf);
  }

  // Disallow copy, allow move initialization
  TypeContext(const TypeContext&) = delete;
  TypeContext& operator=(const TypeContext&) = delete;
  TypeContext(TypeContext&&) = delete;
  TypeContext& operator=(TypeContext&&) = delete;

  TypeDef& type(uint32_t index) { return types_[index]; }
  const TypeDef& type(uint32_t index) const { return types_[index]; }

  TypeDef& operator[](uint32_t index) { return types_[index]; }
  const TypeDef& operator[](uint32_t index) const { return types_[index]; }

  uint32_t length() const { return types_.length(); }

  template <typename U>
  [[nodiscard]] bool append(U&& typeDef) {
    return types_.append(std::forward<U>(typeDef));
  }
  [[nodiscard]] bool resize(uint32_t length) { return types_.resize(length); }

  // Map from type definition to index

  uint32_t indexOf(const TypeDef& typeDef) const {
    const TypeDef* elem = &typeDef;
    MOZ_ASSERT(elem >= types_.begin() && elem < types_.end());
    return elem - types_.begin();
  }

  // FuncType accessors

  bool isFuncType(uint32_t index) const { return types_[index].isFuncType(); }
  bool isFuncType(RefType t) const {
    return t.isTypeIndex() && isFuncType(t.typeIndex());
  }

  FuncType& funcType(uint32_t index) { return types_[index].funcType(); }
  const FuncType& funcType(uint32_t index) const {
    return types_[index].funcType();
  }
  FuncType& funcType(RefType t) { return funcType(t.typeIndex()); }
  const FuncType& funcType(RefType t) const { return funcType(t.typeIndex()); }

  // StructType accessors

  bool isStructType(uint32_t index) const {
    return types_[index].isStructType();
  }
  bool isStructType(RefType t) const {
    return t.isTypeIndex() && isStructType(t.typeIndex());
  }

  StructType& structType(uint32_t index) { return types_[index].structType(); }
  const StructType& structType(uint32_t index) const {
    return types_[index].structType();
  }
  StructType& structType(RefType t) { return structType(t.typeIndex()); }
  const StructType& structType(RefType t) const {
    return structType(t.typeIndex());
  }

  // StructType accessors

  bool isArrayType(uint32_t index) const { return types_[index].isArrayType(); }
  bool isArrayType(RefType t) const {
    return t.isTypeIndex() && isArrayType(t.typeIndex());
  }

  ArrayType& arrayType(uint32_t index) { return types_[index].arrayType(); }
  const ArrayType& arrayType(uint32_t index) const {
    return types_[index].arrayType();
  }
  ArrayType& arrayType(RefType t) { return arrayType(t.typeIndex()); }
  const ArrayType& arrayType(RefType t) const {
    return arrayType(t.typeIndex());
  }

  // Type equivalence

  template <class T>
  TypeResult isEquivalent(T first, T second, TypeCache* cache) const {
    // Anything's equal to itself.
    if (first == second) {
      return TypeResult::True;
    }

    // A reference may be equal to another reference
    if (first.isRefType() && second.isRefType()) {
      return isRefEquivalent(first.refType(), second.refType(), cache);
    }

    return TypeResult::False;
  }

  TypeResult isRefEquivalent(RefType first, RefType second,
                             TypeCache* cache) const;
#ifdef ENABLE_WASM_FUNCTION_REFERENCES
  TypeResult isTypeIndexEquivalent(uint32_t firstIndex, uint32_t secondIndex,
                                   TypeCache* cache) const;
#endif
#ifdef ENABLE_WASM_GC
  TypeResult isStructEquivalent(uint32_t firstIndex, uint32_t secondIndex,
                                TypeCache* cache) const;
  TypeResult isStructFieldEquivalent(const StructField first,
                                     const StructField second,
                                     TypeCache* cache) const;
  TypeResult isArrayEquivalent(uint32_t firstIndex, uint32_t secondIndex,
                               TypeCache* cache) const;
  TypeResult isArrayElementEquivalent(const ArrayType& first,
                                      const ArrayType& second,
                                      TypeCache* cache) const;
#endif

  // Subtyping

  template <class T>
  TypeResult isSubtypeOf(T subType, T superType, TypeCache* cache) const {
    // Anything's a subtype of itself.
    if (subType == superType) {
      return TypeResult::True;
    }

    // A reference may be a subtype of another reference
    if (subType.isRefType() && superType.isRefType()) {
      return isRefSubtypeOf(subType.refType(), superType.refType(), cache);
    }

    return TypeResult::False;
  }

  TypeResult isRefSubtypeOf(RefType subType, RefType superType,
                            TypeCache* cache) const;
#ifdef ENABLE_WASM_FUNCTION_REFERENCES
  TypeResult isTypeIndexSubtypeOf(uint32_t subTypeIndex,
                                  uint32_t superTypeIndex,
                                  TypeCache* cache) const;
#endif

#ifdef ENABLE_WASM_GC
  TypeResult isStructSubtypeOf(uint32_t subTypeIndex, uint32_t superTypeIndex,
                               TypeCache* cache) const;
  TypeResult isStructFieldSubtypeOf(const StructField subType,
                                    const StructField superType,
                                    TypeCache* cache) const;
  TypeResult isArraySubtypeOf(uint32_t subTypeIndex, uint32_t superTypeIndex,
                              TypeCache* cache) const;
  TypeResult isArrayElementSubtypeOf(const ArrayType& subType,
                                     const ArrayType& superType,
                                     TypeCache* cache) const;
#endif
};

using SharedTypeContext = RefPtr<const TypeContext>;
using MutableTypeContext = RefPtr<TypeContext>;

// An unambiguous strong reference to a type definition in a specific type
// context.
class TypeHandle {
 private:
  SharedTypeContext context_;
  uint32_t index_;

 public:
  TypeHandle(SharedTypeContext context, uint32_t index)
      : context_(context), index_(index) {
    MOZ_ASSERT(index_ < context_->length());
  }
  TypeHandle(SharedTypeContext context, const TypeDef& def)
      : context_(context), index_(context->indexOf(def)) {}

  TypeHandle(const TypeHandle&) = default;
  TypeHandle& operator=(const TypeHandle&) = default;

  const SharedTypeContext& context() const { return context_; }
  uint32_t index() const { return index_; }
  const TypeDef& def() const { return context_->type(index_); }
};

// [SMDOC] Signatures and runtime types
//
// TypeIdDesc describes the runtime representation of a TypeDef suitable for
// type equality checks. The kind of representation depends on whether the type
// is a function or a GC type. This design is in flux and will evolve.
//
// # Function types
//
// For functions in the general case, a FuncType is allocated and stored in a
// process-wide hash table, so that pointer equality implies structural
// equality. This process does not correctly handle type references (which would
// require hash-consing of infinite-trees), but that's okay while
// function-references and gc-types are experimental.
//
// A pointer to the hash table entry is stored in the global data
// area for each instance, and TypeIdDesc gives the offset to this entry.
//
// ## Immediate function types
//
// As an optimization for the 99% case where the FuncType has a small number of
// parameters, the FuncType is bit-packed into a uint32 immediate value so that
// integer equality implies structural equality. Both cases can be handled with
// a single comparison by always setting the LSB for the immediates
// (the LSB is necessarily 0 for allocated FuncType pointers due to alignment).
//
// # GC types
//
// For GC types, an entry is always created in the global data area and a
// unique RttValue (see wasm/TypedObject.h) is stored there. This RttValue
// is the value given by 'rtt.canon $t' for each type definition. As each entry
// is given a unique value and no canonicalization is done (which would require
// hash-consing of infinite-trees), this is not yet spec compliant.
//
// # wasm::Instance and the global type context
//
// As GC objects (aka TypedObject) may outlive the module they are created in,
// types are additionally transferred to a wasm::Context (which is part of
// JSContext) upon instantiation. This wasm::Context contains the
// 'global type context' that RTTValues refer to by type index. Types are never
// freed from the global type context as that would shift the index space. In
// the future, this will be fixed.

class TypeIdDesc {
 public:
  static const uintptr_t ImmediateBit = 0x1;

 private:
  TypeIdDescKind kind_;
  size_t bits_;

  WASM_CHECK_CACHEABLE_POD(kind_, bits_);

  TypeIdDesc(TypeIdDescKind kind, size_t bits) : kind_(kind), bits_(bits) {}

 public:
  TypeIdDescKind kind() const { return kind_; }
  static bool isGlobal(const TypeDef& type);

  TypeIdDesc() : kind_(TypeIdDescKind::None), bits_(0) {}
  static TypeIdDesc global(const TypeDef& type, uint32_t globalDataOffset);
  static TypeIdDesc immediate(const TypeDef& type);

  bool isGlobal() const { return kind_ == TypeIdDescKind::Global; }

  uint32_t immediate() const {
    MOZ_ASSERT(kind_ == TypeIdDescKind::Immediate);
    return bits_;
  }
  uint32_t globalDataOffset() const {
    MOZ_ASSERT(kind_ == TypeIdDescKind::Global);
    return bits_;
  }
};

WASM_DECLARE_CACHEABLE_POD(TypeIdDesc);

using TypeIdDescVector = Vector<TypeIdDesc, 0, SystemAllocPolicy>;

// TypeDefWithId pairs a FuncType with TypeIdDesc, describing either how to
// compile code that compares this signature's id or, at instantiation what
// signature ids to allocate in the global hash and where to put them.

struct TypeDefWithId : public TypeDef {
  TypeIdDesc id;

  TypeDefWithId() = default;
  explicit TypeDefWithId(TypeDef&& typeDef)
      : TypeDef(std::move(typeDef)), id() {}
  TypeDefWithId(TypeDef&& typeDef, TypeIdDesc id)
      : TypeDef(std::move(typeDef)), id(id) {}

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

using TypeDefWithIdVector = Vector<TypeDefWithId, 0, SystemAllocPolicy>;
using TypeDefWithIdPtrVector =
    Vector<const TypeDefWithId*, 0, SystemAllocPolicy>;

}  // namespace wasm
}  // namespace js

#endif  // wasm_type_def_h
