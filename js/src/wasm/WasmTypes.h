/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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

#ifndef wasm_types_h
#define wasm_types_h

#include "mozilla/Alignment.h"
#include "mozilla/Atomics.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"

#include <type_traits>

#include "NamespaceImports.h"

#include "ds/LifoAlloc.h"
#include "jit/IonTypes.h"
#include "js/RefCounted.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "js/WasmFeatures.h"
#include "vm/MallocProvider.h"
#include "vm/NativeObject.h"
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmConstants.h"
#include "wasm/WasmTypeDecls.h"
#include "wasm/WasmUtility.h"
#include "wasm/WasmValType.h"
#include "wasm/WasmValue.h"

namespace js {

namespace jit {
enum class RoundingMode;
template <class VecT, class ABIArgGeneratorT>
class ABIArgIterBase;
}  // namespace jit

namespace wasm {

using mozilla::Atomic;
using mozilla::DebugOnly;
using mozilla::EnumeratedArray;
using mozilla::MallocSizeOf;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::PodCopy;
using mozilla::PodZero;
using mozilla::Some;
using mozilla::Unused;

class Code;
class DebugState;
class GeneratedSourceMap;
class Memory;
class Module;
class Instance;
class Table;

// Bit set as the lowest bit of a frame pointer, used in two different mutually
// exclusive situations:
// - either it's a low bit tag in a FramePointer value read from the
// Frame::callerFP of an inner wasm frame. This indicates the previous call
// frame has been set up by a JIT caller that directly called into a wasm
// function's body. This is only stored in Frame::callerFP for a wasm frame
// called from JIT code, and thus it can not appear in a JitActivation's
// exitFP.
// - or it's the low big tag set when exiting wasm code in JitActivation's
// exitFP.

constexpr uintptr_t ExitOrJitEntryFPTag = 0x1;

// To call Vector::shrinkStorageToFit , a type must specialize mozilla::IsPod
// which is pretty verbose to do within js::wasm, so factor that process out
// into a macro.

#define WASM_DECLARE_POD_VECTOR(Type, VectorName)   \
  }                                                 \
  }                                                 \
  namespace mozilla {                               \
  template <>                                       \
  struct IsPod<js::wasm::Type> : std::true_type {}; \
  }                                                 \
  namespace js {                                    \
  namespace wasm {                                  \
  typedef Vector<Type, 0, SystemAllocPolicy> VectorName;

// A wasm Module and everything it contains must support serialization and
// deserialization. Some data can be simply copied as raw bytes and,
// as a convention, is stored in an inline CacheablePod struct. Everything else
// should implement the below methods which are called recusively by the
// containing Module.

#define WASM_DECLARE_SERIALIZABLE(Type)              \
  size_t serializedSize() const;                     \
  uint8_t* serialize(uint8_t* cursor) const;         \
  const uint8_t* deserialize(const uint8_t* cursor); \
  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

template <class T>
struct SerializableRefPtr : RefPtr<T> {
  using RefPtr<T>::operator=;

  SerializableRefPtr() = default;

  template <class U>
  MOZ_IMPLICIT SerializableRefPtr(U&& u) : RefPtr<T>(std::forward<U>(u)) {}

  WASM_DECLARE_SERIALIZABLE(SerializableRefPtr)
};

// This reusable base class factors out the logic for a resource that is shared
// by multiple instances/modules but should only be counted once when computing
// about:memory stats.

template <class T>
struct ShareableBase : AtomicRefCounted<T> {
  using SeenSet = HashSet<const T*, DefaultHasher<const T*>, SystemAllocPolicy>;

  size_t sizeOfIncludingThisIfNotSeen(MallocSizeOf mallocSizeOf,
                                      SeenSet* seen) const {
    const T* self = static_cast<const T*>(this);
    typename SeenSet::AddPtr p = seen->lookupForAdd(self);
    if (p) {
      return 0;
    }
    bool ok = seen->add(p, self);
    (void)ok;  // oh well
    return mallocSizeOf(self) + self->sizeOfExcludingThis(mallocSizeOf);
  }
};

// ShareableBytes is a reference-counted Vector of bytes.

struct ShareableBytes : ShareableBase<ShareableBytes> {
  // Vector is 'final', so instead make Vector a member and add boilerplate.
  Bytes bytes;

  ShareableBytes() = default;
  explicit ShareableBytes(Bytes&& bytes) : bytes(std::move(bytes)) {}
  size_t sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
    return bytes.sizeOfExcludingThis(mallocSizeOf);
  }
  const uint8_t* begin() const { return bytes.begin(); }
  const uint8_t* end() const { return bytes.end(); }
  size_t length() const { return bytes.length(); }
  bool append(const uint8_t* start, size_t len) {
    return bytes.append(start, len);
  }
};

using MutableBytes = RefPtr<ShareableBytes>;
using SharedBytes = RefPtr<const ShareableBytes>;

// The Opcode compactly and safely represents the primary opcode plus any
// extension, with convenient predicates and accessors.

class Opcode {
  uint32_t bits_;

 public:
  MOZ_IMPLICIT Opcode(Op op) : bits_(uint32_t(op)) {
    static_assert(size_t(Op::Limit) == 256, "fits");
    MOZ_ASSERT(size_t(op) < size_t(Op::Limit));
  }
  MOZ_IMPLICIT Opcode(MiscOp op)
      : bits_((uint32_t(op) << 8) | uint32_t(Op::MiscPrefix)) {
    static_assert(size_t(MiscOp::Limit) <= 0xFFFFFF, "fits");
    MOZ_ASSERT(size_t(op) < size_t(MiscOp::Limit));
  }
  MOZ_IMPLICIT Opcode(ThreadOp op)
      : bits_((uint32_t(op) << 8) | uint32_t(Op::ThreadPrefix)) {
    static_assert(size_t(ThreadOp::Limit) <= 0xFFFFFF, "fits");
    MOZ_ASSERT(size_t(op) < size_t(ThreadOp::Limit));
  }
  MOZ_IMPLICIT Opcode(MozOp op)
      : bits_((uint32_t(op) << 8) | uint32_t(Op::MozPrefix)) {
    static_assert(size_t(MozOp::Limit) <= 0xFFFFFF, "fits");
    MOZ_ASSERT(size_t(op) < size_t(MozOp::Limit));
  }
  MOZ_IMPLICIT Opcode(SimdOp op)
      : bits_((uint32_t(op) << 8) | uint32_t(Op::SimdPrefix)) {
    static_assert(size_t(SimdOp::Limit) <= 0xFFFFFF, "fits");
    MOZ_ASSERT(size_t(op) < size_t(SimdOp::Limit));
  }

  bool isOp() const { return bits_ < uint32_t(Op::FirstPrefix); }
  bool isMisc() const { return (bits_ & 255) == uint32_t(Op::MiscPrefix); }
  bool isThread() const { return (bits_ & 255) == uint32_t(Op::ThreadPrefix); }
  bool isMoz() const { return (bits_ & 255) == uint32_t(Op::MozPrefix); }
  bool isSimd() const { return (bits_ & 255) == uint32_t(Op::SimdPrefix); }

  Op asOp() const {
    MOZ_ASSERT(isOp());
    return Op(bits_);
  }
  MiscOp asMisc() const {
    MOZ_ASSERT(isMisc());
    return MiscOp(bits_ >> 8);
  }
  ThreadOp asThread() const {
    MOZ_ASSERT(isThread());
    return ThreadOp(bits_ >> 8);
  }
  MozOp asMoz() const {
    MOZ_ASSERT(isMoz());
    return MozOp(bits_ >> 8);
  }
  SimdOp asSimd() const {
    MOZ_ASSERT(isSimd());
    return SimdOp(bits_ >> 8);
  }

  uint32_t bits() const { return bits_; }

  bool operator==(const Opcode& that) const { return bits_ == that.bits_; }
  bool operator!=(const Opcode& that) const { return bits_ != that.bits_; }
};

// Exception tags are used to uniquely identify exceptions. They are stored
// in a vector in Instances and used by both WebAssembly.Exception for import
// and export, and by the representation of thrown exceptions.
//
// Since an exception tag is a (trivial) substructure of AtomicRefCounted, the
// RefPtr SharedExceptionTag can have many instances/modules referencing a
// single constant exception tag.

struct ExceptionTag : AtomicRefCounted<ExceptionTag> {
  ExceptionTag() = default;
};
using SharedExceptionTag = RefPtr<ExceptionTag>;
using SharedExceptionTagVector =
    Vector<SharedExceptionTag, 0, SystemAllocPolicy>;

// WasmJSExceptionObject wraps a JS Value in order to provide a uniform
// method of handling JS thrown exceptions. Exceptions originating in Wasm
// are WebAssemby.RuntimeException objects, whereas exceptions from JS are
// wrapped as WasmJSExceptionObject objects.
class WasmJSExceptionObject : public NativeObject {
  static const unsigned VALUE_SLOT = 0;

 public:
  static const unsigned RESERVED_SLOTS = 1;
  static const JSClass class_;
  const Value& value() const { return getFixedSlot(VALUE_SLOT); }

  static WasmJSExceptionObject* create(JSContext* cx, MutableHandleValue value);
};

// Code can be compiled either with the Baseline compiler or the Ion compiler,
// and tier-variant data are tagged with the Tier value.
//
// A tier value is used to request tier-variant aspects of code, metadata, or
// linkdata.  The tiers are normally explicit (Baseline and Ion); implicit tiers
// can be obtained through accessors on Code objects (eg, stableTier).

enum class Tier {
  Baseline,
  Debug = Baseline,
  Optimized,
  Serialized = Optimized
};

// Iterator over tiers present in a tiered data structure.

class Tiers {
  Tier t_[2];
  uint32_t n_;

 public:
  explicit Tiers() { n_ = 0; }
  explicit Tiers(Tier t) {
    t_[0] = t;
    n_ = 1;
  }
  explicit Tiers(Tier t, Tier u) {
    MOZ_ASSERT(t != u);
    t_[0] = t;
    t_[1] = u;
    n_ = 2;
  }

  Tier* begin() { return t_; }
  Tier* end() { return t_ + n_; }
};

// A Module can either be asm.js or wasm.

enum ModuleKind { Wasm, AsmJS };

enum class Shareable { False, True };

// Describes per-compilation settings that are controlled by an options bag
// passed to compilation and validation functions.  (Nonstandard extension
// available under prefs.)

struct FeatureOptions {
  FeatureOptions() : simdWormhole(false) {}

  // May be set if javascript.options.wasm_simd_wormhole==true.
  bool simdWormhole;
};

// Describes the features that control wasm compilation.

struct FeatureArgs {
  FeatureArgs()
      :
#define WASM_FEATURE(NAME, LOWER_NAME, ...) LOWER_NAME(false),
        JS_FOR_WASM_FEATURES(WASM_FEATURE, WASM_FEATURE)
#undef WASM_FEATURE
            sharedMemory(Shareable::False),
        hugeMemory(false),
        simdWormhole(false) {
  }
  FeatureArgs(const FeatureArgs&) = default;
  FeatureArgs& operator=(const FeatureArgs&) = default;
  FeatureArgs(FeatureArgs&&) = default;

  static FeatureArgs build(JSContext* cx, const FeatureOptions& options);

#define WASM_FEATURE(NAME, LOWER_NAME, ...) bool LOWER_NAME;
  JS_FOR_WASM_FEATURES(WASM_FEATURE, WASM_FEATURE)
#undef WASM_FEATURE

  Shareable sharedMemory;
  bool hugeMemory;
  bool simdWormhole;
};

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
      if (arg.isReference() && (!arg.isExternRef() || !arg.isNullable())) {
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
      if (result.isReference() &&
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

  void renumber(const RenumberMap& map) {
    for (auto& arg : args_) {
      arg.renumber(map);
    }
    for (auto& result : results_) {
      result.renumber(map);
    }
  }
  void offsetTypeIndex(uint32_t offsetBy) {
    for (auto& arg : args_) {
      arg.offsetTypeIndex(offsetBy);
    }
    for (auto& result : results_) {
      result.offsetTypeIndex(offsetBy);
    }
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

  WASM_DECLARE_SERIALIZABLE(FuncType)
};

struct FuncTypeHashPolicy {
  using Lookup = const FuncType&;
  static HashNumber hash(Lookup ft) { return ft.hash(); }
  static bool match(const FuncType* lhs, Lookup rhs) { return *lhs == rhs; }
};

// ArgTypeVector type.
//
// Functions usually receive one ABI argument per WebAssembly argument.  However
// if a function has multiple results and some of those results go to the stack,
// then it additionally receives a synthetic ABI argument holding a pointer to
// the stack result area.
//
// Given the presence of synthetic arguments, sometimes we need a name for
// non-synthetic arguments.  We call those "natural" arguments.

enum class StackResults { HasStackResults, NoStackResults };

class ArgTypeVector {
  const ValTypeVector& args_;
  bool hasStackResults_;

  // To allow ABIArgIterBase<VecT, ABIArgGeneratorT>, we define a private
  // length() method.  To prevent accidental errors, other users need to be
  // explicit and call lengthWithStackResults() or
  // lengthWithoutStackResults().
  size_t length() const { return args_.length() + size_t(hasStackResults_); }
  template <class VecT, class ABIArgGeneratorT>
  friend class jit::ABIArgIterBase;

 public:
  ArgTypeVector(const ValTypeVector& args, StackResults stackResults)
      : args_(args),
        hasStackResults_(stackResults == StackResults::HasStackResults) {}
  explicit ArgTypeVector(const FuncType& funcType);

  bool hasSyntheticStackResultPointerArg() const { return hasStackResults_; }
  StackResults stackResults() const {
    return hasSyntheticStackResultPointerArg() ? StackResults::HasStackResults
                                               : StackResults::NoStackResults;
  }
  size_t lengthWithoutStackResults() const { return args_.length(); }
  bool isSyntheticStackResultPointerArg(size_t idx) const {
    // The pointer to stack results area, if present, is a synthetic argument
    // tacked on at the end.
    MOZ_ASSERT(idx < lengthWithStackResults());
    return idx == args_.length();
  }
  bool isNaturalArg(size_t idx) const {
    return !isSyntheticStackResultPointerArg(idx);
  }
  size_t naturalIndex(size_t idx) const {
    MOZ_ASSERT(isNaturalArg(idx));
    // Because the synthetic argument, if present, is tacked on the end, an
    // argument index that isn't synthetic is natural.
    return idx;
  }

  size_t lengthWithStackResults() const { return length(); }
  jit::MIRType operator[](size_t i) const {
    MOZ_ASSERT(i < lengthWithStackResults());
    if (isSyntheticStackResultPointerArg(i)) {
      return jit::MIRType::StackResults;
    }
    return ToMIRType(args_[naturalIndex(i)]);
  }
};

template <typename PointerType>
class TaggedValue {
 public:
  enum Kind {
    ImmediateKind1 = 0,
    ImmediateKind2 = 1,
    PointerKind1 = 2,
    PointerKind2 = 3
  };
  using PackedRepr = uintptr_t;

 private:
  PackedRepr bits_;

  static constexpr PackedRepr PayloadShift = 2;
  static constexpr PackedRepr KindMask = 0x3;
  static constexpr PackedRepr PointerKindBit = 0x2;

  constexpr static bool IsPointerKind(Kind kind) {
    return PackedRepr(kind) & PointerKindBit;
  }
  constexpr static bool IsImmediateKind(Kind kind) {
    return !IsPointerKind(kind);
  }

  static_assert(IsImmediateKind(ImmediateKind1), "immediate kind 1");
  static_assert(IsImmediateKind(ImmediateKind2), "immediate kind 2");
  static_assert(IsPointerKind(PointerKind1), "pointer kind 1");
  static_assert(IsPointerKind(PointerKind2), "pointer kind 2");

  static PackedRepr PackImmediate(Kind kind, PackedRepr imm) {
    MOZ_ASSERT(IsImmediateKind(kind));
    MOZ_ASSERT((PackedRepr(kind) & KindMask) == kind);
    MOZ_ASSERT((imm & (PackedRepr(KindMask)
                       << ((sizeof(PackedRepr) * 8) - PayloadShift))) == 0);
    return PackedRepr(kind) | (PackedRepr(imm) << PayloadShift);
  }

  static PackedRepr PackPointer(Kind kind, PointerType* ptr) {
    PackedRepr ptrBits = reinterpret_cast<PackedRepr>(ptr);
    MOZ_ASSERT(IsPointerKind(kind));
    MOZ_ASSERT((PackedRepr(kind) & KindMask) == kind);
    MOZ_ASSERT((ptrBits & KindMask) == 0);
    return PackedRepr(kind) | ptrBits;
  }

 public:
  TaggedValue(Kind kind, PackedRepr imm) : bits_(PackImmediate(kind, imm)) {}
  TaggedValue(Kind kind, PointerType* ptr) : bits_(PackPointer(kind, ptr)) {}

  PackedRepr bits() const { return bits_; }
  Kind kind() const { return Kind(bits() & KindMask); }
  PackedRepr immediate() const {
    MOZ_ASSERT(IsImmediateKind(kind()));
    return mozilla::AssertedCast<PackedRepr>(bits() >> PayloadShift);
  }
  PointerType* pointer() const {
    MOZ_ASSERT(IsPointerKind(kind()));
    return reinterpret_cast<PointerType*>(bits() & ~KindMask);
  }
};

static_assert(
    std::is_same<TaggedValue<void*>::PackedRepr, PackedTypeCode::PackedRepr>(),
    "can use pointer tagging with PackedTypeCode");

// ResultType represents the WebAssembly spec's `resulttype`. Semantically, a
// result type is just a vec(valtype).  For effiency, though, the ResultType
// value is packed into a word, with separate encodings for these 3 cases:
//  []
//  [valtype]
//  pointer to ValTypeVector
//
// Additionally there is an encoding indicating uninitialized ResultType
// values.
//
// Generally in the latter case the ValTypeVector is the args() or results() of
// a FuncType in the compilation unit, so as long as the lifetime of the
// ResultType value is less than the OpIter, we can just borrow the pointer
// without ownership or copying.
class ResultType {
  using Tagged = TaggedValue<const ValTypeVector>;
  Tagged tagged_;

  enum Kind {
    EmptyKind = Tagged::ImmediateKind1,
    SingleKind = Tagged::ImmediateKind2,
    VectorKind = Tagged::PointerKind1,
    InvalidKind = Tagged::PointerKind2,
  };

  ResultType(Kind kind, uint32_t imm) : tagged_(Tagged::Kind(kind), imm) {}
  explicit ResultType(const ValTypeVector* ptr)
      : tagged_(Tagged::Kind(VectorKind), ptr) {}

  Kind kind() const { return Kind(tagged_.kind()); }

  ValType singleValType() const {
    MOZ_ASSERT(kind() == SingleKind);
    return ValType(PackedTypeCode::fromBits(tagged_.immediate()));
  }

  const ValTypeVector& values() const {
    MOZ_ASSERT(kind() == VectorKind);
    return *tagged_.pointer();
  }

 public:
  ResultType() : tagged_(Tagged::Kind(InvalidKind), nullptr) {}

  static ResultType Empty() { return ResultType(EmptyKind, uint32_t(0)); }
  static ResultType Single(ValType vt) {
    return ResultType(SingleKind, vt.bitsUnsafe());
  }
  static ResultType Vector(const ValTypeVector& vals) {
    switch (vals.length()) {
      case 0:
        return Empty();
      case 1:
        return Single(vals[0]);
      default:
        return ResultType(&vals);
    }
  }

  [[nodiscard]] bool cloneToVector(ValTypeVector* out) {
    MOZ_ASSERT(out->empty());
    switch (kind()) {
      case EmptyKind:
        return true;
      case SingleKind:
        return out->append(singleValType());
      case VectorKind:
        return out->appendAll(values());
      default:
        MOZ_CRASH("bad resulttype");
    }
  }

  bool empty() const { return kind() == EmptyKind; }

  size_t length() const {
    switch (kind()) {
      case EmptyKind:
        return 0;
      case SingleKind:
        return 1;
      case VectorKind:
        return values().length();
      default:
        MOZ_CRASH("bad resulttype");
    }
  }

  ValType operator[](size_t i) const {
    switch (kind()) {
      case SingleKind:
        MOZ_ASSERT(i == 0);
        return singleValType();
      case VectorKind:
        return values()[i];
      default:
        MOZ_CRASH("bad resulttype");
    }
  }

  bool operator==(ResultType rhs) const {
    switch (kind()) {
      case EmptyKind:
      case SingleKind:
      case InvalidKind:
        return tagged_.bits() == rhs.tagged_.bits();
      case VectorKind: {
        if (rhs.kind() != VectorKind) {
          return false;
        }
        return EqualContainers(values(), rhs.values());
      }
      default:
        MOZ_CRASH("bad resulttype");
    }
  }
  bool operator!=(ResultType rhs) const { return !(*this == rhs); }
};

// BlockType represents the WebAssembly spec's `blocktype`. Semantically, a
// block type is just a (vec(valtype) -> vec(valtype)) with four special
// encodings which are represented explicitly in BlockType:
//  [] -> []
//  [] -> [valtype]
//  [params] -> [results] via pointer to FuncType
//  [] -> [results] via pointer to FuncType (ignoring [params])

class BlockType {
  using Tagged = TaggedValue<const FuncType>;
  Tagged tagged_;

  enum Kind {
    VoidToVoidKind = Tagged::ImmediateKind1,
    VoidToSingleKind = Tagged::ImmediateKind2,
    FuncKind = Tagged::PointerKind1,
    FuncResultsKind = Tagged::PointerKind2
  };

  BlockType(Kind kind, uint32_t imm) : tagged_(Tagged::Kind(kind), imm) {}
  BlockType(Kind kind, const FuncType& type)
      : tagged_(Tagged::Kind(kind), &type) {}

  Kind kind() const { return Kind(tagged_.kind()); }
  ValType singleValType() const {
    MOZ_ASSERT(kind() == VoidToSingleKind);
    return ValType(PackedTypeCode::fromBits(tagged_.immediate()));
  }

  const FuncType& funcType() const { return *tagged_.pointer(); }

 public:
  BlockType()
      : tagged_(Tagged::Kind(VoidToVoidKind),
                PackedTypeCode::invalid().bits()) {}

  static BlockType VoidToVoid() {
    return BlockType(VoidToVoidKind, uint32_t(0));
  }
  static BlockType VoidToSingle(ValType vt) {
    return BlockType(VoidToSingleKind, vt.bitsUnsafe());
  }
  static BlockType Func(const FuncType& type) {
    if (type.args().length() == 0) {
      return FuncResults(type);
    }
    return BlockType(FuncKind, type);
  }
  static BlockType FuncResults(const FuncType& type) {
    switch (type.results().length()) {
      case 0:
        return VoidToVoid();
      case 1:
        return VoidToSingle(type.results()[0]);
      default:
        return BlockType(FuncResultsKind, type);
    }
  }

  ResultType params() const {
    switch (kind()) {
      case VoidToVoidKind:
      case VoidToSingleKind:
      case FuncResultsKind:
        return ResultType::Empty();
      case FuncKind:
        return ResultType::Vector(funcType().args());
      default:
        MOZ_CRASH("unexpected kind");
    }
  }

  ResultType results() const {
    switch (kind()) {
      case VoidToVoidKind:
        return ResultType::Empty();
      case VoidToSingleKind:
        return ResultType::Single(singleValType());
      case FuncKind:
      case FuncResultsKind:
        return ResultType::Vector(funcType().results());
      default:
        MOZ_CRASH("unexpected kind");
    }
  }

  bool operator==(BlockType rhs) const {
    if (kind() != rhs.kind()) {
      return false;
    }
    switch (kind()) {
      case VoidToVoidKind:
      case VoidToSingleKind:
        return tagged_.bits() == rhs.tagged_.bits();
      case FuncKind:
        return funcType() == rhs.funcType();
      case FuncResultsKind:
        return EqualContainers(funcType().results(), rhs.funcType().results());
      default:
        MOZ_CRASH("unexpected kind");
    }
  }

  bool operator!=(BlockType rhs) const { return !(*this == rhs); }
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
};

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

  void renumber(const RenumberMap& map) {
    for (auto& field : fields_) {
      field.type.renumber(map);
    }
  }
  void offsetTypeIndex(uint32_t offsetBy) {
    for (auto& field : fields_) {
      field.type.offsetTypeIndex(offsetBy);
    }
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

  WASM_DECLARE_SERIALIZABLE(StructType)
};

using StructTypeVector = Vector<StructType, 0, SystemAllocPolicy>;
using StructTypePtrVector = Vector<const StructType*, 0, SystemAllocPolicy>;

// Array type

class ArrayType {
 public:
  FieldType elementType_;  // field type
  bool isMutable_;         // mutability

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

  void renumber(const RenumberMap& map) { elementType_.renumber(map); }
  void offsetTypeIndex(uint32_t offsetBy) {
    elementType_.offsetTypeIndex(offsetBy);
  }

  bool isDefaultable() const { return elementType_.isDefaultable(); }

  WASM_DECLARE_SERIALIZABLE(ArrayType)
};

using ArrayTypeVector = Vector<ArrayType, 0, SystemAllocPolicy>;
using ArrayTypePtrVector = Vector<const ArrayType*, 0, SystemAllocPolicy>;

enum class InitExprKind {
  None,
  Literal,
  Variable,
};

// A InitExpr describes a deferred initializer expression, used to initialize
// a global or a table element offset. Such expressions are created during
// decoding and actually executed on module instantiation.

class InitExpr {
  InitExprKind kind_;
  // The bytecode for this constant expression if this is not a literal.
  Bytes bytecode_;
  // The value if this is a literal.
  LitVal literal_;
  // The value type of this constant expression in either case.
  ValType type_;

 public:
  InitExpr() : kind_(InitExprKind::None) {}

  explicit InitExpr(LitVal literal)
      : kind_(InitExprKind::Literal),
        literal_(literal),
        type_(literal.type()) {}

  // Decode and validate a constant expression given at the current
  // position of the decoder. Upon failure, the decoder contains the failure
  // message or else the failure was an OOM.
  static bool decodeAndValidate(Decoder& d, ModuleEnvironment* env,
                                ValType expected, InitExpr* expr);

  // Evaluate the constant expresssion with the given context. This may only
  // fail due to an OOM, as all InitExpr's are required to have been validated.
  bool evaluate(JSContext* cx, const ValVector& globalImportValues,
                HandleWasmInstanceObject instanceObj,
                MutableHandleVal result) const;

  bool isLiteral() const { return kind_ == InitExprKind::Literal; }

  // Gets the result of this expression if it was determined to be a literal.
  LitVal literal() const {
    MOZ_ASSERT(isLiteral());
    return literal_;
  }

  // Get the type of the resulting value of this expression.
  ValType type() const { return type_; }

  // Allow moving, but not implicit copying
  InitExpr(const InitExpr&) = delete;
  InitExpr& operator=(const InitExpr&) = delete;
  InitExpr(InitExpr&&) = default;
  InitExpr& operator=(InitExpr&&) = default;

  // Allow explicit cloning
  [[nodiscard]] bool clone(const InitExpr& src);

  WASM_DECLARE_SERIALIZABLE(InitExpr)
};

// CacheableChars is used to cacheably store UniqueChars.

struct CacheableChars : UniqueChars {
  CacheableChars() = default;
  explicit CacheableChars(char* ptr) : UniqueChars(ptr) {}
  MOZ_IMPLICIT CacheableChars(UniqueChars&& rhs)
      : UniqueChars(std::move(rhs)) {}
  WASM_DECLARE_SERIALIZABLE(CacheableChars)
};

using CacheableCharsVector = Vector<CacheableChars, 0, SystemAllocPolicy>;

// Import describes a single wasm import. An ImportVector describes all
// of a single module's imports.
//
// ImportVector is built incrementally by ModuleGenerator and then stored
// immutably by Module.

struct Import {
  CacheableChars module;
  CacheableChars field;
  DefinitionKind kind;

  Import() = default;
  Import(UniqueChars&& module, UniqueChars&& field, DefinitionKind kind)
      : module(std::move(module)), field(std::move(field)), kind(kind) {}

  WASM_DECLARE_SERIALIZABLE(Import)
};

using ImportVector = Vector<Import, 0, SystemAllocPolicy>;

// Export describes the export of a definition in a Module to a field in the
// export object. The Export stores the index of the exported item in the
// appropriate type-specific module data structure (function table, global
// table, table table, and - eventually - memory table).
//
// Note a single definition can be exported by multiple Exports in the
// ExportVector.
//
// ExportVector is built incrementally by ModuleGenerator and then stored
// immutably by Module.

class Export {
  CacheableChars fieldName_;
  struct CacheablePod {
    DefinitionKind kind_;
    uint32_t index_;
  } pod;

 public:
  Export() = default;
  explicit Export(UniqueChars fieldName, uint32_t index, DefinitionKind kind);
  explicit Export(UniqueChars fieldName, DefinitionKind kind);

  const char* fieldName() const { return fieldName_.get(); }

  DefinitionKind kind() const { return pod.kind_; }
  uint32_t funcIndex() const;
#ifdef ENABLE_WASM_EXCEPTIONS
  uint32_t eventIndex() const;
#endif
  uint32_t globalIndex() const;
  uint32_t tableIndex() const;

  WASM_DECLARE_SERIALIZABLE(Export)
};

using ExportVector = Vector<Export, 0, SystemAllocPolicy>;

// FuncFlags provides metadata for a function definition.

enum class FuncFlags : uint8_t {
  None = 0x0,
  // The function maybe be accessible by JS and needs thunks generated for it.
  // See `[SMDOC] Exported wasm functions and the jit-entry stubs` in
  // WasmJS.cpp for more information.
  Exported = 0x1,
  // The function should have thunks generated upon instantiation, not upon
  // first call. May only be set if `Exported` is set.
  Eager = 0x2,
  // The function can be the target of a ref.func instruction in the code
  // section. May only be set if `Exported` is set.
  CanRefFunc = 0x4,
};

// A FuncDesc describes a single function definition.

class TypeIdDesc;

struct FuncDesc {
  FuncType* type;
  TypeIdDesc* typeId;
  // Bit pack to keep this struct small on 32-bit systems
  uint32_t typeIndex : 24;
  FuncFlags flags : 8;

  // Assert that the bit packing scheme is viable
  static_assert(MaxTypes <= (1 << 24) - 1);
  static_assert(sizeof(FuncFlags) == sizeof(uint8_t));

  FuncDesc() = default;
  FuncDesc(FuncType* type, TypeIdDesc* typeId, uint32_t typeIndex)
      : type(type),
        typeId(typeId),
        typeIndex(typeIndex),
        flags(FuncFlags::None) {}

  bool isExported() const {
    return uint8_t(flags) & uint8_t(FuncFlags::Exported);
  }
  bool isEager() const { return uint8_t(flags) & uint8_t(FuncFlags::Eager); }
  bool canRefFunc() const {
    return uint8_t(flags) & uint8_t(FuncFlags::CanRefFunc);
  }
};

using FuncDescVector = Vector<FuncDesc, 0, SystemAllocPolicy>;

// A GlobalDesc describes a single global variable.
//
// wasm can import and export mutable and immutable globals.
//
// asm.js can import mutable and immutable globals, but a mutable global has a
// location that is private to the module, and its initial value is copied into
// that cell from the environment.  asm.js cannot export globals.

enum class GlobalKind { Import, Constant, Variable };

class GlobalDesc {
  GlobalKind kind_;
  // Stores the value type of this global for all kinds, and the initializer
  // expression when `constant` or `variable`.
  InitExpr initial_;
  // Metadata for the global when `variable` or `import`.
  unsigned offset_;
  bool isMutable_;
  bool isWasm_;
  bool isExport_;
  // Metadata for the global when `import`.
  uint32_t importIndex_;

  // Private, as they have unusual semantics.

  bool isExport() const { return !isConstant() && isExport_; }
  bool isWasm() const { return !isConstant() && isWasm_; }

 public:
  GlobalDesc() = default;

  explicit GlobalDesc(InitExpr&& initial, bool isMutable,
                      ModuleKind kind = ModuleKind::Wasm)
      : kind_((isMutable || !initial.isLiteral()) ? GlobalKind::Variable
                                                  : GlobalKind::Constant) {
    initial_ = std::move(initial);
    if (isVariable()) {
      isMutable_ = isMutable;
      isWasm_ = kind == Wasm;
      isExport_ = false;
      offset_ = UINT32_MAX;
    }
  }

  explicit GlobalDesc(ValType type, bool isMutable, uint32_t importIndex,
                      ModuleKind kind = ModuleKind::Wasm)
      : kind_(GlobalKind::Import) {
    initial_ = InitExpr(LitVal(type));
    importIndex_ = importIndex;
    isMutable_ = isMutable;
    isWasm_ = kind == Wasm;
    isExport_ = false;
    offset_ = UINT32_MAX;
  }

  void setOffset(unsigned offset) {
    MOZ_ASSERT(!isConstant());
    MOZ_ASSERT(offset_ == UINT32_MAX);
    offset_ = offset;
  }
  unsigned offset() const {
    MOZ_ASSERT(!isConstant());
    MOZ_ASSERT(offset_ != UINT32_MAX);
    return offset_;
  }

  void setIsExport() {
    if (!isConstant()) {
      isExport_ = true;
    }
  }

  GlobalKind kind() const { return kind_; }
  bool isVariable() const { return kind_ == GlobalKind::Variable; }
  bool isConstant() const { return kind_ == GlobalKind::Constant; }
  bool isImport() const { return kind_ == GlobalKind::Import; }

  bool isMutable() const { return !isConstant() && isMutable_; }
  const InitExpr& initExpr() const {
    MOZ_ASSERT(!isImport());
    return initial_;
  }
  uint32_t importIndex() const {
    MOZ_ASSERT(isImport());
    return importIndex_;
  }

  LitVal constantValue() const { return initial_.literal(); }

  // If isIndirect() is true then storage for the value is not in the
  // instance's global area, but in a WasmGlobalObject::Cell hanging off a
  // WasmGlobalObject; the global area contains a pointer to the Cell.
  //
  // We don't want to indirect unless we must, so only mutable, exposed
  // globals are indirected - in all other cases we copy values into and out
  // of their module.
  //
  // Note that isIndirect() isn't equivalent to getting a WasmGlobalObject:
  // an immutable exported global will still get an object, but will not be
  // indirect.
  bool isIndirect() const {
    return isMutable() && isWasm() && (isImport() || isExport());
  }

  ValType type() const { return initial_.type(); }

  WASM_DECLARE_SERIALIZABLE(GlobalDesc)
};

using GlobalDescVector = Vector<GlobalDesc, 0, SystemAllocPolicy>;

// An EventDesc describes a single event for non-local control flow, such as
// for exceptions.

#ifdef ENABLE_WASM_EXCEPTIONS
struct EventDesc {
  EventKind kind;
  ValTypeVector type;
  bool isExport;

  EventDesc(EventKind kind, ValTypeVector&& type, bool isExport = false)
      : kind(kind), type(std::move(type)), isExport(isExport) {}

  ResultType resultType() const { return ResultType::Vector(type); }
};

using EventDescVector = Vector<EventDesc, 0, SystemAllocPolicy>;
#endif

// When a ElemSegment is "passive" it is shared between a wasm::Module and its
// wasm::Instances. To allow each segment to be released as soon as the last
// Instance elem.drops it and the Module is destroyed, each ElemSegment is
// individually atomically ref-counted.

struct ElemSegment : AtomicRefCounted<ElemSegment> {
  enum class Kind {
    Active,
    Passive,
    Declared,
  };

  Kind kind;
  uint32_t tableIndex;
  RefType elemType;
  Maybe<InitExpr> offsetIfActive;
  Uint32Vector elemFuncIndices;  // Element may be NullFuncIndex

  bool active() const { return kind == Kind::Active; }

  const InitExpr& offset() const { return *offsetIfActive; }

  size_t length() const { return elemFuncIndices.length(); }

  WASM_DECLARE_SERIALIZABLE(ElemSegment)
};

// NullFuncIndex represents the case when an element segment (of type funcref)
// contains a null element.
constexpr uint32_t NullFuncIndex = UINT32_MAX;
static_assert(NullFuncIndex > MaxFuncs, "Invariant");

using MutableElemSegment = RefPtr<ElemSegment>;
using SharedElemSegment = SerializableRefPtr<const ElemSegment>;
using ElemSegmentVector = Vector<SharedElemSegment, 0, SystemAllocPolicy>;

// DataSegmentEnv holds the initial results of decoding a data segment from the
// bytecode and is stored in the ModuleEnvironment during compilation. When
// compilation completes, (non-Env) DataSegments are created and stored in
// the wasm::Module which contain copies of the data segment payload. This
// allows non-compilation uses of wasm validation to avoid expensive copies.
//
// When a DataSegment is "passive" it is shared between a wasm::Module and its
// wasm::Instances. To allow each segment to be released as soon as the last
// Instance mem.drops it and the Module is destroyed, each DataSegment is
// individually atomically ref-counted.

struct DataSegmentEnv {
  Maybe<InitExpr> offsetIfActive;
  uint32_t bytecodeOffset;
  uint32_t length;
};

using DataSegmentEnvVector = Vector<DataSegmentEnv, 0, SystemAllocPolicy>;

struct DataSegment : AtomicRefCounted<DataSegment> {
  Maybe<InitExpr> offsetIfActive;
  Bytes bytes;

  DataSegment() = default;

  bool active() const { return !!offsetIfActive; }

  const InitExpr& offset() const { return *offsetIfActive; }

  [[nodiscard]] bool init(const ShareableBytes& bytecode,
                          const DataSegmentEnv& src) {
    if (src.offsetIfActive) {
      offsetIfActive.emplace();
      if (!offsetIfActive->clone(*src.offsetIfActive)) {
        return false;
      }
    }
    return bytes.append(bytecode.begin() + src.bytecodeOffset, src.length);
  }

  WASM_DECLARE_SERIALIZABLE(DataSegment)
};

using MutableDataSegment = RefPtr<DataSegment>;
using SharedDataSegment = SerializableRefPtr<const DataSegment>;
using DataSegmentVector = Vector<SharedDataSegment, 0, SystemAllocPolicy>;

// The CustomSection(Env) structs are like DataSegment(Env): CustomSectionEnv is
// stored in the ModuleEnvironment and CustomSection holds a copy of the payload
// and is stored in the wasm::Module.

struct CustomSectionEnv {
  uint32_t nameOffset;
  uint32_t nameLength;
  uint32_t payloadOffset;
  uint32_t payloadLength;
};

using CustomSectionEnvVector = Vector<CustomSectionEnv, 0, SystemAllocPolicy>;

struct CustomSection {
  Bytes name;
  SharedBytes payload;

  WASM_DECLARE_SERIALIZABLE(CustomSection)
};

using CustomSectionVector = Vector<CustomSection, 0, SystemAllocPolicy>;

// A Name represents a string of utf8 chars embedded within the name custom
// section. The offset of a name is expressed relative to the beginning of the
// name section's payload so that Names can stored in wasm::Code, which only
// holds the name section's bytes, not the whole bytecode.

struct Name {
  // All fields are treated as cacheable POD:
  uint32_t offsetInNamePayload;
  uint32_t length;

  Name() : offsetInNamePayload(UINT32_MAX), length(0) {}
};

using NameVector = Vector<Name, 0, SystemAllocPolicy>;

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

  void renumber(const RenumberMap& map) {
    switch (kind_) {
      case TypeDefKind::Func:
        funcType_.renumber(map);
        break;
      case TypeDefKind::Struct:
        structType_.renumber(map);
        break;
      case TypeDefKind::Array:
        arrayType_.renumber(map);
        break;
      case TypeDefKind::None:
        break;
    }
  }
  void offsetTypeIndex(uint32_t offsetBy) {
    switch (kind_) {
      case TypeDefKind::Func:
        funcType_.offsetTypeIndex(offsetBy);
        break;
      case TypeDefKind::Struct:
        structType_.offsetTypeIndex(offsetBy);
        break;
      case TypeDefKind::Array:
        arrayType_.offsetTypeIndex(offsetBy);
        break;
      case TypeDefKind::None:
        break;
    }
  }

  WASM_DECLARE_SERIALIZABLE(TypeDef)
};

using TypeDefVector = Vector<TypeDef, 0, SystemAllocPolicy>;

// TypeIdDesc describes the runtime representation of a TypeDef suitable for
// type equality checks. The kind of representation depends on whether the type
// is a function or a struct. This will likely be simplified in the future once
// mutually recursives types are able to be collected.
//
// For functions, a FuncType is allocated and stored in a process-wide hash
// table, so that pointer equality implies structural equality. As an
// optimization for the 99% case where the FuncType has a small number of
// parameters, the FuncType is bit-packed into a uint32 immediate value so that
// integer equality implies structural equality. Both cases can be handled with
// a single comparison by always setting the LSB for the immediates
// (the LSB is necessarily 0 for allocated FuncType pointers due to alignment).
//
// TODO: Write description for StructTypes once it is well formed.

class TypeIdDesc {
 public:
  static const uintptr_t ImmediateBit = 0x1;

 private:
  TypeIdDescKind kind_;
  size_t bits_;

  TypeIdDesc(TypeIdDescKind kind, size_t bits) : kind_(kind), bits_(bits) {}

 public:
  TypeIdDescKind kind() const { return kind_; }
  static bool isGlobal(const TypeDef& type);

  TypeIdDesc() : kind_(TypeIdDescKind::None), bits_(0) {}
  static TypeIdDesc global(const TypeDef& type, uint32_t globalDataOffset);
  static TypeIdDesc immediate(const TypeDef& type);

  bool isGlobal() const { return kind_ == TypeIdDescKind::Global; }

  size_t immediate() const {
    MOZ_ASSERT(kind_ == TypeIdDescKind::Immediate);
    return bits_;
  }
  uint32_t globalDataOffset() const {
    MOZ_ASSERT(kind_ == TypeIdDescKind::Global);
    return bits_;
  }
};

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

  WASM_DECLARE_SERIALIZABLE(TypeDefWithId)
};

using TypeDefWithIdVector = Vector<TypeDefWithId, 0, SystemAllocPolicy>;
using TypeDefWithIdPtrVector =
    Vector<const TypeDefWithId*, 0, SystemAllocPolicy>;

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

class TypeContext {
  FeatureArgs features_;
  TypeDefVector types_;

 public:
  TypeContext(const FeatureArgs& features, TypeDefVector&& types)
      : features_(features), types_(std::move(types)) {}

  size_t sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
    return types_.sizeOfExcludingThis(mallocSizeOf);
  }

  // Disallow copy, allow move initialization
  TypeContext(const TypeContext&) = delete;
  TypeContext& operator=(const TypeContext&) = delete;
  TypeContext(TypeContext&&) = default;
  TypeContext& operator=(TypeContext&&) = default;

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

  [[nodiscard]] bool transferTypes(const TypeDefWithIdVector& types,
                                   uint32_t* baseIndex) {
    *baseIndex = length();
    if (!resize(*baseIndex + types.length())) {
      return false;
    }
    for (uint32_t i = 0; i < types.length(); i++) {
      if (!types_[*baseIndex + i].clone(types[i])) {
        return false;
      }
      types_[*baseIndex + i].offsetTypeIndex(*baseIndex);
    }
    return true;
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
  TypeResult isEquivalent(T one, T two, TypeCache* cache) const {
    // Anything's equal to itself.
    if (one == two) {
      return TypeResult::True;
    }

    // A reference may be equal to another reference
    if (one.isReference() && two.isReference()) {
      return isRefEquivalent(one.refType(), two.refType(), cache);
    }

#ifdef ENABLE_WASM_GC
    // An rtt may be a equal to another rtt
    if (one.isRtt() && two.isRtt()) {
      return isTypeIndexEquivalent(one.typeIndex(), two.typeIndex(), cache);
    }
#endif

    return TypeResult::False;
  }

  TypeResult isRefEquivalent(RefType one, RefType two, TypeCache* cache) const;
#ifdef ENABLE_WASM_FUNCTION_REFERENCES
  TypeResult isTypeIndexEquivalent(uint32_t one, uint32_t two,
                                   TypeCache* cache) const;
#endif
#ifdef ENABLE_WASM_GC
  TypeResult isStructEquivalent(uint32_t oneIndex, uint32_t twoIndex,
                                TypeCache* cache) const;
  TypeResult isStructFieldEquivalent(const StructField one,
                                     const StructField two,
                                     TypeCache* cache) const;
  TypeResult isArrayEquivalent(uint32_t oneIndex, uint32_t twoIndex,
                               TypeCache* cache) const;
  TypeResult isArrayElementEquivalent(const ArrayType& one,
                                      const ArrayType& two,
                                      TypeCache* cache) const;
#endif

  // Subtyping

  template <class T>
  TypeResult isSubtypeOf(T one, T two, TypeCache* cache) const {
    // Anything's a subtype of itself.
    if (one == two) {
      return TypeResult::True;
    }

    // A reference may be a subtype of another reference
    if (one.isReference() && two.isReference()) {
      return isRefSubtypeOf(one.refType(), two.refType(), cache);
    }

    // An rtt may be a subtype of another rtt
#ifdef ENABLE_WASM_GC
    if (one.isRtt() && two.isRtt()) {
      return isTypeIndexEquivalent(one.typeIndex(), two.typeIndex(), cache);
    }
#endif

    return TypeResult::False;
  }

  TypeResult isRefSubtypeOf(RefType one, RefType two, TypeCache* cache) const;
#ifdef ENABLE_WASM_FUNCTION_REFERENCES
  TypeResult isTypeIndexSubtypeOf(uint32_t one, uint32_t two,
                                  TypeCache* cache) const;
#endif

#ifdef ENABLE_WASM_GC
  TypeResult isStructSubtypeOf(uint32_t oneIndex, uint32_t twoIndex,
                               TypeCache* cache) const;
  TypeResult isStructFieldSubtypeOf(const StructField one,
                                    const StructField two,
                                    TypeCache* cache) const;
  TypeResult isArraySubtypeOf(uint32_t oneIndex, uint32_t twoIndex,
                              TypeCache* cache) const;
  TypeResult isArrayElementSubtypeOf(const ArrayType& one, const ArrayType& two,
                                     TypeCache* cache) const;
#endif
};

class TypeHandle {
 private:
  uint32_t index_;

 public:
  explicit TypeHandle(uint32_t index) : index_(index) {}

  TypeHandle(const TypeHandle&) = default;
  TypeHandle& operator=(const TypeHandle&) = default;

  TypeDef& get(TypeContext* tycx) const { return tycx->type(index_); }
  const TypeDef& get(const TypeContext* tycx) const {
    return tycx->type(index_);
  }

  uint32_t index() const { return index_; }
};

// A wrapper around the bytecode offset of a wasm instruction within a whole
// module, used for trap offsets or call offsets. These offsets should refer to
// the first byte of the instruction that triggered the trap / did the call and
// should ultimately derive from OpIter::bytecodeOffset.

class BytecodeOffset {
  static const uint32_t INVALID = -1;
  uint32_t offset_;

 public:
  BytecodeOffset() : offset_(INVALID) {}
  explicit BytecodeOffset(uint32_t offset) : offset_(offset) {}

  bool isValid() const { return offset_ != INVALID; }
  uint32_t offset() const {
    MOZ_ASSERT(isValid());
    return offset_;
  }
};

// A TrapSite (in the TrapSiteVector for a given Trap code) represents a wasm
// instruction at a given bytecode offset that can fault at the given pc offset.
// When such a fault occurs, a signal/exception handler looks up the TrapSite to
// confirm the fault is intended/safe and redirects pc to the trap stub.

struct TrapSite {
  uint32_t pcOffset;
  BytecodeOffset bytecode;

  TrapSite() : pcOffset(-1), bytecode() {}
  TrapSite(uint32_t pcOffset, BytecodeOffset bytecode)
      : pcOffset(pcOffset), bytecode(bytecode) {}

  void offsetBy(uint32_t offset) { pcOffset += offset; }
};

WASM_DECLARE_POD_VECTOR(TrapSite, TrapSiteVector)

struct TrapSiteVectorArray
    : EnumeratedArray<Trap, Trap::Limit, TrapSiteVector> {
  bool empty() const;
  void clear();
  void swap(TrapSiteVectorArray& rhs);
  void shrinkStorageToFit();

  WASM_DECLARE_SERIALIZABLE(TrapSiteVectorArray)
};

// On trap, the bytecode offset to be reported in callstacks is saved.

struct TrapData {
  // The resumePC indicates where, if the trap doesn't throw, the trap stub
  // should jump to after restoring all register state.
  void* resumePC;

  // The unwoundPC is the PC after adjustment by wasm::StartUnwinding(), which
  // basically unwinds partially-construted wasm::Frames when pc is in the
  // prologue/epilogue. Stack traces during a trap should use this PC since
  // it corresponds to the JitActivation::wasmExitFP.
  void* unwoundPC;

  Trap trap;
  uint32_t bytecodeOffset;
};

// The (,Callable,Func)Offsets classes are used to record the offsets of
// different key points in a CodeRange during compilation.

struct Offsets {
  explicit Offsets(uint32_t begin = 0, uint32_t end = 0)
      : begin(begin), end(end) {}

  // These define a [begin, end) contiguous range of instructions compiled
  // into a CodeRange.
  uint32_t begin;
  uint32_t end;
};

struct CallableOffsets : Offsets {
  MOZ_IMPLICIT CallableOffsets(uint32_t ret = 0) : Offsets(), ret(ret) {}

  // The offset of the return instruction precedes 'end' by a variable number
  // of instructions due to out-of-line codegen.
  uint32_t ret;
};

struct JitExitOffsets : CallableOffsets {
  MOZ_IMPLICIT JitExitOffsets()
      : CallableOffsets(), untrustedFPStart(0), untrustedFPEnd(0) {}

  // There are a few instructions in the Jit exit where FP may be trash
  // (because it may have been clobbered by the JS Jit), known as the
  // untrusted FP zone.
  uint32_t untrustedFPStart;
  uint32_t untrustedFPEnd;
};

struct FuncOffsets : CallableOffsets {
  MOZ_IMPLICIT FuncOffsets()
      : CallableOffsets(), uncheckedCallEntry(0), tierEntry(0) {}

  // Function CodeRanges have a checked call entry which takes an extra
  // signature argument which is checked against the callee's signature before
  // falling through to the normal prologue. The checked call entry is thus at
  // the beginning of the CodeRange and the unchecked call entry is at some
  // offset after the checked call entry.
  uint32_t uncheckedCallEntry;

  // The tierEntry is the point within a function to which the patching code
  // within a Tier-1 function jumps.  It could be the instruction following
  // the jump in the Tier-1 function, or the point following the standard
  // prologue within a Tier-2 function.
  uint32_t tierEntry;
};

using FuncOffsetsVector = Vector<FuncOffsets, 0, SystemAllocPolicy>;

// A CodeRange describes a single contiguous range of code within a wasm
// module's code segment. A CodeRange describes what the code does and, for
// function bodies, the name and source coordinates of the function.

class CodeRange {
 public:
  enum Kind {
    Function,          // function definition
    InterpEntry,       // calls into wasm from C++
    JitEntry,          // calls into wasm from jit code
    ImportInterpExit,  // slow-path calling from wasm into C++ interp
    ImportJitExit,     // fast-path calling from wasm into jit code
    BuiltinThunk,      // fast-path calling from wasm into a C++ native
    TrapExit,          // calls C++ to report and jumps to throw stub
    DebugTrap,         // calls C++ to handle debug event
    FarJumpIsland,     // inserted to connect otherwise out-of-range insns
    Throw              // special stack-unwinding stub jumped to by other stubs
  };

 private:
  // All fields are treated as cacheable POD:
  uint32_t begin_;
  uint32_t ret_;
  uint32_t end_;
  union {
    struct {
      uint32_t funcIndex_;
      union {
        struct {
          uint32_t lineOrBytecode_;
          uint8_t beginToUncheckedCallEntry_;
          uint8_t beginToTierEntry_;
        } func;
        struct {
          uint16_t beginToUntrustedFPStart_;
          uint16_t beginToUntrustedFPEnd_;
        } jitExit;
      };
    };
    Trap trap_;
  } u;
  Kind kind_ : 8;

 public:
  CodeRange() = default;
  CodeRange(Kind kind, Offsets offsets);
  CodeRange(Kind kind, uint32_t funcIndex, Offsets offsets);
  CodeRange(Kind kind, CallableOffsets offsets);
  CodeRange(Kind kind, uint32_t funcIndex, CallableOffsets);
  CodeRange(uint32_t funcIndex, JitExitOffsets offsets);
  CodeRange(uint32_t funcIndex, uint32_t lineOrBytecode, FuncOffsets offsets);

  void offsetBy(uint32_t offset) {
    begin_ += offset;
    end_ += offset;
    if (hasReturn()) {
      ret_ += offset;
    }
  }

  // All CodeRanges have a begin and end.

  uint32_t begin() const { return begin_; }
  uint32_t end() const { return end_; }

  // Other fields are only available for certain CodeRange::Kinds.

  Kind kind() const { return kind_; }

  bool isFunction() const { return kind() == Function; }
  bool isImportExit() const {
    return kind() == ImportJitExit || kind() == ImportInterpExit ||
           kind() == BuiltinThunk;
  }
  bool isImportInterpExit() const { return kind() == ImportInterpExit; }
  bool isImportJitExit() const { return kind() == ImportJitExit; }
  bool isTrapExit() const { return kind() == TrapExit; }
  bool isDebugTrap() const { return kind() == DebugTrap; }
  bool isThunk() const { return kind() == FarJumpIsland; }

  // Function, import exits and trap exits have standard callable prologues
  // and epilogues. Asynchronous frame iteration needs to know the offset of
  // the return instruction to calculate the frame pointer.

  bool hasReturn() const {
    return isFunction() || isImportExit() || isDebugTrap();
  }
  uint32_t ret() const {
    MOZ_ASSERT(hasReturn());
    return ret_;
  }

  // Functions, export stubs and import stubs all have an associated function
  // index.

  bool isJitEntry() const { return kind() == JitEntry; }
  bool isInterpEntry() const { return kind() == InterpEntry; }
  bool isEntry() const { return isInterpEntry() || isJitEntry(); }
  bool hasFuncIndex() const {
    return isFunction() || isImportExit() || isEntry();
  }
  uint32_t funcIndex() const {
    MOZ_ASSERT(hasFuncIndex());
    return u.funcIndex_;
  }

  // TrapExit CodeRanges have a Trap field.

  Trap trap() const {
    MOZ_ASSERT(isTrapExit());
    return u.trap_;
  }

  // Function CodeRanges have two entry points: one for normal calls (with a
  // known signature) and one for table calls (which involves dynamic
  // signature checking).

  uint32_t funcCheckedCallEntry() const {
    MOZ_ASSERT(isFunction());
    return begin_;
  }
  uint32_t funcUncheckedCallEntry() const {
    MOZ_ASSERT(isFunction());
    return begin_ + u.func.beginToUncheckedCallEntry_;
  }
  uint32_t funcTierEntry() const {
    MOZ_ASSERT(isFunction());
    return begin_ + u.func.beginToTierEntry_;
  }
  uint32_t funcLineOrBytecode() const {
    MOZ_ASSERT(isFunction());
    return u.func.lineOrBytecode_;
  }

  // ImportJitExit have a particular range where the value of FP can't be
  // trusted for profiling and thus must be ignored.

  uint32_t jitExitUntrustedFPStart() const {
    MOZ_ASSERT(isImportJitExit());
    return begin_ + u.jitExit.beginToUntrustedFPStart_;
  }
  uint32_t jitExitUntrustedFPEnd() const {
    MOZ_ASSERT(isImportJitExit());
    return begin_ + u.jitExit.beginToUntrustedFPEnd_;
  }

  // A sorted array of CodeRanges can be looked up via BinarySearch and
  // OffsetInCode.

  struct OffsetInCode {
    size_t offset;
    explicit OffsetInCode(size_t offset) : offset(offset) {}
    bool operator==(const CodeRange& rhs) const {
      return offset >= rhs.begin() && offset < rhs.end();
    }
    bool operator<(const CodeRange& rhs) const { return offset < rhs.begin(); }
  };
};

WASM_DECLARE_POD_VECTOR(CodeRange, CodeRangeVector)

extern const CodeRange* LookupInSorted(const CodeRangeVector& codeRanges,
                                       CodeRange::OffsetInCode target);

// While the frame-pointer chain allows the stack to be unwound without
// metadata, Error.stack still needs to know the line/column of every call in
// the chain. A CallSiteDesc describes a single callsite to which CallSite adds
// the metadata necessary to walk up to the next frame. Lastly CallSiteAndTarget
// adds the function index of the callee.

class CallSiteDesc {
  static constexpr size_t LINE_OR_BYTECODE_BITS_SIZE = 29;
  uint32_t lineOrBytecode_ : LINE_OR_BYTECODE_BITS_SIZE;
  uint32_t kind_ : 3;

 public:
  static constexpr uint32_t MAX_LINE_OR_BYTECODE_VALUE =
      (1 << LINE_OR_BYTECODE_BITS_SIZE) - 1;

  enum Kind {
    Func,        // pc-relative call to a specific function
    Dynamic,     // dynamic callee called via register
    Symbolic,    // call to a single symbolic callee
    EnterFrame,  // call to a enter frame handler
    LeaveFrame,  // call to a leave frame handler
    Breakpoint   // call to instruction breakpoint
  };
  CallSiteDesc() : lineOrBytecode_(0), kind_(0) {}
  explicit CallSiteDesc(Kind kind) : lineOrBytecode_(0), kind_(kind) {
    MOZ_ASSERT(kind == Kind(kind_));
  }
  CallSiteDesc(uint32_t lineOrBytecode, Kind kind)
      : lineOrBytecode_(lineOrBytecode), kind_(kind) {
    MOZ_ASSERT(kind == Kind(kind_));
    MOZ_ASSERT(lineOrBytecode == lineOrBytecode_);
  }
  uint32_t lineOrBytecode() const { return lineOrBytecode_; }
  Kind kind() const { return Kind(kind_); }
  bool mightBeCrossInstance() const { return kind() == CallSiteDesc::Dynamic; }
};

class CallSite : public CallSiteDesc {
  uint32_t returnAddressOffset_;

 public:
  CallSite() : returnAddressOffset_(0) {}

  CallSite(CallSiteDesc desc, uint32_t returnAddressOffset)
      : CallSiteDesc(desc), returnAddressOffset_(returnAddressOffset) {}

  void offsetBy(int32_t delta) { returnAddressOffset_ += delta; }
  uint32_t returnAddressOffset() const { return returnAddressOffset_; }
};

WASM_DECLARE_POD_VECTOR(CallSite, CallSiteVector)

// A CallSiteTarget describes the callee of a CallSite, either a function or a
// trap exit. Although checked in debug builds, a CallSiteTarget doesn't
// officially know whether it targets a function or trap, relying on the Kind of
// the CallSite to discriminate.

class CallSiteTarget {
  uint32_t packed_;
#ifdef DEBUG
  enum Kind { None, FuncIndex, TrapExit } kind_;
#endif

 public:
  explicit CallSiteTarget()
      : packed_(UINT32_MAX)
#ifdef DEBUG
        ,
        kind_(None)
#endif
  {
  }

  explicit CallSiteTarget(uint32_t funcIndex)
      : packed_(funcIndex)
#ifdef DEBUG
        ,
        kind_(FuncIndex)
#endif
  {
  }

  explicit CallSiteTarget(Trap trap)
      : packed_(uint32_t(trap))
#ifdef DEBUG
        ,
        kind_(TrapExit)
#endif
  {
  }

  uint32_t funcIndex() const {
    MOZ_ASSERT(kind_ == FuncIndex);
    return packed_;
  }

  Trap trap() const {
    MOZ_ASSERT(kind_ == TrapExit);
    MOZ_ASSERT(packed_ < uint32_t(Trap::Limit));
    return Trap(packed_);
  }
};

using CallSiteTargetVector = Vector<CallSiteTarget, 0, SystemAllocPolicy>;

// WasmTryNotes are stored in a vector that acts as an exception table for
// wasm try-catch blocks. These represent the information needed to take
// exception handling actions after a throw is executed.
struct WasmTryNote {
  explicit WasmTryNote(uint32_t begin = 0, uint32_t end = 0,
                       uint32_t framePushed = 0)
      : begin(begin), end(end), framePushed(framePushed) {}

  uint32_t begin;        // Begin code offset of try instructions.
  uint32_t end;          // End code offset of try instructions.
  uint32_t entryPoint;   // The offset of the landing pad.
  uint32_t framePushed;  // Track offset from frame of stack pointer.

  void offsetBy(uint32_t offset) {
    begin += offset;
    end += offset;
    entryPoint += offset;
  }

  bool operator<(const WasmTryNote& other) const {
    if (end == other.end) {
      return begin > other.begin;
    }
    return end < other.end;
  }
};

WASM_DECLARE_POD_VECTOR(WasmTryNote, WasmTryNoteVector)

// Represents the resizable limits of memories and tables.

struct Limits {
  uint64_t initial;
  Maybe<uint64_t> maximum;

  // `shared` is Shareable::False for tables but may be Shareable::True for
  // memories.
  Shareable shared;

  Limits() = default;
  explicit Limits(uint64_t initial, const Maybe<uint64_t>& maximum = Nothing(),
                  Shareable shared = Shareable::False)
      : initial(initial), maximum(maximum), shared(shared) {}
};

// TableDesc describes a table as well as the offset of the table's base pointer
// in global memory.
//
// A TableDesc contains the element type and whether the table is for asm.js,
// which determines the table representation.
//  - ExternRef: a wasm anyref word (wasm::AnyRef)
//  - FuncRef: a two-word FunctionTableElem (wasm indirect call ABI)
//  - FuncRef (if `isAsmJS`): a two-word FunctionTableElem (asm.js ABI)
// Eventually there should be a single unified AnyRef representation.

struct TableDesc {
  RefType elemType;
  bool importedOrExported;
  bool isAsmJS;
  uint32_t globalDataOffset;
  uint32_t initialLength;
  Maybe<uint32_t> maximumLength;

  TableDesc() = default;
  TableDesc(RefType elemType, uint32_t initialLength,
            Maybe<uint32_t> maximumLength, bool isAsmJS,
            bool importedOrExported = false)
      : elemType(elemType),
        importedOrExported(importedOrExported),
        isAsmJS(isAsmJS),
        globalDataOffset(UINT32_MAX),
        initialLength(initialLength),
        maximumLength(maximumLength) {}
};

using TableDescVector = Vector<TableDesc, 0, SystemAllocPolicy>;

// TLS data for a single module instance.
//
// Every WebAssembly function expects to be passed a hidden TLS pointer argument
// in WasmTlsReg. The TLS pointer argument points to a TlsData struct.
// Compiled functions expect that the TLS pointer does not change for the
// lifetime of the thread.
//
// There is a TlsData per module instance per thread, so inter-module calls need
// to pass the TLS pointer appropriate for the callee module.
//
// After the TlsData struct follows the module's declared TLS variables.

struct TlsData {
  // Pointer to the base of the default memory (or null if there is none).
  uint8_t* memoryBase;

  // Bounds check limit in bytes (or zero if there is no memory).  This is
  // 64-bits on 64-bit systems so as to allow for heap lengths up to and beyond
  // 4GB, and 32-bits on 32-bit systems, where heaps are limited to 2GB.
  //
  // On 64-bit systems, the upper bits of this value will be zero and 32-bit
  // bounds checks can be used under the following circumstances:
  //
  // - The heap is for asm.js; asm.js heaps are limited to 2GB
  // - The max size of the heap is encoded in the module and is less than 4GB
  // - Cranelift is present in the system; Cranelift-compatible heaps are
  //   limited to 4GB-128K
  //
  // All our jits require little-endian byte order, so the address of the 32-bit
  // heap limit is the same as the address of the 64-bit heap limit: the address
  // of this member.
  uintptr_t boundsCheckLimit;

  // Pointer to the Instance that contains this TLS data.
  Instance* instance;

  // Equal to instance->realm_.
  JS::Realm* realm;

  // The containing JSContext.
  JSContext* cx;

  // The class_ of WasmValueBox, this is a per-process value.
  const JSClass* valueBoxClass;

  // Usually equal to cx->stackLimitForJitCode(JS::StackForUntrustedScript),
  // but can be racily set to trigger immediate trap as an opportunity to
  // CheckForInterrupt without an additional branch.
  Atomic<uintptr_t, mozilla::Relaxed> stackLimit;

  // Set to 1 when wasm should call CheckForInterrupt.
  Atomic<uint32_t, mozilla::Relaxed> interrupt;

  uint8_t* addressOfNeedsIncrementalBarrier;

  // Methods to set, test and clear the above two fields. Both interrupt
  // fields are Relaxed and so no consistency/ordering can be assumed.
  void setInterrupt();
  bool isInterrupted() const;
  void resetInterrupt(JSContext* cx);

  // Pointer that should be freed (due to padding before the TlsData).
  void* allocatedBase;

  // When compiling with tiering, the jumpTable has one entry for each
  // baseline-compiled function.
  void** jumpTable;

  // The globalArea must be the last field.  Globals for the module start here
  // and are inline in this structure.  16-byte alignment is required for SIMD
  // data.
  MOZ_ALIGNED_DECL(16, char globalArea);
};

static const size_t TlsDataAlign = 16;  // = Simd128DataSize
static_assert(offsetof(TlsData, globalArea) % TlsDataAlign == 0, "aligned");

struct TlsDataDeleter {
  void operator()(TlsData* tlsData) { js_free(tlsData->allocatedBase); }
};

using UniqueTlsData = UniquePtr<TlsData, TlsDataDeleter>;

extern UniqueTlsData CreateTlsData(uint32_t globalDataLength);

// ExportArg holds the unboxed operands to the wasm entry trampoline which can
// be called through an ExportFuncPtr.

struct ExportArg {
  uint64_t lo;
  uint64_t hi;
};

using ExportFuncPtr = int32_t (*)(ExportArg*, TlsData*);

// FuncImportTls describes the region of wasm global memory allocated in the
// instance's thread-local storage for a function import. This is accessed
// directly from JIT code and mutated by Instance as exits become optimized and
// deoptimized.

struct FuncImportTls {
  // The code to call at an import site: a wasm callee, a thunk into C++, or a
  // thunk into JIT code.
  void* code;

  // The callee's TlsData pointer, which must be loaded to WasmTlsReg (along
  // with any pinned registers) before calling 'code'.
  TlsData* tls;

  // The callee function's realm.
  JS::Realm* realm;

  // A GC pointer which keeps the callee alive and is used to recover import
  // values for lazy table initialization.
  GCPtrFunction fun;
  static_assert(sizeof(GCPtrFunction) == sizeof(void*), "for JIT access");
};

// TableTls describes the region of wasm global memory allocated in the
// instance's thread-local storage which is accessed directly from JIT code
// to bounds-check and index the table.

struct TableTls {
  // Length of the table in number of elements (not bytes).
  uint32_t length;

  // Pointer to the array of elements (which can have various representations).
  // For tables of anyref this is null.
  void* functionBase;
};

// Table element for TableRepr::Func which carries both the code pointer and
// a tls pointer (and thus anything reachable through the tls, including the
// instance).

struct FunctionTableElem {
  // The code to call when calling this element. The table ABI is the system
  // ABI with the additional ABI requirements that:
  //  - WasmTlsReg and any pinned registers have been loaded appropriately
  //  - if this is a heterogeneous table that requires a signature check,
  //    WasmTableCallSigReg holds the signature id.
  void* code;

  // The pointer to the callee's instance's TlsData. This must be loaded into
  // WasmTlsReg before calling 'code'.
  TlsData* tls;
};

// CalleeDesc describes how to compile one of the variety of asm.js/wasm calls.
// This is hoisted into WasmTypes.h for sharing between Ion and Baseline.

class CalleeDesc {
 public:
  enum Which {
    // Calls a function defined in the same module by its index.
    Func,

    // Calls the import identified by the offset of its FuncImportTls in
    // thread-local data.
    Import,

    // Calls a WebAssembly table (heterogeneous, index must be bounds
    // checked, callee instance depends on TableDesc).
    WasmTable,

    // Calls an asm.js table (homogeneous, masked index, same-instance).
    AsmJSTable,

    // Call a C++ function identified by SymbolicAddress.
    Builtin,

    // Like Builtin, but automatically passes Instance* as first argument.
    BuiltinInstanceMethod
  };

 private:
  // which_ shall be initialized in the static constructors
  MOZ_INIT_OUTSIDE_CTOR Which which_;
  union U {
    U() : funcIndex_(0) {}
    uint32_t funcIndex_;
    struct {
      uint32_t globalDataOffset_;
    } import;
    struct {
      uint32_t globalDataOffset_;
      uint32_t minLength_;
      TypeIdDesc funcTypeId_;
    } table;
    SymbolicAddress builtin_;
  } u;

 public:
  CalleeDesc() = default;
  static CalleeDesc function(uint32_t funcIndex) {
    CalleeDesc c;
    c.which_ = Func;
    c.u.funcIndex_ = funcIndex;
    return c;
  }
  static CalleeDesc import(uint32_t globalDataOffset) {
    CalleeDesc c;
    c.which_ = Import;
    c.u.import.globalDataOffset_ = globalDataOffset;
    return c;
  }
  static CalleeDesc wasmTable(const TableDesc& desc, TypeIdDesc funcTypeId) {
    CalleeDesc c;
    c.which_ = WasmTable;
    c.u.table.globalDataOffset_ = desc.globalDataOffset;
    c.u.table.minLength_ = desc.initialLength;
    c.u.table.funcTypeId_ = funcTypeId;
    return c;
  }
  static CalleeDesc asmJSTable(const TableDesc& desc) {
    CalleeDesc c;
    c.which_ = AsmJSTable;
    c.u.table.globalDataOffset_ = desc.globalDataOffset;
    return c;
  }
  static CalleeDesc builtin(SymbolicAddress callee) {
    CalleeDesc c;
    c.which_ = Builtin;
    c.u.builtin_ = callee;
    return c;
  }
  static CalleeDesc builtinInstanceMethod(SymbolicAddress callee) {
    CalleeDesc c;
    c.which_ = BuiltinInstanceMethod;
    c.u.builtin_ = callee;
    return c;
  }
  Which which() const { return which_; }
  uint32_t funcIndex() const {
    MOZ_ASSERT(which_ == Func);
    return u.funcIndex_;
  }
  uint32_t importGlobalDataOffset() const {
    MOZ_ASSERT(which_ == Import);
    return u.import.globalDataOffset_;
  }
  bool isTable() const { return which_ == WasmTable || which_ == AsmJSTable; }
  uint32_t tableLengthGlobalDataOffset() const {
    MOZ_ASSERT(isTable());
    return u.table.globalDataOffset_ + offsetof(TableTls, length);
  }
  uint32_t tableFunctionBaseGlobalDataOffset() const {
    MOZ_ASSERT(isTable());
    return u.table.globalDataOffset_ + offsetof(TableTls, functionBase);
  }
  TypeIdDesc wasmTableSigId() const {
    MOZ_ASSERT(which_ == WasmTable);
    return u.table.funcTypeId_;
  }
  uint32_t wasmTableMinLength() const {
    MOZ_ASSERT(which_ == WasmTable);
    return u.table.minLength_;
  }
  SymbolicAddress builtin() const {
    MOZ_ASSERT(which_ == Builtin || which_ == BuiltinInstanceMethod);
    return u.builtin_;
  }
};

// Memories can be 32-bit (indices are 32 bits and the max is 4GB) or 64-bit
// (indices are 64 bits and the max is XXX).

enum class MemoryKind { Memory32, Memory64 };

// Because ARM has a fixed-width instruction encoding, ARM can only express a
// limited subset of immediates (in a single instruction).

static const uint64_t HighestValidARMImmediate = 0xff000000;

extern bool IsValidARMImmediate(uint32_t i);

extern uint64_t RoundUpToNextValidARMImmediate(uint64_t i);

// Bounds checks always compare the base of the memory access with the bounds
// check limit. If the memory access is unaligned, this means that, even if the
// bounds check succeeds, a few bytes of the access can extend past the end of
// memory. To guard against this, extra space is included in the guard region to
// catch the overflow. MaxMemoryAccessSize is a conservative approximation of
// the maximum guard space needed to catch all unaligned overflows.

static const unsigned MaxMemoryAccessSize = LitVal::sizeofLargestValue();

#ifdef WASM_SUPPORTS_HUGE_MEMORY

// On WASM_SUPPORTS_HUGE_MEMORY platforms, every asm.js or WebAssembly 32-bit
// memory unconditionally allocates a huge region of virtual memory of size
// wasm::HugeMappedSize. This allows all memory resizing to work without
// reallocation and provides enough guard space for all offsets to be folded
// into memory accesses.

static const uint64_t HugeIndexRange = uint64_t(UINT32_MAX) + 1;
static const uint64_t HugeOffsetGuardLimit = uint64_t(INT32_MAX) + 1;
static const uint64_t HugeUnalignedGuardPage = PageSize;
static const uint64_t HugeMappedSize =
    HugeIndexRange + HugeOffsetGuardLimit + HugeUnalignedGuardPage;

static_assert(MaxMemoryAccessSize <= HugeUnalignedGuardPage,
              "rounded up to static page size");
static_assert(HugeOffsetGuardLimit < UINT32_MAX,
              "checking for overflow against OffsetGuardLimit is enough.");

#endif

// On !WASM_SUPPORTS_HUGE_MEMORY platforms:
//  - To avoid OOM in ArrayBuffer::prepareForAsmJS, asm.js continues to use the
//    original ArrayBuffer allocation which has no guard region at all.
//  - For WebAssembly memories, an additional GuardSize is mapped after the
//    accessible region of the memory to catch folded (base+offset) accesses
//    where `offset < OffsetGuardLimit` as well as the overflow from unaligned
//    accesses, as described above for MaxMemoryAccessSize.

static const size_t OffsetGuardLimit = PageSize - MaxMemoryAccessSize;
static const size_t GuardSize = PageSize;

static_assert(MaxMemoryAccessSize < GuardSize,
              "Guard page handles partial out-of-bounds");
static_assert(OffsetGuardLimit < UINT32_MAX,
              "checking for overflow against OffsetGuardLimit is enough.");

static constexpr size_t GetMaxOffsetGuardLimit(bool hugeMemory) {
#ifdef WASM_SUPPORTS_HUGE_MEMORY
  return hugeMemory ? HugeOffsetGuardLimit : OffsetGuardLimit;
#else
  return OffsetGuardLimit;
#endif
}

static const size_t MinOffsetGuardLimit = OffsetGuardLimit;

// Return whether the given immediate satisfies the constraints of the platform
// (viz. that, on ARM, IsValidARMImmediate).

extern bool IsValidBoundsCheckImmediate(uint32_t i);

// For a given WebAssembly/asm.js max size, return the number of bytes to
// map which will necessarily be a multiple of the system page size and greater
// than maxSize. For a returned mappedSize:
//   boundsCheckLimit = mappedSize - GuardSize
//   IsValidBoundsCheckImmediate(boundsCheckLimit)

extern size_t ComputeMappedSize(uint64_t maxSize);

// The following thresholds were derived from a microbenchmark. If we begin to
// ship this optimization for more platforms, we will need to extend this list.

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM64)
static const uint32_t MaxInlineMemoryCopyLength = 64;
static const uint32_t MaxInlineMemoryFillLength = 64;
#elif defined(JS_CODEGEN_X86)
static const uint32_t MaxInlineMemoryCopyLength = 32;
static const uint32_t MaxInlineMemoryFillLength = 32;
#else
static const uint32_t MaxInlineMemoryCopyLength = 0;
static const uint32_t MaxInlineMemoryFillLength = 0;
#endif

