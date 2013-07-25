/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_BaselineIC_h
#define ion_BaselineIC_h

#ifdef JS_ION

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsopcode.h"
#include "jsproxy.h"

#include "gc/Heap.h"
#include "ion/BaselineJIT.h"
#include "ion/BaselineRegisters.h"

namespace js {
namespace ion {

//
// Baseline Inline Caches are polymorphic caches that aggressively
// share their stub code.
//
// Every polymorphic site contains a linked list of stubs which are
// specific to that site.  These stubs are composed of a |StubData|
// structure that stores parametrization information (e.g.
// the shape pointer for a shape-check-and-property-get stub), any
// dynamic information (e.g. use counts), a pointer to the stub code,
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
// different TypeObject to update.  New input types must be tracked on a typeobject-to-
// typeobject basis.
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

//
// An entry in the Baseline IC descriptor table.
//
class ICEntry
{
  private:
    // Offset from the start of the JIT code where the IC
    // load and call instructions are.
    uint32_t returnOffset_;

    // The PC of this IC's bytecode op within the JSScript.
    uint32_t pcOffset_ : 31;

    // Whether this IC is for a bytecode op.
    uint32_t isForOp_ : 1;

    // A pointer to the baseline IC stub for this instruction.
    ICStub *firstStub_;

  public:
    ICEntry(uint32_t pcOffset, bool isForOp)
      : returnOffset_(), pcOffset_(pcOffset), isForOp_(isForOp), firstStub_(NULL)
    {}

    CodeOffsetLabel returnOffset() const {
        return CodeOffsetLabel(returnOffset_);
    }

    void setReturnOffset(CodeOffsetLabel offset) {
        JS_ASSERT(offset.offset() <= (size_t) UINT32_MAX);
        returnOffset_ = (uint32_t) offset.offset();
    }

    void fixupReturnOffset(MacroAssembler &masm) {
        CodeOffsetLabel offset = returnOffset();
        offset.fixup(&masm);
        JS_ASSERT(offset.offset() <= UINT32_MAX);
        returnOffset_ = (uint32_t) offset.offset();
    }

    uint32_t pcOffset() const {
        return pcOffset_;
    }

    jsbytecode *pc(JSScript *script) const {
        return script->code + pcOffset_;
    }

    bool isForOp() const {
        return isForOp_;
    }

    bool hasStub() const {
        return firstStub_ != NULL;
    }
    ICStub *firstStub() const {
        JS_ASSERT(hasStub());
        return firstStub_;
    }

    ICFallbackStub *fallbackStub() const;

    void setFirstStub(ICStub *stub) {
        firstStub_ = stub;
    }

    static inline size_t offsetOfFirstStub() {
        return offsetof(ICEntry, firstStub_);
    }

    inline ICStub **addressOfFirstStub() {
        return &firstStub_;
    }
};

// List of baseline IC stub kinds.
#define IC_STUB_KIND_LIST(_)    \
    _(UseCount_Fallback)        \
                                \
    _(Profiler_Fallback)        \
    _(Profiler_PushFunction)    \
                                \
    _(TypeMonitor_Fallback)     \
    _(TypeMonitor_SingleObject) \
    _(TypeMonitor_TypeObject)   \
    _(TypeMonitor_PrimitiveSet) \
                                \
    _(TypeUpdate_Fallback)      \
    _(TypeUpdate_SingleObject)  \
    _(TypeUpdate_TypeObject)    \
    _(TypeUpdate_PrimitiveSet)  \
                                \
    _(This_Fallback)            \
                                \
    _(NewArray_Fallback)        \
    _(NewObject_Fallback)       \
                                \
    _(Compare_Fallback)         \
    _(Compare_Int32)            \
    _(Compare_Double)           \
    _(Compare_NumberWithUndefined) \
    _(Compare_String)           \
    _(Compare_Boolean)          \
    _(Compare_Object)           \
    _(Compare_ObjectWithUndefined) \
    _(Compare_Int32WithBoolean) \
                                \
    _(ToBool_Fallback)          \
    _(ToBool_Int32)             \
    _(ToBool_String)            \
    _(ToBool_NullUndefined)     \
    _(ToBool_Double)            \
    _(ToBool_Object)            \
                                \
    _(ToNumber_Fallback)        \
                                \
    _(BinaryArith_Fallback)     \
    _(BinaryArith_Int32)        \
    _(BinaryArith_Double)       \
    _(BinaryArith_StringConcat) \
    _(BinaryArith_StringObjectConcat) \
    _(BinaryArith_BooleanWithInt32) \
    _(BinaryArith_DoubleWithInt32) \
                                \
    _(UnaryArith_Fallback)      \
    _(UnaryArith_Int32)         \
    _(UnaryArith_Double)        \
                                \
    _(Call_Fallback)            \
    _(Call_Scripted)            \
    _(Call_AnyScripted)         \
    _(Call_Native)              \
    _(Call_ScriptedApplyArguments) \
                                \
    _(GetElem_Fallback)         \
    _(GetElem_Native)           \
    _(GetElem_NativePrototype)  \
    _(GetElem_String)           \
    _(GetElem_Dense)            \
    _(GetElem_TypedArray)       \
    _(GetElem_Arguments)        \
                                \
    _(SetElem_Fallback)         \
    _(SetElem_Dense)            \
    _(SetElem_DenseAdd)         \
    _(SetElem_TypedArray)       \
                                \
    _(In_Fallback)              \
                                \
    _(GetName_Fallback)         \
    _(GetName_Global)           \
    _(GetName_Scope0)           \
    _(GetName_Scope1)           \
    _(GetName_Scope2)           \
    _(GetName_Scope3)           \
    _(GetName_Scope4)           \
    _(GetName_Scope5)           \
    _(GetName_Scope6)           \
                                \
    _(BindName_Fallback)        \
                                \
    _(GetIntrinsic_Fallback)    \
    _(GetIntrinsic_Constant)    \
                                \
    _(GetProp_Fallback)         \
    _(GetProp_ArrayLength)      \
    _(GetProp_TypedArrayLength) \
    _(GetProp_String)           \
    _(GetProp_StringLength)     \
    _(GetProp_Native)           \
    _(GetProp_NativePrototype)  \
    _(GetProp_CallScripted)     \
    _(GetProp_CallNative)       \
    _(GetProp_CallDOMProxyNative)\
    _(GetProp_CallDOMProxyWithGenerationNative)\
    _(GetProp_DOMProxyShadowed) \
    _(GetProp_ArgumentsLength)  \
                                \
    _(SetProp_Fallback)         \
    _(SetProp_Native)           \
    _(SetProp_NativeAdd)        \
    _(SetProp_CallScripted)     \
    _(SetProp_CallNative)       \
                                \
    _(TableSwitch)              \
                                \
    _(IteratorNew_Fallback)     \
    _(IteratorMore_Fallback)    \
    _(IteratorMore_Native)      \
    _(IteratorNext_Fallback)    \
    _(IteratorNext_Native)      \
    _(IteratorClose_Fallback)   \
                                \
    _(InstanceOf_Fallback)      \
                                \
    _(TypeOf_Fallback)          \
    _(TypeOf_Typed)             \
                                \
    _(Rest_Fallback)            \
                                \
    _(RetSub_Fallback)          \
    _(RetSub_Resume)

#define FORWARD_DECLARE_STUBS(kindName) class IC##kindName;
    IC_STUB_KIND_LIST(FORWARD_DECLARE_STUBS)
#undef FORWARD_DECLARE_STUBS

class ICMonitoredStub;
class ICMonitoredFallbackStub;
class ICUpdatedStub;

// Constant iterator that traverses arbitrary chains of ICStubs.
// No requirements are made of the ICStub used to construct this
// iterator, aside from that the stub be part of a NULL-terminated
// chain.
// The iterator is considered to be at its end once it has been
// incremented _past_ the last stub.  Thus, if 'atEnd()' returns
// true, the '*' and '->' operations are not valid.
class ICStubConstIterator
{
    friend class ICStub;
    friend class ICFallbackStub;

  private:
    ICStub *currentStub_;

  public:
    ICStubConstIterator(ICStub *currentStub) : currentStub_(currentStub) {}

    static ICStubConstIterator StartingAt(ICStub *stub) {
        return ICStubConstIterator(stub);
    }
    static ICStubConstIterator End(ICStub *stub) {
        return ICStubConstIterator(NULL);
    }

    bool operator ==(const ICStubConstIterator &other) const {
        return currentStub_ == other.currentStub_;
    }
    bool operator !=(const ICStubConstIterator &other) const {
        return !(*this == other);
    }

    ICStubConstIterator &operator++();

    ICStubConstIterator operator++(int) {
        ICStubConstIterator oldThis(*this);
        ++(*this);
        return oldThis;
    }

    ICStub *operator *() const {
        JS_ASSERT(currentStub_);
        return currentStub_;
    }

    ICStub *operator ->() const {
        JS_ASSERT(currentStub_);
        return currentStub_;
    }

    bool atEnd() const {
        return currentStub_ == NULL;
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
    ICEntry *icEntry_;
    ICFallbackStub *fallbackStub_;
    ICStub *previousStub_;
    ICStub *currentStub_;
    bool unlinked_;

    ICStubIterator(ICFallbackStub *fallbackStub, bool end=false);
  public:

    bool operator ==(const ICStubIterator &other) const {
        // == should only ever be called on stubs from the same chain.
        JS_ASSERT(icEntry_ == other.icEntry_);
        JS_ASSERT(fallbackStub_ == other.fallbackStub_);
        return currentStub_ == other.currentStub_;
    }
    bool operator !=(const ICStubIterator &other) const {
        return !(*this == other);
    }

    ICStubIterator &operator++();

    ICStubIterator operator++(int) {
        ICStubIterator oldThis(*this);
        ++(*this);
        return oldThis;
    }

    ICStub *operator *() const {
        return currentStub_;
    }

    ICStub *operator ->() const {
        return currentStub_;
    }

    bool atEnd() const {
        return currentStub_ == (ICStub *) fallbackStub_;
    }

    void unlink(Zone *zone);
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
        IC_STUB_KIND_LIST(DEF_ENUM_KIND)
#undef DEF_ENUM_KIND
        LIMIT
    };

    static inline bool IsValidKind(Kind k) {
        return (k > INVALID) && (k < LIMIT);
    }

    static const char *KindString(Kind k) {
        switch(k) {
#define DEF_KIND_STR(kindName) case kindName: return #kindName;
            IC_STUB_KIND_LIST(DEF_KIND_STR)
#undef DEF_KIND_STR
          default:
            MOZ_ASSUME_UNREACHABLE("Invalid kind.");
        }
    }

    enum Trait {
        Regular             = 0x0,
        Fallback            = 0x1,
        Monitored           = 0x2,
        MonitoredFallback   = 0x3,
        Updated             = 0x4
    };

    void markCode(JSTracer *trc, const char *name);
    void updateCode(IonCode *stubCode);
    void trace(JSTracer *trc);

  protected:
    // The kind of the stub.
    //  High bit is 'isFallback' flag.
    //  Second high bit is 'isMonitored' flag.
    Trait trait_ : 3;
    Kind kind_ : 13;

    // A 16-bit field usable by subtypes of ICStub for subtype-specific small-info
    uint16_t extra_;

    // The raw jitcode to call for this stub.
    uint8_t *stubCode_;

    // Pointer to next IC stub.  This is null for the last IC stub, which should
    // either be a fallback or inert IC stub.
    ICStub *next_;

    inline ICStub(Kind kind, IonCode *stubCode)
      : trait_(Regular),
        kind_(kind),
        extra_(0),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

    inline ICStub(Kind kind, Trait trait, IonCode *stubCode)
      : trait_(trait),
        kind_(kind),
        extra_(0),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
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

    inline const ICFallbackStub *toFallbackStub() const {
        JS_ASSERT(isFallback());
        return reinterpret_cast<const ICFallbackStub *>(this);
    }

    inline ICFallbackStub *toFallbackStub() {
        JS_ASSERT(isFallback());
        return reinterpret_cast<ICFallbackStub *>(this);
    }

    inline const ICMonitoredStub *toMonitoredStub() const {
        JS_ASSERT(isMonitored());
        return reinterpret_cast<const ICMonitoredStub *>(this);
    }

    inline ICMonitoredStub *toMonitoredStub() {
        JS_ASSERT(isMonitored());
        return reinterpret_cast<ICMonitoredStub *>(this);
    }

    inline const ICMonitoredFallbackStub *toMonitoredFallbackStub() const {
        JS_ASSERT(isMonitoredFallback());
        return reinterpret_cast<const ICMonitoredFallbackStub *>(this);
    }

    inline ICMonitoredFallbackStub *toMonitoredFallbackStub() {
        JS_ASSERT(isMonitoredFallback());
        return reinterpret_cast<ICMonitoredFallbackStub *>(this);
    }

    inline const ICUpdatedStub *toUpdatedStub() const {
        JS_ASSERT(isUpdated());
        return reinterpret_cast<const ICUpdatedStub *>(this);
    }

    inline ICUpdatedStub *toUpdatedStub() {
        JS_ASSERT(isUpdated());
        return reinterpret_cast<ICUpdatedStub *>(this);
    }

#define KIND_METHODS(kindName)   \
    inline bool is##kindName() const { return kind() == kindName; } \
    inline const IC##kindName *to##kindName() const { \
        JS_ASSERT(is##kindName()); \
        return reinterpret_cast<const IC##kindName *>(this); \
    } \
    inline IC##kindName *to##kindName() { \
        JS_ASSERT(is##kindName()); \
        return reinterpret_cast<IC##kindName *>(this); \
    }
    IC_STUB_KIND_LIST(KIND_METHODS)
#undef KIND_METHODS

    inline ICStub *next() const {
        return next_;
    }

    inline bool hasNext() const {
        return next_ != NULL;
    }

    inline void setNext(ICStub *stub) {
        next_ = stub;
    }

    inline ICStub **addressOfNext() {
        return &next_;
    }

    inline IonCode *ionCode() {
        return IonCode::FromExecutable(stubCode_);
    }

    inline uint8_t *rawStubCode() const {
        return stubCode_;
    }

    // This method is not valid on TypeUpdate stub chains!
    inline ICFallbackStub *getChainFallback() {
        ICStub *lastStub = this;
        while (lastStub->next_)
            lastStub = lastStub->next_;
        JS_ASSERT(lastStub->isFallback());
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

    static bool CanMakeCalls(ICStub::Kind kind) {
        JS_ASSERT(IsValidKind(kind));
        switch (kind) {
          case Call_Fallback:
          case Call_Scripted:
          case Call_AnyScripted:
          case Call_Native:
          case Call_ScriptedApplyArguments:
          case UseCount_Fallback:
          case GetProp_CallScripted:
          case GetProp_CallNative:
          case GetProp_CallDOMProxyNative:
          case GetProp_CallDOMProxyWithGenerationNative:
          case GetProp_DOMProxyShadowed:
          case SetProp_CallScripted:
          case SetProp_CallNative:
          case RetSub_Fallback:
          // These two fallback stubs don't actually make non-tail calls,
          // but the fallback code for the bailout path needs to pop the stub frame
          // pushed during the bailout.
          case GetProp_Fallback:
          case SetProp_Fallback:
            return true;
          default:
            return false;
        }
    }

    // Optimized stubs get purged on GC.  But some stubs can be active on the
    // stack during GC - specifically the ones that can make calls.  To ensure
    // that these do not get purged, all stubs that can make calls are allocated
    // in the fallback stub space.
    bool allocatedInFallbackSpace() const {
        JS_ASSERT(next());
        return CanMakeCalls(kind());
    }
};

class ICFallbackStub : public ICStub
{
    friend class ICStubConstIterator;
  protected:
    // Fallback stubs need these fields to easily add new stubs to
    // the linked list of stubs for an IC.

