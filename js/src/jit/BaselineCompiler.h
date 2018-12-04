/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineCompiler_h
#define jit_BaselineCompiler_h

#include "jit/BaselineFrameInfo.h"
#include "jit/BaselineIC.h"
#include "jit/BytecodeAnalysis.h"
#include "jit/FixedList.h"
#include "jit/MacroAssembler.h"

namespace js {
namespace jit {

#define OPCODE_LIST(_)                \
  _(JSOP_NOP)                         \
  _(JSOP_NOP_DESTRUCTURING)           \
  _(JSOP_LABEL)                       \
  _(JSOP_ITERNEXT)                    \
  _(JSOP_POP)                         \
  _(JSOP_POPN)                        \
  _(JSOP_DUPAT)                       \
  _(JSOP_ENTERWITH)                   \
  _(JSOP_LEAVEWITH)                   \
  _(JSOP_DUP)                         \
  _(JSOP_DUP2)                        \
  _(JSOP_SWAP)                        \
  _(JSOP_PICK)                        \
  _(JSOP_UNPICK)                      \
  _(JSOP_GOTO)                        \
  _(JSOP_IFEQ)                        \
  _(JSOP_IFNE)                        \
  _(JSOP_AND)                         \
  _(JSOP_OR)                          \
  _(JSOP_NOT)                         \
  _(JSOP_POS)                         \
  _(JSOP_LOOPHEAD)                    \
  _(JSOP_LOOPENTRY)                   \
  _(JSOP_VOID)                        \
  _(JSOP_UNDEFINED)                   \
  _(JSOP_HOLE)                        \
  _(JSOP_NULL)                        \
  _(JSOP_TRUE)                        \
  _(JSOP_FALSE)                       \
  _(JSOP_ZERO)                        \
  _(JSOP_ONE)                         \
  _(JSOP_INT8)                        \
  _(JSOP_INT32)                       \
  _(JSOP_UINT16)                      \
  _(JSOP_UINT24)                      \
  _(JSOP_RESUMEINDEX)                 \
  _(JSOP_DOUBLE)                      \
  IF_BIGINT(_(JSOP_BIGINT), )         \
  _(JSOP_STRING)                      \
  _(JSOP_SYMBOL)                      \
  _(JSOP_OBJECT)                      \
  _(JSOP_CALLSITEOBJ)                 \
  _(JSOP_REGEXP)                      \
  _(JSOP_LAMBDA)                      \
  _(JSOP_LAMBDA_ARROW)                \
  _(JSOP_SETFUNNAME)                  \
  _(JSOP_BITOR)                       \
  _(JSOP_BITXOR)                      \
  _(JSOP_BITAND)                      \
  _(JSOP_LSH)                         \
  _(JSOP_RSH)                         \
  _(JSOP_URSH)                        \
  _(JSOP_ADD)                         \
  _(JSOP_SUB)                         \
  _(JSOP_MUL)                         \
  _(JSOP_DIV)                         \
  _(JSOP_MOD)                         \
  _(JSOP_POW)                         \
  _(JSOP_LT)                          \
  _(JSOP_LE)                          \
  _(JSOP_GT)                          \
  _(JSOP_GE)                          \
  _(JSOP_EQ)                          \
  _(JSOP_NE)                          \
  _(JSOP_STRICTEQ)                    \
  _(JSOP_STRICTNE)                    \
  _(JSOP_CONDSWITCH)                  \
  _(JSOP_CASE)                        \
  _(JSOP_DEFAULT)                     \
  _(JSOP_LINENO)                      \
  _(JSOP_BITNOT)                      \
  _(JSOP_NEG)                         \
  _(JSOP_NEWARRAY)                    \
  _(JSOP_NEWARRAY_COPYONWRITE)        \
  _(JSOP_INITELEM_ARRAY)              \
  _(JSOP_NEWOBJECT)                   \
  _(JSOP_NEWINIT)                     \
  _(JSOP_INITELEM)                    \
  _(JSOP_INITELEM_GETTER)             \
  _(JSOP_INITELEM_SETTER)             \
  _(JSOP_INITELEM_INC)                \
  _(JSOP_MUTATEPROTO)                 \
  _(JSOP_INITPROP)                    \
  _(JSOP_INITLOCKEDPROP)              \
  _(JSOP_INITHIDDENPROP)              \
  _(JSOP_INITPROP_GETTER)             \
  _(JSOP_INITPROP_SETTER)             \
  _(JSOP_GETELEM)                     \
  _(JSOP_SETELEM)                     \
  _(JSOP_STRICTSETELEM)               \
  _(JSOP_CALLELEM)                    \
  _(JSOP_DELELEM)                     \
  _(JSOP_STRICTDELELEM)               \
  _(JSOP_GETELEM_SUPER)               \
  _(JSOP_SETELEM_SUPER)               \
  _(JSOP_STRICTSETELEM_SUPER)         \
  _(JSOP_IN)                          \
  _(JSOP_HASOWN)                      \
  _(JSOP_GETGNAME)                    \
  _(JSOP_BINDGNAME)                   \
  _(JSOP_SETGNAME)                    \
  _(JSOP_STRICTSETGNAME)              \
  _(JSOP_SETNAME)                     \
  _(JSOP_STRICTSETNAME)               \
  _(JSOP_GETPROP)                     \
  _(JSOP_SETPROP)                     \
  _(JSOP_STRICTSETPROP)               \
  _(JSOP_CALLPROP)                    \
  _(JSOP_DELPROP)                     \
  _(JSOP_STRICTDELPROP)               \
  _(JSOP_GETPROP_SUPER)               \
  _(JSOP_SETPROP_SUPER)               \
  _(JSOP_STRICTSETPROP_SUPER)         \
  _(JSOP_LENGTH)                      \
  _(JSOP_GETBOUNDNAME)                \
  _(JSOP_GETALIASEDVAR)               \
  _(JSOP_SETALIASEDVAR)               \
  _(JSOP_GETNAME)                     \
  _(JSOP_BINDNAME)                    \
  _(JSOP_DELNAME)                     \
  _(JSOP_GETIMPORT)                   \
  _(JSOP_GETINTRINSIC)                \
  _(JSOP_BINDVAR)                     \
  _(JSOP_DEFVAR)                      \
  _(JSOP_DEFCONST)                    \
  _(JSOP_DEFLET)                      \
  _(JSOP_DEFFUN)                      \
  _(JSOP_GETLOCAL)                    \
  _(JSOP_SETLOCAL)                    \
  _(JSOP_GETARG)                      \
  _(JSOP_SETARG)                      \
  _(JSOP_CHECKLEXICAL)                \
  _(JSOP_INITLEXICAL)                 \
  _(JSOP_INITGLEXICAL)                \
  _(JSOP_CHECKALIASEDLEXICAL)         \
  _(JSOP_INITALIASEDLEXICAL)          \
  _(JSOP_UNINITIALIZED)               \
  _(JSOP_CALL)                        \
  _(JSOP_CALL_IGNORES_RV)             \
  _(JSOP_CALLITER)                    \
  _(JSOP_FUNCALL)                     \
  _(JSOP_FUNAPPLY)                    \
  _(JSOP_NEW)                         \
  _(JSOP_EVAL)                        \
  _(JSOP_STRICTEVAL)                  \
  _(JSOP_SPREADCALL)                  \
  _(JSOP_SPREADNEW)                   \
  _(JSOP_SPREADEVAL)                  \
  _(JSOP_STRICTSPREADEVAL)            \
  _(JSOP_OPTIMIZE_SPREADCALL)         \
  _(JSOP_IMPLICITTHIS)                \
  _(JSOP_GIMPLICITTHIS)               \
  _(JSOP_INSTANCEOF)                  \
  _(JSOP_TYPEOF)                      \
  _(JSOP_TYPEOFEXPR)                  \
  _(JSOP_THROWMSG)                    \
  _(JSOP_THROW)                       \
  _(JSOP_TRY)                         \
  _(JSOP_FINALLY)                     \
  _(JSOP_GOSUB)                       \
  _(JSOP_RETSUB)                      \
  _(JSOP_PUSHLEXICALENV)              \
  _(JSOP_POPLEXICALENV)               \
  _(JSOP_FRESHENLEXICALENV)           \
  _(JSOP_RECREATELEXICALENV)          \
  _(JSOP_DEBUGLEAVELEXICALENV)        \
  _(JSOP_PUSHVARENV)                  \
  _(JSOP_POPVARENV)                   \
  _(JSOP_EXCEPTION)                   \
  _(JSOP_DEBUGGER)                    \
  _(JSOP_ARGUMENTS)                   \
  _(JSOP_RUNONCE)                     \
  _(JSOP_REST)                        \
  _(JSOP_TOASYNC)                     \
  _(JSOP_TOASYNCGEN)                  \
  _(JSOP_TOASYNCITER)                 \
  _(JSOP_TOID)                        \
  _(JSOP_TOSTRING)                    \
  _(JSOP_TABLESWITCH)                 \
  _(JSOP_ITER)                        \
  _(JSOP_MOREITER)                    \
  _(JSOP_ISNOITER)                    \
  _(JSOP_ENDITER)                     \
  _(JSOP_ISGENCLOSING)                \
  _(JSOP_GENERATOR)                   \
  _(JSOP_INITIALYIELD)                \
  _(JSOP_YIELD)                       \
  _(JSOP_AWAIT)                       \
  _(JSOP_TRYSKIPAWAIT)                \
  _(JSOP_DEBUGAFTERYIELD)             \
  _(JSOP_FINALYIELDRVAL)              \
  _(JSOP_RESUME)                      \
  _(JSOP_CALLEE)                      \
  _(JSOP_ENVCALLEE)                   \
  _(JSOP_SUPERBASE)                   \
  _(JSOP_SUPERFUN)                    \
  _(JSOP_GETRVAL)                     \
  _(JSOP_SETRVAL)                     \
  _(JSOP_RETRVAL)                     \
  _(JSOP_RETURN)                      \
  _(JSOP_FUNCTIONTHIS)                \
  _(JSOP_GLOBALTHIS)                  \
  _(JSOP_CHECKISOBJ)                  \
  _(JSOP_CHECKISCALLABLE)             \
  _(JSOP_CHECKTHIS)                   \
  _(JSOP_CHECKTHISREINIT)             \
  _(JSOP_CHECKRETURN)                 \
  _(JSOP_NEWTARGET)                   \
  _(JSOP_SUPERCALL)                   \
  _(JSOP_SPREADSUPERCALL)             \
  _(JSOP_THROWSETCONST)               \
  _(JSOP_THROWSETALIASEDCONST)        \
  _(JSOP_THROWSETCALLEE)              \
  _(JSOP_INITHIDDENPROP_GETTER)       \
  _(JSOP_INITHIDDENPROP_SETTER)       \
  _(JSOP_INITHIDDENELEM)              \
  _(JSOP_INITHIDDENELEM_GETTER)       \
  _(JSOP_INITHIDDENELEM_SETTER)       \
  _(JSOP_CHECKOBJCOERCIBLE)           \
  _(JSOP_DEBUGCHECKSELFHOSTED)        \
  _(JSOP_JUMPTARGET)                  \
  _(JSOP_IS_CONSTRUCTING)             \
  _(JSOP_TRY_DESTRUCTURING_ITERCLOSE) \
  _(JSOP_CHECKCLASSHERITAGE)          \
  _(JSOP_INITHOMEOBJECT)              \
  _(JSOP_BUILTINPROTO)                \
  _(JSOP_OBJWITHPROTO)                \
  _(JSOP_FUNWITHPROTO)                \
  _(JSOP_CLASSCONSTRUCTOR)            \
  _(JSOP_DERIVEDCONSTRUCTOR)          \
  _(JSOP_IMPORTMETA)                  \
  _(JSOP_DYNAMIC_IMPORT)

// Base class for BaselineCompiler and BaselineInterpreterGenerator. The Handler
// template is a class storing fields/methods that are interpreter or compiler
// specific. This can be combined with template specialization of methods in
// this class to specialize behavior.
template <typename Handler>
class BaselineCodeGen {
 protected:
  Handler handler;

