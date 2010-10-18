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
 *   Sean Stangl <sstangl@mozilla.com>
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

#if !defined jsjaeger_assembler64_h__ && defined JS_METHODJIT && defined JS_PUNBOX64
#define jsjaeger_assembler64_h__

#include "methodjit/BaseAssembler.h"
#include "methodjit/MachineRegs.h"
#include "methodjit/RematInfo.h"

namespace js {
namespace mjit {

struct Imm64 : JSC::MacroAssembler::ImmPtr
{
    Imm64(uint64 u)
      : ImmPtr((const void *)u)
    { }
};

/* Tag stored in shifted format. */
struct ImmTag : JSC::MacroAssembler::ImmPtr
{
    ImmTag(JSValueShiftedTag shtag)
      : ImmPtr((const void *)shtag)
    { }
};

struct ImmType : ImmTag
{
    ImmType(JSValueType type)
      : ImmTag(JSValueShiftedTag(JSVAL_TYPE_TO_SHIFTED_TAG(type)))
    { }
};

struct ImmPayload : Imm64
{
    ImmPayload(uint64 payload)
      : Imm64(payload)
    { }
};

class Assembler : public BaseAssembler
{
    static const uint32 PAYLOAD_OFFSET = 0;

  public:
    static const JSC::MacroAssembler::Scale JSVAL_SCALE = JSC::MacroAssembler::TimesEight;

    template <typename T>
    T payloadOf(T address) {
        return address;
    }

    template <typename T>
    T valueOf(T address) {
        return address;
    }

    void loadSlot(RegisterID obj, RegisterID clobber, uint32 slot, bool inlineAccess,
                  RegisterID type, RegisterID data) {
        JS_ASSERT(type != data);
        Address address(obj, JSObject::getFixedSlotOffset(slot));
        if (!inlineAccess) {
            loadPtr(Address(obj, offsetof(JSObject, slots)), clobber);
            address = Address(clobber, slot * sizeof(Value));
        }
        
        loadValueAsComponents(address, type, data);
    }

    template <typename T>
    void loadValue(T address, RegisterID dst) {
        loadPtr(address, dst);
    }

    void convertValueToType(RegisterID val) {
        andPtr(Registers::TypeMaskReg, val);
    }

    void convertValueToPayload(RegisterID val) {
        andPtr(Registers::PayloadMaskReg, val);
    }

    /* Returns a label after the one Value load. */
    template <typename T>
    Label loadValueAsComponents(T address, RegisterID type, RegisterID payload) {
        loadValue(address, type);
        Label l = label();

        move(Registers::PayloadMaskReg, payload);
        andPtr(type, payload);
        xorPtr(payload, type);

        return l;
    }

    void loadValueAsComponents(const Value &val, RegisterID type, RegisterID payload) {
        move(Imm64(val.asRawBits() & JSVAL_TAG_MASK), type);
        move(Imm64(val.asRawBits() & JSVAL_PAYLOAD_MASK), payload);
    }

    template <typename T>
    void storeValueFromComponents(RegisterID type, RegisterID payload, T address) {
        move(type, Registers::ValueReg);
        orPtr(payload, Registers::ValueReg);
        storeValue(Registers::ValueReg, address);
    }

    template <typename T>
    void storeValueFromComponents(ImmTag type, RegisterID payload, T address) {
        move(type, Registers::ValueReg);
        orPtr(payload, Registers::ValueReg);
        storeValue(Registers::ValueReg, address);
    }

    template <typename T>
    void loadTypeTag(T address, RegisterID reg) {
        loadValue(address, reg);
        convertValueToType(reg);
    }