    // The IC entry for this linked list of stubs.
    ICEntry *icEntry_;

    // The number of stubs kept in the IC entry.
    uint32_t numOptimizedStubs_;

    // A pointer to the location stub pointer that needs to be
    // changed to add a new "last" stub immediately before the fallback
    // stub.  This'll start out pointing to the icEntry's "firstStub_"
    // field, and as new stubs are addd, it'll point to the current
    // last stub's "next_" field.
    ICStub **lastStubPtrAddr_;

    ICFallbackStub(Kind kind, IonCode *stubCode)
      : ICStub(kind, ICStub::Fallback, stubCode),
        icEntry_(NULL),
        numOptimizedStubs_(0),
        lastStubPtrAddr_(NULL) {}

    ICFallbackStub(Kind kind, Trait trait, IonCode *stubCode)
      : ICStub(kind, trait, stubCode),
        icEntry_(NULL),
        numOptimizedStubs_(0),
        lastStubPtrAddr_(NULL)
    {
        JS_ASSERT(trait == ICStub::Fallback ||
                  trait == ICStub::MonitoredFallback);
    }

  public:
    inline ICEntry *icEntry() const {
        return icEntry_;
    }

    inline size_t numOptimizedStubs() const {
        return (size_t) numOptimizedStubs_;
    }

    // The icEntry and lastStubPtrAddr_ fields can't be initialized when the stub is
    // created since the stub is created at compile time, and we won't know the IC entry
    // address until after compile when the BaselineScript is created.  This method
    // allows these fields to be fixed up at that point.
    void fixupICEntry(ICEntry *icEntry) {
        JS_ASSERT(icEntry_ == NULL);
        JS_ASSERT(lastStubPtrAddr_ == NULL);
        icEntry_ = icEntry;
        lastStubPtrAddr_ = icEntry_->addressOfFirstStub();
    }

    // Add a new stub to the IC chain terminated by this fallback stub.
    void addNewStub(ICStub *stub) {
        JS_ASSERT(*lastStubPtrAddr_ == this);
        JS_ASSERT(stub->next() == NULL);
        stub->setNext(this);
        *lastStubPtrAddr_ = stub;
        lastStubPtrAddr_ = stub->addressOfNext();
        numOptimizedStubs_++;
    }

    ICStubConstIterator beginChainConst() const {
        return ICStubConstIterator(icEntry_->firstStub());
    }

    ICStubIterator beginChain() {
        return ICStubIterator(this);
    }

    bool hasStub(ICStub::Kind kind) const {
        for (ICStubConstIterator iter = beginChainConst(); !iter.atEnd(); iter++) {
            if (iter->kind() == kind)
                return true;
        }
        return false;
    }

    unsigned numStubsWithKind(ICStub::Kind kind) const {
        unsigned count = 0;
        for (ICStubConstIterator iter = beginChainConst(); !iter.atEnd(); iter++) {
            if (iter->kind() == kind)
                count++;
        }
        return count;
    }

    void unlinkStub(Zone *zone, ICStub *prev, ICStub *stub);
    void unlinkStubsWithKind(JSContext *cx, ICStub::Kind kind);
};

// Monitored stubs are IC stubs that feed a single resulting value out to a
// type monitor operation.
class ICMonitoredStub : public ICStub
{
  protected:
    // Pointer to the start of the type monitoring stub chain.
    ICStub *firstMonitorStub_;

    ICMonitoredStub(Kind kind, IonCode *stubCode, ICStub *firstMonitorStub);

  public:
    inline void updateFirstMonitorStub(ICStub *monitorStub) {
        // This should only be called once: when the first optimized monitor stub
        // is added to the type monitor IC chain.
        JS_ASSERT(firstMonitorStub_ && firstMonitorStub_->isTypeMonitor_Fallback());
        firstMonitorStub_ = monitorStub;
    }
    inline void resetFirstMonitorStub(ICStub *monitorFallback) {
        JS_ASSERT(monitorFallback->isTypeMonitor_Fallback());
        firstMonitorStub_ = monitorFallback;
    }
    inline ICStub *firstMonitorStub() const {
        return firstMonitorStub_;
    }

    static inline size_t offsetOfFirstMonitorStub() {
        return offsetof(ICMonitoredStub, firstMonitorStub_);
    }
};

// Monitored fallback stubs - as the name implies.
class ICMonitoredFallbackStub : public ICFallbackStub
{
  protected:
    // Pointer to the fallback monitor stub.
    ICTypeMonitor_Fallback *fallbackMonitorStub_;

    ICMonitoredFallbackStub(Kind kind, IonCode *stubCode)
      : ICFallbackStub(kind, ICStub::MonitoredFallback, stubCode),
        fallbackMonitorStub_(NULL) {}

  public:
    bool initMonitoringChain(JSContext *cx, ICStubSpace *space);
    bool addMonitorStubForValue(JSContext *cx, HandleScript script, HandleValue val);

    inline ICTypeMonitor_Fallback *fallbackMonitorStub() const {
        return fallbackMonitorStub_;
    }

    static inline size_t offsetOfFallbackMonitorStub() {
        return offsetof(ICMonitoredFallbackStub, fallbackMonitorStub_);
    }
};

// Updated stubs are IC stubs that use a TypeUpdate IC to track
// the status of heap typesets that need to be updated.
class ICUpdatedStub : public ICStub
{
  protected:
    // Pointer to the start of the type updating stub chain.
    ICStub *firstUpdateStub_;

    static const uint32_t MAX_OPTIMIZED_STUBS = 8;
    uint32_t numOptimizedStubs_;

    ICUpdatedStub(Kind kind, IonCode *stubCode)
      : ICStub(kind, ICStub::Updated, stubCode),
        firstUpdateStub_(NULL),
        numOptimizedStubs_(0)
    {}

  public:
    bool initUpdatingChain(JSContext *cx, ICStubSpace *space);

    bool addUpdateStubForValue(JSContext *cx, HandleScript script, HandleObject obj, HandleId id,
                               HandleValue val);

    void addOptimizedUpdateStub(ICStub *stub) {
        if (firstUpdateStub_->isTypeUpdate_Fallback()) {
            stub->setNext(firstUpdateStub_);
            firstUpdateStub_ = stub;
        } else {
            ICStub *iter = firstUpdateStub_;
            JS_ASSERT(iter->next() != NULL);
            while (!iter->next()->isTypeUpdate_Fallback())
                iter = iter->next();
            JS_ASSERT(iter->next()->next() == NULL);
            stub->setNext(iter->next());
            iter->setNext(stub);
        }

        numOptimizedStubs_++;
    }

    inline ICStub *firstUpdateStub() const {
        return firstUpdateStub_;
    }

    bool hasTypeUpdateStub(ICStub::Kind kind) {
        ICStub *stub = firstUpdateStub_;
        do {
            if (stub->kind() == kind)
                return true;

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

// Base class for stubcode compilers.
class ICStubCompiler
{
    // Prevent GC in the middle of stub compilation.
    js::gc::AutoSuppressGC suppressGC;


  protected:
    mozilla::DebugOnly<bool> entersStubFrame_;
    JSContext *cx;
    ICStub::Kind kind;

    // By default the stubcode key is just the kind.
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind);
    }

    virtual bool generateStubCode(MacroAssembler &masm) = 0;
    virtual bool postGenerateStubCode(MacroAssembler &masm, Handle<IonCode *> genCode) {
        return true;
    }
    IonCode *getStubCode();

    ICStubCompiler(JSContext *cx, ICStub::Kind kind)
      : suppressGC(cx), entersStubFrame_(false), cx(cx), kind(kind)
    {}

    // Emits a tail call to a VMFunction wrapper.
    bool tailCallVM(const VMFunction &fun, MacroAssembler &masm);

    // Emits a normal (non-tail) call to a VMFunction wrapper.
    bool callVM(const VMFunction &fun, MacroAssembler &masm);

    // Emits a call to a type-update IC, assuming that the value to be
    // checked is already in R0.
    bool callTypeUpdateIC(MacroAssembler &masm, uint32_t objectOffset);

    // A stub frame is used when a stub wants to call into the VM without
    // performing a tail call. This is required for the return address
    // to pc mapping to work.
    void enterStubFrame(MacroAssembler &masm, Register scratch);
    void leaveStubFrame(MacroAssembler &masm, bool calledIntoIon = false);

    // Some stubs need to emit SPS profiler updates.  This emits the guarding
    // jitcode for those stubs.  If profiling is not enabled, jumps to the
    // given label.
    void guardProfilingEnabled(MacroAssembler &masm, Register scratch, Label *skip);

    inline GeneralRegisterSet availableGeneralRegs(size_t numInputs) const {
        GeneralRegisterSet regs(GeneralRegisterSet::All());
        JS_ASSERT(!regs.has(BaselineStackReg));
#ifdef JS_CPU_ARM
        JS_ASSERT(!regs.has(BaselineTailCallReg));
#endif
        regs.take(BaselineFrameReg);
        regs.take(BaselineStubReg);
#ifdef JS_CPU_X64
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
            MOZ_ASSUME_UNREACHABLE("Invalid numInputs");
        }

        return regs;
    }

#ifdef JSGC_GENERATIONAL
    inline bool emitPostWriteBarrierSlot(MacroAssembler &masm, Register obj, Register scratch,
                                         GeneralRegisterSet saveRegs);
#endif

  public:
    virtual ICStub *getStub(ICStubSpace *space) = 0;

    ICStubSpace *getStubSpace(JSScript *script) {
        if (ICStub::CanMakeCalls(kind))
            return script->baselineScript()->fallbackStubSpace();
        return script->compartment()->ionCompartment()->optimizedStubSpace();
    }
};

// Base class for stub compilers that can generate multiple stubcodes.
// These compilers need access to the JSOp they are compiling for.
class ICMultiStubCompiler : public ICStubCompiler
{
  protected:
    JSOp op;

    // Stub keys for multi-stub kinds are composed of both the kind
    // and the op they are compiled for.
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(op) << 16);
    }

    ICMultiStubCompiler(JSContext *cx, ICStub::Kind kind, JSOp op)
      : ICStubCompiler(cx, kind), op(op) {}
};

// UseCount_Fallback

// A UseCount IC chain has only the fallback stub.
class ICUseCount_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICUseCount_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::UseCount_Fallback, stubCode)
    { }

  public:
    static inline ICUseCount_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICUseCount_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::UseCount_Fallback)
        { }

        ICUseCount_Fallback *getStub(ICStubSpace *space) {
            return ICUseCount_Fallback::New(space, getStubCode());
        }
    };
};

// Profiler_Fallback

class ICProfiler_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICProfiler_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::Profiler_Fallback, stubCode)
    { }

  public:
    static inline ICProfiler_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICProfiler_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::Profiler_Fallback)
        { }

        ICProfiler_Fallback *getStub(ICStubSpace *space) {
            return ICProfiler_Fallback::New(space, getStubCode());
        }
    };
};

// Profiler_PushFunction

class ICProfiler_PushFunction : public ICStub
{
    friend class ICStubSpace;

  protected:
    const char *str_;
    HeapPtrScript script_;

    ICProfiler_PushFunction(IonCode *stubCode, const char *str, HandleScript script);

  public:
    static inline ICProfiler_PushFunction *New(ICStubSpace *space, IonCode *code,
                                               const char *str, HandleScript script)
    {
        if (!code)
            return NULL;
        return space->allocate<ICProfiler_PushFunction>(code, str, script);
    }

    HeapPtrScript &script() {
        return script_;
    }

    static size_t offsetOfStr() {
        return offsetof(ICProfiler_PushFunction, str_);
    }
    static size_t offsetOfScript() {
        return offsetof(ICProfiler_PushFunction, script_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        const char *str_;
        RootedScript script_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, const char *str, HandleScript script)
          : ICStubCompiler(cx, ICStub::Profiler_PushFunction),
            str_(str),
            script_(cx, script)
        { }

        ICProfiler_PushFunction *getStub(ICStubSpace *space) {
            return ICProfiler_PushFunction::New(space, getStubCode(), str_, script_);
        }
    };
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

    TypeCheckPrimitiveSetStub(Kind kind, IonCode *stubCode, uint16_t flags)
        : ICStub(kind, stubCode)
    {
        JS_ASSERT(kind == TypeMonitor_PrimitiveSet || kind == TypeUpdate_PrimitiveSet);
        JS_ASSERT(flags && !(flags & ~ValidFlags()));
        extra_ = flags;
    }

    TypeCheckPrimitiveSetStub *updateTypesAndCode(uint16_t flags, IonCode *code) {
        JS_ASSERT(flags && !(flags & ~ValidFlags()));
        if (!code)
            return NULL;
        extra_ = flags;
        updateCode(code);
        return this;
    }

  public:
    uint16_t typeFlags() const {
        return extra_;
    }

    bool containsType(JSValueType type) const {
        JS_ASSERT(type <= JSVAL_TYPE_OBJECT);
        JS_ASSERT(type != JSVAL_TYPE_MAGIC);
        return extra_ & TypeToFlag(type);
    }

    ICTypeMonitor_PrimitiveSet *toMonitorStub() {
        return toTypeMonitor_PrimitiveSet();
    }

    ICTypeUpdate_PrimitiveSet *toUpdateStub() {
        return toTypeUpdate_PrimitiveSet();
    }

