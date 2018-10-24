/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
#include "vm/UnboxedObject.h"

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
// baseline compiler would have otherwise had to perform a type Monitor operation
// (e.g. the result of GetProp, GetElem, etc.), or locations where the baseline
// compiler would have had to modify a heap typeset using the type of an input
// value (e.g. SetProp, SetElem, etc.)
//
// There are two kinds of Type ICs: Monitor and Update.
//
// Note that type stub bodies are no-ops.  The stubs only exist for their
// guards, and their existence simply signifies that the typeset (implicit)
// that is being checked already contains that type.
//
// TypeMonitor ICs
// ---------------
// Monitor ICs are shared between stubs in the general IC, and monitor the resulting
// types of getter operations (call returns, getprop outputs, etc.)
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
// After an optimized IC stub successfully executes, it passes control to the type stub
// chain to check the resulting type.  If no type stub succeeds, and the monitor fallback
// stub is reached, the monitor fallback stub performs a manual monitor, and also adds the
// appropriate type stub to the chain.
//
// The IC's main fallback, in addition to generating new mainline stubs, also generates
// type stubs as reflected by its returned value.
//
// NOTE: The type IC chain returns directly to the mainline code, not back to the
// stub it was entered from.  Thus, entering a type IC is a matter of a |jump|, not
// a |call|.  This allows us to safely call a VM Monitor function from within the monitor IC's
// fallback chain, since the return address (needed for stack inspection) is preserved.
//
//
// TypeUpdate ICs
// --------------
// Update ICs update heap typesets and monitor the input types of setter operations
// (setelem, setprop inputs, etc.).  Unlike monitor ICs, they are not shared
// between stubs on an IC, but instead are kept track of on a per-stub basis.
//
// This is because the main stubs for the operation will each identify a potentially
// different ObjectGroup to update.  New input types must be tracked on a group-to-
// group basis.
//
// Type-update ICs cannot be called in tail position (they must return to the
// the stub that called them so that the stub may continue to perform its original
// purpose).  This means that any VMCall to perform a manual type update from C++ must be
// done from within the main IC stub.  This necessitates that the stub enter a
// "BaselineStub" frame before making the call.
//
// If the type-update IC chain could itself make the VMCall, then the BaselineStub frame
// must be entered before calling the type-update chain, and exited afterward.  This
// is very expensive for a common case where we expect the type-update fallback to not
// be called.  To avoid the cost of entering and exiting a BaselineStub frame when
// using the type-update IC chain, we design the chain to not perform any VM-calls
// in its fallback.
//
// Instead, the type-update IC chain is responsible for returning 1 or 0, depending
// on if a type is represented in the chain or not.  The fallback stub simply returns
// 0, and all other optimized stubs return 1.
// If the chain returns 1, then the IC stub goes ahead and performs its operation.
// If the chain returns 0, then the IC stub performs a call to the fallback function
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
void TypeFallbackICSpew(JSContext* cx, ICTypeMonitor_Fallback* stub, const char* fmt, ...)
    MOZ_FORMAT_PRINTF(3, 4);
#else
#define FallbackICSpew(...)
#define TypeFallbackICSpew(...)
#endif

// An entry in the BaselineScript IC descriptor table. There's one ICEntry per
// IC.
class ICEntry
{
    // A pointer to the first IC stub for this instruction.
    ICStub* firstStub_;

    // The PC of this IC's bytecode op within the JSScript.
    uint32_t pcOffset_ : 31;
    uint32_t isForOp_ : 1;

  public:
    ICEntry(ICStub* firstStub, uint32_t pcOffset, bool isForOp)
      : firstStub_(firstStub), pcOffset_(pcOffset), isForOp_(uint32_t(isForOp))
    {
        // The offset must fit in at least 31 bits, since we shave off 1 for
        // the isForOp_ flag.
        MOZ_ASSERT(pcOffset_ == pcOffset);
        JS_STATIC_ASSERT(BaselineMaxScriptLength <= (1u << 31) - 1);
        MOZ_ASSERT(pcOffset <= BaselineMaxScriptLength);
    }

    ICStub* firstStub() const {
        MOZ_ASSERT(firstStub_);
        return firstStub_;
    }

    ICFallbackStub* fallbackStub() const;

    void setFirstStub(ICStub* stub) {
        firstStub_ = stub;
    }

    uint32_t pcOffset() const {
        return pcOffset_;
    }
    jsbytecode* pc(JSScript* script) const {
        return script->offsetToPC(pcOffset_);
    }

    static inline size_t offsetOfFirstStub() {
        return offsetof(ICEntry, firstStub_);
    }

    inline ICStub** addressOfFirstStub() {
        return &firstStub_;
    }

    bool isForOp() const {
        return !!isForOp_;
    }

    void trace(JSTracer* trc);
};

class ICMonitoredStub;
class ICMonitoredFallbackStub;
class ICUpdatedStub;

// Constant iterator that traverses arbitrary chains of ICStubs.
// No requirements are made of the ICStub used to construct this
// iterator, aside from that the stub be part of a nullptr-terminated
// chain.
// The iterator is considered to be at its end once it has been
// incremented _past_ the last stub.  Thus, if 'atEnd()' returns
// true, the '*' and '->' operations are not valid.
class ICStubConstIterator
{
    friend class ICStub;
    friend class ICFallbackStub;

  private:
    ICStub* currentStub_;

  public:
    explicit ICStubConstIterator(ICStub* currentStub) : currentStub_(currentStub) {}

    static ICStubConstIterator StartingAt(ICStub* stub) {
        return ICStubConstIterator(stub);
    }
    static ICStubConstIterator End(ICStub* stub) {
        return ICStubConstIterator(nullptr);
    }

