/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineIC_h
#define jit_BaselineIC_h

#include "mozilla/Assertions.h"

#include "builtin/TypedObject.h"
#include "gc/Barrier.h"
#include "gc/GC.h"
#include "jit/BaselineICList.h"
#include "jit/BaselineJIT.h"
#include "jit/ICState.h"
#include "jit/SharedICRegisters.h"
#include "js/GCVector.h"
#include "vm/ArrayObject.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"

namespace js {
namespace jit {

// [SMDOC] JIT Inline Caches (ICs)
//
// Baseline Inline Caches are polymorphic caches that aggressively
// share their stub code.
//
// Every polymorphic site contains a linked list of stubs which are
// specific to that site.  These stubs are composed of a |StubData|
// structure that stores parametrization information (e.g.
// the shape pointer for a shape-check-and-property-get stub), any
// dynamic information (e.g. warm-up counters), a pointer to the stub code,
// and a pointer to the next stub state in the linked list.
//
// Every BaselineScript keeps an table of |CacheDescriptor| data
// structures, which store the following:
//      A pointer to the first StubData in the cache.
//      The bytecode PC of the relevant IC.
//      The machine-code PC where the call to the stubcode returns.
//
// A diagram:
//
//        Control flow                  Pointers
//      =======#                     ----.     .---->
//             #                         |     |
//             #======>                  \-----/
//
//
//                                   .---------------------------------------.
//                                   |         .-------------------------.   |
//                                   |         |         .----.          |   |
//         Baseline                  |         |         |    |          |   |
//         JIT Code              0   ^     1   ^     2   ^    |          |   |
//     +--------------+    .-->+-----+   +-----+   +-----+    |          |   |
//     |              |  #=|==>|     |==>|     |==>| FB  |    |          |   |
//     |              |  # |   +-----+   +-----+   +-----+    |          |   |
//     |              |  # |      #         #         #       |          |   |
//     |==============|==# |      #         #         #       |          |   |
//     |=== IC =======|    |      #         #         #       |          |   |
//  .->|==============|<===|======#=========#=========#       |          |   |
//  |  |              |    |                                  |          |   |
//  |  |              |    |                                  |          |   |
//  |  |              |    |                                  |          |   |
//  |  |              |    |                                  v          |   |
//  |  |              |    |                              +---------+    |   |
//  |  |              |    |                              | Fallback|    |   |
//  |  |              |    |                              | Stub    |    |   |
//  |  |              |    |                              | Code    |    |   |
//  |  |              |    |                              +---------+    |   |
//  |  +--------------+    |                                             |   |
//  |         |_______     |                              +---------+    |   |
//  |                |     |                              | Stub    |<---/   |
//  |        IC      |     \--.                           | Code    |        |
//  |    Descriptor  |        |                           +---------+        |
//  |      Table     v        |                                              |
//  |  +-----------------+    |                           +---------+        |
//  \--| Ins | PC | Stub |----/                           | Stub    |<-------/
//     +-----------------+                                | Code    |
//     |       ...       |                                +---------+
//     +-----------------+
//                                                          Shared
//                                                          Stub Code
//
//
// Type ICs
// ========
//
// Type ICs are otherwise regular ICs that are actually nested within
// other IC chains.  They serve to optimize locations in the code where the
// baseline compiler would have otherwise had to perform a type Monitor
// operation (e.g. the result of GetProp, GetElem, etc.), or locations where the
// baseline compiler would have had to modify a heap typeset using the type of
// an input value (e.g. SetProp, SetElem, etc.)
//
// There are two kinds of Type ICs: Monitor and Update.
//
// Note that type stub bodies are no-ops.  The stubs only exist for their
// guards, and their existence simply signifies that the typeset (implicit)
// that is being checked already contains that type.
//
// TypeMonitor ICs
// ---------------
// Monitor ICs are shared between stubs in the general IC, and monitor the
// resulting types of getter operations (call returns, getprop outputs, etc.)
//
//        +-----------+     +-----------+     +-----------+     +-----------+
//   ---->| Stub 1    |---->| Stub 2    |---->| Stub 3    |---->| FB Stub   |
//        +-----------+     +-----------+     +-----------+     +-----------+
//             |                  |                 |                  |
//             |------------------/-----------------/                  |
//             v                                                       |
//        +-----------+     +-----------+     +-----------+            |
//        | Type 1    |---->| Type 2    |---->| Type FB   |            |
//        +-----------+     +-----------+     +-----------+            |
//             |                 |                  |                  |
//  <----------/-----------------/------------------/------------------/
//                r e t u r n    p a t h
//
// After an optimized IC stub successfully executes, it passes control to the
// type stub chain to check the resulting type.  If no type stub succeeds, and
// the monitor fallback stub is reached, the monitor fallback stub performs a
// manual monitor, and also adds the appropriate type stub to the chain.
//
// The IC's main fallback, in addition to generating new mainline stubs, also
// generates type stubs as reflected by its returned value.
//
// NOTE: The type IC chain returns directly to the mainline code, not back to
// the stub it was entered from.  Thus, entering a type IC is a matter of a
// |jump|, not a |call|.  This allows us to safely call a VM Monitor function
// from within the monitor IC's fallback chain, since the return address (needed
// for stack inspection) is preserved.
//
//
// TypeUpdate ICs
// --------------
// Update ICs update heap typesets and monitor the input types of setter
// operations (setelem, setprop inputs, etc.).  Unlike monitor ICs, they are not
// shared between stubs on an IC, but instead are kept track of on a per-stub
// basis.
//
// This is because the main stubs for the operation will each identify a
// potentially different ObjectGroup to update.  New input types must be tracked
// on a group-to- group basis.
//
// Type-update ICs cannot be called in tail position (they must return to the
// the stub that called them so that the stub may continue to perform its
// original purpose).  This means that any VMCall to perform a manual type
// update from C++ must be done from within the main IC stub.  This necessitates
// that the stub enter a "BaselineStub" frame before making the call.
//
// If the type-update IC chain could itself make the VMCall, then the
// BaselineStub frame must be entered before calling the type-update chain, and
// exited afterward.  This is very expensive for a common case where we expect
// the type-update fallback to not be called.  To avoid the cost of entering and
// exiting a BaselineStub frame when using the type-update IC chain, we design
// the chain to not perform any VM-calls in its fallback.
//
// Instead, the type-update IC chain is responsible for returning 1 or 0,
// depending on if a type is represented in the chain or not.  The fallback stub
// simply returns 0, and all other optimized stubs return 1. If the chain
// returns 1, then the IC stub goes ahead and performs its operation. If the
// chain returns 0, then the IC stub performs a call to the fallback function
// inline (doing the requisite BaselineStub frame enter/exit).
// This allows us to avoid the expensive subfram enter/exit in the common case.
//
//                                 r e t u r n    p a t h
//   <--------------.-----------------.-----------------.-----------------.
//                  |                 |                 |                 |
//        +-----------+     +-----------+     +-----------+     +-----------+
//   ---->| Stub 1    |---->| Stub 2    |---->| Stub 3    |---->| FB Stub   |
//        +-----------+     +-----------+     +-----------+     +-----------+
//          |   ^             |   ^             |   ^
//          |   |             |   |             |   |
//          |   |             |   |             |   |----------------.
//          |   |             |   |             v   |1               |0
//          |   |             |   |         +-----------+    +-----------+
//          |   |             |   |         | Type 3.1  |--->|    FB 3   |
//          |   |             |   |         +-----------+    +-----------+
//          |   |             |   |
//          |   |             |   \-------------.-----------------.
//          |   |             |   |             |                 |
//          |   |             v   |1            |1                |0
//          |   |         +-----------+     +-----------+     +-----------+
//          |   |         | Type 2.1  |---->| Type 2.2  |---->|    FB 2   |
//          |   |         +-----------+     +-----------+     +-----------+
//          |   |
//          |   \-------------.-----------------.
//          |   |             |                 |
//          v   |1            |1                |0
//     +-----------+     +-----------+     +-----------+
//     | Type 1.1  |---->| Type 1.2  |---->|   FB 1    |
//     +-----------+     +-----------+     +-----------+
//

class ICStub;
class ICFallbackStub;

#define FORWARD_DECLARE_STUBS(kindName) class IC##kindName;
IC_BASELINE_STUB_KIND_LIST(FORWARD_DECLARE_STUBS)
#undef FORWARD_DECLARE_STUBS

#ifdef JS_JITSPEW
void FallbackICSpew(JSContext* cx, ICFallbackStub* stub, const char* fmt, ...)
    MOZ_FORMAT_PRINTF(3, 4);
void TypeFallbackICSpew(JSContext* cx, ICTypeMonitor_Fallback* stub,
                        const char* fmt, ...) MOZ_FORMAT_PRINTF(3, 4);
#else
#  define FallbackICSpew(...)
#  define TypeFallbackICSpew(...)
#endif

// An entry in the BaselineScript IC descriptor table. There's one ICEntry per
// IC.
class ICEntry {
  // A pointer to the first IC stub for this instruction.
  ICStub* firstStub_;