    class Compiler : public ICStubCompiler {
      protected:
        TypeCheckPrimitiveSetStub *existingStub_;
        uint16_t flags_;

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(flags_) << 16);
        }

      public:
        Compiler(JSContext *cx, Kind kind, TypeCheckPrimitiveSetStub *existingStub,
                 JSValueType type)
          : ICStubCompiler(cx, kind),
            existingStub_(existingStub),
            flags_((existingStub ? existingStub->typeFlags() : 0) | TypeToFlag(type))
        {
            JS_ASSERT_IF(existingStub_, flags_ != existingStub_->typeFlags());
        }

        TypeCheckPrimitiveSetStub *updateStub() {
            JS_ASSERT(existingStub_);
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
        ICMonitoredFallbackStub *mainFallbackStub_;
        ICEntry *icEntry_;
    };

    // Pointer to the first monitor stub.
    ICStub *firstMonitorStub_;

    // Address of the last monitor stub's field pointing to this
    // fallback monitor stub.  This will get updated when new
    // monitor stubs are created and added.
    ICStub **lastMonitorStubPtrAddr_;

    // Count of optimized type monitor stubs in this chain.
    uint32_t numOptimizedMonitorStubs_ : 8;

    // Whether this has a fallback stub referring to the IC entry.
    bool hasFallbackStub_ : 1;

    // Index of 'this' or argument which is being monitored, or BYTECODE_INDEX
    // if this is monitoring the types of values pushed at some bytecode.
    uint32_t argumentIndex_ : 23;

    static const uint32_t BYTECODE_INDEX = (1 << 23) - 1;

    ICTypeMonitor_Fallback(IonCode *stubCode, ICMonitoredFallbackStub *mainFallbackStub,
                           uint32_t argumentIndex)
      : ICStub(ICStub::TypeMonitor_Fallback, stubCode),
        mainFallbackStub_(mainFallbackStub),
        firstMonitorStub_(thisFromCtor()),
        lastMonitorStubPtrAddr_(NULL),
        numOptimizedMonitorStubs_(0),
        hasFallbackStub_(mainFallbackStub != NULL),
        argumentIndex_(argumentIndex)
    { }

    ICTypeMonitor_Fallback *thisFromCtor() {
        return this;
    }

    void addOptimizedMonitorStub(ICStub *stub) {
        stub->setNext(this);

        JS_ASSERT((lastMonitorStubPtrAddr_ != NULL) ==
                  (numOptimizedMonitorStubs_ || !hasFallbackStub_));

        if (lastMonitorStubPtrAddr_)
            *lastMonitorStubPtrAddr_ = stub;

        if (numOptimizedMonitorStubs_ == 0) {
            JS_ASSERT(firstMonitorStub_ == this);
            firstMonitorStub_ = stub;
        } else {
            JS_ASSERT(firstMonitorStub_ != NULL);
        }

        lastMonitorStubPtrAddr_ = stub->addressOfNext();
        numOptimizedMonitorStubs_++;
    }

  public:
    static inline ICTypeMonitor_Fallback *New(
        ICStubSpace *space, IonCode *code, ICMonitoredFallbackStub *mainFbStub,
        uint32_t argumentIndex)
    {
        if (!code)
            return NULL;
        return space->allocate<ICTypeMonitor_Fallback>(code, mainFbStub, argumentIndex);
    }

    bool hasStub(ICStub::Kind kind) {
        ICStub *stub = firstMonitorStub_;
        do {
            if (stub->kind() == kind)
                return true;

            stub = stub->next();
        } while (stub);

        return false;
    }

    inline ICFallbackStub *mainFallbackStub() const {
        JS_ASSERT(hasFallbackStub_);
        return mainFallbackStub_;
    }

    inline ICEntry *icEntry() const {
        return hasFallbackStub_ ? mainFallbackStub()->icEntry() : icEntry_;
    }

    inline ICStub *firstMonitorStub() const {
        return firstMonitorStub_;
    }

    static inline size_t offsetOfFirstMonitorStub() {
        return offsetof(ICTypeMonitor_Fallback, firstMonitorStub_);
    }

    inline uint32_t numOptimizedMonitorStubs() const {
        return numOptimizedMonitorStubs_;
    }

    inline bool monitorsThis() const {
        return argumentIndex_ == 0;
    }

    inline bool monitorsArgument(uint32_t *pargument) const {
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
    void fixupICEntry(ICEntry *icEntry) {
        JS_ASSERT(!hasFallbackStub_);
        JS_ASSERT(icEntry_ == NULL);
        JS_ASSERT(lastMonitorStubPtrAddr_ == NULL);
        icEntry_ = icEntry;
        lastMonitorStubPtrAddr_ = icEntry_->addressOfFirstStub();
    }

    // Create a new monitor stub for the type of the given value, and
    // add it to this chain.
    bool addMonitorStubForValue(JSContext *cx, HandleScript script, HandleValue val);

    void resetMonitorStubChain(Zone *zone);

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
        ICMonitoredFallbackStub *mainFallbackStub_;
        uint32_t argumentIndex_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICMonitoredFallbackStub *mainFallbackStub)
          : ICStubCompiler(cx, ICStub::TypeMonitor_Fallback),
            mainFallbackStub_(mainFallbackStub),
            argumentIndex_(BYTECODE_INDEX)
        { }

        Compiler(JSContext *cx, uint32_t argumentIndex)
          : ICStubCompiler(cx, ICStub::TypeMonitor_Fallback),
            mainFallbackStub_(NULL),
            argumentIndex_(argumentIndex)
        { }

        ICTypeMonitor_Fallback *getStub(ICStubSpace *space) {
            return ICTypeMonitor_Fallback::New(space, getStubCode(), mainFallbackStub_,
                                               argumentIndex_);
        }
    };
};

class ICTypeMonitor_PrimitiveSet : public TypeCheckPrimitiveSetStub
{
    friend class ICStubSpace;

    ICTypeMonitor_PrimitiveSet(IonCode *stubCode, uint16_t flags)
        : TypeCheckPrimitiveSetStub(TypeMonitor_PrimitiveSet, stubCode, flags)
    {}

  public:
    static inline ICTypeMonitor_PrimitiveSet *New(ICStubSpace *space, IonCode *code,
                                                  uint16_t flags)
    {
        if (!code)
            return NULL;
        return space->allocate<ICTypeMonitor_PrimitiveSet>(code, flags);
    }

    class Compiler : public TypeCheckPrimitiveSetStub::Compiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICTypeMonitor_PrimitiveSet *existingStub, JSValueType type)
          : TypeCheckPrimitiveSetStub::Compiler(cx, TypeMonitor_PrimitiveSet, existingStub, type)
        {}

        ICTypeMonitor_PrimitiveSet *updateStub() {
            TypeCheckPrimitiveSetStub *stub =
                this->TypeCheckPrimitiveSetStub::Compiler::updateStub();
            if (!stub)
                return NULL;
            return stub->toMonitorStub();
        }

        ICTypeMonitor_PrimitiveSet *getStub(ICStubSpace *space) {
            JS_ASSERT(!existingStub_);
            return ICTypeMonitor_PrimitiveSet::New(space, getStubCode(), flags_);
        }
    };
};

class ICTypeMonitor_SingleObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrObject obj_;

    ICTypeMonitor_SingleObject(IonCode *stubCode, HandleObject obj);

  public:
    static inline ICTypeMonitor_SingleObject *New(
            ICStubSpace *space, IonCode *code, HandleObject obj)
    {
        if (!code)
            return NULL;
        return space->allocate<ICTypeMonitor_SingleObject>(code, obj);
    }

    HeapPtrObject &object() {
        return obj_;
    }

    static size_t offsetOfObject() {
        return offsetof(ICTypeMonitor_SingleObject, obj_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObject obj_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleObject obj)
          : ICStubCompiler(cx, TypeMonitor_SingleObject),
            obj_(obj)
        { }

        ICTypeMonitor_SingleObject *getStub(ICStubSpace *space) {
            return ICTypeMonitor_SingleObject::New(space, getStubCode(), obj_);
        }
    };
};

class ICTypeMonitor_TypeObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrTypeObject type_;

    ICTypeMonitor_TypeObject(IonCode *stubCode, HandleTypeObject type);

  public:
    static inline ICTypeMonitor_TypeObject *New(
            ICStubSpace *space, IonCode *code, HandleTypeObject type)
    {
        if (!code)
            return NULL;
        return space->allocate<ICTypeMonitor_TypeObject>(code, type);
    }

    HeapPtrTypeObject &type() {
        return type_;
    }

    static size_t offsetOfType() {
        return offsetof(ICTypeMonitor_TypeObject, type_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleTypeObject type_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleTypeObject type)
          : ICStubCompiler(cx, TypeMonitor_TypeObject),
            type_(type)
        { }

        ICTypeMonitor_TypeObject *getStub(ICStubSpace *space) {
            return ICTypeMonitor_TypeObject::New(space, getStubCode(), type_);
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

    ICTypeUpdate_Fallback(IonCode *stubCode)
      : ICStub(ICStub::TypeUpdate_Fallback, stubCode)
    {}

  public:
    static inline ICTypeUpdate_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICTypeUpdate_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::TypeUpdate_Fallback)
        { }

        ICTypeUpdate_Fallback *getStub(ICStubSpace *space) {
            return ICTypeUpdate_Fallback::New(space, getStubCode());
        }
    };
};

class ICTypeUpdate_PrimitiveSet : public TypeCheckPrimitiveSetStub
{
    friend class ICStubSpace;

    ICTypeUpdate_PrimitiveSet(IonCode *stubCode, uint16_t flags)
        : TypeCheckPrimitiveSetStub(TypeUpdate_PrimitiveSet, stubCode, flags)
    {}

  public:
    static inline ICTypeUpdate_PrimitiveSet *New(ICStubSpace *space, IonCode *code,
                                                 uint16_t flags)
    {
        if (!code)
            return NULL;
        return space->allocate<ICTypeUpdate_PrimitiveSet>(code, flags);
    }

    class Compiler : public TypeCheckPrimitiveSetStub::Compiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICTypeUpdate_PrimitiveSet *existingStub, JSValueType type)
          : TypeCheckPrimitiveSetStub::Compiler(cx, TypeUpdate_PrimitiveSet, existingStub, type)
        {}

        ICTypeUpdate_PrimitiveSet *updateStub() {
            TypeCheckPrimitiveSetStub *stub =
                this->TypeCheckPrimitiveSetStub::Compiler::updateStub();
            if (!stub)
                return NULL;
            return stub->toUpdateStub();
        }

        ICTypeUpdate_PrimitiveSet *getStub(ICStubSpace *space) {
            JS_ASSERT(!existingStub_);
            return ICTypeUpdate_PrimitiveSet::New(space, getStubCode(), flags_);
        }
    };
};

// Type update stub to handle a singleton object.
class ICTypeUpdate_SingleObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrObject obj_;

    ICTypeUpdate_SingleObject(IonCode *stubCode, HandleObject obj);

  public:
    static inline ICTypeUpdate_SingleObject *New(ICStubSpace *space, IonCode *code,
                                                 HandleObject obj)
    {
        if (!code)
            return NULL;
        return space->allocate<ICTypeUpdate_SingleObject>(code, obj);
    }

    HeapPtrObject &object() {
        return obj_;
    }

    static size_t offsetOfObject() {
        return offsetof(ICTypeUpdate_SingleObject, obj_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObject obj_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleObject obj)
          : ICStubCompiler(cx, TypeUpdate_SingleObject),
            obj_(obj)
        { }

        ICTypeUpdate_SingleObject *getStub(ICStubSpace *space) {
            return ICTypeUpdate_SingleObject::New(space, getStubCode(), obj_);
        }
    };
};

// Type update stub to handle a single TypeObject.
class ICTypeUpdate_TypeObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrTypeObject type_;

    ICTypeUpdate_TypeObject(IonCode *stubCode, HandleTypeObject type);

  public:
    static inline ICTypeUpdate_TypeObject *New(ICStubSpace *space, IonCode *code,
                                               HandleTypeObject type)
    {
        if (!code)
            return NULL;
        return space->allocate<ICTypeUpdate_TypeObject>(code, type);
    }

    HeapPtrTypeObject &type() {
        return type_;
    }

    static size_t offsetOfType() {
        return offsetof(ICTypeUpdate_TypeObject, type_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleTypeObject type_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleTypeObject type)
          : ICStubCompiler(cx, TypeUpdate_TypeObject),
            type_(type)
        { }

        ICTypeUpdate_TypeObject *getStub(ICStubSpace *space) {
            return ICTypeUpdate_TypeObject::New(space, getStubCode(), type_);
        }
    };
};

// This
//      JSOP_THIS

class ICThis_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICThis_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::This_Fallback, stubCode) {}

  public:
    static inline ICThis_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICThis_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::This_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICThis_Fallback::New(space, getStubCode());
        }
    };
};

class ICNewArray_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICNewArray_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::NewArray_Fallback, stubCode)
    {}

  public:
    static inline ICNewArray_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICNewArray_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::NewArray_Fallback)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICNewArray_Fallback::New(space, getStubCode());
        }
    };
};

class ICNewObject_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICNewObject_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::NewObject_Fallback, stubCode)
    {}

  public:
    static inline ICNewObject_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICNewObject_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::NewObject_Fallback)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICNewObject_Fallback::New(space, getStubCode());
        }
    };
};

// Compare
//      JSOP_LT
//      JSOP_GT

class ICCompare_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICCompare_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::Compare_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICCompare_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::Compare_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Fallback::New(space, getStubCode());
        }
    };
};

class ICCompare_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Int32(IonCode *stubCode)
      : ICStub(ICStub::Compare_Int32, stubCode) {}

  public:
    static inline ICCompare_Int32 *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_Int32>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Int32, op) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Int32::New(space, getStubCode());
        }
    };
};

class ICCompare_Double : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Double(IonCode *stubCode)
      : ICStub(ICStub::Compare_Double, stubCode)
    {}

  public:
    static inline ICCompare_Double *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_Double>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Double, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Double::New(space, getStubCode());
        }
    };
};

class ICCompare_NumberWithUndefined : public ICStub
{
    friend class ICStubSpace;

    ICCompare_NumberWithUndefined(IonCode *stubCode, bool lhsIsUndefined)
      : ICStub(ICStub::Compare_NumberWithUndefined, stubCode)
    {
        extra_ = lhsIsUndefined;
    }

  public:
    static inline ICCompare_NumberWithUndefined *New(ICStubSpace *space, IonCode *code, bool lhsIsUndefined) {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_NumberWithUndefined>(code, lhsIsUndefined);
    }

    bool lhsIsUndefined() {
        return extra_;
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

        bool lhsIsUndefined;

      public:
        Compiler(JSContext *cx, JSOp op, bool lhsIsUndefined)
          : ICMultiStubCompiler(cx, ICStub::Compare_NumberWithUndefined, op),
            lhsIsUndefined(lhsIsUndefined)
        {}

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind)
                 | (static_cast<int32_t>(op) << 16)
                 | (static_cast<int32_t>(lhsIsUndefined) << 24);
        }

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_NumberWithUndefined::New(space, getStubCode(), lhsIsUndefined);
        }
    };
};

class ICCompare_String : public ICStub
{
    friend class ICStubSpace;

    ICCompare_String(IonCode *stubCode)
      : ICStub(ICStub::Compare_String, stubCode)
    {}

  public:
    static inline ICCompare_String *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_String>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_String, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_String::New(space, getStubCode());
        }
    };
};

class ICCompare_Boolean : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Boolean(IonCode *stubCode)
      : ICStub(ICStub::Compare_Boolean, stubCode)
    {}

  public:
    static inline ICCompare_Boolean *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_Boolean>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Boolean, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Boolean::New(space, getStubCode());
        }
    };
};

class ICCompare_Object : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Object(IonCode *stubCode)
      : ICStub(ICStub::Compare_Object, stubCode)
    {}

  public:
    static inline ICCompare_Object *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_Object>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Object, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Object::New(space, getStubCode());
        }
    };
};

class ICCompare_ObjectWithUndefined : public ICStub
{
    friend class ICStubSpace;

    ICCompare_ObjectWithUndefined(IonCode *stubCode)
      : ICStub(ICStub::Compare_ObjectWithUndefined, stubCode)
    {}

  public:
    static inline ICCompare_ObjectWithUndefined *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_ObjectWithUndefined>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

        bool lhsIsUndefined;
        bool compareWithNull;

      public:
        Compiler(JSContext *cx, JSOp op, bool lhsIsUndefined, bool compareWithNull)
          : ICMultiStubCompiler(cx, ICStub::Compare_ObjectWithUndefined, op),
            lhsIsUndefined(lhsIsUndefined),
            compareWithNull(compareWithNull)
        {}

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind)
                 | (static_cast<int32_t>(op) << 16)
                 | (static_cast<int32_t>(lhsIsUndefined) << 24)
                 | (static_cast<int32_t>(compareWithNull) << 25);
        }

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_ObjectWithUndefined::New(space, getStubCode());
        }
    };
};

