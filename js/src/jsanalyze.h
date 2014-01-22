/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Definitions for javascript analysis. */

#ifndef jsanalyze_h
#define jsanalyze_h

#include "jscompartment.h"

namespace js {
namespace analyze {

class Bytecode;
struct LifetimeVariable;
class SlotValue;
class SSAValue;
struct SSAValueInfo;
class SSAUseChain;

// Common representation of slots between ScriptAnalysis, TypeScript, and in the
// case of TotalSlots, Ion.
static inline uint32_t ThisSlot() {
    return 0;
}
static inline uint32_t ArgSlot(uint32_t arg) {
    return 1 + arg;
}
static inline uint32_t LocalSlot(JSScript *script, uint32_t local) {
    return 1 + local +
           (script->functionNonDelazifying() ? script->functionNonDelazifying()->nargs() : 0);
}
static inline uint32_t TotalSlots(JSScript *script) {
    return LocalSlot(script, 0) + script->nfixed();
}

// Analysis information about a script.  FIXME: At this point, the entire
// purpose of this class is to compute JSScript::needsArgsObj, and to support
// isReachable() in order for jsinfer.cpp:FindPreviousInnerInitializer to get
// the previous opcode.  For that purpose, it is completely overkill.
class ScriptAnalysis
{
    friend class Bytecode;

    JSScript *script_;

    Bytecode **codeArray;

    uint32_t numSlots;

    bool *escapedSlots;

#ifdef DEBUG
    /* Whether the compartment was in debug mode when we performed the analysis. */
    bool originalDebugMode_: 1;
#endif

    /* --------- Bytecode analysis --------- */

    bool localsAliasStack_:1;
    bool canTrackVars:1;
    bool argumentsContentsObserved_:1;

    /* --------- Lifetime analysis --------- */

    LifetimeVariable *lifetimes;

  public:
    ScriptAnalysis(JSScript *script) {
        mozilla::PodZero(this);
        this->script_ = script;
#ifdef DEBUG
        this->originalDebugMode_ = script_->compartment()->debugMode();
#endif
    }

    JS_WARN_UNUSED_RESULT
    bool analyzeBytecode(JSContext *cx);

    /*
     * True if there are any LOCAL opcodes aliasing values on the stack (above
     * script_->nfixed).
     */
    bool localsAliasStack() { return localsAliasStack_; }

    bool isReachable(const jsbytecode *pc) { return maybeCode(pc); }

  private:
    JS_WARN_UNUSED_RESULT
    bool analyzeSSA(JSContext *cx);
    JS_WARN_UNUSED_RESULT
    bool analyzeLifetimes(JSContext *cx);

    /* Accessors for bytecode information. */
    Bytecode& getCode(uint32_t offset) {
        JS_ASSERT(offset < script_->length());
        JS_ASSERT(codeArray[offset]);
        return *codeArray[offset];
    }
    Bytecode& getCode(const jsbytecode *pc) { return getCode(script_->pcToOffset(pc)); }

    Bytecode* maybeCode(uint32_t offset) {
        JS_ASSERT(offset < script_->length());
        return codeArray[offset];
    }
    Bytecode* maybeCode(const jsbytecode *pc) { return maybeCode(script_->pcToOffset(pc)); }

    inline bool jumpTarget(uint32_t offset);
    inline bool jumpTarget(const jsbytecode *pc);

    inline const SSAValue &poppedValue(uint32_t offset, uint32_t which);
    inline const SSAValue &poppedValue(const jsbytecode *pc, uint32_t which);

    inline const SlotValue *newValues(uint32_t offset);
    inline const SlotValue *newValues(const jsbytecode *pc);

    inline bool trackUseChain(const SSAValue &v);

    /*
     * Get the use chain for an SSA value. May be invalid for some opcodes in
     * scripts where localsAliasStack(). You have been warned!
     */
    inline SSAUseChain *& useChain(const SSAValue &v);


    /* For a JSOP_CALL* op, get the pc of the corresponding JSOP_CALL/NEW/etc. */
    inline jsbytecode *getCallPC(jsbytecode *pc);

    /* Accessors for local variable information. */

