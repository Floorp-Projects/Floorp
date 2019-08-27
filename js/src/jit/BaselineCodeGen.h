/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineCodeGen_h
#define jit_BaselineCodeGen_h

#include "jit/BaselineFrameInfo.h"
#include "jit/BaselineIC.h"
#include "jit/BytecodeAnalysis.h"
#include "jit/FixedList.h"
#include "jit/MacroAssembler.h"

namespace js {

enum class GeneratorResumeKind;

namespace jit {

#define OPCODE_LIST(_)             \
  _(JSOP_NOP)                      \
  _(JSOP_NOP_DESTRUCTURING)        \
  _(JSOP_LABEL)                    \
  _(JSOP_ITERNEXT)                 \
  _(JSOP_POP)                      \
  _(JSOP_POPN)                     \
  _(JSOP_DUPAT)                    \
  _(JSOP_ENTERWITH)                \
  _(JSOP_LEAVEWITH)                \
  _(JSOP_DUP)                      \
  _(JSOP_DUP2)                     \
  _(JSOP_SWAP)                     \
  _(JSOP_PICK)                     \
  _(JSOP_UNPICK)                   \
  _(JSOP_GOTO)                     \
  _(JSOP_IFEQ)                     \
  _(JSOP_IFNE)                     \
  _(JSOP_AND)                      \
  _(JSOP_OR)                       \
  _(JSOP_NOT)                      \
  _(JSOP_POS)                      \
  _(JSOP_TONUMERIC)                \
  _(JSOP_LOOPHEAD)                 \
  _(JSOP_LOOPENTRY)                \
  _(JSOP_VOID)                     \
  _(JSOP_UNDEFINED)                \
  _(JSOP_HOLE)                     \
  _(JSOP_NULL)                     \
  _(JSOP_TRUE)                     \
  _(JSOP_FALSE)                    \
  _(JSOP_ZERO)                     \
  _(JSOP_ONE)                      \
  _(JSOP_INT8)                     \
  _(JSOP_INT32)                    \
  _(JSOP_UINT16)                   \
  _(JSOP_UINT24)                   \
  _(JSOP_RESUMEINDEX)              \
  _(JSOP_DOUBLE)                   \
  _(JSOP_BIGINT)                   \
  _(JSOP_STRING)                   \
  _(JSOP_SYMBOL)                   \
  _(JSOP_OBJECT)                   \
  _(JSOP_CALLSITEOBJ)              \
  _(JSOP_REGEXP)                   \
  _(JSOP_LAMBDA)                   \
  _(JSOP_LAMBDA_ARROW)             \
  _(JSOP_SETFUNNAME)               \
  _(JSOP_BITOR)                    \
  _(JSOP_BITXOR)                   \
  _(JSOP_BITAND)                   \
  _(JSOP_LSH)                      \
  _(JSOP_RSH)                      \
  _(JSOP_URSH)                     \
  _(JSOP_ADD)                      \
  _(JSOP_SUB)                      \
  _(JSOP_MUL)                      \
  _(JSOP_DIV)                      \
  _(JSOP_MOD)                      \
  _(JSOP_POW)                      \
  _(JSOP_LT)                       \
  _(JSOP_LE)                       \
  _(JSOP_GT)                       \
  _(JSOP_GE)                       \
  _(JSOP_EQ)                       \
  _(JSOP_NE)                       \
  _(JSOP_STRICTEQ)                 \
  _(JSOP_STRICTNE)                 \
  _(JSOP_CONDSWITCH)               \
  _(JSOP_CASE)                     \
  _(JSOP_DEFAULT)                  \
  _(JSOP_LINENO)                   \
  _(JSOP_BITNOT)                   \
  _(JSOP_NEG)                      \
  _(JSOP_NEWARRAY)                 \
  _(JSOP_NEWARRAY_COPYONWRITE)     \
  _(JSOP_INITELEM_ARRAY)           \
  _(JSOP_NEWOBJECT)                \
  _(JSOP_NEWINIT)                  \
  _(JSOP_INITELEM)                 \
  _(JSOP_INITELEM_GETTER)          \
  _(JSOP_INITELEM_SETTER)          \
  _(JSOP_INITELEM_INC)             \
  _(JSOP_MUTATEPROTO)              \
  _(JSOP_INITPROP)                 \
  _(JSOP_INITLOCKEDPROP)           \
  _(JSOP_INITHIDDENPROP)           \
  _(JSOP_INITPROP_GETTER)          \
  _(JSOP_INITPROP_SETTER)          \
  _(JSOP_GETELEM)                  \
  _(JSOP_SETELEM)                  \
  _(JSOP_STRICTSETELEM)            \
  _(JSOP_CALLELEM)                 \
  _(JSOP_DELELEM)                  \
  _(JSOP_STRICTDELELEM)            \
  _(JSOP_GETELEM_SUPER)            \
  _(JSOP_SETELEM_SUPER)            \
  _(JSOP_STRICTSETELEM_SUPER)      \
  _(JSOP_IN)                       \
  _(JSOP_HASOWN)                   \
  _(JSOP_GETGNAME)                 \
  _(JSOP_BINDGNAME)                \
  _(JSOP_SETGNAME)                 \
  _(JSOP_STRICTSETGNAME)           \
  _(JSOP_SETNAME)                  \
  _(JSOP_STRICTSETNAME)            \
  _(JSOP_GETPROP)                  \
  _(JSOP_SETPROP)                  \
  _(JSOP_STRICTSETPROP)            \
  _(JSOP_CALLPROP)                 \
  _(JSOP_DELPROP)                  \
  _(JSOP_STRICTDELPROP)            \
  _(JSOP_GETPROP_SUPER)            \
  _(JSOP_SETPROP_SUPER)            \
  _(JSOP_STRICTSETPROP_SUPER)      \
  _(JSOP_LENGTH)                   \
  _(JSOP_GETBOUNDNAME)             \
  _(JSOP_GETALIASEDVAR)            \
  _(JSOP_SETALIASEDVAR)            \
  _(JSOP_GETNAME)                  \
  _(JSOP_BINDNAME)                 \
  _(JSOP_DELNAME)                  \
  _(JSOP_GETIMPORT)                \
  _(JSOP_GETINTRINSIC)             \
  _(JSOP_SETINTRINSIC)             \
  _(JSOP_BINDVAR)                  \
  _(JSOP_DEFVAR)                   \
  _(JSOP_DEFCONST)                 \
  _(JSOP_DEFLET)                   \
  _(JSOP_DEFFUN)                   \
  _(JSOP_GETLOCAL)                 \
  _(JSOP_SETLOCAL)                 \
  _(JSOP_GETARG)                   \
  _(JSOP_SETARG)                   \
  _(JSOP_CHECKLEXICAL)             \
  _(JSOP_INITLEXICAL)              \
  _(JSOP_INITGLEXICAL)             \
  _(JSOP_CHECKALIASEDLEXICAL)      \
  _(JSOP_INITALIASEDLEXICAL)       \
  _(JSOP_UNINITIALIZED)            \
  _(JSOP_CALL)                     \
  _(JSOP_CALL_IGNORES_RV)          \
  _(JSOP_CALLITER)                 \
  _(JSOP_FUNCALL)                  \
  _(JSOP_FUNAPPLY)                 \
  _(JSOP_NEW)                      \
  _(JSOP_EVAL)                     \
  _(JSOP_STRICTEVAL)               \
  _(JSOP_SPREADCALL)               \
  _(JSOP_SPREADNEW)                \
  _(JSOP_SPREADEVAL)               \
  _(JSOP_STRICTSPREADEVAL)         \
  _(JSOP_OPTIMIZE_SPREADCALL)      \
  _(JSOP_IMPLICITTHIS)             \
  _(JSOP_GIMPLICITTHIS)            \
  _(JSOP_INSTANCEOF)               \
  _(JSOP_TYPEOF)                   \
  _(JSOP_TYPEOFEXPR)               \
  _(JSOP_THROWMSG)                 \
  _(JSOP_THROW)                    \
  _(JSOP_TRY)                      \
  _(JSOP_FINALLY)                  \
  _(JSOP_GOSUB)                    \
  _(JSOP_RETSUB)                   \
  _(JSOP_PUSHLEXICALENV)           \
  _(JSOP_POPLEXICALENV)            \
  _(JSOP_FRESHENLEXICALENV)        \
  _(JSOP_RECREATELEXICALENV)       \
  _(JSOP_DEBUGLEAVELEXICALENV)     \
  _(JSOP_PUSHVARENV)               \
  _(JSOP_POPVARENV)                \
  _(JSOP_EXCEPTION)                \
  _(JSOP_DEBUGGER)                 \
  _(JSOP_ARGUMENTS)                \
  _(JSOP_REST)                     \
  _(JSOP_TOASYNCITER)              \
  _(JSOP_TOID)                     \
  _(JSOP_TOSTRING)                 \
  _(JSOP_TABLESWITCH)              \
  _(JSOP_ITER)                     \
  _(JSOP_MOREITER)                 \
  _(JSOP_ISNOITER)                 \
  _(JSOP_ENDITER)                  \
  _(JSOP_ISGENCLOSING)             \
  _(JSOP_GENERATOR)                \
  _(JSOP_INITIALYIELD)             \
  _(JSOP_YIELD)                    \
  _(JSOP_AWAIT)                    \
  _(JSOP_TRYSKIPAWAIT)             \
  _(JSOP_AFTERYIELD)               \
  _(JSOP_FINALYIELDRVAL)           \
  _(JSOP_RESUME)                   \
  _(JSOP_ASYNCAWAIT)               \
  _(JSOP_ASYNCRESOLVE)             \
  _(JSOP_CALLEE)                   \
  _(JSOP_ENVCALLEE)                \
  _(JSOP_SUPERBASE)                \
  _(JSOP_SUPERFUN)                 \
  _(JSOP_GETRVAL)                  \
  _(JSOP_SETRVAL)                  \
  _(JSOP_RETRVAL)                  \
  _(JSOP_RETURN)                   \
  _(JSOP_FUNCTIONTHIS)             \
  _(JSOP_GLOBALTHIS)               \
  _(JSOP_CHECKISOBJ)               \
  _(JSOP_CHECKISCALLABLE)          \
  _(JSOP_CHECKTHIS)                \
  _(JSOP_CHECKTHISREINIT)          \
  _(JSOP_CHECKRETURN)              \
  _(JSOP_NEWTARGET)                \
  _(JSOP_SUPERCALL)                \
  _(JSOP_SPREADSUPERCALL)          \
  _(JSOP_THROWSETCONST)            \
  _(JSOP_THROWSETALIASEDCONST)     \
  _(JSOP_THROWSETCALLEE)           \
  _(JSOP_INITHIDDENPROP_GETTER)    \
  _(JSOP_INITHIDDENPROP_SETTER)    \
  _(JSOP_INITHIDDENELEM)           \
  _(JSOP_INITHIDDENELEM_GETTER)    \
  _(JSOP_INITHIDDENELEM_SETTER)    \
  _(JSOP_CHECKOBJCOERCIBLE)        \
  _(JSOP_DEBUGCHECKSELFHOSTED)     \
  _(JSOP_JUMPTARGET)               \
  _(JSOP_IS_CONSTRUCTING)          \
  _(JSOP_TRY_DESTRUCTURING)        \
  _(JSOP_CHECKCLASSHERITAGE)       \
  _(JSOP_INITHOMEOBJECT)           \
  _(JSOP_BUILTINPROTO)             \
  _(JSOP_OBJWITHPROTO)             \
  _(JSOP_FUNWITHPROTO)             \
  _(JSOP_CLASSCONSTRUCTOR)         \
  _(JSOP_DERIVEDCONSTRUCTOR)       \
  _(JSOP_IMPORTMETA)               \
  _(JSOP_DYNAMIC_IMPORT)           \
  _(JSOP_INC)                      \
  _(JSOP_DEC)                      \
  _(JSOP_INSTRUMENTATION_ACTIVE)   \
  _(JSOP_INSTRUMENTATION_CALLBACK) \
  _(JSOP_INSTRUMENTATION_SCRIPT_ID)

enum class ScriptGCThingType { RegExp, Function, Scope, BigInt };

// Base class for BaselineCompiler and BaselineInterpreterGenerator. The Handler
// template is a class storing fields/methods that are interpreter or compiler
// specific. This can be combined with template specialization of methods in
// this class to specialize behavior.
template <typename Handler>
class BaselineCodeGen {
 protected:
  Handler handler;

