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
    _(Compare_Fallback)         \
    _(Compare_Int32)            \
                                \
    _(ToBool_Fallback)          \
    _(ToBool_Bool)              \
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
    // The kind of the stub - high bit is 'isFallback' flag.
    uint16_t kind_;

    // The raw jitcode to call for this stub.
    uint8_t *stubCode_;

    // Pointer to next IC stub.  This is null for the last IC stub, which should
    // either be a fallback or inert IC stub.
    ICStub *next_;

    inline static uint16_t packKindAndFallbackFlag(Kind kind, bool isFallback) {
        uint16_t val = static_cast<uint16_t>(kind);
        JS_ASSERT(!(val & 0x8000U));
        if (isFallback)
            val |= 0x8000U;
        return val;
    }

    inline ICStub(Kind kind, IonCode *stubCode)
      : kind_(packKindAndFallbackFlag(kind, false)),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

    inline ICStub(Kind kind, bool isFallback, IonCode *stubCode)
      : kind_(packKindAndFallbackFlag(kind, isFallback)),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

  public:

    inline Kind kind() const {
        return static_cast<Kind>(kind_ & 0x7fffU);
    }

    inline bool isFallback() const {
        return !!(kind_ & 0x8000U);
    }

    inline const ICFallbackStub *toFallbackStub() const {
        return reinterpret_cast<const ICFallbackStub *>(this);
    }

    inline ICFallbackStub *toFallbackStub()  {
        return reinterpret_cast<ICFallbackStub *>(this);
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
      : ICStub(kind, true, stubCode),
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

class ICGetElem_Fallback : public ICFallbackStub
{
    ICGetElem_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::GetElem_Fallback, stubCode)
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
            return ICGetElem_Fallback::New(getStubCode());
        }
    };
};

class ICGetElem_Dense : public ICStub
{
    ICGetElem_Dense(IonCode *stubCode)
      : ICStub(GetElem_Dense, stubCode) {}

  public:
    static inline ICGetElem_Dense *New(IonCode *code) {
        return new ICGetElem_Dense(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetElem_Dense) {}

        ICStub *getStub() {
            return ICGetElem_Dense::New(getStubCode());
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