    bool operator ==(const ICStubConstIterator& other) const {
        return currentStub_ == other.currentStub_;
    }
    bool operator !=(const ICStubConstIterator& other) const {
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

    ICStub* operator ->() const {
        MOZ_ASSERT(currentStub_);
        return currentStub_;
    }

    bool atEnd() const {
        return currentStub_ == nullptr;
    }
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
class ICStubIterator
{
    friend class ICFallbackStub;

  private:
    ICEntry* icEntry_;
    ICFallbackStub* fallbackStub_;
    ICStub* previousStub_;
    ICStub* currentStub_;
    bool unlinked_;

    explicit ICStubIterator(ICFallbackStub* fallbackStub, bool end=false);
  public:

    bool operator ==(const ICStubIterator& other) const {
        // == should only ever be called on stubs from the same chain.
        MOZ_ASSERT(icEntry_ == other.icEntry_);
        MOZ_ASSERT(fallbackStub_ == other.fallbackStub_);
        return currentStub_ == other.currentStub_;
    }
    bool operator !=(const ICStubIterator& other) const {
        return !(*this == other);
    }

    ICStubIterator& operator++();

    ICStubIterator operator++(int) {
        ICStubIterator oldThis(*this);
        ++(*this);
        return oldThis;
    }

    ICStub* operator*() const {
        return currentStub_;
    }

    ICStub* operator ->() const {
        return currentStub_;
    }

    bool atEnd() const {
        return currentStub_ == (ICStub*) fallbackStub_;
    }

    void unlink(JSContext* cx);
};

//
// Base class for all IC stubs.
//
class ICStub
{
    friend class ICFallbackStub;

  public:
    enum Kind {
        INVALID = 0,
#define DEF_ENUM_KIND(kindName) kindName,
        IC_BASELINE_STUB_KIND_LIST(DEF_ENUM_KIND)
#undef DEF_ENUM_KIND
        LIMIT
    };

    static bool IsValidKind(Kind k) {
        return (k > INVALID) && (k < LIMIT);
    }
    static bool IsCacheIRKind(Kind k) {
        return k == CacheIR_Regular || k == CacheIR_Monitored || k == CacheIR_Updated;
    }

    static const char* KindString(Kind k) {
        switch(k) {
#define DEF_KIND_STR(kindName) case kindName: return #kindName;
            IC_BASELINE_STUB_KIND_LIST(DEF_KIND_STR)
#undef DEF_KIND_STR
          default:
            MOZ_CRASH("Invalid kind.");
        }
    }

    enum Trait {
        Regular             = 0x0,
        Fallback            = 0x1,
        Monitored           = 0x2,
        MonitoredFallback   = 0x3,
        Updated             = 0x4
    };

    void traceCode(JSTracer* trc, const char* name);
    void updateCode(JitCode* stubCode);
    void trace(JSTracer* trc);

    template <typename T, typename... Args>
    static T* New(JSContext* cx, ICStubSpace* space, JitCode* code, Args&&... args) {
        if (!code) {
            return nullptr;
        }
        T* result = space->allocate<T>(code, std::forward<Args>(args)...);
        if (!result) {
            ReportOutOfMemory(cx);
        }
        return result;
    }

  protected:
    // The raw jitcode to call for this stub.
    uint8_t* stubCode_;

    // Pointer to next IC stub.  This is null for the last IC stub, which should
    // either be a fallback or inert IC stub.
    ICStub* next_;

    // A 16-bit field usable by subtypes of ICStub for subtype-specific small-info
    uint16_t extra_;

    // The kind of the stub.
    //  High bit is 'isFallback' flag.
    //  Second high bit is 'isMonitored' flag.
    Trait trait_ : 3;
    Kind kind_ : 13;

    inline ICStub(Kind kind, JitCode* stubCode)
      : stubCode_(stubCode->raw()),
        next_(nullptr),
        extra_(0),
        trait_(Regular),
        kind_(kind)
    {
        MOZ_ASSERT(stubCode != nullptr);
    }

    inline ICStub(Kind kind, Trait trait, JitCode* stubCode)
      : stubCode_(stubCode->raw()),
        next_(nullptr),
        extra_(0),
        trait_(trait),
        kind_(kind)
    {
        MOZ_ASSERT(stubCode != nullptr);
    }

    inline Trait trait() const {
        // Workaround for MSVC reading trait_ as signed value.
        return (Trait)(trait_ & 0x7);
    }

  public:

    inline Kind kind() const {
        return static_cast<Kind>(kind_);
    }

    inline bool isFallback() const {
        return trait() == Fallback || trait() == MonitoredFallback;
    }

    inline bool isMonitored() const {
        return trait() == Monitored;
    }

    inline bool isUpdated() const {
        return trait() == Updated;
    }

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

    inline const ICUpdatedStub* toUpdatedStub() const {
        MOZ_ASSERT(isUpdated());
        return reinterpret_cast<const ICUpdatedStub*>(this);
    }

    inline ICUpdatedStub* toUpdatedStub() {
        MOZ_ASSERT(isUpdated());
        return reinterpret_cast<ICUpdatedStub*>(this);
    }

#define KIND_METHODS(kindName)   \
    inline bool is##kindName() const { return kind() == kindName; } \
    inline const IC##kindName* to##kindName() const { \
        MOZ_ASSERT(is##kindName()); \
        return reinterpret_cast<const IC##kindName*>(this); \
    } \
    inline IC##kindName* to##kindName() { \
        MOZ_ASSERT(is##kindName()); \
        return reinterpret_cast<IC##kindName*>(this); \
    }
    IC_BASELINE_STUB_KIND_LIST(KIND_METHODS)
#undef KIND_METHODS

    inline ICStub* next() const {
        return next_;
    }

    inline bool hasNext() const {
        return next_ != nullptr;
    }

    inline void setNext(ICStub* stub) {
        // Note: next_ only needs to be changed under the compilation lock for
        // non-type-monitor/update ICs.
        next_ = stub;
    }

    inline ICStub** addressOfNext() {
        return &next_;
    }

    inline JitCode* jitCode() {
        return JitCode::FromExecutable(stubCode_);
    }

    inline uint8_t* rawStubCode() const {
        return stubCode_;
    }

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

    static inline size_t offsetOfNext() {
        return offsetof(ICStub, next_);
    }

    static inline size_t offsetOfStubCode() {
        return offsetof(ICStub, stubCode_);
    }

    static inline size_t offsetOfExtra() {
        return offsetof(ICStub, extra_);
    }

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

class ICFallbackStub : public ICStub
{
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

    ICFallbackStub(Kind kind, JitCode* stubCode)
      : ICStub(kind, ICStub::Fallback, stubCode),
        icEntry_(nullptr),
        state_(),
        enteredCount_(0),
        lastStubPtrAddr_(nullptr) {}

    ICFallbackStub(Kind kind, Trait trait, JitCode* stubCode)
      : ICStub(kind, trait, stubCode),
        icEntry_(nullptr),
        state_(),
        enteredCount_(0),
        lastStubPtrAddr_(nullptr)
    {
        MOZ_ASSERT(trait == ICStub::Fallback ||
                   trait == ICStub::MonitoredFallback);
    }

  public:
    inline ICEntry* icEntry() const {
        return icEntry_;
    }

    inline size_t numOptimizedStubs() const {
        return state_.numOptimizedStubs();
    }

    void setInvalid() {
        state_.setInvalid();
    }

    bool invalid() const {
        return state_.invalid();
    }

    ICState& state() {
        return state_;
    }

    // The icEntry and lastStubPtrAddr_ fields can't be initialized when the stub is
    // created since the stub is created at compile time, and we won't know the IC entry
    // address until after compile when the JitScript is created.  This method
    // allows these fields to be fixed up at that point.
    void fixupICEntry(ICEntry* icEntry) {
        MOZ_ASSERT(icEntry_ == nullptr);
        MOZ_ASSERT(lastStubPtrAddr_ == nullptr);
        icEntry_ = icEntry;
        lastStubPtrAddr_ = icEntry_->addressOfFirstStub();
    }

    // Add a new stub to the IC chain terminated by this fallback stub.
    void addNewStub(ICStub* stub) {
        MOZ_ASSERT(!invalid());
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

    ICStubIterator beginChain() {
        return ICStubIterator(this);
    }

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

    // Return the number of times this stub has successfully provided a value to the
    // caller.
    uint32_t enteredCount() const { return enteredCount_; }
    inline void incrementEnteredCount() { enteredCount_++; }
};

// Base class for Trait::Regular CacheIR stubs
class ICCacheIR_Regular : public ICStub
{
    const CacheIRStubInfo* stubInfo_;

    // Counts the number of times the stub was entered
    //
    // See Bug 1494473 comment 6 for a mechanism to handle overflow if overflow
    // becomes a concern.
    uint32_t enteredCount_;

  public:
    ICCacheIR_Regular(JitCode* stubCode, const CacheIRStubInfo* stubInfo)
      : ICStub(ICStub::CacheIR_Regular, stubCode),
        stubInfo_(stubInfo),
        enteredCount_(0)
    {}

    static ICCacheIR_Regular* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                    ICCacheIR_Regular& other);

    void notePreliminaryObject() {
        extra_ = 1;
    }
    bool hasPreliminaryObject() const {
        return extra_;
    }

    const CacheIRStubInfo* stubInfo() const {
        return stubInfo_;
    }

    uint8_t* stubDataStart();

    // Return the number of times this stub has successfully provided a value to the
    // caller.
    uint32_t enteredCount() const { return enteredCount_; }
    static size_t offsetOfEnteredCount() { return offsetof(ICCacheIR_Regular, enteredCount_); }
};

// Monitored stubs are IC stubs that feed a single resulting value out to a
// type monitor operation.
class ICMonitoredStub : public ICStub
{
  protected:
    // Pointer to the start of the type monitoring stub chain.
    ICStub* firstMonitorStub_;

    ICMonitoredStub(Kind kind, JitCode* stubCode, ICStub* firstMonitorStub);

  public:
    inline void updateFirstMonitorStub(ICStub* monitorStub) {
        // This should only be called once: when the first optimized monitor stub
        // is added to the type monitor IC chain.
        MOZ_ASSERT(firstMonitorStub_ && firstMonitorStub_->isTypeMonitor_Fallback());
        firstMonitorStub_ = monitorStub;
    }
    inline void resetFirstMonitorStub(ICStub* monitorFallback) {
        MOZ_ASSERT(monitorFallback->isTypeMonitor_Fallback());
        firstMonitorStub_ = monitorFallback;
    }
    inline ICStub* firstMonitorStub() const {
        return firstMonitorStub_;
    }

    static inline size_t offsetOfFirstMonitorStub() {
        return offsetof(ICMonitoredStub, firstMonitorStub_);
    }
};

class ICCacheIR_Monitored : public ICMonitoredStub
{
    const CacheIRStubInfo* stubInfo_;

  public:
    ICCacheIR_Monitored(JitCode* stubCode, ICStub* firstMonitorStub,
                        const CacheIRStubInfo* stubInfo)
      : ICMonitoredStub(ICStub::CacheIR_Monitored, stubCode, firstMonitorStub),
        stubInfo_(stubInfo)
    {}

    static ICCacheIR_Monitored* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                      ICCacheIR_Monitored& other);

    void notePreliminaryObject() {
        extra_ = 1;
    }
    bool hasPreliminaryObject() const {
        return extra_;
    }

    const CacheIRStubInfo* stubInfo() const {
        return stubInfo_;
    }

    uint8_t* stubDataStart();
};

// Updated stubs are IC stubs that use a TypeUpdate IC to track
// the status of heap typesets that need to be updated.
class ICUpdatedStub : public ICStub
{
  protected:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;
    uint32_t numOptimizedStubs_;

    // Pointer to the start of the type updating stub chain.
    ICStub* firstUpdateStub_;

    ICUpdatedStub(Kind kind, JitCode* stubCode)
      : ICStub(kind, ICStub::Updated, stubCode),
        numOptimizedStubs_(0),
        firstUpdateStub_(nullptr)
    {}

  public:
    MOZ_MUST_USE bool initUpdatingChain(JSContext* cx, ICStubSpace* space);

    MOZ_MUST_USE bool addUpdateStubForValue(JSContext* cx, HandleScript script, HandleObject obj,
                                            HandleObjectGroup group, HandleId id, HandleValue val);

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

    inline ICStub* firstUpdateStub() const {
        return firstUpdateStub_;
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

    inline uint32_t numOptimizedStubs() const {
        return numOptimizedStubs_;
    }

    static inline size_t offsetOfFirstUpdateStub() {
        return offsetof(ICUpdatedStub, firstUpdateStub_);
    }
};

class ICCacheIR_Updated : public ICUpdatedStub
{
    const CacheIRStubInfo* stubInfo_;
    GCPtrObjectGroup updateStubGroup_;
    GCPtrId updateStubId_;

  public:
    ICCacheIR_Updated(JitCode* stubCode, const CacheIRStubInfo* stubInfo)
      : ICUpdatedStub(ICStub::CacheIR_Updated, stubCode),
        stubInfo_(stubInfo),
        updateStubGroup_(nullptr),
        updateStubId_(JSID_EMPTY)
    {}

    static ICCacheIR_Updated* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                    ICCacheIR_Updated& other);

    GCPtrObjectGroup& updateStubGroup() {
        return updateStubGroup_;
    }
    GCPtrId& updateStubId() {
        return updateStubId_;
    }

    void notePreliminaryObject() {
        extra_ = 1;
    }
    bool hasPreliminaryObject() const {
        return extra_;
    }

    const CacheIRStubInfo* stubInfo() const {
        return stubInfo_;
    }

    uint8_t* stubDataStart();
};

// Base class for stubcode compilers.
class ICStubCompiler
{
    // Prevent GC in the middle of stub compilation.
    js::gc::AutoSuppressGC suppressGC;

  protected:
    JSContext* cx;
    ICStub::Kind kind;
    bool inStubFrame_;

#ifdef DEBUG
    bool entersStubFrame_;
    uint32_t framePushedAtEnterStubFrame_;
#endif

    // By default the stubcode key is just the kind.
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind);
    }