  JSContext* cx;
  StackMacroAssembler masm;

  typename Handler::FrameInfoT& frame;

  js::Vector<CodeOffset> traceLoggerToggleOffsets_;

  // Shared epilogue code to return to the caller.
  NonAssertingLabel return_;

  // Like return_ but skips the debug epilogue instrumentation.
  NonAssertingLabel returnNoDebugEpilogue_;

  NonAssertingLabel postBarrierSlot_;

  // Prologue code where we resume for Ion prologue bailouts.
  NonAssertingLabel bailoutPrologue_;

  CodeOffset profilerEnterFrameToggleOffset_;
  CodeOffset profilerExitFrameToggleOffset_;

  // Early Ion bailouts will enter at this address. This is after frame
  // construction and before environment chain is initialized.
  CodeOffset bailoutPrologueOffset_;

  // Baseline Interpreter can enter Baseline Compiler code at this address. This
  // is right after the warm-up counter check in the prologue.
  CodeOffset warmUpCheckPrologueOffset_;

  uint32_t pushedBeforeCall_ = 0;
#ifdef DEBUG
  bool inCall_ = false;
#endif

  template <typename... HandlerArgs>
  explicit BaselineCodeGen(JSContext* cx, HandlerArgs&&... args);

  template <typename T>
  void pushArg(const T& t) {
    masm.Push(t);
  }