  // The PC offset of this IC's bytecode op within the JSScript or
  // ProloguePCOffset if this is a prologue IC.
  uint32_t pcOffset_;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
#  ifdef JS_64BIT
  // On 64-bit architectures, we have 32 bits of alignment padding.
  // We fill it with a magic value, and check that value when tracing.
  static const uint32_t EXPECTED_TRACE_MAGIC = 0xdeaddead;
  uint32_t traceMagic_ = EXPECTED_TRACE_MAGIC;
#  endif
#endif

 public:
  // Prologue ICs are Baseline ICs used for function argument/this type
  // monitoring in the script's prologue. Note: the last bytecode op in a script
  // is always a return so UINT32_MAX is never a valid bytecode offset.
  static constexpr uint32_t ProloguePCOffset = UINT32_MAX;

  ICEntry(ICStub* firstStub, uint32_t pcOffset)
      : firstStub_(firstStub), pcOffset_(pcOffset) {}

  ICStub* firstStub() const {
    MOZ_ASSERT(firstStub_);
    return firstStub_;
  }

  ICFallbackStub* fallbackStub() const;

  void setFirstStub(ICStub* stub) { firstStub_ = stub; }

  uint32_t pcOffset() const {
    return pcOffset_ == ProloguePCOffset ? 0 : pcOffset_;
  }
  jsbytecode* pc(JSScript* script) const {
    return script->offsetToPC(pcOffset());
  }

  static inline size_t offsetOfFirstStub() {
    return offsetof(ICEntry, firstStub_);
  }

  inline ICStub** addressOfFirstStub() { return &firstStub_; }

  bool isForPrologue() const { return pcOffset_ == ProloguePCOffset; }

  void trace(JSTracer* trc);
};

class ICMonitoredStub;
class ICMonitoredFallbackStub;

// Constant iterator that traverses arbitrary chains of ICStubs.
// No requirements are made of the ICStub used to construct this
// iterator, aside from that the stub be part of a nullptr-terminated
// chain.
// The iterator is considered to be at its end once it has been
// incremented _past_ the last stub.  Thus, if 'atEnd()' returns
// true, the '*' and '->' operations are not valid.
class ICStubConstIterator {
  friend class ICStub;
  friend class ICFallbackStub;

 private:
  ICStub* currentStub_;

 public:
  explicit ICStubConstIterator(ICStub* currentStub)
      : currentStub_(currentStub) {}

  static ICStubConstIterator StartingAt(ICStub* stub) {
    return ICStubConstIterator(stub);
  }
  static ICStubConstIterator End(ICStub* stub) {
    return ICStubConstIterator(nullptr);
  }

  bool operator==(const ICStubConstIterator& other) const {
    return currentStub_ == other.currentStub_;
  }
  bool operator!=(const ICStubConstIterator& other) const {
    return !(*this == other);
  }

  ICStubConstIterator& operator++();

  ICStubConstIterator operator++(int) {
    ICStubConstIterator oldThis(*this);
    ++(*this);
    return oldThis;
  }

  ICStub* operator*() const {
    MOZ_ASSERT(currentStub_);
    return currentStub_;
  }

  ICStub* operator->() const {
    MOZ_ASSERT(currentStub_);
    return currentStub_;
  }

  bool atEnd() const { return currentStub_ == nullptr; }
};

// Iterator that traverses "regular" IC chains that start at an ICEntry
// and are terminated with an ICFallbackStub.
//
// The iterator is considered to be at its end once it is _at_ the
// fallback stub.  Thus, unlike the ICStubConstIterator, operators
// '*' and '->' are valid even if 'atEnd()' returns true - they
// will act on the fallback stub.
//
// This iterator also allows unlinking of stubs being traversed.
// Note that 'unlink' does not implicitly advance the iterator -
// it must be advanced explicitly using '++'.
class ICStubIterator {
  friend class ICFallbackStub;

 private:
  ICEntry* icEntry_;
  ICFallbackStub* fallbackStub_;
  ICStub* previousStub_;
  ICStub* currentStub_;
  bool unlinked_;

  explicit ICStubIterator(ICFallbackStub* fallbackStub, bool end = false);

 public:
  bool operator==(const ICStubIterator& other) const {
    // == should only ever be called on stubs from the same chain.
    MOZ_ASSERT(icEntry_ == other.icEntry_);
    MOZ_ASSERT(fallbackStub_ == other.fallbackStub_);
    return currentStub_ == other.currentStub_;
  }
  bool operator!=(const ICStubIterator& other) const {
    return !(*this == other);
  }

  ICStubIterator& operator++();

  ICStubIterator operator++(int) {
    ICStubIterator oldThis(*this);
    ++(*this);
    return oldThis;
  }

  ICStub* operator*() const { return currentStub_; }

  ICStub* operator->() const { return currentStub_; }

  bool atEnd() const { return currentStub_ == (ICStub*)fallbackStub_; }

  void unlink(JSContext* cx);
};

//
// Base class for all IC stubs.
//
class ICStub {
  friend class ICFallbackStub;

 public:
  enum Kind : uint16_t {
    INVALID = 0,
#define DEF_ENUM_KIND(kindName) kindName,
    IC_BASELINE_STUB_KIND_LIST(DEF_ENUM_KIND)
#undef DEF_ENUM_KIND
        LIMIT
  };

  static bool IsValidKind(Kind k) { return (k > INVALID) && (k < LIMIT); }
  static bool IsCacheIRKind(Kind k) {
    return k == CacheIR_Regular || k == CacheIR_Monitored ||
           k == CacheIR_Updated;
  }

  static const char* KindString(Kind k) {
    switch (k) {
#define DEF_KIND_STR(kindName) \
  case kindName:               \
    return #kindName;
      IC_BASELINE_STUB_KIND_LIST(DEF_KIND_STR)
#undef DEF_KIND_STR
      default:
        MOZ_CRASH("Invalid kind.");
    }
  }

  enum Trait : uint16_t {
    Regular = 0x0,
    Fallback = 0x1,
    Monitored = 0x2,
    MonitoredFallback = 0x3,
    Updated = 0x4
  };

  void updateCode(JitCode* stubCode);
  void trace(JSTracer* trc);

  static const uint16_t EXPECTED_TRACE_MAGIC = 0b1100011;

  template <typename T, typename... Args>
  static T* New(JSContext* cx, ICStubSpace* space, JitCode* code,
                Args&&... args) {
    if (!code) {
      return nullptr;
    }
    T* result = space->allocate<T>(code, std::forward<Args>(args)...);
    if (!result) {
      ReportOutOfMemory(cx);
    }
    return result;
  }

  template <typename T, typename... Args>
  static T* NewFallback(JSContext* cx, ICStubSpace* space, TrampolinePtr code,
                        Args&&... args) {
    T* result = space->allocate<T>(code, std::forward<Args>(args)...);
    if (MOZ_UNLIKELY(!result)) {
      ReportOutOfMemory(cx);
    }
    return result;
  }

 protected:
  // The raw jitcode to call for this stub.
  uint8_t* stubCode_;

  // Pointer to next IC stub.  This is null for the last IC stub, which should
  // either be a fallback or inert IC stub.
  ICStub* next_ = nullptr;

  // A 16-bit field usable by subtypes of ICStub for subtype-specific small-info
  uint16_t extra_ = 0;

  // A 16-bit field storing the trait and kind.
  // Unused bits are filled with a magic value and verified when tracing.
  uint16_t traitKindBits_;

  static const uint16_t TRAIT_OFFSET = 0;
  static const uint16_t TRAIT_BITS = 3;
  static const uint16_t TRAIT_MASK = (1 << TRAIT_BITS) - 1;
  static const uint16_t KIND_OFFSET = TRAIT_OFFSET + TRAIT_BITS;
  static const uint16_t KIND_BITS = 6;
  static const uint16_t KIND_MASK = (1 << KIND_BITS) - 1;
  static const uint16_t MAGIC_OFFSET = KIND_OFFSET + KIND_BITS;
  static const uint16_t MAGIC_BITS = 7;
  static const uint16_t MAGIC_MASK = (1 << MAGIC_BITS) - 1;
  static const uint16_t EXPECTED_MAGIC = 0b1100011;