class ICCompare_Int32WithBoolean : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Int32WithBoolean(IonCode *stubCode, bool lhsIsInt32)
      : ICStub(ICStub::Compare_Int32WithBoolean, stubCode)
    {
        extra_ = lhsIsInt32;
    }

  public:
    static inline ICCompare_Int32WithBoolean *New(ICStubSpace *space, IonCode *code,
                                                  bool lhsIsInt32)
    {
        if (!code)
            return NULL;
        return space->allocate<ICCompare_Int32WithBoolean>(code, lhsIsInt32);
    }

    bool lhsIsInt32() const {
        return extra_;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        JSOp op_;
        bool lhsIsInt32_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return (static_cast<int32_t>(kind) | (static_cast<int32_t>(op_) << 16) |
                    (static_cast<int32_t>(lhsIsInt32_) << 24));
        }

      public:
        Compiler(JSContext *cx, JSOp op, bool lhsIsInt32)
          : ICStubCompiler(cx, ICStub::Compare_Int32WithBoolean),
            op_(op),
            lhsIsInt32_(lhsIsInt32)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Int32WithBoolean::New(space, getStubCode(), lhsIsInt32_);
        }
    };
};

// ToBool
//      JSOP_IFNE

class ICToBool_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICToBool_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::ToBool_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICToBool_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICToBool_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_Fallback::New(space, getStubCode());
        }
    };
};

class ICToBool_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICToBool_Int32(IonCode *stubCode)
      : ICStub(ICStub::ToBool_Int32, stubCode) {}

  public:
    static inline ICToBool_Int32 *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICToBool_Int32>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Int32) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_Int32::New(space, getStubCode());
        }
    };
};

class ICToBool_String : public ICStub
{
    friend class ICStubSpace;

    ICToBool_String(IonCode *stubCode)
      : ICStub(ICStub::ToBool_String, stubCode) {}

  public:
    static inline ICToBool_String *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICToBool_String>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_String) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_String::New(space, getStubCode());
        }
    };
};

class ICToBool_NullUndefined : public ICStub
{
    friend class ICStubSpace;

    ICToBool_NullUndefined(IonCode *stubCode)
      : ICStub(ICStub::ToBool_NullUndefined, stubCode) {}

  public:
    static inline ICToBool_NullUndefined *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICToBool_NullUndefined>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_NullUndefined) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_NullUndefined::New(space, getStubCode());
        }
    };
};

class ICToBool_Double : public ICStub
{
    friend class ICStubSpace;

    ICToBool_Double(IonCode *stubCode)
      : ICStub(ICStub::ToBool_Double, stubCode) {}

  public:
    static inline ICToBool_Double *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICToBool_Double>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Double) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_Double::New(space, getStubCode());
        }
    };
};

class ICToBool_Object : public ICStub
{
    friend class ICStubSpace;

    ICToBool_Object(IonCode *stubCode)
      : ICStub(ICStub::ToBool_Object, stubCode) {}

  public:
    static inline ICToBool_Object *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICToBool_Object>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Object) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_Object::New(space, getStubCode());
        }
    };
};

// ToNumber
//     JSOP_POS

class ICToNumber_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICToNumber_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::ToNumber_Fallback, stubCode) {}

  public:
    static inline ICToNumber_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICToNumber_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToNumber_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToNumber_Fallback::New(space, getStubCode());
        }
    };
};

// BinaryArith
//      JSOP_ADD
//      JSOP_BITAND, JSOP_BITXOR, JSOP_BITOR
//      JSOP_LSH, JSOP_RSH, JSOP_URSH

class ICBinaryArith_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICBinaryArith_Fallback(IonCode *stubCode)
      : ICFallbackStub(BinaryArith_Fallback, stubCode)
    {
        extra_ = 0;
    }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICBinaryArith_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICBinaryArith_Fallback>(code);
    }

    bool sawDoubleResult() {
        return extra_;
    }
    void setSawDoubleResult() {
        extra_ = 1;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::BinaryArith_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_Fallback::New(space, getStubCode());
        }
    };
};

class ICBinaryArith_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_Int32(IonCode *stubCode, bool allowDouble)
      : ICStub(BinaryArith_Int32, stubCode)
    {
        extra_ = allowDouble;
    }

  public:
    static inline ICBinaryArith_Int32 *New(ICStubSpace *space, IonCode *code, bool allowDouble) {
        if (!code)
            return NULL;
        return space->allocate<ICBinaryArith_Int32>(code, allowDouble);
    }
    bool allowDouble() const {
        return extra_;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        JSOp op_;
        bool allowDouble_;

        bool generateStubCode(MacroAssembler &masm);

        // Stub keys shift-stubs need to encode the kind, the JSOp and if we allow doubles.
        virtual int32_t getKey() const {
            return (static_cast<int32_t>(kind) | (static_cast<int32_t>(op_) << 16) |
                    (static_cast<int32_t>(allowDouble_) << 24));
        }

      public:
        Compiler(JSContext *cx, JSOp op, bool allowDouble)
          : ICStubCompiler(cx, ICStub::BinaryArith_Int32),
            op_(op), allowDouble_(allowDouble) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_Int32::New(space, getStubCode(), allowDouble_);
        }
    };
};

class ICBinaryArith_StringConcat : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_StringConcat(IonCode *stubCode)
      : ICStub(BinaryArith_StringConcat, stubCode)
    {}

  public:
    static inline ICBinaryArith_StringConcat *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICBinaryArith_StringConcat>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::BinaryArith_StringConcat)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_StringConcat::New(space, getStubCode());
        }
    };
};

class ICBinaryArith_StringObjectConcat : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_StringObjectConcat(IonCode *stubCode, bool lhsIsString)
      : ICStub(BinaryArith_StringObjectConcat, stubCode)
    {
        extra_ = lhsIsString;
    }

  public:
    static inline ICBinaryArith_StringObjectConcat *New(ICStubSpace *space, IonCode *code,
                                                        bool lhsIsString) {
        if (!code)
            return NULL;
        return space->allocate<ICBinaryArith_StringObjectConcat>(code, lhsIsString);
    }

    bool lhsIsString() const {
        return extra_;
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool lhsIsString_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(lhsIsString_) << 16);
        }

      public:
        Compiler(JSContext *cx, bool lhsIsString)
          : ICStubCompiler(cx, ICStub::BinaryArith_StringObjectConcat),
            lhsIsString_(lhsIsString)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_StringObjectConcat::New(space, getStubCode(), lhsIsString_);
        }
    };
};

class ICBinaryArith_Double : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_Double(IonCode *stubCode)
      : ICStub(BinaryArith_Double, stubCode)
    {}

  public:
    static inline ICBinaryArith_Double *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICBinaryArith_Double>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::BinaryArith_Double, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_Double::New(space, getStubCode());
        }
    };
};

class ICBinaryArith_BooleanWithInt32 : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_BooleanWithInt32(IonCode *stubCode, bool lhsIsBool, bool rhsIsBool)
      : ICStub(BinaryArith_BooleanWithInt32, stubCode)
    {
        JS_ASSERT(lhsIsBool || rhsIsBool);
        extra_ = 0;
        if (lhsIsBool)
            extra_ |= 1;
        if (rhsIsBool)
            extra_ |= 2;
    }

  public:
    static inline ICBinaryArith_BooleanWithInt32 *New(ICStubSpace *space, IonCode *code,
                                                      bool lhsIsBool, bool rhsIsBool) {
        if (!code)
            return NULL;
        return space->allocate<ICBinaryArith_BooleanWithInt32>(code, lhsIsBool, rhsIsBool);
    }

    bool lhsIsBoolean() const {
        return extra_ & 1;
    }

    bool rhsIsBoolean() const {
        return extra_ & 2;
    }

    class Compiler : public ICStubCompiler {
      protected:
        JSOp op_;
        bool lhsIsBool_;
        bool rhsIsBool_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(op_) << 16) |
                   (static_cast<int32_t>(lhsIsBool_) << 24) |
                   (static_cast<int32_t>(rhsIsBool_) << 25);
        }

      public:
        Compiler(JSContext *cx, JSOp op, bool lhsIsBool, bool rhsIsBool)
          : ICStubCompiler(cx, ICStub::BinaryArith_BooleanWithInt32),
            op_(op), lhsIsBool_(lhsIsBool), rhsIsBool_(rhsIsBool)
        {
            JS_ASSERT(op_ == JSOP_ADD || op_ == JSOP_SUB || op_ == JSOP_BITOR ||
                      op_ == JSOP_BITAND || op_ == JSOP_BITXOR);
            JS_ASSERT(lhsIsBool_ || rhsIsBool_);
        }

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_BooleanWithInt32::New(space, getStubCode(),
                                                       lhsIsBool_, rhsIsBool_);
        }
    };
};

class ICBinaryArith_DoubleWithInt32 : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_DoubleWithInt32(IonCode *stubCode, bool lhsIsDouble)
      : ICStub(BinaryArith_DoubleWithInt32, stubCode)
    {
        extra_ = lhsIsDouble;
    }

  public:
    static inline ICBinaryArith_DoubleWithInt32 *New(ICStubSpace *space, IonCode *code,
                                                     bool lhsIsDouble) {
        if (!code)
            return NULL;
        return space->allocate<ICBinaryArith_DoubleWithInt32>(code, lhsIsDouble);
    }

    bool lhsIsDouble() const {
        return extra_;
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool lhsIsDouble_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(op) << 16) |
                   (static_cast<int32_t>(lhsIsDouble_) << 24);
        }

      public:
        Compiler(JSContext *cx, JSOp op, bool lhsIsDouble)
          : ICMultiStubCompiler(cx, ICStub::BinaryArith_DoubleWithInt32, op),
            lhsIsDouble_(lhsIsDouble)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_DoubleWithInt32::New(space, getStubCode(), lhsIsDouble_);
        }
    };
};

// UnaryArith
//     JSOP_BITNOT
//     JSOP_NEG

class ICUnaryArith_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICUnaryArith_Fallback(IonCode *stubCode)
      : ICFallbackStub(UnaryArith_Fallback, stubCode)
    {
        extra_ = 0;
    }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICUnaryArith_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICUnaryArith_Fallback>(code);
    }

    bool sawDoubleResult() {
        return extra_;
    }
    void setSawDoubleResult() {
        extra_ = 1;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::UnaryArith_Fallback)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICUnaryArith_Fallback::New(space, getStubCode());
        }
    };
};

class ICUnaryArith_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICUnaryArith_Int32(IonCode *stubCode)
      : ICStub(UnaryArith_Int32, stubCode)
    {}

  public:
    static inline ICUnaryArith_Int32 *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICUnaryArith_Int32>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::UnaryArith_Int32, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICUnaryArith_Int32::New(space, getStubCode());
        }
    };
};

class ICUnaryArith_Double : public ICStub
{
    friend class ICStubSpace;

    ICUnaryArith_Double(IonCode *stubCode)
      : ICStub(UnaryArith_Double, stubCode)
    {}

  public:
    static inline ICUnaryArith_Double *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICUnaryArith_Double>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::UnaryArith_Double, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICUnaryArith_Double::New(space, getStubCode());
        }
    };
};

// GetElem
//      JSOP_GETELEM

class ICGetElem_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetElem_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 16;

    static inline ICGetElem_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICGetElem_Fallback>(code);
    }

    void noteNonNativeAccess() {
        extra_ = 1;
    }
    bool hasNonNativeAccess() const {
        return extra_;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetElem_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICGetElem_Fallback *stub = ICGetElem_Fallback::New(space, getStubCode());
            if (!stub)
                return NULL;
            if (!stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

class ICGetElemNativeStub : public ICMonitoredStub
{
    HeapPtrShape shape_;
    HeapValue idval_;
    uint32_t offset_;

  protected:
    ICGetElemNativeStub(ICStub::Kind kind, IonCode *stubCode, ICStub *firstMonitorStub,
                        HandleShape shape, HandleValue idval,
                        bool isFixedSlot, uint32_t offset);

    ~ICGetElemNativeStub();

  public:
    HeapPtrShape &shape() {
        return shape_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICGetElemNativeStub, shape_);
    }

    HeapValue &idval() {
        return idval_;
    }
    static size_t offsetOfIdval() {
        return offsetof(ICGetElemNativeStub, idval_);
    }

    uint32_t offset() const {
        return offset_;
    }
    static size_t offsetOfOffset() {
        return offsetof(ICGetElemNativeStub, offset_);
    }

    bool isFixedSlot() const {
        return extra_;
    }
};

class ICGetElem_Native : public ICGetElemNativeStub
{
    friend class ICStubSpace;
    ICGetElem_Native(IonCode *stubCode, ICStub *firstMonitorStub,
                     HandleShape shape, HandleValue idval,
                     bool isFixedSlot, uint32_t offset)
      : ICGetElemNativeStub(ICStub::GetElem_Native, stubCode, firstMonitorStub, shape, idval,
                            isFixedSlot, offset)
    {}

  public:
    static inline ICGetElem_Native *New(ICStubSpace *space, IonCode *code,
                                        ICStub *firstMonitorStub,
                                        HandleShape shape, HandleValue idval,
                                        bool isFixedSlot, uint32_t offset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetElem_Native>(code, firstMonitorStub, shape, idval,
                                                 isFixedSlot, offset);
    }
};

class ICGetElem_NativePrototype : public ICGetElemNativeStub
{
    friend class ICStubSpace;
    HeapPtrObject holder_;
    HeapPtrShape holderShape_;

    ICGetElem_NativePrototype(IonCode *stubCode, ICStub *firstMonitorStub,
                              HandleShape shape, HandleValue idval,
                              bool isFixedSlot, uint32_t offset,
                              HandleObject holder, HandleShape holderShape);

  public:
    static inline ICGetElem_NativePrototype *New(ICStubSpace *space, IonCode *code,
                                                 ICStub *firstMonitorStub,
                                                 HandleShape shape, HandleValue idval,
                                                 bool isFixedSlot, uint32_t offset,
                                                 HandleObject holder, HandleShape holderShape)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetElem_NativePrototype>(code, firstMonitorStub, shape, idval,
                                                          isFixedSlot, offset, holder, holderShape);
    }

    HeapPtrObject &holder() {
        return holder_;
    }
    static size_t offsetOfHolder() {
        return offsetof(ICGetElem_NativePrototype, holder_);
    }

    HeapPtrShape &holderShape() {
        return holderShape_;
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICGetElem_NativePrototype, holderShape_);
    }
};

// Compiler for GetElem_Native and GetElem_NativePrototype stubs.
class ICGetElemNativeCompiler : public ICStubCompiler
{
    ICStub *firstMonitorStub_;
    HandleObject obj_;
    HandleObject holder_;
    HandleValue idval_;
    bool isFixedSlot_;
    uint32_t offset_;

    bool generateStubCode(MacroAssembler &masm);

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
    }

  public:
    ICGetElemNativeCompiler(JSContext *cx, ICStub::Kind kind, ICStub *firstMonitorStub,
                            HandleObject obj, HandleObject holder, HandleValue idval,
                            bool isFixedSlot, uint32_t offset)
      : ICStubCompiler(cx, kind),
        firstMonitorStub_(firstMonitorStub),
        obj_(obj),
        holder_(holder),
        idval_(idval),
        isFixedSlot_(isFixedSlot),
        offset_(offset)
    {}

    ICStub *getStub(ICStubSpace *space) {
        RootedShape shape(cx, obj_->lastProperty());
        if (kind == ICStub::GetElem_Native) {
            JS_ASSERT(obj_ == holder_);
            return ICGetElem_Native::New(space, getStubCode(), firstMonitorStub_, shape, idval_,
                                         isFixedSlot_, offset_);
        }

        JS_ASSERT(obj_ != holder_);
        JS_ASSERT(kind == ICStub::GetElem_NativePrototype);
        RootedShape holderShape(cx, holder_->lastProperty());
        return ICGetElem_NativePrototype::New(space, getStubCode(), firstMonitorStub_, shape,
                                              idval_, isFixedSlot_, offset_, holder_, holderShape);
    }
};

