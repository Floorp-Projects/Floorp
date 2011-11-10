/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#if !defined jsjaeger_compiler_h__ && defined JS_METHODJIT
#define jsjaeger_compiler_h__

#include "jsanalyze.h"
#include "jscntxt.h"
#include "MethodJIT.h"
#include "CodeGenIncludes.h"
#include "BaseCompiler.h"
#include "StubCompiler.h"
#include "MonoIC.h"
#include "PolyIC.h"

namespace js {
namespace mjit {

/*
 * Patch for storing call site and rejoin site return addresses at, for
 * redirecting the return address in InvariantFailure.
 */
struct InvariantCodePatch {
    bool hasPatch;
    JSC::MacroAssembler::DataLabelPtr codePatch;
    InvariantCodePatch() : hasPatch(false) {}
};

class Compiler : public BaseCompiler
{
    friend class StubCompiler;

    struct BranchPatch {
        BranchPatch(const Jump &j, jsbytecode *pc, uint32 inlineIndex)
          : jump(j), pc(pc), inlineIndex(inlineIndex)
        { }

        Jump jump;
        jsbytecode *pc;
        uint32 inlineIndex;
    };

#if defined JS_MONOIC
    struct GlobalNameICInfo {
        Label fastPathStart;
        Call slowPathCall;
        DataLabel32 shape;
        DataLabelPtr addrLabel;
        bool usePropertyCache;

        void copyTo(ic::GlobalNameIC &to, JSC::LinkBuffer &full, JSC::LinkBuffer &stub) {
            to.fastPathStart = full.locationOf(fastPathStart);

            int offset = full.locationOf(shape) - to.fastPathStart;
            to.shapeOffset = offset;
            JS_ASSERT(to.shapeOffset == offset);

            to.slowPathCall = stub.locationOf(slowPathCall);
            to.usePropertyCache = usePropertyCache;
        }
    };

    struct GetGlobalNameICInfo : public GlobalNameICInfo {
        Label load;
    };

    struct SetGlobalNameICInfo : public GlobalNameICInfo {
        Label slowPathStart;
        Label fastPathRejoin;
        DataLabel32 store;
        Jump shapeGuardJump;
        ValueRemat vr;
        RegisterID objReg;
        RegisterID shapeReg;
        bool objConst;
    };

    struct EqualityGenInfo {
        DataLabelPtr addrLabel;
        Label stubEntry;
        Call stubCall;
        BoolStub stub;
        MaybeJump jumpToStub;
        Label fallThrough;
        jsbytecode *jumpTarget;
        bool trampoline;
        Label trampolineStart;
        ValueRemat lvr, rvr;
        Assembler::Condition cond;
        JSC::MacroAssembler::RegisterID tempReg;
    };

    /* InlineFrameAssembler wants to see this. */
  public:
    struct CallGenInfo {
        /*
         * These members map to members in CallICInfo. See that structure for
         * more comments.
         */
        uint32       callIndex;
        DataLabelPtr funGuard;
        Jump         funJump;
        Jump         hotJump;
        Call         oolCall;
        Label        joinPoint;
        Label        slowJoinPoint;
        Label        slowPathStart;
        Label        hotPathLabel;
        DataLabelPtr addrLabel1;
        DataLabelPtr addrLabel2;
        Jump         oolJump;
        Label        icCall;
        RegisterID   funObjReg;
        RegisterID   funPtrReg;
        FrameSize    frameSize;
        bool         typeMonitored;
    };

  private:
#endif

    /*
     * Writes of call return addresses which needs to be delayed until the final
     * absolute address of the join point is known.
     */
    struct CallPatchInfo {
        CallPatchInfo() : hasFastNcode(false), hasSlowNcode(false), joinSlow(false) {}
        Label joinPoint;
        DataLabelPtr fastNcodePatch;
        DataLabelPtr slowNcodePatch;
        bool hasFastNcode;
        bool hasSlowNcode;
        bool joinSlow;
    };