    virtual MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) = 0;
    virtual void postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> genCode) {}

    JitCode* getStubCode();

    ICStubCompiler(JSContext* cx, ICStub::Kind kind)
      : suppressGC(cx), cx(cx), kind(kind), inStubFrame_(false)
#ifdef DEBUG
      , entersStubFrame_(false), framePushedAtEnterStubFrame_(0)
#endif
    {}

    // Push a payload specialized per compiler needed to execute stubs.
    void PushStubPayload(MacroAssembler& masm, Register scratch);
    void pushStubPayload(MacroAssembler& masm, Register scratch);

    // Emits a tail call to a VMFunction wrapper.
    MOZ_MUST_USE bool tailCallVM(const VMFunction& fun, MacroAssembler& masm);

    // Emits a normal (non-tail) call to a VMFunction wrapper.
    MOZ_MUST_USE bool callVM(const VMFunction& fun, MacroAssembler& masm);

    // A stub frame is used when a stub wants to call into the VM without
    // performing a tail call. This is required for the return address
    // to pc mapping to work.
    void enterStubFrame(MacroAssembler& masm, Register scratch);
    void assumeStubFrame();
    void leaveStubFrame(MacroAssembler& masm, bool calledIntoIon = false);

  public:
    static inline AllocatableGeneralRegisterSet availableGeneralRegs(size_t numInputs) {
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

  protected:
    template <typename T, typename... Args>
    T* newStub(Args&&... args) {
        return ICStub::New<T>(cx, std::forward<Args>(args)...);
    }

  public:
    virtual ICStub* getStub(ICStubSpace* space) = 0;

    static ICStubSpace* StubSpaceForStub(bool makesGCCalls, JSScript* outerScript) {
        if (makesGCCalls) {
            return outerScript->baselineScript()->fallbackStubSpace();
        }
        return outerScript->zone()->jitZone()->optimizedStubSpace();
    }
    ICStubSpace* getStubSpace(JSScript* outerScript) {
        return StubSpaceForStub(ICStub::NonCacheIRStubMakesGCCalls(kind), outerScript);
    }
};

// WarmUpCounter_Fallback

// A WarmUpCounter IC chain has only the fallback stub.
class ICWarmUpCounter_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICWarmUpCounter_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::WarmUpCounter_Fallback, stubCode)
    { }

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::WarmUpCounter_Fallback)
        { }

        ICWarmUpCounter_Fallback* getStub(ICStubSpace* space) override {
            return newStub<ICWarmUpCounter_Fallback>(space, getStubCode());
        }
    };
};


// Monitored fallback stubs - as the name implies.
class ICMonitoredFallbackStub : public ICFallbackStub
{
  protected:
    // Pointer to the fallback monitor stub. Created lazily by
    // getFallbackMonitorStub if needed.
    ICTypeMonitor_Fallback* fallbackMonitorStub_;

    ICMonitoredFallbackStub(Kind kind, JitCode* stubCode)
      : ICFallbackStub(kind, ICStub::MonitoredFallback, stubCode),
        fallbackMonitorStub_(nullptr) {}