  static_assert(LIMIT <= (1 << KIND_BITS), "Not enough kind bits");
  static_assert(LIMIT > (1 << (KIND_BITS - 1)), "Too many kind bits");
  static_assert(TRAIT_BITS + KIND_BITS + MAGIC_BITS == 16, "Unused bits");

  inline ICStub(Kind kind, uint8_t* stubCode) : stubCode_(stubCode) {
    setTraitKind(Regular, kind);
    MOZ_ASSERT(stubCode != nullptr);
  }

  inline ICStub(Kind kind, JitCode* stubCode) : ICStub(kind, stubCode->raw()) {
    MOZ_ASSERT(stubCode != nullptr);
  }

  inline ICStub(Kind kind, Trait trait, uint8_t* stubCode)
      : stubCode_(stubCode) {
    setTraitKind(trait, kind);
    MOZ_ASSERT(stubCode != nullptr);
  }

  inline ICStub(Kind kind, Trait trait, JitCode* stubCode)
      : ICStub(kind, trait, stubCode->raw()) {
    MOZ_ASSERT(stubCode != nullptr);
  }

  inline Trait trait() const {
    return (Trait)((traitKindBits_ >> TRAIT_OFFSET) & TRAIT_MASK);
  }

  inline void setTraitKind(Trait trait, Kind kind) {
    traitKindBits_ = (trait << TRAIT_OFFSET) | (kind << KIND_OFFSET) |
                     (EXPECTED_MAGIC << MAGIC_OFFSET);
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  inline void checkTraceMagic() {
    uint16_t magic = (traitKindBits_ >> MAGIC_OFFSET) & MAGIC_MASK;
    MOZ_DIAGNOSTIC_ASSERT(magic == EXPECTED_MAGIC);
  }
#endif

 public:
  inline Kind kind() const {
    return (Kind)((traitKindBits_ >> KIND_OFFSET) & KIND_MASK);
  }

  inline bool isFallback() const {
    return trait() == Fallback || trait() == MonitoredFallback;
  }

  inline bool isMonitored() const { return trait() == Monitored; }

  inline bool isUpdated() const { return trait() == Updated; }

  inline bool isMonitoredFallback() const {
    return trait() == MonitoredFallback;
  }

  inline const ICFallbackStub* toFallbackStub() const {
    MOZ_ASSERT(isFallback());
    return reinterpret_cast<const ICFallbackStub*>(this);
  }

  inline ICFallbackStub* toFallbackStub() {
    MOZ_ASSERT(isFallback());
    return reinterpret_cast<ICFallbackStub*>(this);
  }

  inline const ICMonitoredStub* toMonitoredStub() const {
    MOZ_ASSERT(isMonitored());
    return reinterpret_cast<const ICMonitoredStub*>(this);
  }

  inline ICMonitoredStub* toMonitoredStub() {
    MOZ_ASSERT(isMonitored());
    return reinterpret_cast<ICMonitoredStub*>(this);
  }

  inline const ICMonitoredFallbackStub* toMonitoredFallbackStub() const {
    MOZ_ASSERT(isMonitoredFallback());
    return reinterpret_cast<const ICMonitoredFallbackStub*>(this);
  }

  inline ICMonitoredFallbackStub* toMonitoredFallbackStub() {
    MOZ_ASSERT(isMonitoredFallback());
    return reinterpret_cast<ICMonitoredFallbackStub*>(this);
  }

  inline const ICCacheIR_Updated* toUpdatedStub() const {
    MOZ_ASSERT(isUpdated());
    return reinterpret_cast<const ICCacheIR_Updated*>(this);
  }

  inline ICCacheIR_Updated* toUpdatedStub() {
    MOZ_ASSERT(isUpdated());
    return reinterpret_cast<ICCacheIR_Updated*>(this);
  }

#define KIND_METHODS(kindName)                                    \
  inline bool is##kindName() const { return kind() == kindName; } \
  inline const IC##kindName* to##kindName() const {               \
    MOZ_ASSERT(is##kindName());                                   \
    return reinterpret_cast<const IC##kindName*>(this);           \
  }                                                               \
  inline IC##kindName* to##kindName() {                           \
    MOZ_ASSERT(is##kindName());                                   \
    return reinterpret_cast<IC##kindName*>(this);                 \
  }
  IC_BASELINE_STUB_KIND_LIST(KIND_METHODS)
#undef KIND_METHODS

  inline ICStub* next() const { return next_; }

  inline bool hasNext() const { return next_ != nullptr; }

  inline void setNext(ICStub* stub) {
    // Note: next_ only needs to be changed under the compilation lock for
    // non-type-monitor/update ICs.
    next_ = stub;
  }

  inline ICStub** addressOfNext() { return &next_; }

  bool usesTrampolineCode() const {
    // All fallback code is stored in a single JitCode instance, so we can't
    // call JitCode::FromExecutable on the raw pointer.
    return isFallback() || isTypeMonitor_Fallback() || isTypeUpdate_Fallback();
  }
  JitCode* jitCode() {
    MOZ_ASSERT(!usesTrampolineCode());
    return JitCode::FromExecutable(stubCode_);
  }

  inline uint8_t* rawStubCode() const { return stubCode_; }

  // This method is not valid on TypeUpdate stub chains!
  inline ICFallbackStub* getChainFallback() {
    ICStub* lastStub = this;
    while (lastStub->next_) {
      lastStub = lastStub->next_;
    }
    MOZ_ASSERT(lastStub->isFallback());
    return lastStub->toFallbackStub();
  }

  inline ICStubConstIterator beginHere() {
    return ICStubConstIterator::StartingAt(this);
  }

  static inline size_t offsetOfNext() { return offsetof(ICStub, next_); }

  static inline size_t offsetOfStubCode() {
    return offsetof(ICStub, stubCode_);
  }

  static inline size_t offsetOfExtra() { return offsetof(ICStub, extra_); }

  static bool NonCacheIRStubMakesGCCalls(Kind kind);
  bool makesGCCalls() const;

  // Optimized stubs get purged on GC.  But some stubs can be active on the
  // stack during GC - specifically the ones that can make calls.  To ensure
  // that these do not get purged, all stubs that can make calls are allocated
  // in the fallback stub space.
  bool allocatedInFallbackSpace() const {
    MOZ_ASSERT(next());
    return makesGCCalls();
  }
};

class ICFallbackStub : public ICStub {
  friend class ICStubConstIterator;

 protected:
  // Fallback stubs need these fields to easily add new stubs to
  // the linked list of stubs for an IC.

  // The IC entry for this linked list of stubs.
  ICEntry* icEntry_;

  // The state of this IC
  ICState state_;

  // Counts the number of times the stub was entered
  //
  // See Bug 1494473 comment 6 for a mechanism to handle overflow if overflow
  // becomes a concern.
  uint32_t enteredCount_;

  // A pointer to the location stub pointer that needs to be
  // changed to add a new "last" stub immediately before the fallback
  // stub.  This'll start out pointing to the icEntry's "firstStub_"
  // field, and as new stubs are added, it'll point to the current
  // last stub's "next_" field.
  ICStub** lastStubPtrAddr_;

  ICFallbackStub(Kind kind, TrampolinePtr stubCode)
      : ICStub(kind, ICStub::Fallback, stubCode.value),
        icEntry_(nullptr),
        state_(),
        enteredCount_(0),
        lastStubPtrAddr_(nullptr) {}

  ICFallbackStub(Kind kind, Trait trait, TrampolinePtr stubCode)
      : ICStub(kind, trait, stubCode.value),
        icEntry_(nullptr),
        state_(),
        enteredCount_(0),
        lastStubPtrAddr_(nullptr) {
    MOZ_ASSERT(trait == ICStub::Fallback || trait == ICStub::MonitoredFallback);
  }

 public:
  inline ICEntry* icEntry() const { return icEntry_; }

  inline size_t numOptimizedStubs() const { return state_.numOptimizedStubs(); }

  ICState& state() { return state_; }

  // The icEntry and lastStubPtrAddr_ fields can't be initialized when the stub
  // is created since the stub is created at compile time, and we won't know the
  // IC entry address until after compile when the JitScript is created.  This
  // method allows these fields to be fixed up at that point.
  void fixupICEntry(ICEntry* icEntry) {
    MOZ_ASSERT(icEntry_ == nullptr);
    MOZ_ASSERT(lastStubPtrAddr_ == nullptr);
    icEntry_ = icEntry;
    lastStubPtrAddr_ = icEntry_->addressOfFirstStub();
  }

