/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_ic_h__) && defined(JS_ION)
#define jsion_baseline_ic_h__

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsopcode.h"
#include "BaselineJIT.h"
#include "BaselineRegisters.h"

#include "gc/Heap.h"

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
    uint32_t pcOffset_;

    // A pointer to the baseline IC stub for this instruction.
    ICStub *firstStub_;

  public:
    ICEntry(uint32_t pcOffset)
      : returnOffset_(), pcOffset_(pcOffset), firstStub_(NULL)
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
    bool hasStub() const {
        return firstStub_ != NULL;
    }
    ICStub *firstStub() const {
        JS_ASSERT(hasStub());
        return firstStub_;
    }

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
    _(StackCheck_Fallback)      \
                                \
    _(UseCount_Fallback)        \
                                \
    _(TypeMonitor_Fallback)     \
    _(TypeMonitor_Int32)        \
    _(TypeMonitor_Double)       \
    _(TypeMonitor_Boolean)      \
    _(TypeMonitor_String)       \
    _(TypeMonitor_Null)         \
    _(TypeMonitor_Undefined)    \
    _(TypeMonitor_TypeObject)   \
                                \
    _(TypeUpdate_Fallback)      \
                                \
    _(This_Fallback)            \
                                \
    _(NewArray_Fallback)        \
                                \
    _(Compare_Fallback)         \
    _(Compare_Int32)            \
    _(Compare_Double)           \
                                \
    _(ToBool_Fallback)          \
    _(ToBool_Int32)             \
                                \
    _(ToNumber_Fallback)        \
                                \
    _(BinaryArith_Fallback)     \
    _(BinaryArith_Int32)        \
    _(BinaryArith_Double)       \
                                \
    _(UnaryArith_Fallback)      \
    _(UnaryArith_Int32)         \
                                \
    _(Call_Fallback)            \
    _(Call_Scripted)            \
    _(Call_Native)              \
                                \
    _(GetElem_Fallback)         \
    _(GetElem_Dense)            \
                                \
    _(SetElem_Fallback)         \
    _(SetElem_Dense)            \
                                \
    _(GetName_Fallback)         \
    _(GetName_Global)           \
                                \
    _(GetProp_Fallback)         \
    _(GetProp_DenseLength)      \
    _(GetProp_StringLength)     \
    _(GetProp_Native)           \
    _(GetProp_NativePrototype)  \
                                \
    _(SetProp_Fallback)

#define FORWARD_DECLARE_STUBS(kindName) class IC##kindName;
    IC_STUB_KIND_LIST(FORWARD_DECLARE_STUBS)
#undef FORWARD_DECLARE_STUBS

class ICFallbackStub;
class ICMonitoredStub;
class ICMonitoredFallbackStub;
class ICUpdatedStub;

//
// Base class for all IC stubs.
//
class ICStub
{
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
            JS_NOT_REACHED("Invalid kind.");
            return "INVALID_KIND";
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
    void trace(JSTracer *trc);

  protected:
    // The kind of the stub.
    //  High bit is 'isFallback' flag.
    //  Second high bit is 'isMonitored' flag.
    Trait trait_ : 3;
    Kind kind_ : 13;

    // The raw jitcode to call for this stub.
    uint8_t *stubCode_;

    // Pointer to next IC stub.  This is null for the last IC stub, which should
    // either be a fallback or inert IC stub.
    ICStub *next_;

    inline ICStub(Kind kind, IonCode *stubCode)
      : trait_(Regular),
        kind_(kind),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

    inline ICStub(Kind kind, Trait trait, IonCode *stubCode)
      : trait_(trait),
        kind_(kind),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

  public:

    inline Kind kind() const {
        return static_cast<Kind>(kind_);
    }

    inline bool isFallback() const {
        return trait_ == Fallback || trait_ == MonitoredFallback;
    }

    inline bool isMonitored() const {
        return trait_ == Monitored;
    }

    inline bool isUpdated() const {
        return trait_ == Updated;
    }

