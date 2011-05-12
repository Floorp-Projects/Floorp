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
#include "jstl.h"
#include "MethodJIT.h"
#include "CodeGenIncludes.h"
#include "BaseCompiler.h"
#include "StubCompiler.h"
#include "MonoIC.h"
#include "PolyIC.h"

namespace js {
namespace mjit {

class Compiler : public BaseCompiler
{
    friend class StubCompiler;

    struct BranchPatch {
        BranchPatch(const Jump &j, jsbytecode *pc)
          : jump(j), pc(pc)
        { }

        Jump jump;
        jsbytecode *pc;
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
        ValueRemat lvr, rvr;
        Assembler::Condition cond;
        JSC::MacroAssembler::RegisterID tempReg;
    };
    
    struct TraceGenInfo {
        bool initialized;
        Label stubEntry;
        DataLabelPtr addrLabel;
        jsbytecode *jumpTarget;
        Jump traceHint;
        MaybeJump slowTraceHint;

        TraceGenInfo() : initialized(false) {}
    };

    /* InlineFrameAssembler wants to see this. */
  public:
    struct CallGenInfo {
        CallGenInfo(jsbytecode *pc) : pc(pc) {}

        /*
         * These members map to members in CallICInfo. See that structure for
         * more comments.
         */
        jsbytecode   *pc;
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
    };

  private:
#endif

    /*
     * Writes of call return addresses which needs to be delayed until the final
     * absolute address of the join point is known.
     */
    struct CallPatchInfo {
        CallPatchInfo() : hasFastNcode(false), hasSlowNcode(false) {}
        Label joinPoint;
        DataLabelPtr fastNcodePatch;
        DataLabelPtr slowNcodePatch;
        bool hasFastNcode;
        bool hasSlowNcode;
    };

    struct BaseICInfo {
        BaseICInfo(JSOp op) : op(op)
        { }
        Label fastPathStart;
        Label fastPathRejoin;
        Label slowPathStart;
        Call slowPathCall;
        DataLabelPtr paramAddr;
        JSOp op;