static_assert(MaxInlineMemoryCopyLength < MinOffsetGuardLimit, "precondition");
static_assert(MaxInlineMemoryFillLength < MinOffsetGuardLimit, "precondition");

// wasm::Frame represents the bytes pushed by the call instruction and the
// fixed prologue generated by wasm::GenerateCallablePrologue.
//
// Across all architectures it is assumed that, before the call instruction, the
// stack pointer is WasmStackAlignment-aligned. Thus after the prologue, and
// before the function has made its stack reservation, the stack alignment is
// sizeof(Frame) % WasmStackAlignment.
//
// During MacroAssembler code generation, the bytes pushed after the wasm::Frame
// are counted by masm.framePushed. Thus, the stack alignment at any point in
// time is (sizeof(wasm::Frame) + masm.framePushed) % WasmStackAlignment.

class Frame {
  // See GenerateCallableEpilogue for why this must be
  // the first field of wasm::Frame (in a downward-growing stack).
  // It's either the caller's Frame*, for wasm callers, or the JIT caller frame
  // plus a tag otherwise.
  uint8_t* callerFP_;

  // The return address pushed by the call (in the case of ARM/MIPS the return
  // address is pushed by the first instruction of the prologue).
  void* returnAddress_;

 public:
  static constexpr uint32_t callerFPOffset() {
    return offsetof(Frame, callerFP_);
  }
  static constexpr uint32_t returnAddressOffset() {
    return offsetof(Frame, returnAddress_);
  }