  public:
    MOZ_MUST_USE bool initMonitoringChain(JSContext* cx, JSScript* script);
    MOZ_MUST_USE bool addMonitorStubForValue(JSContext* cx, BaselineFrame* frame,
                                             StackTypeSet* types, HandleValue val);

    ICTypeMonitor_Fallback* maybeFallbackMonitorStub() const {
        return fallbackMonitorStub_;
    }
    ICTypeMonitor_Fallback* getFallbackMonitorStub(JSContext* cx, JSScript* script) {
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

class TypeCheckPrimitiveSetStub : public ICStub
{
    friend class ICStubSpace;
  protected:
    inline static uint16_t TypeToFlag(JSValueType type) {
        return 1u << static_cast<unsigned>(type);
    }

    inline static uint16_t ValidFlags() {
        return ((TypeToFlag(JSVAL_TYPE_OBJECT) << 1) - 1) & ~TypeToFlag(JSVAL_TYPE_MAGIC);
    }

    TypeCheckPrimitiveSetStub(Kind kind, JitCode* stubCode, uint16_t flags)
        : ICStub(kind, stubCode)
    {
        MOZ_ASSERT(kind == TypeMonitor_PrimitiveSet || kind == TypeUpdate_PrimitiveSet);
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
    uint16_t typeFlags() const {
        return extra_;
    }

    bool containsType(JSValueType type) const {
        MOZ_ASSERT(type <= JSVAL_TYPE_OBJECT);
        MOZ_ASSERT(type != JSVAL_TYPE_MAGIC);
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
            return static_cast<int32_t>(kind) |
                  (static_cast<int32_t>(flags_) << 16);
        }

      public:
        Compiler(JSContext* cx, Kind kind, TypeCheckPrimitiveSetStub* existingStub,
                 JSValueType type)
          : ICStubCompiler(cx, kind),
            existingStub_(existingStub),
            flags_((existingStub ? existingStub->typeFlags() : 0) | TypeToFlag(type))
        {
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
class ICTypeMonitor_Fallback : public ICStub
{
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

    uint32_t invalid_ : 1;

    // Whether this has a fallback stub referring to the IC entry.
    bool hasFallbackStub_ : 1;

    // Index of 'this' or argument which is being monitored, or BYTECODE_INDEX
    // if this is monitoring the types of values pushed at some bytecode.
    uint32_t argumentIndex_ : 23;

    static const uint32_t BYTECODE_INDEX = (1 << 23) - 1;

    ICTypeMonitor_Fallback(JitCode* stubCode, ICMonitoredFallbackStub* mainFallbackStub,
                           uint32_t argumentIndex)
      : ICStub(ICStub::TypeMonitor_Fallback, stubCode),
        mainFallbackStub_(mainFallbackStub),
        firstMonitorStub_(thisFromCtor()),
        lastMonitorStubPtrAddr_(nullptr),
        numOptimizedMonitorStubs_(0),
        invalid_(false),
        hasFallbackStub_(mainFallbackStub != nullptr),
        argumentIndex_(argumentIndex)
    { }

    ICTypeMonitor_Fallback* thisFromCtor() {
        return this;
    }

    void addOptimizedMonitorStub(ICStub* stub) {
        MOZ_ASSERT(!invalid());
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

    inline ICStub* firstMonitorStub() const {
        return firstMonitorStub_;
    }

    static inline size_t offsetOfFirstMonitorStub() {
        return offsetof(ICTypeMonitor_Fallback, firstMonitorStub_);
    }

    inline uint32_t numOptimizedMonitorStubs() const {
        return numOptimizedMonitorStubs_;
    }

    void setInvalid() {
        invalid_ = 1;
    }

    bool invalid() const {
        return invalid_;
    }

    inline bool monitorsThis() const {
        return argumentIndex_ == 0;
    }

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
                                             StackTypeSet* types, HandleValue val);

    void resetMonitorStubChain(Zone* zone);

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
        ICMonitoredFallbackStub* mainFallbackStub_;
        uint32_t argumentIndex_;

      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ICMonitoredFallbackStub* mainFallbackStub)
          : ICStubCompiler(cx, ICStub::TypeMonitor_Fallback),
            mainFallbackStub_(mainFallbackStub),
            argumentIndex_(BYTECODE_INDEX)
        { }

        Compiler(JSContext* cx, uint32_t argumentIndex)
          : ICStubCompiler(cx, ICStub::TypeMonitor_Fallback),
            mainFallbackStub_(nullptr),
            argumentIndex_(argumentIndex)
        { }

        ICTypeMonitor_Fallback* getStub(ICStubSpace* space) override {
            return newStub<ICTypeMonitor_Fallback>(space, getStubCode(), mainFallbackStub_,
                                                       argumentIndex_);
        }
    };
};

class ICTypeMonitor_PrimitiveSet : public TypeCheckPrimitiveSetStub
{
    friend class ICStubSpace;

    ICTypeMonitor_PrimitiveSet(JitCode* stubCode, uint16_t flags)
        : TypeCheckPrimitiveSetStub(TypeMonitor_PrimitiveSet, stubCode, flags)
    {}

  public:
    class Compiler : public TypeCheckPrimitiveSetStub::Compiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ICTypeMonitor_PrimitiveSet* existingStub,
                 JSValueType type)
          : TypeCheckPrimitiveSetStub::Compiler(cx, TypeMonitor_PrimitiveSet, existingStub,
                                                type)
        {}

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

class ICTypeMonitor_SingleObject : public ICStub
{
    friend class ICStubSpace;

    GCPtrObject obj_;

    ICTypeMonitor_SingleObject(JitCode* stubCode, JSObject* obj);

  public:
    GCPtrObject& object() {
        return obj_;
    }

    static size_t offsetOfObject() {
        return offsetof(ICTypeMonitor_SingleObject, obj_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObject obj_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, HandleObject obj)
          : ICStubCompiler(cx, TypeMonitor_SingleObject),
            obj_(obj)
        { }

        ICTypeMonitor_SingleObject* getStub(ICStubSpace* space) override {
            return newStub<ICTypeMonitor_SingleObject>(space, getStubCode(), obj_);
        }
    };
};

class ICTypeMonitor_ObjectGroup : public ICStub
{
    friend class ICStubSpace;

    GCPtrObjectGroup group_;

    ICTypeMonitor_ObjectGroup(JitCode* stubCode, ObjectGroup* group);

  public:
    GCPtrObjectGroup& group() {
        return group_;
    }

    static size_t offsetOfGroup() {
        return offsetof(ICTypeMonitor_ObjectGroup, group_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObjectGroup group_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, HandleObjectGroup group)
          : ICStubCompiler(cx, TypeMonitor_ObjectGroup),
            group_(group)
        { }

        ICTypeMonitor_ObjectGroup* getStub(ICStubSpace* space) override {
            return newStub<ICTypeMonitor_ObjectGroup>(space, getStubCode(), group_);
        }
    };
};

class ICTypeMonitor_AnyValue : public ICStub
{
    friend class ICStubSpace;

    explicit ICTypeMonitor_AnyValue(JitCode* stubCode)
      : ICStub(TypeMonitor_AnyValue, stubCode)
    {}

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, TypeMonitor_AnyValue)
        { }

        ICTypeMonitor_AnyValue* getStub(ICStubSpace* space) override {
            return newStub<ICTypeMonitor_AnyValue>(space, getStubCode());
        }
    };
};

// TypeUpdate

extern const VMFunction DoTypeUpdateFallbackInfo;

// The TypeUpdate fallback is not a regular fallback, since it just
// forwards to a different entry point in the main fallback stub.
class ICTypeUpdate_Fallback : public ICStub
{
    friend class ICStubSpace;

