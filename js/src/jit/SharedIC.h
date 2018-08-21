/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_SharedIC_h
#define jit_SharedIC_h

#include "gc/GC.h"
#include "jit/BaselineICList.h"
#include "jit/BaselineJIT.h"
#include "jit/ICState.h"
#include "jit/MacroAssembler.h"
#include "jit/SharedICRegisters.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/ReceiverGuard.h"
#include "vm/TypedArrayObject.h"

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

//
// An entry in the JIT IC descriptor table.
//
class ICEntry
{
  private:
    // A pointer to the shared IC stub for this instruction.
    ICStub* firstStub_;

    // Offset from the start of the JIT code where the IC
    // load and call instructions are.
    uint32_t returnOffset_;

    // The PC of this IC's bytecode op within the JSScript.
    uint32_t pcOffset_ : 28;

  public:
    enum Kind {
        // A for-op IC entry.
        Kind_Op = 0,

        // A non-op IC entry.
        Kind_NonOp,

        // A fake IC entry for returning from a callVM for an op.
        Kind_CallVM,

        // A fake IC entry for returning from a callVM not for an op (e.g., in
        // the prologue).
        Kind_NonOpCallVM,

        // A fake IC entry for returning from a callVM to after the
        // warmup counter.
        Kind_WarmupCounter,

        // A fake IC entry for returning from a callVM to the interrupt
        // handler via the over-recursion check on function entry.
        Kind_StackCheck,

        // As above, but for the early check. See emitStackCheck.
        Kind_EarlyStackCheck,

        // A fake IC entry for returning from DebugTrapHandler.
        Kind_DebugTrap,

        // A fake IC entry for returning from a callVM to
        // Debug{Prologue,AfterYield,Epilogue}.
        Kind_DebugPrologue,
        Kind_DebugAfterYield,
        Kind_DebugEpilogue,

        Kind_Invalid
    };

  private:
    // What this IC is for.
    Kind kind_ : 4;

    // Set the kind and asserts that it's sane.
    void setKind(Kind kind) {
        MOZ_ASSERT(kind < Kind_Invalid);
        kind_ = kind;
        MOZ_ASSERT(this->kind() == kind);
    }

  public:
    ICEntry(uint32_t pcOffset, Kind kind)
      : firstStub_(nullptr), returnOffset_(), pcOffset_(pcOffset)
    {
        // The offset must fit in at least 28 bits, since we shave off 4 for
        // the Kind enum.
        MOZ_ASSERT(pcOffset_ == pcOffset);
        JS_STATIC_ASSERT(BaselineScript::MAX_JSSCRIPT_LENGTH <= (1u << 28) - 1);
        MOZ_ASSERT(pcOffset <= BaselineScript::MAX_JSSCRIPT_LENGTH);
        setKind(kind);
    }

    CodeOffset returnOffset() const {
        return CodeOffset(returnOffset_);
    }

    void setReturnOffset(CodeOffset offset) {
        MOZ_ASSERT(offset.offset() <= (size_t) UINT32_MAX);
        returnOffset_ = (uint32_t) offset.offset();
    }

    uint32_t pcOffset() const {
        return pcOffset_;
    }

    jsbytecode* pc(JSScript* script) const {
        return script->offsetToPC(pcOffset_);
    }

    Kind kind() const {
        // MSVC compiles enums as signed.
        return Kind(kind_ & 0xf);
    }
    bool isForOp() const {
        return kind() == Kind_Op;
    }

    void setFakeKind(Kind kind) {
        MOZ_ASSERT(kind != Kind_Op && kind != Kind_NonOp);
        setKind(kind);
    }

    bool hasStub() const {
        return firstStub_ != nullptr;
    }
    ICStub* firstStub() const {
        MOZ_ASSERT(hasStub());
        return firstStub_;
    }

    ICFallbackStub* fallbackStub() const;

    void setFirstStub(ICStub* stub) {
        firstStub_ = stub;
    }

    static inline size_t offsetOfFirstStub() {
        return offsetof(ICEntry, firstStub_);
    }

    inline ICStub** addressOfFirstStub() {
        return &firstStub_;
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
        if (!code)
            return nullptr;
        T* result = space->allocate<T>(code, std::forward<Args>(args)...);
        if (!result)
            ReportOutOfMemory(cx);
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
        while (lastStub->next_)
            lastStub = lastStub->next_;
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

} // namespace jit
} // namespace js

#endif /* jit_SharedIC_h */
