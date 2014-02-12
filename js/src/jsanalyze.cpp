/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/PodOperations.h"

#include "jscntxt.h"
#include "jscompartment.h"

#include "vm/Opcodes.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"

using mozilla::DebugOnly;
using mozilla::PodCopy;
using mozilla::PodZero;
using mozilla::FloorLog2;

namespace js {
namespace analyze {
class LoopAnalysis;
} // namespace analyze
} // namespace js

namespace mozilla {

template <> struct IsPod<js::analyze::LifetimeVariable> : TrueType {};
template <> struct IsPod<js::analyze::LoopAnalysis>     : TrueType {};
template <> struct IsPod<js::analyze::SlotValue>        : TrueType {};
template <> struct IsPod<js::analyze::SSAValue>         : TrueType {};
template <> struct IsPod<js::analyze::SSAUseChain>      : TrueType {};

} /* namespace mozilla */

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

    /* Stack depth before this opcode. */
    uint32_t stackDepth;

  private:

    /* If this is a JSOP_LOOPHEAD or JSOP_LOOPENTRY, information about the loop. */
    LoopAnalysis *loop;

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
};

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
     * The start of this lifetime is a bytecode writing the variable. Each
     * write to a variable is associated with a lifetime.
     */
    bool write;

    /* Next lifetime. The variable is dead from this->end to next->start. */
    Lifetime *next;

    Lifetime(uint32_t offset, uint32_t savedEnd, Lifetime *next)
        : start(offset), end(offset), savedEnd(savedEnd),
          write(false), next(next)
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
        return nullptr;
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

/////////////////////////////////////////////////////////////////////
// Bytecode
/////////////////////////////////////////////////////////////////////

#ifdef DEBUG
void
PrintBytecode(JSContext *cx, HandleScript script, jsbytecode *pc)
{
    fprintf(stderr, "#%u:", script->id());
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return;
    js_Disassemble1(cx, script, pc, script->pcToOffset(pc), true, &sprinter);
    fprintf(stderr, "%s", sprinter.string());
}
#endif

/*
 * For opcodes which assign to a local variable or argument, track an extra def
 * during SSA analysis for the value's use chain and assigned type.
 */
