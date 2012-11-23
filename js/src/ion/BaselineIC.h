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
#include "gc/Heap.h"
#include "BaselineJIT.h"
#include "BaselineRegisters.h"

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
// However, the typechains for TypeUpdate ICs do not have their own fallbacks.  This
// is because the type update stubs do NOT return to the mainline code (as with monitor
// ICs).  They return back to the stub they were entered from.  This means that they
// must be entered via a |call|, which alters the return address, and makes it
// difficult to make VMCalls while still allowing for stack inspection.
//
// To deal with this issue, we take the following steps:
//  [1] We guarantee that the type update IC is entered with register-accessible
//      values saved to stack, the value to check in R0, and the return address
//      in R2.scratchReg()
//
//  [2] The last type check stub in any individual chain sends control flow to
//      an "Update fallback" stub, which is shared beteen all chains on an IC.
//      the update fallback stub has the sole job of jumping to a different entry
//      point on the regular fallback stub.
//
//  [3] The IC's main fallback stub has two entry points: the regular one that
//      is entered when all IC stubs fail their guards, and a special entry
//      point for when an IC stub succeeded, but its subsequent typechecks all
//      failed.  In the latter case, the fallback stub fixes its stack state,
//      and jumps into main fallback stub codepath, which does the operation as
//      well as the typeset update manually.  Along with adding a new IC stub
//      for the operation (if that's possible), it also initializes the type
//      stub chain for that IC stub to include the input type seen.
//
//                                 r e t u r n    p a t h
//   <--------------.-----------------.-----------------.-----------------.
//                  |                 |                 |                 |
//        +-----------+     +-----------+     +-----------+     +-----------+
//   ---->| Stub 1    |---->| Stub 2    |---->| Stub 3    |---->| FB Stub   |
//        +-----------+     +-----------+     +-----------+     +-----------+
//          |   ^             |   ^             |   ^                ^
//          |   |             |   |             |   |                |
//          |   |             |   |             |   |                |
//          |   |             |   |             v   |                |
//          |   |             |   |         +-----------+    +-----------+
//          |   |             |   |         | Type 3.1  |--->| Update FB |
//          |   |             |   |         +-----------+    +-----------+
//          |   |             |   |                                   ^
//          |   |             |   \-------------.-----------------.   |
//          |   |             |   |             |                 |   \------.
//          |   |             v   |             |                 |          |
//          |   |         +-----------+     +-----------+     +-----------+  |
//          |   |         | Type 2.1  |---->| Type 2.2  |---->| Type 2.3  |--/
//          |   |         +-----------+     +-----------+     +-----------+  |
//          |   |                                                            |
//          |   \-------------.                                              |
//          |   |             |                                              |
//          v   |             |                                              |
//     +-----------+     +-----------+                                       |
//     | Type 1.1  |---->| Type 1.2  |---------------------------------------/
//     +-----------+     +-----------+
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
    uint32_t            returnOffset_;

    // The PC of this IC's bytecode op within the JSScript.
    uint32_t            pcOffset_;

    // A pointer to the baseline IC stub for this instruction.
    ICStub *            firstStub_;

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

    ICStub *firstStub() const {
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
                                \
    _(TypeMonitor_Fallback)     \
    _(TypeUpdate_Fallback)      \
                                \
    _(Compare_Fallback)         \
    _(Compare_Int32)            \
                                \
    _(ToBool_Fallback)          \
    _(ToBool_Bool)              \
    _(ToBool_Int32)             \
                                \
    _(ToNumber_Fallback)        \
                                \
    _(BinaryArith_Fallback)     \
    _(BinaryArith_Int32)        \
                                \
    _(Call_Fallback)            \
                                \
    _(GetElem_Fallback)         \
    _(GetElem_Dense)            \
                                \
    _(SetElem_Fallback)         \
    _(SetElem_Dense)

#define FORWARD_DECLARE_STUBS(kindName) class IC##kindName;
    IC_STUB_KIND_LIST(FORWARD_DECLARE_STUBS)
#undef FORWARD_DECLARE_STUBS

class ICFallbackStub;
class ICMonitoredStub;
class ICMonitoredFallbackStub;

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

  protected:
    // The kind of the stub.
    //  High bit is 'isFallback' flag.
    //  Second high bit is 'isMonitored' flag.
    bool     isFallback_  : 1;
    bool     isMonitored_ : 1;
    Kind     kind_        : 14;

    // The raw jitcode to call for this stub.
    uint8_t *stubCode_;

    // Pointer to next IC stub.  This is null for the last IC stub, which should
    // either be a fallback or inert IC stub.
    ICStub *next_;

    inline ICStub(Kind kind, IonCode *stubCode)
      : isFallback_(false),
        isMonitored_(false),
        kind_(kind),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

    inline ICStub(Kind kind, bool isFallback, IonCode *stubCode)
      : isFallback_(isFallback),
        isMonitored_(false),
        kind_(kind),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

    inline ICStub(Kind kind, bool isFallback, bool isMonitored, IonCode *stubCode)
      : isFallback_(isFallback),
        isMonitored_(isMonitored),
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
        return isFallback_;
    }

    inline bool isMonitored() const {
        return isMonitored_ && !isFallback_;
    }

    inline bool isMonitoredFallback() const {
        return isMonitored_ && isFallback_;
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
    ICEntry *           icEntry_;

    // The number of stubs kept in the IC entry.
    uint32_t            numOptimizedStubs_;

    // A pointer to the location stub pointer that needs to be
    // changed to add a new "last" stub immediately before the fallback
    // stub.  This'll start out pointing to the icEntry's "firstStub_"
    // field, and as new stubs are addd, it'll point to the current
    // last stub's "next_" field.
    ICStub **           lastStubPtrAddr_;

    ICFallbackStub(Kind kind, IonCode *stubCode)
      : ICStub(kind, true, false, stubCode),
        icEntry_(NULL),
        numOptimizedStubs_(0),
        lastStubPtrAddr_(NULL) {}

    ICFallbackStub(Kind kind, bool isMonitored, IonCode *stubCode)
      : ICStub(kind, true, isMonitored, stubCode),
        icEntry_(NULL),
        numOptimizedStubs_(0),
        lastStubPtrAddr_(NULL) {}

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
};

