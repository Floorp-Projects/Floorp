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

#if !defined jsjaeger_assembler_h__ && defined JS_METHODJIT && defined JS_NUNBOX32
#define jsjaeger_assembler_h__

#include "assembler/assembler/MacroAssembler.h"
#include "methodjit/RematInfo.h"

namespace js {
namespace mjit {

/* Don't use ImmTag. Use ImmType instead. */
struct ImmTag : JSC::MacroAssembler::Imm32
{
    ImmTag(JSValueTag mask)
      : Imm32(int32(mask))
    { }
};

struct ImmType : ImmTag
{
    ImmType(JSValueType type)
      : ImmTag(JSVAL_TYPE_TO_TAG(type))
    { }
};

struct ImmPayload : JSC::MacroAssembler::Imm32
{
    ImmPayload(uint32 payload)
      : Imm32(payload)
    { }
};

class NunboxAssembler : public JSC::MacroAssembler
{
    static const uint32 PAYLOAD_OFFSET = 0;
    static const uint32 TAG_OFFSET     = 4;

  public:
    static const JSC::MacroAssembler::Scale JSVAL_SCALE = JSC::MacroAssembler::TimesEight;

    template <typename T>
    T payloadOf(T address) {
        return address;
    }

    Address tagOf(Address address) {
        return Address(address.base, address.offset + TAG_OFFSET);
    }

    BaseIndex tagOf(BaseIndex address) {
        return BaseIndex(address.base, address.index, address.scale, address.offset + TAG_OFFSET);
    }

    void loadSlot(RegisterID obj, RegisterID clobber, uint32 slot, bool inlineAccess,
                  RegisterID type, RegisterID data) {
        JS_ASSERT(type != data);
        Address address(obj, JSObject::getFixedSlotOffset(slot));
        RegisterID activeAddressReg = obj;
        if (!inlineAccess) {
            loadPtr(Address(obj, offsetof(JSObject, slots)), clobber);
            address = Address(clobber, slot * sizeof(Value));
            activeAddressReg = clobber;
        }
        if (activeAddressReg == type) {
            loadPayload(address, data);
            loadTypeTag(address, type);
        } else {
            loadTypeTag(address, type);
            loadPayload(address, data);
        }
    }

    template <typename T>
    void loadTypeTag(T address, RegisterID reg) {
        load32(tagOf(address), reg);
    }

    template <typename T>
    void storeTypeTag(ImmTag imm, T address) {
        store32(imm, tagOf(address));
    }

    template <typename T>
    void storeTypeTag(RegisterID reg, T address) {
        store32(reg, tagOf(address));
    }

    template <typename T>
    void loadPayload(T address, RegisterID reg) {
        load32(payloadOf(address), reg);
    }

    template <typename T>
    void storePayload(RegisterID reg, T address) {
        store32(reg, payloadOf(address));
    }

    template <typename T>
    void storePayload(ImmPayload imm, T address) {
        store32(imm, payloadOf(address));
    }

    bool addressUsesRegister(BaseIndex address, RegisterID reg) {
        return (address.base == reg) || (address.index == reg);
    }

    bool addressUsesRegister(Address address, RegisterID reg) {
        return address.base == reg;
    }

    /* Loads type first, then payload, returning label after type load. */
    template <typename T>
    Label loadValueAsComponents(T address, RegisterID type, RegisterID payload) {
        JS_ASSERT(!addressUsesRegister(address, type));
        loadTypeTag(address, type);
        Label l = label();
        loadPayload(address, payload);
        return l;
    }

    void loadValueAsComponents(const Value &val, RegisterID type, RegisterID payload) {
        jsval_layout jv;
        jv.asBits = JSVAL_BITS(Jsvalify(val));

        move(ImmTag(jv.s.tag), type);
        move(Imm32(jv.s.payload.u32), payload);
    }

    /*
     * Stores type first, then payload.
     */
    template <typename T>
    Label storeValue(const Value &v, T address) {
        jsval_layout jv;
        jv.asBits = JSVAL_BITS(Jsvalify(v));

        store32(ImmTag(jv.s.tag), tagOf(address));
        Label l = label();
        store32(Imm32(jv.s.payload.u32), payloadOf(address));
        return l;
    }

    template <typename T>
    void storeValueFromComponents(RegisterID type, RegisterID payload, T address) {
        storeTypeTag(type, address);
        storePayload(payload, address);
    }

    template <typename T>
    void storeValueFromComponents(ImmType type, RegisterID payload, T address) {
        storeTypeTag(type, address);
        storePayload(payload, address);
    }