static inline bool
ExtendedDef(jsbytecode *pc)
{
    switch ((JSOp)*pc) {
      case JSOP_SETARG:
      case JSOP_SETLOCAL:
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

static inline unsigned
FollowBranch(JSContext *cx, JSScript *script, unsigned offset)
{
    /*
     * Get the target offset of a branch. For GOTO opcodes implementing
     * 'continue' statements, short circuit any artificial backwards jump
     * inserted by the emitter.
     */
    jsbytecode *pc = script->offsetToPC(offset);
    unsigned targetOffset = offset + GET_JUMP_OFFSET(pc);
    if (targetOffset < offset) {
        jsbytecode *target = script->offsetToPC(targetOffset);
        JSOp nop = JSOp(*target);
        if (nop == JSOP_GOTO)
            return targetOffset + GET_JUMP_OFFSET(target);
    }
    return targetOffset;
}

static inline uint32_t StackSlot(JSScript *script, uint32_t index) {
    return TotalSlots(script) + index;
}

static inline uint32_t GetBytecodeSlot(JSScript *script, jsbytecode *pc)
{
    switch (JSOp(*pc)) {

      case JSOP_GETARG:
      case JSOP_CALLARG:
      case JSOP_SETARG:
        return ArgSlot(GET_ARGNO(pc));

      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL:
      case JSOP_SETLOCAL:
        return LocalSlot(script, GET_LOCALNO(pc));

      case JSOP_THIS:
        return ThisSlot();

      default:
        MOZ_ASSUME_UNREACHABLE("Bad slot opcode");
    }
}

/////////////////////////////////////////////////////////////////////
// Bytecode Analysis
/////////////////////////////////////////////////////////////////////

inline bool
ScriptAnalysis::addJump(JSContext *cx, unsigned offset,
                        unsigned *currentOffset, unsigned *forwardJump, unsigned *forwardLoop,
                        unsigned stackDepth)
{
    JS_ASSERT(offset < script_->length());

    Bytecode *&code = codeArray[offset];
    if (!code) {
        code = cx->typeLifoAlloc().new_<Bytecode>();
        if (!code) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        code->stackDepth = stackDepth;
    }
    JS_ASSERT(code->stackDepth == stackDepth);

    code->jumpTarget = true;

    if (offset < *currentOffset) {
        if (!code->analyzed) {
            /*
             * Backedge in a while/for loop, whose body has not been analyzed
             * due to a lack of fallthrough at the loop head. Roll back the
             * offset to analyze the body.
             */
            if (*forwardJump == 0)
                *forwardJump = *currentOffset;
            if (*forwardLoop == 0)
                *forwardLoop = *currentOffset;
            *currentOffset = offset;
        }
    } else if (offset > *forwardJump) {
        *forwardJump = offset;
    }

    return true;
}

inline bool
ScriptAnalysis::jumpTarget(uint32_t offset)
{
    JS_ASSERT(offset < script_->length());
    return codeArray[offset] && codeArray[offset]->jumpTarget;
}

inline bool
ScriptAnalysis::jumpTarget(const jsbytecode *pc)
{
    return jumpTarget(script_->pcToOffset(pc));
}

inline const SSAValue &
ScriptAnalysis::poppedValue(uint32_t offset, uint32_t which)
{
    JS_ASSERT(which < GetUseCount(script_, offset) +
              (ExtendedUse(script_->offsetToPC(offset)) ? 1 : 0));
    return getCode(offset).poppedValues[which];
}

inline const SSAValue &
ScriptAnalysis::poppedValue(const jsbytecode *pc, uint32_t which)
{
    return poppedValue(script_->pcToOffset(pc), which);
}

inline const SlotValue *
ScriptAnalysis::newValues(uint32_t offset)
{
    JS_ASSERT(offset < script_->length());
    return getCode(offset).newValues;
}

inline const SlotValue *
ScriptAnalysis::newValues(const jsbytecode *pc)
{
    return newValues(script_->pcToOffset(pc));
}

inline bool
ScriptAnalysis::trackUseChain(const SSAValue &v)
{
    JS_ASSERT_IF(v.kind() == SSAValue::VAR, trackSlot(v.varSlot()));
    return v.kind() != SSAValue::EMPTY &&
        (v.kind() != SSAValue::VAR || !v.varInitial());
}

/*
 * Get the use chain for an SSA value. May be invalid for some opcodes in
 * scripts where localsAliasStack(). You have been warned!
 */
inline SSAUseChain *&
ScriptAnalysis::useChain(const SSAValue &v)
{
    JS_ASSERT(trackUseChain(v));
    if (v.kind() == SSAValue::PUSHED)
        return getCode(v.pushedOffset()).pushedUses[v.pushedIndex()];
    if (v.kind() == SSAValue::VAR)
        return getCode(v.varOffset()).pushedUses[GetDefCount(script_, v.varOffset())];
    return v.phiNode()->uses;
}

/* For a JSOP_CALL* op, get the pc of the corresponding JSOP_CALL/NEW/etc. */
inline jsbytecode *
ScriptAnalysis::getCallPC(jsbytecode *pc)
{
    SSAUseChain *uses = useChain(SSAValue::PushedValue(script_->pcToOffset(pc), 0));
    JS_ASSERT(uses && uses->popped);
    JS_ASSERT(js_CodeSpec[script_->code()[uses->offset]].format & JOF_INVOKE);
    return script_->offsetToPC(uses->offset);
}

/* Accessors for local variable information. */

/*
 * Escaping slots include all slots that can be accessed in ways other than
 * through the corresponding LOCAL/ARG opcode. This includes all closed
 * slots in the script, all slots in scripts which use eval or are in debug
 * mode, and slots which are aliased by NAME or similar opcodes in the
 * containing script (which does not imply the variable is closed).
 */
inline bool
ScriptAnalysis::slotEscapes(uint32_t slot)
{
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
inline bool
ScriptAnalysis::trackSlot(uint32_t slot)
{
    return !slotEscapes(slot) && canTrackVars && slot < 1000;
}

inline const LifetimeVariable &
ScriptAnalysis::liveness(uint32_t slot)
{
    JS_ASSERT(script_->compartment()->activeAnalysis);
    JS_ASSERT(!slotEscapes(slot));
    return lifetimes[slot];
}

bool
ScriptAnalysis::analyzeBytecode(JSContext *cx)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);
    JS_ASSERT(!codeArray);

    LifoAlloc &alloc = cx->typeLifoAlloc();

    numSlots = TotalSlots(script_);

    unsigned length = script_->length();
    codeArray = alloc.newArray<Bytecode*>(length);
    escapedSlots = alloc.newArray<bool>(numSlots);

    if (!codeArray || !escapedSlots) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    PodZero(codeArray, length);

    /*
     * Populate arg and local slots which can escape and be accessed in ways
     * other than through ARG* and LOCAL* opcodes (though arguments can still
     * be indirectly read but not written through 'arguments' properties).
     * All escaping locals are treated as having possible use-before-defs.
     * Conservatively use 'argumentsHasVarBinding' instead of 'needsArgsObj'
     * (needsArgsObj requires SSA which requires escapedSlots). Lastly, the
     * debugger can access any local at any time. Even though debugger
     * reads/writes are monitored by the DebugScopeProxy, this monitoring
     * updates the flow-insensitive type sets, so we cannot use SSA.
     */

    PodZero(escapedSlots, numSlots);

    bool allVarsAliased = script_->compartment()->debugMode();
    bool allArgsAliased = allVarsAliased || script_->argumentsHasVarBinding();

    RootedScript script(cx, script_);
    for (BindingIter bi(script); bi; bi++) {
        if (bi->kind() == Binding::ARGUMENT)
            escapedSlots[ArgSlot(bi.frameIndex())] = allArgsAliased || bi->aliased();
        else
            escapedSlots[LocalSlot(script_, bi.frameIndex())] = allVarsAliased || bi->aliased();
    }

    canTrackVars = true;

    /*
     * If we are in the middle of one or more jumps, the offset of the highest
     * target jumping over this bytecode.  Includes implicit jumps from
     * try/catch/finally blocks.
     */
    unsigned forwardJump = 0;

    /* If we are in the middle of a loop, the offset of the highest backedge. */
    unsigned forwardLoop = 0;

    /*
     * If we are in the middle of a try block, the offset of the highest
     * catch/finally/enditer.
     */
    unsigned forwardCatch = 0;

    /* Fill in stack depth and definitions at initial bytecode. */
    Bytecode *startcode = alloc.new_<Bytecode>();
    if (!startcode) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    startcode->stackDepth = 0;
    codeArray[0] = startcode;

    unsigned offset, nextOffset = 0;
    while (nextOffset < length) {
        offset = nextOffset;

        JS_ASSERT(forwardCatch <= forwardJump);

        /* Check if the current forward jump/try-block has finished. */
        if (forwardJump && forwardJump == offset)
            forwardJump = 0;
        if (forwardCatch && forwardCatch == offset)
            forwardCatch = 0;

        Bytecode *code = maybeCode(offset);
        jsbytecode *pc = script_->offsetToPC(offset);

        JSOp op = (JSOp)*pc;
        JS_ASSERT(op < JSOP_LIMIT);

        /* Immediate successor of this bytecode. */
        unsigned successorOffset = offset + GetBytecodeLength(pc);

        /*
         * Next bytecode to analyze.  This is either the successor, or is an
         * earlier bytecode if this bytecode has a loop backedge.
         */
        nextOffset = successorOffset;

        if (!code) {
            /* Haven't found a path by which this bytecode is reachable. */
            continue;
        }

        /*
         * Update info about bytecodes inside loops, which may have been
         * analyzed before the backedge was seen.
         */
        if (forwardLoop) {
            if (forwardLoop <= offset)
                forwardLoop = 0;
        }

        if (code->analyzed) {
            /* No need to reanalyze, see Bytecode::mergeDefines. */
            continue;
        }

        code->analyzed = true;

        if (script_->hasBreakpointsAt(pc))
            canTrackVars = false;

        unsigned stackDepth = code->stackDepth;

        if (!forwardJump)
            code->unconditional = true;

        unsigned nuses = GetUseCount(script_, offset);
        unsigned ndefs = GetDefCount(script_, offset);

        JS_ASSERT(stackDepth >= nuses);
        stackDepth -= nuses;
        stackDepth += ndefs;

        switch (op) {

          case JSOP_DEFFUN:
          case JSOP_DEFVAR:
          case JSOP_DEFCONST:
          case JSOP_SETCONST:
            canTrackVars = false;
            break;

          case JSOP_EVAL:
          case JSOP_SPREADEVAL:
          case JSOP_ENTERWITH:
            canTrackVars = false;
            break;

          case JSOP_TABLESWITCH: {
            unsigned defaultOffset = offset + GET_JUMP_OFFSET(pc);
            jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
            int32_t low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            int32_t high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            if (!addJump(cx, defaultOffset, &nextOffset, &forwardJump, &forwardLoop, stackDepth))
                return false;

            for (int32_t i = low; i <= high; i++) {
                unsigned targetOffset = offset + GET_JUMP_OFFSET(pc2);
                if (targetOffset != offset) {
                    if (!addJump(cx, targetOffset, &nextOffset, &forwardJump, &forwardLoop, stackDepth))
                        return false;
                }
                pc2 += JUMP_OFFSET_LEN;
            }
            break;
          }

          case JSOP_TRY: {
            /*
             * Everything between a try and corresponding catch or finally is conditional.
             * Note that there is no problem with code which is skipped by a thrown
             * exception but is not caught by a later handler in the same function:
             * no more code will execute, and it does not matter what is defined.
             */
            JSTryNote *tn = script_->trynotes()->vector;
            JSTryNote *tnlimit = tn + script_->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script_->mainOffset() + tn->start;
                if (startOffset == offset + 1) {
                    unsigned catchOffset = startOffset + tn->length;

                    /* This will overestimate try block code, for multiple catch/finally. */
                    if (catchOffset > forwardCatch)
                        forwardCatch = catchOffset;

                    if (tn->kind != JSTRY_ITER && tn->kind != JSTRY_LOOP) {
                        if (!addJump(cx, catchOffset, &nextOffset, &forwardJump, &forwardLoop, stackDepth))
                            return false;
                        getCode(catchOffset).exceptionEntry = true;
                    }
                }
            }
            break;
          }

          case JSOP_GETLOCAL:
          case JSOP_CALLLOCAL:
          case JSOP_SETLOCAL:
            JS_ASSERT(GET_LOCALNO(pc) < script_->nfixed());
            break;

          case JSOP_PUSHBLOCKSCOPE:
            localsAliasStack_ = true;
            break;

          default:
            break;
        }

        bool jump = IsJumpOpcode(op);

        /* Check basic jump opcodes, which may or may not have a fallthrough. */
        if (jump) {
            /* Case instructions do not push the lvalue back when branching. */
            unsigned newStackDepth = stackDepth;
            if (op == JSOP_CASE)
                newStackDepth--;

            unsigned targetOffset = offset + GET_JUMP_OFFSET(pc);
            if (!addJump(cx, targetOffset, &nextOffset, &forwardJump, &forwardLoop, newStackDepth))
                return false;
        }

        /* Handle any fallthrough from this opcode. */
        if (BytecodeFallsThrough(op)) {
            JS_ASSERT(successorOffset < script_->length());

            Bytecode *&nextcode = codeArray[successorOffset];

            if (!nextcode) {
                nextcode = alloc.new_<Bytecode>();
                if (!nextcode) {
                    js_ReportOutOfMemory(cx);
                    return false;
                }
                nextcode->stackDepth = stackDepth;
            }
            JS_ASSERT(nextcode->stackDepth == stackDepth);

            if (jump)
                nextcode->jumpFallthrough = true;

            /* Treat the fallthrough of a branch instruction as a jump target. */
            if (jump)
                nextcode->jumpTarget = true;
            else
                nextcode->fallthrough = true;
        }
    }

    JS_ASSERT(forwardJump == 0 && forwardLoop == 0 && forwardCatch == 0);

    /*
     * Always ensure that a script's arguments usage has been analyzed before
     * entering the script. This allows the functionPrologue to ensure that
     * arguments are always created eagerly which simplifies interp logic.
     */
    if (!script_->analyzedArgsUsage()) {
        if (!analyzeLifetimes(cx))
            return false;
        if (!analyzeSSA(cx))
            return false;

        /*
         * Now that we have full SSA information for the script, analyze whether
         * we can avoid creating the arguments object.
         */
        script_->setNeedsArgsObj(needsArgsObj(cx));
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
// Lifetime Analysis
/////////////////////////////////////////////////////////////////////

bool
ScriptAnalysis::analyzeLifetimes(JSContext *cx)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);
    JS_ASSERT(codeArray);
    JS_ASSERT(!lifetimes);

    LifoAlloc &alloc = cx->typeLifoAlloc();

    lifetimes = alloc.newArray<LifetimeVariable>(numSlots);
    if (!lifetimes) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    PodZero(lifetimes, numSlots);

    /*
     * Variables which are currently dead. On forward branches to locations
     * where these are live, they need to be marked as live.
     */
    LifetimeVariable **saved = cx->pod_calloc<LifetimeVariable*>(numSlots);
    if (!saved) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    unsigned savedCount = 0;

    LoopAnalysis *loop = nullptr;

    uint32_t offset = script_->length() - 1;
    while (offset < script_->length()) {
        Bytecode *code = maybeCode(offset);
        if (!code) {
            offset--;
            continue;
        }

        jsbytecode *pc = script_->offsetToPC(offset);

        JSOp op = (JSOp) *pc;

        if (op == JSOP_LOOPHEAD && code->loop) {
            /*
             * This is the head of a loop, we need to go and make sure that any
             * variables live at the head are live at the backedge and points prior.
             * For each such variable, look for the last lifetime segment in the body
             * and extend it to the end of the loop.
             */
            JS_ASSERT(loop == code->loop);
            unsigned backedge = code->loop->backedge;
            for (unsigned i = 0; i < numSlots; i++) {
                if (lifetimes[i].lifetime) {
                    if (!extendVariable(cx, lifetimes[i], offset, backedge))
                        return false;
                }
            }

            loop = loop->parent;
            JS_ASSERT_IF(loop, loop->head < offset);
        }

        if (code->exceptionEntry) {
            DebugOnly<bool> found = false;
            JSTryNote *tn = script_->trynotes()->vector;
            JSTryNote *tnlimit = tn + script_->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script_->mainOffset() + tn->start;
                if (startOffset + tn->length == offset) {
                    /*
                     * Extend all live variables at exception entry to the start of
                     * the try block.
                     */
                    for (unsigned i = 0; i < numSlots; i++) {
                        if (lifetimes[i].lifetime)
                            ensureVariable(lifetimes[i], startOffset - 1);
                    }

                    found = true;
                    break;
                }
            }
            JS_ASSERT(found);
        }

        switch (op) {
          case JSOP_GETARG:
          case JSOP_CALLARG:
          case JSOP_GETLOCAL:
          case JSOP_CALLLOCAL:
          case JSOP_THIS: {
            uint32_t slot = GetBytecodeSlot(script_, pc);
            if (!slotEscapes(slot)) {
                if (!addVariable(cx, lifetimes[slot], offset, saved, savedCount))
                    return false;
            }
            break;
          }

          case JSOP_SETARG:
          case JSOP_SETLOCAL: {
            uint32_t slot = GetBytecodeSlot(script_, pc);
            if (!slotEscapes(slot)) {
                if (!killVariable(cx, lifetimes[slot], offset, saved, savedCount))
                    return false;
            }
            break;
          }

          case JSOP_TABLESWITCH:
            /* Restore all saved variables. :FIXME: maybe do this precisely. */
            for (unsigned i = 0; i < savedCount; i++) {
                LifetimeVariable &var = *saved[i];
                uint32_t savedEnd = var.savedEnd;
                var.lifetime = alloc.new_<Lifetime>(offset, savedEnd, var.saved);
                if (!var.lifetime) {
                    js_free(saved);
                    js_ReportOutOfMemory(cx);
                    return false;
                }
                var.saved = nullptr;
                saved[i--] = saved[--savedCount];
            }
            savedCount = 0;
            break;

          case JSOP_TRY:
            for (unsigned i = 0; i < numSlots; i++) {
                LifetimeVariable &var = lifetimes[i];
                if (var.ensured) {
                    JS_ASSERT(var.lifetime);
                    if (var.lifetime->start == offset)
                        var.ensured = false;
                }
            }
            break;

          case JSOP_LOOPENTRY:
            getCode(offset).loop = loop;
            break;

          default:;
        }

        if (IsJumpOpcode(op)) {
            /*
             * Forward jumps need to pull in all variables which are live at
             * their target offset --- the variables live before the jump are
             * the union of those live at the fallthrough and at the target.
             */
            uint32_t targetOffset = FollowBranch(cx, script_, offset);

            if (targetOffset < offset) {
                /* This is a loop back edge, no lifetime to pull in yet. */

#ifdef DEBUG
                JSOp nop = JSOp(script_->code()[targetOffset]);
                JS_ASSERT(nop == JSOP_LOOPHEAD);
#endif

                LoopAnalysis *nloop = alloc.new_<LoopAnalysis>();
                if (!nloop) {
                    js_free(saved);
                    js_ReportOutOfMemory(cx);
                    return false;
                }
                PodZero(nloop);

                nloop->parent = loop;
                loop = nloop;

                getCode(targetOffset).loop = loop;
                loop->head = targetOffset;
                loop->backedge = offset;

                /*
                 * Find the entry jump, which will be a GOTO for 'for' or
                 * 'while' loops or a fallthrough for 'do while' loops.
                 */
                uint32_t entry = targetOffset;
                if (entry) {
                    do {
                        entry--;
                    } while (!maybeCode(entry));

                    jsbytecode *entrypc = script_->offsetToPC(entry);

                    if (JSOp(*entrypc) == JSOP_GOTO)
                        entry += GET_JUMP_OFFSET(entrypc);
                    else
                        entry = targetOffset;
                } else {
                    /* Do-while loop at the start of the script. */
                    entry = targetOffset;
                }
                JS_ASSERT(script_->code()[entry] == JSOP_LOOPHEAD ||
                          script_->code()[entry] == JSOP_LOOPENTRY);
            } else {
                for (unsigned i = 0; i < savedCount; i++) {
                    LifetimeVariable &var = *saved[i];
                    JS_ASSERT(!var.lifetime && var.saved);
                    if (var.live(targetOffset)) {
                        /*
                         * Jumping to a place where this variable is live. Make a new
                         * lifetime segment for the variable.
                         */
                        uint32_t savedEnd = var.savedEnd;
                        var.lifetime = alloc.new_<Lifetime>(offset, savedEnd, var.saved);
                        if (!var.lifetime) {
                            js_free(saved);
                            js_ReportOutOfMemory(cx);
                            return false;
                        }
                        var.saved = nullptr;
                        saved[i--] = saved[--savedCount];
                    } else if (loop && !var.savedEnd) {
                        /*
                         * This jump precedes the basic block which killed the variable,
                         * remember it and use it for the end of the next lifetime
                         * segment should the variable become live again. This is needed
                         * for loops, as if we wrap liveness around the loop the isLive
                         * test below may have given the wrong answer.
                         */
                        var.savedEnd = offset;
                    }
                }
            }
        }

        offset--;
    }

    js_free(saved);

    return true;
}