  JSContext* cx;
  JSScript* script;
  jsbytecode* pc;
  StackMacroAssembler masm;
  bool ionCompileable_;

  TempAllocator& alloc_;
  BytecodeAnalysis analysis_;
  FrameInfo frame;

  js::Vector<RetAddrEntry, 16, SystemAllocPolicy> retAddrEntries_;
  js::Vector<CodeOffset> traceLoggerToggleOffsets_;

  NonAssertingLabel return_;
  NonAssertingLabel postBarrierSlot_;

  // Index of the current ICEntry in the script's ICScript.
  uint32_t icEntryIndex_;

  uint32_t pushedBeforeCall_;
#ifdef DEBUG
  bool inCall_;
#endif

  // Whether any on stack arguments are modified.
  bool modifiesArguments_;

  template <typename... HandlerArgs>
  BaselineCodeGen(JSContext* cx, TempAllocator& alloc, JSScript* script,
                  HandlerArgs&&... args);

  MOZ_MUST_USE bool appendRetAddrEntry(RetAddrEntry::Kind kind,
                                       uint32_t retOffset) {
    if (!retAddrEntries_.emplaceBack(script->pcToOffset(pc), kind,
                                     CodeOffset(retOffset))) {
      ReportOutOfMemory(cx);
      return false;
    }
    return true;
  }