    inline bool isMonitoredFallback() const {
        return trait_ == MonitoredFallback;
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

    static inline size_t offsetOfNext() {
        return offsetof(ICStub, next_);
    }

    static inline size_t offsetOfStubCode() {
        return offsetof(ICStub, stubCode_);
    }
};

class ICFallbackStub : public ICStub
{
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
    bool hasStub(ICStub::Kind kind) {
        ICStub *stub = icEntry_->firstStub();
        do {
            if (stub->kind() == kind)
                return true;

            stub = stub->next();
        } while (stub);

        return false;
    }
    void unlinkStubsWithKind(ICStub::Kind kind);
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

    inline ICTypeMonitor_Fallback *fallbackMonitorStub() const {
        return fallbackMonitorStub_;
    }
};

// Updated stubs are IC stubs that use a TypeUpdate IC to track
// the status of heap typesets that need to be updated.
class ICUpdatedStub : public ICStub
{
  protected:
    // Pointer to the start of the type updating stub chain.
    ICStub *firstUpdateStub_;

    uint32_t numOptimizedStubs_;

    ICUpdatedStub(Kind kind, IonCode *stubCode)
      : ICStub(kind, ICStub::Updated, stubCode),
        firstUpdateStub_(NULL),
        numOptimizedStubs_(0)
    {}

  public:
    bool initUpdatingChain(JSContext *cx, ICStubSpace *space);

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
  protected:
    JSContext *cx;
    ICStub::Kind kind;

    // By default the stubcode key is just the kind.
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind);
    }

    virtual bool generateStubCode(MacroAssembler &masm) = 0;
    IonCode *getStubCode();

    ICStubCompiler(JSContext *cx, ICStub::Kind kind)
      : cx(cx), kind(kind) {}

    // Emits a tail call to a VMFunction wrapper.
    bool tailCallVM(const VMFunction &fun, MacroAssembler &masm);

    // Emits a normal (non-tail) call to a VMFunction wrapper.
    bool callVM(const VMFunction &fun, MacroAssembler &masm);

    // Emits a call to a type-update IC, assuming that the value to be
    // checked is already in R0.
    bool callTypeUpdateIC(MacroAssembler &masm);

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
            JS_NOT_REACHED("Invalid numInputs");
        }

        return regs;
    }

  public:
    virtual ICStub *getStub(ICStubSpace *space) = 0;
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

// StackCheck_Fallback

// A StackCheck IC chain has only the fallback stub.
class ICStackCheck_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICStackCheck_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::StackCheck_Fallback, stubCode)
    { }

  public:
    static inline ICStackCheck_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICStackCheck_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::StackCheck_Fallback)
        { }

        ICStackCheck_Fallback *getStub(ICStubSpace *space) {
            return ICStackCheck_Fallback::New(space, getStubCode());
        }
    };
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

// TypeMonitor

// The TypeMonitor fallback stub is not a regular fallback stub.
// It doesn't hold a pointer to the IC entry, but rather back to the
// main fallback stub for the IC (from which a pointer to the IC entry
// can be retreived).
class ICTypeMonitor_Fallback : public ICStub
{
    friend class ICStubSpace;

    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    // Pointer to the main fallback stub for the IC.
    ICMonitoredFallbackStub *mainFallbackStub_;

    // Pointer to the first monitor stub.
    ICStub *firstMonitorStub_;

    // Address of the last monitor stub's field pointing to this
    // fallback monitor stub.  This will get updated when new
    // monitor stubs are created and added.
    ICStub **lastMonitorStubPtrAddr_;

    // Count of optimized type monitor stubs in this chain.
    uint32_t numOptimizedMonitorStubs_;

    ICTypeMonitor_Fallback(IonCode *stubCode, ICMonitoredFallbackStub *mainFallbackStub)
      : ICStub(ICStub::TypeMonitor_Fallback, stubCode),
        mainFallbackStub_(mainFallbackStub),
        firstMonitorStub_(this),
        lastMonitorStubPtrAddr_(NULL),
        numOptimizedMonitorStubs_(0)
    { }