  // Add a new stub to the IC chain terminated by this fallback stub.
  void addNewStub(ICStub* stub) {
    MOZ_ASSERT(*lastStubPtrAddr_ == this);
    MOZ_ASSERT(stub->next() == nullptr);
    stub->setNext(this);
    *lastStubPtrAddr_ = stub;
    lastStubPtrAddr_ = stub->addressOfNext();
    state_.trackAttached();
  }

  ICStubConstIterator beginChainConst() const {
    return ICStubConstIterator(icEntry_->firstStub());
  }

  ICStubIterator beginChain() { return ICStubIterator(this); }

  bool hasStub(ICStub::Kind kind) const {
    for (ICStubConstIterator iter = beginChainConst(); !iter.atEnd(); iter++) {
      if (iter->kind() == kind) {
        return true;
      }
    }
    return false;
  }

  unsigned numStubsWithKind(ICStub::Kind kind) const {
    unsigned count = 0;
    for (ICStubConstIterator iter = beginChainConst(); !iter.atEnd(); iter++) {
      if (iter->kind() == kind) {
        count++;
      }
    }
    return count;
  }

  void discardStubs(JSContext* cx);

  void unlinkStub(Zone* zone, ICStub* prev, ICStub* stub);
  void unlinkStubsWithKind(JSContext* cx, ICStub::Kind kind);

  // Return the number of times this stub has successfully provided a value to
  // the caller.
  uint32_t enteredCount() const { return enteredCount_; }
  inline void incrementEnteredCount() { enteredCount_++; }
  void resetEnteredCount() { enteredCount_ = 0; }
};

// Shared trait for all CacheIR stubs.
template <typename T>
class ICCacheIR_Trait {
 protected:
  const CacheIRStubInfo* stubInfo_;

  // Counts the number of times the stub was entered
  //
  // See Bug 1494473 comment 6 for a mechanism to handle overflow if overflow
  // becomes a concern.
  uint32_t enteredCount_;

 public:
  explicit ICCacheIR_Trait(const CacheIRStubInfo* stubInfo)
      : stubInfo_(stubInfo), enteredCount_(0) {}

  const CacheIRStubInfo* stubInfo() const { return stubInfo_; }

  // Return the number of times this stub has successfully provided a value to
  // the caller.
  uint32_t enteredCount() const { return enteredCount_; }
  void resetEnteredCount() { enteredCount_ = 0; }

  static size_t offsetOfEnteredCount() { return offsetof(T, enteredCount_); }
};

// Base class for Trait::Regular CacheIR stubs
class ICCacheIR_Regular : public ICStub,
                          public ICCacheIR_Trait<ICCacheIR_Regular> {
 public:
  ICCacheIR_Regular(JitCode* stubCode, const CacheIRStubInfo* stubInfo)
      : ICStub(ICStub::CacheIR_Regular, stubCode), ICCacheIR_Trait(stubInfo) {}

  void notePreliminaryObject() { extra_ = 1; }
  bool hasPreliminaryObject() const { return extra_; }

  uint8_t* stubDataStart();
};

// Monitored stubs are IC stubs that feed a single resulting value out to a
// type monitor operation.
class ICMonitoredStub : public ICStub {
 protected:
  // Pointer to the start of the type monitoring stub chain.
  ICStub* firstMonitorStub_;

  ICMonitoredStub(Kind kind, JitCode* stubCode, ICStub* firstMonitorStub);

 public:
  inline void updateFirstMonitorStub(ICStub* monitorStub) {
    // This should only be called once: when the first optimized monitor stub
    // is added to the type monitor IC chain.
    MOZ_ASSERT(firstMonitorStub_ &&
               firstMonitorStub_->isTypeMonitor_Fallback());
    firstMonitorStub_ = monitorStub;
  }
  inline void resetFirstMonitorStub(ICStub* monitorFallback) {
    MOZ_ASSERT(monitorFallback->isTypeMonitor_Fallback());
    firstMonitorStub_ = monitorFallback;
  }
  inline ICStub* firstMonitorStub() const { return firstMonitorStub_; }

  static inline size_t offsetOfFirstMonitorStub() {
    return offsetof(ICMonitoredStub, firstMonitorStub_);
  }
};

class ICCacheIR_Monitored : public ICMonitoredStub,
                            public ICCacheIR_Trait<ICCacheIR_Monitored> {
 public:
  ICCacheIR_Monitored(JitCode* stubCode, ICStub* firstMonitorStub,
                      const CacheIRStubInfo* stubInfo)
      : ICMonitoredStub(ICStub::CacheIR_Monitored, stubCode, firstMonitorStub),
        ICCacheIR_Trait(stubInfo) {}

  void notePreliminaryObject() { extra_ = 1; }
  bool hasPreliminaryObject() const { return extra_; }

  uint8_t* stubDataStart();
};

class ICCacheIR_Updated : public ICStub,
                          public ICCacheIR_Trait<ICCacheIR_Updated> {
  uint32_t numOptimizedStubs_;

  GCPtrObjectGroup updateStubGroup_;
  GCPtrId updateStubId_;

  // Pointer to the start of the type updating stub chain.
  ICStub* firstUpdateStub_;

  static const uint32_t MAX_OPTIMIZED_STUBS = 8;

 public:
  ICCacheIR_Updated(JitCode* stubCode, const CacheIRStubInfo* stubInfo)
      : ICStub(ICStub::CacheIR_Updated, ICStub::Updated, stubCode),
        ICCacheIR_Trait(stubInfo),
        numOptimizedStubs_(0),
        updateStubGroup_(nullptr),
        updateStubId_(JSID_EMPTY),
        firstUpdateStub_(nullptr) {}

  GCPtrObjectGroup& updateStubGroup() { return updateStubGroup_; }
  GCPtrId& updateStubId() { return updateStubId_; }

  uint8_t* stubDataStart();

  inline ICStub* firstUpdateStub() const { return firstUpdateStub_; }

  static inline size_t offsetOfFirstUpdateStub() {
    return offsetof(ICCacheIR_Updated, firstUpdateStub_);
  }

  inline uint32_t numOptimizedStubs() const { return numOptimizedStubs_; }

  void notePreliminaryObject() { extra_ = 1; }
  bool hasPreliminaryObject() const { return extra_; }

  MOZ_MUST_USE bool initUpdatingChain(JSContext* cx, ICStubSpace* space);

  MOZ_MUST_USE bool addUpdateStubForValue(JSContext* cx, HandleScript script,
                                          HandleObject obj,
                                          HandleObjectGroup group, HandleId id,
                                          HandleValue val);

  void addOptimizedUpdateStub(ICStub* stub) {
    if (firstUpdateStub_->isTypeUpdate_Fallback()) {
      stub->setNext(firstUpdateStub_);
      firstUpdateStub_ = stub;
    } else {
      ICStub* iter = firstUpdateStub_;
      MOZ_ASSERT(iter->next() != nullptr);
      while (!iter->next()->isTypeUpdate_Fallback()) {
        iter = iter->next();
      }
      MOZ_ASSERT(iter->next()->next() == nullptr);
      stub->setNext(iter->next());
      iter->setNext(stub);
    }

    numOptimizedStubs_++;
  }

  void resetUpdateStubChain(Zone* zone);

  bool hasTypeUpdateStub(ICStub::Kind kind) {
    ICStub* stub = firstUpdateStub_;
    do {
      if (stub->kind() == kind) {
        return true;
      }

      stub = stub->next();
    } while (stub);

    return false;
  }
};

// Base class for stubcode compilers.
class ICStubCompilerBase {
 protected:
  JSContext* cx;
  bool inStubFrame_ = false;

#ifdef DEBUG
  bool entersStubFrame_ = false;
  uint32_t framePushedAtEnterStubFrame_ = 0;
#endif

  explicit ICStubCompilerBase(JSContext* cx) : cx(cx) {}

  void pushCallArguments(MacroAssembler& masm,
                         AllocatableGeneralRegisterSet regs, Register argcReg,
                         bool isConstructing);

  // Push a payload specialized per compiler needed to execute stubs.
  void PushStubPayload(MacroAssembler& masm, Register scratch);
  void pushStubPayload(MacroAssembler& masm, Register scratch);

  // Emits a tail call to a VMFunction wrapper.
  MOZ_MUST_USE bool tailCallVMInternal(MacroAssembler& masm,
                                       TailCallVMFunctionId id);

  template <typename Fn, Fn fn>
  MOZ_MUST_USE bool tailCallVM(MacroAssembler& masm);