  uint8_t* returnAddress() const {
    return reinterpret_cast<uint8_t*>(returnAddress_);
  }

  void** addressOfReturnAddress() {
    return reinterpret_cast<void**>(&returnAddress_);
  }

  uint8_t* rawCaller() const { return callerFP_; }

  Frame* wasmCaller() const {
    MOZ_ASSERT(!callerIsExitOrJitEntryFP());
    return reinterpret_cast<Frame*>(callerFP_);
  }

  bool callerIsExitOrJitEntryFP() const {
    return isExitOrJitEntryFP(callerFP_);
  }

  uint8_t* jitEntryCaller() const { return toJitEntryCaller(callerFP_); }

  static const Frame* fromUntaggedWasmExitFP(const void* savedFP) {
    MOZ_ASSERT(!isExitOrJitEntryFP(savedFP));
    return reinterpret_cast<const Frame*>(savedFP);
  }

  static bool isExitOrJitEntryFP(const void* fp) {
    return reinterpret_cast<uintptr_t>(fp) & ExitOrJitEntryFPTag;
  }

  static uint8_t* toJitEntryCaller(const void* fp) {
    MOZ_ASSERT(isExitOrJitEntryFP(fp));
    return reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(fp) &
                                      ~ExitOrJitEntryFPTag);
  }

  static uint8_t* addExitOrJitEntryFPTag(const Frame* fp) {
    MOZ_ASSERT(!isExitOrJitEntryFP(fp));
    return reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(fp) |
                                      ExitOrJitEntryFPTag);
  }
};

