/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SpiderMonkey bytecode analysis
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* Definitions for javascript analysis. */

#ifndef jsanalyze_h___
#define jsanalyze_h___

#include "jsarena.h"
#include "jscompartment.h"
#include "jscntxt.h"
#include "jsinfer.h"
#include "jsscript.h"

struct JSScript;

/* Forward declaration of downstream register allocations computed for join points. */
namespace js { namespace mjit { struct RegisterAllocation; } }

namespace js {
namespace analyze {

/*
 * There are three analyses we can perform on a JSScript, outlined below.
 * The results of all three are stored in ScriptAnalysis, but the analyses
 * themselves can be performed separately. Along with type inference results,
 * per-script analysis results are tied to the per-compartment analysis pool
 * and are freed on every garbage collection.
 *
 * - Basic bytecode analysis. For each bytecode, determine the stack depth at
 * that point and control flow information needed for compilation. Also does
 * a defined-variables analysis to look for local variables which have uses
 * before definitions.
 *
 * - Lifetime analysis. Makes a backwards pass over the script to approximate
 * the regions where each variable is live, avoiding a full fixpointing
 * live-variables pass. This is based on the algorithm described in:
 *
 *     "Quality and Speed in Linear-scan Register Allocation"
 *     Traub et. al.
 *     PLDI, 1998
 *
 * - SSA analysis of the script's variables and stack values. For each stack
 * value popped and non-escaping local variable or argument read, determines
 * which push(es) or write(s) produced that value.
 *
 * Intermediate type inference results are additionally stored here. The above
 * analyses are independent from type inference.
 */

class SSAValue;
struct SSAUseChain;
struct LoopAnalysis;
struct SlotValue;

/* Information about a bytecode instruction. */
class Bytecode
{
    friend class ScriptAnalysis;

  public:
    Bytecode() { PodZero(this); }

    /* --------- Bytecode analysis --------- */

    /* Whether there are any incoming jumps to this instruction. */
    bool jumpTarget : 1;

    /* There is a backwards jump to this instruction. */
    bool loopHead : 1;

    /* Whether there is fallthrough to this instruction from a non-branching instruction. */
    bool fallthrough : 1;

    /* Whether this instruction is the fall through point of a conditional jump. */
    bool jumpFallthrough : 1;

    /* Whether this instruction can be branched to from a switch statement. Implies jumpTarget. */
    bool switchTarget : 1;

    /*
     * Whether this instruction must always execute, unless the script throws
     * an exception which it does not later catch.
     */
    bool unconditional : 1;

    /* Whether this instruction has been analyzed to get its output defines and stack. */
    bool analyzed : 1;

    /* Whether this is a catch/finally entry point. */
    bool exceptionEntry : 1;

    /* Whether this is in a try block. */
    bool inTryBlock : 1;

    /* Method JIT safe point. */
    bool safePoint : 1;

    /*
     * Side effects of this bytecode were not determined by type inference.
     * Either a property set with unknown lvalue, or call with unknown callee.
     */
    bool monitoredTypes : 1;

    /* Call whose result should be monitored. */
    bool monitoredTypesReturn : 1;

    /* Stack depth before this opcode. */
    uint32 stackDepth;

  private:
    /*
     * The set of locals defined at this point. This does not include locals which
     * were unconditionally defined at an earlier point in the script.
     */
    uint32 defineCount;
    uint32 *defineArray;

    union {
        /* If this is a JOF_TYPESET opcode, index into the observed types for the op. */
        types::TypeSet *observedTypes;

        /* If this is a loop head (TRACE or NOTRACE), information about the loop. */
        LoopAnalysis *loop;
    };

    /* --------- Lifetime analysis --------- */

    /* Any allocation computed downstream for this bytecode. */
    mjit::RegisterAllocation *allocation;

    /* --------- SSA analysis --------- */

    /* Generated location of each value popped by this bytecode. */
    SSAValue *poppedValues;

    /* Points where values pushed or written by this bytecode are popped. */
    SSAUseChain **pushedUses;

    union {
        /*
         * If this is a join point (implies jumpTarget), any slots at this
         * point which can have a different values than at the immediate
         * predecessor in the bytecode. Array is terminated by an entry with
         * a zero slot.
         */
        SlotValue *newValues;

        /*
         * Vector used during SSA analysis to store values in need of merging
         * at this point. If this has incoming forward jumps and we have not
         * yet reached this point, stores values for entries on the stack and
         * for variables which have changed since the branch. If this is a loop
         * head and we haven't reached the back edge yet, stores loop phi nodes
         * for variables and entries live at the head of the loop.
         */
        Vector<SlotValue> *pendingValues;
    };

    /* --------- Type inference --------- */

    /* Types for all values pushed by this bytecode. */
    types::TypeSet *pushedTypes;

    /* Any type barriers in place at this bytecode. */
    types::TypeBarrier *typeBarriers;

    /* --------- Helpers --------- */