  // Emits a normal (non-tail) call to a VMFunction wrapper.
  MOZ_MUST_USE bool callVMInternal(MacroAssembler& masm, VMFunctionId id);

  template <typename Fn, Fn fn>
  MOZ_MUST_USE bool callVM(MacroAssembler& masm);

  // A stub frame is used when a stub wants to call into the VM without
  // performing a tail call. This is required for the return address
  // to pc mapping to work.
  void enterStubFrame(MacroAssembler& masm, Register scratch);
  void assumeStubFrame();
  void leaveStubFrame(MacroAssembler& masm, bool calledIntoIon = false);

 public:
  static inline AllocatableGeneralRegisterSet availableGeneralRegs(
      size_t numInputs) {
    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
#if defined(JS_CODEGEN_ARM)
    MOZ_ASSERT(!regs.has(BaselineStackReg));
    MOZ_ASSERT(!regs.has(ICTailCallReg));
    regs.take(BaselineSecondScratchReg);
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    MOZ_ASSERT(!regs.has(BaselineStackReg));
    MOZ_ASSERT(!regs.has(ICTailCallReg));
    MOZ_ASSERT(!regs.has(BaselineSecondScratchReg));
#elif defined(JS_CODEGEN_ARM64)
    MOZ_ASSERT(!regs.has(PseudoStackPointer));
    MOZ_ASSERT(!regs.has(RealStackPointer));
    MOZ_ASSERT(!regs.has(ICTailCallReg));
#else
    MOZ_ASSERT(!regs.has(BaselineStackReg));
#endif
    regs.take(BaselineFrameReg);
    regs.take(ICStubReg);
#ifdef JS_CODEGEN_X64
    regs.take(ExtractTemp0);
    regs.take(ExtractTemp1);
#endif

    switch (numInputs) {
      case 0:
        break;
      case 1:
        regs.take(R0);
        break;
      case 2:
        regs.take(R0);
        regs.take(R1);
        break;
      default:
        MOZ_CRASH("Invalid numInputs");
    }

    return regs;
  }
};

class ICStubCompiler : public ICStubCompilerBase {
  // Prevent GC in the middle of stub compilation.
  js::gc::AutoSuppressGC suppressGC;

 protected:
  ICStub::Kind kind;

  // By default the stubcode key is just the kind.
  virtual int32_t getKey() const { return static_cast<int32_t>(kind); }

  virtual MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) = 0;

  JitCode* getStubCode();

  ICStubCompiler(JSContext* cx, ICStub::Kind kind)
      : ICStubCompilerBase(cx), suppressGC(cx), kind(kind) {}

 protected:
  template <typename T, typename... Args>
  T* newStub(Args&&... args) {
    return ICStub::New<T>(cx, std::forward<Args>(args)...);
  }

 public:
  virtual ICStub* getStub(ICStubSpace* space) = 0;

  static ICStubSpace* StubSpaceForStub(bool makesGCCalls, JSScript* script);

  ICStubSpace* getStubSpace(JSScript* outerScript) {
    return StubSpaceForStub(ICStub::NonCacheIRStubMakesGCCalls(kind),
                            outerScript);
  }
};

// Monitored fallback stubs - as the name implies.
class ICMonitoredFallbackStub : public ICFallbackStub {
 protected:
  // Pointer to the fallback monitor stub. Created lazily by
  // getFallbackMonitorStub if needed.
  ICTypeMonitor_Fallback* fallbackMonitorStub_;

  ICMonitoredFallbackStub(Kind kind, TrampolinePtr stubCode)
      : ICFallbackStub(kind, ICStub::MonitoredFallback, stubCode),
        fallbackMonitorStub_(nullptr) {}

 public:
  MOZ_MUST_USE bool initMonitoringChain(JSContext* cx, JSScript* script);

  ICTypeMonitor_Fallback* maybeFallbackMonitorStub() const {
    return fallbackMonitorStub_;
  }
  ICTypeMonitor_Fallback* getFallbackMonitorStub(JSContext* cx,
                                                 JSScript* script) {
    if (!fallbackMonitorStub_ && !initMonitoringChain(cx, script)) {
      return nullptr;
    }
    MOZ_ASSERT(fallbackMonitorStub_);
    return fallbackMonitorStub_;
  }

  static inline size_t offsetOfFallbackMonitorStub() {
    return offsetof(ICMonitoredFallbackStub, fallbackMonitorStub_);
  }
};

// TypeCheckPrimitiveSetStub
//   Base class for IC stubs (TypeUpdate or TypeMonitor) that check that a given
//   value's type falls within a set of primitive types.

class TypeCheckPrimitiveSetStub : public ICStub {
  friend class ICStubSpace;

 protected:
  inline static uint16_t TypeToFlag(ValueType type) {
    return 1u << static_cast<unsigned>(type);
  }

  inline static uint16_t ValidFlags() {
    return ((TypeToFlag(ValueType::Object) << 1) - 1) &
           ~(TypeToFlag(ValueType::Magic) |
             TypeToFlag(ValueType::PrivateGCThing));
  }

  TypeCheckPrimitiveSetStub(Kind kind, JitCode* stubCode, uint16_t flags)
      : ICStub(kind, stubCode) {
    MOZ_ASSERT(kind == TypeMonitor_PrimitiveSet ||
               kind == TypeUpdate_PrimitiveSet);
    MOZ_ASSERT(flags && !(flags & ~ValidFlags()));
    extra_ = flags;
  }

  TypeCheckPrimitiveSetStub* updateTypesAndCode(uint16_t flags, JitCode* code) {
    MOZ_ASSERT(flags && !(flags & ~ValidFlags()));
    if (!code) {
      return nullptr;
    }
    extra_ = flags;
    updateCode(code);
    return this;
  }

 public:
  uint16_t typeFlags() const { return extra_; }

  bool containsType(ValueType type) const {
    MOZ_ASSERT(type != ValueType::Magic && type != ValueType::PrivateGCThing);
    return extra_ & TypeToFlag(type);
  }

  ICTypeMonitor_PrimitiveSet* toMonitorStub() {
    return toTypeMonitor_PrimitiveSet();
  }

  ICTypeUpdate_PrimitiveSet* toUpdateStub() {
    return toTypeUpdate_PrimitiveSet();
  }

  class Compiler : public ICStubCompiler {
   protected:
    TypeCheckPrimitiveSetStub* existingStub_;
    uint16_t flags_;

    virtual int32_t getKey() const override {
      return static_cast<int32_t>(kind) | (static_cast<int32_t>(flags_) << 16);
    }

   public:
    Compiler(JSContext* cx, Kind kind, TypeCheckPrimitiveSetStub* existingStub,
             ValueType type)
        : ICStubCompiler(cx, kind),
          existingStub_(existingStub),
          flags_((existingStub ? existingStub->typeFlags() : 0) |
                 TypeToFlag(type)) {
      MOZ_ASSERT_IF(existingStub_, flags_ != existingStub_->typeFlags());
    }

    TypeCheckPrimitiveSetStub* updateStub() {
      MOZ_ASSERT(existingStub_);
      return existingStub_->updateTypesAndCode(flags_, getStubCode());
    }
  };
};

// TypeMonitor

// The TypeMonitor fallback stub is not always a regular fallback stub. When
// used for monitoring the values pushed by a bytecode it doesn't hold a
// pointer to the IC entry, but rather back to the main fallback stub for the
// IC (from which a pointer to the IC entry can be retrieved). When monitoring
// the types of 'this', arguments or other values with no associated IC, there
// is no main fallback stub, and the IC entry is referenced directly.
class ICTypeMonitor_Fallback : public ICStub {
  friend class ICStubSpace;

  static const uint32_t MAX_OPTIMIZED_STUBS = 8;

  // Pointer to the main fallback stub for the IC or to the main IC entry,
  // depending on hasFallbackStub.
  union {
    ICMonitoredFallbackStub* mainFallbackStub_;
    ICEntry* icEntry_;
  };

  // Pointer to the first monitor stub.
  ICStub* firstMonitorStub_;

  // Address of the last monitor stub's field pointing to this
  // fallback monitor stub.  This will get updated when new
  // monitor stubs are created and added.
  ICStub** lastMonitorStubPtrAddr_;

  // Count of optimized type monitor stubs in this chain.
  uint32_t numOptimizedMonitorStubs_ : 7;

  // Whether this has a fallback stub referring to the IC entry.
  bool hasFallbackStub_ : 1;