  // Pushes the current script as argument for a VM function.
  void pushScriptArg();

  // Pushes the bytecode pc as argument for a VM function.
  void pushBytecodePCArg();

  // Pushes a name/object/scope associated with the current bytecode op (and
  // stored in the script) as argument for a VM function.
  void loadScriptGCThing(ScriptGCThingType type, Register dest,
                         Register scratch);
  void pushScriptGCThingArg(ScriptGCThingType type, Register scratch1,
                            Register scratch2);
  void pushScriptNameArg(Register scratch1, Register scratch2);

  // Pushes a bytecode operand as argument for a VM function.
  void pushUint8BytecodeOperandArg(Register scratch);
  void pushUint16BytecodeOperandArg(Register scratch);

  void loadInt32LengthBytecodeOperand(Register dest);
  void loadInt32IndexBytecodeOperand(ValueOperand dest);
  void loadNumFormalArguments(Register dest);

  // Loads the current JSScript* in dest.
  void loadScript(Register dest);

  void saveInterpreterPCReg();
  void restoreInterpreterPCReg();

  // Subtracts |script->nslots() * sizeof(Value)| from reg.
  void subtractScriptSlotsSize(Register reg, Register scratch);

  // Jump to the script's resume entry indicated by resumeIndex.
  void jumpToResumeEntry(Register resumeIndex, Register scratch1,
                         Register scratch2);