        void copyTo(ic::BaseIC &to, JSC::LinkBuffer &full, JSC::LinkBuffer &stub) {
            to.fastPathStart = full.locationOf(fastPathStart);
            to.fastPathRejoin = full.locationOf(fastPathRejoin);
            to.slowPathStart = stub.locationOf(slowPathStart);
            to.slowPathCall = stub.locationOf(slowPathCall);
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
          : BaseICInfo(op), kind(kind), usePropCache(usePropCache)
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
            JS_ASSERT(kind == ic::PICInfo::NAME || kind == ic::PICInfo::XNAME);
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
            JS_ASSERT(kind == ic::PICInfo::NAME || kind == ic::PICInfo::XNAME);
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
        jsbytecode *pc;
        uint32 id;
        bool call;
        bool ool;

        InternalCallSite(uint32 returnOffset, jsbytecode *pc, uint32 id,
                         bool call, bool ool)
          : returnOffset(returnOffset), pc(pc), id(id), call(call), ool(ool)
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

    StackFrame *fp;
    JSScript *script;
    JSObject *scopeChain;
    JSObject *globalObj;
    JSFunction *fun;
    bool isConstructing;
    analyze::Script *analysis;
    Label *jumpMap;
    bool *savedTraps;
    jsbytecode *PC;
    Assembler masm;
    FrameState frame;
    js::Vector<BranchPatch, 64, CompilerAllocPolicy> branchPatches;
#if defined JS_MONOIC
    js::Vector<GetGlobalNameICInfo, 16, CompilerAllocPolicy> getGlobalNames;
    js::Vector<SetGlobalNameICInfo, 16, CompilerAllocPolicy> setGlobalNames;
    js::Vector<CallGenInfo, 64, CompilerAllocPolicy> callICs;
    js::Vector<EqualityGenInfo, 64, CompilerAllocPolicy> equalityICs;
    js::Vector<TraceGenInfo, 64, CompilerAllocPolicy> traceICs;
#endif
#if defined JS_POLYIC
    js::Vector<PICGenInfo, 16, CompilerAllocPolicy> pics;
    js::Vector<GetElementICInfo, 16, CompilerAllocPolicy> getElemICs;
    js::Vector<SetElementICInfo, 16, CompilerAllocPolicy> setElemICs;
#endif
    js::Vector<CallPatchInfo, 64, CompilerAllocPolicy> callPatches;
    js::Vector<InternalCallSite, 64, CompilerAllocPolicy> callSites;
    js::Vector<DoublePatch, 16, CompilerAllocPolicy> doubleList;
    js::Vector<JumpTable, 16> jumpTables;
    js::Vector<uint32, 16> jumpTableOffsets;
    StubCompiler stubcc;
    Label invokeLabel;
    Label arityLabel;
    bool debugMode_;
    bool addTraceHints;
    bool oomInVector;       // True if we have OOM'd appending to a vector. 
    enum { NoApplyTricks, LazyArgsObj } applyTricks;

    Compiler *thisFromCtor() { return this; }

    friend class CompilerAllocPolicy;
  public:
    // Special atom index used to indicate that the atom is 'length'. This
    // follows interpreter usage in JSOP_LENGTH.
    enum { LengthAtomIndex = uint32(-2) };

    Compiler(JSContext *cx, StackFrame *fp);
    ~Compiler();

    CompileStatus compile();

    jsbytecode *getPC() { return PC; }
    Label getLabel() { return masm.label(); }
    bool knownJump(jsbytecode *pc);
    Label labelOf(jsbytecode *target);
    void *findCallSite(const CallSite &callSite);
    void addCallSite(const InternalCallSite &callSite);
    void addReturnSite(Label joinPoint, uint32 id);
    bool loadOldTraps(const Vector<CallSite> &site);

    bool debugMode() { return debugMode_; }

  private:
    CompileStatus performCompilation(JITScript **jitp);
    CompileStatus generatePrologue();
    CompileStatus generateMethod();
    CompileStatus generateEpilogue();
    CompileStatus finishThisUp(JITScript **jitp);

    /* Non-emitting helpers. */
    uint32 fullAtomIndex(jsbytecode *pc);
    bool jumpInScript(Jump j, jsbytecode *pc);
    bool compareTwoValues(JSContext *cx, JSOp op, const Value &lhs, const Value &rhs);
    bool canUseApplyTricks();

    /* Emitting helpers. */
    void restoreFrameRegs(Assembler &masm);
    bool emitStubCmpOp(BoolStub stub, jsbytecode *target, JSOp fused);
    bool iter(uintN flags);
    void iterNext();
    bool iterMore();
    void iterEnd();
    MaybeJump loadDouble(FrameEntry *fe, FPRegisterID fpReg);
#ifdef JS_POLYIC
    void passICAddress(BaseICInfo *ic);
#endif
#ifdef JS_MONOIC
    void passMICAddress(GlobalNameICInfo &mic);
#endif
    bool constructThis();

    /* Opcode handlers. */
    bool jumpAndTrace(Jump j, jsbytecode *target, Jump *slow = NULL);
    void jsop_bindname(JSAtom *atom, bool usePropCache);
    void jsop_setglobal(uint32 index);
    void jsop_getglobal(uint32 index);
    void jsop_getprop_slow(JSAtom *atom, bool usePropCache = true);
    void jsop_getarg(uint32 slot);
    void jsop_setarg(uint32 slot, bool popped);
    void jsop_this();
    void emitReturn(FrameEntry *fe);
    void emitFinalReturn(Assembler &masm);
    void loadReturnValue(Assembler *masm, FrameEntry *fe);
    void emitReturnValue(Assembler *masm, FrameEntry *fe);
    void dispatchCall(VoidPtrStubUInt32 stub, uint32 argc);
    void interruptCheckHelper();
    void emitUncachedCall(uint32 argc, bool callingNew);
    void checkCallApplySpeculation(uint32 callImmArgc, uint32 speculatedArgc,
                                   FrameEntry *origCallee, FrameEntry *origThis,
                                   MaybeRegisterID origCalleeType, RegisterID origCalleeData,
                                   MaybeRegisterID origThisType, RegisterID origThisData,
                                   Jump *uncachedCallSlowRejoin, CallPatchInfo *uncachedCallPatch);
    void inlineCallHelper(uint32 argc, bool callingNew);
    void fixPrimitiveReturn(Assembler *masm, FrameEntry *fe);
    void jsop_gnameinc(JSOp op, VoidStubAtom stub, uint32 index);
    bool jsop_nameinc(JSOp op, VoidStubAtom stub, uint32 index);
    bool jsop_propinc(JSOp op, VoidStubAtom stub, uint32 index);
    void jsop_eleminc(JSOp op, VoidStub);
    void jsop_getgname(uint32 index);
    void jsop_getgname_slow(uint32 index);
    void jsop_callgname_epilogue();
    void jsop_setgname(JSAtom *atom, bool usePropertyCache);
    void jsop_setgname_slow(JSAtom *atom, bool usePropertyCache);
    void jsop_bindgname();
    void jsop_setelem_slow();
    void jsop_getelem_slow();
    void jsop_callelem_slow();
    void jsop_unbrand();
    bool jsop_getprop(JSAtom *atom, bool typeCheck = true, bool usePropCache = true);
    bool jsop_length();
    bool jsop_setprop(JSAtom *atom, bool usePropCache = true);
    void jsop_setprop_slow(JSAtom *atom, bool usePropCache = true);
    bool jsop_callprop_slow(JSAtom *atom);
    bool jsop_callprop(JSAtom *atom);
    bool jsop_callprop_obj(JSAtom *atom);
    bool jsop_callprop_str(JSAtom *atom);
    bool jsop_callprop_generic(JSAtom *atom);
    bool jsop_instanceof();
    void jsop_name(JSAtom *atom);
    bool jsop_xname(JSAtom *atom);
    void enterBlock(JSObject *obj);
    void leaveBlock();
    void emitEval(uint32 argc);
    void jsop_arguments();
    bool jsop_tableswitch(jsbytecode *pc);
    void jsop_forprop(JSAtom *atom);
    void jsop_forname(JSAtom *atom);
    void jsop_forgname(JSAtom *atom);

    /* Fast arithmetic. */
    void jsop_binary(JSOp op, VoidStub stub);
    void jsop_binary_full(FrameEntry *lhs, FrameEntry *rhs, JSOp op, VoidStub stub);
    void jsop_binary_full_simple(FrameEntry *fe, JSOp op, VoidStub stub);
    void jsop_binary_double(FrameEntry *lhs, FrameEntry *rhs, JSOp op, VoidStub stub);
    void slowLoadConstantDouble(Assembler &masm, FrameEntry *fe,
                                FPRegisterID fpreg);
    void maybeJumpIfNotInt32(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                             MaybeRegisterID &mreg);
    void maybeJumpIfNotDouble(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                              MaybeRegisterID &mreg);
    bool jsop_relational(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    bool jsop_relational_self(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    bool jsop_relational_full(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    bool jsop_relational_double(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);

    void emitLeftDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                            MaybeJump &lhsNotDouble, MaybeJump &rhsNotNumber,
                            MaybeJump &lhsUnknownDone);
    void emitRightDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                             MaybeJump &rhsNotNumber2);
    bool tryBinaryConstantFold(JSContext *cx, FrameState &frame, JSOp op,
                               FrameEntry *lhs, FrameEntry *rhs);

    /* Fast opcodes. */
    void jsop_bitop(JSOp op);
    void jsop_rsh();
    RegisterID rightRegForShift(FrameEntry *rhs);
    void jsop_rsh_int_int(FrameEntry *lhs, FrameEntry *rhs);
    void jsop_rsh_const_int(FrameEntry *lhs, FrameEntry *rhs);
    void jsop_rsh_int_const(FrameEntry *lhs, FrameEntry *rhs);
    void jsop_rsh_int_unknown(FrameEntry *lhs, FrameEntry *rhs);
    void jsop_rsh_const_const(FrameEntry *lhs, FrameEntry *rhs);
    void jsop_rsh_const_unknown(FrameEntry *lhs, FrameEntry *rhs);
    void jsop_rsh_unknown_const(FrameEntry *lhs, FrameEntry *rhs);
    void jsop_rsh_unknown_any(FrameEntry *lhs, FrameEntry *rhs);
    void jsop_mod();
    void jsop_neg();
    void jsop_bitnot();
    void jsop_not();
    void jsop_typeof();
    bool booleanJumpScript(JSOp op, jsbytecode *target);
    bool jsop_ifneq(JSOp op, jsbytecode *target);
    bool jsop_andor(JSOp op, jsbytecode *target);
    void jsop_arginc(JSOp op, uint32 slot, bool popped);
    void jsop_localinc(JSOp op, uint32 slot, bool popped);
    void jsop_newinit();
    void jsop_initmethod();
    void jsop_initprop();
    void jsop_initelem();
    bool jsop_setelem(bool popGuaranteed);
    bool jsop_getelem(bool isCall);
    bool isCacheableBaseAndIndex(FrameEntry *obj, FrameEntry *id);
    void jsop_stricteq(JSOp op);
    bool jsop_equality(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    bool jsop_equality_int_string(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    void jsop_pos();

   
    void prepareStubCall(Uses uses);
    Call emitStubCall(void *ptr);
};

// Given a stub call, emits the call into the inline assembly path. If
// debug mode is on, adds the appropriate instrumentation for recompilation.
#define INLINE_STUBCALL(stub)                                               \
    do {                                                                    \
        Call cl = emitStubCall(JS_FUNC_TO_DATA_PTR(void *, (stub)));        \
        if (debugMode()) {                                                  \
            InternalCallSite site(masm.callReturnOffset(cl), PC, __LINE__,  \
                                  true, false);                             \
            addCallSite(site);                                              \
        }                                                                   \
    } while (0)                                                             \

// Given a stub call, emits the call into the out-of-line assembly path. If
// debug mode is on, adds the appropriate instrumentation for recompilation.
// Unlike the INLINE_STUBCALL variant, this returns the Call offset.
#define OOL_STUBCALL(stub)                                                      \
    stubcc.emitStubCall(JS_FUNC_TO_DATA_PTR(void *, (stub)), __LINE__)          \

// Same as OOL_STUBCALL, but specifies a slot depth.
#define OOL_STUBCALL_LOCAL_SLOTS(stub, slots)                                   \
    stubcc.emitStubCall(JS_FUNC_TO_DATA_PTR(void *, (stub)), (slots), __LINE__) \

} /* namespace js */
} /* namespace mjit */

#endif