  // Index of 'this' or argument which is being monitored, or BYTECODE_INDEX
  // if this is monitoring the types of values pushed at some bytecode.
  uint32_t argumentIndex_ : 23;

  static const uint32_t BYTECODE_INDEX = (1 << 23) - 1;

  ICTypeMonitor_Fallback(TrampolinePtr stubCode,
                         ICMonitoredFallbackStub* mainFallbackStub,
                         uint32_t argumentIndex = BYTECODE_INDEX)
      : ICStub(ICStub::TypeMonitor_Fallback, stubCode.value),
        mainFallbackStub_(mainFallbackStub),
        firstMonitorStub_(thisFromCtor()),
        lastMonitorStubPtrAddr_(nullptr),
        numOptimizedMonitorStubs_(0),
        hasFallbackStub_(mainFallbackStub != nullptr),
        argumentIndex_(argumentIndex) {}

  ICTypeMonitor_Fallback* thisFromCtor() { return this; }

  void addOptimizedMonitorStub(ICStub* stub) {
    stub->setNext(this);

    MOZ_ASSERT((lastMonitorStubPtrAddr_ != nullptr) ==
               (numOptimizedMonitorStubs_ || !hasFallbackStub_));

    if (lastMonitorStubPtrAddr_) {
      *lastMonitorStubPtrAddr_ = stub;
    }

    if (numOptimizedMonitorStubs_ == 0) {
      MOZ_ASSERT(firstMonitorStub_ == this);
      firstMonitorStub_ = stub;
    } else {
      MOZ_ASSERT(firstMonitorStub_ != nullptr);
    }

    lastMonitorStubPtrAddr_ = stub->addressOfNext();
    numOptimizedMonitorStubs_++;
  }

 public:
  bool hasStub(ICStub::Kind kind) {
    ICStub* stub = firstMonitorStub_;
    do {
      if (stub->kind() == kind) {
        return true;
      }

      stub = stub->next();
    } while (stub);

    return false;
  }

  inline ICFallbackStub* mainFallbackStub() const {
    MOZ_ASSERT(hasFallbackStub_);
    return mainFallbackStub_;
  }

  inline ICEntry* icEntry() const {
    return hasFallbackStub_ ? mainFallbackStub()->icEntry() : icEntry_;
  }

  inline ICStub* firstMonitorStub() const { return firstMonitorStub_; }

  static inline size_t offsetOfFirstMonitorStub() {
    return offsetof(ICTypeMonitor_Fallback, firstMonitorStub_);
  }

  inline uint32_t numOptimizedMonitorStubs() const {
    return numOptimizedMonitorStubs_;
  }

  inline bool monitorsThis() const { return argumentIndex_ == 0; }

  inline bool monitorsArgument(uint32_t* pargument) const {
    if (argumentIndex_ > 0 && argumentIndex_ < BYTECODE_INDEX) {
      *pargument = argumentIndex_ - 1;
      return true;
    }
    return false;
  }

  inline bool monitorsBytecode() const {
    return argumentIndex_ == BYTECODE_INDEX;
  }

  // Fixup the IC entry as for a normal fallback stub, for this/arguments.
  void fixupICEntry(ICEntry* icEntry) {
    MOZ_ASSERT(!hasFallbackStub_);
    MOZ_ASSERT(icEntry_ == nullptr);
    MOZ_ASSERT(lastMonitorStubPtrAddr_ == nullptr);
    icEntry_ = icEntry;
    lastMonitorStubPtrAddr_ = icEntry_->addressOfFirstStub();
  }

  // Create a new monitor stub for the type of the given value, and
  // add it to this chain.
  MOZ_MUST_USE bool addMonitorStubForValue(JSContext* cx, BaselineFrame* frame,
                                           StackTypeSet* types,
                                           HandleValue val);

  void resetMonitorStubChain(Zone* zone);
};

class ICTypeMonitor_PrimitiveSet : public TypeCheckPrimitiveSetStub {
  friend class ICStubSpace;

  ICTypeMonitor_PrimitiveSet(JitCode* stubCode, uint16_t flags)
      : TypeCheckPrimitiveSetStub(TypeMonitor_PrimitiveSet, stubCode, flags) {}

 public:
  class Compiler : public TypeCheckPrimitiveSetStub::Compiler {
   protected:
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

   public:
    Compiler(JSContext* cx, ICTypeMonitor_PrimitiveSet* existingStub,
             ValueType type)
        : TypeCheckPrimitiveSetStub::Compiler(cx, TypeMonitor_PrimitiveSet,
                                              existingStub, type) {}

    ICTypeMonitor_PrimitiveSet* updateStub() {
      TypeCheckPrimitiveSetStub* stub =
          this->TypeCheckPrimitiveSetStub::Compiler::updateStub();
      if (!stub) {
        return nullptr;
      }
      return stub->toMonitorStub();
    }

    ICTypeMonitor_PrimitiveSet* getStub(ICStubSpace* space) override {
      MOZ_ASSERT(!existingStub_);
      return newStub<ICTypeMonitor_PrimitiveSet>(space, getStubCode(), flags_);
    }
  };
};

class ICTypeMonitor_SingleObject : public ICStub {
  friend class ICStubSpace;

  GCPtrObject obj_;

  ICTypeMonitor_SingleObject(JitCode* stubCode, JSObject* obj);

 public:
  GCPtrObject& object() { return obj_; }

  static size_t offsetOfObject() {
    return offsetof(ICTypeMonitor_SingleObject, obj_);
  }

  class Compiler : public ICStubCompiler {
   protected:
    HandleObject obj_;
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

   public:
    Compiler(JSContext* cx, HandleObject obj)
        : ICStubCompiler(cx, TypeMonitor_SingleObject), obj_(obj) {}

    ICTypeMonitor_SingleObject* getStub(ICStubSpace* space) override {
      return newStub<ICTypeMonitor_SingleObject>(space, getStubCode(), obj_);
    }
  };
};

class ICTypeMonitor_ObjectGroup : public ICStub {
  friend class ICStubSpace;

  GCPtrObjectGroup group_;

  ICTypeMonitor_ObjectGroup(JitCode* stubCode, ObjectGroup* group);

 public:
  GCPtrObjectGroup& group() { return group_; }

  static size_t offsetOfGroup() {
    return offsetof(ICTypeMonitor_ObjectGroup, group_);
  }

  class Compiler : public ICStubCompiler {
   protected:
    HandleObjectGroup group_;
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

   public:
    Compiler(JSContext* cx, HandleObjectGroup group)
        : ICStubCompiler(cx, TypeMonitor_ObjectGroup), group_(group) {}

    ICTypeMonitor_ObjectGroup* getStub(ICStubSpace* space) override {
      return newStub<ICTypeMonitor_ObjectGroup>(space, getStubCode(), group_);
    }
  };
};

class ICTypeMonitor_AnyValue : public ICStub {
  friend class ICStubSpace;

  explicit ICTypeMonitor_AnyValue(JitCode* stubCode)
      : ICStub(TypeMonitor_AnyValue, stubCode) {}

 public:
  class Compiler : public ICStubCompiler {
   protected:
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

   public:
    explicit Compiler(JSContext* cx)
        : ICStubCompiler(cx, TypeMonitor_AnyValue) {}

    ICTypeMonitor_AnyValue* getStub(ICStubSpace* space) override {
      return newStub<ICTypeMonitor_AnyValue>(space, getStubCode());
    }
  };
};

// TypeUpdate

// The TypeUpdate fallback is not a regular fallback, since it just
// forwards to a different entry point in the main fallback stub.
class ICTypeUpdate_Fallback : public ICStub {
  friend class ICStubSpace;

  explicit ICTypeUpdate_Fallback(TrampolinePtr stubCode)
      : ICStub(ICStub::TypeUpdate_Fallback, stubCode.value) {}
};

class ICTypeUpdate_PrimitiveSet : public TypeCheckPrimitiveSetStub {
  friend class ICStubSpace;

  ICTypeUpdate_PrimitiveSet(JitCode* stubCode, uint16_t flags)
      : TypeCheckPrimitiveSetStub(TypeUpdate_PrimitiveSet, stubCode, flags) {}

 public:
  class Compiler : public TypeCheckPrimitiveSetStub::Compiler {
   protected:
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

   public:
    Compiler(JSContext* cx, ICTypeUpdate_PrimitiveSet* existingStub,
             ValueType type)
        : TypeCheckPrimitiveSetStub::Compiler(cx, TypeUpdate_PrimitiveSet,
                                              existingStub, type) {}