static_assert(!std::is_polymorphic_v<Frame>, "Frame doesn't need a vtable.");
static_assert(sizeof(Frame) == 2 * sizeof(void*),
              "Frame is a two pointer structure");

class FrameWithTls : public Frame {
  TlsData* calleeTls_;
  TlsData* callerTls_;

 public:
  TlsData* calleeTls() { return calleeTls_; }
  TlsData* callerTls() { return callerTls_; }

  constexpr static uint32_t sizeWithoutFrame() {
    return sizeof(wasm::FrameWithTls) - sizeof(wasm::Frame);
  }

  constexpr static uint32_t calleeTLSOffset() {
    return offsetof(FrameWithTls, calleeTls_) - sizeof(wasm::Frame);
  }

  constexpr static uint32_t callerTLSOffset() {
    return offsetof(FrameWithTls, callerTls_) - sizeof(wasm::Frame);
  }
};

static_assert(FrameWithTls::calleeTLSOffset() == 0u,
              "Callee tls stored right above the return address.");
static_assert(FrameWithTls::callerTLSOffset() == sizeof(void*),
              "Caller tls stored right above the callee tls.");

static_assert(FrameWithTls::sizeWithoutFrame() == 2 * sizeof(void*),
              "There are only two additional slots");

#if defined(JS_CODEGEN_ARM64)
static_assert(sizeof(Frame) % 16 == 0, "frame is aligned");
#endif

