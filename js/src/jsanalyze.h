/* -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set ts=4 sw=4 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Definitions for javascript analysis. */

#ifndef jsanalyze_h___
#define jsanalyze_h___

#include "mozilla/PodOperations.h"
#include "mozilla/TypeTraits.h"

#include "jsautooplen.h"
#include "jscompartment.h"
#include "jscntxt.h"
#include "jsinfer.h"
#include "jsscript.h"

#include "ds/LifoAlloc.h"
#include "js/TemplateLib.h"
#include "vm/ScopeObject.h"

class JSScript;

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

/* Information about a bytecode instruction. */
class Bytecode
{
    friend class ScriptAnalysis;

  public:
    Bytecode() { mozilla::PodZero(this); }

    /* --------- Bytecode analysis --------- */

    /* Whether there are any incoming jumps to this instruction. */
    bool jumpTarget : 1;

    /* Whether there is fallthrough to this instruction from a non-branching instruction. */
    bool fallthrough : 1;

    /* Whether this instruction is the fall through point of a conditional jump. */
    bool jumpFallthrough : 1;

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

    /* Whether this is in a loop. */
    bool inLoop : 1;

    /* Method JIT safe point. */
    bool safePoint : 1;

    /*
     * Side effects of this bytecode were not determined by type inference.
     * Either a property set with unknown lvalue, or call with unknown callee.
     */
    bool monitoredTypes : 1;

    /* Call whose result should be monitored. */
    bool monitoredTypesReturn : 1;

    /*
     * Dynamically observed state about the execution of this opcode. These are
     * hints about the script for use during compilation.
     */
    bool arrayWriteHole: 1;     /* SETELEM which has written to an array hole. */
    bool getStringElement:1;    /* GETELEM which has accessed string properties. */
    bool nonNativeGetElement:1; /* GETELEM on a non-native, non-array object. */
    bool accessGetter: 1;       /* Property read on a shape with a getter hook. */

    /* Stack depth before this opcode. */
    uint32_t stackDepth;

  private:

    union {
        /* If this is a JOF_TYPESET opcode, index into the observed types for the op. */
        types::StackTypeSet *observedTypes;

        /* If this is a JSOP_LOOPHEAD or JSOP_LOOPENTRY, information about the loop. */
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
    types::StackTypeSet *pushedTypes;

