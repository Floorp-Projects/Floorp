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
#include "jsscope.h"
#include "jsnum.h"
#include "MonoIC.h"
#include "StubCalls.h"
#include "assembler/assembler/RepatchBuffer.h"

#include "jsscopeinlines.h"

using namespace js;
using namespace js::mjit;
using namespace js::mjit::ic;

#if defined JS_MONOIC

static void
PatchGetFallback(VMFrame &f, ic::MICInfo &mic)
{
    JSC::RepatchBuffer repatch(mic.stubEntry.executableAddress(), 64);
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, stubs::GetGlobalName));
    repatch.relink(mic.stubCall, fptr);
}

void JS_FASTCALL
ic::GetGlobalName(VMFrame &f, uint32 index)
{
    JSObject *obj = f.fp->scopeChain->getGlobal();
    ic::MICInfo &mic = f.fp->script->mics[index];
    JSAtom *atom = f.fp->script->getAtom(GET_INDEX(f.regs.pc));
    jsid id = ATOM_TO_JSID(atom);

    JS_ASSERT(mic.kind == ic::MICInfo::GET);

    JS_LOCK_OBJ(f.cx, obj);
    JSScope *scope = obj->scope();
    JSScopeProperty *sprop = scope->lookup(id);
    if (!sprop ||
        !sprop->hasDefaultGetterOrIsMethod() ||
        !SPROP_HAS_VALID_SLOT(sprop, scope))
    {
        JS_UNLOCK_SCOPE(f.cx, scope);
        if (sprop)
            PatchGetFallback(f, mic);
        stubs::GetGlobalName(f);
        return;
    }
    uint32 shape = obj->shape();
    uint32 slot = sprop->slot;
    JS_UNLOCK_SCOPE(f.cx, scope);

    mic.u.name.touched = true;

    /* Patch shape guard. */
    JSC::RepatchBuffer repatch(mic.entry.executableAddress(), 50);
    repatch.repatch(mic.shape, reinterpret_cast<void*>(shape));

    /* Patch loads. */
    JS_ASSERT(slot >= JS_INITIAL_NSLOTS);
    slot -= JS_INITIAL_NSLOTS;
    slot *= sizeof(Value);
    JSC::RepatchBuffer loads(mic.load.executableAddress(), 32, false);
    loads.repatch(mic.load.dataLabel32AtOffset(MICInfo::GET_TYPE_OFFSET), slot + 4);
    loads.repatch(mic.load.dataLabel32AtOffset(MICInfo::GET_DATA_OFFSET), slot);

    /* Do load anyway... this time. */
    stubs::GetGlobalName(f);
}

static void JS_FASTCALL
SetGlobalNameSlow(VMFrame &f, uint32 index)
{
    JSAtom *atom = f.fp->script->getAtom(GET_INDEX(f.regs.pc));
    stubs::SetGlobalName(f, atom);
}

static void
PatchSetFallback(VMFrame &f, ic::MICInfo &mic)
{
    JSC::RepatchBuffer repatch(mic.stubEntry.executableAddress(), 64);
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, SetGlobalNameSlow));
    repatch.relink(mic.stubCall, fptr);
}

void JS_FASTCALL
ic::SetGlobalName(VMFrame &f, uint32 index)
{
    JSObject *obj = f.fp->scopeChain->getGlobal();
    ic::MICInfo &mic = f.fp->script->mics[index];
    JSAtom *atom = f.fp->script->getAtom(GET_INDEX(f.regs.pc));
    jsid id = ATOM_TO_JSID(atom);

    JS_ASSERT(mic.kind == ic::MICInfo::SET);

    JS_LOCK_OBJ(f.cx, obj);
    JSScope *scope = obj->scope();
    JSScopeProperty *sprop = scope->lookup(id);
    if (!sprop ||
        !sprop->hasDefaultGetterOrIsMethod() ||
        !SPROP_HAS_VALID_SLOT(sprop, scope))
    {
        JS_UNLOCK_SCOPE(f.cx, scope);
        if (sprop)
            PatchSetFallback(f, mic);
        stubs::SetGlobalName(f, atom);
        return;
    }
    uint32 shape = obj->shape();
    uint32 slot = sprop->slot;
    JS_UNLOCK_SCOPE(f.cx, scope);

    mic.u.name.touched = true;

    /* Patch shape guard. */
    JSC::RepatchBuffer repatch(mic.entry.executableAddress(), 50);
    repatch.repatch(mic.shape, reinterpret_cast<void*>(shape));

    /* Patch loads. */
    JS_ASSERT(slot >= JS_INITIAL_NSLOTS);
    slot -= JS_INITIAL_NSLOTS;
    slot *= sizeof(Value);

    JSC::RepatchBuffer stores(mic.load.executableAddress(), 32, false);
    stores.repatch(mic.load.dataLabel32AtOffset(MICInfo::SET_TYPE_OFFSET), slot + 4);

    uint32 dataOffset;
    if (mic.u.name.typeConst)
        dataOffset = MICInfo::SET_DATA_CONST_TYPE_OFFSET;
    else
        dataOffset = MICInfo::SET_DATA_TYPE_OFFSET;
    if (mic.u.name.dataWrite)
        stores.repatch(mic.load.dataLabel32AtOffset(dataOffset), slot);

    /* Do load anyway... this time. */
    stubs::SetGlobalName(f, atom);
}

#endif /* JS_MONOIC */