// Monitored stubs are IC stubs that feed a single resulting value out to a
// type monitor operation.
class ICMonitoredStub : public ICStub
{
  protected:
    // Pointer to the start of the type monitoring stub chain.
    ICStub *            firstMonitorStub_;

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
    ICTypeMonitor_Fallback *    fallbackMonitorStub_;

    ICMonitoredFallbackStub(Kind kind, IonCode *stubCode)
      : ICFallbackStub(kind, true, stubCode),
        fallbackMonitorStub_(NULL) {}

  public:
    bool initMonitoringChain(JSContext *cx);

    inline ICTypeMonitor_Fallback *fallbackMonitorStub() const {
        return fallbackMonitorStub_;
    }
};

// Base class for stubcode compilers.
class ICStubCompiler
{
  protected:
    JSContext *     cx;
    ICStub::Kind    kind;

    // By default the stubcode key is just the kind.
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind);
    }

    virtual bool generateStubCode(MacroAssembler &masm) = 0;
    IonCode *getStubCode();

    ICStubCompiler(JSContext *cx, ICStub::Kind kind)
      : cx(cx), kind(kind) {}

    // Helper to generate an stubcall IonCode from a VMFunction wrapper.
    bool callVM(const VMFunction &fun, MacroAssembler &masm);

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
    virtual ICStub *getStub() = 0;
};

// Base class for stub compilers that can generate multiple stubcodes.
// These compilers need access to the JSOp they are compiling for.
class ICMultiStubCompiler : public ICStubCompiler
{
  protected:
    JSOp            op;

    // Stub keys for multi-stub kinds are composed of both the kind
    // and the op they are compiled for.
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(op) << 16);
    }

    ICMultiStubCompiler(JSContext *cx, ICStub::Kind kind, JSOp op)
      : ICStubCompiler(cx, kind), op(op) {}
};

// TypeMonitor

// The TypeMonitor fallback stub is not a regular fallback stub.
// It doesn't hold a pointer to the IC entry, but rather back to the
// main fallback stub for the IC (from which a pointer to the IC entry
// can be retreived).
class ICTypeMonitor_Fallback : public ICStub
{
    // Pointer to the main fallback stub for the IC.
    ICMonitoredFallbackStub *   mainFallbackStub_;