    bool mergeDefines(JSContext *cx, ScriptAnalysis *script, bool initial,
                      uint32 newDepth, uint32 *newArray, uint32 newCount);

    /* Whether a local variable is in the define set at this bytecode. */
    bool isDefined(uint32 slot)
    {
        JS_ASSERT(analyzed);
        for (unsigned i = 0; i < defineCount; i++) {
            if (defineArray[i] == slot)
                return true;
        }
        return false;
    }
};

static inline unsigned
GetBytecodeLength(jsbytecode *pc)
{
    JSOp op = (JSOp)*pc;
    JS_ASSERT(op < JSOP_LIMIT);
    JS_ASSERT(op != JSOP_TRAP);

    if (js_CodeSpec[op].length != -1)
        return js_CodeSpec[op].length;
    return js_GetVariableBytecodeLength(pc);
}

static inline unsigned
GetDefCount(JSScript *script, unsigned offset)
{
    JS_ASSERT(offset < script->length);
    jsbytecode *pc = script->code + offset;
    JS_ASSERT(JSOp(*pc) != JSOP_TRAP);

    if (js_CodeSpec[*pc].ndefs == -1)
        return js_GetEnterBlockStackDefs(NULL, script, pc);

    /*
     * Add an extra pushed value for OR/AND opcodes, so that they are included
     * in the pushed array of stack values for type inference.
     */
    switch (JSOp(*pc)) {
      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        return 1;
      case JSOP_FILTER:
        return 2;
      case JSOP_PICK:
        /*
         * Pick pops and pushes how deep it looks in the stack + 1
         * items. i.e. if the stack were |a b[2] c[1] d[0]|, pick 2
         * would pop b, c, and d to rearrange the stack to |a c[0]
         * d[1] b[2]|.
         */
        return (pc[1] + 1);
      default:
        return js_CodeSpec[*pc].ndefs;
    }
}

static inline unsigned
GetUseCount(JSScript *script, unsigned offset)
{
    JS_ASSERT(offset < script->length);
    jsbytecode *pc = script->code + offset;
    JS_ASSERT(JSOp(*pc) != JSOP_TRAP);

    if (JSOp(*pc) == JSOP_PICK)
        return (pc[1] + 1);
    if (js_CodeSpec[*pc].nuses == -1)
        return js_GetVariableStackUses(JSOp(*pc), pc);
    return js_CodeSpec[*pc].nuses;
}

/*
 * For opcodes which assign to a local variable or argument, track an extra def
 * during SSA analysis for the value's use chain and assigned type.
 */
static inline bool
ExtendedDef(jsbytecode *pc)
{
    JS_ASSERT(JSOp(*pc) != JSOP_TRAP);

    switch ((JSOp)*pc) {
      case JSOP_SETARG:
      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
      case JSOP_SETLOCAL:
      case JSOP_SETLOCALPOP:
      case JSOP_DEFLOCALFUN:
      case JSOP_DEFLOCALFUN_FC:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC:
        return true;
      default:
        return false;
    }
}

/*
 * For opcodes which access local variables or arguments, we track an extra
 * use during SSA analysis for the value of the variable before/after the op.
 */
static inline bool
ExtendedUse(jsbytecode *pc)
{
    if (ExtendedDef(pc))
        return true;
    switch ((JSOp)*pc) {
      case JSOP_GETARG:
      case JSOP_CALLARG:
      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL:
        return true;
      default:
        return false;
    }
}

static inline ptrdiff_t
GetJumpOffset(jsbytecode *pc, jsbytecode *pc2)
{
    JS_ASSERT(JSOp(*pc) != JSOP_TRAP);

    uint32 type = JOF_OPTYPE(*pc);
    if (JOF_TYPE_IS_EXTENDED_JUMP(type))
        return GET_JUMPX_OFFSET(pc2);
    return GET_JUMP_OFFSET(pc2);
}

static inline JSOp
ReverseCompareOp(JSOp op)
{
    switch (op) {
      case JSOP_GT:
        return JSOP_LT;
      case JSOP_GE:
        return JSOP_LE;
      case JSOP_LT:
        return JSOP_GT;
      case JSOP_LE:
        return JSOP_GE;
      default:
        JS_NOT_REACHED("unrecognized op");
        return op;
    }
}

/* Untrap a single PC, and retrap it at scope exit. */
struct UntrapOpcode
{
    jsbytecode *pc;
    bool trap;

    UntrapOpcode(JSContext *cx, JSScript *script, jsbytecode *pc)
        : pc(pc), trap(JSOp(*pc) == JSOP_TRAP)
    {
        if (trap)
            *pc = JS_GetTrapOpcode(cx, script, pc);
    }

    void retrap()
    {
        if (trap) {
            *pc = JSOP_TRAP;
            trap = false;
        }
    }