#ifdef DEBUG
void
LifetimeVariable::print() const
{
    Lifetime *segment = lifetime ? lifetime : saved;
    while (segment) {
        printf(" (%u,%u)", segment->start, segment->end);
        segment = segment->next;
    }
    printf("\n");
}
#endif /* DEBUG */

inline bool
ScriptAnalysis::addVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                            LifetimeVariable **&saved, unsigned &savedCount)
{
    if (var.lifetime) {
        if (var.ensured)
            return true;

        JS_ASSERT(offset < var.lifetime->start);
        var.lifetime->start = offset;
    } else {
        if (var.saved) {
            /* Remove from the list of saved entries. */
            for (unsigned i = 0; i < savedCount; i++) {
                if (saved[i] == &var) {
                    JS_ASSERT(savedCount);
                    saved[i--] = saved[--savedCount];
                    break;
                }
            }
        }
        uint32_t savedEnd = var.savedEnd;
        var.lifetime = cx->typeLifoAlloc().new_<Lifetime>(offset, savedEnd, var.saved);
        if (!var.lifetime) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        var.saved = nullptr;
    }

    return true;
}

inline bool
ScriptAnalysis::killVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                             LifetimeVariable **&saved, unsigned &savedCount)
{
    if (!var.lifetime) {
        /* Make a point lifetime indicating the write. */
        uint32_t savedEnd = var.savedEnd;
        Lifetime *lifetime = cx->typeLifoAlloc().new_<Lifetime>(offset, savedEnd, var.saved);
        if (!lifetime) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        if (!var.saved)
            saved[savedCount++] = &var;
        var.saved = lifetime;
        var.saved->write = true;
        var.savedEnd = 0;
        return true;
    }

    JS_ASSERT_IF(!var.ensured, offset < var.lifetime->start);
    unsigned start = var.lifetime->start;

    /*
     * The variable is considered to be live at the bytecode which kills it
     * (just not at earlier bytecodes). This behavior is needed by downstream
     * register allocation (see FrameState::bestEvictReg).
     */
    var.lifetime->start = offset;
    var.lifetime->write = true;

    if (var.ensured) {
        /*
         * The variable is live even before the write, due to an enclosing try
         * block. We need to split the lifetime to indicate there was a write.
         * We set the new interval's savedEnd to 0, since it will always be
         * adjacent to the old interval, so it never needs to be extended.
         */
        var.lifetime = cx->typeLifoAlloc().new_<Lifetime>(start, 0, var.lifetime);
        if (!var.lifetime) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        var.lifetime->end = offset;
    } else {
        var.saved = var.lifetime;
        var.savedEnd = 0;
        var.lifetime = nullptr;

        saved[savedCount++] = &var;
    }

    return true;
}

