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

static const uint32 TAG_OFFSET     = 4;
static const uint32 PAYLOAD_OFFSET = 0;

class ImmIntPtr : public JSC::MacroAssembler::ImmPtr
{
  public:
    ImmIntPtr(int64_t i64)
      : ImmPtr(reinterpret_cast<void*>(i64))
    { }
};

CodeGenerator::CodeGenerator(MacroAssembler &masm, FrameState &frame)
  : masm(masm), frame(frame)
{
}

void
CodeGenerator::storeJsval(const Value &v, Address address)
{
    jsval_layout jv;
    jv.asBits = Jsvalify(v);

    masm.store32(Imm32(jv.s.mask32), Address(address.base, address.offset + TAG_OFFSET));
    if (!v.isNullOrUndefined())
        masm.store32(Imm32(jv.s.payload.u32), Address(address.base, address.offset + PAYLOAD_OFFSET));
}

void
CodeGenerator::storeValue(FrameEntry *fe, Address address, bool popped)
{
    if (fe->isConstant()) {
        storeJsval(fe->getValue(), address);
        return;
    }

    Address feAddr = frame.addressOf(fe);
    if (fe->isTypeConstant()) {
        masm.store32(Imm32(fe->getTypeTag()),
                     Address(address.base, address.offset + TAG_OFFSET));
    } else {
        RegisterID typeReg = frame.tempRegForType(fe);
        masm.store32(typeReg, Address(address.base, address.offset + TAG_OFFSET));
    }
    RegisterID tempReg = frame.allocReg();
    masm.load32(Address(feAddr.base, feAddr.offset + PAYLOAD_OFFSET), tempReg);
    masm.store32(tempReg, Address(address.base, address.offset + PAYLOAD_OFFSET));
    frame.freeReg(tempReg);
}

void
CodeGenerator::pushValueOntoFrame(Address address)
{
    RegisterID tempReg = frame.allocReg();

    Address tos = frame.topOfStack();

    masm.load32(Address(address.base, address.offset + PAYLOAD_OFFSET), tempReg);
    masm.store32(tempReg, Address(tos.base, tos.offset + PAYLOAD_OFFSET));
    masm.load32(Address(address.base, address.offset + TAG_OFFSET), tempReg);
    masm.store32(tempReg, Address(tos.base, tos.offset + TAG_OFFSET));

    frame.pushUnknownType(tempReg);
}