    /* Any type barriers in place at this bytecode. */
    types::TypeBarrier *typeBarriers;
};

static inline unsigned
GetDefCount(RawScript script, unsigned offset)
{
    JS_ASSERT(offset < script->length);
    jsbytecode *pc = script->code + offset;

    /*
     * Add an extra pushed value for OR/AND opcodes, so that they are included
     * in the pushed array of stack values for type inference.
     */
    switch (JSOp(*pc)) {
      case JSOP_OR:
      case JSOP_AND:
        return 1;
      case JSOP_PICK:
        /*
         * Pick pops and pushes how deep it looks in the stack + 1
         * items. i.e. if the stack were |a b[2] c[1] d[0]|, pick 2
         * would pop b, c, and d to rearrange the stack to |a c[0]
         * d[1] b[2]|.
         */
        return (pc[1] + 1);
      default:
        return StackDefs(script, pc);
    }
}

static inline unsigned
GetUseCount(RawScript script, unsigned offset)
{
    JS_ASSERT(offset < script->length);
    jsbytecode *pc = script->code + offset;

    if (JSOp(*pc) == JSOP_PICK)
        return (pc[1] + 1);
    if (js_CodeSpec[*pc].nuses == -1)
        return StackUses(script, pc);
    return js_CodeSpec[*pc].nuses;
}

/*
 * For opcodes which assign to a local variable or argument, track an extra def
 * during SSA analysis for the value's use chain and assigned type.
 */
static inline bool
ExtendedDef(jsbytecode *pc)
{
    switch ((JSOp)*pc) {
      case JSOP_SETARG:
      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
      case JSOP_SETLOCAL:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC:
        return true;
      default:
        return false;
    }
}

/* Return whether op bytecodes do not fallthrough (they may do a jump). */
static inline bool
BytecodeNoFallThrough(JSOp op)
{
    switch (op) {
      case JSOP_GOTO:
      case JSOP_DEFAULT:
      case JSOP_RETURN:
      case JSOP_STOP:
      case JSOP_RETRVAL:
      case JSOP_THROW:
      case JSOP_TABLESWITCH:
        return true;
      case JSOP_GOSUB:
        /* These fall through indirectly, after executing a 'finally'. */
        return false;
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
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE:
        return op;
      default:
        JS_NOT_REACHED("unrecognized op");
        return op;
    }
}

static inline JSOp
NegateCompareOp(JSOp op)
{
    switch (op) {
      case JSOP_GT:
        return JSOP_LE;
      case JSOP_GE:
        return JSOP_LT;
      case JSOP_LT:
        return JSOP_GE;
      case JSOP_LE:
        return JSOP_GT;
      case JSOP_EQ:
        return JSOP_NE;
      case JSOP_NE:
        return JSOP_EQ;
      case JSOP_STRICTNE:
        return JSOP_STRICTEQ;
      case JSOP_STRICTEQ:
        return JSOP_STRICTNE;
      default:
        JS_NOT_REACHED("unrecognized op");
        return op;
    }
}

static inline unsigned
FollowBranch(JSContext *cx, RawScript script, unsigned offset)
{
    /*
     * Get the target offset of a branch. For GOTO opcodes implementing
     * 'continue' statements, short circuit any artificial backwards jump
     * inserted by the emitter.
     */
    jsbytecode *pc = script->code + offset;
    unsigned targetOffset = offset + GET_JUMP_OFFSET(pc);
    if (targetOffset < offset) {
        jsbytecode *target = script->code + targetOffset;
        JSOp nop = JSOp(*target);
        if (nop == JSOP_GOTO)
            return targetOffset + GET_JUMP_OFFSET(target);
    }
    return targetOffset;
}

/* Common representation of slots throughout analyses and the compiler. */
static inline uint32_t CalleeSlot() {
    return 0;
}
static inline uint32_t ThisSlot() {
    return 1;
}
static inline uint32_t ArgSlot(uint32_t arg) {
    return 2 + arg;
}
static inline uint32_t LocalSlot(RawScript script, uint32_t local) {
    return 2 + (script->function() ? script->function()->nargs : 0) + local;
}
static inline uint32_t TotalSlots(RawScript script) {
    return LocalSlot(script, 0) + script->nfixed;
}

static inline uint32_t StackSlot(RawScript script, uint32_t index) {
    return TotalSlots(script) + index;
}

static inline uint32_t GetBytecodeSlot(RawScript script, jsbytecode *pc)
{
    switch (JSOp(*pc)) {

      case JSOP_GETARG:
      case JSOP_CALLARG:
      case JSOP_SETARG:
        return ArgSlot(GET_SLOTNO(pc));

      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL:
      case JSOP_SETLOCAL:
        return LocalSlot(script, GET_SLOTNO(pc));

      case JSOP_THIS:
        return ThisSlot();

      default:
        JS_NOT_REACHED("Bad slot opcode");
        return 0;
    }
}

/* Slot opcodes which update SSA information. */
static inline bool
BytecodeUpdatesSlot(JSOp op)
{
    return (op == JSOP_SETARG || op == JSOP_SETLOCAL);
}

static inline int32_t
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
    uint32_t start;
    uint32_t end;

    /*
     * In a loop body, endpoint to extend this lifetime with if the variable is
     * live in the next iteration.
     */
    uint32_t savedEnd;

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

    Lifetime(uint32_t offset, uint32_t savedEnd, Lifetime *next)
        : start(offset), end(offset), savedEnd(savedEnd),
          loopTail(false), write(false), next(next)
    {}
};

/* Basic information for a loop. */
class LoopAnalysis
{
  public:
    /* Any loop this one is nested in. */
    LoopAnalysis *parent;

    /* Offset of the head of the loop. */
    uint32_t head;

    /*
     * Offset of the unique jump going to the head of the loop. The code
     * between the head and the backedge forms the loop body.
     */
    uint32_t backedge;

    /* Target offset of the initial jump or fallthrough into the loop. */
    uint32_t entry;

    /*
     * Start of the last basic block in the loop, excluding the initial jump to
     * entry. All code between lastBlock and the backedge runs in every
     * iteration, and if entry >= lastBlock all code between entry and the
     * backedge runs when the loop is initially entered.
     */
    uint32_t lastBlock;