    // Pointer to the first monitor stub.
    ICStub *                    firstMonitorStub_;

    // Address of the last monitor stub's field pointing to this
    // fallback monitor stub.  This will get updated when new
    // monitor stubs are created and added.
    ICStub **                   lastMonitorStubPtrAddr_;

    // Count of optimized type monitor stubs in this chain.
    uint32_t                    numOptimizedMonitorStubs_;

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
    static inline ICTypeMonitor_Fallback *New(IonCode *code, ICMonitoredFallbackStub *mainFbStub) {
        return new ICTypeMonitor_Fallback(code, mainFbStub);
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
    bool addMonitorStubForValue(JSContext *cx, HandleValue val);

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

        ICTypeMonitor_Fallback *getStub() {
            return ICTypeMonitor_Fallback::New(getStubCode(), mainFallbackStub_);
        }
    };
};

// TypeUpdate

// The TypeUpdate fallback is not a regular fallback, since it just
// forwards to a different entry point in the main fallback stub.
class ICTypeUpdate_Fallback : public ICStub
{
    uint8_t *entryPoint_;

    ICTypeUpdate_Fallback(IonCode *stubCode, uint8_t *entryPoint)
      : ICStub(ICStub::TypeUpdate_Fallback, stubCode),
        entryPoint_(entryPoint)
    { }

  public:
    static inline ICTypeUpdate_Fallback *New(IonCode *code, uint8_t *entryPoint) {
        return new ICTypeUpdate_Fallback(code, entryPoint);
    }

    static inline size_t offsetOfEntryPoint() {
        return offsetof(ICTypeUpdate_Fallback, entryPoint_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      CodeLocationLabel fallbackEntryPoint_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, CodeLocationLabel fallbackEntryPoint)
          : ICStubCompiler(cx, ICStub::TypeUpdate_Fallback),
            fallbackEntryPoint_(fallbackEntryPoint)
        { }

        ICStub *getStub() {
            // TODO: Fix NULL to calculate and pass actual entrypoint.
            return ICTypeUpdate_Fallback::New(getStubCode(), fallbackEntryPoint_.raw());
        }
    };
};

// Compare
//      JSOP_LT
//      JSOP_GT

class ICCompare_Fallback : public ICFallbackStub
{
    ICCompare_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::Compare_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICCompare_Fallback *New(IonCode *code) {
        return new ICCompare_Fallback(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::Compare_Fallback) {}

        ICStub *getStub() {
            return ICCompare_Fallback::New(getStubCode());
        }
    };
};

class ICCompare_Int32 : public ICFallbackStub
{
    ICCompare_Int32(IonCode *stubCode)
      : ICFallbackStub(ICStub::Compare_Int32, stubCode) {}

  public:
    static inline ICCompare_Int32 *New(IonCode *code) {
        return new ICCompare_Int32(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Int32, op) {}

        ICStub *getStub() {
            return ICCompare_Int32::New(getStubCode());
        }
    };
};

// ToBool
//      JSOP_IFNE

class ICToBool_Fallback : public ICFallbackStub
{
    ICToBool_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::ToBool_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICToBool_Fallback *New(IonCode *code) {
        return new ICToBool_Fallback(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Fallback) {}

        ICStub *getStub() {
            return ICToBool_Fallback::New(getStubCode());
        }
    };
};

class ICToBool_Bool : public ICStub
{
    ICToBool_Bool(IonCode *stubCode)
      : ICStub(ICStub::ToBool_Bool, stubCode) {}

  public:
    static inline ICToBool_Bool *New(IonCode *code) {
        return new ICToBool_Bool(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Bool) {}

        ICStub *getStub() {
            return ICToBool_Bool::New(getStubCode());
        }
    };
};

class ICToBool_Int32 : public ICStub
{
    ICToBool_Int32(IonCode *stubCode)
      : ICStub(ICStub::ToBool_Int32, stubCode) {}

  public:
    static inline ICToBool_Int32 *New(IonCode *code) {
        return new ICToBool_Int32(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Int32) {}

        ICStub *getStub() {
            return ICToBool_Int32::New(getStubCode());
        }
    };
};

