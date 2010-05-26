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
#define __STDC_LIMIT_MACROS

#include "jsapi.h"
#include "jsnum.h"
#include "CodeGenerator.h"

using namespace js;
using namespace js::mjit;

CodeGenerator::CodeGenerator(MacroAssembler &masm, FrameState &frame)
  : masm(masm), frame(frame)
{
}

void
CodeGenerator::storeJsval(const Value &v, Address address)
{
    const jsval jv = Jsvalify(v);
    masm.store32(Imm32(jv.mask), Address(address.base, address.offset + offsetof(jsval, mask)));
    if (v.isNullOrUndefined())
        return;
#ifdef JS_CPU_X64
    masm.storePtr(ImmPtr(jv.data.ptr), Address(address.base, address.offset + offsetof(jsval, data)));
#elif defined(JS_CPU_ARM) || defined(JS_CPU_X86)
    if (v.isDouble()) {
        jsdpun u;
        u.d = jv.data.dbl;
        masm.store32(Imm32(u.s.hi), Address(address.base,
                                            address.offset + offsetof(jsval, data)));
        masm.store32(Imm32(u.s.lo), Address(address.base,
                                            address.offset + offsetof(jsval, data) + 4));
    } else {
        masm.storePtr(ImmPtr(jv.data.ptr), Address(address.base, address.offset + offsetof(jsval, data)));
    }
#else
# error "Unknown architecture"
#endif
    return;
}

void
CodeGenerator::storeValue(FrameEntry *fe, Address address, bool popped)
{
    if (fe->isConstant()) {
        storeJsval(Valueify(fe->getConstant()), address);
        return;
    }

    Address feAddr = frame.addressOf(fe);

    /* Copy the payload - :TODO: something better */
#ifdef JS_32BIT
    RegisterID tempReg = frame.allocReg();
    masm.loadPtr(Address(feAddr.base, feAddr.offset + offsetof(jsval, data)), tempReg);
    masm.storePtr(tempReg, Address(address.base, address.offset + offsetof(jsval, data)));
    masm.loadPtr(Address(feAddr.base, feAddr.offset + offsetof(jsval, data) + 4), tempReg);
    masm.storePtr(tempReg, Address(address.base, address.offset + offsetof(jsval, data) + 4));
    frame.freeReg(tempReg);
#else
# error "IMPLEMENT ME"
#endif

    /* Copy the type. */
    if (fe->isTypeConstant()) {
        masm.store32(Imm32(fe->getTypeTag()),
                     Address(address.base,
                             address.offset + offsetof(jsval, mask)));
    } else {
        RegisterID reg = frame.tempRegForType(fe);
        masm.store32(reg, Address(address.base, address.offset + offsetof(jsval, mask)));
    }
}

void
CodeGenerator::pushValueOntoFrame(Address address)
{
    RegisterID tempReg = frame.allocReg();

    Address tos = frame.topOfStack();

#ifdef JS_CPU_X64
    masm.loadPtr(Address(address.base, address.offset + offsetof(jsval, data)), tempReg);
    masm.storePtr(tempReg, Address(tos.base, tos.offset + offsetof(jsval, data)));
#elif defined(JS_CPU_ARM) || defined(JS_CPU_X86)
    masm.load32(Address(address.base, address.offset + offsetof(jsval, data)), tempReg);
    masm.store32(tempReg, Address(tos.base, tos.offset + offsetof(jsval, data)));
    masm.load32(Address(address.base, address.offset + offsetof(jsval, data) + 4), tempReg);
    masm.store32(tempReg, Address(tos.base, tos.offset + offsetof(jsval, data) + 4));
#endif
    masm.load32(Address(address.base, address.offset + offsetof(jsval, mask)), tempReg);

    frame.pushUnknownType(tempReg);
}