    explicit ICTypeUpdate_Fallback(JitCode* stubCode)
      : ICStub(ICStub::TypeUpdate_Fallback, stubCode)
    {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::TypeUpdate_Fallback)
        { }

        ICTypeUpdate_Fallback* getStub(ICStubSpace* space) override {
            return newStub<ICTypeUpdate_Fallback>(space, getStubCode());
        }
    };
};

class ICTypeUpdate_PrimitiveSet : public TypeCheckPrimitiveSetStub
{
    friend class ICStubSpace;

    ICTypeUpdate_PrimitiveSet(JitCode* stubCode, uint16_t flags)
        : TypeCheckPrimitiveSetStub(TypeUpdate_PrimitiveSet, stubCode, flags)
    {}

  public:
    class Compiler : public TypeCheckPrimitiveSetStub::Compiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ICTypeUpdate_PrimitiveSet* existingStub, JSValueType type)
          : TypeCheckPrimitiveSetStub::Compiler(cx, TypeUpdate_PrimitiveSet,
                                                existingStub, type)
        {}

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
class ICTypeUpdate_SingleObject : public ICStub
{
    friend class ICStubSpace;

    GCPtrObject obj_;

    ICTypeUpdate_SingleObject(JitCode* stubCode, JSObject* obj);

  public:
    GCPtrObject& object() {
        return obj_;
    }

    static size_t offsetOfObject() {
        return offsetof(ICTypeUpdate_SingleObject, obj_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObject obj_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, HandleObject obj)
          : ICStubCompiler(cx, TypeUpdate_SingleObject),
            obj_(obj)
        { }

        ICTypeUpdate_SingleObject* getStub(ICStubSpace* space) override {
            return newStub<ICTypeUpdate_SingleObject>(space, getStubCode(), obj_);
        }
    };
};

// Type update stub to handle a single ObjectGroup.
class ICTypeUpdate_ObjectGroup : public ICStub
{
    friend class ICStubSpace;

    GCPtrObjectGroup group_;

    ICTypeUpdate_ObjectGroup(JitCode* stubCode, ObjectGroup* group);

  public:
    GCPtrObjectGroup& group() {
        return group_;
    }

    static size_t offsetOfGroup() {
        return offsetof(ICTypeUpdate_ObjectGroup, group_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObjectGroup group_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, HandleObjectGroup group)
          : ICStubCompiler(cx, TypeUpdate_ObjectGroup),
            group_(group)
        { }

        ICTypeUpdate_ObjectGroup* getStub(ICStubSpace* space) override {
            return newStub<ICTypeUpdate_ObjectGroup>(space, getStubCode(), group_);
        }
    };
};

class ICTypeUpdate_AnyValue : public ICStub
{
    friend class ICStubSpace;

    explicit ICTypeUpdate_AnyValue(JitCode* stubCode)
      : ICStub(TypeUpdate_AnyValue, stubCode)
    {}

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, TypeUpdate_AnyValue)
        {}

        ICTypeUpdate_AnyValue* getStub(ICStubSpace* space) override {
            return newStub<ICTypeUpdate_AnyValue>(space, getStubCode());
        }
    };
};

// ToBool
//      JSOP_IFNE

class ICToBool_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICToBool_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::ToBool_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToBool_Fallback) {}

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICToBool_Fallback>(space, getStubCode());
        }
    };
};

// ToNumber
//     JSOP_POS

class ICToNumber_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICToNumber_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::ToNumber_Fallback, stubCode) {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToNumber_Fallback) {}

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICToNumber_Fallback>(space, getStubCode());
        }
    };
};

// GetElem
//      JSOP_GETELEM
//      JSOP_GETELEM_SUPER

class ICGetElem_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    explicit ICGetElem_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::GetElem_Fallback, stubCode)
    { }

    static const uint16_t EXTRA_NEGATIVE_INDEX = 0x1;

  public:
    void noteNegativeIndex() {
        extra_ |= EXTRA_NEGATIVE_INDEX;
    }
    bool hasNegativeIndex() const {
        return extra_ & EXTRA_NEGATIVE_INDEX;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool hasReceiver_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

        virtual int32_t getKey() const override {
            return static_cast<int32_t>(kind) |
                  (static_cast<int32_t>(hasReceiver_) << 16);
        }

      public:
        explicit Compiler(JSContext* cx, bool hasReceiver = false)
          : ICStubCompiler(cx, ICStub::GetElem_Fallback),
            hasReceiver_(hasReceiver)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICGetElem_Fallback>(space, getStubCode());
        }
    };
};

// SetElem
//      JSOP_SETELEM
//      JSOP_INITELEM

class ICSetElem_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICSetElem_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::SetElem_Fallback, stubCode)
    { }

    static const size_t HasDenseAddFlag = 0x1;
    static const size_t HasTypedArrayOOBFlag = 0x2;

  public:
    void noteHasDenseAdd() { extra_ |= HasDenseAddFlag; }
    bool hasDenseAdd() const { return extra_ & HasDenseAddFlag; }

    void noteHasTypedArrayOOB() { extra_ |= HasTypedArrayOOBFlag; }
    bool hasTypedArrayOOB() const { return extra_ & HasTypedArrayOOBFlag; }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::SetElem_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICSetElem_Fallback>(space, getStubCode());
        }
    };
};

// In
//      JSOP_IN
class ICIn_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICIn_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::In_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::In_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICIn_Fallback>(space, getStubCode());
        }
    };
};

// HasOwn
//      JSOP_HASOWN
class ICHasOwn_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICHasOwn_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::HasOwn_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::HasOwn_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICHasOwn_Fallback>(space, getStubCode());
        }
    };
};

// GetName
//      JSOP_GETNAME
//      JSOP_GETGNAME
class ICGetName_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    explicit ICGetName_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::GetName_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::GetName_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICGetName_Fallback>(space, getStubCode());
        }
    };
};

// BindName
//      JSOP_BINDNAME
class ICBindName_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICBindName_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::BindName_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::BindName_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICBindName_Fallback>(space, getStubCode());
        }
    };
};

// GetIntrinsic
//      JSOP_GETINTRINSIC
class ICGetIntrinsic_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    explicit ICGetIntrinsic_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::GetIntrinsic_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::GetIntrinsic_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICGetIntrinsic_Fallback>(space, getStubCode());
        }
    };
};

// GetProp
//     JSOP_GETPROP
//     JSOP_GETPROP_SUPER

class ICGetProp_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    explicit ICGetProp_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::GetProp_Fallback, stubCode)
    { }

  public:
    static const size_t ACCESSED_GETTER_BIT = 1;

    void noteAccessedGetter() {
        extra_ |= (1u << ACCESSED_GETTER_BIT);
    }
    bool hasAccessedGetter() const {
        return extra_ & (1u << ACCESSED_GETTER_BIT);
    }

    class Compiler : public ICStubCompiler {
      protected:
        CodeOffset bailoutReturnOffset_;
        bool hasReceiver_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;
        void postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code) override;

        virtual int32_t getKey() const override {
            return static_cast<int32_t>(kind) |
                  (static_cast<int32_t>(hasReceiver_) << 16);
        }

      public:
        explicit Compiler(JSContext* cx, bool hasReceiver = false)
          : ICStubCompiler(cx, ICStub::GetProp_Fallback),
            hasReceiver_(hasReceiver)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICGetProp_Fallback>(space, getStubCode());
        }
    };
};


// SetProp
//     JSOP_SETPROP
//     JSOP_SETNAME
//     JSOP_SETGNAME
//     JSOP_INITPROP

