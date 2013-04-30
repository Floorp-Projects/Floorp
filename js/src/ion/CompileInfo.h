/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_compileinfo_h__
#define jsion_compileinfo_h__

#include "Registers.h"

namespace js {
namespace ion {

inline unsigned
StartArgSlot(JSScript *script, JSFunction *fun)
{
    // First slot is for scope chain.
    // Second one may be for arguments object.
    return 1 + (script->argumentsHasVarBinding() ? 1 : 0);
}

inline unsigned
CountArgSlots(JSScript *script, JSFunction *fun)
{
    return StartArgSlot(script, fun) + (fun ? fun->nargs + 1 : 0);
}

enum ExecutionMode {
    // Normal JavaScript execution
    SequentialExecution = 0,

    // JavaScript code to be executed in parallel worker threads,
    // e.g. by ParallelArray
    ParallelExecution
};

// Contains information about the compilation source for IR being generated.
class CompileInfo
{
  public:
    CompileInfo(JSScript *script, JSFunction *fun, jsbytecode *osrPc, bool constructing,
                ExecutionMode executionMode)
      : script_(script), fun_(fun), osrPc_(osrPc), constructing_(constructing),
        executionMode_(executionMode)
    {
        JS_ASSERT_IF(osrPc, JSOp(*osrPc) == JSOP_LOOPENTRY);
        nimplicit_ = StartArgSlot(script, fun)              /* scope chain and argument obj */
                   + (fun ? 1 : 0);                         /* this */
        nargs_ = fun ? fun->nargs : 0;
        nlocals_ = script->nfixed;
        nstack_ = script->nslots - script->nfixed;
        nslots_ = nimplicit_ + nargs_ + nlocals_ + nstack_;
    }

    CompileInfo(unsigned nlocals)
      : script_(NULL), fun_(NULL), osrPc_(NULL), constructing_(false)
    {
        nimplicit_ = 0;
        nargs_ = 0;
        nlocals_ = nlocals;
        nstack_ = 1;  /* For FunctionCompiler::pushPhiInput/popPhiOutput */
        nslots_ = nlocals_ + nstack_;
    }

    JSScript *script() const {
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

    inline const char *filename() const;

    unsigned lineno() const {
        return script_->lineno;
    }
    unsigned lineno(JSContext *cx, jsbytecode *pc) const {
        return PCToLineNumber(script_, pc);
    }

    // Script accessors based on PC.

    inline JSAtom *getAtom(jsbytecode *pc) const;
    inline PropertyName *getName(jsbytecode *pc) const;
    inline RegExpObject *getRegExp(jsbytecode *pc) const;
    inline JSObject *getObject(jsbytecode *pc) const;
    inline JSFunction *getFunction(jsbytecode *pc) const;
    inline const Value &getConst(jsbytecode *pc) const;
    inline jssrcnote *getNote(JSContext *cx, jsbytecode *pc) const;

    // Total number of slots: args, locals, and stack.
    unsigned nslots() const {
        return nslots_;
    }

    unsigned nargs() const {
        return nargs_;
    }
    unsigned nlocals() const {
        return nlocals_;
    }
    unsigned ninvoke() const {
        return nslots_ - nstack_;
    }

    uint32_t scopeChainSlot() const {
        JS_ASSERT(script());
        return 0;
    }
    uint32_t argsObjSlot() const {
        JS_ASSERT(hasArguments());
        return 1;
    }
    uint32_t thisSlot() const {
        JS_ASSERT(fun());
        return hasArguments() ? 2 : 1;
    }
    uint32_t firstArgSlot() const {
        return nimplicit_;
    }
    uint32_t argSlotUnchecked(uint32_t i) const {
        // During initialization, some routines need to get at arg
        // slots regardless of how regular argument access is done.
        JS_ASSERT(i < nargs_);
        return nimplicit_ + i;
    }
    uint32_t argSlot(uint32_t i) const {
        // This should only be accessed when compiling functions for
        // which argument accesses don't need to go through the
        // argument object.
        JS_ASSERT(!argsObjAliasesFormals());
        return argSlotUnchecked(i);
    }
    uint32_t firstLocalSlot() const {
        return nimplicit_ + nargs_;
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

    uint32_t startArgSlot() const {
        JS_ASSERT(scopeChainSlot() == 0);
        return StartArgSlot(script(), fun());
    }
    uint32_t endArgSlot() const {
        JS_ASSERT(scopeChainSlot() == 0);
        return CountArgSlots(script(), fun());
    }

    uint32_t totalSlots() const {
        return 2 + (hasArguments() ? 1 : 0) + nargs() + nlocals();
    }

    bool hasArguments() const {
        return script()->argumentsHasVarBinding();
    }
    bool needsArgsObj() const {
        return script()->needsArgsObj();
    }
    bool argsObjAliasesFormals() const {
        return script()->argsObjAliasesFormals();
    }

    ExecutionMode executionMode() const {
        return executionMode_;
    }

    bool isParallelExecution() const {
        return executionMode_ == ParallelExecution;
    }

  private:
    unsigned nimplicit_;
    unsigned nargs_;
    unsigned nlocals_;
    unsigned nstack_;
    unsigned nslots_;
    JSScript *script_;
    JSFunction *fun_;
    jsbytecode *osrPc_;
    bool constructing_;
    ExecutionMode executionMode_;
};

} // namespace ion
} // namespace js

#endif