    template <typename T>
    void storeTypeTag(ImmTag imm, T address) {
        loadPayload(address, Registers::ValueReg);
        orPtr(imm, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    template <typename T>
    void storeTypeTag(RegisterID reg, T address) {
        /* The type tag must be stored in shifted format. */
        loadPayload(address, Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    template <typename T>
    void loadPayload(T address, RegisterID reg) {
        loadValue(address, reg);
        convertValueToPayload(reg);
    }

    template <typename T>
    void storePayload(RegisterID reg, T address) {
        /* Not for doubles. */
        loadTypeTag(address, Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }
    
    template <typename T>
    void storePayload(ImmPayload imm, T address) {
        /* Not for doubles. */
        storePtr(imm, valueOf(address));
    }

    template <typename T>
    void storeValue(RegisterID reg, T address) {
        storePtr(reg, valueOf(address));
    }

    template <typename T>
    void storeValue(const Value &v, T address) {
        jsval_layout jv;
        jv.asBits = JSVAL_BITS(Jsvalify(v));

        storePtr(Imm64(jv.asBits), valueOf(address));
    }

    template <typename T>
    void storeValue(const ValueRemat &vr, T address) {
        if (vr.isConstant)
            storeValue(Valueify(vr.u.v), address);
        else if (vr.u.s.isTypeKnown)
            storeValueFromComponents(ImmType(vr.u.s.type.knownType), vr.u.s.data, address);
        else
            storeValueFromComponents(vr.u.s.type.reg, vr.u.s.data, address);
    }

    void loadPrivate(Address privAddr, RegisterID to) {
        loadPtr(privAddr, to);
        lshiftPtr(Imm32(1), to);
    }

    void loadFunctionPrivate(RegisterID base, RegisterID to) {
        Address priv(base, offsetof(JSObject, privateData));
        loadPtr(priv, to);
    }

    Jump testNull(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_NULL));
    }

    Jump testNull(Assembler::Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testNull(cond, Registers::ValueReg);
    }

    Jump testUndefined(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_UNDEFINED));
    }

    Jump testUndefined(Assembler::Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testUndefined(cond, Registers::ValueReg);
    }

    Jump testInt32(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testInt32(Assembler::Condition cond, Address address) {
        loadTypeTag(address, Registers::ValueReg);
        return testInt32(cond, Registers::ValueReg);
    }

    Jump testNumber(Assembler::Condition cond, RegisterID reg) {
        cond = (cond == Assembler::Equal) ? Assembler::Below : Assembler::AboveOrEqual;
        return branchPtr(cond, reg,
                         ImmTag(JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET));
    }

    Jump testNumber(Assembler::Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testNumber(cond, Registers::ValueReg);
    }

    Jump testPrimitive(Assembler::Condition cond, RegisterID reg) {
        cond = (cond == Assembler::Equal) ? Assembler::Below : Assembler::AboveOrEqual;
        return branchPtr(cond, reg,
                         ImmTag(JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET));
    }

    Jump testPrimitive(Assembler::Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testPrimitive(cond, Registers::ValueReg);
    }

    Jump testObject(Assembler::Condition cond, RegisterID reg) {
        cond = (cond == Assembler::Equal) ? Assembler::AboveOrEqual : Assembler::Below;
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testObject(Assembler::Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testObject(cond, Registers::ValueReg);
    }

    Jump testDouble(Assembler::Condition cond, RegisterID reg) {
        cond = (cond == Assembler::Equal) ? Assembler::BelowOrEqual : Assembler::Above;
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_MAX_DOUBLE));
    }

    Jump testDouble(Assembler::Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testDouble(cond, Registers::ValueReg);
    }

    Jump testBoolean(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_BOOLEAN));
    }

    Jump testBoolean(Assembler::Condition cond, Address address) {
        loadTypeTag(address, Registers::ValueReg);
        return testBoolean(cond, Registers::ValueReg);
    }

    Jump testString(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_STRING));
    }

    Jump testString(Assembler::Condition cond, Address address) {
        loadTypeTag(address, Registers::ValueReg);
        return testString(cond, Registers::ValueReg);
    }
};

} /* namespace mjit */
} /* namespace js */

#endif