    /* Loop nesting depth, 0 for the outermost loop. */
    uint16_t depth;

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
    uint32_t savedEnd : 31;

    /* If the variable needs to be kept alive until lifetime->start. */
    bool ensured : 1;

    /* Whether this variable is live at offset. */
    Lifetime * live(uint32_t offset) const {
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
     * UINT32_MAX if the variable is not written in the range.
     */
    uint32_t firstWrite(uint32_t start, uint32_t end) const {
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= end) {
            if (segment->start >= start && segment->write)
                return segment->start;
            segment = segment->next;
        }
        return UINT32_MAX;
    }
    uint32_t firstWrite(LoopAnalysis *loop) const {
        return firstWrite(loop->head, loop->backedge);
    }

    /* Return true if the variable cannot decrease during the body of a loop. */
    bool nonDecreasing(RawScript script, LoopAnalysis *loop) const {
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
     * that write. UINT32_MAX otherwise.
     */
    uint32_t onlyWrite(LoopAnalysis *loop) const {
        uint32_t offset = UINT32_MAX;
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= loop->backedge) {
            if (segment->start >= loop->head && segment->write) {
                if (offset != UINT32_MAX)
                    return UINT32_MAX;
                offset = segment->start;
            }
            segment = segment->next;
        }
        return offset;
    }

#ifdef JS_METHODJIT_SPEW
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

    bool operator==(const SSAValue &o) const {
        return !memcmp(this, &o, sizeof(SSAValue));
    }

    /* Accessors for values pushed by a bytecode within this script. */

    uint32_t pushedOffset() const {
        JS_ASSERT(kind() == PUSHED);
        return u.pushed.offset;
    }

    uint32_t pushedIndex() const {
        JS_ASSERT(kind() == PUSHED);
        return u.pushed.index;
    }

    /* Accessors for initial and written values of arguments and (undefined) locals. */

    bool varInitial() const {
        JS_ASSERT(kind() == VAR);
        return u.var.initial;
    }

    uint32_t varSlot() const {
        JS_ASSERT(kind() == VAR);
        return u.var.slot;
    }

    uint32_t varOffset() const {
        JS_ASSERT(!varInitial());
        return u.var.offset;
    }

    /* Accessors for phi nodes. */

    uint32_t phiSlot() const;
    uint32_t phiLength() const;
    const SSAValue &phiValue(uint32_t i) const;
    types::TypeSet *phiTypes() const;

    /* Offset at which this phi node was created. */
    uint32_t phiOffset() const {
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
        mozilla::PodZero(this);
        JS_ASSERT(kind() == EMPTY);
    }

    void initPushed(uint32_t offset, uint32_t index) {
        clear();
        u.pushed.kind = PUSHED;
        u.pushed.offset = offset;
        u.pushed.index = index;
    }

    static SSAValue PushedValue(uint32_t offset, uint32_t index) {
        SSAValue v;
        v.initPushed(offset, index);
        return v;
    }

    void initInitial(uint32_t slot) {
        clear();
        u.var.kind = VAR;
        u.var.initial = true;
        u.var.slot = slot;
    }

    void initWritten(uint32_t slot, uint32_t offset) {
        clear();
        u.var.kind = VAR;
        u.var.initial = false;
        u.var.slot = slot;
        u.var.offset = offset;
    }

    static SSAValue WrittenVar(uint32_t slot, uint32_t offset) {
        SSAValue v;
        v.initWritten(slot, offset);
        return v;
    }

    void initPhi(uint32_t offset, SSAPhiNode *node) {
        clear();
        u.phi.kind = PHI;
        u.phi.offset = offset;
        u.phi.node = node;
    }

    static SSAValue PhiValue(uint32_t offset, SSAPhiNode *node) {
        SSAValue v;
        v.initPhi(offset, node);
        return v;
    }