  // Load the global's lexical environment.
  void loadGlobalLexicalEnvironment(Register dest);
  void pushGlobalLexicalEnvironmentValue(ValueOperand scratch);

  // Load the |this|-value from the global's lexical environment.
  void loadGlobalThisValue(ValueOperand dest);

  // Load script atom |index| into |dest|.
  void loadScriptAtom(Register index, Register dest);

  void prepareVMCall();

  void storeFrameSizeAndPushDescriptor(uint32_t frameBaseSize, uint32_t argSize,
                                       const Address& frameSizeAddr,
                                       Register scratch1, Register scratch2);

  enum class CallVMPhase { BeforePushingLocals, AfterPushingLocals };
  bool callVMInternal(VMFunctionId id, RetAddrEntry::Kind kind,
                      CallVMPhase phase);

  template <typename Fn, Fn fn>
  bool callVM(RetAddrEntry::Kind kind = RetAddrEntry::Kind::CallVM,
              CallVMPhase phase = CallVMPhase::AfterPushingLocals);

  template <typename Fn, Fn fn>
  bool callVMNonOp(CallVMPhase phase = CallVMPhase::AfterPushingLocals) {
    return callVM<Fn, fn>(RetAddrEntry::Kind::NonOpCallVM, phase);
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

  template <typename F>
  MOZ_MUST_USE bool emitAfterYieldDebugInstrumentation(const F& ifDebuggee,
                                                       Register scratch);

  // ifSet should be a function emitting code for when the script has |flag|
  // set. ifNotSet emits code for when the flag isn't set.
  template <typename F1, typename F2>
  MOZ_MUST_USE bool emitTestScriptFlag(JSScript::ImmutableFlags flag,
                                       const F1& ifSet, const F2& ifNotSet,
                                       Register scratch);

  // If |script->hasFlag(flag) == value|, execute the code emitted by |emit|.
  template <typename F>
  MOZ_MUST_USE bool emitTestScriptFlag(JSScript::ImmutableFlags flag,
                                       bool value, const F& emit,
                                       Register scratch);
  template <typename F>
  MOZ_MUST_USE bool emitTestScriptFlag(JSScript::MutableFlags flag, bool value,
                                       const F& emit, Register scratch);

  MOZ_MUST_USE bool emitGeneratorResume(GeneratorResumeKind resumeKind);
  MOZ_MUST_USE bool emitEnterGeneratorCode(Register script,
                                           Register resumeIndex,
                                           Register scratch);
  MOZ_MUST_USE bool emitGeneratorThrowOrReturnCallVM();

  void emitInterpJumpToResumeEntry(Register script, Register resumeIndex,
                                   Register scratch);
  void emitJumpToInterpretOpLabel();

  MOZ_MUST_USE bool emitIncExecutionProgressCounter(Register scratch);

  MOZ_MUST_USE bool emitCheckThis(ValueOperand val, bool reinit = false);
  void emitLoadReturnValue(ValueOperand val);
  void emitPushNonArrowFunctionNewTarget();
  void emitGetAliasedVar(ValueOperand dest);

  MOZ_MUST_USE bool emitNextIC();
  MOZ_MUST_USE bool emitInterruptCheck();
  MOZ_MUST_USE bool emitWarmUpCounterIncrement();
  MOZ_MUST_USE bool emitTraceLoggerResume(Register script,
                                          AllocatableGeneralRegisterSet& regs);

#define EMIT_OP(op) bool emit_##op();
  OPCODE_LIST(EMIT_OP)
#undef EMIT_OP

  // JSOP_NEG, JSOP_BITNOT, JSOP_INC, JSOP_DEC
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
  void emitGetTableSwitchIndex(ValueOperand val, Register dest,
                               Register scratch1, Register scratch2);

  // Jumps to the target of a table switch based on |key| and the
  // firstResumeIndex stored in JSOP_TABLESWITCH.
  void emitTableSwitchJump(Register key, Register scratch1, Register scratch2);

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

  // Try to bake in the result of GETGNAME/BINDGNAME instead of using an IC.
  // Return true if we managed to optimize the op.
  bool tryOptimizeGetGlobalName();
  bool tryOptimizeBindGlobalName();

  MOZ_MUST_USE bool emitInitPropGetterSetter();
  MOZ_MUST_USE bool emitInitElemGetterSetter();

  MOZ_MUST_USE bool emitFormalArgAccess(JSOp op);

  MOZ_MUST_USE bool emitThrowConstAssignment();
  MOZ_MUST_USE bool emitUninitializedLexicalCheck(const ValueOperand& val);

  MOZ_MUST_USE bool emitIsMagicValue();

  void getEnvironmentCoordinateObject(Register reg);
  Address getEnvironmentCoordinateAddressFromObject(Register objReg,
                                                    Register reg);
  Address getEnvironmentCoordinateAddress(Register reg);

  MOZ_MUST_USE bool emitPrologue();
  MOZ_MUST_USE bool emitEpilogue();
  MOZ_MUST_USE bool emitOutOfLinePostBarrierSlot();
  MOZ_MUST_USE bool emitStackCheck();
  MOZ_MUST_USE bool emitArgumentTypeChecks();
  MOZ_MUST_USE bool emitDebugPrologue();
  MOZ_MUST_USE bool emitDebugEpilogue();

  template <typename F1, typename F2>
  MOZ_MUST_USE bool initEnvironmentChainHelper(const F1& initFunctionEnv,
                                               const F2& initGlobalOrEvalEnv,
                                               Register scratch);
  MOZ_MUST_USE bool initEnvironmentChain();

  MOZ_MUST_USE bool emitTraceLoggerEnter();
  MOZ_MUST_USE bool emitTraceLoggerExit();

  MOZ_MUST_USE bool emitHandleCodeCoverageAtPrologue();

  void emitInitFrameFields(Register nonFunctionEnv);
  MOZ_MUST_USE bool emitIsDebuggeeCheck();
  void emitInitializeLocals();

  void emitProfilerEnterFrame();
  void emitProfilerExitFrame();
};

using RetAddrEntryVector = js::Vector<RetAddrEntry, 16, SystemAllocPolicy>;

// Interface used by BaselineCodeGen for BaselineCompiler.
class BaselineCompilerHandler {
  CompilerFrameInfo frame_;
  TempAllocator& alloc_;
  BytecodeAnalysis analysis_;
#ifdef DEBUG
  const MacroAssembler& masm_;
#endif
  FixedList<Label> labels_;
  RetAddrEntryVector retAddrEntries_;