    ~UntrapOpcode()
    {
        retrap();
    }
};

static inline unsigned
FollowBranch(JSContext *cx, JSScript *script, unsigned offset)
{
    /*
     * Get the target offset of a branch. For GOTO opcodes implementing
     * 'continue' statements, short circuit any artificial backwards jump
     * inserted by the emitter.
     */
    jsbytecode *pc = script->code + offset;
    unsigned targetOffset = offset + GetJumpOffset(pc, pc);
    if (targetOffset < offset) {
        jsbytecode *target = script->code + targetOffset;
        UntrapOpcode untrap(cx, script, target);
        JSOp nop = JSOp(*target);
        if (nop == JSOP_GOTO || nop == JSOP_GOTOX)
            return targetOffset + GetJumpOffset(target, target);
    }
    return targetOffset;
}

/* Common representation of slots throughout analyses and the compiler. */
static inline uint32 CalleeSlot() {
    return 0;
}
static inline uint32 ThisSlot() {
    return 1;
}
static inline uint32 ArgSlot(uint32 arg) {
    return 2 + arg;
}
static inline uint32 LocalSlot(JSScript *script, uint32 local) {
    return 2 + (script->hasFunction ? script->function()->nargs : 0) + local;
}
static inline uint32 TotalSlots(JSScript *script) {
    return LocalSlot(script, 0) + script->nfixed;
}

static inline uint32 StackSlot(JSScript *script, uint32 index) {
    return TotalSlots(script) + index;
}

static inline uint32 GetBytecodeSlot(JSScript *script, jsbytecode *pc)
{
    switch (JSOp(*pc)) {

      case JSOP_GETARG:
      case JSOP_CALLARG:
      case JSOP_SETARG:
      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
        return ArgSlot(GET_SLOTNO(pc));

      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL:
      case JSOP_SETLOCAL:
      case JSOP_SETLOCALPOP:
      case JSOP_DEFLOCALFUN:
      case JSOP_DEFLOCALFUN_FC:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC:
        return LocalSlot(script, GET_SLOTNO(pc));

      case JSOP_THIS:
        return ThisSlot();

      default:
        JS_NOT_REACHED("Bad slot opcode");
        return 0;
    }
}

static inline int32
GetBytecodeInteger(jsbytecode *pc)
{
    switch (JSOp(*pc)) {
      case JSOP_ZERO:   return 0;
      case JSOP_ONE:    return 1;
      case JSOP_UINT16: return GET_UINT16(pc);
      case JSOP_UINT24: return GET_UINT24(pc);
      case JSOP_INT8:   return GET_INT8(pc);
      case JSOP_INT32:  return GET_INT32(pc);
      default:
        JS_NOT_REACHED("Bad op");
        return 0;
    }
}

/*
 * Information about the lifetime of a local or argument. These form a linked
 * list describing successive intervals in the program where the variable's
 * value may be live. At points in the script not in one of these segments
 * (points in a 'lifetime hole'), the variable is dead and registers containing
 * its type/payload can be discarded without needing to be synced.
 */
struct Lifetime
{
    /*
     * Start and end offsets of this lifetime. The variable is live at the
     * beginning of every bytecode in this (inclusive) range.
     */
    uint32 start;
    uint32 end;

    /*
     * In a loop body, endpoint to extend this lifetime with if the variable is
     * live in the next iteration.
     */
    uint32 savedEnd;

    /*
     * This is an artificial segment extending the lifetime of this variable
     * when it is live at the head of the loop. It will not be used until the
     * next iteration.
     */
    bool loopTail;

    /*
     * The start of this lifetime is a bytecode writing the variable. Each
     * write to a variable is associated with a lifetime.
     */
    bool write;

    /* Next lifetime. The variable is dead from this->end to next->start. */
    Lifetime *next;

    Lifetime(uint32 offset, uint32 savedEnd, Lifetime *next)
        : start(offset), end(offset), savedEnd(savedEnd),
          loopTail(false), write(false), next(next)
    {}
};

/* Basic information for a loop. */
struct LoopAnalysis
{
    /* Any loop this one is nested in. */
    LoopAnalysis *parent;

    /* Offset of the head of the loop. */
    uint32 head;

    /*
     * Offset of the unique jump going to the head of the loop. The code
     * between the head and the backedge forms the loop body.
     */
    uint32 backedge;

    /* Target offset of the initial jump or fallthrough into the loop. */
    uint32 entry;

    /*
     * Start of the last basic block in the loop, excluding the initial jump to
     * entry. All code between lastBlock and the backedge runs in every
     * iteration, and if entry >= lastBlock all code between entry and the
     * backedge runs when the loop is initially entered.
     */
    uint32 lastBlock;

    /*
     * This loop contains safe points in its body which the interpreter might
     * join at directly.
     */
    bool hasSafePoints;

    /* This loop has calls or inner loops. */
    bool hasCallsLoops;
};

/* Current lifetime information for a variable. */
struct LifetimeVariable
{
    /* If the variable is currently live, the lifetime segment. */
    Lifetime *lifetime;

    /* If the variable is currently dead, the next live segment. */
    Lifetime *saved;

    /* Jump preceding the basic block which killed this variable. */
    uint32 savedEnd : 31;