class ICGetElem_String : public ICStub
{
    friend class ICStubSpace;

    ICGetElem_String(IonCode *stubCode)
      : ICStub(ICStub::GetElem_String, stubCode) {}

  public:
    static inline ICGetElem_String *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICGetElem_String>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetElem_String) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetElem_String::New(space, getStubCode());
        }
    };
};

class ICGetElem_Dense : public ICMonitoredStub
{
    friend class ICStubSpace;

    HeapPtrShape shape_;

    ICGetElem_Dense(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape);

  public:
    static inline ICGetElem_Dense *New(ICStubSpace *space, IonCode *code,
                                       ICStub *firstMonitorStub, HandleShape shape)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetElem_Dense>(code, firstMonitorStub, shape);
    }

    static size_t offsetOfShape() {
        return offsetof(ICGetElem_Dense, shape_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }

    class Compiler : public ICStubCompiler {
      ICStub *firstMonitorStub_;
      RootedShape shape_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, Shape *shape)
          : ICStubCompiler(cx, ICStub::GetElem_Dense),
            firstMonitorStub_(firstMonitorStub),
            shape_(cx, shape)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetElem_Dense::New(space, getStubCode(), firstMonitorStub_, shape_);
        }
    };
};

class ICGetElem_TypedArray : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    HeapPtrShape shape_;

    ICGetElem_TypedArray(IonCode *stubCode, HandleShape shape, uint32_t type);

  public:
    static inline ICGetElem_TypedArray *New(ICStubSpace *space, IonCode *code,
                                            HandleShape shape, uint32_t type)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetElem_TypedArray>(code, shape, type);
    }

    static size_t offsetOfShape() {
        return offsetof(ICGetElem_TypedArray, shape_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }

    class Compiler : public ICStubCompiler {
      RootedShape shape_;
      uint32_t type_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(type_) << 16);
        }

      public:
        Compiler(JSContext *cx, Shape *shape, uint32_t type)
          : ICStubCompiler(cx, ICStub::GetElem_TypedArray),
            shape_(cx, shape),
            type_(type)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetElem_TypedArray::New(space, getStubCode(), shape_, type_);
        }
    };
};

class ICGetElem_Arguments : public ICMonitoredStub
{
    friend class ICStubSpace;
  public:
    enum Which { Normal, Strict, Magic };

  private:
    ICGetElem_Arguments(IonCode *stubCode, ICStub *firstMonitorStub, Which which)
      : ICMonitoredStub(ICStub::GetElem_Arguments, stubCode, firstMonitorStub)
    {
        extra_ = static_cast<uint16_t>(which);
    }

  public:
    static inline ICGetElem_Arguments *New(ICStubSpace *space, IonCode *code,
                                           ICStub *firstMonitorStub, Which which)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetElem_Arguments>(code, firstMonitorStub, which);
    }

    Which which() const {
        return static_cast<Which>(extra_);
    }

    class Compiler : public ICStubCompiler {
      ICStub *firstMonitorStub_;
      Which which_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(which_) << 16);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, Which which)
          : ICStubCompiler(cx, ICStub::GetElem_Arguments),
            firstMonitorStub_(firstMonitorStub),
            which_(which)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetElem_Arguments::New(space, getStubCode(), firstMonitorStub_, which_);
        }
    };
};

// SetElem
//      JSOP_SETELEM
//      JSOP_INITELEM

class ICSetElem_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICSetElem_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::SetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICSetElem_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICSetElem_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::SetElem_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICSetElem_Fallback::New(space, getStubCode());
        }
    };
};

class ICSetElem_Dense : public ICUpdatedStub
{
    friend class ICStubSpace;

    HeapPtrShape shape_;
    HeapPtrTypeObject type_;

    ICSetElem_Dense(IonCode *stubCode, HandleShape shape, HandleTypeObject type);

  public:
    static inline ICSetElem_Dense *New(ICStubSpace *space, IonCode *code, HandleShape shape,
                                       HandleTypeObject type) {
        if (!code)
            return NULL;
        return space->allocate<ICSetElem_Dense>(code, shape, type);
    }

    static size_t offsetOfShape() {
        return offsetof(ICSetElem_Dense, shape_);
    }
    static size_t offsetOfType() {
        return offsetof(ICSetElem_Dense, type_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }
    HeapPtrTypeObject &type() {
        return type_;
    }

    class Compiler : public ICStubCompiler {
        RootedShape shape_;

        // Compiler is only live on stack during compilation, it should
        // outlive any RootedTypeObject it's passed.  So it can just
        // use the handle.
        HandleTypeObject type_;

        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, Shape *shape, HandleTypeObject type)
          : ICStubCompiler(cx, ICStub::SetElem_Dense),
            shape_(cx, shape),
            type_(type)
        {}

        ICUpdatedStub *getStub(ICStubSpace *space) {
            ICSetElem_Dense *stub = ICSetElem_Dense::New(space, getStubCode(), shape_, type_);
            if (!stub || !stub->initUpdatingChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

template <size_t ProtoChainDepth> class ICSetElem_DenseAddImpl;

class ICSetElem_DenseAdd : public ICUpdatedStub
{
    friend class ICStubSpace;

  public:
    static const size_t MAX_PROTO_CHAIN_DEPTH = 4;

  protected:
    HeapPtrTypeObject type_;

    ICSetElem_DenseAdd(IonCode *stubCode, types::TypeObject *type, size_t protoChainDepth);

  public:
    static size_t offsetOfType() {
        return offsetof(ICSetElem_DenseAdd, type_);
    }

    HeapPtrTypeObject &type() {
        return type_;
    }
    size_t protoChainDepth() const {
        return extra_;
    }

    template <size_t ProtoChainDepth>
    ICSetElem_DenseAddImpl<ProtoChainDepth> *toImplUnchecked() {
        return static_cast<ICSetElem_DenseAddImpl<ProtoChainDepth> *>(this);
    }

    template <size_t ProtoChainDepth>
    ICSetElem_DenseAddImpl<ProtoChainDepth> *toImpl() {
        JS_ASSERT(ProtoChainDepth == protoChainDepth());
        return toImplUnchecked<ProtoChainDepth>();
    }
};

template <size_t ProtoChainDepth>
class ICSetElem_DenseAddImpl : public ICSetElem_DenseAdd
{
    friend class ICStubSpace;

    static const size_t NumShapes = ProtoChainDepth + 1;
    HeapPtrShape shapes_[NumShapes];

    ICSetElem_DenseAddImpl(IonCode *stubCode, types::TypeObject *type,
                           const AutoShapeVector *shapes)
      : ICSetElem_DenseAdd(stubCode, type, ProtoChainDepth)
    {
        JS_ASSERT(shapes->length() == NumShapes);
        for (size_t i = 0; i < NumShapes; i++)
            shapes_[i].init((*shapes)[i]);
    }

  public:
    static inline ICSetElem_DenseAddImpl *New(ICStubSpace *space, IonCode *code,
                                              types::TypeObject *type,
                                              const AutoShapeVector *shapes)
    {
        if (!code)
            return NULL;
        return space->allocate<ICSetElem_DenseAddImpl<ProtoChainDepth> >(code, type, shapes);
    }

    void traceShapes(JSTracer *trc) {
        for (size_t i = 0; i < NumShapes; i++)
            MarkShape(trc, &shapes_[i], "baseline-setelem-denseadd-stub-shape");
    }
    Shape *shape(size_t i) const {
        JS_ASSERT(i < NumShapes);
        return shapes_[i];
    }
    static size_t offsetOfShape(size_t idx) {
        return offsetof(ICSetElem_DenseAddImpl, shapes_) + idx * sizeof(HeapPtrShape);
    }
};

class ICSetElemDenseAddCompiler : public ICStubCompiler {
    RootedObject obj_;
    size_t protoChainDepth_;

    bool generateStubCode(MacroAssembler &masm);

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(protoChainDepth_) << 16);
    }

  public:
    ICSetElemDenseAddCompiler(JSContext *cx, HandleObject obj, size_t protoChainDepth)
        : ICStubCompiler(cx, ICStub::SetElem_DenseAdd),
          obj_(cx, obj),
          protoChainDepth_(protoChainDepth)
    {}

    template <size_t ProtoChainDepth>
    ICUpdatedStub *getStubSpecific(ICStubSpace *space, const AutoShapeVector *shapes);

    ICUpdatedStub *getStub(ICStubSpace *space);
};

class ICSetElem_TypedArray : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    HeapPtrShape shape_;

    ICSetElem_TypedArray(IonCode *stubCode, HandleShape shape, uint32_t type,
                         bool expectOutOfBounds);

  public:
    static inline ICSetElem_TypedArray *New(ICStubSpace *space, IonCode *code,
                                            HandleShape shape, uint32_t type,
                                            bool expectOutOfBounds)
    {
        if (!code)
            return NULL;
        return space->allocate<ICSetElem_TypedArray>(code, shape, type, expectOutOfBounds);
    }

    uint32_t type() const {
        return extra_ & 0xff;
    }

    bool expectOutOfBounds() const {
        return (extra_ >> 8) & 1;
    }

    static size_t offsetOfShape() {
        return offsetof(ICSetElem_TypedArray, shape_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }

    class Compiler : public ICStubCompiler {
        RootedShape shape_;
        uint32_t type_;
        bool expectOutOfBounds_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(type_) << 16) |
                   (static_cast<int32_t>(expectOutOfBounds_) << 24);
        }

      public:
        Compiler(JSContext *cx, Shape *shape, uint32_t type, bool expectOutOfBounds)
          : ICStubCompiler(cx, ICStub::SetElem_TypedArray),
            shape_(cx, shape),
            type_(type),
            expectOutOfBounds_(expectOutOfBounds)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICSetElem_TypedArray::New(space, getStubCode(), shape_, type_,
                                             expectOutOfBounds_);
        }
    };
};

// In
//      JSOP_IN
class ICIn_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIn_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::In_Fallback, stubCode)
    { }

  public:
    static inline ICIn_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICIn_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::In_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIn_Fallback::New(space, getStubCode());
        }
    };
};

// GetName
//      JSOP_NAME
//      JSOP_CALLNAME
//      JSOP_GETGNAME
//      JSOP_CALLGNAME
class ICGetName_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetName_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetName_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICGetName_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICGetName_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetName_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICGetName_Fallback *stub = ICGetName_Fallback::New(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

// Optimized GETGNAME/CALLGNAME stub.
class ICGetName_Global : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    HeapPtrShape shape_;
    uint32_t slot_;

    ICGetName_Global(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape, uint32_t slot);

  public:
    static inline ICGetName_Global *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                        HandleShape shape, uint32_t slot)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetName_Global>(code, firstMonitorStub, shape, slot);
    }

    HeapPtrShape &shape() {
        return shape_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICGetName_Global, shape_);
    }
    static size_t offsetOfSlot() {
        return offsetof(ICGetName_Global, slot_);
    }

    class Compiler : public ICStubCompiler {
        ICStub *firstMonitorStub_;
        RootedShape shape_;
        uint32_t slot_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, Shape *shape, uint32_t slot)
          : ICStubCompiler(cx, ICStub::GetName_Global),
            firstMonitorStub_(firstMonitorStub),
            shape_(cx, shape),
            slot_(slot)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetName_Global::New(space, getStubCode(), firstMonitorStub_, shape_, slot_);
        }
    };
};

// Optimized GETNAME/CALLNAME stub, making a variable number of hops to get an
// 'own' property off some scope object. Unlike GETPROP on an object's
// prototype, there is no teleporting optimization to take advantage of and
// shape checks are required all along the scope chain.
template <size_t NumHops>
class ICGetName_Scope : public ICMonitoredStub
{
    friend class ICStubSpace;

    static const size_t MAX_HOPS = 6;

    HeapPtrShape shapes_[NumHops + 1];
    uint32_t offset_;

    ICGetName_Scope(IonCode *stubCode, ICStub *firstMonitorStub,
                    AutoShapeVector *shapes, uint32_t offset);

    static Kind GetStubKind() {
        return (Kind) (GetName_Scope0 + NumHops);
    }

  public:
    static inline ICGetName_Scope *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                       AutoShapeVector *shapes, uint32_t offset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetName_Scope<NumHops> >(code, firstMonitorStub, shapes, offset);
    }

    void traceScopes(JSTracer *trc) {
        for (size_t i = 0; i < NumHops + 1; i++)
            MarkShape(trc, &shapes_[i], "baseline-scope-stub-shape");
    }

    static size_t offsetOfShape(size_t index) {
        JS_ASSERT(index <= NumHops);
        return offsetof(ICGetName_Scope, shapes_) + (index * sizeof(HeapPtrShape));
    }
    static size_t offsetOfOffset() {
        return offsetof(ICGetName_Scope, offset_);
    }

    class Compiler : public ICStubCompiler {
        ICStub *firstMonitorStub_;
        AutoShapeVector *shapes_;
        bool isFixedSlot_;
        uint32_t offset_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      protected:
        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub,
                 AutoShapeVector *shapes, bool isFixedSlot, uint32_t offset)
          : ICStubCompiler(cx, GetStubKind()),
            firstMonitorStub_(firstMonitorStub),
            shapes_(shapes),
            isFixedSlot_(isFixedSlot),
            offset_(offset)
        {
        }

        ICStub *getStub(ICStubSpace *space) {
            return ICGetName_Scope::New(space, getStubCode(), firstMonitorStub_, shapes_, offset_);
        }
    };
};

// BindName
//      JSOP_BINDNAME
class ICBindName_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICBindName_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::BindName_Fallback, stubCode)
    { }

  public:
    static inline ICBindName_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICBindName_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::BindName_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICBindName_Fallback::New(space, getStubCode());
        }
    };
};

// GetIntrinsic
//      JSOP_GETINTRINSIC
//      JSOP_CALLINTRINSIC
class ICGetIntrinsic_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetIntrinsic_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetIntrinsic_Fallback, stubCode)
    { }

  public:
    static inline ICGetIntrinsic_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICGetIntrinsic_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetIntrinsic_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICGetIntrinsic_Fallback *stub = ICGetIntrinsic_Fallback::New(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

// Stub that loads the constant result of a GETINTRINSIC operation.
class ICGetIntrinsic_Constant : public ICStub
{
    friend class ICStubSpace;

    HeapValue value_;

    ICGetIntrinsic_Constant(IonCode *stubCode, HandleValue value);
    ~ICGetIntrinsic_Constant();

  public:
    static inline ICGetIntrinsic_Constant *New(ICStubSpace *space, IonCode *code,
                                               HandleValue value)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetIntrinsic_Constant>(code, value);
    }

    HeapValue &value() {
        return value_;
    }
    static size_t offsetOfValue() {
        return offsetof(ICGetIntrinsic_Constant, value_);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

        HandleValue value_;

      public:
        Compiler(JSContext *cx, HandleValue value)
          : ICStubCompiler(cx, ICStub::GetIntrinsic_Constant),
            value_(value)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetIntrinsic_Constant::New(space, getStubCode(), value_);
        }
    };
};