class ICSetProp_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICSetProp_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::SetProp_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        CodeOffset bailoutReturnOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;
        void postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::SetProp_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICSetProp_Fallback>(space, getStubCode());
        }
    };
};

// Call
//      JSOP_CALL
//      JSOP_CALL_IGNORES_RV
//      JSOP_FUNAPPLY
//      JSOP_FUNCALL
//      JSOP_NEW
//      JSOP_SPREADCALL
//      JSOP_SPREADNEW
//      JSOP_SPREADEVAL

class ICCallStubCompiler : public ICStubCompiler
{
  protected:
    ICCallStubCompiler(JSContext* cx, ICStub::Kind kind)
      : ICStubCompiler(cx, kind)
    { }

    enum FunApplyThing {
        FunApply_MagicArgs,
        FunApply_Array
    };

    void pushCallArguments(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                           Register argcReg, bool isJitCall, bool isConstructing = false);
    void pushSpreadCallArguments(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                                 Register argcReg, bool isJitCall, bool isConstructing);
    void guardSpreadCall(MacroAssembler& masm, Register argcReg, Label* failure,
                         bool isConstructing);
    Register guardFunApply(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                           Register argcReg, FunApplyThing applyThing,
                           Label* failure);
    void pushCallerArguments(MacroAssembler& masm, AllocatableGeneralRegisterSet regs);
    void pushArrayArguments(MacroAssembler& masm, Address arrayVal,
                            AllocatableGeneralRegisterSet regs);
};

class ICCall_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;
  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 16;

  private:
    explicit ICCall_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::Call_Fallback, stubCode)
    {}

  public:
    bool scriptedStubsAreGeneralized() const {
        return hasStub(Call_AnyScripted);
    }
    bool nativeStubsAreGeneralized() const {
        // Return hasStub(Call_AnyNative) after Call_AnyNative stub is added.
        return false;
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        bool isConstructing_;
        bool isSpread_;
        CodeOffset bailoutReturnOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;
        void postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code) override;

        virtual int32_t getKey() const override {
            return static_cast<int32_t>(kind) |
                  (static_cast<int32_t>(isSpread_) << 16) |
                  (static_cast<int32_t>(isConstructing_) << 17);
        }

      public:
        Compiler(JSContext* cx, bool isConstructing, bool isSpread)
          : ICCallStubCompiler(cx, ICStub::Call_Fallback),
            isConstructing_(isConstructing),
            isSpread_(isSpread)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCall_Fallback>(space, getStubCode());
        }
    };
};

class ICCall_Scripted : public ICMonitoredStub
{
    friend class ICStubSpace;
  public:
    // The maximum number of inlineable spread call arguments. Keep this small
    // to avoid controllable stack overflows by attackers passing large arrays
    // to spread call. This value is shared with ICCall_Native.
    static const uint32_t MAX_ARGS_SPREAD_LENGTH = 16;

  protected:
    GCPtrFunction callee_;
    GCPtrObject templateObject_;
    uint32_t pcOffset_;

    ICCall_Scripted(JitCode* stubCode, ICStub* firstMonitorStub,
                    JSFunction* callee, JSObject* templateObject,
                    uint32_t pcOffset);

  public:
    static ICCall_Scripted* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                  ICCall_Scripted& other);

    GCPtrFunction& callee() {
        return callee_;
    }
    GCPtrObject& templateObject() {
        return templateObject_;
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Scripted, callee_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_Scripted, pcOffset_);
    }
};

class ICCall_AnyScripted : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;

    ICCall_AnyScripted(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_AnyScripted, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    { }

  public:
    static ICCall_AnyScripted* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                     ICCall_AnyScripted& other);

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_AnyScripted, pcOffset_);
    }
};

// Compiler for Call_Scripted and Call_AnyScripted stubs.
class ICCallScriptedCompiler : public ICCallStubCompiler {
  protected:
    ICStub* firstMonitorStub_;
    bool isConstructing_;
    bool isSpread_;
    bool maybeCrossRealm_;
    RootedFunction callee_;
    RootedObject templateObject_;
    uint32_t pcOffset_;
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

    virtual int32_t getKey() const override {
        return static_cast<int32_t>(kind) |
              (static_cast<int32_t>(isConstructing_) << 16) |
              (static_cast<int32_t>(isSpread_) << 17) |
              (static_cast<int32_t>(maybeCrossRealm_) << 18);
    }

  public:
    ICCallScriptedCompiler(JSContext* cx, ICStub* firstMonitorStub,
                           JSFunction* callee, JSObject* templateObject,
                           bool isConstructing, bool isSpread, bool maybeCrossRealm,
                           uint32_t pcOffset)
      : ICCallStubCompiler(cx, ICStub::Call_Scripted),
        firstMonitorStub_(firstMonitorStub),
        isConstructing_(isConstructing),
        isSpread_(isSpread),
        maybeCrossRealm_(maybeCrossRealm),
        callee_(cx, callee),
        templateObject_(cx, templateObject),
        pcOffset_(pcOffset)
    { }

    ICCallScriptedCompiler(JSContext* cx, ICStub* firstMonitorStub, bool isConstructing,
                           bool isSpread, uint32_t pcOffset)
      : ICCallStubCompiler(cx, ICStub::Call_AnyScripted),
        firstMonitorStub_(firstMonitorStub),
        isConstructing_(isConstructing),
        isSpread_(isSpread),
        maybeCrossRealm_(true),
        callee_(cx, nullptr),
        templateObject_(cx, nullptr),
        pcOffset_(pcOffset)
    { }

    ICStub* getStub(ICStubSpace* space) override {
        if (callee_) {
            return newStub<ICCall_Scripted>(space, getStubCode(), firstMonitorStub_, callee_,
                                            templateObject_, pcOffset_);
        }
        return newStub<ICCall_AnyScripted>(space, getStubCode(), firstMonitorStub_, pcOffset_);
    }
};

class ICCall_Native : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    GCPtrFunction callee_;
    GCPtrObject templateObject_;
    uint32_t pcOffset_;

#ifdef JS_SIMULATOR
    void* native_;
#endif

    ICCall_Native(JitCode* stubCode, ICStub* firstMonitorStub,
                  JSFunction* callee, JSObject* templateObject,
                  uint32_t pcOffset);

  public:
    static ICCall_Native* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                ICCall_Native& other);

    GCPtrFunction& callee() {
        return callee_;
    }
    GCPtrObject& templateObject() {
        return templateObject_;
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Native, callee_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_Native, pcOffset_);
    }

#ifdef JS_SIMULATOR
    static size_t offsetOfNative() {
        return offsetof(ICCall_Native, native_);
    }
#endif

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        bool isConstructing_;
        bool ignoresReturnValue_;
        bool isSpread_;
        bool isCrossRealm_;
        RootedFunction callee_;
        RootedObject templateObject_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

        virtual int32_t getKey() const override {
            return static_cast<int32_t>(kind) |
                  (static_cast<int32_t>(isSpread_) << 16) |
                  (static_cast<int32_t>(isConstructing_) << 17) |
                  (static_cast<int32_t>(ignoresReturnValue_) << 18) |
                  (static_cast<int32_t>(isCrossRealm_) << 19);
        }

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub,
                 HandleFunction callee, HandleObject templateObject,
                 bool isConstructing, bool ignoresReturnValue, bool isSpread, bool isCrossRealm,
                 uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_Native),
            firstMonitorStub_(firstMonitorStub),
            isConstructing_(isConstructing),
            ignoresReturnValue_(ignoresReturnValue),
            isSpread_(isSpread),
            isCrossRealm_(isCrossRealm),
            callee_(cx, callee),
            templateObject_(cx, templateObject),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCall_Native>(space, getStubCode(), firstMonitorStub_, callee_,
                                          templateObject_, pcOffset_);
        }
    };
};