    struct BaseICInfo {
        BaseICInfo(JSOp op) : op(op), canCallHook(false), forcedTypeBarrier(false)
        { }
        Label fastPathStart;
        Label fastPathRejoin;
        Label slowPathStart;
        Call slowPathCall;
        DataLabelPtr paramAddr;
        JSOp op;
        bool canCallHook;
        bool forcedTypeBarrier;

        void copyTo(ic::BaseIC &to, JSC::LinkBuffer &full, JSC::LinkBuffer &stub) {
            to.fastPathStart = full.locationOf(fastPathStart);
            to.fastPathRejoin = full.locationOf(fastPathRejoin);
            to.slowPathStart = stub.locationOf(slowPathStart);
            to.slowPathCall = stub.locationOf(slowPathCall);
            to.canCallHook = canCallHook;
            to.forcedTypeBarrier = forcedTypeBarrier;
            to.op = op;
            JS_ASSERT(to.op == op);
        }
    };

    struct GetElementICInfo : public BaseICInfo {
        GetElementICInfo(JSOp op) : BaseICInfo(op)
        { }
        RegisterID  typeReg;
        RegisterID  objReg;
        ValueRemat  id;
        MaybeJump   typeGuard;
        Jump        claspGuard;
    };

    struct SetElementICInfo : public BaseICInfo {
        SetElementICInfo(JSOp op) : BaseICInfo(op)
        { }
        RegisterID  objReg;
        StateRemat  objRemat;
        ValueRemat  vr;
        Jump        capacityGuard;
        Jump        claspGuard;
        Jump        holeGuard;
        Int32Key    key;
        uint32      volatileMask;
    };

    struct PICGenInfo : public BaseICInfo {
        PICGenInfo(ic::PICInfo::Kind kind, JSOp op, bool usePropCache)
          : BaseICInfo(op), kind(kind), usePropCache(usePropCache), typeMonitored(false)
        { }
        ic::PICInfo::Kind kind;
        Label typeCheck;
        RegisterID shapeReg;
        RegisterID objReg;
        RegisterID typeReg;
        bool usePropCache;
        Label shapeGuard;
        jsbytecode *pc;
        JSAtom *atom;
        bool hasTypeCheck;
        bool typeMonitored;
        types::TypeSet *rhsTypes;
        ValueRemat vr;
#ifdef JS_HAS_IC_LABELS
        union {
            ic::GetPropLabels getPropLabels_;
            ic::SetPropLabels setPropLabels_;
            ic::BindNameLabels bindNameLabels_;
            ic::ScopeNameLabels scopeNameLabels_;
        };

        ic::GetPropLabels &getPropLabels() {
            JS_ASSERT(kind == ic::PICInfo::GET || kind == ic::PICInfo::CALL);
            return getPropLabels_;
        }
        ic::SetPropLabels &setPropLabels() {
            JS_ASSERT(kind == ic::PICInfo::SET || kind == ic::PICInfo::SETMETHOD);
            return setPropLabels_;
        }
        ic::BindNameLabels &bindNameLabels() {
            JS_ASSERT(kind == ic::PICInfo::BIND);
            return bindNameLabels_;
        }
        ic::ScopeNameLabels &scopeNameLabels() {
            JS_ASSERT(kind == ic::PICInfo::NAME || kind == ic::PICInfo::CALLNAME ||
                      kind == ic::PICInfo::XNAME);
            return scopeNameLabels_;
        }
#else
        ic::GetPropLabels &getPropLabels() {
            JS_ASSERT(kind == ic::PICInfo::GET || kind == ic::PICInfo::CALL);
            return ic::PICInfo::getPropLabels_;
        }
        ic::SetPropLabels &setPropLabels() {
            JS_ASSERT(kind == ic::PICInfo::SET || kind == ic::PICInfo::SETMETHOD);
            return ic::PICInfo::setPropLabels_;
        }
        ic::BindNameLabels &bindNameLabels() {
            JS_ASSERT(kind == ic::PICInfo::BIND);
            return ic::PICInfo::bindNameLabels_;
        }
        ic::ScopeNameLabels &scopeNameLabels() {
            JS_ASSERT(kind == ic::PICInfo::NAME || kind == ic::PICInfo::CALLNAME ||
                      kind == ic::PICInfo::XNAME);
            return ic::PICInfo::scopeNameLabels_;
        }
#endif