  // Native code offsets for OSR at JSOP_LOOPENTRY ops.
  using OSREntryVector =
      Vector<BaselineScript::OSREntry, 16, SystemAllocPolicy>;
  OSREntryVector osrEntries_;

  JSScript* script_;
  jsbytecode* pc_;

  // Index of the current ICEntry in the script's JitScript.
  uint32_t icEntryIndex_;

  bool compileDebugInstrumentation_;
  bool ionCompileable_;

 public:
  using FrameInfoT = CompilerFrameInfo;

  BaselineCompilerHandler(JSContext* cx, MacroAssembler& masm,
                          TempAllocator& alloc, JSScript* script);

  MOZ_MUST_USE bool init(JSContext* cx);

  CompilerFrameInfo& frame() { return frame_; }

  jsbytecode* pc() const { return pc_; }
  jsbytecode* maybePC() const { return pc_; }

  void moveToNextPC() { pc_ += GetBytecodeLength(pc_); }
  Label* labelOf(jsbytecode* pc) { return &labels_[script_->pcToOffset(pc)]; }

  bool isDefinitelyLastOp() const { return pc_ == script_->lastPC(); }

  bool shouldEmitDebugEpilogueAtReturnOp() const {
    // The JIT uses the return address -> pc mapping and bakes in the pc
    // argument so the DebugEpilogue call needs to be part of the returning
    // bytecode op for this to work.
    return true;
  }