    void addOptimizedMonitorStub(ICStub *stub) {
        stub->setNext(this);

        if (numOptimizedMonitorStubs_ == 0) {
            JS_ASSERT(lastMonitorStubPtrAddr_ == NULL);
            JS_ASSERT(firstMonitorStub_ == this);
            firstMonitorStub_ = stub;
        } else {
            JS_ASSERT(lastMonitorStubPtrAddr_ != NULL);
            JS_ASSERT(firstMonitorStub_ != NULL);
            *lastMonitorStubPtrAddr_ = stub;
        }

        lastMonitorStubPtrAddr_ = stub->addressOfNext();
        numOptimizedMonitorStubs_++;
    }

  public:
    static inline ICTypeMonitor_Fallback *New(
        ICStubSpace *space, IonCode *code, ICMonitoredFallbackStub *mainFbStub)
    {
        return space->allocate<ICTypeMonitor_Fallback>(code, mainFbStub);
    }

    inline ICFallbackStub *mainFallbackStub() const {
        return mainFallbackStub_;
    }

    inline ICStub *firstMonitorStub() const {
        return firstMonitorStub_;
    }

    inline uint32_t numOptimizedMonitorStubs() const {
        return numOptimizedMonitorStubs_;
    }

    // Create a new monitor stub for the type of the given value, and
    // add it to this chain.
    bool addMonitorStubForValue(JSContext *cx, ICStubSpace *space, HandleValue val);

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
        ICMonitoredFallbackStub *mainFallbackStub_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICMonitoredFallbackStub *mainFallbackStub)
          : ICStubCompiler(cx, ICStub::TypeMonitor_Fallback),
            mainFallbackStub_(mainFallbackStub)
        { }

        ICTypeMonitor_Fallback *getStub(ICStubSpace *space) {
            return ICTypeMonitor_Fallback::New(space, getStubCode(), mainFallbackStub_);
        }
    };
};

class ICTypeMonitor_Type : public ICStub
{
    friend class ICStubSpace;

    ICTypeMonitor_Type(Kind kind, IonCode *stubCode)
        : ICStub(kind, stubCode)
    { }

  public:
    static inline ICTypeMonitor_Type *New(ICStubSpace *space, Kind kind, IonCode *code) {
        return space->allocate<ICTypeMonitor_Type>(kind, code);
    }

    static Kind KindFromType(JSValueType type) {
        switch (type) {
          case JSVAL_TYPE_INT32:     return TypeMonitor_Int32;
          case JSVAL_TYPE_DOUBLE:    return TypeMonitor_Double;
          case JSVAL_TYPE_BOOLEAN:   return TypeMonitor_Boolean;
          case JSVAL_TYPE_STRING:    return TypeMonitor_String;
          case JSVAL_TYPE_NULL:      return TypeMonitor_Null;
          case JSVAL_TYPE_UNDEFINED: return TypeMonitor_Undefined;
          default: JS_NOT_REACHED("Invalid type");
        }
    }

    class Compiler : public ICStubCompiler {
      protected:
        JSValueType type_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSValueType type)
          : ICStubCompiler(cx, ICTypeMonitor_Type::KindFromType(type)),
            type_(type)
        { }

        ICTypeMonitor_Type *getStub(ICStubSpace *space) {
            return ICTypeMonitor_Type::New(space, kind, getStubCode());
        }
    };
};