    /* If the variable needs to be kept alive until lifetime->start. */
    bool ensured : 1;

    /* Whether this variable is live at offset. */
    Lifetime * live(uint32 offset) const {
        if (lifetime && lifetime->end >= offset)
            return lifetime;
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= offset) {
            if (segment->end >= offset)
                return segment;
            segment = segment->next;
        }
        return NULL;
    }

    /*
     * Get the offset of the first write to the variable in an inclusive range,
     * -1 if the variable is not written in the range.
     */
    uint32 firstWrite(uint32 start, uint32 end) const {
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= end) {
            if (segment->start >= start && segment->write)
                return segment->start;
            segment = segment->next;
        }
        return uint32(-1);
    }
    uint32 firstWrite(LoopAnalysis *loop) const {
        return firstWrite(loop->head, loop->backedge);
    }

    /* Return true if the variable cannot decrease during the body of a loop. */
    bool nonDecreasing(JSScript *script, LoopAnalysis *loop) const {
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= loop->backedge) {
            if (segment->start >= loop->head && segment->write) {
                switch (JSOp(script->code[segment->start])) {
                  case JSOP_INCLOCAL:
                  case JSOP_LOCALINC:
                  case JSOP_INCARG:
                  case JSOP_ARGINC:
                    break;
                  default:
                    return false;
                }
            }
            segment = segment->next;
        }
        return true;
    }

    /*
     * If the variable is only written once in the body of a loop, offset of
     * that write. -1 otherwise.
     */
    uint32 onlyWrite(LoopAnalysis *loop) const {
        uint32 offset = uint32(-1);
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= loop->backedge) {
            if (segment->start >= loop->head && segment->write) {
                if (offset != uint32(-1))
                    return uint32(-1);
                offset = segment->start;
            }
            segment = segment->next;
        }
        return offset;
    }

#ifdef DEBUG
    void print() const;
#endif
};

struct SSAPhiNode;

/*
 * Representation of values on stack or in slots at each point in the script.
 * Values are independent from the bytecode position, and mean the same thing
 * everywhere in the script. SSA values are immutable, except for contents of
 * the values and types in an SSAPhiNode.
 */
class SSAValue
{
    friend class ScriptAnalysis;

  public:
    enum Kind {
        EMPTY  = 0, /* Invalid entry. */
        PUSHED = 1, /* Value pushed by some bytecode. */
        VAR    = 2, /* Initial or written value to some argument or local. */
        PHI    = 3  /* Selector for one of several values. */
    };

    Kind kind() const {
        JS_ASSERT(u.pushed.kind == u.var.kind && u.pushed.kind == u.phi.kind);

        /* Use a bitmask because MSVC wants to use -1 for PHI nodes. */
        return (Kind) (u.pushed.kind & 0x3);
    }

    bool equals(const SSAValue &o) const {
        return !memcmp(this, &o, sizeof(SSAValue));
    }

    /* Accessors for values pushed by a bytecode within this script. */

    uint32 pushedOffset() const {
        JS_ASSERT(kind() == PUSHED);
        return u.pushed.offset;
    }

    uint32 pushedIndex() const {
        JS_ASSERT(kind() == PUSHED);
        return u.pushed.index;
    }

    /* Accessors for initial and written values of arguments and (undefined) locals. */

    bool varInitial() const {
        JS_ASSERT(kind() == VAR);
        return u.var.initial;
    }

    uint32 varSlot() const {
        JS_ASSERT(kind() == VAR);
        return u.var.slot;
    }

    uint32 varOffset() const {
        JS_ASSERT(!varInitial());
        return u.var.offset;
    }

    /* Accessors for phi nodes. */

    uint32 phiSlot() const;
    uint32 phiLength() const;
    const SSAValue &phiValue(uint32 i) const;
    types::TypeSet *phiTypes() const;

    /* Offset at which this phi node was created. */
    uint32 phiOffset() const {
        JS_ASSERT(kind() == PHI);
        return u.phi.offset;
    }

    SSAPhiNode *phiNode() const {
        JS_ASSERT(kind() == PHI);
        return u.phi.node;
    }

    /* Other accessors. */

#ifdef DEBUG
    void print() const;
#endif

    void clear() {
        PodZero(this);
        JS_ASSERT(kind() == EMPTY);
    }

    void initPushed(uint32 offset, uint32 index) {
        clear();
        u.pushed.kind = PUSHED;
        u.pushed.offset = offset;
        u.pushed.index = index;
    }

    static SSAValue PushedValue(uint32 offset, uint32 index) {
        SSAValue v;
        v.initPushed(offset, index);
        return v;
    }

    void initInitial(uint32 slot) {
        clear();
        u.var.kind = VAR;
        u.var.initial = true;
        u.var.slot = slot;
    }

    void initWritten(uint32 slot, uint32 offset) {
        clear();
        u.var.kind = VAR;
        u.var.initial = false;
        u.var.slot = slot;
        u.var.offset = offset;
    }