inline bool
ScriptAnalysis::extendVariable(JSContext *cx, LifetimeVariable &var,
                               unsigned start, unsigned end)
{
    JS_ASSERT(var.lifetime);
    if (var.ensured) {
        /*
         * If we are still ensured to be live, the try block must scope over
         * the loop, in which case the variable is already guaranteed to be
         * live for the entire loop.
         */
        JS_ASSERT(var.lifetime->start < start);
        return true;
    }

    var.lifetime->start = start;

    /*
     * Consider this code:
     *
     *   while (...) { (#1)
     *       use x;    (#2)
     *       ...
     *       x = ...;  (#3)
     *       ...
     *   }             (#4)
     *
     * Just before analyzing the while statement, there would be a live range
     * from #1..#2 and a "point range" at #3. The job of extendVariable is to
     * create a new live range from #3..#4.
     *
     * However, more extensions may be required if the definition of x is
     * conditional. Consider the following.
     *
     *   while (...) {     (#1)
     *       use x;        (#2)
     *       ...
     *       if (...)      (#5)
     *           x = ...;  (#3)
     *       ...
     *   }                 (#4)
     *
     * Assume that x is not used after the loop. Then, before extendVariable is
     * run, the live ranges would be the same as before (#1..#2 and #3..#3). We
     * still need to create a range from #3..#4. But, since the assignment at #3
     * may never run, we also need to create a range from #2..#3. This is done
     * as follows.
     *
     * Each time we create a Lifetime, we store the start of the most recently
     * seen sequence of conditional code in the Lifetime's savedEnd field. So,
     * when creating the Lifetime at #2, we set the Lifetime's savedEnd to
     * #5. (The start of the most recent conditional is cached in each
     * variable's savedEnd field.) Consequently, extendVariable is able to
     * create a new interval from #2..#5 using the savedEnd field of the
     * existing #1..#2 interval.
     */

    Lifetime *segment = var.lifetime;
    while (segment && segment->start < end) {
        uint32_t savedEnd = segment->savedEnd;
        if (!segment->next || segment->next->start >= end) {
            /*
             * savedEnd is only set for variables killed in the middle of the
             * loop. Make a tail segment connecting the last use with the
             * back edge.
             */
            if (segment->end >= end) {
                /* Variable known to be live after the loop finishes. */
                break;
            }
            savedEnd = end;
        }
        JS_ASSERT(savedEnd <= end);
        if (savedEnd > segment->end) {
            Lifetime *tail = cx->typeLifoAlloc().new_<Lifetime>(savedEnd, 0, segment->next);
            if (!tail) {
                js_ReportOutOfMemory(cx);
                return false;
            }
            tail->start = segment->end;

            /*
             * Clear the segment's saved end, but preserve in the tail if this
             * is the last segment in the loop and the variable is killed in an
             * outer loop before the backedge.
             */
            if (segment->savedEnd > end) {
                JS_ASSERT(savedEnd == end);
                tail->savedEnd = segment->savedEnd;
            }
            segment->savedEnd = 0;

            segment->next = tail;
            segment = tail->next;
        } else {
            JS_ASSERT(segment->savedEnd == 0);
            segment = segment->next;
        }
    }
    return true;
}