        void copySimpleMembersTo(ic::PICInfo &ic) {
            ic.kind = kind;
            ic.shapeReg = shapeReg;
            ic.objReg = objReg;
            ic.atom = atom;
            ic.usePropCache = usePropCache;
            if (ic.isSet()) {
                ic.u.vr = vr;
            } else if (ic.isGet()) {
                ic.u.get.typeReg = typeReg;
                ic.u.get.hasTypeCheck = hasTypeCheck;
            }
            ic.typeMonitored = typeMonitored;
            ic.rhsTypes = rhsTypes;
#ifdef JS_HAS_IC_LABELS
            if (ic.isGet())
                ic.setLabels(getPropLabels());
            else if (ic.isSet())
                ic.setLabels(setPropLabels());
            else if (ic.isBind())
                ic.setLabels(bindNameLabels());
            else if (ic.isScopeName())
                ic.setLabels(scopeNameLabels());
#endif
        }

    };

    struct Defs {
        Defs(uint32 ndefs)
          : ndefs(ndefs)
        { }
        uint32 ndefs;
    };

    struct InternalCallSite {
        uint32 returnOffset;
        DataLabelPtr inlinePatch;
        uint32 inlineIndex;
        jsbytecode *inlinepc;
        RejoinState rejoin;
        bool ool;
        Label loopJumpLabel;
        InvariantCodePatch loopPatch;

        InternalCallSite(uint32 returnOffset,
                         uint32 inlineIndex, jsbytecode *inlinepc,
                         RejoinState rejoin, bool ool)
          : returnOffset(returnOffset),
            inlineIndex(inlineIndex), inlinepc(inlinepc),
            rejoin(rejoin), ool(ool)
        { }
    };

    struct DoublePatch {
        double d;
        DataLabelPtr label;
        bool ool;
    };

    struct JumpTable {
        DataLabelPtr label;
        size_t offsetIndex;
    };

    struct LoopEntry {
        uint32 pcOffset;
        Label label;
    };

    struct VarType {
        JSValueType type;
        types::TypeSet *types;
    };

    struct SlotType
    {
        uint32 slot;
        VarType vt;
        SlotType(uint32 slot, VarType vt) : slot(slot), vt(vt) {}
    };

    JSScript *outerScript;
    bool isConstructing;

    /* SSA information for the outer script and all frames we will be inlining. */
    analyze::CrossScriptSSA ssa;

    GlobalObject *globalObj;
    const HeapValue *globalSlots;  /* Original slots pointer. */

    Assembler masm;
    FrameState frame;

    /*
     * State for the current stack frame, and links to its parents going up to
     * the outermost script.
     */

public:
    struct ActiveFrame {
        ActiveFrame *parent;
        jsbytecode *parentPC;
        JSScript *script;
        Label *jumpMap;

        /*
         * Index into inlineFrames or OUTER_FRAME, matches this frame's index
         * in the cross script SSA.
         */
        uint32 inlineIndex;

        /* Current types for non-escaping vars in the script. */
        VarType *varTypes;

        /* JIT code generation tracking state */
        size_t mainCodeStart;
        size_t stubCodeStart;
        size_t inlinePCOffset;

        /* State for managing return from inlined frames. */
        bool needReturnValue;          /* Return value will be used. */
        bool syncReturnValue;          /* Return value should be fully synced. */
        bool returnValueDouble;        /* Return value should be a double. */
        bool returnSet;                /* Whether returnRegister is valid. */
        AnyRegisterID returnRegister;  /* Register holding return value. */
        const FrameEntry *returnEntry; /* Entry copied by return value. */
        Vector<Jump, 4, CompilerAllocPolicy> *returnJumps;