class ICGetProp_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetProp_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetProp_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICGetProp_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_Fallback>(code);
    }

    static const size_t UNOPTIMIZABLE_ACCESS_BIT = 0;
    static const size_t ACCESSED_GETTER_BIT = 1;

    void noteUnoptimizableAccess() {
        extra_ |= (1u << UNOPTIMIZABLE_ACCESS_BIT);
    }
    bool hadUnoptimizableAccess() const {
        return extra_ & (1u << UNOPTIMIZABLE_ACCESS_BIT);
    }

    void noteAccessedGetter() {
        extra_ |= (1u << ACCESSED_GETTER_BIT);
    }
    bool hasAccessedGetter() const {
        return extra_ & (1u << ACCESSED_GETTER_BIT);
    }

    class Compiler : public ICStubCompiler {
      protected:
        uint32_t returnOffset_;
        bool generateStubCode(MacroAssembler &masm);
        bool postGenerateStubCode(MacroAssembler &masm, Handle<IonCode *> code);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICGetProp_Fallback *stub = ICGetProp_Fallback::New(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

// Stub for accessing a dense array's length.
class ICGetProp_ArrayLength : public ICStub
{
    friend class ICStubSpace;

    ICGetProp_ArrayLength(IonCode *stubCode)
      : ICStub(GetProp_ArrayLength, stubCode)
    {}

  public:
    static inline ICGetProp_ArrayLength *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_ArrayLength>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_ArrayLength)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetProp_ArrayLength::New(space, getStubCode());
        }
    };
};

// Stub for accessing a typed array's length.
class ICGetProp_TypedArrayLength : public ICStub
{
    friend class ICStubSpace;

    ICGetProp_TypedArrayLength(IonCode *stubCode)
      : ICStub(GetProp_TypedArrayLength, stubCode)
    {}

  public:
    static inline ICGetProp_TypedArrayLength *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_TypedArrayLength>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_TypedArrayLength)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetProp_TypedArrayLength::New(space, getStubCode());
        }
    };
};

// Stub for accessing a string's length.
class ICGetProp_String : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    // Shape of String.prototype to check for.
    HeapPtrShape stringProtoShape_;

    // Fixed or dynamic slot offset.
    uint32_t offset_;

    ICGetProp_String(IonCode *stubCode, ICStub *firstMonitorStub,
                     HandleShape stringProtoShape, uint32_t offset);

  public:
    static inline ICGetProp_String *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                        HandleShape stringProtoShape, uint32_t offset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_String>(code, firstMonitorStub, stringProtoShape, offset);
    }

    HeapPtrShape &stringProtoShape() {
        return stringProtoShape_;
    }
    static size_t offsetOfStringProtoShape() {
        return offsetof(ICGetProp_String, stringProtoShape_);
    }

    static size_t offsetOfOffset() {
        return offsetof(ICGetProp_String, offset_);
    }

    class Compiler : public ICStubCompiler {
        ICStub *firstMonitorStub_;
        RootedObject stringPrototype_;
        bool isFixedSlot_;
        uint32_t offset_;

        bool generateStubCode(MacroAssembler &masm);

      protected:
        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleObject stringPrototype,
                 bool isFixedSlot, uint32_t offset)
          : ICStubCompiler(cx, ICStub::GetProp_String),
            firstMonitorStub_(firstMonitorStub),
            stringPrototype_(cx, stringPrototype),
            isFixedSlot_(isFixedSlot),
            offset_(offset)
        {}

        ICStub *getStub(ICStubSpace *space) {
            RootedShape stringProtoShape(cx, stringPrototype_->lastProperty());
            return ICGetProp_String::New(space, getStubCode(), firstMonitorStub_,
                                         stringProtoShape, offset_);
        }
    };
};

// Stub for accessing a string's length.
class ICGetProp_StringLength : public ICStub
{
    friend class ICStubSpace;

    ICGetProp_StringLength(IonCode *stubCode)
      : ICStub(GetProp_StringLength, stubCode)
    {}

  public:
    static inline ICGetProp_StringLength *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_StringLength>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_StringLength)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetProp_StringLength::New(space, getStubCode());
        }
    };
};

// Base class for GetProp_Native and GetProp_NativePrototype stubs.
class ICGetPropNativeStub : public ICMonitoredStub
{
    // Object shape (lastProperty).
    HeapPtrShape shape_;

    // Fixed or dynamic slot offset.
    uint32_t offset_;

  protected:
    ICGetPropNativeStub(ICStub::Kind kind, IonCode *stubCode, ICStub *firstMonitorStub,
                        HandleShape shape, uint32_t offset);

  public:
    HeapPtrShape &shape() {
        return shape_;
    }
    uint32_t offset() const {
        return offset_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICGetPropNativeStub, shape_);
    }
    static size_t offsetOfOffset() {
        return offsetof(ICGetPropNativeStub, offset_);
    }
};

// Stub for accessing an own property on a native object.
class ICGetProp_Native : public ICGetPropNativeStub
{
    friend class ICStubSpace;

    ICGetProp_Native(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape,
                     uint32_t offset)
      : ICGetPropNativeStub(GetProp_Native, stubCode, firstMonitorStub, shape, offset)
    {}

  public:
    static inline ICGetProp_Native *New(ICStubSpace *space, IonCode *code,
                                        ICStub *firstMonitorStub, HandleShape shape,
                                        uint32_t offset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_Native>(code, firstMonitorStub, shape, offset);
    }
};

// Stub for accessing a property on a native object's prototype. Note that due to
// the shape teleporting optimization, we only have to guard on the object's shape
// and the holder's shape.
class ICGetProp_NativePrototype : public ICGetPropNativeStub
{
    friend class ICStubSpace;

  protected:
    // Holder and its shape.
    HeapPtrObject holder_;
    HeapPtrShape holderShape_;

    ICGetProp_NativePrototype(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape,
                              uint32_t offset, HandleObject holder, HandleShape holderShape);

  public:
    static inline ICGetProp_NativePrototype *New(ICStubSpace *space, IonCode *code,
                                                 ICStub *firstMonitorStub, HandleShape shape,
                                                 uint32_t offset, HandleObject holder,
                                                 HandleShape holderShape)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_NativePrototype>(code, firstMonitorStub, shape, offset,
                                                          holder, holderShape);
    }

  public:
    HeapPtrObject &holder() {
        return holder_;
    }
    HeapPtrShape &holderShape() {
        return holderShape_;
    }
    static size_t offsetOfHolder() {
        return offsetof(ICGetProp_NativePrototype, holder_);
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICGetProp_NativePrototype, holderShape_);
    }
};


// Compiler for GetProp_Native and GetProp_NativePrototype stubs.
class ICGetPropNativeCompiler : public ICStubCompiler
{
    ICStub *firstMonitorStub_;
    HandleObject obj_;
    HandleObject holder_;
    bool isFixedSlot_;
    uint32_t offset_;

    bool generateStubCode(MacroAssembler &masm);

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
    }

  public:
    ICGetPropNativeCompiler(JSContext *cx, ICStub::Kind kind, ICStub *firstMonitorStub,
                            HandleObject obj, HandleObject holder, bool isFixedSlot,
                            uint32_t offset)
      : ICStubCompiler(cx, kind),
        firstMonitorStub_(firstMonitorStub),
        obj_(obj),
        holder_(holder),
        isFixedSlot_(isFixedSlot),
        offset_(offset)
    {}

    ICStub *getStub(ICStubSpace *space) {
        RootedShape shape(cx, obj_->lastProperty());
        if (kind == ICStub::GetProp_Native) {
            JS_ASSERT(obj_ == holder_);
            return ICGetProp_Native::New(space, getStubCode(), firstMonitorStub_, shape, offset_);
        }

        JS_ASSERT(obj_ != holder_);
        JS_ASSERT(kind == ICStub::GetProp_NativePrototype);
        RootedShape holderShape(cx, holder_->lastProperty());
        return ICGetProp_NativePrototype::New(space, getStubCode(), firstMonitorStub_, shape,
                                              offset_, holder_, holderShape);
    }
};

// Stub for calling a getter (native or scripted) on a native object.
class ICGetPropCallGetter : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    // Object shape (lastProperty).
    HeapPtrShape shape_;

    // Holder and shape.
    HeapPtrObject holder_;
    HeapPtrShape holderShape_;

    // Function to call.
    HeapPtrFunction getter_;

    // PC offset of call
    uint32_t pcOffset_;

    ICGetPropCallGetter(Kind kind, IonCode *stubCode, ICStub *firstMonitorStub,
                         HandleShape shape, HandleObject holder, HandleShape holderShape,
                         HandleFunction getter, uint32_t pcOffset);

  public:
    HeapPtrShape &shape() {
        return shape_;
    }
    HeapPtrObject &holder() {
        return holder_;
    }
    HeapPtrShape &holderShape() {
        return holderShape_;
    }
    HeapPtrFunction &getter() {
        return getter_;
    }

    static size_t offsetOfShape() {
        return offsetof(ICGetPropCallGetter, shape_);
    }
    static size_t offsetOfHolder() {
        return offsetof(ICGetPropCallGetter, holder_);
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICGetPropCallGetter, holderShape_);
    }
    static size_t offsetOfGetter() {
        return offsetof(ICGetPropCallGetter, getter_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICGetPropCallGetter, pcOffset_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        ICStub *firstMonitorStub_;
        RootedObject obj_;
        RootedObject holder_;
        RootedFunction getter_;
        uint32_t pcOffset_;

      public:
        Compiler(JSContext *cx, ICStub::Kind kind, ICStub *firstMonitorStub, HandleObject obj,
                 HandleObject holder, HandleFunction getter, uint32_t pcOffset)
          : ICStubCompiler(cx, kind),
            firstMonitorStub_(firstMonitorStub),
            obj_(cx, obj),
            holder_(cx, holder),
            getter_(cx, getter),
            pcOffset_(pcOffset)
        {
            JS_ASSERT(kind == ICStub::GetProp_CallScripted || kind == ICStub::GetProp_CallNative);
        }
    };
};

// Stub for calling a scripted getter on a native object.
class ICGetProp_CallScripted : public ICGetPropCallGetter
{
    friend class ICStubSpace;

  protected:
    ICGetProp_CallScripted(IonCode *stubCode, ICStub *firstMonitorStub,
                           HandleShape shape, HandleObject holder, HandleShape holderShape,
                           HandleFunction getter, uint32_t pcOffset)
      : ICGetPropCallGetter(GetProp_CallScripted, stubCode, firstMonitorStub,
                            shape, holder, holderShape, getter, pcOffset)
    {}

  public:
    static inline ICGetProp_CallScripted *New(
                ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                HandleShape shape, HandleObject holder, HandleShape holderShape,
                HandleFunction getter, uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_CallScripted>(code, firstMonitorStub,
                                                       shape, holder, holderShape, getter,
                                                       pcOffset);
    }

    class Compiler : public ICGetPropCallGetter::Compiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleObject obj,
                 HandleObject holder, HandleFunction getter, uint32_t pcOffset)
          : ICGetPropCallGetter::Compiler(cx, ICStub::GetProp_CallScripted, firstMonitorStub,
                                          obj, holder, getter, pcOffset)
        {}

        ICStub *getStub(ICStubSpace *space) {
            RootedShape shape(cx, obj_->lastProperty());
            RootedShape holderShape(cx, holder_->lastProperty());
            return ICGetProp_CallScripted::New(space, getStubCode(), firstMonitorStub_, shape,
                                               holder_, holderShape, getter_, pcOffset_);
        }
    };
};

// Stub for calling a native getter on a native object.
class ICGetProp_CallNative : public ICGetPropCallGetter
{
    friend class ICStubSpace;

  protected:
    ICGetProp_CallNative(IonCode *stubCode, ICStub *firstMonitorStub,
                         HandleShape shape, HandleObject holder, HandleShape holderShape,
                         HandleFunction getter, uint32_t pcOffset)
      : ICGetPropCallGetter(GetProp_CallNative, stubCode, firstMonitorStub,
                            shape, holder, holderShape, getter, pcOffset)
    {}

  public:
    static inline ICGetProp_CallNative *New(
                ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                HandleShape shape, HandleObject holder, HandleShape holderShape,
                HandleFunction getter, uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_CallNative>(code, firstMonitorStub,
                                                     shape, holder, holderShape, getter,
                                                     pcOffset);
    }

    class Compiler : public ICGetPropCallGetter::Compiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleObject obj,
                 HandleObject holder, HandleFunction getter, uint32_t pcOffset)
          : ICGetPropCallGetter::Compiler(cx, ICStub::GetProp_CallNative, firstMonitorStub,
                                          obj, holder, getter, pcOffset)
        {}

        ICStub *getStub(ICStubSpace *space) {
            RootedShape shape(cx, obj_->lastProperty());
            RootedShape holderShape(cx, holder_->lastProperty());
            return ICGetProp_CallNative::New(space, getStubCode(), firstMonitorStub_, shape,
                                               holder_, holderShape, getter_, pcOffset_);
        }
    };
};

class ICGetPropCallDOMProxyNativeStub : public ICMonitoredStub
{
  friend class ICStubSpace;
  protected:
    // Shape of the DOMProxy
    HeapPtrShape shape_;

    // Proxy handler to check against.
    BaseProxyHandler *proxyHandler_;

    // Object shape of expected expando object. (NULL if no expando object should be there)
    HeapPtrShape expandoShape_;

    // Holder and its shape.
    HeapPtrObject holder_;
    HeapPtrShape holderShape_;

    // Function to call.
    HeapPtrFunction getter_;

    // PC offset of call
    uint32_t pcOffset_;

    ICGetPropCallDOMProxyNativeStub(ICStub::Kind kind, IonCode *stubCode,
                                    ICStub *firstMonitorStub, HandleShape shape,
                                    BaseProxyHandler *proxyHandler, HandleShape expandoShape,
                                    HandleObject holder, HandleShape holderShape,
                                    HandleFunction getter, uint32_t pcOffset);

  public:
    HeapPtrShape &shape() {
        return shape_;
    }
    HeapPtrShape &expandoShape() {
        return expandoShape_;
    }
    HeapPtrObject &holder() {
        return holder_;
    }
    HeapPtrShape &holderShape() {
        return holderShape_;
    }
    HeapPtrFunction &getter() {
        return getter_;
    }
    uint32_t pcOffset() const {
        return pcOffset_;
    }

    static size_t offsetOfShape() {
        return offsetof(ICGetPropCallDOMProxyNativeStub, shape_);
    }
    static size_t offsetOfProxyHandler() {
        return offsetof(ICGetPropCallDOMProxyNativeStub, proxyHandler_);
    }
    static size_t offsetOfExpandoShape() {
        return offsetof(ICGetPropCallDOMProxyNativeStub, expandoShape_);
    }
    static size_t offsetOfHolder() {
        return offsetof(ICGetPropCallDOMProxyNativeStub, holder_);
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICGetPropCallDOMProxyNativeStub, holderShape_);
    }
    static size_t offsetOfGetter() {
        return offsetof(ICGetPropCallDOMProxyNativeStub, getter_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICGetPropCallDOMProxyNativeStub, pcOffset_);
    }
};