    ICTypeUpdate_PrimitiveSet* updateStub() {
      TypeCheckPrimitiveSetStub* stub =
          this->TypeCheckPrimitiveSetStub::Compiler::updateStub();
      if (!stub) {
        return nullptr;
      }
      return stub->toUpdateStub();
    }

    ICTypeUpdate_PrimitiveSet* getStub(ICStubSpace* space) override {
      MOZ_ASSERT(!existingStub_);
      return newStub<ICTypeUpdate_PrimitiveSet>(space, getStubCode(), flags_);
    }
  };
};

// Type update stub to handle a singleton object.
class ICTypeUpdate_SingleObject : public ICStub {
  friend class ICStubSpace;

  GCPtrObject obj_;

  ICTypeUpdate_SingleObject(JitCode* stubCode, JSObject* obj);

 public:
  GCPtrObject& object() { return obj_; }

  static size_t offsetOfObject() {
    return offsetof(ICTypeUpdate_SingleObject, obj_);
  }

  class Compiler : public ICStubCompiler {
   protected:
    HandleObject obj_;
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

   public:
    Compiler(JSContext* cx, HandleObject obj)
        : ICStubCompiler(cx, TypeUpdate_SingleObject), obj_(obj) {}

    ICTypeUpdate_SingleObject* getStub(ICStubSpace* space) override {
      return newStub<ICTypeUpdate_SingleObject>(space, getStubCode(), obj_);
    }
  };
};

// Type update stub to handle a single ObjectGroup.
class ICTypeUpdate_ObjectGroup : public ICStub {
  friend class ICStubSpace;

  GCPtrObjectGroup group_;

  ICTypeUpdate_ObjectGroup(JitCode* stubCode, ObjectGroup* group);

 public:
  GCPtrObjectGroup& group() { return group_; }

  static size_t offsetOfGroup() {
    return offsetof(ICTypeUpdate_ObjectGroup, group_);
  }

  class Compiler : public ICStubCompiler {
   protected:
    HandleObjectGroup group_;
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

   public:
    Compiler(JSContext* cx, HandleObjectGroup group)
        : ICStubCompiler(cx, TypeUpdate_ObjectGroup), group_(group) {}

    ICTypeUpdate_ObjectGroup* getStub(ICStubSpace* space) override {
      return newStub<ICTypeUpdate_ObjectGroup>(space, getStubCode(), group_);
    }
  };
};

class ICTypeUpdate_AnyValue : public ICStub {
  friend class ICStubSpace;

  explicit ICTypeUpdate_AnyValue(JitCode* stubCode)
      : ICStub(TypeUpdate_AnyValue, stubCode) {}

 public:
  class Compiler : public ICStubCompiler {
   protected:
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

   public:
    explicit Compiler(JSContext* cx)
        : ICStubCompiler(cx, TypeUpdate_AnyValue) {}

    ICTypeUpdate_AnyValue* getStub(ICStubSpace* space) override {
      return newStub<ICTypeUpdate_AnyValue>(space, getStubCode());
    }
  };
};

// ToBool
//      JSOp::IfNe

class ICToBool_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICToBool_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::ToBool_Fallback, stubCode) {}

 public:
  static const uint32_t MAX_OPTIMIZED_STUBS = 8;
};

// GetElem
//      JSOp::GetElem
//      JSOp::GetElemSuper

class ICGetElem_Fallback : public ICMonitoredFallbackStub {
  friend class ICStubSpace;

  explicit ICGetElem_Fallback(TrampolinePtr stubCode)
      : ICMonitoredFallbackStub(ICStub::GetElem_Fallback, stubCode) {}

  static const uint16_t EXTRA_NEGATIVE_INDEX = 0x1;
  static const uint16_t SAW_NON_INTEGER_INDEX_BIT = 0x2;

 public:
  void noteNegativeIndex() { extra_ |= EXTRA_NEGATIVE_INDEX; }
  bool hasNegativeIndex() const { return extra_ & EXTRA_NEGATIVE_INDEX; }

  void setSawNonIntegerIndex() { extra_ |= SAW_NON_INTEGER_INDEX_BIT; }
  bool sawNonIntegerIndex() const { return extra_ & SAW_NON_INTEGER_INDEX_BIT; }
};

// SetElem
//      JSOp::SetElem
//      JSOp::InitElem

class ICSetElem_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICSetElem_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::SetElem_Fallback, stubCode) {}

  static const size_t HasDenseAddFlag = 0x1;
  static const size_t HasTypedArrayOOBFlag = 0x2;

 public:
  void noteHasDenseAdd() { extra_ |= HasDenseAddFlag; }
  bool hasDenseAdd() const { return extra_ & HasDenseAddFlag; }

  void noteHasTypedArrayOOB() { extra_ |= HasTypedArrayOOBFlag; }
  bool hasTypedArrayOOB() const { return extra_ & HasTypedArrayOOBFlag; }
};

// In
//      JSOp::In
class ICIn_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICIn_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::In_Fallback, stubCode) {}
};

// HasOwn
//      JSOp::HasOwn
class ICHasOwn_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICHasOwn_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::HasOwn_Fallback, stubCode) {}
};

// GetName
//      JSOp::GetName
//      JSOp::GetGName
class ICGetName_Fallback : public ICMonitoredFallbackStub {
  friend class ICStubSpace;

  explicit ICGetName_Fallback(TrampolinePtr stubCode)
      : ICMonitoredFallbackStub(ICStub::GetName_Fallback, stubCode) {}
};

// BindName
//      JSOp::BindName
class ICBindName_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICBindName_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::BindName_Fallback, stubCode) {}
};

// GetIntrinsic
//      JSOp::GetIntrinsic
class ICGetIntrinsic_Fallback : public ICMonitoredFallbackStub {
  friend class ICStubSpace;

  explicit ICGetIntrinsic_Fallback(TrampolinePtr stubCode)
      : ICMonitoredFallbackStub(ICStub::GetIntrinsic_Fallback, stubCode) {}
};

// GetProp
//     JSOp::GetProp
//     JSOp::GetPropSuper

class ICGetProp_Fallback : public ICMonitoredFallbackStub {
  friend class ICStubSpace;

  explicit ICGetProp_Fallback(TrampolinePtr stubCode)
      : ICMonitoredFallbackStub(ICStub::GetProp_Fallback, stubCode) {}

 public:
  static const size_t ACCESSED_GETTER_BIT = 1;

  void noteAccessedGetter() { extra_ |= (1u << ACCESSED_GETTER_BIT); }
  bool hasAccessedGetter() const {
    return extra_ & (1u << ACCESSED_GETTER_BIT);
  }
};

// SetProp
//     JSOp::SetProp
//     JSOp::SetName
//     JSOp::SetGName
//     JSOp::InitProp

class ICSetProp_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICSetProp_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::SetProp_Fallback, stubCode) {}
};

// Call
//      JSOp::Call
//      JSOp::CallIgnoresRv
//      JSOp::CallIter
//      JSOp::FunApply
//      JSOp::FunCall
//      JSOp::New
//      JSOp::SuperCall
//      JSOp::Eval
//      JSOp::StrictEval
//      JSOp::SpreadCall
//      JSOp::SpreadNew
//      JSOp::SpreadSuperCall
//      JSOp::SpreadEval
//      JSOp::SpreadStrictEval

class ICCall_Fallback : public ICMonitoredFallbackStub {
  friend class ICStubSpace;

 public:
  static const uint32_t MAX_OPTIMIZED_STUBS = 16;

 private:
  explicit ICCall_Fallback(TrampolinePtr stubCode)
      : ICMonitoredFallbackStub(ICStub::Call_Fallback, stubCode) {}
};

// IC for constructing an iterator from an input value.
class ICGetIterator_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICGetIterator_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::GetIterator_Fallback, stubCode) {}
};

// InstanceOf
//      JSOp::Instanceof
class ICInstanceOf_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICInstanceOf_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::InstanceOf_Fallback, stubCode) {}
};

// TypeOf
//      JSOp::Typeof
//      JSOp::TypeofExpr
class ICTypeOf_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICTypeOf_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::TypeOf_Fallback, stubCode) {}

 public:
  static const uint32_t MAX_OPTIMIZED_STUBS = 6;
};

class ICRest_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  GCPtrArrayObject templateObject_;

  ICRest_Fallback(TrampolinePtr stubCode, ArrayObject* templateObject)
      : ICFallbackStub(ICStub::Rest_Fallback, stubCode),
        templateObject_(templateObject) {}

 public:
  static const uint32_t MAX_OPTIMIZED_STUBS = 8;

  GCPtrArrayObject& templateObject() { return templateObject_; }
};