inline void
ScriptAnalysis::ensureVariable(LifetimeVariable &var, unsigned until)
{
    JS_ASSERT(var.lifetime);

    /*
     * If we are already ensured, the current range we are trying to ensure
     * should already be included.
     */
    if (var.ensured) {
        JS_ASSERT(var.lifetime->start <= until);
        return;
    }

    JS_ASSERT(until < var.lifetime->start);
    var.lifetime->start = until;
    var.ensured = true;
}

/////////////////////////////////////////////////////////////////////
// SSA Analysis
/////////////////////////////////////////////////////////////////////

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

bool
ScriptAnalysis::analyzeSSA(JSContext *cx)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);
    JS_ASSERT(codeArray);
    JS_ASSERT(lifetimes);

    LifoAlloc &alloc = cx->typeLifoAlloc();
    unsigned maxDepth = script_->nslots() - script_->nfixed();

    /*
     * Current value of each variable and stack value. Empty for missing or
     * untracked entries, i.e. escaping locals and arguments.
     */
    SSAValueInfo *values = cx->pod_calloc<SSAValueInfo>(numSlots + maxDepth);
    if (!values) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    struct FreeSSAValues {
        SSAValueInfo *values;
        FreeSSAValues(SSAValueInfo *values) : values(values) {}
        ~FreeSSAValues() { js_free(values); }
    } free(values);

    SSAValueInfo *stack = values + numSlots;
    uint32_t stackDepth = 0;

    for (uint32_t slot = ArgSlot(0); slot < numSlots; slot++) {
        if (trackSlot(slot))
            values[slot].v.initInitial(slot);
    }

    /*
     * All target offsets for forward jumps we have seen (including ones whose
     * target we have advanced past). We lazily add pending entries at these
     * targets for the original value of variables modified before the branch
     * rejoins.
     */
    Vector<uint32_t> branchTargets(cx);

    /*
     * Subset of branchTargets which are exception handlers at future offsets.
     * Any new value of a variable modified before the target is reached is a
     * potential value at that target, along with the lazy original value.
     */
    Vector<uint32_t> exceptionTargets(cx);

    uint32_t offset = 0;
    while (offset < script_->length()) {
        jsbytecode *pc = script_->offsetToPC(offset);
        JSOp op = (JSOp)*pc;

        uint32_t successorOffset = offset + GetBytecodeLength(pc);

        Bytecode *code = maybeCode(pc);
        if (!code) {
            offset = successorOffset;
            continue;
        }

        if (code->exceptionEntry) {
            /* Remove from exception targets list, which reflects only future targets. */
            for (size_t i = 0; i < exceptionTargets.length(); i++) {
                if (exceptionTargets[i] == offset) {
                    exceptionTargets[i] = exceptionTargets.back();
                    exceptionTargets.popBack();
                    break;
                }
            }
        }

        if (code->stackDepth > stackDepth)
            PodZero(stack + stackDepth, code->stackDepth - stackDepth);
        stackDepth = code->stackDepth;

        if (op == JSOP_LOOPHEAD && code->loop) {
            /*
             * Make sure there is a pending value array for phi nodes at the
             * loop head. We won't be able to clear these until we reach the
             * loop's back edge.
             *
             * We need phi nodes for all variables which might be modified
             * during the loop. This ensures that in the loop body we have
             * already updated state to reflect possible changes that happen
             * before the back edge, and don't need to go back and fix things
             * up when we *do* get to the back edge. This could be made lazier.
             *
             * We don't make phi nodes for values on the stack at the head of
             * the loop. These may be popped during the loop (i.e. for ITER
             * loops), but in such cases the original value is pushed back.
             */
            Vector<SlotValue> *&pending = code->pendingValues;
            if (!pending) {
                pending = cx->new_< Vector<SlotValue> >(cx);
                if (!pending) {
                    js_ReportOutOfMemory(cx);
                    return false;
                }
            }

            /*
             * Make phi nodes and update state for slots which are already in
             * pending from previous branches to the loop head, and which are
             * modified in the body of the loop.
             */
            for (unsigned i = 0; i < pending->length(); i++) {
                SlotValue &v = (*pending)[i];
                if (v.slot < numSlots && liveness(v.slot).firstWrite(code->loop) != UINT32_MAX) {
                    if (v.value.kind() != SSAValue::PHI || v.value.phiOffset() != offset) {
                        JS_ASSERT(v.value.phiOffset() < offset);
                        SSAValue ov = v.value;
                        if (!makePhi(cx, v.slot, offset, &ov))
                            return false;
                        if (!insertPhi(cx, ov, v.value))
                            return false;
                        v.value = ov;
                    }
                }
                if (code->fallthrough || code->jumpFallthrough) {
                    if (!mergeValue(cx, offset, values[v.slot].v, &v))
                        return false;
                }
                if (!mergeBranchTarget(cx, values[v.slot], v.slot, branchTargets, offset - 1))
                    return false;
                values[v.slot].v = v.value;
            }

            /*
             * Make phi nodes for all other slots which might be modified
             * during the loop. This ensures that in the loop body we have
             * already updated state to reflect possible changes that happen
             * before the back edge, and don't need to go back and fix things
             * up when we *do* get to the back edge. This could be made lazier.
             */
            for (uint32_t slot = ArgSlot(0); slot < numSlots + stackDepth; slot++) {
                if (slot >= numSlots || !trackSlot(slot))
                    continue;
                if (liveness(slot).firstWrite(code->loop) == UINT32_MAX)
                    continue;
                if (values[slot].v.kind() == SSAValue::PHI && values[slot].v.phiOffset() == offset) {
                    /* There is already a pending entry for this slot. */
                    continue;
                }
                SSAValue ov;
                if (!makePhi(cx, slot, offset, &ov))
                    return false;
                if (code->fallthrough || code->jumpFallthrough) {
                    if (!insertPhi(cx, ov, values[slot].v))
                        return false;
                }
                if (!mergeBranchTarget(cx, values[slot], slot, branchTargets, offset - 1))
                    return false;
                values[slot].v = ov;
                if (!pending->append(SlotValue(slot, ov))) {
                    js_ReportOutOfMemory(cx);
                    return false;
                }
            }
        } else if (code->pendingValues) {
            /*
             * New values at this point from a previous jump to this bytecode.
             * If there is fallthrough from the previous instruction, merge
             * with the current state and create phi nodes where necessary,
             * otherwise replace current values with the new values.
             *
             * Catch blocks are artifically treated as having fallthrough, so
             * that values written inside the block but not subsequently
             * overwritten are picked up.
             */
            bool exception = getCode(offset).exceptionEntry;
            Vector<SlotValue> *pending = code->pendingValues;
            for (unsigned i = 0; i < pending->length(); i++) {
                SlotValue &v = (*pending)[i];
                if (code->fallthrough || code->jumpFallthrough ||
                    (exception && values[v.slot].v.kind() != SSAValue::EMPTY)) {
                    if (!mergeValue(cx, offset, values[v.slot].v, &v))
                        return false;
                }
                if (!mergeBranchTarget(cx, values[v.slot], v.slot, branchTargets, offset))
                    return false;
                values[v.slot].v = v.value;
            }
            if (!freezeNewValues(cx, offset))
                return false;
        }

        unsigned nuses = GetUseCount(script_, offset);
        unsigned ndefs = GetDefCount(script_, offset);
        JS_ASSERT(stackDepth >= nuses);

        unsigned xuses = ExtendedUse(pc) ? nuses + 1 : nuses;

        if (xuses) {
            code->poppedValues = alloc.newArray<SSAValue>(xuses);
            if (!code->poppedValues) {
                js_ReportOutOfMemory(cx);
                return false;
            }
            for (unsigned i = 0; i < nuses; i++) {
                SSAValue &v = stack[stackDepth - 1 - i].v;
                code->poppedValues[i] = v;
                v.clear();
            }
            if (xuses > nuses) {
                /*
                 * For SETLOCAL, etc. opcodes, add an extra popped value
                 * holding the value of the local before the op.
                 */
                uint32_t slot = GetBytecodeSlot(script_, pc);
                if (trackSlot(slot))
                    code->poppedValues[nuses] = values[slot].v;
                else
                    code->poppedValues[nuses].clear();
            }

            if (xuses) {
                SSAUseChain *useChains = alloc.newArray<SSAUseChain>(xuses);
                if (!useChains) {
                    js_ReportOutOfMemory(cx);
                    return false;
                }
                PodZero(useChains, xuses);
                for (unsigned i = 0; i < xuses; i++) {
                    const SSAValue &v = code->poppedValues[i];
                    if (trackUseChain(v)) {
                        SSAUseChain *&uses = useChain(v);
                        useChains[i].popped = true;
                        useChains[i].offset = offset;
                        useChains[i].u.which = i;
                        useChains[i].next = uses;
                        uses = &useChains[i];
                    }
                }
            }
        }

        stackDepth -= nuses;

        for (unsigned i = 0; i < ndefs; i++)
            stack[stackDepth + i].v.initPushed(offset, i);

        unsigned xdefs = ExtendedDef(pc) ? ndefs + 1 : ndefs;
        if (xdefs) {
            code->pushedUses = alloc.newArray<SSAUseChain *>(xdefs);
            if (!code->pushedUses) {
                js_ReportOutOfMemory(cx);
                return false;
            }
            PodZero(code->pushedUses, xdefs);
        }

        stackDepth += ndefs;

        if (op == JSOP_SETARG || op == JSOP_SETLOCAL) {
            uint32_t slot = GetBytecodeSlot(script_, pc);
            if (trackSlot(slot)) {
                if (!mergeBranchTarget(cx, values[slot], slot, branchTargets, offset))
                    return false;
                if (!mergeExceptionTarget(cx, values[slot].v, slot, exceptionTargets))
                    return false;
                values[slot].v.initWritten(slot, offset);
            }
        }

        switch (op) {
          case JSOP_GETARG:
          case JSOP_GETLOCAL: {
            uint32_t slot = GetBytecodeSlot(script_, pc);
            if (trackSlot(slot)) {
                /*
                 * Propagate the current value of the local to the pushed value,
                 * and remember it with an extended use on the opcode.
                 */
                stack[stackDepth - 1].v = code->poppedValues[0] = values[slot].v;
            }
            break;
          }

          /* Short circuit ops which push back one of their operands. */

          case JSOP_MOREITER:
            stack[stackDepth - 2].v = code->poppedValues[0];
            break;

          case JSOP_MUTATEPROTO:
          case JSOP_INITPROP:
          case JSOP_INITPROP_GETTER:
          case JSOP_INITPROP_SETTER:
            stack[stackDepth - 1].v = code->poppedValues[1];
            break;

          case JSOP_SPREAD:
          case JSOP_INITELEM_INC:
            stack[stackDepth - 2].v = code->poppedValues[2];
            break;

          case JSOP_INITELEM_ARRAY:
            stack[stackDepth - 1].v = code->poppedValues[1];
            break;

          case JSOP_INITELEM:
          case JSOP_INITELEM_GETTER:
          case JSOP_INITELEM_SETTER:
            stack[stackDepth - 1].v = code->poppedValues[2];
            break;

          case JSOP_DUP:
            stack[stackDepth - 1].v = stack[stackDepth - 2].v = code->poppedValues[0];
            break;

          case JSOP_DUP2:
            stack[stackDepth - 1].v = stack[stackDepth - 3].v = code->poppedValues[0];
            stack[stackDepth - 2].v = stack[stackDepth - 4].v = code->poppedValues[1];
            break;

          case JSOP_DUPAT: {
            unsigned pickedDepth = GET_UINT24 (pc);
            JS_ASSERT(pickedDepth < stackDepth - 1);
            stack[stackDepth - 1].v = stack[stackDepth - 2 - pickedDepth].v;
            break;
          }

          case JSOP_SWAP:
            /* Swap is like pick 1. */
          case JSOP_PICK: {
            unsigned pickedDepth = (op == JSOP_SWAP ? 1 : pc[1]);
            stack[stackDepth - 1].v = code->poppedValues[pickedDepth];
            for (unsigned i = 0; i < pickedDepth; i++)
                stack[stackDepth - 2 - i].v = code->poppedValues[i];
            break;
          }

          /*
           * Switch and try blocks preserve the stack between the original op
           * and all case statements or exception/finally handlers.
           */

          case JSOP_TABLESWITCH: {
            unsigned defaultOffset = offset + GET_JUMP_OFFSET(pc);
            jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
            int32_t low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            int32_t high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            for (int32_t i = low; i <= high; i++) {
                unsigned targetOffset = offset + GET_JUMP_OFFSET(pc2);
                if (targetOffset != offset) {
                    if (!checkBranchTarget(cx, targetOffset, branchTargets, values, stackDepth))
                        return false;
                }
                pc2 += JUMP_OFFSET_LEN;
            }

            if (!checkBranchTarget(cx, defaultOffset, branchTargets, values, stackDepth))
                return false;
            break;
          }

          case JSOP_TRY: {
            JSTryNote *tn = script_->trynotes()->vector;
            JSTryNote *tnlimit = tn + script_->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script_->mainOffset() + tn->start;
                if (startOffset == offset + 1) {
                    unsigned catchOffset = startOffset + tn->length;

                    if (tn->kind != JSTRY_ITER && tn->kind != JSTRY_LOOP) {
                        if (!checkBranchTarget(cx, catchOffset, branchTargets, values, stackDepth))
                            return false;
                        if (!checkExceptionTarget(cx, catchOffset, exceptionTargets))
                            return false;
                    }
                }
            }
            break;
          }

          case JSOP_THROW:
          case JSOP_RETURN:
          case JSOP_RETRVAL:
            if (!mergeAllExceptionTargets(cx, values, exceptionTargets))
                return false;
            break;

          default:;
        }

        if (IsJumpOpcode(op)) {
            unsigned targetOffset = FollowBranch(cx, script_, offset);
            if (!checkBranchTarget(cx, targetOffset, branchTargets, values, stackDepth))
                return false;

            /*
             * If this is a back edge, we're done with the loop and can freeze
             * the phi values at the head now.
             */
            if (targetOffset < offset) {
                if (!freezeNewValues(cx, targetOffset))
                    return false;
            }
        }

        offset = successorOffset;
    }

    return true;
}