    /*
     * Escaping slots include all slots that can be accessed in ways other than
     * through the corresponding LOCAL/ARG opcode. This includes all closed
     * slots in the script, all slots in scripts which use eval or are in debug
     * mode, and slots which are aliased by NAME or similar opcodes in the
     * containing script (which does not imply the variable is closed).
     */
    inline bool slotEscapes(uint32_t slot);

    /*
     * Whether we distinguish different writes of this variable while doing
     * SSA analysis. Escaping locals can be written in other scripts, and the
     * presence of NAME opcodes which could alias local variables or arguments
     * keeps us from tracking variable values at each point.
     */
    inline bool trackSlot(uint32_t slot);

    inline const LifetimeVariable & liveness(uint32_t slot);

    void printSSA(JSContext *cx);
    void printTypes(JSContext *cx);

    /* Bytecode helpers */
    JS_WARN_UNUSED_RESULT
    inline bool addJump(JSContext *cx, unsigned offset,
                        unsigned *currentOffset, unsigned *forwardJump, unsigned *forwardLoop,
                        unsigned stackDepth);

    /* Lifetime helpers */
    JS_WARN_UNUSED_RESULT
    inline bool addVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                            LifetimeVariable **&saved, unsigned &savedCount);
    JS_WARN_UNUSED_RESULT
    inline bool killVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                             LifetimeVariable **&saved, unsigned &savedCount);
    JS_WARN_UNUSED_RESULT
    inline bool extendVariable(JSContext *cx, LifetimeVariable &var, unsigned start, unsigned end);

    inline void ensureVariable(LifetimeVariable &var, unsigned until);

    /* SSA helpers */
    JS_WARN_UNUSED_RESULT
    bool makePhi(JSContext *cx, uint32_t slot, uint32_t offset, SSAValue *pv);
    JS_WARN_UNUSED_RESULT
    bool insertPhi(JSContext *cx, SSAValue &phi, const SSAValue &v);
    JS_WARN_UNUSED_RESULT
    bool mergeValue(JSContext *cx, uint32_t offset, const SSAValue &v, SlotValue *pv);
    JS_WARN_UNUSED_RESULT
    bool checkPendingValue(JSContext *cx, const SSAValue &v, uint32_t slot,
                           Vector<SlotValue> *pending);
    JS_WARN_UNUSED_RESULT
    bool checkBranchTarget(JSContext *cx, uint32_t targetOffset, Vector<uint32_t> &branchTargets,
                           SSAValueInfo *values, uint32_t stackDepth);
    JS_WARN_UNUSED_RESULT
    bool checkExceptionTarget(JSContext *cx, uint32_t catchOffset,
                              Vector<uint32_t> &exceptionTargets);
    JS_WARN_UNUSED_RESULT
    bool mergeBranchTarget(JSContext *cx, SSAValueInfo &value, uint32_t slot,
                           const Vector<uint32_t> &branchTargets, uint32_t currentOffset);
    JS_WARN_UNUSED_RESULT
    bool mergeExceptionTarget(JSContext *cx, const SSAValue &value, uint32_t slot,
                              const Vector<uint32_t> &exceptionTargets);
    JS_WARN_UNUSED_RESULT
    bool mergeAllExceptionTargets(JSContext *cx, SSAValueInfo *values,
                                  const Vector<uint32_t> &exceptionTargets);
    JS_WARN_UNUSED_RESULT
    bool freezeNewValues(JSContext *cx, uint32_t offset);

    typedef Vector<SSAValue, 16> SeenVector;
    bool needsArgsObj(JSContext *cx, SeenVector &seen, const SSAValue &v);
    bool needsArgsObj(JSContext *cx, SeenVector &seen, SSAUseChain *use);
    bool needsArgsObj(JSContext *cx);

  public:
#ifdef DEBUG
    void assertMatchingDebugMode();
    void assertMatchingStackDepthAtOffset(uint32_t offset, uint32_t stackDepth);
#else
    void assertMatchingDebugMode() { }
    void assertMatchingStackDepthAtOffset(uint32_t offset, uint32_t stackDepth) { }
#endif
};

#ifdef DEBUG
void PrintBytecode(JSContext *cx, HandleScript script, jsbytecode *pc);
#endif

} /* namespace analyze */
} /* namespace js */

#endif /* jsanalyze_h */