    static SSAValue WrittenVar(uint32 slot, uint32 offset) {
        SSAValue v;
        v.initWritten(slot, offset);
        return v;
    }

    void initPhi(uint32 offset, SSAPhiNode *node) {
        clear();
        u.phi.kind = PHI;
        u.phi.offset = offset;
        u.phi.node = node;
    }

    static SSAValue PhiValue(uint32 offset, SSAPhiNode *node) {
        SSAValue v;
        v.initPhi(offset, node);
        return v;
    }

  private:
    union {
        struct {
            Kind kind : 2;
            uint32 offset : 30;
            uint32 index;
        } pushed;
        struct {
            Kind kind : 2;
            bool initial : 1;
            uint32 slot : 29;
            uint32 offset;
        } var;
        struct {
            Kind kind : 2;
            uint32 offset : 30;
            SSAPhiNode *node;
        } phi;
    } u;
};

/*
 * Mutable component of a phi node, with the possible values of the phi
 * and the possible types of the node as determined by type inference.
 * When phi nodes are copied around, any updates to the original will
 * be seen by all copies made.
 */
struct SSAPhiNode
{
    types::TypeSet types;
    uint32 slot;
    uint32 length;
    SSAValue *options;
    SSAUseChain *uses;
    SSAPhiNode() { PodZero(this); }
};

inline uint32
SSAValue::phiSlot() const
{
    return u.phi.node->slot;
}

inline uint32
SSAValue::phiLength() const
{
    JS_ASSERT(kind() == PHI);
    return u.phi.node->length;
}

inline const SSAValue &
SSAValue::phiValue(uint32 i) const
{
    JS_ASSERT(kind() == PHI && i < phiLength());
    return u.phi.node->options[i];
}

inline types::TypeSet *
SSAValue::phiTypes() const
{
    JS_ASSERT(kind() == PHI);
    return &u.phi.node->types;
}

struct SSAUseChain
{
    bool popped : 1;
    uint32 offset : 31;
    union {
        uint32 which;
        SSAPhiNode *phi;
    } u;
    SSAUseChain *next;

    SSAUseChain() { PodZero(this); }
};

struct SlotValue
{
    uint32 slot;
    SSAValue value;
    SlotValue(uint32 slot, const SSAValue &value) : slot(slot), value(value) {}
};

/* Analysis information about a script. */
class ScriptAnalysis
{
    friend class Bytecode;

    JSScript *script;

    Bytecode **codeArray;

    uint32 numSlots;

    bool outOfMemory;
    bool hadFailure;

    JSPackedBool *escapedSlots;

    /* Which analyses have been performed. */
    bool ranBytecode_;
    bool ranSSA_;
    bool ranLifetimes_;
    bool ranInference_;

    /* --------- Bytecode analysis --------- */

    bool usesRval;
    bool usesScope;
    bool usesThis;
    bool hasCalls;
    bool canTrackVars;
    bool isInlineable;
    uint32 numReturnSites_;
    bool modifiesArguments_;
    bool localsAliasStack_;

    /* Offsets at which each local becomes unconditionally defined, or a value below. */
    uint32 *definedLocals;

    static const uint32 LOCAL_USE_BEFORE_DEF = uint32(-1);
    static const uint32 LOCAL_CONDITIONALLY_DEFINED = uint32(-2);

    /* --------- Lifetime analysis --------- */

    LifetimeVariable *lifetimes;

  public:

    ScriptAnalysis(JSScript *script) { PodZero(this); this->script = script; }

    bool ranBytecode() { return ranBytecode_; }
    bool ranSSA() { return ranSSA_; }
    bool ranLifetimes() { return ranLifetimes_; }
    bool ranInference() { return ranInference_; }

    void analyzeBytecode(JSContext *cx);
    void analyzeSSA(JSContext *cx);
    void analyzeLifetimes(JSContext *cx);
    void analyzeTypes(JSContext *cx);

    /* Analyze the effect of invoking 'new' on script. */
    void analyzeTypesNew(JSContext *cx);

    bool OOM() { return outOfMemory; }
    bool failed() { return hadFailure; }
    bool inlineable(uint32 argc) { return isInlineable && argc == script->function()->nargs; }

    /* Whether there are POPV/SETRVAL bytecodes which can write to the frame's rval. */
    bool usesReturnValue() const { return usesRval; }

    /* Whether there are NAME bytecodes which can access the frame's scope chain. */
    bool usesScopeChain() const { return usesScope; }

    bool usesThisValue() const { return usesThis; }
    bool hasFunctionCalls() const { return hasCalls; }
    uint32 numReturnSites() const { return numReturnSites_; }

    /*
     * True if all named formal arguments are not modified. If the arguments
     * object cannot escape, the arguments are never modified within the script.
     */
    bool modifiesArguments() { return modifiesArguments_; }

    /*
     * True if there are any LOCAL opcodes aliasing values on the stack (above
     * script->nfixed).
     */
    bool localsAliasStack() { return localsAliasStack_; }

    /* Accessors for bytecode information. */