// ToNumber
//     JSOP_POS

class ICToNumber_Fallback : public ICFallbackStub
{
    ICToNumber_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::ToNumber_Fallback, stubCode) {}

  public:
    static inline ICToNumber_Fallback *New(IonCode *code) {
        return new ICToNumber_Fallback(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToNumber_Fallback) {}

        ICStub *getStub() {
            return ICToNumber_Fallback::New(getStubCode());
        }
    };
};

// BinaryArith
//      JSOP_ADD
//      JSOP_BITAND, JSOP_BITXOR, JSOP_BITOR
//      JSOP_LSH, JSOP_RSH, JSOP_URSH

class ICBinaryArith_Fallback : public ICFallbackStub
{
    ICBinaryArith_Fallback(IonCode *stubCode)
      : ICFallbackStub(BinaryArith_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICBinaryArith_Fallback *New(IonCode *code) {
        return new ICBinaryArith_Fallback(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::BinaryArith_Fallback) {}

        ICStub *getStub() {
            return ICBinaryArith_Fallback::New(getStubCode());
        }
    };
};

class ICBinaryArith_Int32 : public ICStub
{
    ICBinaryArith_Int32(IonCode *stubCode)
      : ICStub(BinaryArith_Int32, stubCode) {}

  public:
    static inline ICBinaryArith_Int32 *New(IonCode *code) {
        return new ICBinaryArith_Int32(code);
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

        ICStub *getStub() {
            return ICBinaryArith_Int32::New(getStubCode());
        }
    };
};

// GetElem
//      JSOP_GETELEM

class ICGetElem_Fallback : public ICMonitoredFallbackStub
{
    ICGetElem_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICGetElem_Fallback *New(IonCode *code) {
        return new ICGetElem_Fallback(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetElem_Fallback)
        { }

        ICStub *getStub() {
            ICGetElem_Fallback *stub = ICGetElem_Fallback::New(getStubCode());
            if (!stub)
                return NULL;
            if (!stub->initMonitoringChain(cx)) {
                delete stub;
                return NULL;
            }
            return stub;
        }
    };
};

class ICGetElem_Dense : public ICMonitoredStub
{
    ICGetElem_Dense(IonCode *stubCode, ICStub *firstMonitorStub)
      : ICMonitoredStub(GetElem_Dense, stubCode, firstMonitorStub) {}

  public:
    static inline ICGetElem_Dense *New(IonCode *code, ICStub *firstMonitorStub) {
        return new ICGetElem_Dense(code, firstMonitorStub);
    }

    class Compiler : public ICStubCompiler {
      ICStub *firstMonitorStub_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub)
          : ICStubCompiler(cx, ICStub::GetElem_Dense),
            firstMonitorStub_(firstMonitorStub) {}

        ICStub *getStub() {
            return ICGetElem_Dense::New(getStubCode(), firstMonitorStub_);
        }
    };
};

// SetElem
//      JSOP_SETELEM

class ICSetElem_Fallback : public ICFallbackStub
{
    ICSetElem_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::SetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICSetElem_Fallback *New(IonCode *code) {
        return new ICSetElem_Fallback(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::SetElem_Fallback)
        { }

        ICStub *getStub() {
            return ICSetElem_Fallback::New(getStubCode());
        }
    };
};

class ICSetElem_Dense : public ICStub
{
    ICSetElem_Dense(IonCode *stubCode)
      : ICStub(SetElem_Dense, stubCode) {}

  public:
    static inline ICSetElem_Dense *New(IonCode *code) {
        return new ICSetElem_Dense(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::SetElem_Dense) {}

        ICStub *getStub() {
            return ICSetElem_Dense::New(getStubCode());
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

    void pushCallArguments(MacroAssembler &masm, Register argcReg);
};

class ICCall_Fallback : public ICFallbackStub
{
    ICCall_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::Call_Fallback, stubCode)
    { }

  public:
    static inline ICCall_Fallback *New(IonCode *code) {
        return new ICCall_Fallback(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICCallStubCompiler(cx, ICStub::Call_Fallback)
        { }

        ICStub *getStub() {
            return ICCall_Fallback::New(getStubCode());
        }
    };
};


} // namespace ion
} // namespace js

#endif

