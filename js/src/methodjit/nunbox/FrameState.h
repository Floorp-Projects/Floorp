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

#if !defined jsjaeger_framestate_h__ && defined JS_METHODJIT
#define jsjaeger_framestate_h__

#include "jsapi.h"
#include "methodjit/MachineRegs.h"
#include "methodjit/nunbox/Assembler.h"
#include "methodjit/nunbox/FrameEntry.h"

namespace js {
namespace mjit {

enum TypeInfo {
    Type_Unknown
};

struct RegState {
    enum ValuePart {
        Part_Type,
        Part_Data
    };

    RegState()
      : tracked(false)
    { }

    RegState(ValuePart part, uint32 index, bool spillable)
      : index(index), part(part), spillable(spillable), tracked(true)
    { }

    uint16 index   : 16;
    ValuePart part : 1;
    bool spillable : 1;
    bool tracked   : 1;
};

struct RegSnapshot {
    RegState regs[JSC::MacroAssembler::TotalRegisters];
    Registers alloc;
};

class FrameState
{
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::Imm32 Imm32;
    typedef JSC::MacroAssembler MacroAssembler;

  public:
    FrameState(JSContext *cx, JSScript *script, Assembler &masm)
      : cx(cx), script(script), masm(masm), base(NULL)
    { }
    ~FrameState();

  public:
    bool init(uint32 nargs);

    RegisterID allocReg() {
        if (!regalloc.anyRegsFree())
            evictSomething();
        RegisterID reg = regalloc.allocReg();
        regstate[reg].tracked = false;
        return reg;
    }

    void freeReg(RegisterID reg) {
        JS_ASSERT(!regstate[reg].tracked);
        regalloc.freeReg(reg);
    }

    void incSp() {
        sp++;
    }

    void decSp() {
        sp--;
    }

    /*
     * Push a type register, unsycned, with unknown payload.
     */
    void pushUnknownType(RegisterID reg) {
        sp[0].type.setRegister(reg);
        sp[0].data.setMemory();
        sp[0].copies = 0;
        regstate[reg] = RegState(RegState::Part_Type, uint32(sp - base), true);
        incSp();
    }

    void push(const Value &v) {
        push(Jsvalify(v));
    }

    void push(const jsval &v) {
        sp[0].setConstant(v);
        sp[0].copies = 0;
        incSp();
        JS_ASSERT(sp - locals <= script->nslots);
    }

    void pushObject(RegisterID reg) {
        sp[0].type.setConstant();
        sp[0].v_.s.mask32 = JSVAL_MASK32_NONFUNOBJ;
        sp[0].data.setRegister(reg);
        sp[0].copies = 0;
        regstate[reg] = RegState(RegState::Part_Data, uint32(sp - base), true);
        incSp();
    }

    FrameEntry *peek(int32 depth) {
        JS_ASSERT(depth < 0);
        JS_ASSERT(sp + depth >= locals + script->nfixed);
        return &sp[depth];
    }

    void pop() {
        FrameEntry *vi = peek(-1);
        if (!vi->isConstant()) {
            if (vi->type.inRegister())
                regalloc.freeReg(vi->type.reg());
            if (vi->data.inRegister())
                regalloc.freeReg(vi->data.reg());
        }
        decSp();
    }

    Address topOfStack() {
        return Address(Assembler::FpReg, sizeof(JSStackFrame) +
                       (script->nfixed + stackDepth()) * sizeof(Value));
    }

    uint32 stackDepth() {
        return sp - (locals + script->nfixed);
    }

    RegisterID tempRegForType(FrameEntry *fe) {
        JS_ASSERT(!fe->type.isConstant());
        if (fe->type.inRegister())
            return fe->type.reg();
        JS_NOT_REACHED("wat");
    }

    Address addressOf(FrameEntry *fe) const {
        JS_ASSERT(fe >= locals);
        if (fe >= locals) {
            return Address(Assembler::FpReg, sizeof(JSStackFrame) +
                           (fe - locals) * sizeof(Value));
        }
        return Address(Assembler::FpReg, 0);
    }

    void forceStackDepth(uint32 newDepth) {
        uint32 oldDepth = stackDepth();
        FrameEntry *spBase = locals + script->nfixed;
        sp = spBase + newDepth;
        if (oldDepth <= newDepth)
            return;
        memset(spBase, 0, sizeof(FrameEntry) * (newDepth - oldDepth));
    }

    void forget(uint32 depth) {
        JS_ASSERT(sp - depth  >= locals + script->nfixed);
    }

    void sync() {
        for (FrameEntry *fe = base; fe < sp; fe++) {
            if (fe->type.needsSync()) {
                syncType(fe, masm);
                fe->type.setSynced();
            }
            if (fe->data.needsSync()) {
                syncData(fe, masm);
                fe->type.setSynced();
            }
        }
    }

    void flush();
    void killSyncedRegs(uint32 mask);
    void assertValidRegisterState();
    void sync(Assembler &masm, RegSnapshot &snapshot) const;
    void merge(Assembler &masm, const RegSnapshot &snapshot, uint32 invalidationDepth) const;
    void restoreTempRegs(Assembler &masm) const;

  private:
    void syncType(FrameEntry *fe, Assembler &masm) const;
    void syncData(FrameEntry *fe, Assembler &masm) const;
    void evictSomething();
    void invalidate(FrameEntry *fe);
    void reset(FrameEntry *fe);
    RegisterID getDataReg(FrameEntry *vi, FrameEntry *backing);

  private:
    JSContext *cx;
    JSScript *script;
    Assembler &masm;
    FrameEntry *base;
    FrameEntry *locals;
    FrameEntry *args;
    FrameEntry *sp;

    Registers regalloc;
    RegState  regstate[MacroAssembler::TotalRegisters];
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_framestate_h__ */