class ICCall_ClassHook : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    const Class* clasp_;
    void* native_;
    GCPtrObject templateObject_;
    uint32_t pcOffset_;

    ICCall_ClassHook(JitCode* stubCode, ICStub* firstMonitorStub,
                     const Class* clasp, Native native, JSObject* templateObject,
                     uint32_t pcOffset);

  public:
    static ICCall_ClassHook* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                   ICCall_ClassHook& other);

    const Class* clasp() {
        return clasp_;
    }
    void* native() {
        return native_;
    }
    GCPtrObject& templateObject() {
        return templateObject_;
    }

    static size_t offsetOfClass() {
        return offsetof(ICCall_ClassHook, clasp_);
    }
    static size_t offsetOfNative() {
        return offsetof(ICCall_ClassHook, native_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ClassHook, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        bool isConstructing_;
        const Class* clasp_;
        Native native_;
        RootedObject templateObject_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

        virtual int32_t getKey() const override {
            return static_cast<int32_t>(kind) |
                  (static_cast<int32_t>(isConstructing_) << 16);
        }

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub,
                 const Class* clasp, Native native,
                 HandleObject templateObject, uint32_t pcOffset,
                 bool isConstructing)
          : ICCallStubCompiler(cx, ICStub::Call_ClassHook),
            firstMonitorStub_(firstMonitorStub),
            isConstructing_(isConstructing),
            clasp_(clasp),
            native_(native),
            templateObject_(cx, templateObject),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCall_ClassHook>(space, getStubCode(), firstMonitorStub_, clasp_,
                                             native_, templateObject_, pcOffset_);
        }
    };
};

class ICCall_ScriptedApplyArray : public ICMonitoredStub
{
    friend class ICStubSpace;
  public:
    // The maximum length of an inlineable funcall array.
    // Keep this small to avoid controllable stack overflows by attackers passing large
    // arrays to fun.apply.
    static const uint32_t MAX_ARGS_ARRAY_LENGTH = 16;

  protected:
    uint32_t pcOffset_;

    ICCall_ScriptedApplyArray(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_ScriptedApplyArray, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    {}

  public:
    static ICCall_ScriptedApplyArray* Clone(JSContext* cx,
                                            ICStubSpace* space,
                                            ICStub* firstMonitorStub,
                                            ICCall_ScriptedApplyArray& other);

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ScriptedApplyArray, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_ScriptedApplyArray),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCall_ScriptedApplyArray>(space, getStubCode(), firstMonitorStub_,
                                                      pcOffset_);
        }
    };
};

class ICCall_ScriptedApplyArguments : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;

    ICCall_ScriptedApplyArguments(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_ScriptedApplyArguments, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    {}

  public:
    static ICCall_ScriptedApplyArguments* Clone(JSContext* cx,
                                                ICStubSpace* space,
                                                ICStub* firstMonitorStub,
                                                ICCall_ScriptedApplyArguments& other);

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ScriptedApplyArguments, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_ScriptedApplyArguments),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCall_ScriptedApplyArguments>(space, getStubCode(), firstMonitorStub_,
                                                          pcOffset_);
        }
    };
};

// Handles calls of the form |fun.call(...)| where fun is a scripted function.
class ICCall_ScriptedFunCall : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;

    ICCall_ScriptedFunCall(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_ScriptedFunCall, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    {}

  public:
    static ICCall_ScriptedFunCall* Clone(JSContext* cx, ICStubSpace* space,
                                         ICStub* firstMonitorStub, ICCall_ScriptedFunCall& other);

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ScriptedFunCall, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_ScriptedFunCall),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCall_ScriptedFunCall>(space, getStubCode(), firstMonitorStub_,
                                                   pcOffset_);
        }
    };
};

class ICCall_ConstStringSplit : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;
    GCPtrString expectedStr_;
    GCPtrString expectedSep_;
    GCPtrArrayObject templateObject_;

    ICCall_ConstStringSplit(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset,
                            JSString* str, JSString* sep, ArrayObject* templateObject)
      : ICMonitoredStub(ICStub::Call_ConstStringSplit, stubCode, firstMonitorStub),
        pcOffset_(pcOffset), expectedStr_(str), expectedSep_(sep),
        templateObject_(templateObject)
    { }

  public:
    static size_t offsetOfExpectedStr() {
        return offsetof(ICCall_ConstStringSplit, expectedStr_);
    }

    static size_t offsetOfExpectedSep() {
        return offsetof(ICCall_ConstStringSplit, expectedSep_);
    }

    static size_t offsetOfTemplateObject() {
        return offsetof(ICCall_ConstStringSplit, templateObject_);
    }

    GCPtrString& expectedStr() {
        return expectedStr_;
    }

    GCPtrString& expectedSep() {
        return expectedSep_;
    }

    GCPtrArrayObject& templateObject() {
        return templateObject_;
    }

    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        uint32_t pcOffset_;
        RootedString expectedStr_;
        RootedString expectedSep_;
        RootedArrayObject templateObject_;

        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub, uint32_t pcOffset, HandleString str,
                 HandleString sep, HandleArrayObject templateObject)
          : ICCallStubCompiler(cx, ICStub::Call_ConstStringSplit),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset),
            expectedStr_(cx, str),
            expectedSep_(cx, sep),
            templateObject_(cx, templateObject)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCall_ConstStringSplit>(space, getStubCode(), firstMonitorStub_,
                                                    pcOffset_, expectedStr_, expectedSep_,
                                                    templateObject_);
        }
   };
};

class ICCall_IsSuspendedGenerator : public ICStub
{
    friend class ICStubSpace;

  protected:
    explicit ICCall_IsSuspendedGenerator(JitCode* stubCode)
      : ICStub(ICStub::Call_IsSuspendedGenerator, stubCode)
    {}

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::Call_IsSuspendedGenerator)
        {}
        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCall_IsSuspendedGenerator>(space, getStubCode());
        }
   };
};

// Stub for performing a TableSwitch, updating the IC's return address to jump
// to whatever point the switch is branching to.
class ICTableSwitch : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    void** table_;
    int32_t min_;
    int32_t length_;
    void* defaultTarget_;

    ICTableSwitch(JitCode* stubCode, void** table,
                  int32_t min, int32_t length, void* defaultTarget)
      : ICStub(TableSwitch, stubCode), table_(table),
        min_(min), length_(length), defaultTarget_(defaultTarget)
    {}

  public:
    void fixupJumpTable(JSScript* script, BaselineScript* baseline);

    class Compiler : public ICStubCompiler {
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

        jsbytecode* pc_;

      public:
        Compiler(JSContext* cx, jsbytecode* pc)
          : ICStubCompiler(cx, ICStub::TableSwitch), pc_(pc)
        {}

        ICStub* getStub(ICStubSpace* space) override;
    };
};

// IC for constructing an iterator from an input value.
class ICGetIterator_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICGetIterator_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::GetIterator_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::GetIterator_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICGetIterator_Fallback>(space, getStubCode());
        }
    };
};

// IC for testing if there are more values in an iterator.
class ICIteratorMore_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICIteratorMore_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::IteratorMore_Fallback, stubCode)
    { }

  public:
    void setHasNonStringResult() {
        extra_ = 1;
    }
    bool hasNonStringResult() const {
        MOZ_ASSERT(extra_ <= 1);
        return extra_;
    }

    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::IteratorMore_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICIteratorMore_Fallback>(space, getStubCode());
        }
    };
};