    Bytecode& getCode(uint32 offset) {
        JS_ASSERT(script->compartment->activeAnalysis);
        JS_ASSERT(offset < script->length);
        JS_ASSERT(codeArray[offset]);
        return *codeArray[offset];
    }
    Bytecode& getCode(const jsbytecode *pc) { return getCode(pc - script->code); }

    Bytecode* maybeCode(uint32 offset) {
        JS_ASSERT(script->compartment->activeAnalysis);
        JS_ASSERT(offset < script->length);
        return codeArray[offset];
    }
    Bytecode* maybeCode(const jsbytecode *pc) { return maybeCode(pc - script->code); }

    bool jumpTarget(uint32 offset) {
        JS_ASSERT(offset < script->length);
        return codeArray[offset] && codeArray[offset]->jumpTarget;
    }
    bool jumpTarget(const jsbytecode *pc) { return jumpTarget(pc - script->code); }

    bool popGuaranteed(jsbytecode *pc) {
        jsbytecode *next = pc + GetBytecodeLength(pc);
        return JSOp(*next) == JSOP_POP && !jumpTarget(next);
    }

    bool incrementInitialValueObserved(jsbytecode *pc) {
        const JSCodeSpec *cs = &js_CodeSpec[*pc];
        return (cs->format & JOF_POST) && !popGuaranteed(pc);
    }

    types::TypeSet *bytecodeTypes(const jsbytecode *pc) {
        JS_ASSERT(JSOp(*pc) == JSOP_TRAP || (js_CodeSpec[*pc].format & JOF_TYPESET));
        return getCode(pc).observedTypes;
    }

    const SSAValue &poppedValue(uint32 offset, uint32 which) {
        JS_ASSERT(offset < script->length);
        JS_ASSERT_IF(script->code[offset] != JSOP_TRAP,
                     which < GetUseCount(script, offset) +
                     (ExtendedUse(script->code + offset) ? 1 : 0));
        return getCode(offset).poppedValues[which];
    }
    const SSAValue &poppedValue(const jsbytecode *pc, uint32 which) {
        return poppedValue(pc - script->code, which);
    }

    const SlotValue *newValues(uint32 offset) {
        JS_ASSERT(offset < script->length);
        return getCode(offset).newValues;
    }
    const SlotValue *newValues(const jsbytecode *pc) { return newValues(pc - script->code); }

    types::TypeSet *pushedTypes(uint32 offset, uint32 which = 0) {
        JS_ASSERT(offset < script->length);
        JS_ASSERT_IF(script->code[offset] != JSOP_TRAP,
                     which < GetDefCount(script, offset) +
                     (ExtendedDef(script->code + offset) ? 1 : 0));
        types::TypeSet *array = getCode(offset).pushedTypes;
        JS_ASSERT(array);
        return array + which;
    }
    types::TypeSet *pushedTypes(const jsbytecode *pc, uint32 which) {
        return pushedTypes(pc - script->code, which);
    }

    bool hasPushedTypes(const jsbytecode *pc) { return getCode(pc).pushedTypes != NULL; }

    types::TypeBarrier *typeBarriers(uint32 offset) {
        if (getCode(offset).typeBarriers)
            pruneTypeBarriers(offset);
        return getCode(offset).typeBarriers;
    }
    types::TypeBarrier *typeBarriers(const jsbytecode *pc) {
        return typeBarriers(pc - script->code);
    }
    void addTypeBarrier(JSContext *cx, const jsbytecode *pc,
                        types::TypeSet *target, types::Type type);

    /* Remove obsolete type barriers at the given offset. */
    void pruneTypeBarriers(uint32 offset);

    /*
     * Remove still-active type barriers at the given offset. If 'all' is set,
     * then all barriers are removed, otherwise only those deemed excessive
     * are removed.
     */
    void breakTypeBarriers(JSContext *cx, uint32 offset, bool all);

    /* Break all type barriers used in computing v. */
    void breakTypeBarriersSSA(JSContext *cx, const SSAValue &v);

    inline void addPushedType(JSContext *cx, uint32 offset, uint32 which, types::Type type);

    types::TypeSet *getValueTypes(const SSAValue &v) {
        switch (v.kind()) {
          case SSAValue::PUSHED:
            return pushedTypes(v.pushedOffset(), v.pushedIndex());
          case SSAValue::VAR:
            JS_ASSERT(!slotEscapes(v.varSlot()));
            if (v.varInitial()) {
                return types::TypeScript::SlotTypes(script, v.varSlot());
            } else {
                /*
                 * Results of intermediate assignments have the same type as
                 * the first type pushed by the assignment op. Note that this
                 * may not be the exact same value as was pushed, due to
                 * post-inc/dec ops.
                 */
                return pushedTypes(v.varOffset(), 0);
            }
          case SSAValue::PHI:
            return &v.phiNode()->types;
          default:
            /* Cannot compute types for empty SSA values. */
            JS_NOT_REACHED("Bad SSA value");
            return NULL;
        }
    }