        /*
         * Snapshot of the heap state to use after the call, in case
         * there are multiple return paths the inlined frame could take.
         */
        RegisterAllocation *exitState;

        ActiveFrame(JSContext *cx);
        ~ActiveFrame();
    };

private:
    ActiveFrame *a;
    ActiveFrame *outer;

    JSScript *script;
    analyze::ScriptAnalysis *analysis;
    jsbytecode *PC;

    LoopState *loop;

    /* State spanning all stack frames. */

    js::Vector<ActiveFrame*, 4, CompilerAllocPolicy> inlineFrames;
    js::Vector<BranchPatch, 64, CompilerAllocPolicy> branchPatches;
#if defined JS_MONOIC
    js::Vector<GetGlobalNameICInfo, 16, CompilerAllocPolicy> getGlobalNames;
    js::Vector<SetGlobalNameICInfo, 16, CompilerAllocPolicy> setGlobalNames;
    js::Vector<CallGenInfo, 64, CompilerAllocPolicy> callICs;
    js::Vector<EqualityGenInfo, 64, CompilerAllocPolicy> equalityICs;
#endif
#if defined JS_POLYIC
    js::Vector<PICGenInfo, 16, CompilerAllocPolicy> pics;
    js::Vector<GetElementICInfo, 16, CompilerAllocPolicy> getElemICs;
    js::Vector<SetElementICInfo, 16, CompilerAllocPolicy> setElemICs;
#endif
    js::Vector<CallPatchInfo, 64, CompilerAllocPolicy> callPatches;
    js::Vector<InternalCallSite, 64, CompilerAllocPolicy> callSites;
    js::Vector<DoublePatch, 16, CompilerAllocPolicy> doubleList;
    js::Vector<uint32> fixedIntToDoubleEntries;
    js::Vector<uint32> fixedDoubleToAnyEntries;
    js::Vector<JumpTable, 16> jumpTables;
    js::Vector<uint32, 16> jumpTableOffsets;
    js::Vector<LoopEntry, 16> loopEntries;
    StubCompiler stubcc;
    Label invokeLabel;
    Label arityLabel;
    Label argsCheckLabel;
#ifdef JS_MONOIC
    Label argsCheckStub;
    Label argsCheckFallthrough;
    Jump argsCheckJump;
#endif
    bool debugMode_;
    bool inlining_;
    bool hasGlobalReallocation;
    bool oomInVector;       // True if we have OOM'd appending to a vector. 
    bool overflowICSpace;   // True if we added a constant pool in a reserved space.
    uint32 gcNumber;
    enum { NoApplyTricks, LazyArgsObj } applyTricks;
    PCLengthEntry *pcLengths;

    Compiler *thisFromCtor() { return this; }

    friend class CompilerAllocPolicy;
  public:
    Compiler(JSContext *cx, JSScript *outerScript, bool isConstructing);
    ~Compiler();

    CompileStatus compile();

    Label getLabel() { return masm.label(); }
    bool knownJump(jsbytecode *pc);
    Label labelOf(jsbytecode *target, uint32 inlineIndex);
    void addCallSite(const InternalCallSite &callSite);
    void addReturnSite();
    void inlineStubCall(void *stub, RejoinState rejoin, Uses uses);

    bool debugMode() { return debugMode_; }
    bool inlining() { return inlining_; }
    bool constructing() { return isConstructing; }

    jsbytecode *outerPC() {
        if (a == outer)
            return PC;
        ActiveFrame *scan = a;
        while (scan && scan->parent != outer)
            scan = scan->parent;
        return scan->parentPC;
    }

    jsbytecode *inlinePC() { return PC; }
    uint32 inlineIndex() { return a->inlineIndex; }

    Assembler &getAssembler(bool ool) { return ool ? stubcc.masm : masm; }