    template <typename T>
    Label storeValue(const ValueRemat &vr, T address) {
        if (vr.isConstant()) {
            return storeValue(vr.value(), address);
        } else {
            if (vr.isTypeKnown())
                storeTypeTag(ImmType(vr.knownType()), address);
            else
                storeTypeTag(vr.typeReg(), address);
            Label l = label();
            storePayload(vr.dataReg(), address);
            return l;
        }
    }

    void loadPrivate(Address privAddr, RegisterID to) {
        loadPtr(privAddr, to);
    }

    void loadFunctionPrivate(RegisterID base, RegisterID to) {
        Address priv(base, offsetof(JSObject, privateData));
        loadPtr(priv, to);
    }

    Jump testNull(Condition cond, RegisterID reg) {
        return branch32(cond, reg, ImmTag(JSVAL_TAG_NULL));
    }

    Jump testNull(Condition cond, Address address) {
        return branch32(cond, tagOf(address), ImmTag(JSVAL_TAG_NULL));
    }

    Jump testUndefined(Condition cond, RegisterID reg) {
        return branch32(cond, reg, ImmTag(JSVAL_TAG_UNDEFINED));
    }

    Jump testUndefined(Condition cond, Address address) {
        return branch32(cond, tagOf(address), ImmTag(JSVAL_TAG_UNDEFINED));
    }

    Jump testInt32(Condition cond, RegisterID reg) {
        return branch32(cond, reg, ImmTag(JSVAL_TAG_INT32));
    }

    Jump testInt32(Condition cond, Address address) {
        return branch32(cond, tagOf(address), ImmTag(JSVAL_TAG_INT32));
    }

    Jump testNumber(Condition cond, RegisterID reg) {
        cond = (cond == Equal) ? BelowOrEqual : Above;
        return branch32(cond, reg, ImmTag(JSVAL_TAG_INT32));
    }

    Jump testNumber(Condition cond, Address address) {
        cond = (cond == Equal) ? BelowOrEqual : Above;
        return branch32(cond, tagOf(address), ImmTag(JSVAL_TAG_INT32));
    }

    Jump testPrimitive(Condition cond, RegisterID reg) {
        cond = (cond == NotEqual) ? AboveOrEqual : Below;
        return branch32(cond, reg, ImmTag(JSVAL_TAG_OBJECT));
    }

    Jump testPrimitive(Condition cond, Address address) {
        cond = (cond == NotEqual) ? AboveOrEqual : Below;
        return branch32(cond, tagOf(address), ImmTag(JSVAL_TAG_OBJECT));
    }

    Jump testObject(Condition cond, RegisterID reg) {
        return branch32(cond, reg, ImmTag(JSVAL_TAG_OBJECT));
    }

    Jump testObject(Condition cond, Address address) {
        return branch32(cond, tagOf(address), ImmTag(JSVAL_TAG_OBJECT));
    }

    Jump testDouble(Condition cond, RegisterID reg) {
        Condition opcond;
        if (cond == Equal)
            opcond = Below;
        else
            opcond = AboveOrEqual;
        return branch32(opcond, reg, ImmTag(JSVAL_TAG_CLEAR));
    }

    Jump testDouble(Condition cond, Address address) {
        Condition opcond;
        if (cond == Equal)
            opcond = Below;
        else
            opcond = AboveOrEqual;
        return branch32(opcond, tagOf(address), ImmTag(JSVAL_TAG_CLEAR));
    }

    Jump testBoolean(Condition cond, RegisterID reg) {
        return branch32(cond, reg, ImmTag(JSVAL_TAG_BOOLEAN));
    }

    Jump testBoolean(Condition cond, Address address) {
        return branch32(cond, tagOf(address), ImmTag(JSVAL_TAG_BOOLEAN));
    }

    Jump testString(Condition cond, RegisterID reg) {
        return branch32(cond, reg, ImmTag(JSVAL_TAG_STRING));
    }

    Jump testString(Condition cond, Address address) {
        return branch32(cond, tagOf(address), ImmTag(JSVAL_TAG_STRING));
    }

    template <typename T>
    Jump fastArrayLoadSlot(T address, RegisterID typeReg, RegisterID dataReg) {
        loadTypeTag(address, typeReg);
        Jump notHole = branch32(Equal, typeReg, ImmType(JSVAL_TYPE_MAGIC));
        loadPayload(address, dataReg);
        return notHole;
    }
};

typedef NunboxAssembler ValueAssembler;

} /* namespace mjit */
} /* namespace js */

#endif