    types::TypeSet *poppedTypes(uint32 offset, uint32 which) {
        return getValueTypes(poppedValue(offset, which));
    }
    types::TypeSet *poppedTypes(const jsbytecode *pc, uint32 which) {
        return getValueTypes(poppedValue(pc, which));
    }

    bool trackUseChain(const SSAValue &v) {
        JS_ASSERT_IF(v.kind() == SSAValue::VAR, trackSlot(v.varSlot()));
        return v.kind() != SSAValue::EMPTY &&
            (v.kind() != SSAValue::VAR || !v.varInitial());
    }

    /*
     * Get the use chain for an SSA value. May be invalid for some opcodes in
     * scripts where localsAliasStack(). You have been warned!
     */
    SSAUseChain *& useChain(const SSAValue &v) {
        JS_ASSERT(trackUseChain(v));
        if (v.kind() == SSAValue::PUSHED)
            return getCode(v.pushedOffset()).pushedUses[v.pushedIndex()];
        if (v.kind() == SSAValue::VAR)
            return getCode(v.varOffset()).pushedUses[GetDefCount(script, v.varOffset())];
        return v.phiNode()->uses;
    }

    mjit::RegisterAllocation *&getAllocation(uint32 offset) {
        JS_ASSERT(offset < script->length);
        return getCode(offset).allocation;
    }
    mjit::RegisterAllocation *&getAllocation(const jsbytecode *pc) {
        return getAllocation(pc - script->code);
    }

    LoopAnalysis *getLoop(uint32 offset) {
        JS_ASSERT(offset < script->length);
        JS_ASSERT(getCode(offset).loop);
        return getCode(offset).loop;
    }
    LoopAnalysis *getLoop(const jsbytecode *pc) { return getLoop(pc - script->code); }

    /* For a JSOP_CALL* op, get the pc of the corresponding JSOP_CALL/NEW/etc. */
    jsbytecode *getCallPC(jsbytecode *pc)
    {
        JS_ASSERT(js_CodeSpec[*pc].format & JOF_CALLOP);
        SSAUseChain *uses = useChain(SSAValue::PushedValue(pc - script->code, 1));
        JS_ASSERT(uses && !uses->next && uses->popped);
        return script->code + uses->offset;
    }

    /* Accessors for local variable information. */

    bool localHasUseBeforeDef(uint32 local) {
        JS_ASSERT(!failed());
        return slotEscapes(LocalSlot(script, local)) ||
            definedLocals[local] == LOCAL_USE_BEFORE_DEF;
    }

    /* These return true for variables that may have a use before def. */
    bool localDefined(uint32 local, uint32 offset) {
        return localHasUseBeforeDef(local) || (definedLocals[local] <= offset) ||
            getCode(offset).isDefined(local);
    }
    bool localDefined(uint32 local, jsbytecode *pc) {
        return localDefined(local, pc - script->code);
    }

    /*
     * Escaping slots include all slots that can be accessed in ways other than
     * through the corresponding LOCAL/ARG opcode. This includes all closed
     * slots in the script, all slots in scripts which use eval or are in debug
     * mode, and slots which are aliased by NAME or similar opcodes in the
     * containing script (which does not imply the variable is closed).
     */
    bool slotEscapes(uint32 slot) {
        JS_ASSERT(script->compartment->activeAnalysis);
        if (slot >= numSlots)
            return true;
        return escapedSlots[slot];
    }

    /*
     * Whether we distinguish different writes of this variable while doing
     * SSA analysis. Escaping locals can be written in other scripts, and the
     * presence of NAME opcodes, switch or try blocks keeps us from tracking
     * variable values at each point.
     */
    bool trackSlot(uint32 slot) { return !slotEscapes(slot) && canTrackVars; }

    const LifetimeVariable & liveness(uint32 slot) {
        JS_ASSERT(script->compartment->activeAnalysis);
        JS_ASSERT(!slotEscapes(slot));
        return lifetimes[slot];
    }

    void printSSA(JSContext *cx);
    void printTypes(JSContext *cx);

    void clearAllocations();

  private:
    void setOOM(JSContext *cx) {
        if (!outOfMemory)
            js_ReportOutOfMemory(cx);
        outOfMemory = true;
        hadFailure = true;
    }

    /* Bytecode helpers */
    inline bool addJump(JSContext *cx, unsigned offset,
                        unsigned *currentOffset, unsigned *forwardJump,
                        unsigned stackDepth, uint32 *defineArray, unsigned defineCount);
    inline void setLocal(uint32 local, uint32 offset);
    void checkAliasedName(JSContext *cx, jsbytecode *pc);