  JSFunction* function() const {
    // Not delazifying here is ok as the function is guaranteed to have
    // been delazified before compilation started.
    return script->functionNonDelazifying();
  }

  ModuleObject* module() const { return script->module(); }

  template <typename T>
  void pushArg(const T& t) {
    masm.Push(t);
  }
  void prepareVMCall();

  enum CallVMPhase { POST_INITIALIZE, CHECK_OVER_RECURSED };
  bool callVM(const VMFunction& fun, CallVMPhase phase = POST_INITIALIZE);

  bool callVMNonOp(const VMFunction& fun, CallVMPhase phase = POST_INITIALIZE) {
    if (!callVM(fun, phase)) {
      return false;
    }
    retAddrEntries_.back().setKind(RetAddrEntry::Kind::NonOpCallVM);
    return true;
  }

  // ifDebuggee should be a function emitting code for when the script is a
  // debuggee script. ifNotDebuggee (if present) is called to emit code for
  // non-debuggee scripts.
  template <typename F1, typename F2>
  MOZ_MUST_USE bool emitDebugInstrumentation(
      const F1& ifDebuggee, const mozilla::Maybe<F2>& ifNotDebuggee);
  template <typename F>
  MOZ_MUST_USE bool emitDebugInstrumentation(const F& ifDebuggee) {
    return emitDebugInstrumentation(ifDebuggee, mozilla::Maybe<F>());
  }