  private:
    union {
        struct {
            Kind kind : 2;
            uint32_t offset : 30;
            uint32_t index;
        } pushed;
        struct {
            Kind kind : 2;
            bool initial : 1;
            uint32_t slot : 29;
            uint32_t offset;
        } var;
        struct {
            Kind kind : 2;
            uint32_t offset : 30;
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
    types::StackTypeSet types;
    uint32_t slot;
    uint32_t length;
    SSAValue *options;
    SSAUseChain *uses;
    SSAPhiNode() { mozilla::PodZero(this); }
};

inline uint32_t
SSAValue::phiSlot() const
{
    return u.phi.node->slot;
}

inline uint32_t
SSAValue::phiLength() const
{
    JS_ASSERT(kind() == PHI);
    return u.phi.node->length;
}

inline const SSAValue &
SSAValue::phiValue(uint32_t i) const
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

class SSAUseChain
{
  public:
    bool popped : 1;
    uint32_t offset : 31;
    /* FIXME: Assert that only the proper arm of this union is accessed. */
    union {
        uint32_t which;
        SSAPhiNode *phi;
    } u;
    SSAUseChain *next;

    SSAUseChain() { mozilla::PodZero(this); }
};

class SlotValue
{
  public:
    uint32_t slot;
    SSAValue value;
    SlotValue(uint32_t slot, const SSAValue &value) : slot(slot), value(value) {}
};

struct NeedsArgsObjState;

/* Analysis information about a script. */
class ScriptAnalysis
{
    friend class Bytecode;

    JSScript *script_;

    Bytecode **codeArray;

    uint32_t numSlots;
    uint32_t numPropertyReads_;

    bool outOfMemory;
    bool hadFailure;

    bool *escapedSlots;

    /* Which analyses have been performed. */
    bool ranBytecode_;
    bool ranSSA_;
    bool ranLifetimes_;
    bool ranInference_;

#ifdef DEBUG
    /* Whether the compartment was in debug mode when we performed the analysis. */
    bool originalDebugMode_: 1;
#endif

    /* --------- Bytecode analysis --------- */

    bool usesReturnValue_:1;
    bool usesScopeChain_:1;
    bool usesThisValue_:1;
    bool hasFunctionCalls_:1;
    bool modifiesArguments_:1;
    bool localsAliasStack_:1;
    bool isJaegerInlineable:1;
    bool isIonInlineable:1;
    bool isJaegerCompileable:1;
    bool canTrackVars:1;
    bool hasLoops_:1;

    uint32_t numReturnSites_;

    /* --------- Lifetime analysis --------- */

    LifetimeVariable *lifetimes;

  public:

    ScriptAnalysis(RawScript script) {
        mozilla::PodZero(this);
        this->script_ = script;
#ifdef DEBUG
        this->originalDebugMode_ = script_->compartment()->debugMode();
#endif
    }

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

    bool OOM() const { return outOfMemory; }
    bool failed() const { return hadFailure; }
    bool ionInlineable() const { return isIonInlineable; }
    bool ionInlineable(uint32_t argc) const { return isIonInlineable && argc == script_->function()->nargs; }
    void setIonUninlineable() { isIonInlineable = false; }
    bool jaegerInlineable() const { return isJaegerInlineable; }
    bool jaegerInlineable(uint32_t argc) const { return isJaegerInlineable && argc == script_->function()->nargs; }
    bool jaegerCompileable() { return isJaegerCompileable; }

    /* Number of property read opcodes in the script. */
    uint32_t numPropertyReads() const { return numPropertyReads_; }

    /* Whether there are POPV/SETRVAL bytecodes which can write to the frame's rval. */
    bool usesReturnValue() const { return usesReturnValue_; }

    /* Whether there are NAME bytecodes which can access the frame's scope chain. */
    bool usesScopeChain() const { return usesScopeChain_; }

    bool usesThisValue() const { return usesThisValue_; }
    bool hasFunctionCalls() const { return hasFunctionCalls_; }
    uint32_t numReturnSites() const { return numReturnSites_; }

    bool hasLoops() const { return hasLoops_; }

    /*
     * True if all named formal arguments are not modified. If the arguments
     * object cannot escape, the arguments are never modified within the script.
     */
    bool modifiesArguments() { return modifiesArguments_; }

    /*
     * True if there are any LOCAL opcodes aliasing values on the stack (above
     * script_->nfixed).
     */
    bool localsAliasStack() { return localsAliasStack_; }

    /* Accessors for bytecode information. */

    Bytecode& getCode(uint32_t offset) {
        JS_ASSERT(offset < script_->length);
        JS_ASSERT(codeArray[offset]);
        return *codeArray[offset];
    }
    Bytecode& getCode(const jsbytecode *pc) { return getCode(pc - script_->code); }