  JSScript* script() const { return script_; }
  JSScript* maybeScript() const { return script_; }

  JSFunction* function() const {
    // Not delazifying here is ok as the function is guaranteed to have
    // been delazified before compilation started.
    return script_->functionNonDelazifying();
  }
  JSFunction* maybeFunction() const { return function(); }

  ModuleObject* module() const { return script_->module(); }

  void setCompileDebugInstrumentation() { compileDebugInstrumentation_ = true; }
  bool compileDebugInstrumentation() const {
    return compileDebugInstrumentation_;
  }

  bool maybeIonCompileable() const { return ionCompileable_; }

  uint32_t icEntryIndex() const { return icEntryIndex_; }
  void moveToNextICEntry() { icEntryIndex_++; }

  BytecodeAnalysis& analysis() { return analysis_; }

  RetAddrEntryVector& retAddrEntries() { return retAddrEntries_; }
  OSREntryVector& osrEntries() { return osrEntries_; }

  MOZ_MUST_USE bool recordCallRetAddr(JSContext* cx, RetAddrEntry::Kind kind,
                                      uint32_t retOffset);

  // If a script has more |nslots| than this the stack check must account
  // for these slots explicitly.
  bool mustIncludeSlotsInStackCheck() const {
    static constexpr size_t NumSlotsLimit = 128;
    return script()->nslots() > NumSlotsLimit;
  }