  MOZ_MUST_USE bool emitCheckThis(ValueOperand val, bool reinit = false);
  void emitLoadReturnValue(ValueOperand val);

  MOZ_MUST_USE bool emitNextIC();
  MOZ_MUST_USE bool emitInterruptCheck();
  MOZ_MUST_USE bool emitWarmUpCounterIncrement(bool allowOsr = true);
  MOZ_MUST_USE bool emitTraceLoggerResume(Register script,
                                          AllocatableGeneralRegisterSet& regs);

  void storeValue(const StackValue* source, const Address& dest,
                  const ValueOperand& scratch);

#define EMIT_OP(op) bool emit_##op();
  OPCODE_LIST(EMIT_OP)
#undef EMIT_OP

  // JSOP_NEG, JSOP_BITNOT
  MOZ_MUST_USE bool emitUnaryArith();

  // JSOP_BITXOR, JSOP_LSH, JSOP_ADD etc.
  MOZ_MUST_USE bool emitBinaryArith();

  // Handles JSOP_LT, JSOP_GT, and friends
  MOZ_MUST_USE bool emitCompare();

  // For a JOF_JUMP op, jumps to the op's jump target.
  void emitJump();

  // For a JOF_JUMP op, jumps to the op's jump target depending on the Value
  // in |val|.
  void emitTestBooleanTruthy(bool branchIfTrue, ValueOperand val);

  // Converts |val| to an index in the jump table and stores this in |dest|
  // or branches to the default pc if not int32 or out-of-range.
  void emitGetTableSwitchIndex(ValueOperand val, Register dest);

  MOZ_MUST_USE bool emitReturn();

  MOZ_MUST_USE bool emitToBoolean();
  MOZ_MUST_USE bool emitTest(bool branchIfTrue);
  MOZ_MUST_USE bool emitAndOr(bool branchIfTrue);

  MOZ_MUST_USE bool emitCall(JSOp op);
  MOZ_MUST_USE bool emitSpreadCall(JSOp op);

  MOZ_MUST_USE bool emitDelElem(bool strict);
  MOZ_MUST_USE bool emitDelProp(bool strict);
  MOZ_MUST_USE bool emitSetElemSuper(bool strict);
  MOZ_MUST_USE bool emitSetPropSuper(bool strict);

  MOZ_MUST_USE bool emitBindName(JSOp op);
  MOZ_MUST_USE bool emitDefLexical(JSOp op);

  MOZ_MUST_USE bool emitInitPropGetterSetter();
  MOZ_MUST_USE bool emitInitElemGetterSetter();

  MOZ_MUST_USE bool emitFormalArgAccess(uint32_t arg, bool get);

  MOZ_MUST_USE bool emitThrowConstAssignment();
  MOZ_MUST_USE bool emitUninitializedLexicalCheck(const ValueOperand& val);

  MOZ_MUST_USE bool emitIsMagicValue();

  void getEnvironmentCoordinateObject(Register reg);
  Address getEnvironmentCoordinateAddressFromObject(Register objReg,
                                                    Register reg);
  Address getEnvironmentCoordinateAddress(Register reg);
};

// Interface used by BaselineCodeGen for BaselineCompiler.
class BaselineCompilerHandler {
  TempAllocator& alloc_;
  FixedList<Label> labels_;
  JSScript* script_;
  bool compileDebugInstrumentation_;

 public:
  BaselineCompilerHandler(TempAllocator& alloc, JSScript* script);