    InvariantCodePatch *getInvariantPatch(unsigned index) {
        return &callSites[index].loopPatch;
    }
    jsbytecode *getInvariantPC(unsigned index) {
        return callSites[index].inlinepc;
    }

    bool arrayPrototypeHasIndexedProperty();

    bool activeFrameHasMultipleExits() {
        ActiveFrame *na = a;
        while (na->parent) {
            if (na->exitState)
                return true;
            na = na->parent;
        }
        return false;
    }

  private:
    CompileStatus performCompilation(JITScript **jitp);
    CompileStatus generatePrologue();
    CompileStatus generateMethod();
    CompileStatus generateEpilogue();
    CompileStatus finishThisUp(JITScript **jitp);
    CompileStatus pushActiveFrame(JSScript *script, uint32 argc);
    void popActiveFrame();
    void updatePCCounters(jsbytecode *pc, Label *start, bool *updated);
    void updatePCTypes(jsbytecode *pc, FrameEntry *fe);
    void updateArithCounters(jsbytecode *pc, FrameEntry *fe,
                             JSValueType firstUseType, JSValueType secondUseType);
    void updateElemCounters(jsbytecode *pc, FrameEntry *obj, FrameEntry *id);
    void bumpPropCounter(jsbytecode *pc, int counter);

    /* Analysis helpers. */
    CompileStatus prepareInferenceTypes(JSScript *script, ActiveFrame *a);
    void ensureDoubleArguments();
    void markUndefinedLocals();
    void fixDoubleTypes(jsbytecode *target);
    void watchGlobalReallocation();
    void updateVarType();
    void updateJoinVarTypes();
    void restoreVarType();
    JSValueType knownPushedType(uint32 pushed);
    bool mayPushUndefined(uint32 pushed);
    types::TypeSet *pushedTypeSet(uint32 which);
    bool monitored(jsbytecode *pc);
    bool hasTypeBarriers(jsbytecode *pc);
    bool testSingletonProperty(JSObject *obj, jsid id);
    bool testSingletonPropertyTypes(FrameEntry *top, jsid id, bool *testObject);
    CompileStatus addInlineFrame(JSScript *script, uint32 depth, uint32 parent, jsbytecode *parentpc);
    CompileStatus scanInlineCalls(uint32 index, uint32 depth);
    CompileStatus checkAnalysis(JSScript *script);

    struct BarrierState {
        MaybeJump jump;
        RegisterID typeReg;
        RegisterID dataReg;
    };

    MaybeJump trySingleTypeTest(types::TypeSet *types, RegisterID typeReg);
    Jump addTypeTest(types::TypeSet *types, RegisterID typeReg, RegisterID dataReg);
    BarrierState pushAddressMaybeBarrier(Address address, JSValueType type, bool reuseBase,
                                         bool testUndefined = false);
    BarrierState testBarrier(RegisterID typeReg, RegisterID dataReg,
                             bool testUndefined = false, bool testReturn = false,
                             bool force = false);
    void finishBarrier(const BarrierState &barrier, RejoinState rejoin, uint32 which);

    void testPushedType(RejoinState rejoin, int which, bool ool = true);

    /* Non-emitting helpers. */
    void pushSyncedEntry(uint32 pushed);
    uint32 fullAtomIndex(jsbytecode *pc);
    bool jumpInScript(Jump j, jsbytecode *pc);
    bool compareTwoValues(JSContext *cx, JSOp op, const Value &lhs, const Value &rhs);
    bool canUseApplyTricks();

    /* Emitting helpers. */
    bool constantFoldBranch(jsbytecode *target, bool taken);
    bool emitStubCmpOp(BoolStub stub, jsbytecode *target, JSOp fused);
    bool iter(uintN flags);
    void iterNext(ptrdiff_t offset);
    bool iterMore(jsbytecode *target);
    void iterEnd();
    MaybeJump loadDouble(FrameEntry *fe, FPRegisterID *fpReg, bool *allocated);
#ifdef JS_POLYIC
    void passICAddress(BaseICInfo *ic);
#endif
#ifdef JS_MONOIC
    void passMICAddress(GlobalNameICInfo &mic);
#endif
    bool constructThis();
    void ensureDouble(FrameEntry *fe);