class ICGetProp_CallDOMProxyNative : public ICGetPropCallDOMProxyNativeStub
{
    friend class ICStubSpace;
    ICGetProp_CallDOMProxyNative(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape,
                                 BaseProxyHandler *proxyHandler, HandleShape expandoShape,
                                 HandleObject holder, HandleShape holderShape,
                                 HandleFunction getter, uint32_t pcOffset)
      : ICGetPropCallDOMProxyNativeStub(ICStub::GetProp_CallDOMProxyNative, stubCode,
                                        firstMonitorStub, shape, proxyHandler, expandoShape,
                                        holder, holderShape, getter, pcOffset)
    {}

  public:
    static inline ICGetProp_CallDOMProxyNative *New(
            ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
            HandleShape shape, BaseProxyHandler *proxyHandler,
            HandleShape expandoShape, HandleObject holder, HandleShape holderShape,
            HandleFunction getter, uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_CallDOMProxyNative>(code, firstMonitorStub, shape,
                                                   proxyHandler, expandoShape, holder,
                                                   holderShape, getter, pcOffset);
    }
};

class ICGetProp_CallDOMProxyWithGenerationNative : public ICGetPropCallDOMProxyNativeStub
{
  protected:
    ExpandoAndGeneration *expandoAndGeneration_;
    uint32_t generation_;

  public:
    ICGetProp_CallDOMProxyWithGenerationNative(IonCode *stubCode, ICStub *firstMonitorStub,
                                               HandleShape shape, BaseProxyHandler *proxyHandler,
                                               ExpandoAndGeneration *expandoAndGeneration,
                                               uint32_t generation, HandleShape expandoShape,
                                               HandleObject holder, HandleShape holderShape,
                                               HandleFunction getter, uint32_t pcOffset)
      : ICGetPropCallDOMProxyNativeStub(ICStub::GetProp_CallDOMProxyWithGenerationNative,
                                        stubCode, firstMonitorStub, shape, proxyHandler,
                                        expandoShape, holder, holderShape, getter, pcOffset),
        expandoAndGeneration_(expandoAndGeneration),
        generation_(generation)
    {
    }

    static inline ICGetProp_CallDOMProxyWithGenerationNative *New(
            ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
            HandleShape shape, BaseProxyHandler *proxyHandler,
            ExpandoAndGeneration *expandoAndGeneration, uint32_t generation,
            HandleShape expandoShape, HandleObject holder, HandleShape holderShape,
            HandleFunction getter, uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_CallDOMProxyWithGenerationNative>(code, firstMonitorStub,
                                                   shape, proxyHandler, expandoAndGeneration,
                                                   generation, expandoShape, holder, holderShape,
                                                   getter, pcOffset);
    }

    void *expandoAndGeneration() const {
        return expandoAndGeneration_;
    }
    uint32_t generation() const {
        return generation_;
    }

    void setGeneration(uint32_t value) {
        generation_ = value;
    }

    static size_t offsetOfInternalStruct() {
        return offsetof(ICGetProp_CallDOMProxyWithGenerationNative, expandoAndGeneration_);
    }
    static size_t offsetOfGeneration() {
        return offsetof(ICGetProp_CallDOMProxyWithGenerationNative, generation_);
    }
};

class ICGetPropCallDOMProxyNativeCompiler : public ICStubCompiler {
    ICStub *firstMonitorStub_;
    Rooted<ProxyObject*> proxy_;
    RootedObject holder_;
    RootedFunction getter_;
    uint32_t pcOffset_;

    bool generateStubCode(MacroAssembler &masm, Address* internalStructAddr,
                          Address* generationAddr);
    bool generateStubCode(MacroAssembler &masm);

  public:
    ICGetPropCallDOMProxyNativeCompiler(JSContext *cx, ICStub::Kind kind,
                                        ICStub *firstMonitorStub, Handle<ProxyObject*> proxy,
                                        HandleObject holder, HandleFunction getter,
                                        uint32_t pcOffset);

    ICStub *getStub(ICStubSpace *space);
};

class ICGetProp_DOMProxyShadowed : public ICMonitoredStub
{
  friend class ICStubSpace;
  protected:
    HeapPtrShape shape_;
    BaseProxyHandler *proxyHandler_;
    HeapPtrPropertyName name_;
    uint32_t pcOffset_;

    ICGetProp_DOMProxyShadowed(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape,
                               BaseProxyHandler *proxyHandler, HandlePropertyName name,
                               uint32_t pcOffset);

  public:
    static inline ICGetProp_DOMProxyShadowed *New(ICStubSpace *space, IonCode *code,
                                                  ICStub *firstMonitorStub, HandleShape shape,
                                                  BaseProxyHandler *proxyHandler,
                                                  HandlePropertyName name, uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_DOMProxyShadowed>(code, firstMonitorStub, shape,
                                                           proxyHandler, name, pcOffset);
    }

    HeapPtrShape &shape() {
        return shape_;
    }
    HeapPtrPropertyName &name() {
        return name_;
    }

    static size_t offsetOfShape() {
        return offsetof(ICGetProp_DOMProxyShadowed, shape_);
    }
    static size_t offsetOfProxyHandler() {
        return offsetof(ICGetProp_DOMProxyShadowed, proxyHandler_);
    }
    static size_t offsetOfName() {
        return offsetof(ICGetProp_DOMProxyShadowed, name_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICGetProp_DOMProxyShadowed, pcOffset_);
    }

    class Compiler : public ICStubCompiler {
        ICStub *firstMonitorStub_;
        Rooted<ProxyObject*> proxy_;
        RootedPropertyName name_;
        uint32_t pcOffset_;

        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, Handle<ProxyObject*> proxy,
                 HandlePropertyName name, uint32_t pcOffset)
          : ICStubCompiler(cx, ICStub::GetProp_CallNative),
            firstMonitorStub_(firstMonitorStub),
            proxy_(cx, proxy),
            name_(cx, name),
            pcOffset_(pcOffset)
        {}

        ICStub *getStub(ICStubSpace *space);
    };
};

class ICGetProp_ArgumentsLength : public ICStub
{
  friend class ICStubSpace;
  public:
    enum Which { Normal, Strict, Magic };

  protected:
    ICGetProp_ArgumentsLength(IonCode *stubCode)
      : ICStub(ICStub::GetProp_ArgumentsLength, stubCode)
    { }

  public:
    static inline ICGetProp_ArgumentsLength *New(ICStubSpace *space, IonCode *code)
    {
        if (!code)
            return NULL;
        return space->allocate<ICGetProp_ArgumentsLength>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        Which which_;

        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(which_) << 16);
        }

      public:
        Compiler(JSContext *cx, Which which)
          : ICStubCompiler(cx, ICStub::GetProp_ArgumentsLength),
            which_(which)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetProp_ArgumentsLength::New(space, getStubCode());
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

    ICSetProp_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::SetProp_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICSetProp_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICSetProp_Fallback>(code);
    }

    static const size_t UNOPTIMIZABLE_ACCESS_BIT = 0;
    void noteUnoptimizableAccess() {
        extra_ |= (1u << UNOPTIMIZABLE_ACCESS_BIT);
    }
    bool hadUnoptimizableAccess() const {
        return extra_ & (1u << UNOPTIMIZABLE_ACCESS_BIT);
    }

    class Compiler : public ICStubCompiler {
      protected:
        uint32_t returnOffset_;
        bool generateStubCode(MacroAssembler &masm);
        bool postGenerateStubCode(MacroAssembler &masm, Handle<IonCode *> code);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::SetProp_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICSetProp_Fallback::New(space, getStubCode());
        }
    };
};

// Optimized SETPROP/SETGNAME/SETNAME stub.
class ICSetProp_Native : public ICUpdatedStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    HeapPtrTypeObject type_;
    HeapPtrShape shape_;
    uint32_t offset_;

    ICSetProp_Native(IonCode *stubCode, HandleTypeObject type, HandleShape shape, uint32_t offset);

  public:
    static inline ICSetProp_Native *New(ICStubSpace *space, IonCode *code, HandleTypeObject type,
                                        HandleShape shape, uint32_t offset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICSetProp_Native>(code, type, shape, offset);
    }
    HeapPtrTypeObject &type() {
        return type_;
    }
    HeapPtrShape &shape() {
        return shape_;
    }
    static size_t offsetOfType() {
        return offsetof(ICSetProp_Native, type_);
    }
    static size_t offsetOfShape() {
        return offsetof(ICSetProp_Native, shape_);
    }
    static size_t offsetOfOffset() {
        return offsetof(ICSetProp_Native, offset_);
    }

    class Compiler : public ICStubCompiler {
        RootedObject obj_;
        bool isFixedSlot_;
        uint32_t offset_;

      protected:
        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
        }

        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleObject obj, bool isFixedSlot, uint32_t offset)
          : ICStubCompiler(cx, ICStub::SetProp_Native),
            obj_(cx, obj),
            isFixedSlot_(isFixedSlot),
            offset_(offset)
        {}

        ICUpdatedStub *getStub(ICStubSpace *space);
    };
};


template <size_t ProtoChainDepth> class ICSetProp_NativeAddImpl;

class ICSetProp_NativeAdd : public ICUpdatedStub
{
  public:
    static const size_t MAX_PROTO_CHAIN_DEPTH = 4;

  protected: // Protected to silence Clang warning.
    HeapPtrTypeObject type_;
    HeapPtrShape newShape_;
    uint32_t offset_;

    ICSetProp_NativeAdd(IonCode *stubCode, HandleTypeObject type, size_t protoChainDepth,
                        HandleShape newShape, uint32_t offset);

  public:
    size_t protoChainDepth() const {
        return extra_;
    }
    HeapPtrTypeObject &type() {
        return type_;
    }
    HeapPtrShape &newShape() {
        return newShape_;
    }

    template <size_t ProtoChainDepth>
    ICSetProp_NativeAddImpl<ProtoChainDepth> *toImpl() {
        JS_ASSERT(ProtoChainDepth == protoChainDepth());
        return static_cast<ICSetProp_NativeAddImpl<ProtoChainDepth> *>(this);
    }

    static size_t offsetOfType() {
        return offsetof(ICSetProp_NativeAdd, type_);
    }
    static size_t offsetOfNewShape() {
        return offsetof(ICSetProp_NativeAdd, newShape_);
    }
    static size_t offsetOfOffset() {
        return offsetof(ICSetProp_NativeAdd, offset_);
    }
};

template <size_t ProtoChainDepth>
class ICSetProp_NativeAddImpl : public ICSetProp_NativeAdd
{
    friend class ICStubSpace;

    static const size_t NumShapes = ProtoChainDepth + 1;
    HeapPtrShape shapes_[NumShapes];

    ICSetProp_NativeAddImpl(IonCode *stubCode, HandleTypeObject type,
                            const AutoShapeVector *shapes,
                            HandleShape newShape, uint32_t offset);

  public:
    static inline ICSetProp_NativeAddImpl *New(
            ICStubSpace *space, IonCode *code, HandleTypeObject type,
            const AutoShapeVector *shapes, HandleShape newShape, uint32_t offset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICSetProp_NativeAddImpl<ProtoChainDepth> >(
                            code, type, shapes, newShape, offset);
    }

    void traceShapes(JSTracer *trc) {
        for (size_t i = 0; i < NumShapes; i++)
            MarkShape(trc, &shapes_[i], "baseline-setpropnativeadd-stub-shape");
    }

    static size_t offsetOfShape(size_t idx) {
        return offsetof(ICSetProp_NativeAddImpl, shapes_) + (idx * sizeof(HeapPtrShape));
    }
};

class ICSetPropNativeAddCompiler : public ICStubCompiler {
    RootedObject obj_;
    RootedShape oldShape_;
    size_t protoChainDepth_;
    bool isFixedSlot_;
    uint32_t offset_;

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16) |
               (static_cast<int32_t>(protoChainDepth_) << 20);
    }

    bool generateStubCode(MacroAssembler &masm);

  public:
    ICSetPropNativeAddCompiler(JSContext *cx, HandleObject obj, HandleShape oldShape,
                               size_t protoChainDepth, bool isFixedSlot, uint32_t offset);

    template <size_t ProtoChainDepth>
    ICUpdatedStub *getStubSpecific(ICStubSpace *space, const AutoShapeVector *shapes)
    {
        RootedTypeObject type(cx, obj_->getType(cx));
        RootedShape newShape(cx, obj_->lastProperty());

        return ICSetProp_NativeAddImpl<ProtoChainDepth>::New(
                    space, getStubCode(), type, shapes, newShape, offset_);
    }

    ICUpdatedStub *getStub(ICStubSpace *space);
};

// Base stub for calling a setters on a native object.
class ICSetPropCallSetter : public ICStub
{
    friend class ICStubSpace;

  protected:
    // Object shape (lastProperty).
    HeapPtrShape shape_;

    // Holder and shape.
    HeapPtrObject holder_;
    HeapPtrShape holderShape_;

    // Function to call.
    HeapPtrFunction setter_;

    // PC of call, for profiler
    uint32_t pcOffset_;

    ICSetPropCallSetter(Kind kind, IonCode *stubCode, HandleShape shape, HandleObject holder,
                        HandleShape holderShape, HandleFunction setter, uint32_t pcOffset);

  public:
    HeapPtrShape &shape() {
        return shape_;
    }
    HeapPtrObject &holder() {
        return holder_;
    }
    HeapPtrShape &holderShape() {
        return holderShape_;
    }
    HeapPtrFunction &setter() {
        return setter_;
    }

    static size_t offsetOfShape() {
        return offsetof(ICSetPropCallSetter, shape_);
    }
    static size_t offsetOfHolder() {
        return offsetof(ICSetPropCallSetter, holder_);
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICSetPropCallSetter, holderShape_);
    }
    static size_t offsetOfSetter() {
        return offsetof(ICSetPropCallSetter, setter_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICSetPropCallSetter, pcOffset_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        RootedObject obj_;
        RootedObject holder_;
        RootedFunction setter_;
        uint32_t pcOffset_;

      public:
        Compiler(JSContext *cx, ICStub::Kind kind, HandleObject obj, HandleObject holder,
                 HandleFunction setter, uint32_t pcOffset)
          : ICStubCompiler(cx, kind),
            obj_(cx, obj),
            holder_(cx, holder),
            setter_(cx, setter),
            pcOffset_(pcOffset)
        {
            JS_ASSERT(kind == ICStub::SetProp_CallScripted || kind == ICStub::SetProp_CallNative);
        }
    };
};

// Stub for calling a scripted setter on a native object.
class ICSetProp_CallScripted : public ICSetPropCallSetter
{
    friend class ICStubSpace;

  protected:
    ICSetProp_CallScripted(IonCode *stubCode, HandleShape shape, HandleObject holder,
                           HandleShape holderShape, HandleFunction setter, uint32_t pcOffset)
      : ICSetPropCallSetter(SetProp_CallScripted, stubCode, shape, holder, holderShape,
                            setter, pcOffset)
    {}

  public:
    static inline ICSetProp_CallScripted *New(ICStubSpace *space, IonCode *code,
                                              HandleShape shape, HandleObject holder,
                                              HandleShape holderShape, HandleFunction setter,
                                              uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICSetProp_CallScripted>(code, shape, holder, holderShape, setter,
                                                       pcOffset);
    }