// A DebugFrame is a Frame with additional fields that are added after the
// normal function prologue by the baseline compiler. If a Module is compiled
// with debugging enabled, then all its code creates DebugFrames on the stack
// instead of just Frames. These extra fields are used by the Debugger API.

class DebugFrame {
  // The register results field.  Initialized only during the baseline
  // compiler's return sequence to allow the debugger to inspect and
  // modify the return values of a frame being debugged.
  union SpilledRegisterResult {
   private:
    int32_t i32_;
    int64_t i64_;
    intptr_t ref_;
    AnyRef anyref_;
    float f32_;
    double f64_;
#ifdef ENABLE_WASM_SIMD
    V128 v128_;
#endif
#ifdef DEBUG
    // Should we add a new value representation, this will remind us to update
    // SpilledRegisterResult.
    static inline void assertAllValueTypesHandled(ValType type) {
      switch (type.kind()) {
        case ValType::I32:
        case ValType::I64:
        case ValType::F32:
        case ValType::F64:
        case ValType::V128:
        case ValType::Rtt:
          return;
        case ValType::Ref:
          switch (type.refTypeKind()) {
            case RefType::Func:
            case RefType::Extern:
            case RefType::Eq:
            case RefType::TypeIndex:
              return;
          }
      }
    }
#endif
  };
  SpilledRegisterResult registerResults_[MaxRegisterResults];