    /*
     * Ensure fe is an integer, truncating from double if necessary, or jump to
     * the slow path per uses.
     */
    void ensureInteger(FrameEntry *fe, Uses uses);

    /* Convert fe from a double to integer (per ValueToECMAInt32) in place. */
    void truncateDoubleToInt32(FrameEntry *fe, Uses uses);

    /*
     * Try to convert a double fe to an integer, with no truncation performed,
     * or jump to the slow path per uses.
     */
    void tryConvertInteger(FrameEntry *fe, Uses uses);

    /* Opcode handlers. */
    bool jumpAndTrace(Jump j, jsbytecode *target, Jump *slow = NULL, bool *trampoline = NULL);
    bool startLoop(jsbytecode *head, Jump entry, jsbytecode *entryTarget);
    bool finishLoop(jsbytecode *head);
    void jsop_bindname(JSAtom *atom, bool usePropCache);
    void jsop_setglobal(uint32 index);
    void jsop_getprop_slow(JSAtom *atom, bool usePropCache = true);
    void jsop_getarg(uint32 slot);
    void jsop_setarg(uint32 slot, bool popped);
    void jsop_this();
    void emitReturn(FrameEntry *fe);
    void emitFinalReturn(Assembler &masm);
    void loadReturnValue(Assembler *masm, FrameEntry *fe);
    void emitReturnValue(Assembler *masm, FrameEntry *fe);
    void emitInlineReturnValue(FrameEntry *fe);
    void dispatchCall(VoidPtrStubUInt32 stub, uint32 argc);
    void interruptCheckHelper();
    void recompileCheckHelper();
    void emitUncachedCall(uint32 argc, bool callingNew);
    void checkCallApplySpeculation(uint32 callImmArgc, uint32 speculatedArgc,
                                   FrameEntry *origCallee, FrameEntry *origThis,
                                   MaybeRegisterID origCalleeType, RegisterID origCalleeData,
                                   MaybeRegisterID origThisType, RegisterID origThisData,
                                   Jump *uncachedCallSlowRejoin, CallPatchInfo *uncachedCallPatch);
    bool inlineCallHelper(uint32 argc, bool callingNew, FrameSize &callFrameSize);
    void fixPrimitiveReturn(Assembler *masm, FrameEntry *fe);
    void jsop_getgname(uint32 index);
    void jsop_getgname_slow(uint32 index);
    void jsop_callgname_epilogue();
    void jsop_setgname(JSAtom *atom, bool usePropertyCache, bool popGuaranteed);
    void jsop_setgname_slow(JSAtom *atom, bool usePropertyCache);
    void jsop_bindgname();
    void jsop_setelem_slow();
    void jsop_getelem_slow();
    void jsop_callelem_slow();
    void jsop_unbrand();
    bool jsop_getprop(JSAtom *atom, JSValueType type,
                      bool typeCheck = true, bool usePropCache = true);
    bool jsop_setprop(JSAtom *atom, bool usePropCache, bool popGuaranteed);
    void jsop_setprop_slow(JSAtom *atom, bool usePropCache = true);
    bool jsop_callprop_slow(JSAtom *atom);
    bool jsop_callprop(JSAtom *atom);
    bool jsop_callprop_obj(JSAtom *atom);
    bool jsop_callprop_str(JSAtom *atom);
    bool jsop_callprop_generic(JSAtom *atom);
    bool jsop_callprop_dispatch(JSAtom *atom);
    bool jsop_instanceof();
    void jsop_name(JSAtom *atom, JSValueType type, bool isCall);
    bool jsop_xname(JSAtom *atom);
    void enterBlock(JSObject *obj);
    void leaveBlock();
    void emitEval(uint32 argc);
    void jsop_arguments(RejoinState rejoin);
    bool jsop_tableswitch(jsbytecode *pc);