  MOZ_MUST_USE bool init();

  Label* labelOf(jsbytecode* pc) { return &labels_[script_->pcToOffset(pc)]; }

  void setCompileDebugInstrumentation() { compileDebugInstrumentation_ = true; }
  bool compileDebugInstrumentation() const {
    return compileDebugInstrumentation_;
  }
};

using BaselineCompilerCodeGen = BaselineCodeGen<BaselineCompilerHandler>;

class BaselineCompiler final : private BaselineCompilerCodeGen {
  // Stores the native code offset for a bytecode pc.
  struct PCMappingEntry {
    uint32_t pcOffset;
    uint32_t nativeOffset;
    PCMappingSlotInfo slotInfo;

    // If set, insert a PCMappingIndexEntry before encoding the
    // current entry.
    bool addIndexEntry;
  };

  js::Vector<PCMappingEntry, 16, SystemAllocPolicy> pcMappingEntries_;

  CodeOffset profilerPushToggleOffset_;
  CodeOffset profilerEnterFrameToggleOffset_;
  CodeOffset profilerExitFrameToggleOffset_;

  CodeOffset traceLoggerScriptTextIdOffset_;

  // Early Ion bailouts will enter at this address. This is after frame
  // construction and before environment chain is initialized.
  CodeOffset bailoutPrologueOffset_;

  // Baseline Debug OSR during prologue will enter at this address. This is
  // right after where a debug prologue VM call would have returned.
  CodeOffset debugOsrPrologueOffset_;

  // Baseline Debug OSR during epilogue will enter at this address. This is
  // right after where a debug epilogue VM call would have returned.
  CodeOffset debugOsrEpilogueOffset_;

  // If a script has more |nslots| than this, then emit code to do an
  // early stack check.
  static const unsigned EARLY_STACK_CHECK_SLOT_COUNT = 128;
  bool needsEarlyStackCheck() const {
    return script->nslots() > EARLY_STACK_CHECK_SLOT_COUNT;
  }

 public:
  BaselineCompiler(JSContext* cx, TempAllocator& alloc, JSScript* script);
  MOZ_MUST_USE bool init();

  MethodStatus compile();

  bool compileDebugInstrumentation() const {
    return handler.compileDebugInstrumentation();
  }
  void setCompileDebugInstrumentation() {
    handler.setCompileDebugInstrumentation();
  }

 private:
  PCMappingSlotInfo getStackTopSlotInfo() {
    MOZ_ASSERT(frame.numUnsyncedSlots() <= 2);
    switch (frame.numUnsyncedSlots()) {
      case 0:
        return PCMappingSlotInfo::MakeSlotInfo();
      case 1:
        return PCMappingSlotInfo::MakeSlotInfo(
            PCMappingSlotInfo::ToSlotLocation(frame.peek(-1)));
      case 2:
      default:
        return PCMappingSlotInfo::MakeSlotInfo(
            PCMappingSlotInfo::ToSlotLocation(frame.peek(-1)),
            PCMappingSlotInfo::ToSlotLocation(frame.peek(-2)));
    }
  }

  BytecodeAnalysis& analysis() { return analysis_; }

  MethodStatus emitBody();

  void emitInitializeLocals();
  MOZ_MUST_USE bool emitPrologue();
  MOZ_MUST_USE bool emitEpilogue();
  MOZ_MUST_USE bool emitOutOfLinePostBarrierSlot();
  MOZ_MUST_USE bool emitStackCheck();
  MOZ_MUST_USE bool emitArgumentTypeChecks();
  void emitIsDebuggeeCheck();
  MOZ_MUST_USE bool emitDebugPrologue();
  MOZ_MUST_USE bool emitDebugTrap();
  MOZ_MUST_USE bool emitTraceLoggerEnter();
  MOZ_MUST_USE bool emitTraceLoggerExit();

  void emitProfilerEnterFrame();
  void emitProfilerExitFrame();

  MOZ_MUST_USE bool initEnvironmentChain();

  MOZ_MUST_USE bool addPCMappingEntry(bool addIndexEntry);
};

// Interface used by BaselineCodeGen for BaselineInterpreterGenerator.
class BaselineInterpreterHandler {
 public:
  explicit BaselineInterpreterHandler();
};

using BaselineInterpreterCodeGen = BaselineCodeGen<BaselineInterpreterHandler>;

class BaselineInterpreterGenerator final : private BaselineInterpreterCodeGen {
 public:
};

extern const VMFunction NewArrayCopyOnWriteInfo;
extern const VMFunction ImplicitThisInfo;

}  // namespace jit
}  // namespace js

#endif /* jit_BaselineCompiler_h */
