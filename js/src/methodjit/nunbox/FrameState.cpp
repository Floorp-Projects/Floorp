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
#include "jscntxt.h"
#include "FrameState.h"

using namespace js;
using namespace js::mjit;

bool
FrameState::init(uint32 nargs)
{
    base = (FrameEntry *)cx->malloc(sizeof(FrameEntry) * (script->nslots + nargs));
    if (!base)
        return false;

    memset(base, 0, sizeof(FrameEntry) * (script->nslots + nargs));
    memset(regstate, 0, sizeof(regstate));

    args = base;
    locals = args + nargs;
    sp = locals + script->nfixed;

    return true;
}

void
FrameState::evictSomething()
{
}

FrameState::~FrameState()
{
    cx->free(base);
}

void
FrameState::assertValidRegisterState()
{
    Registers temp;

    RegisterID reg;
    uint32 index = 0;
    for (FrameEntry *fe = base; fe < sp; fe++, index++) {
        if (fe->type.inRegister()) {
            reg = fe->type.reg();
            temp.allocSpecific(reg);
            JS_ASSERT(regstate[reg].tracked);
            JS_ASSERT(regstate[reg].index == index);
            JS_ASSERT(regstate[reg].part == RegState::Part_Type);
        }
        if (fe->data.inRegister()) {
            reg = fe->data.reg();
            temp.allocSpecific(reg);
            JS_ASSERT(regstate[reg].tracked);
            JS_ASSERT(regstate[reg].index == index);
            JS_ASSERT(regstate[reg].part == RegState::Part_Data);
        }
    }

    JS_ASSERT(temp == regalloc);
}

void
FrameState::invalidate(FrameEntry *fe)
{
    if (!fe->type.synced()) {
        JS_NOT_REACHED("wat");
    }
    if (!fe->data.synced()) {
        JS_NOT_REACHED("wat2");
    }
    fe->type.setMemory();
    fe->data.setMemory();
}

void
FrameState::reset(FrameEntry *fe)
{
    fe->type.setMemory();
    fe->data.setMemory();
}

void
FrameState::flush()
{
    for (FrameEntry *fe = base; fe < sp; fe++)
        invalidate(fe);
}

void
FrameState::killSyncedRegs(uint32 mask)
{
    /* Subtract any regs we haven't allocated. */
    Registers regs(mask & ~(regalloc.freeMask));

    while (regs.anyRegsFree()) {
        RegisterID reg = regs.allocReg();
        if (!regstate[reg].tracked)
            continue;
        regstate[reg].tracked = false;
        regalloc.freeReg(reg);
        FrameEntry &fe = base[regstate[reg].index];
        RematInfo *mat;
        if (regstate[reg].part == RegState::Part_Type)
            mat = &fe.type;
        else
            mat = &fe.data;
        JS_ASSERT(mat->inRegister() && mat->reg() == reg);
        JS_ASSERT(mat->synced());
        mat->setMemory();
    }
}

void
FrameState::syncType(FrameEntry *fe, Assembler &masm) const
{
    JS_ASSERT(!fe->type.synced() && !fe->type.inMemory());
    if (fe->type.isConstant())
        masm.storeTypeTag(Imm32(fe->getTypeTag()), addressOf(fe));
    else
        masm.storeTypeTag(fe->type.reg(), addressOf(fe));
    fe->type.setSynced();
}

void
FrameState::syncData(FrameEntry *fe, Assembler &masm) const
{
    JS_ASSERT(!fe->data.synced() && !fe->data.inMemory());
    if (fe->data.isConstant())
        masm.storeData32(Imm32(fe->getPayload32()), addressOf(fe));
    else
        masm.storeData32(fe->data.reg(), addressOf(fe));
    fe->data.setSynced();
}

void
FrameState::sync(Assembler &masm, RegSnapshot &snapshot) const
{
    for (FrameEntry *fe = base; fe < sp; fe++) {
        if (fe->type.needsSync())
            syncType(fe, masm);
        if (fe->data.needsSync())
            syncData(fe, masm);
    }

    JS_STATIC_ASSERT(sizeof(snapshot.regs) == sizeof(regstate));
    JS_STATIC_ASSERT(sizeof(RegState) == sizeof(uint32));
    memcpy(snapshot.regs, regstate, sizeof(regstate));
    snapshot.alloc = regalloc;
}

void
FrameState::merge(Assembler &masm, const RegSnapshot &snapshot, uint32 invalidationDepth) const
{
    uint32 threshold = uint32(sp - base) - invalidationDepth;

    /*
     * We must take care not to accidentally clobber registers while merging
     * state. For example, if slot entry #1 has moved from EAX to EDX, and
     * slot entry #2 has moved from EDX to EAX, then the transition cannot
     * be completed with:
     *      mov eax, edx
     *      mov edx, eax
     *
     * This case doesn't need to be super fast, but we do want to detect it
     * quickly. To do this, we create a register allocator with the snapshot's
     * used registers, and incrementally free them.
     */
    Registers depends(snapshot.alloc);

    for (uint32 i = 0; i < MacroAssembler::TotalRegisters; i++) {
        if (!regstate[i].tracked)
            continue;

        if (regstate[i].index >= threshold) {
            /*
             * If the register is within the invalidation threshold, do a
             * normal load no matter what. This is guaranteed to be safe
             * because nothing above the invalidation threshold will yet
             * be in a register.
             */
            FrameEntry *fe = &base[regstate[i].index];
            Address address = addressOf(fe);
            RegisterID reg = RegisterID(i);
            if (regstate[i].part == RegState::Part_Type) {
                masm.loadTypeTag(address, reg);
            } else {
                /* :TODO: assert better */
                JS_ASSERT(fe->isTypeConstant());
                masm.loadData32(address, reg);
            }
        } else if (regstate[i].index != snapshot.regs[i].index ||
                   regstate[i].part != snapshot.regs[i].part) {
            JS_NOT_REACHED("say WAT");
        }
    }
}

