/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineIC_h
#define jit_BaselineIC_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "gc/Barrier.h"
#include "gc/GC.h"
#include "gc/Rooting.h"
#include "jit/BaselineICList.h"
#include "jit/ICState.h"
#include "jit/ICStubSpace.h"
#include "jit/JitCode.h"
#include "jit/JitOptions.h"
#include "jit/Registers.h"
#include "jit/RegisterSets.h"
#include "jit/shared/Assembler-shared.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/ArrayObject.h"
#include "vm/JSScript.h"

class JS_PUBLIC_API JSTracer;

namespace js {

MOZ_COLD void ReportOutOfMemory(JSContext* cx);

namespace jit {

class BaselineFrame;
class CacheIRStubInfo;
class ICScript;
class MacroAssembler;

enum class TailCallVMFunctionId;
enum class VMFunctionId;

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

class ICStub;
class ICCacheIRStub;
class ICFallbackStub;

#ifdef JS_JITSPEW
void FallbackICSpew(JSContext* cx, ICFallbackStub* stub, const char* fmt, ...)
    MOZ_FORMAT_PRINTF(3, 4);
#else
#  define FallbackICSpew(...)
#endif

// An entry in the BaselineScript IC descriptor table. There's one ICEntry per
// IC.
class ICEntry {
  // A pointer to the first IC stub for this instruction.
  ICStub* firstStub_;

  // The PC offset of this IC's bytecode op within the JSScript.
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
  ICEntry(ICStub* firstStub, uint32_t pcOffset)
      : firstStub_(firstStub), pcOffset_(pcOffset) {}

  ICStub* firstStub() const {
    MOZ_ASSERT(firstStub_);
    return firstStub_;
  }

  ICFallbackStub* fallbackStub() const;

  void setFirstStub(ICStub* stub) { firstStub_ = stub; }

  uint32_t pcOffset() const { return pcOffset_; }
  jsbytecode* pc(JSScript* script) const {
    return script->offsetToPC(pcOffset());
  }

  static constexpr size_t offsetOfFirstStub() {
    return offsetof(ICEntry, firstStub_);
  }

  void trace(JSTracer* trc);
};

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
  ICCacheIRStub* previousStub_;
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

 protected:
  // The raw jitcode to call for this stub.
  uint8_t* stubCode_;

  // Counts the number of times the stub was entered
  //
  // See Bug 1494473 comment 6 for a mechanism to handle overflow if overflow
  // becomes a concern.
  uint32_t enteredCount_ = 0;

  // Whether this is an ICFallbackStub or an ICCacheIRStub.
  bool isFallback_;

  ICStub(uint8_t* stubCode, bool isFallback)
      : stubCode_(stubCode), isFallback_(isFallback) {
    MOZ_ASSERT(stubCode != nullptr);
  }

 public:
  inline bool isFallback() const { return isFallback_; }

  inline ICStub* maybeNext() const;

  inline const ICFallbackStub* toFallbackStub() const {
    MOZ_ASSERT(isFallback());
    return reinterpret_cast<const ICFallbackStub*>(this);
  }

  inline ICFallbackStub* toFallbackStub() {
    MOZ_ASSERT(isFallback());
    return reinterpret_cast<ICFallbackStub*>(this);
  }

  ICCacheIRStub* toCacheIRStub() {
    MOZ_ASSERT(!isFallback());
    return reinterpret_cast<ICCacheIRStub*>(this);
  }
  const ICCacheIRStub* toCacheIRStub() const {
    MOZ_ASSERT(!isFallback());
    return reinterpret_cast<const ICCacheIRStub*>(this);
  }

  bool usesTrampolineCode() const {
    // All fallback code is stored in a single JitCode instance, so we can't
    // call JitCode::FromExecutable on the raw pointer.
    return isFallback();
  }
  JitCode* jitCode() {
    MOZ_ASSERT(!usesTrampolineCode());
    return JitCode::FromExecutable(stubCode_);
  }

  uint32_t enteredCount() const { return enteredCount_; }
  inline void incrementEnteredCount() { enteredCount_++; }
  void resetEnteredCount() { enteredCount_ = 0; }

  inline ICFallbackStub* getChainFallback();

  static constexpr size_t offsetOfStubCode() {
    return offsetof(ICStub, stubCode_);
  }
  static constexpr size_t offsetOfEnteredCount() {
    return offsetof(ICStub, enteredCount_);
  }
};

class ICFallbackStub final : public ICStub {
  friend class ICStubConstIterator;

 protected:
  // Fallback stubs need these fields to easily add new stubs to
  // the linked list of stubs for an IC.