  // The returnValue() method returns a HandleValue pointing to this field.
  js::Value cachedReturnJSValue_;

  // If the function returns multiple results, this field is initialized
  // to a pointer to the stack results.
  void* stackResultsPointer_;

  // The function index of this frame. Technically, this could be derived
  // given a PC into this frame (which could lookup the CodeRange which has
  // the function index), but this isn't always readily available.
  uint32_t funcIndex_;

  // Flags whose meaning are described below.
  union Flags {
    struct {
      uint32_t observing : 1;
      uint32_t isDebuggee : 1;
      uint32_t prevUpToDate : 1;
      uint32_t hasCachedSavedFrame : 1;
      uint32_t hasCachedReturnJSValue : 1;
      uint32_t hasSpilledRefRegisterResult : MaxRegisterResults;
    };
    uint32_t allFlags;
  } flags_;

  // Avoid -Wunused-private-field warnings.
 protected:
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_ARM) || \
    defined(JS_CODEGEN_X86) || defined(__wasi__)
  // See alignmentStaticAsserts().  For MIPS32, ARM32 and X86 DebugFrame is only
  // 4-byte aligned, so we add another word to get up to 8-byte
  // alignment.
  uint32_t padding_;
#endif
#if defined(ENABLE_WASM_SIMD) && defined(JS_CODEGEN_ARM64)
  uint64_t padding_;