// IC for testing if there are more values in a native iterator.
class ICIteratorMore_Native : public ICStub
{
    friend class ICStubSpace;

    explicit ICIteratorMore_Native(JitCode* stubCode)
      : ICStub(ICStub::IteratorMore_Native, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::IteratorMore_Native)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICIteratorMore_Native>(space, getStubCode());
        }
    };
};

// IC for closing an iterator.
class ICIteratorClose_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICIteratorClose_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::IteratorClose_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::IteratorClose_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICIteratorClose_Fallback>(space, getStubCode());
        }
    };
};

// InstanceOf
//      JSOP_INSTANCEOF
class ICInstanceOf_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICInstanceOf_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::InstanceOf_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::InstanceOf_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICInstanceOf_Fallback>(space, getStubCode());
        }
    };
};

// TypeOf
//      JSOP_TYPEOF
//      JSOP_TYPEOFEXPR
class ICTypeOf_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICTypeOf_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::TypeOf_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 6;

    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::TypeOf_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICTypeOf_Fallback>(space, getStubCode());
        }
    };
};

class ICRest_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    GCPtrArrayObject templateObject_;

    ICRest_Fallback(JitCode* stubCode, ArrayObject* templateObject)
      : ICFallbackStub(ICStub::Rest_Fallback, stubCode), templateObject_(templateObject)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    GCPtrArrayObject& templateObject() {
        return templateObject_;
    }

    class Compiler : public ICStubCompiler {
      protected:
        RootedArrayObject templateObject;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ArrayObject* templateObject)
          : ICStubCompiler(cx, ICStub::Rest_Fallback),
            templateObject(cx, templateObject)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICRest_Fallback>(space, getStubCode(), templateObject);
        }
    };
};

// Stub for JSOP_RETSUB ("returning" from a |finally| block).
class ICRetSub_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICRetSub_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::RetSub_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::RetSub_Fallback)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICRetSub_Fallback>(space, getStubCode());
        }
    };
};

// Optimized JSOP_RETSUB stub. Every stub maps a single pc offset to its
// native code address.
class ICRetSub_Resume : public ICStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;
    uint8_t* addr_;

    ICRetSub_Resume(JitCode* stubCode, uint32_t pcOffset, uint8_t* addr)
      : ICStub(ICStub::RetSub_Resume, stubCode),
        pcOffset_(pcOffset),
        addr_(addr)
    { }

  public:
    static size_t offsetOfPCOffset() {
        return offsetof(ICRetSub_Resume, pcOffset_);
    }
    static size_t offsetOfAddr() {
        return offsetof(ICRetSub_Resume, addr_);
    }

    class Compiler : public ICStubCompiler {
        uint32_t pcOffset_;
        uint8_t* addr_;

        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, uint32_t pcOffset, uint8_t* addr)
          : ICStubCompiler(cx, ICStub::RetSub_Resume),
            pcOffset_(pcOffset),
            addr_(addr)
        { }

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICRetSub_Resume>(space, getStubCode(), pcOffset_, addr_);
        }
    };
};

// UnaryArith
//     JSOP_BITNOT
//     JSOP_NEG

class ICUnaryArith_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICUnaryArith_Fallback(JitCode* stubCode)
      : ICFallbackStub(UnaryArith_Fallback, stubCode)
    {
        extra_ = 0;
    }

  public:
    bool sawDoubleResult() {
        return extra_;
    }
    void setSawDoubleResult() {
        extra_ = 1;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::UnaryArith_Fallback)
        {}

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICUnaryArith_Fallback>(space, getStubCode());
        }
    };
};

// Compare
//      JSOP_LT
//      JSOP_LE
//      JSOP_GT
//      JSOP_GE
//      JSOP_EQ
//      JSOP_NE
//      JSOP_STRICTEQ
//      JSOP_STRICTNE

class ICCompare_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICCompare_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::Compare_Fallback, stubCode) {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::Compare_Fallback) {}

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICCompare_Fallback>(space, getStubCode());
        }
    };
};


// BinaryArith
//      JSOP_ADD, JSOP_SUB, JSOP_MUL, JOP_DIV, JSOP_MOD
//      JSOP_BITAND, JSOP_BITXOR, JSOP_BITOR
//      JSOP_LSH, JSOP_RSH, JSOP_URSH

class ICBinaryArith_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICBinaryArith_Fallback(JitCode* stubCode)
      : ICFallbackStub(BinaryArith_Fallback, stubCode)
    {
        extra_ = 0;
    }

    static const uint16_t SAW_DOUBLE_RESULT_BIT = 0x1;

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    bool sawDoubleResult() const {
        return extra_ & SAW_DOUBLE_RESULT_BIT;
    }
    void setSawDoubleResult() {
        extra_ |= SAW_DOUBLE_RESULT_BIT;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::BinaryArith_Fallback) {}

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICBinaryArith_Fallback>(space, getStubCode());
        }
    };
};

// JSOP_NEWARRAY

class ICNewArray_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    GCPtrObject templateObject_;

    // The group used for objects created here is always available, even if the
    // template object itself is not.
    GCPtrObjectGroup templateGroup_;

    ICNewArray_Fallback(JitCode* stubCode, ObjectGroup* templateGroup)
      : ICFallbackStub(ICStub::NewArray_Fallback, stubCode),
        templateObject_(nullptr), templateGroup_(templateGroup)
    {}

  public:
    class Compiler : public ICStubCompiler {
        RootedObjectGroup templateGroup;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        Compiler(JSContext* cx, ObjectGroup* templateGroup)
          : ICStubCompiler(cx, ICStub::NewArray_Fallback),
            templateGroup(cx, templateGroup)
        {}

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICNewArray_Fallback>(space, getStubCode(), templateGroup);
        }
    };

    GCPtrObject& templateObject() {
        return templateObject_;
    }

    void setTemplateObject(JSObject* obj) {
        MOZ_ASSERT(obj->group() == templateGroup());
        templateObject_ = obj;
    }

    GCPtrObjectGroup& templateGroup() {
        return templateGroup_;
    }

    void setTemplateGroup(ObjectGroup* group) {
        templateObject_ = nullptr;
        templateGroup_ = group;
    }
};


// JSOP_NEWOBJECT

class ICNewObject_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    GCPtrObject templateObject_;

    explicit ICNewObject_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::NewObject_Fallback, stubCode), templateObject_(nullptr)
    {}

  public:
    class Compiler : public ICStubCompiler {
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm) override;

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::NewObject_Fallback)
        {}

        ICStub* getStub(ICStubSpace* space) override {
            return newStub<ICNewObject_Fallback>(space, getStubCode());
        }
    };

    GCPtrObject& templateObject() {
        return templateObject_;
    }

    void setTemplateObject(JSObject* obj) {
        templateObject_ = obj;
    }
};

inline bool
IsCacheableDOMProxy(JSObject* obj)
{
    if (!obj->is<ProxyObject>()) {
        return false;
    }

    const BaseProxyHandler* handler = obj->as<ProxyObject>().handler();
    return handler->family() == GetDOMProxyHandlerFamily();
}

struct IonOsrTempData;

template <typename T>
void EmitICUnboxedPreBarrier(MacroAssembler &masm, const T& address, JSValueType type);

// Write an arbitrary value to a typed array or typed object address at dest.
// If the value could not be converted to the appropriate format, jump to
// failure.
template <typename T>
void StoreToTypedArray(JSContext* cx, MacroAssembler& masm, Scalar::Type type,
                       const ValueOperand& value, const T& dest, Register scratch,
                       Label* failure);

} // namespace jit
} // namespace js

#endif /* jit_BaselineIC_h */