/* Get a phi node's capacity for a given length. */
static inline unsigned
PhiNodeCapacity(unsigned length)
{
    if (length <= 4)
        return 4;

    return 1 << (FloorLog2(length - 1) + 1);
}

bool
ScriptAnalysis::makePhi(JSContext *cx, uint32_t slot, uint32_t offset, SSAValue *pv)
{
    SSAPhiNode *node = cx->typeLifoAlloc().new_<SSAPhiNode>();
    SSAValue *options = cx->typeLifoAlloc().newArray<SSAValue>(PhiNodeCapacity(0));
    if (!node || !options) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    node->slot = slot;
    node->options = options;
    pv->initPhi(offset, node);
    return true;
}

bool
ScriptAnalysis::insertPhi(JSContext *cx, SSAValue &phi, const SSAValue &v)
{
    JS_ASSERT(phi.kind() == SSAValue::PHI);
    SSAPhiNode *node = phi.phiNode();

    /*
     * Filter dupes inserted into small nodes to keep things clean and avoid
     * extra type constraints, but don't bother on large phi nodes to avoid
     * quadratic behavior.
     */
    if (node->length <= 8) {
        for (unsigned i = 0; i < node->length; i++) {
            if (v == node->options[i])
                return true;
        }
    }

    if (trackUseChain(v)) {
        SSAUseChain *&uses = useChain(v);

        SSAUseChain *use = cx->typeLifoAlloc().new_<SSAUseChain>();
        if (!use) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        use->popped = false;
        use->offset = phi.phiOffset();
        use->u.phi = node;
        use->next = uses;
        uses = use;
    }

    if (node->length < PhiNodeCapacity(node->length)) {
        node->options[node->length++] = v;
        return true;
    }

    SSAValue *newOptions =
        cx->typeLifoAlloc().newArray<SSAValue>(PhiNodeCapacity(node->length + 1));
    if (!newOptions) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    PodCopy(newOptions, node->options, node->length);
    node->options = newOptions;
    node->options[node->length++] = v;

    return true;
}

