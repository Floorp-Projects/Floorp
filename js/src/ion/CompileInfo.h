/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_compileinfo_h__
#define jsion_compileinfo_h__

namespace js {
namespace ion {

inline unsigned
CountArgSlots(JSFunction *fun)
{
    return fun ? fun->nargs + 2 : 1; // +2 for |scopeChain| and |this|, or +1 for |scopeChain|
}

enum ExecutionMode {
    // Normal JavaScript execution
    SequentialExecution,

    // JavaScript code to be executed in parallel worker threads,
    // e.g. by ParallelArray
    ParallelExecution
};

// Contains information about the compilation source for IR being generated.
class CompileInfo
{
  public:
    CompileInfo(UnrootedScript script, JSFunction *fun, jsbytecode *osrPc, bool constructing,
                ExecutionMode executionMode)
      : script_(script), fun_(fun), osrPc_(osrPc), constructing_(constructing),
        executionMode_(executionMode)
    {
        JS_ASSERT_IF(osrPc, JSOp(*osrPc) == JSOP_LOOPENTRY);
        nslots_ = script->nslots + CountArgSlots(fun);
    }

    UnrootedScript script() const {
        return script_;
    }
    JSFunction *fun() const {
        return fun_;
    }
    bool constructing() const {
        return constructing_;
    }
    jsbytecode *osrPc() {
        return osrPc_;
    }

    bool hasOsrAt(jsbytecode *pc) {
        JS_ASSERT(JSOp(*pc) == JSOP_LOOPENTRY);
        return pc == osrPc();
    }

    jsbytecode *startPC() const {
        return script_->code;
    }
    jsbytecode *limitPC() const {
        return script_->code + script_->length;
    }

    const char *filename() const {
        return script_->filename;
    }
    unsigned lineno() const {
        return script_->lineno;
    }
    unsigned lineno(JSContext *cx, jsbytecode *pc) const {
        return PCToLineNumber(script_, pc);
    }

    // Script accessors based on PC.

    JSAtom *getAtom(jsbytecode *pc) const {
        return script_->getAtom(GET_UINT32_INDEX(pc));
    }
    PropertyName *getName(jsbytecode *pc) const {
        return script_->getName(GET_UINT32_INDEX(pc));
    }
    RegExpObject *getRegExp(jsbytecode *pc) const {
        return script_->getRegExp(GET_UINT32_INDEX(pc));
    }
    JSObject *getObject(jsbytecode *pc) const {
        return script_->getObject(GET_UINT32_INDEX(pc));
    }
    JSFunction *getFunction(jsbytecode *pc) const {
        return script_->getFunction(GET_UINT32_INDEX(pc));
    }
    const Value &getConst(jsbytecode *pc) const {
        return script_->getConst(GET_UINT32_INDEX(pc));
    }
    jssrcnote *getNote(JSContext *cx, jsbytecode *pc) const {
        return js_GetSrcNote(cx, script(), pc);
    }

    // Total number of slots: args, locals, and stack.
    unsigned nslots() const {
        return nslots_;
    }

    unsigned nargs() const {
        return fun()->nargs;
    }
    unsigned nlocals() const {
        return script()->nfixed;
    }
    unsigned ninvoke() const {
        return nlocals() + CountArgSlots(fun());
    }

    uint32_t scopeChainSlot() const {
        return 0;
    }
    uint32_t thisSlot() const {
        JS_ASSERT(fun());
        return 1;
    }
    uint32_t firstArgSlot() const {
        JS_ASSERT(fun());
        return 2;
    }
    uint32_t argSlot(uint32_t i) const {
        return firstArgSlot() + i;
    }
    uint32_t firstLocalSlot() const {
        return CountArgSlots(fun());
    }
    uint32_t localSlot(uint32_t i) const {
        return firstLocalSlot() + i;
    }
    uint32_t firstStackSlot() const {
        return firstLocalSlot() + nlocals();
    }
    uint32_t stackSlot(uint32_t i) const {
        return firstStackSlot() + i;
    }

    bool hasArguments() {
        return script()->argumentsHasVarBinding();
    }

    ExecutionMode executionMode() const {
        return executionMode_;
    }

    bool isParallelExecution() const {
        return executionMode_ == ParallelExecution;
    }

  private:
    JSScript *script_;
    JSFunction *fun_;
    unsigned nslots_;
    jsbytecode *osrPc_;
    bool constructing_;
    ExecutionMode executionMode_;
};

} // namespace ion
} // namespace js

#endif