  JSObject* maybeNoCloneSingletonObject();
};

using BaselineCompilerCodeGen = BaselineCodeGen<BaselineCompilerHandler>;

class BaselineCompiler final : private BaselineCompilerCodeGen {
  // Native code offsets for bytecode ops in the script's resume offsets list.
  ResumeOffsetEntryVector resumeOffsetEntries_;

  // Native code offsets for debug traps if the script is compiled with debug
  // instrumentation.
  using DebugTrapEntryVector =
      Vector<BaselineScript::DebugTrapEntry, 0, SystemAllocPolicy>;
  DebugTrapEntryVector debugTrapEntries_;

  CodeOffset profilerPushToggleOffset_;

  CodeOffset traceLoggerScriptTextIdOffset_;

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
  MethodStatus emitBody();

  MOZ_MUST_USE bool emitDebugTrap();
};

// Interface used by BaselineCodeGen for BaselineInterpreterGenerator.
class BaselineInterpreterHandler {
  InterpreterFrameInfo frame_;

  // Entry point to start interpreting a bytecode op. No registers are live. PC
  // is loaded from the frame.
  Label interpretOp_;

  // Like interpretOp_ but at this point the PC is expected to be in
  // InterpreterPCReg.
  Label interpretOpWithPCReg_;

  // Offsets of toggled jumps for debugger instrumentation.
  using CodeOffsetVector = Vector<uint32_t, 0, SystemAllocPolicy>;
  CodeOffsetVector debugInstrumentationOffsets_;

  // Offsets of toggled jumps for code coverage instrumentation.
  CodeOffsetVector codeCoverageOffsets_;
  Label codeCoverageAtPrologueLabel_;
  Label codeCoverageAtPCLabel_;

  // Offsets of IC calls for IsIonInlinableOp ops, for Ion bailouts.
  BaselineInterpreter::ICReturnOffsetVector icReturnOffsets_;

  // Offsets of some callVMs for BaselineDebugModeOSR.
  BaselineInterpreter::CallVMOffsets callVMOffsets_;

  // Offset of the GeneratorThrowOrReturn callVM. See also
  // emitGeneratorThrowOrReturnCallVM.
  uint32_t generatorThrowOrReturnCallOffset_ = 0;

  // The current JSOp we are emitting interpreter code for.
  mozilla::Maybe<JSOp> currentOp_;

 public:
  using FrameInfoT = InterpreterFrameInfo;

  explicit BaselineInterpreterHandler(JSContext* cx, MacroAssembler& masm);

  InterpreterFrameInfo& frame() { return frame_; }