  // The IC entry in JitScript for this linked list of stubs.
  ICEntry* icEntry_ = nullptr;

  // The state of this IC.
  ICState state_{};

 public:
  explicit ICFallbackStub(ICEntry* icEntry, TrampolinePtr stubCode)
      : ICStub(stubCode.value, /* isFallback = */ true), icEntry_(icEntry) {}

  inline ICEntry* icEntry() const { return icEntry_; }

  inline size_t numOptimizedStubs() const { return state_.numOptimizedStubs(); }

  bool newStubIsFirstStub() const { return state_.newStubIsFirstStub(); }

  ICState& state() { return state_; }

  // Add a new stub to the IC chain terminated by this fallback stub.
  inline void addNewStub(ICCacheIRStub* stub);

  ICStubConstIterator beginChainConst() const {
    return ICStubConstIterator(icEntry_->firstStub());
  }

  ICStubIterator beginChain() { return ICStubIterator(this); }

  void discardStubs(JSContext* cx);

  void clearUsedByTranspiler() { state_.clearUsedByTranspiler(); }
  void setUsedByTranspiler() { state_.setUsedByTranspiler(); }

  TrialInliningState trialInliningState() const {
    return state_.trialInliningState();
  }
  void setTrialInliningState(TrialInliningState state) {
    state_.setTrialInliningState(state);
  }

  void trackNotAttached();

  void unlinkStub(Zone* zone, ICCacheIRStub* prev, ICCacheIRStub* stub);
};

class ICCacheIRStub final : public ICStub {
  // Pointer to next IC stub.
  ICStub* next_ = nullptr;

  const CacheIRStubInfo* stubInfo_;

#ifndef JS_64BIT
  // Ensure stub data is 8-byte aligned on 32-bit.
  uintptr_t padding_ = 0;
#endif

 public:
  ICCacheIRStub(JitCode* stubCode, const CacheIRStubInfo* stubInfo)
      : ICStub(stubCode->raw(), /* isFallback = */ false),
        stubInfo_(stubInfo) {}

  ICStub* next() const { return next_; }
  void setNext(ICStub* stub) { next_ = stub; }

  const CacheIRStubInfo* stubInfo() const { return stubInfo_; }
  uint8_t* stubDataStart();

  void trace(JSTracer* trc);

  // Optimized stubs get purged on GC.  But some stubs can be active on the
  // stack during GC - specifically the ones that can make calls.  To ensure
  // that these do not get purged, all stubs that can make calls are allocated
  // in the fallback stub space.
  bool makesGCCalls() const;
  bool allocatedInFallbackSpace() const { return makesGCCalls(); }