    class Compiler : public ICSetPropCallSetter::Compiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleObject obj, HandleObject holder, HandleFunction setter,
                 uint32_t pcOffset)
          : ICSetPropCallSetter::Compiler(cx, ICStub::SetProp_CallScripted,
                                          obj, holder, setter, pcOffset)
        {}

        ICStub *getStub(ICStubSpace *space) {
            RootedShape shape(cx, obj_->lastProperty());
            RootedShape holderShape(cx, holder_->lastProperty());
            return ICSetProp_CallScripted::New(space, getStubCode(), shape, holder_, holderShape,
                                               setter_, pcOffset_);
        }
    };
};

// Stub for calling a native setter on a native object.
class ICSetProp_CallNative : public ICSetPropCallSetter
{
    friend class ICStubSpace;

  protected:
    ICSetProp_CallNative(IonCode *stubCode, HandleShape shape, HandleObject holder,
                           HandleShape holderShape, HandleFunction setter, uint32_t pcOffset)
      : ICSetPropCallSetter(SetProp_CallNative, stubCode, shape, holder, holderShape,
                            setter, pcOffset)
    {}

  public:
    static inline ICSetProp_CallNative *New(ICStubSpace *space, IonCode *code,
                                            HandleShape shape, HandleObject holder,
                                            HandleShape holderShape, HandleFunction setter,
                                            uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICSetProp_CallNative>(code, shape, holder, holderShape, setter,
                                                     pcOffset);
    }

    class Compiler : public ICSetPropCallSetter::Compiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleObject obj, HandleObject holder, HandleFunction setter,
                 uint32_t pcOffset)
          : ICSetPropCallSetter::Compiler(cx, ICStub::SetProp_CallNative,
                                          obj, holder, setter, pcOffset)
        {}

        ICStub *getStub(ICStubSpace *space) {
            RootedShape shape(cx, obj_->lastProperty());
            RootedShape holderShape(cx, holder_->lastProperty());
            return ICSetProp_CallNative::New(space, getStubCode(), shape, holder_, holderShape,
                                               setter_, pcOffset_);
        }
    };
};

// Call
//      JSOP_CALL
//      JSOP_FUNAPPLY
//      JSOP_FUNCALL
//      JSOP_NEW

class ICCallStubCompiler : public ICStubCompiler
{
  protected:
    ICCallStubCompiler(JSContext *cx, ICStub::Kind kind)
      : ICStubCompiler(cx, kind)
    { }

    void pushCallArguments(MacroAssembler &masm, GeneralRegisterSet regs, Register argcReg);
    Register guardFunApply(MacroAssembler &masm, GeneralRegisterSet regs, Register argcReg,
                           bool checkNative, Label *failure);
    void pushCallerArguments(MacroAssembler &masm, GeneralRegisterSet regs);
};

class ICCall_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;
  public:
    static const unsigned CONSTRUCTING_FLAG = 0x0001;

    static const uint32_t MAX_OPTIMIZED_STUBS = 16;
    static const uint32_t MAX_SCRIPTED_STUBS = 7;
    static const uint32_t MAX_NATIVE_STUBS = 7;
  private:

    ICCall_Fallback(IonCode *stubCode, bool isConstructing)
      : ICMonitoredFallbackStub(ICStub::Call_Fallback, stubCode)
    {
        extra_ = 0;
        if (isConstructing)
            extra_ |= CONSTRUCTING_FLAG;
    }

  public:

    static inline ICCall_Fallback *New(ICStubSpace *space, IonCode *code, bool isConstructing)
    {
        if (!code)
            return NULL;
        return space->allocate<ICCall_Fallback>(code, isConstructing);
    }

    bool isConstructing() const {
        return extra_ & CONSTRUCTING_FLAG;
    }

    unsigned scriptedStubCount() const {
        return numStubsWithKind(Call_Scripted);
    }
    bool scriptedStubsAreGeneralized() const {
        return hasStub(Call_AnyScripted);
    }

    unsigned nativeStubCount() const {
        return numStubsWithKind(Call_Native);
    }
    bool nativeStubsAreGeneralized() const {
        // Return hasStub(Call_AnyNative) after Call_AnyNative stub is added.
        return false;
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        bool isConstructing_;
        uint32_t returnOffset_;
        bool generateStubCode(MacroAssembler &masm);
        bool postGenerateStubCode(MacroAssembler &masm, Handle<IonCode *> code);

      public:
        Compiler(JSContext *cx, bool isConstructing)
          : ICCallStubCompiler(cx, ICStub::Call_Fallback),
            isConstructing_(isConstructing)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICCall_Fallback *stub = ICCall_Fallback::New(space, getStubCode(), isConstructing_);
            if (!stub || !stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

class ICCall_Scripted : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    HeapPtrScript calleeScript_;
    uint32_t pcOffset_;

    ICCall_Scripted(IonCode *stubCode, ICStub *firstMonitorStub, HandleScript calleeScript,
                    uint32_t pcOffset);

  public:
    static inline ICCall_Scripted *New(
            ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub, HandleScript calleeScript,
            uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICCall_Scripted>(code, firstMonitorStub, calleeScript, pcOffset);
    }

    HeapPtrScript &calleeScript() {
        return calleeScript_;
    }

    static size_t offsetOfCalleeScript() {
        return offsetof(ICCall_Scripted, calleeScript_);
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

    ICCall_AnyScripted(IonCode *stubCode, ICStub *firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_AnyScripted, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    { }

  public:
    static inline ICCall_AnyScripted *New(ICStubSpace *space, IonCode *code,
                                          ICStub *firstMonitorStub, uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICCall_AnyScripted>(code, firstMonitorStub, pcOffset);
    }

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_AnyScripted, pcOffset_);
    }
};

// Compiler for Call_Scripted and Call_AnyScripted stubs.
class ICCallScriptedCompiler : public ICCallStubCompiler {
  protected:
    ICStub *firstMonitorStub_;
    bool isConstructing_;
    RootedScript calleeScript_;
    uint32_t pcOffset_;
    bool generateStubCode(MacroAssembler &masm);

    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(isConstructing_) << 16);
    }

  public:
    ICCallScriptedCompiler(JSContext *cx, ICStub *firstMonitorStub, HandleScript calleeScript,
                            bool isConstructing, uint32_t pcOffset)
      : ICCallStubCompiler(cx, ICStub::Call_Scripted),
        firstMonitorStub_(firstMonitorStub),
        isConstructing_(isConstructing),
        calleeScript_(cx, calleeScript),
        pcOffset_(pcOffset)
    { }

    ICCallScriptedCompiler(JSContext *cx, ICStub *firstMonitorStub, bool isConstructing,
                           uint32_t pcOffset)
      : ICCallStubCompiler(cx, ICStub::Call_AnyScripted),
        firstMonitorStub_(firstMonitorStub),
        isConstructing_(isConstructing),
        calleeScript_(cx, NULL),
        pcOffset_(pcOffset)
    { }

    ICStub *getStub(ICStubSpace *space) {
        if (calleeScript_) {
            return ICCall_Scripted::New(space, getStubCode(), firstMonitorStub_, calleeScript_,
                                        pcOffset_);
        }
        return ICCall_AnyScripted::New(space, getStubCode(), firstMonitorStub_, pcOffset_);
    }
};

class ICCall_Native : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    HeapPtrFunction callee_;
    uint32_t pcOffset_;

    ICCall_Native(IonCode *stubCode, ICStub *firstMonitorStub, HandleFunction callee,
                  uint32_t pcOffset);

  public:
    static inline ICCall_Native *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                     HandleFunction callee, uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICCall_Native>(code, firstMonitorStub, callee, pcOffset);
    }

    HeapPtrFunction &callee() {
        return callee_;
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Native, callee_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_Native, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub *firstMonitorStub_;
        bool isConstructing_;
        RootedFunction callee_;
        uint32_t pcOffset_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isConstructing_) << 16);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleFunction callee,
                 bool isConstructing, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_Native),
            firstMonitorStub_(firstMonitorStub),
            isConstructing_(isConstructing),
            callee_(cx, callee),
            pcOffset_(pcOffset)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICCall_Native::New(space, getStubCode(), firstMonitorStub_, callee_, pcOffset_);
        }
    };
};

class ICCall_ScriptedApplyArguments : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;

    ICCall_ScriptedApplyArguments(IonCode *stubCode, ICStub *firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_ScriptedApplyArguments, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    {}

  public:
    static inline ICCall_ScriptedApplyArguments *New(ICStubSpace *space, IonCode *code,
                                                     ICStub *firstMonitorStub, uint32_t pcOffset)
    {
        if (!code)
            return NULL;
        return space->allocate<ICCall_ScriptedApplyArguments>(code, firstMonitorStub, pcOffset);
    }

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ScriptedApplyArguments, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub *firstMonitorStub_;
        uint32_t pcOffset_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_ScriptedApplyArguments),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICCall_ScriptedApplyArguments::New(space, getStubCode(), firstMonitorStub_,
                                                      pcOffset_);
        }
    };
};

// Stub for performing a TableSwitch, updating the IC's return address to jump
// to whatever point the switch is branching to.
class ICTableSwitch : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    void **table_;
    int32_t min_;
    int32_t length_;
    void *defaultTarget_;

    ICTableSwitch(IonCode *stubCode, void **table,
                  int32_t min, int32_t length, void *defaultTarget)
      : ICStub(TableSwitch, stubCode), table_(table),
        min_(min), length_(length), defaultTarget_(defaultTarget)
    {}

  public:
    static inline ICTableSwitch *New(ICStubSpace *space, IonCode *code, void **table,
                                     int32_t min, int32_t length, void *defaultTarget) {
        if (!code)
            return NULL;
        return space->allocate<ICTableSwitch>(code, table, min, length, defaultTarget);
    }

    void fixupJumpTable(HandleScript script, BaselineScript *baseline);

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

        jsbytecode *pc_;

      public:
        Compiler(JSContext *cx, jsbytecode *pc)
          : ICStubCompiler(cx, ICStub::TableSwitch), pc_(pc)
        {}

        ICStub *getStub(ICStubSpace *space);
    };
};

// IC for constructing an iterator from an input value.
class ICIteratorNew_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIteratorNew_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::IteratorNew_Fallback, stubCode)
    { }

  public:
    static inline ICIteratorNew_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICIteratorNew_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorNew_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorNew_Fallback::New(space, getStubCode());
        }
    };
};

// IC for testing if there are more values in an iterator.
class ICIteratorMore_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIteratorMore_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::IteratorMore_Fallback, stubCode)
    { }

  public:
    static inline ICIteratorMore_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICIteratorMore_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorMore_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorMore_Fallback::New(space, getStubCode());
        }
    };
};

// IC for testing if there are more values in a native iterator.
class ICIteratorMore_Native : public ICStub
{
    friend class ICStubSpace;

    ICIteratorMore_Native(IonCode *stubCode)
      : ICStub(ICStub::IteratorMore_Native, stubCode)
    { }

  public:
    static inline ICIteratorMore_Native *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICIteratorMore_Native>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorMore_Native)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorMore_Native::New(space, getStubCode());
        }
    };
};

// IC for getting the next value in an iterator.
class ICIteratorNext_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIteratorNext_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::IteratorNext_Fallback, stubCode)
    { }

  public:
    static inline ICIteratorNext_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICIteratorNext_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorNext_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorNext_Fallback::New(space, getStubCode());
        }
    };
};

// IC for getting the next value in a native iterator.
class ICIteratorNext_Native : public ICStub
{
    friend class ICStubSpace;

    ICIteratorNext_Native(IonCode *stubCode)
      : ICStub(ICStub::IteratorNext_Native, stubCode)
    { }

  public:
    static inline ICIteratorNext_Native *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICIteratorNext_Native>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorNext_Native)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorNext_Native::New(space, getStubCode());
        }
    };
};

// IC for closing an iterator.
class ICIteratorClose_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIteratorClose_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::IteratorClose_Fallback, stubCode)
    { }

  public:
    static inline ICIteratorClose_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICIteratorClose_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorClose_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorClose_Fallback::New(space, getStubCode());
        }
    };
};

// InstanceOf
//      JSOP_INSTANCEOF
class ICInstanceOf_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICInstanceOf_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::InstanceOf_Fallback, stubCode)
    { }

  public:
    static inline ICInstanceOf_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICInstanceOf_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::InstanceOf_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICInstanceOf_Fallback::New(space, getStubCode());
        }
    };
};

// TypeOf
//      JSOP_TYPEOF
//      JSOP_TYPEOFEXPR
class ICTypeOf_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICTypeOf_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::TypeOf_Fallback, stubCode)
    { }

  public:
    static inline ICTypeOf_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICTypeOf_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::TypeOf_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICTypeOf_Fallback::New(space, getStubCode());
        }
    };
};

class ICTypeOf_Typed : public ICFallbackStub
{
    friend class ICStubSpace;

    ICTypeOf_Typed(IonCode *stubCode, JSType type)
      : ICFallbackStub(ICStub::TypeOf_Typed, stubCode)
    {
        extra_ = uint16_t(type);
        JS_ASSERT(JSType(extra_) == type);
    }

  public:
    static inline ICTypeOf_Typed *New(ICStubSpace *space, IonCode *code, JSType type) {
        if (!code)
            return NULL;
        return space->allocate<ICTypeOf_Typed>(code, type);
    }

    JSType type() const {
        return JSType(extra_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        JSType type_;
        RootedString typeString_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(type_) << 16);
        }

      public:
        Compiler(JSContext *cx, JSType type, HandleString string)
          : ICStubCompiler(cx, ICStub::TypeOf_Typed),
            type_(type),
            typeString_(cx, string)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICTypeOf_Typed::New(space, getStubCode(), type_);
        }
    };
};

// Rest
//      JSOP_REST
class ICRest_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICRest_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::Rest_Fallback, stubCode)
    { }

  public:
    static inline ICRest_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICRest_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::Rest_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICRest_Fallback::New(space, getStubCode());
        }
    };
};

// Stub for JSOP_RETSUB ("returning" from a |finally| block).
class ICRetSub_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICRetSub_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::RetSub_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICRetSub_Fallback *New(ICStubSpace *space, IonCode *code) {
        if (!code)
            return NULL;
        return space->allocate<ICRetSub_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::RetSub_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICRetSub_Fallback::New(space, getStubCode());
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
    uint8_t *addr_;

    ICRetSub_Resume(IonCode *stubCode, uint32_t pcOffset, uint8_t *addr)
      : ICStub(ICStub::RetSub_Resume, stubCode),
        pcOffset_(pcOffset),
        addr_(addr)
    { }

  public:
    static ICRetSub_Resume *New(ICStubSpace *space, IonCode *code, uint32_t pcOffset,
                                uint8_t *addr) {
        if (!code)
            return NULL;
        return space->allocate<ICRetSub_Resume>(code, pcOffset, addr);
    }

    static size_t offsetOfPCOffset() {
        return offsetof(ICRetSub_Resume, pcOffset_);
    }
    static size_t offsetOfAddr() {
        return offsetof(ICRetSub_Resume, addr_);
    }

    class Compiler : public ICStubCompiler {
        uint32_t pcOffset_;
        uint8_t *addr_;

        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, uint32_t pcOffset, uint8_t *addr)
          : ICStubCompiler(cx, ICStub::RetSub_Resume),
            pcOffset_(pcOffset),
            addr_(addr)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICRetSub_Resume::New(space, getStubCode(), pcOffset_, addr_);
        }
    };
};

} // namespace ion
} // namespace js

#endif // JS_ION

#endif /* ion_BaselineIC_h */