// UnaryArith
//     JSOp::BitNot
//     JSOp::Pos
//     JSOp::Neg
//     JSOp::Inc
//     JSOp::Dec

class ICUnaryArith_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICUnaryArith_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(UnaryArith_Fallback, stubCode) {}

 public:
  bool sawDoubleResult() { return extra_; }
  void setSawDoubleResult() { extra_ = 1; }
};

// Compare
//      JSOp::Lt
//      JSOp::Le
//      JSOp::Gt
//      JSOp::Ge
//      JSOp::Eq
//      JSOp::Ne
//      JSOp::StrictEq
//      JSOp::StrictNe

class ICCompare_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICCompare_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::Compare_Fallback, stubCode) {}
};

// BinaryArith
//      JSOp::Add, JSOp::Sub, JSOp::Mul, JOP_DIV, JSOp::Mod
//      JSOp::BitAnd, JSOp::BitXor, JSOp::BitOr
//      JSOp::Lsh, JSOp::Rsh, JSOp::Ursh

class ICBinaryArith_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  explicit ICBinaryArith_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(BinaryArith_Fallback, stubCode) {}

  static const uint16_t SAW_DOUBLE_RESULT_BIT = 0x1;

 public:
  static const uint32_t MAX_OPTIMIZED_STUBS = 8;

  bool sawDoubleResult() const { return extra_ & SAW_DOUBLE_RESULT_BIT; }
  void setSawDoubleResult() { extra_ |= SAW_DOUBLE_RESULT_BIT; }
};

// JSOp::NewArray

class ICNewArray_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  GCPtrObject templateObject_;

  // The group used for objects created here is always available, even if the
  // template object itself is not.
  GCPtrObjectGroup templateGroup_;

  ICNewArray_Fallback(TrampolinePtr stubCode, ObjectGroup* templateGroup)
      : ICFallbackStub(ICStub::NewArray_Fallback, stubCode),
        templateObject_(nullptr),
        templateGroup_(templateGroup) {}

 public:
  GCPtrObject& templateObject() { return templateObject_; }

  void setTemplateObject(JSObject* obj) {
    MOZ_ASSERT(obj->group() == templateGroup());
    templateObject_ = obj;
  }

  GCPtrObjectGroup& templateGroup() { return templateGroup_; }

  void setTemplateGroup(ObjectGroup* group) {
    templateObject_ = nullptr;
    templateGroup_ = group;
  }
};

// JSOp::NewObject

class ICNewObject_Fallback : public ICFallbackStub {
  friend class ICStubSpace;

  GCPtrObject templateObject_;

  explicit ICNewObject_Fallback(TrampolinePtr stubCode)
      : ICFallbackStub(ICStub::NewObject_Fallback, stubCode),
        templateObject_(nullptr) {}

 public:
  GCPtrObject& templateObject() { return templateObject_; }

  void setTemplateObject(JSObject* obj) { templateObject_ = obj; }
};

inline bool IsCacheableDOMProxy(JSObject* obj) {
  if (!obj->is<ProxyObject>()) {
    return false;
  }

  const BaseProxyHandler* handler = obj->as<ProxyObject>().handler();
  if (handler->family() != GetDOMProxyHandlerFamily()) {
    return false;
  }

  // Some DOM proxies have dynamic prototypes.  We can't really cache those very
  // well.
  return obj->hasStaticPrototype();
}

struct IonOsrTempData;

extern MOZ_MUST_USE bool TypeMonitorResult(JSContext* cx,
                                           ICMonitoredFallbackStub* stub,
                                           BaselineFrame* frame,
                                           HandleScript script, jsbytecode* pc,
                                           HandleValue val);

extern bool DoTypeUpdateFallback(JSContext* cx, BaselineFrame* frame,
                                 ICCacheIR_Updated* stub, HandleValue objval,
                                 HandleValue value);

extern bool DoCallFallback(JSContext* cx, BaselineFrame* frame,
                           ICCall_Fallback* stub, uint32_t argc, Value* vp,
                           MutableHandleValue res);

extern bool DoSpreadCallFallback(JSContext* cx, BaselineFrame* frame,
                                 ICCall_Fallback* stub, Value* vp,
                                 MutableHandleValue res);

extern bool DoTypeMonitorFallback(JSContext* cx, BaselineFrame* frame,
                                  ICTypeMonitor_Fallback* stub,
                                  HandleValue value, MutableHandleValue res);

extern bool DoToBoolFallback(JSContext* cx, BaselineFrame* frame,
                             ICToBool_Fallback* stub, HandleValue arg,
                             MutableHandleValue ret);

extern bool DoGetElemSuperFallback(JSContext* cx, BaselineFrame* frame,
                                   ICGetElem_Fallback* stub, HandleValue lhs,
                                   HandleValue rhs, HandleValue receiver,
                                   MutableHandleValue res);

extern bool DoGetElemFallback(JSContext* cx, BaselineFrame* frame,
                              ICGetElem_Fallback* stub, HandleValue lhs,
                              HandleValue rhs, MutableHandleValue res);

extern bool DoSetElemFallback(JSContext* cx, BaselineFrame* frame,
                              ICSetElem_Fallback* stub, Value* stack,
                              HandleValue objv, HandleValue index,
                              HandleValue rhs);

extern bool DoInFallback(JSContext* cx, BaselineFrame* frame,
                         ICIn_Fallback* stub, HandleValue key,
                         HandleValue objValue, MutableHandleValue res);

extern bool DoHasOwnFallback(JSContext* cx, BaselineFrame* frame,
                             ICHasOwn_Fallback* stub, HandleValue keyValue,
                             HandleValue objValue, MutableHandleValue res);

extern bool DoGetNameFallback(JSContext* cx, BaselineFrame* frame,
                              ICGetName_Fallback* stub, HandleObject envChain,
                              MutableHandleValue res);

extern bool DoBindNameFallback(JSContext* cx, BaselineFrame* frame,
                               ICBindName_Fallback* stub, HandleObject envChain,
                               MutableHandleValue res);

extern bool DoGetIntrinsicFallback(JSContext* cx, BaselineFrame* frame,
                                   ICGetIntrinsic_Fallback* stub,
                                   MutableHandleValue res);

extern bool DoGetPropFallback(JSContext* cx, BaselineFrame* frame,
                              ICGetProp_Fallback* stub, MutableHandleValue val,
                              MutableHandleValue res);

extern bool DoGetPropSuperFallback(JSContext* cx, BaselineFrame* frame,
                                   ICGetProp_Fallback* stub,
                                   HandleValue receiver, MutableHandleValue val,
                                   MutableHandleValue res);

extern bool DoSetPropFallback(JSContext* cx, BaselineFrame* frame,
                              ICSetProp_Fallback* stub, Value* stack,
                              HandleValue lhs, HandleValue rhs);

extern bool DoGetIteratorFallback(JSContext* cx, BaselineFrame* frame,
                                  ICGetIterator_Fallback* stub,
                                  HandleValue value, MutableHandleValue res);

extern bool DoInstanceOfFallback(JSContext* cx, BaselineFrame* frame,
                                 ICInstanceOf_Fallback* stub, HandleValue lhs,
                                 HandleValue rhs, MutableHandleValue res);

extern bool DoTypeOfFallback(JSContext* cx, BaselineFrame* frame,
                             ICTypeOf_Fallback* stub, HandleValue val,
                             MutableHandleValue res);

extern bool DoRestFallback(JSContext* cx, BaselineFrame* frame,
                           ICRest_Fallback* stub, MutableHandleValue res);

extern bool DoUnaryArithFallback(JSContext* cx, BaselineFrame* frame,
                                 ICUnaryArith_Fallback* stub, HandleValue val,
                                 MutableHandleValue res);

extern bool DoBinaryArithFallback(JSContext* cx, BaselineFrame* frame,
                                  ICBinaryArith_Fallback* stub, HandleValue lhs,
                                  HandleValue rhs, MutableHandleValue ret);

extern bool DoNewArrayFallback(JSContext* cx, BaselineFrame* frame,
                               ICNewArray_Fallback* stub, uint32_t length,
                               MutableHandleValue res);

extern bool DoNewObjectFallback(JSContext* cx, BaselineFrame* frame,
                                ICNewObject_Fallback* stub,
                                MutableHandleValue res);

extern bool DoCompareFallback(JSContext* cx, BaselineFrame* frame,
                              ICCompare_Fallback* stub, HandleValue lhs,
                              HandleValue rhs, MutableHandleValue ret);

}  // namespace jit
}  // namespace js

#endif /* jit_BaselineIC_h */