    Bytecode* maybeCode(uint32_t offset) {
        JS_ASSERT(offset < script_->length);
        return codeArray[offset];
    }
    Bytecode* maybeCode(const jsbytecode *pc) { return maybeCode(pc - script_->code); }

    bool jumpTarget(uint32_t offset) {
        JS_ASSERT(offset < script_->length);
        return codeArray[offset] && codeArray[offset]->jumpTarget;
    }
    bool jumpTarget(const jsbytecode *pc) { return jumpTarget(pc - script_->code); }

    bool popGuaranteed(jsbytecode *pc) {
        jsbytecode *next = pc + GetBytecodeLength(pc);
        return JSOp(*next) == JSOP_POP && !jumpTarget(next);
    }

    bool incrementInitialValueObserved(jsbytecode *pc) {
        const JSCodeSpec *cs = &js_CodeSpec[*pc];
        return (cs->format & JOF_POST) && !popGuaranteed(pc);
    }

    types::StackTypeSet *bytecodeTypes(const jsbytecode *pc) {
        JS_ASSERT(js_CodeSpec[*pc].format & JOF_TYPESET);
        return getCode(pc).observedTypes;
    }

    const SSAValue &poppedValue(uint32_t offset, uint32_t which) {
        JS_ASSERT(offset < script_->length);
        JS_ASSERT(which < GetUseCount(script_, offset) +
                  (ExtendedUse(script_->code + offset) ? 1 : 0));
        return getCode(offset).poppedValues[which];
    }
    const SSAValue &poppedValue(const jsbytecode *pc, uint32_t which) {
        return poppedValue(pc - script_->code, which);
    }

    const SlotValue *newValues(uint32_t offset) {
        JS_ASSERT(offset < script_->length);
        return getCode(offset).newValues;
    }
    const SlotValue *newValues(const jsbytecode *pc) { return newValues(pc - script_->code); }

    types::StackTypeSet *pushedTypes(uint32_t offset, uint32_t which = 0) {
        JS_ASSERT(offset < script_->length);
        JS_ASSERT(which < GetDefCount(script_, offset) +
                  (ExtendedDef(script_->code + offset) ? 1 : 0));
        types::StackTypeSet *array = getCode(offset).pushedTypes;
        JS_ASSERT(array);
        return array + which;
    }
    types::StackTypeSet *pushedTypes(const jsbytecode *pc, uint32_t which) {
        return pushedTypes(pc - script_->code, which);
    }

    bool hasPushedTypes(const jsbytecode *pc) { return getCode(pc).pushedTypes != NULL; }

    types::TypeBarrier *typeBarriers(JSContext *cx, uint32_t offset) {
        if (getCode(offset).typeBarriers)
            pruneTypeBarriers(cx, offset);
        return getCode(offset).typeBarriers;
    }
    types::TypeBarrier *typeBarriers(JSContext *cx, const jsbytecode *pc) {
        return typeBarriers(cx, pc - script_->code);
    }
    void addTypeBarrier(JSContext *cx, const jsbytecode *pc,
                        types::TypeSet *target, types::Type type);
    void addSingletonTypeBarrier(JSContext *cx, const jsbytecode *pc,
                                 types::TypeSet *target,
                                 HandleObject singleton, HandleId singletonId);

    /* Remove obsolete type barriers at the given offset. */
    void pruneTypeBarriers(JSContext *cx, uint32_t offset);

    /*
     * Remove still-active type barriers at the given offset. If 'all' is set,
     * then all barriers are removed, otherwise only those deemed excessive
     * are removed.
     */
    void breakTypeBarriers(JSContext *cx, uint32_t offset, bool all);

    /* Break all type barriers used in computing v. */
    void breakTypeBarriersSSA(JSContext *cx, const SSAValue &v);

    inline void addPushedType(JSContext *cx, uint32_t offset, uint32_t which, types::Type type);