inline bool
ScriptAnalysis::mergeValue(JSContext *cx, uint32_t offset, const SSAValue &v, SlotValue *pv)
{
    /* Make sure that v is accounted for in the pending value or phi value at pv. */
    JS_ASSERT(v.kind() != SSAValue::EMPTY && pv->value.kind() != SSAValue::EMPTY);

    if (v == pv->value)
        return true;

    if (pv->value.kind() != SSAValue::PHI || pv->value.phiOffset() < offset) {
        SSAValue ov = pv->value;
        return makePhi(cx, pv->slot, offset, &pv->value)
            && insertPhi(cx, pv->value, v)
            && insertPhi(cx, pv->value, ov);
    }

    JS_ASSERT(pv->value.phiOffset() == offset);
    return insertPhi(cx, pv->value, v);
}

bool
ScriptAnalysis::checkPendingValue(JSContext *cx, const SSAValue &v, uint32_t slot,
                                  Vector<SlotValue> *pending)
{
    JS_ASSERT(v.kind() != SSAValue::EMPTY);

    for (unsigned i = 0; i < pending->length(); i++) {
        if ((*pending)[i].slot == slot)
            return true;
    }

    if (!pending->append(SlotValue(slot, v))) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

bool
ScriptAnalysis::checkBranchTarget(JSContext *cx, uint32_t targetOffset,
                                  Vector<uint32_t> &branchTargets,
                                  SSAValueInfo *values, uint32_t stackDepth)
{
    unsigned targetDepth = getCode(targetOffset).stackDepth;
    JS_ASSERT(targetDepth <= stackDepth);

    /*
     * If there is already an active branch to target, make sure its pending
     * values reflect any changes made since the first branch. Otherwise, add a
     * new pending branch and determine its pending values lazily.
     */
    Vector<SlotValue> *&pending = getCode(targetOffset).pendingValues;
    if (pending) {
        for (unsigned i = 0; i < pending->length(); i++) {
            SlotValue &v = (*pending)[i];
            if (!mergeValue(cx, targetOffset, values[v.slot].v, &v))
                return false;
        }
    } else {
        pending = cx->new_< Vector<SlotValue> >(cx);
        if (!pending || !branchTargets.append(targetOffset)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    /*
     * Make sure there is a pending entry for each value on the stack.
     * The number of stack entries at join points is usually zero, and
     * we don't want to look at the active branches while popping and
     * pushing values in each opcode.
     */
    for (unsigned i = 0; i < targetDepth; i++) {
        uint32_t slot = StackSlot(script_, i);
        if (!checkPendingValue(cx, values[slot].v, slot, pending))
            return false;
    }

    return true;
}

bool
ScriptAnalysis::checkExceptionTarget(JSContext *cx, uint32_t catchOffset,
                                     Vector<uint32_t> &exceptionTargets)
{
    JS_ASSERT(getCode(catchOffset).exceptionEntry);

    /*
     * The catch offset will already be in the branch targets, just check
     * whether this is already a known exception target.
     */
    for (unsigned i = 0; i < exceptionTargets.length(); i++) {
        if (exceptionTargets[i] == catchOffset)
            return true;
    }
    if (!exceptionTargets.append(catchOffset)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

bool
ScriptAnalysis::mergeBranchTarget(JSContext *cx, SSAValueInfo &value, uint32_t slot,
                                  const Vector<uint32_t> &branchTargets, uint32_t currentOffset)
{
    if (slot >= numSlots) {
        /*
         * There is no need to lazily check that there are pending values at
         * branch targets for slots on the stack, these are added to pending
         * eagerly.
         */
        return true;
    }

    JS_ASSERT(trackSlot(slot));

    /*
     * Before changing the value of a variable, make sure the old value is
     * marked at the target of any branches jumping over the current opcode.
     * Only look at new branch targets which have appeared since the last time
     * the variable was written.
     */
    for (int i = branchTargets.length() - 1; i >= value.branchSize; i--) {
        if (branchTargets[i] <= currentOffset)
            continue;

        const Bytecode &code = getCode(branchTargets[i]);

        Vector<SlotValue> *pending = code.pendingValues;
        if (!checkPendingValue(cx, value.v, slot, pending))
            return false;
    }

    value.branchSize = branchTargets.length();
    return true;
}

bool
ScriptAnalysis::mergeExceptionTarget(JSContext *cx, const SSAValue &value, uint32_t slot,
                                     const Vector<uint32_t> &exceptionTargets)
{
    JS_ASSERT(trackSlot(slot));

    /*
     * Update the value at exception targets with the value of a variable
     * before it is overwritten. Unlike mergeBranchTarget, this is done whether
     * or not the overwritten value is the value of the variable at the
     * original branch. Values for a variable which are written after the
     * try block starts and overwritten before it is finished can still be
     * seen at exception handlers via exception paths.
     */
    for (unsigned i = 0; i < exceptionTargets.length(); i++) {
        unsigned offset = exceptionTargets[i];
        Vector<SlotValue> *pending = getCode(offset).pendingValues;

        bool duplicate = false;
        for (unsigned i = 0; i < pending->length(); i++) {
            if ((*pending)[i].slot == slot) {
                duplicate = true;
                SlotValue &v = (*pending)[i];
                if (!mergeValue(cx, offset, value, &v))
                    return false;
                break;
            }
        }

        if (!duplicate && !pending->append(SlotValue(slot, value))) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    return true;
}

bool
ScriptAnalysis::mergeAllExceptionTargets(JSContext *cx, SSAValueInfo *values,
                                         const Vector<uint32_t> &exceptionTargets)
{
    for (unsigned i = 0; i < exceptionTargets.length(); i++) {
        Vector<SlotValue> *pending = getCode(exceptionTargets[i]).pendingValues;
        for (unsigned i = 0; i < pending->length(); i++) {
            const SlotValue &v = (*pending)[i];
            if (trackSlot(v.slot)) {
                if (!mergeExceptionTarget(cx, values[v.slot].v, v.slot, exceptionTargets))
                    return false;
            }
        }
    }
    return true;
}

bool
ScriptAnalysis::freezeNewValues(JSContext *cx, uint32_t offset)
{
    Bytecode &code = getCode(offset);

    Vector<SlotValue> *pending = code.pendingValues;
    code.pendingValues = nullptr;

    unsigned count = pending->length();
    if (count == 0) {
        js_delete(pending);
        return true;
    }

    code.newValues = cx->typeLifoAlloc().newArray<SlotValue>(count + 1);
    if (!code.newValues) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    for (unsigned i = 0; i < count; i++)
        code.newValues[i] = (*pending)[i];
    code.newValues[count].slot = 0;
    code.newValues[count].value.clear();

    js_delete(pending);
    return true;
}

bool
ScriptAnalysis::needsArgsObj(JSContext *cx, SeenVector &seen, const SSAValue &v)
{
    /*
     * trackUseChain is false for initial values of variables, which
     * cannot hold the script's arguments object.
     */
    if (!trackUseChain(v))
        return false;

    for (unsigned i = 0; i < seen.length(); i++) {
        if (v == seen[i])
            return false;
    }
    if (!seen.append(v)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return true;
    }

    SSAUseChain *use = useChain(v);
    while (use) {
        if (needsArgsObj(cx, seen, use))
            return true;
        use = use->next;
    }

    return false;
}

bool
ScriptAnalysis::needsArgsObj(JSContext *cx, SeenVector &seen, SSAUseChain *use)
{
    if (!use->popped)
        return needsArgsObj(cx, seen, SSAValue::PhiValue(use->offset, use->u.phi));

    jsbytecode *pc = script_->offsetToPC(use->offset);
    JSOp op = JSOp(*pc);

    if (op == JSOP_POP || op == JSOP_POPN)
        return false;

    /* We can read the frame's arguments directly for f.apply(x, arguments). */
    if (op == JSOP_FUNAPPLY && GET_ARGC(pc) == 2 && use->u.which == 0) {
        argumentsContentsObserved_ = true;
        return false;
    }

    /* arguments[i] can read fp->canonicalActualArg(i) directly. */
    if (op == JSOP_GETELEM && use->u.which == 1) {
        argumentsContentsObserved_ = true;
        return false;
    }

    /* arguments.length length can read fp->numActualArgs() directly. */
    if (op == JSOP_LENGTH)
        return false;

    /* Allow assignments to non-closed locals (but not arguments). */

    if (op == JSOP_SETLOCAL) {
        uint32_t slot = GetBytecodeSlot(script_, pc);
        if (!trackSlot(slot))
            return true;
        return needsArgsObj(cx, seen, SSAValue::PushedValue(use->offset, 0)) ||
               needsArgsObj(cx, seen, SSAValue::WrittenVar(slot, use->offset));
    }

    if (op == JSOP_GETLOCAL)
        return needsArgsObj(cx, seen, SSAValue::PushedValue(use->offset, 0));

    return true;
}

bool
ScriptAnalysis::needsArgsObj(JSContext *cx)
{
    JS_ASSERT(script_->argumentsHasVarBinding());

    /*
     * Always construct arguments objects when in debug mode and for generator
     * scripts (generators can be suspended when speculation fails).
     *
     * FIXME: Don't build arguments for ES6 generator expressions.
     */
    if (cx->compartment()->debugMode() || script_->isGenerator())
        return true;

    /*
     * If the script has dynamic name accesses which could reach 'arguments',
     * the parser will already have checked to ensure there are no explicit
     * uses of 'arguments' in the function. If there are such uses, the script
     * will be marked as definitely needing an arguments object.
     *
     * New accesses on 'arguments' can occur through 'eval' or the debugger
     * statement. In the former case, we will dynamically detect the use and
     * mark the arguments optimization as having failed.
     */
    if (script_->bindingsAccessedDynamically())
        return false;

    /*
     * Since let variables and are not tracked, we cannot soundly perform this
     * analysis in their presence.
     */
    if (localsAliasStack())
        return true;

    unsigned pcOff = script_->pcToOffset(script_->argumentsBytecode());

    SeenVector seen(cx);
    if (needsArgsObj(cx, seen, SSAValue::PushedValue(pcOff, 0)))
        return true;

    /*
     * If a script explicitly accesses the contents of 'arguments', and has
     * formals which may be stored as part of a call object, don't use lazy
     * arguments. The compiler can then assume that accesses through
     * arguments[i] will be on unaliased variables.
     */
    if (script_->funHasAnyAliasedFormal() && argumentsContentsObserved_)
        return true;

    return false;
}

#ifdef DEBUG

void
ScriptAnalysis::printSSA(JSContext *cx)
{
    types::AutoEnterAnalysis enter(cx);

    printf("\n");

    RootedScript script(cx, script_);
    for (unsigned offset = 0; offset < script_->length(); offset++) {
        Bytecode *code = maybeCode(offset);
        if (!code)
            continue;

        jsbytecode *pc = script_->offsetToPC(offset);

        PrintBytecode(cx, script, pc);

        SlotValue *newv = code->newValues;
        if (newv) {
            while (newv->slot) {
                if (newv->value.kind() != SSAValue::PHI || newv->value.phiOffset() != offset) {
                    newv++;
                    continue;
                }
                printf("  phi ");
                newv->value.print();
                printf(" [");
                for (unsigned i = 0; i < newv->value.phiLength(); i++) {
                    if (i)
                        printf(",");
                    newv->value.phiValue(i).print();
                }
                printf("]\n");
                newv++;
            }
        }

        unsigned nuses = GetUseCount(script_, offset);
        unsigned xuses = ExtendedUse(pc) ? nuses + 1 : nuses;

        for (unsigned i = 0; i < xuses; i++) {
            printf("  popped%d: ", i);
            code->poppedValues[i].print();
            printf("\n");
        }
    }

    printf("\n");
}

void
SSAValue::print() const
{
    switch (kind()) {

      case EMPTY:
        printf("empty");
        break;

      case PUSHED:
        printf("pushed:%05u#%u", pushedOffset(), pushedIndex());
        break;

      case VAR:
        if (varInitial())
            printf("initial:%u", varSlot());
        else
            printf("write:%05u", varOffset());
        break;

      case PHI:
        printf("phi:%05u#%u", phiOffset(), phiSlot());
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Bad kind");
    }
}

void
ScriptAnalysis::assertMatchingDebugMode()
{
    JS_ASSERT(!!script_->compartment()->debugMode() == !!originalDebugMode_);
}

void
ScriptAnalysis::assertMatchingStackDepthAtOffset(uint32_t offset, uint32_t stackDepth) {
    JS_ASSERT_IF(maybeCode(offset), getCode(offset).stackDepth == stackDepth);
}

#endif  /* DEBUG */

} // namespace analyze
} // namespace js