  static constexpr size_t offsetOfNext() {
    return offsetof(ICCacheIRStub, next_);
  }
};

// Assert stub size is what we expect to catch regressions.
#ifdef JS_64BIT
static_assert(sizeof(ICFallbackStub) == 4 * sizeof(uintptr_t));
static_assert(sizeof(ICCacheIRStub) == 4 * sizeof(uintptr_t));
#else
static_assert(sizeof(ICFallbackStub) == 5 * sizeof(uintptr_t));
static_assert(sizeof(ICCacheIRStub) == 6 * sizeof(uintptr_t));
#endif

inline ICStub* ICStub::maybeNext() const {
  return isFallback() ? nullptr : toCacheIRStub()->next();
}

inline void ICFallbackStub::addNewStub(ICCacheIRStub* stub) {
  MOZ_ASSERT(stub->next() == nullptr);
  stub->setNext(icEntry_->firstStub());
  icEntry_->setFirstStub(stub);
  state_.trackAttached();
}

inline ICFallbackStub* ICStub::getChainFallback() {
  ICStub* lastStub = this;
  while (!lastStub->isFallback()) {
    lastStub = lastStub->toCacheIRStub()->next();
  }
  return lastStub->toFallbackStub();
}

AllocatableGeneralRegisterSet BaselineICAvailableGeneralRegs(size_t numInputs);

struct IonOsrTempData;

extern bool DoCallFallback(JSContext* cx, BaselineFrame* frame,
                           ICFallbackStub* stub, uint32_t argc, Value* vp,
                           MutableHandleValue res);

extern bool DoSpreadCallFallback(JSContext* cx, BaselineFrame* frame,
                                 ICFallbackStub* stub, Value* vp,
                                 MutableHandleValue res);

extern bool DoToBoolFallback(JSContext* cx, BaselineFrame* frame,
                             ICFallbackStub* stub, HandleValue arg,
                             MutableHandleValue ret);

extern bool DoGetElemSuperFallback(JSContext* cx, BaselineFrame* frame,
                                   ICFallbackStub* stub, HandleValue lhs,
                                   HandleValue rhs, HandleValue receiver,
                                   MutableHandleValue res);

extern bool DoGetElemFallback(JSContext* cx, BaselineFrame* frame,
                              ICFallbackStub* stub, HandleValue lhs,
                              HandleValue rhs, MutableHandleValue res);

extern bool DoSetElemFallback(JSContext* cx, BaselineFrame* frame,
                              ICFallbackStub* stub, Value* stack,
                              HandleValue objv, HandleValue index,
                              HandleValue rhs);

extern bool DoInFallback(JSContext* cx, BaselineFrame* frame,
                         ICFallbackStub* stub, HandleValue key,
                         HandleValue objValue, MutableHandleValue res);

extern bool DoHasOwnFallback(JSContext* cx, BaselineFrame* frame,
                             ICFallbackStub* stub, HandleValue keyValue,
                             HandleValue objValue, MutableHandleValue res);

extern bool DoCheckPrivateFieldFallback(JSContext* cx, BaselineFrame* frame,
                                        ICFallbackStub* stub,
                                        HandleValue objValue,
                                        HandleValue keyValue,
                                        MutableHandleValue res);

extern bool DoGetNameFallback(JSContext* cx, BaselineFrame* frame,
                              ICFallbackStub* stub, HandleObject envChain,
                              MutableHandleValue res);

extern bool DoBindNameFallback(JSContext* cx, BaselineFrame* frame,
                               ICFallbackStub* stub, HandleObject envChain,
                               MutableHandleValue res);

extern bool DoGetIntrinsicFallback(JSContext* cx, BaselineFrame* frame,
                                   ICFallbackStub* stub,
                                   MutableHandleValue res);

extern bool DoGetPropFallback(JSContext* cx, BaselineFrame* frame,
                              ICFallbackStub* stub, MutableHandleValue val,
                              MutableHandleValue res);

extern bool DoGetPropSuperFallback(JSContext* cx, BaselineFrame* frame,
                                   ICFallbackStub* stub, HandleValue receiver,
                                   MutableHandleValue val,
                                   MutableHandleValue res);

extern bool DoSetPropFallback(JSContext* cx, BaselineFrame* frame,
                              ICFallbackStub* stub, Value* stack,
                              HandleValue lhs, HandleValue rhs);

extern bool DoGetIteratorFallback(JSContext* cx, BaselineFrame* frame,
                                  ICFallbackStub* stub, HandleValue value,
                                  MutableHandleValue res);

extern bool DoOptimizeSpreadCallFallback(JSContext* cx, BaselineFrame* frame,
                                         ICFallbackStub* stub,
                                         HandleValue value,
                                         MutableHandleValue res);

extern bool DoInstanceOfFallback(JSContext* cx, BaselineFrame* frame,
                                 ICFallbackStub* stub, HandleValue lhs,
                                 HandleValue rhs, MutableHandleValue res);

extern bool DoTypeOfFallback(JSContext* cx, BaselineFrame* frame,
                             ICFallbackStub* stub, HandleValue val,
                             MutableHandleValue res);

extern bool DoToPropertyKeyFallback(JSContext* cx, BaselineFrame* frame,
                                    ICFallbackStub* stub, HandleValue val,
                                    MutableHandleValue res);

extern bool DoRestFallback(JSContext* cx, BaselineFrame* frame,
                           ICFallbackStub* stub, MutableHandleValue res);

extern bool DoUnaryArithFallback(JSContext* cx, BaselineFrame* frame,
                                 ICFallbackStub* stub, HandleValue val,
                                 MutableHandleValue res);

extern bool DoBinaryArithFallback(JSContext* cx, BaselineFrame* frame,
                                  ICFallbackStub* stub, HandleValue lhs,
                                  HandleValue rhs, MutableHandleValue ret);

extern bool DoNewArrayFallback(JSContext* cx, BaselineFrame* frame,
                               ICFallbackStub* stub, MutableHandleValue res);

extern bool DoNewObjectFallback(JSContext* cx, BaselineFrame* frame,
                                ICFallbackStub* stub, MutableHandleValue res);

extern bool DoCompareFallback(JSContext* cx, BaselineFrame* frame,
                              ICFallbackStub* stub, HandleValue lhs,
                              HandleValue rhs, MutableHandleValue ret);

}  // namespace jit
}  // namespace js

#endif /* jit_BaselineIC_h */