    types::StackTypeSet *getValueTypes(const SSAValue &v) {
        switch (v.kind()) {
          case SSAValue::PUSHED:
            return pushedTypes(v.pushedOffset(), v.pushedIndex());
          case SSAValue::VAR:
            JS_ASSERT(!slotEscapes(v.varSlot()));
            if (v.varInitial()) {
                return types::TypeScript::SlotTypes(script_, v.varSlot());
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

    types::StackTypeSet *poppedTypes(uint32_t offset, uint32_t which) {
        return getValueTypes(poppedValue(offset, which));
    }
    types::StackTypeSet *poppedTypes(const jsbytecode *pc, uint32_t which) {
        return getValueTypes(poppedValue(pc, which));
    }

    /* Whether an arithmetic operation is operating on integers, with an integer result. */
    bool integerOperation(jsbytecode *pc);

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
            return getCode(v.varOffset()).pushedUses[GetDefCount(script_, v.varOffset())];
        return v.phiNode()->uses;
    }

    mjit::RegisterAllocation *&getAllocation(uint32_t offset) {
        JS_ASSERT(offset < script_->length);
        return getCode(offset).allocation;
    }
    mjit::RegisterAllocation *&getAllocation(const jsbytecode *pc) {
        return getAllocation(pc - script_->code);
    }

    LoopAnalysis *getLoop(uint32_t offset) {
        JS_ASSERT(offset < script_->length);
        return getCode(offset).loop;
    }
    LoopAnalysis *getLoop(const jsbytecode *pc) { return getLoop(pc - script_->code); }

    /* For a JSOP_CALL* op, get the pc of the corresponding JSOP_CALL/NEW/etc. */
    jsbytecode *getCallPC(jsbytecode *pc)
    {
        SSAUseChain *uses = useChain(SSAValue::PushedValue(pc - script_->code, 0));
        JS_ASSERT(uses && uses->popped);
        JS_ASSERT(js_CodeSpec[script_->code[uses->offset]].format & JOF_INVOKE);
        return script_->code + uses->offset;
    }

    /* Accessors for local variable information. */

    /*
     * Escaping slots include all slots that can be accessed in ways other than
     * through the corresponding LOCAL/ARG opcode. This includes all closed
     * slots in the script, all slots in scripts which use eval or are in debug
     * mode, and slots which are aliased by NAME or similar opcodes in the
     * containing script (which does not imply the variable is closed).
     */
    bool slotEscapes(uint32_t slot) {
        JS_ASSERT(script_->compartment()->activeAnalysis);
        if (slot >= numSlots)
            return true;
        return escapedSlots[slot];
    }

    /*
     * Whether we distinguish different writes of this variable while doing
     * SSA analysis. Escaping locals can be written in other scripts, and the
     * presence of NAME opcodes which could alias local variables or arguments
     * keeps us from tracking variable values at each point.
     */
    bool trackSlot(uint32_t slot) { return !slotEscapes(slot) && canTrackVars && slot < 1000; }

    const LifetimeVariable & liveness(uint32_t slot) {
        JS_ASSERT(script_->compartment()->activeAnalysis);
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
                        unsigned *currentOffset, unsigned *forwardJump, unsigned *forwardLoop,
                        unsigned stackDepth);

    /* Lifetime helpers */
    inline void addVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                            LifetimeVariable **&saved, unsigned &savedCount);
    inline void killVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                             LifetimeVariable **&saved, unsigned &savedCount);
    inline void extendVariable(JSContext *cx, LifetimeVariable &var, unsigned start, unsigned end);
    inline void ensureVariable(LifetimeVariable &var, unsigned until);

    /* Current value for a variable or stack value, as tracked during SSA. */
    struct SSAValueInfo
    {
        SSAValue v;

        /*
         * Sizes of branchTargets the last time this slot was written. Branches less
         * than this threshold do not need to be inspected if the slot is written
         * again, as they will already reflect the slot's value at the branch.
         */
        int32_t branchSize;
    };