    /* Lifetime helpers */
    inline void addVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                            LifetimeVariable **&saved, unsigned &savedCount);
    inline void killVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                             LifetimeVariable **&saved, unsigned &savedCount);
    inline void extendVariable(JSContext *cx, LifetimeVariable &var, unsigned start, unsigned end);
    inline void ensureVariable(LifetimeVariable &var, unsigned until);

    /* SSA helpers */
    bool makePhi(JSContext *cx, uint32 slot, uint32 offset, SSAValue *pv);
    void insertPhi(JSContext *cx, SSAValue &phi, const SSAValue &v);
    void mergeValue(JSContext *cx, uint32 offset, const SSAValue &v, SlotValue *pv);
    void checkPendingValue(JSContext *cx, const SSAValue &v, uint32 slot,
                           Vector<SlotValue> *pending);
    void checkBranchTarget(JSContext *cx, uint32 targetOffset, Vector<uint32> &branchTargets,
                           SSAValue *values, uint32 stackDepth);
    void mergeBranchTarget(JSContext *cx, const SSAValue &value, uint32 slot,
                           const Vector<uint32> &branchTargets);
    void removeBranchTarget(Vector<uint32> &branchTargets, uint32 offset);
    void freezeNewValues(JSContext *cx, uint32 offset);

    struct TypeInferenceState {
        Vector<SSAPhiNode *> phiNodes;
        bool hasGetSet;
        bool hasHole;
        types::TypeSet *forTypes;
        TypeInferenceState(JSContext *cx)
            : phiNodes(cx), hasGetSet(false), hasHole(false), forTypes(NULL)
        {}
    };

    /* Type inference helpers */
    bool analyzeTypesBytecode(JSContext *cx, unsigned offset, TypeInferenceState &state);
    bool followEscapingArguments(JSContext *cx, const SSAValue &v, Vector<SSAValue> *seen);
    bool followEscapingArguments(JSContext *cx, SSAUseChain *use, Vector<SSAValue> *seen);
};

/* Protect analysis structures from GC while they are being used. */
struct AutoEnterAnalysis
{
    JSContext *cx;
    bool oldActiveAnalysis;
    bool left;

    AutoEnterAnalysis(JSContext *cx)
        : cx(cx), oldActiveAnalysis(cx->compartment->activeAnalysis), left(false)
    {
        cx->compartment->activeAnalysis = true;
    }

    void leave()
    {
        if (!left) {
            left = true;
            cx->compartment->activeAnalysis = oldActiveAnalysis;
        }
    }

    ~AutoEnterAnalysis()
    {
        leave();
    }
};

/* SSA value as used by CrossScriptSSA, identifies the frame it came from. */
struct CrossSSAValue
{
    unsigned frame;
    SSAValue v;
    CrossSSAValue(unsigned frame, const SSAValue &v) : frame(frame), v(v) {}
};

/*
 * Analysis for managing SSA values from multiple call stack frames. These are
 * created by the backend compiler when inlining functions, and allow for
 * values to be tracked as they flow into or out of the inlined frames.
 */
class CrossScriptSSA
{
  public:

    static const uint32 OUTER_FRAME = uint32(-1);
    static const unsigned INVALID_FRAME = uint32(-2);

    struct Frame {
        uint32 index;
        JSScript *script;
        uint32 depth;  /* Distance from outer frame to this frame, in sizeof(Value) */
        uint32 parent;
        jsbytecode *parentpc;

        Frame(uint32 index, JSScript *script, uint32 depth, uint32 parent, jsbytecode *parentpc)
            : index(index), script(script), depth(depth), parent(parent), parentpc(parentpc)
        {}
    };

    const Frame &getFrame(uint32 index) {
        if (index == OUTER_FRAME)
            return outerFrame;
        return inlineFrames[index];
    }

    unsigned numFrames() { return 1 + inlineFrames.length(); }
    const Frame &iterFrame(unsigned i) {
        if (i == 0)
            return outerFrame;
        return inlineFrames[i - 1];
    }

    JSScript *outerScript() { return outerFrame.script; }

    /* Total length of scripts preceding a frame. */
    size_t frameLength(uint32 index) {
        if (index == OUTER_FRAME)
            return 0;
        size_t res = outerFrame.script->length;
        for (unsigned i = 0; i < index; i++)
            res += inlineFrames[i].script->length;
        return res;
    }

    types::TypeSet *getValueTypes(const CrossSSAValue &cv) {
        return getFrame(cv.frame).script->analysis()->getValueTypes(cv.v);
    }

    bool addInlineFrame(JSScript *script, uint32 depth, uint32 parent, jsbytecode *parentpc)
    {
        uint32 index = inlineFrames.length();
        return inlineFrames.append(Frame(index, script, depth, parent, parentpc));
    }

    CrossScriptSSA(JSContext *cx, JSScript *outer)
        : cx(cx), outerFrame(OUTER_FRAME, outer, 0, INVALID_FRAME, NULL), inlineFrames(cx)
    {}

    CrossSSAValue foldValue(const CrossSSAValue &cv);

  private:
    JSContext *cx;

    Frame outerFrame;
    Vector<Frame> inlineFrames;
};

#ifdef DEBUG
void PrintBytecode(JSContext *cx, JSScript *script, jsbytecode *pc);
#endif

} /* namespace analyze */
} /* namespace js */

#endif // jsanalyze_h___
