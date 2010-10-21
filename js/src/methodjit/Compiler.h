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
    struct BranchPatch {
        BranchPatch(const Jump &j, jsbytecode *pc)
          : jump(j), pc(pc)
        { }

        Jump jump;
        jsbytecode *pc;
    };

#if defined JS_MONOIC
    struct MICGenInfo {
        MICGenInfo(ic::MICInfo::Kind kind) : kind(kind)
        { }
        Label entry;
        Label stubEntry;
        DataLabel32 shape;
        DataLabelPtr addrLabel;
#if defined JS_PUNBOX64
        uint32 patchValueOffset;
#endif
        Label load;
        Call call;
        ic::MICInfo::Kind kind;
        jsbytecode *jumpTarget;
        Jump traceHint;
        MaybeJump slowTraceHint;
        union {
            struct {
                bool typeConst;
                bool dataConst;
            } name;
            struct {
                uint32 pcOffs;
            } tracer;
        } u;
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
    };

    struct PICGenInfo : public BaseICInfo {
        PICGenInfo(ic::PICInfo::Kind kind, JSOp op, bool usePropCache)
          : BaseICInfo(op), kind(kind), usePropCache(usePropCache)
        { }
        ic::PICInfo::Kind kind;
        Label typeCheck;
        RegisterID shapeReg;
        RegisterID objReg;
        RegisterID idReg;
        RegisterID typeReg;
        bool usePropCache;
        Label shapeGuard;
        jsbytecode *pc;
        JSAtom *atom;
        bool hasTypeCheck;
        ValueRemat vr;
# if defined JS_CPU_X64
        ic::PICLabels labels;
# endif

        void copySimpleMembersTo(ic::PICInfo &ic) const {
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
        }

    };

    struct Defs {
        Defs(uint32 ndefs)
          : ndefs(ndefs)
        { }
        uint32 ndefs;
    };

    struct InternalCallSite {
        bool stub;
        Label location;
        jsbytecode *pc;
        uint32 id;
    };

    struct DoublePatch {
        double d;
        DataLabelPtr label;
        bool ool;
    };

    JSStackFrame *fp;
    JSScript *script;
    JSObject *scopeChain;
    JSObject *globalObj;
    JSFunction *fun;
    bool isConstructing;
    analyze::Script *analysis;
    Label *jumpMap;
    jsbytecode *PC;
    Assembler masm;
    FrameState frame;
    js::Vector<BranchPatch, 64, CompilerAllocPolicy> branchPatches;
#if defined JS_MONOIC
    js::Vector<MICGenInfo, 64, CompilerAllocPolicy> mics;
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
    StubCompiler stubcc;
    Label invokeLabel;
    Label arityLabel;
    bool debugMode;
    bool addTraceHints;
    bool oomInVector;       // True if we have OOM'd appending to a vector. 

    Compiler *thisFromCtor() { return this; }

    friend class CompilerAllocPolicy;
  public:
    // Special atom index used to indicate that the atom is 'length'. This
    // follows interpreter usage in JSOP_LENGTH.
    enum { LengthAtomIndex = uint32(-2) };

    Compiler(JSContext *cx, JSStackFrame *fp);
    ~Compiler();

    CompileStatus compile();

    jsbytecode *getPC() { return PC; }
    Label getLabel() { return masm.label(); }
    bool knownJump(jsbytecode *pc);
    Label labelOf(jsbytecode *target);
    void *findCallSite(const CallSite &callSite);

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
    void addCallSite(uint32 id, bool stub);

    /* Emitting helpers. */
    void restoreFrameRegs(Assembler &masm);
    bool emitStubCmpOp(BoolStub stub, jsbytecode *target, JSOp fused);
    void iter(uintN flags);
    void iterNext();
    bool iterMore();
    void iterEnd();
    MaybeJump loadDouble(FrameEntry *fe, FPRegisterID fpReg);
#ifdef JS_POLYIC
    void passICAddress(BaseICInfo *ic);
#endif
#ifdef JS_MONOIC
    void passMICAddress(MICGenInfo &mic);
#endif
    bool constructThis();

    /* Opcode handlers. */
    bool jumpAndTrace(Jump j, jsbytecode *target, Jump *slow = NULL);
    void jsop_bindname(uint32 index, bool usePropCache);
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
    void checkCallApplySpeculation(uint32 argc, FrameEntry *origCallee, FrameEntry *origThis,
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
    void jsop_setgname(uint32 index);
    void jsop_setgname_slow(uint32 index);
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
    void jsop_globalinc(JSOp op, uint32 index);
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
    bool jsop_setelem();
    bool jsop_getelem(bool isCall);
    bool isCacheableBaseAndIndex(FrameEntry *obj, FrameEntry *id);
    void jsop_stricteq(JSOp op);
    bool jsop_equality(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    bool jsop_equality_int_string(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused);
    void jsop_pos();

#define STUB_CALL_TYPE(type)                                            \
    Call stubCall(type stub) {                                          \
        return stubCall(JS_FUNC_TO_DATA_PTR(void *, stub));             \
    }

    STUB_CALL_TYPE(JSObjStub);
    STUB_CALL_TYPE(VoidStubUInt32);
    STUB_CALL_TYPE(VoidStub);
    STUB_CALL_TYPE(VoidPtrStubUInt32);
    STUB_CALL_TYPE(VoidPtrStub);
    STUB_CALL_TYPE(BoolStub);
    STUB_CALL_TYPE(JSObjStubUInt32);
    STUB_CALL_TYPE(JSObjStubFun);
    STUB_CALL_TYPE(JSObjStubJSObj);
    STUB_CALL_TYPE(VoidStubAtom);
    STUB_CALL_TYPE(JSStrStub);
    STUB_CALL_TYPE(JSStrStubUInt32);
    STUB_CALL_TYPE(VoidStubJSObj);
    STUB_CALL_TYPE(VoidPtrStubPC);
    STUB_CALL_TYPE(VoidVpStub);
    STUB_CALL_TYPE(VoidStubPC);
    STUB_CALL_TYPE(BoolStubUInt32);
    STUB_CALL_TYPE(VoidStubFun);

#undef STUB_CALL_TYPE
    void prepareStubCall(Uses uses);
    Call stubCall(void *ptr);
};

} /* namespace js */
} /* namespace mjit */

#endif