    /* Fast arithmetic. */
    bool jsop_binary(JSOp op, VoidStub stub, JSValueType type, types::TypeSet *typeSet);
    void jsop_binary_full(FrameEntry *lhs, FrameEntry *rhs, JSOp op, VoidStub stub,
                          JSValueType type, bool cannotOverflow, bool ignoreOverflow);
    void jsop_binary_full_simple(FrameEntry *fe, JSOp op, VoidStub stub,
                                 JSValueType type);
    void jsop_binary_double(FrameEntry *lhs, FrameEntry *rhs, JSOp op, VoidStub stub,
                            JSValueType type);
    void slowLoadConstantDouble(Assembler &masm, FrameEntry *fe,
                                FPRegisterID fpreg);
    void maybeJumpIfNotInt32(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                             MaybeRegisterID &mreg);
    void maybeJumpIfNotDouble(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                              MaybeRegisterID &mreg);
    bool jsop_relational(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    bool jsop_relational_full(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    bool jsop_relational_double(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    bool jsop_relational_int(JSOp op, jsbytecode *target, JSOp fused);

    void emitLeftDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                            MaybeJump &lhsNotDouble, MaybeJump &rhsNotNumber,
                            MaybeJump &lhsUnknownDone);
    void emitRightDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                             MaybeJump &rhsNotNumber2);
    bool tryBinaryConstantFold(JSContext *cx, FrameState &frame, JSOp op,
                               FrameEntry *lhs, FrameEntry *rhs, Value *vp);

    /* Fast opcodes. */
    void jsop_bitop(JSOp op);
    bool jsop_mod();
    void jsop_neg();
    void jsop_bitnot();
    void jsop_not();
    void jsop_typeof();
    bool booleanJumpScript(JSOp op, jsbytecode *target);
    bool jsop_ifneq(JSOp op, jsbytecode *target);
    bool jsop_andor(JSOp op, jsbytecode *target);
    bool jsop_arginc(JSOp op, uint32 slot);
    bool jsop_localinc(JSOp op, uint32 slot);
    bool jsop_newinit();
    bool jsop_regexp();
    void jsop_initmethod();
    void jsop_initprop();
    void jsop_initelem();
    void jsop_setelem_dense();
#ifdef JS_METHODJIT_TYPED_ARRAY
    void jsop_setelem_typed(int atype);
    void convertForTypedArray(int atype, ValueRemat *vr, bool *allocated);
#endif
    bool jsop_setelem(bool popGuaranteed);
    bool jsop_getelem(bool isCall);
    void jsop_getelem_dense(bool isPacked);
    void jsop_getelem_args();
#ifdef JS_METHODJIT_TYPED_ARRAY
    bool jsop_getelem_typed(int atype);
#endif
    void jsop_toid();
    bool isCacheableBaseAndIndex(FrameEntry *obj, FrameEntry *id);
    void jsop_stricteq(JSOp op);
    bool jsop_equality(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    CompileStatus jsop_equality_obj_obj(JSOp op, jsbytecode *target, JSOp fused);
    bool jsop_equality_int_string(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    void jsop_pos();

    static inline Assembler::Condition
    GetCompareCondition(JSOp op, JSOp fused)
    {
        bool ifeq = fused == JSOP_IFEQ;
        switch (op) {
          case JSOP_GT:
            return ifeq ? Assembler::LessThanOrEqual : Assembler::GreaterThan;
          case JSOP_GE:
            return ifeq ? Assembler::LessThan : Assembler::GreaterThanOrEqual;
          case JSOP_LT:
            return ifeq ? Assembler::GreaterThanOrEqual : Assembler::LessThan;
          case JSOP_LE:
            return ifeq ? Assembler::GreaterThan : Assembler::LessThanOrEqual;
          case JSOP_STRICTEQ:
          case JSOP_EQ:
            return ifeq ? Assembler::NotEqual : Assembler::Equal;
          case JSOP_STRICTNE:
          case JSOP_NE:
            return ifeq ? Assembler::Equal : Assembler::NotEqual;
          default:
            JS_NOT_REACHED("unrecognized op");
            return Assembler::Equal;
        }
    }

    static inline Assembler::Condition
    GetStubCompareCondition(JSOp fused)
    {
        return (fused == JSOP_IFEQ) ? Assembler::Zero : Assembler::NonZero;
    }

    /* Fast builtins. */
    JSObject *pushedSingleton(unsigned pushed);
    CompileStatus callArrayBuiltin(uint32 argc, bool callingNew);
    CompileStatus inlineNativeFunction(uint32 argc, bool callingNew);
    CompileStatus inlineScriptedFunction(uint32 argc, bool callingNew);
    CompileStatus compileMathAbsInt(FrameEntry *arg);
    CompileStatus compileMathAbsDouble(FrameEntry *arg);
    CompileStatus compileMathSqrt(FrameEntry *arg);
    CompileStatus compileMathMinMaxDouble(FrameEntry *arg1, FrameEntry *arg2, 
                                          Assembler::DoubleCondition cond); 
    CompileStatus compileMathMinMaxInt(FrameEntry *arg1, FrameEntry *arg2, 
                                       Assembler::Condition cond);                                       
    CompileStatus compileMathPowSimple(FrameEntry *arg1, FrameEntry *arg2);
    CompileStatus compileArrayPush(FrameEntry *thisv, FrameEntry *arg);
    CompileStatus compileArrayConcat(types::TypeSet *thisTypes, types::TypeSet *argTypes,
                                     FrameEntry *thisValue, FrameEntry *argValue);
    CompileStatus compileArrayPopShift(FrameEntry *thisv, bool isPacked, bool isArrayPop);
    CompileStatus compileArrayWithLength(uint32 argc);
    CompileStatus compileArrayWithArgs(uint32 argc);

    enum RoundingMode { Floor, Round };
    CompileStatus compileRound(FrameEntry *arg, RoundingMode mode);

    enum GetCharMode { GetChar, GetCharCode };
    CompileStatus compileGetChar(FrameEntry *thisValue, FrameEntry *arg, GetCharMode mode);
    
    CompileStatus compileStringFromCode(FrameEntry *arg);

    void prepareStubCall(Uses uses);
    Call emitStubCall(void *ptr, DataLabelPtr *pinline);
};

// Given a stub call, emits the call into the inline assembly path. rejoin
// indicates how to rejoin should this call trigger expansion/discarding.
#define INLINE_STUBCALL(stub, rejoin)                                       \
    inlineStubCall(JS_FUNC_TO_DATA_PTR(void *, (stub)), rejoin, Uses(0))
#define INLINE_STUBCALL_USES(stub, rejoin, uses)                            \
    inlineStubCall(JS_FUNC_TO_DATA_PTR(void *, (stub)), rejoin, uses)

// Given a stub call, emits the call into the out-of-line assembly path.
// Unlike the INLINE_STUBCALL variant, this returns the Call offset.
#define OOL_STUBCALL(stub, rejoin)                                          \
    stubcc.emitStubCall(JS_FUNC_TO_DATA_PTR(void *, (stub)), rejoin, Uses(0))
#define OOL_STUBCALL_USES(stub, rejoin, uses)                               \
    stubcc.emitStubCall(JS_FUNC_TO_DATA_PTR(void *, (stub)), rejoin, uses)

// Same as OOL_STUBCALL, but specifies a slot depth.
#define OOL_STUBCALL_LOCAL_SLOTS(stub, rejoin, slots)                       \
    stubcc.emitStubCall(JS_FUNC_TO_DATA_PTR(void *, (stub)), rejoin, Uses(0), (slots))

} /* namespace js */
} /* namespace mjit */

#endif