#endif

 private:
  // The Frame goes at the end since the stack grows down.
  Frame frame_;

 public:
  static DebugFrame* from(Frame* fp);
  Frame& frame() { return frame_; }
  uint32_t funcIndex() const { return funcIndex_; }
  Instance* instance() const;
  GlobalObject* global() const;
  bool hasGlobal(const GlobalObject* global) const;
  JSObject* environmentChain() const;
  bool getLocal(uint32_t localIndex, MutableHandleValue vp);

  // The return value must be written from the unboxed representation in the
  // results union into cachedReturnJSValue_ by updateReturnJSValue() before
  // returnValue() can return a Handle to it.

  bool hasCachedReturnJSValue() const { return flags_.hasCachedReturnJSValue; }
  [[nodiscard]] bool updateReturnJSValue(JSContext* cx);
  HandleValue returnValue() const;
  void clearReturnJSValue();

  // Once the debugger observes a frame, it must be notified via
  // onLeaveFrame() before the frame is popped. Calling observe() ensures the
  // leave frame traps are enabled. Both methods are idempotent so the caller
  // doesn't have to worry about calling them more than once.

  void observe(JSContext* cx);
  void leave(JSContext* cx);

  // The 'isDebugge' bit is initialized to false and set by the WebAssembly
  // runtime right before a frame is exposed to the debugger, as required by
  // the Debugger API. The bit is then used for Debugger-internal purposes
  // afterwards.

  bool isDebuggee() const { return flags_.isDebuggee; }
  void setIsDebuggee() { flags_.isDebuggee = true; }
  void unsetIsDebuggee() { flags_.isDebuggee = false; }

  // These are opaque boolean flags used by the debugger to implement
  // AbstractFramePtr. They are initialized to false and not otherwise read or
  // written by wasm code or runtime.

  bool prevUpToDate() const { return flags_.prevUpToDate; }
  void setPrevUpToDate() { flags_.prevUpToDate = true; }
  void unsetPrevUpToDate() { flags_.prevUpToDate = false; }

  bool hasCachedSavedFrame() const { return flags_.hasCachedSavedFrame; }
  void setHasCachedSavedFrame() { flags_.hasCachedSavedFrame = true; }
  void clearHasCachedSavedFrame() { flags_.hasCachedSavedFrame = false; }

  bool hasSpilledRegisterRefResult(size_t n) const {
    uint32_t mask = hasSpilledRegisterRefResultBitMask(n);
    return (flags_.allFlags & mask) != 0;
  }

  // DebugFrame is accessed directly by JIT code.

  static constexpr size_t offsetOfRegisterResults() {
    return offsetof(DebugFrame, registerResults_);
  }
  static constexpr size_t offsetOfRegisterResult(size_t n) {
    MOZ_ASSERT(n < MaxRegisterResults);
    return offsetOfRegisterResults() + n * sizeof(SpilledRegisterResult);
  }
  static constexpr size_t offsetOfCachedReturnJSValue() {
    return offsetof(DebugFrame, cachedReturnJSValue_);
  }
  static constexpr size_t offsetOfStackResultsPointer() {
    return offsetof(DebugFrame, stackResultsPointer_);
  }
  static constexpr size_t offsetOfFlags() {
    return offsetof(DebugFrame, flags_);
  }
  static constexpr uint32_t hasSpilledRegisterRefResultBitMask(size_t n) {
    MOZ_ASSERT(n < MaxRegisterResults);
    union Flags flags = {.allFlags = 0};
    flags.hasSpilledRefRegisterResult = 1 << n;
    MOZ_ASSERT(flags.allFlags != 0);
    return flags.allFlags;
  }
  static constexpr size_t offsetOfFuncIndex() {
    return offsetof(DebugFrame, funcIndex_);
  }
  static constexpr size_t offsetOfFrame() {
    return offsetof(DebugFrame, frame_);
  }

  // DebugFrames are aligned to 8-byte aligned, allowing them to be placed in
  // an AbstractFramePtr.

  static const unsigned Alignment = 8;
  static void alignmentStaticAsserts();
};

// Verbose logging support.

extern void Log(JSContext* cx, const char* fmt, ...) MOZ_FORMAT_PRINTF(2, 3);

// Codegen debug support.

enum class DebugChannel {
  Function,
  Import,
};

#ifdef WASM_CODEGEN_DEBUG
bool IsCodegenDebugEnabled(DebugChannel channel);
#endif

void DebugCodegen(DebugChannel channel, const char* fmt, ...)
    MOZ_FORMAT_PRINTF(2, 3);

using PrintCallback = void (*)(const char*);

#ifdef ENABLE_WASM_SIMD_WORMHOLE
bool IsWormholeTrigger(const V128& shuffleMask);
jit::SimdConstant WormholeSignature();
#endif

}  // namespace wasm
}  // namespace js

#endif  // wasm_types_h