    /* SSA helpers */
    bool makePhi(JSContext *cx, uint32_t slot, uint32_t offset, SSAValue *pv);
    void insertPhi(JSContext *cx, SSAValue &phi, const SSAValue &v);
    void mergeValue(JSContext *cx, uint32_t offset, const SSAValue &v, SlotValue *pv);
    void checkPendingValue(JSContext *cx, const SSAValue &v, uint32_t slot,
                           Vector<SlotValue> *pending);
    void checkBranchTarget(JSContext *cx, uint32_t targetOffset, Vector<uint32_t> &branchTargets,
                           SSAValueInfo *values, uint32_t stackDepth);
    void checkExceptionTarget(JSContext *cx, uint32_t catchOffset,
                              Vector<uint32_t> &exceptionTargets);
    void mergeBranchTarget(JSContext *cx, SSAValueInfo &value, uint32_t slot,
                           const Vector<uint32_t> &branchTargets, uint32_t currentOffset);
    void mergeExceptionTarget(JSContext *cx, const SSAValue &value, uint32_t slot,
                              const Vector<uint32_t> &exceptionTargets);
    void mergeAllExceptionTargets(JSContext *cx, SSAValueInfo *values,
                                  const Vector<uint32_t> &exceptionTargets);
    void freezeNewValues(JSContext *cx, uint32_t offset);

    struct TypeInferenceState {
        Vector<SSAPhiNode *> phiNodes;
        bool hasGetSet;
        bool hasHole;
        types::StackTypeSet *forTypes;
        bool hasPropertyReadTypes;
        uint32_t propertyReadIndex;
        TypeInferenceState(JSContext *cx)
            : phiNodes(cx), hasGetSet(false), hasHole(false), forTypes(NULL),
              hasPropertyReadTypes(false), propertyReadIndex(0)
        {}
    };

    /* Type inference helpers */
    bool analyzeTypesBytecode(JSContext *cx, unsigned offset, TypeInferenceState &state);

    typedef Vector<SSAValue, 16> SeenVector;
    bool needsArgsObj(JSContext *cx, SeenVector &seen, const SSAValue &v);
    bool needsArgsObj(JSContext *cx, SeenVector &seen, SSAUseChain *use);
    bool needsArgsObj(JSContext *cx);

  public:
#ifdef DEBUG
    void assertMatchingDebugMode();
#else
    void assertMatchingDebugMode() { }
#endif
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

    static const uint32_t OUTER_FRAME = UINT32_MAX;
    static const unsigned INVALID_FRAME = uint32_t(-2);

    struct Frame {
        uint32_t index;
        JSScript *script;
        uint32_t depth;  /* Distance from outer frame to this frame, in sizeof(Value) */
        uint32_t parent;
        jsbytecode *parentpc;

        Frame(uint32_t index, RawScript script, uint32_t depth, uint32_t parent,
              jsbytecode *parentpc)
          : index(index), script(script), depth(depth), parent(parent), parentpc(parentpc)
        {}
    };

    const Frame &getFrame(uint32_t index) {
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

    RawScript outerScript() { return outerFrame.script; }

    /* Total length of scripts preceding a frame. */
    size_t frameLength(uint32_t index) {
        if (index == OUTER_FRAME)
            return 0;
        size_t res = outerFrame.script->length;
        for (unsigned i = 0; i < index; i++)
            res += inlineFrames[i].script->length;
        return res;
    }

    types::StackTypeSet *getValueTypes(const CrossSSAValue &cv) {
        return getFrame(cv.frame).script->analysis()->getValueTypes(cv.v);
    }

    bool addInlineFrame(RawScript script, uint32_t depth, uint32_t parent,
                        jsbytecode *parentpc)
    {
        uint32_t index = inlineFrames.length();
        return inlineFrames.append(Frame(index, script, depth, parent, parentpc));
    }

    CrossScriptSSA(JSContext *cx, RawScript outer)
        : outerFrame(OUTER_FRAME, outer, 0, INVALID_FRAME, NULL), inlineFrames(cx)
    {}

    CrossSSAValue foldValue(const CrossSSAValue &cv);

  private:
    Frame outerFrame;
    Vector<Frame> inlineFrames;
};

#ifdef DEBUG
void PrintBytecode(JSContext *cx, HandleScript script, jsbytecode *pc);
#endif

} /* namespace analyze */
} /* namespace js */

namespace mozilla {

template <> struct IsPod<js::analyze::LifetimeVariable> : TrueType {};
template <> struct IsPod<js::analyze::LoopAnalysis>     : TrueType {};
template <> struct IsPod<js::analyze::SlotValue>        : TrueType {};
template <> struct IsPod<js::analyze::SSAValue>         : TrueType {};
template <> struct IsPod<js::analyze::SSAUseChain>      : TrueType {};

} /* namespace mozilla */

#endif // jsanalyze_h___