class ICTypeMonitor_TypeObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrTypeObject type_;

    ICTypeMonitor_TypeObject(IonCode *stubCode, HandleTypeObject type)
      : ICStub(TypeMonitor_TypeObject, stubCode),
        type_(type)
    { }

  public:
    static inline ICTypeMonitor_TypeObject *New(
            ICStubSpace *space, IonCode *code, HandleTypeObject type)
    {
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

// This
//      JSOP_THIS

class ICThis_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICThis_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::This_Fallback, stubCode) {}

  public:
    static inline ICThis_Fallback *New(ICStubSpace *space, IonCode *code) {
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

// ToNumber
//     JSOP_POS

class ICToNumber_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICToNumber_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::ToNumber_Fallback, stubCode) {}

  public:
    static inline ICToNumber_Fallback *New(ICStubSpace *space, IonCode *code) {
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
      : ICFallbackStub(BinaryArith_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICBinaryArith_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICBinaryArith_Fallback>(code);
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

    ICBinaryArith_Int32(IonCode *stubCode)
      : ICStub(BinaryArith_Int32, stubCode) {}

  public:
    static inline ICBinaryArith_Int32 *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICBinaryArith_Int32>(code);
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
            return ICBinaryArith_Int32::New(space, getStubCode());
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
      : ICFallbackStub(UnaryArith_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICUnaryArith_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICUnaryArith_Fallback>(code);
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

class ICBinaryArith_Double : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_Double(IonCode *stubCode)
      : ICStub(BinaryArith_Double, stubCode)
    {}

  public:
    static inline ICBinaryArith_Double *New(ICStubSpace *space, IonCode *code) {
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

class ICUnaryArith_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICUnaryArith_Int32(IonCode *stubCode)
      : ICStub(UnaryArith_Int32, stubCode)
    {}

  public:
    static inline ICUnaryArith_Int32 *New(ICStubSpace *space, IonCode *code) {
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

// GetElem
//      JSOP_GETELEM

class ICGetElem_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetElem_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICGetElem_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetElem_Fallback>(code);
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

class ICGetElem_Dense : public ICMonitoredStub
{
    friend class ICStubSpace;

    ICGetElem_Dense(IonCode *stubCode, ICStub *firstMonitorStub)
      : ICMonitoredStub(GetElem_Dense, stubCode, firstMonitorStub) {}

  public:
    static inline ICGetElem_Dense *New(
            ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub)
    {
        return space->allocate<ICGetElem_Dense>(code, firstMonitorStub);
    }

    class Compiler : public ICStubCompiler {
      ICStub *firstMonitorStub_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub)
          : ICStubCompiler(cx, ICStub::GetElem_Dense),
            firstMonitorStub_(firstMonitorStub) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetElem_Dense::New(space, getStubCode(), firstMonitorStub_);
        }
    };
};

// SetElem
//      JSOP_SETELEM

class ICSetElem_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICSetElem_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::SetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICSetElem_Fallback *New(ICStubSpace *space, IonCode *code) {
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

    HeapPtrTypeObject type_;

    ICSetElem_Dense(IonCode *stubCode, HandleTypeObject type)
      : ICUpdatedStub(SetElem_Dense, stubCode),
        type_(type) {}

  public:
    static inline ICSetElem_Dense *New(ICStubSpace *space, IonCode *code, HandleTypeObject type) {
        return space->allocate<ICSetElem_Dense>(code, type);
    }

    static size_t offsetOfType() {
        return offsetof(ICSetElem_Dense, type_);
    }

    HeapPtrTypeObject &type() {
        return type_;
    }

    class Compiler : public ICStubCompiler {
        // Compiler is only live on stack during compilation, it should
        // outlive any RootedTypeObject it's passed.  So it can just
        // use the handle.
        HandleTypeObject type_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleTypeObject type)
          : ICStubCompiler(cx, ICStub::SetElem_Dense),
            type_(type) {}

        ICStub *getStub(ICStubSpace *space) {
            ICSetElem_Dense *stub = ICSetElem_Dense::New(space, getStubCode(), type_);
            if (!stub || !stub->initUpdatingChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

// GetName
//      JSOP_GETGNAME
class ICGetName_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetName_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetName_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICGetName_Fallback *New(ICStubSpace *space, IonCode *code) {
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

    HeapPtrShape shape_;
    uint32_t slot_;

    ICGetName_Global(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape, uint32_t slot)
      : ICMonitoredStub(GetName_Global, stubCode, firstMonitorStub),
        shape_(shape),
        slot_(slot)
    {}

  public:
    static inline ICGetName_Global *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                        HandleShape shape, uint32_t slot)
    {
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

class ICGetProp_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetProp_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetProp_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICGetProp_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetProp_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

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
class ICGetProp_DenseLength : public ICStub
{
    friend class ICStubSpace;

    ICGetProp_DenseLength(IonCode *stubCode)
      : ICStub(GetProp_DenseLength, stubCode)
    {}

  public:
    static inline ICGetProp_DenseLength *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetProp_DenseLength>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_DenseLength)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetProp_DenseLength::New(space, getStubCode());
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
                        HandleShape shape, uint32_t offset)
      : ICMonitoredStub(kind, stubCode, firstMonitorStub),
        shape_(shape),
        offset_(offset)
    {}

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
        return space->allocate<ICGetProp_Native>(code, firstMonitorStub, shape, offset);
    }
};

// Stub for accessing a property on a native object's prototype. Note that due to
// the shape teleporting optimization, we only have to guard on the object's shape
// and the holder's shape.
class ICGetProp_NativePrototype : public ICGetPropNativeStub
{
    friend class ICStubSpace;

    // Holder and its shape.
    HeapPtrObject holder_;
    HeapPtrShape holderShape_;

    ICGetProp_NativePrototype(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape,
                              uint32_t offset, HandleObject holder, HandleShape holderShape)
      : ICGetPropNativeStub(GetProp_NativePrototype, stubCode, firstMonitorStub, shape, offset),
        holder_(holder),
        holderShape_(holderShape)
    {}

  public:
    static inline ICGetProp_NativePrototype *New(ICStubSpace *space, IonCode *code,
                                                 ICStub *firstMonitorStub, HandleShape shape,
                                                 uint32_t offset, HandleObject holder,
                                                 HandleShape holderShape)
    {
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

// SetProp
//     JSOP_SETPROP
//     JSOP_SETNAME
//     JSOP_SETGNAME

class ICSetProp_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICSetProp_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::SetProp_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICSetProp_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICSetProp_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::SetProp_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICSetProp_Fallback::New(space, getStubCode());
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
};

class ICCall_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICCall_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::Call_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICCall_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCall_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICCallStubCompiler(cx, ICStub::Call_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICCall_Fallback *stub = ICCall_Fallback::New(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

class ICCall_Scripted : public ICMonitoredStub
{
    friend class ICStubSpace;

    HeapPtrFunction callee_;

    ICCall_Scripted(IonCode *stubCode, ICStub *firstMonitorStub, HandleFunction callee)
      : ICMonitoredStub(ICStub::Call_Scripted, stubCode, firstMonitorStub),
        callee_(callee)
    { }

  public:
    static inline ICCall_Scripted *New(
            ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub, HandleFunction callee)
    {
        return space->allocate<ICCall_Scripted>(code, firstMonitorStub, callee);
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Scripted, callee_);
    }
    HeapPtrFunction &callee() {
        return callee_;
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub *firstMonitorStub_;
        RootedFunction callee_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleFunction callee)
          : ICCallStubCompiler(cx, ICStub::Call_Scripted),
            firstMonitorStub_(firstMonitorStub),
            callee_(cx, callee)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICCall_Scripted::New(space, getStubCode(), firstMonitorStub_, callee_);
        }
    };
};

class ICCall_Native : public ICMonitoredStub
{
    friend class ICStubSpace;

    HeapPtrFunction callee_;

    ICCall_Native(IonCode *stubCode, ICStub *firstMonitorStub, HandleFunction callee)
      : ICMonitoredStub(ICStub::Call_Native, stubCode, firstMonitorStub),
        callee_(callee)
    { }

  public:
    static inline ICCall_Native *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                     HandleFunction callee)
    {
        return space->allocate<ICCall_Native>(code, firstMonitorStub, callee);
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Native, callee_);
    }
    HeapPtrFunction &callee() {
        return callee_;
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub *firstMonitorStub_;
        RootedFunction callee_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleFunction callee)
          : ICCallStubCompiler(cx, ICStub::Call_Native),
            firstMonitorStub_(firstMonitorStub),
            callee_(cx, callee)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICCall_Native::New(space, getStubCode(), firstMonitorStub_, callee_);
        }
    };
};

} // namespace ion
} // namespace js

#endif