  Label* interpretOpLabel() { return &interpretOp_; }
  Label* interpretOpWithPCRegLabel() { return &interpretOpWithPCReg_; }

  Label* codeCoverageAtPrologueLabel() { return &codeCoverageAtPrologueLabel_; }
  Label* codeCoverageAtPCLabel() { return &codeCoverageAtPCLabel_; }

  CodeOffsetVector& debugInstrumentationOffsets() {
    return debugInstrumentationOffsets_;
  }
  CodeOffsetVector& codeCoverageOffsets() { return codeCoverageOffsets_; }

  BaselineInterpreter::ICReturnOffsetVector& icReturnOffsets() {
    return icReturnOffsets_;
  }

  uint32_t generatorThrowOrReturnCallOffset() const {
    return generatorThrowOrReturnCallOffset_;
  }
  void setGeneratorThrowOrReturnCallOffset(uint32_t offset) {
    generatorThrowOrReturnCallOffset_ = offset;
  }

  void setCurrentOp(JSOp op) { currentOp_.emplace(op); }
  void resetCurrentOp() { currentOp_.reset(); }
  mozilla::Maybe<JSOp> currentOp() const { return currentOp_; }

  // Interpreter doesn't know the script and pc statically.
  jsbytecode* maybePC() const { return nullptr; }
  bool isDefinitelyLastOp() const { return false; }
  JSScript* maybeScript() const { return nullptr; }
  JSFunction* maybeFunction() const { return nullptr; }

  bool shouldEmitDebugEpilogueAtReturnOp() const {
    // The interpreter doesn't use the return address -> pc mapping and doesn't
    // bake in bytecode PCs so it can emit a shared DebugEpilogue call instead
    // of duplicating it for every return op.
    return false;
  }

  MOZ_MUST_USE bool addDebugInstrumentationOffset(CodeOffset offset) {
    return debugInstrumentationOffsets_.append(offset.offset());
  }

  const BaselineInterpreter::CallVMOffsets& callVMOffsets() const {
    return callVMOffsets_;
  }

  MOZ_MUST_USE bool recordCallRetAddr(JSContext* cx, RetAddrEntry::Kind kind,
                                      uint32_t retOffset);

  bool maybeIonCompileable() const { return true; }

  // The interpreter doesn't know the number of slots statically so we always
  // include them.
  bool mustIncludeSlotsInStackCheck() const { return true; }

  JSObject* maybeNoCloneSingletonObject() { return nullptr; }
};

using BaselineInterpreterCodeGen = BaselineCodeGen<BaselineInterpreterHandler>;

class BaselineInterpreterGenerator final : private BaselineInterpreterCodeGen {
  // Offsets of patchable call instructions for debugger breakpoints/stepping.
  Vector<uint32_t, 0, SystemAllocPolicy> debugTrapOffsets_;

  // Offsets of move instructions for tableswitch base address.
  Vector<CodeOffset, 0, SystemAllocPolicy> tableLabels_;

  // Offset of the first tableswitch entry.
  uint32_t tableOffset_ = 0;

  // Offset of the code to start interpreting a bytecode op.
  uint32_t interpretOpOffset_ = 0;

  // Like interpretOpOffset_ but skips the debug trap for the current op.
  uint32_t interpretOpNoDebugTrapOffset_ = 0;

  // Offset of the jump (tail call) to the debug trap handler trampoline code.
  // When the debugger is enabled, NOPs are patched to calls to this location.
  uint32_t debugTrapHandlerOffset_ = 0;

 public:
  explicit BaselineInterpreterGenerator(JSContext* cx);

  MOZ_MUST_USE bool generate(BaselineInterpreter& interpreter);

 private:
  MOZ_MUST_USE bool emitInterpreterLoop();
  MOZ_MUST_USE bool emitDebugTrap();

  void emitOutOfLineCodeCoverageInstrumentation();
};

}  // namespace jit
}  // namespace js

#endif /* jit_BaselineCodeGen_h */
