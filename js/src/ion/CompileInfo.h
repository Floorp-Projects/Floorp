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

// Contains information about the compilation source for IR being generated.
class CompileInfo
{
  public:
    CompileInfo(JSScript *script, JSFunction *fun, jsbytecode *osrPc, bool constructing)
      : script_(script), fun_(fun), osrPc_(osrPc), constructing_(constructing)
    {
        JS_ASSERT_IF(osrPc, JSOp(*osrPc) == JSOP_LOOPENTRY);
        nslots_ = script->nslots + CountArgSlots(fun);
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
        return js_GetSrcNoteCached(cx, script(), pc);
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

    uint32 scopeChainSlot() const {
        return 0;
    }
    uint32 thisSlot() const {
        JS_ASSERT(fun());
        return 1;
    }
    uint32 firstArgSlot() const {
        JS_ASSERT(fun());
        return 2;
    }
    uint32 argSlot(uint32 i) const {
        return firstArgSlot() + i;
    }
    uint32 firstLocalSlot() const {
        return CountArgSlots(fun());
    }
    uint32 localSlot(uint32 i) const {
        return firstLocalSlot() + i;
    }
    uint32 firstStackSlot() const {
        return firstLocalSlot() + nlocals();
    }
    uint32 stackSlot(uint32 i) const {
        return firstStackSlot() + i;
    }

    bool hasArguments() {
        return script()->argumentsHasVarBinding();
    }

  private:
    JSScript *script_;
    JSFunction *fun_;
    unsigned nslots_;
    jsbytecode *osrPc_;
    bool constructing_;
};

} // namespace ion
} // namespace js

#endif
