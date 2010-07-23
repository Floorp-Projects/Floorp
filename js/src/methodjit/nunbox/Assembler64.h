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

#if !defined jsjaeger_assembler64_h__ && defined JS_METHODJIT && defined JS_64BIT
#define jsjaeger_assembler64_h__

#include "methodjit/BaseAssembler.h"
#include "methodjit/MachineRegs.h"

namespace js {
namespace mjit {

class Imm64 : public JSC::MacroAssembler::ImmPtr
{
  public:
    Imm64(uint64 u)
      : ImmPtr((const void *)u)
    { }
};

class ImmShiftedTag : public JSC::MacroAssembler::ImmPtr
{
  public:
    ImmShiftedTag(JSValueShiftedTag shtag)
      : ImmPtr((const void *)shtag)
    { }
};

class ImmType : public ImmShiftedTag
{
  public:
    ImmType(JSValueType type)
      : ImmShiftedTag(JSValueShiftedTag(JSVAL_TYPE_TO_SHIFTED_TAG(type)))
    { }
};

class Assembler : public BaseAssembler
{
    static const uint32 PAYLOAD_OFFSET = 0;

  public:
    static const JSC::MacroAssembler::Scale JSVAL_SCALE = JSC::MacroAssembler::TimesEight;

    Address payloadOf(Address address) {
        return address;
    }

    BaseIndex payloadOf(BaseIndex address) {
        return address;
    }

    Address valueOf(Address address) {
        return address;
    }

    BaseIndex valueOf(BaseIndex address) {
        return address;
    }

#if 0
    /* This does not work with 64-bit boxing format, since tag is not byte-aligned. */
    Address tagOf(Address address) {
        return Address(address.base, address.offset + TAG_OFFSET);
    }

    BaseIndex tagOf(BaseIndex address) {
        return BaseIndex(address.base, address.index, address.scale, address.offset + TAG_OFFSET);
    }
#endif /* 0 */

#if 0
    /* type and data need to be unified. */
    void loadSlot(RegisterID obj, RegisterID clobber, uint32 slot, RegisterID type, RegisterID data) {
        JS_ASSERT(type != data);
        Address address(obj, offsetof(JSObject, fslots) + slot * sizeof(Value));
        if (slot >= JS_INITIAL_NSLOTS) {
            loadPtr(Address(obj, offsetof(JSObject, dslots)), clobber);
            address = Address(obj, (slot - JS_INITIAL_NSLOTS) * sizeof(Value));
        }
        if (obj == type) {
            loadData32(address, data);
            loadTypeTag(address, type);
        } else {
            loadTypeTag(address, type);
            loadData32(address, data);
        }
    }
#endif

    /* TODO: Don't really need this..? */
    void loadValue(Address address, RegisterID dst) {
        loadPtr(address, dst);
    }

    void loadValue(BaseIndex address, RegisterID dst) {
        loadPtr(address, dst);
    }

    void loadValueThenType(Address address, RegisterID val, RegisterID type) {
        loadValue(valueOf(address), val);
        if (val != type)
            move(val, type);
        andPtr(Imm64(JSVAL_TAG_MASK), type);
    }
    
    void loadValueThenType(BaseIndex address, RegisterID val, RegisterID type) {
        loadValue(valueOf(address), val);
        if (val != type)
            move(val, type);
        andPtr(Imm64(JSVAL_TAG_MASK), type);
    }

    void loadValueThenPayload(Address address, RegisterID val, RegisterID payload) {
        loadValue(valueOf(address), val);
        if (val != payload)
            move(val, payload);
        andPtr(Imm64(JSVAL_PAYLOAD_MASK), payload);
    }

    void loadValueThenPayload(BaseIndex address, RegisterID val, RegisterID payload) {
        loadValue(valueOf(address), val);
        if (val != payload)
            move(val, payload);
        andPtr(Imm64(JSVAL_PAYLOAD_MASK), payload);
    }

    /* 
     * TODO: All this gets to go.
     * This needs to be part of the FrameState, since it will
     * be performing register allocation.
     */
    void loadTypeTag(Address address, RegisterID reg) {
        loadValueThenType(valueOf(address), Registers::ValueReg, reg);
    }

    void loadTypeTag(BaseIndex address, RegisterID reg) {
        loadValueThenType(valueOf(address), Registers::ValueReg, reg);
    }

    void storeTypeTag(ImmShiftedTag imm, Address address) {
        loadValue(valueOf(address), Registers::ValueReg);
        andPtr(Imm64(JSVAL_PAYLOAD_MASK), Registers::ValueReg);
        orPtr(imm, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    void storeTypeTag(ImmShiftedTag imm, BaseIndex address) {
        loadValue(valueOf(address), Registers::ValueReg);
        andPtr(Imm64(JSVAL_PAYLOAD_MASK), Registers::ValueReg);
        orPtr(imm, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    void storeTypeTag(RegisterID reg, Address address) {
        /* The type tag must be stored in shifted format. */
        loadValue(valueOf(address), Registers::ValueReg);
        andPtr(Imm64(JSVAL_PAYLOAD_MASK), Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));

    }

    void storeTypeTag(RegisterID reg, BaseIndex address) {
        /* The type tag must be stored in shifted format. */
        loadValue(valueOf(address), Registers::ValueReg);
        andPtr(Imm64(JSVAL_PAYLOAD_MASK), Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    void loadPayload(Address address, RegisterID reg) {
        loadValueThenPayload(address, Registers::ValueReg, reg);
    }

    void loadPayload(BaseIndex address, RegisterID reg) {
        loadValueThenPayload(address, Registers::ValueReg, reg);
    }

    void storePayload(RegisterID reg, Address address) {
        /* Not for doubles. */
        loadValue(valueOf(address), Registers::ValueReg);
        andPtr(Imm64(JSVAL_TAG_MASK), Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    void storePayload(RegisterID reg, BaseIndex address) {
        /* Not for doubles. */
        loadValue(valueOf(address), Registers::ValueReg);
        andPtr(Imm64(JSVAL_TAG_MASK), Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }
    
    void storePayload(Imm64 imm, Address address) {
        /* Not for doubles. */
        storePtr(imm, valueOf(address));
    }

#if 0
    void storePayload(Imm32 imm, Address address) {
    }
#endif

    void storeValue(const Value &v, Address address) {
        jsval_layout jv;
        jv.asBits = JSVAL_BITS(Jsvalify(v));

        storePtr(Imm64(jv.asBits), valueOf(address));
    }

    void storeValue(const Value &v, BaseIndex address) {
        jsval_layout jv;
        jv.asBits = JSVAL_BITS(Jsvalify(v));

        storePtr(Imm64(jv.asBits), valueOf(address));        
    }

    /*
     * FIXME: This is only used by slowLoadConstantDouble().
     * It should disappear when that function can generate
     * constants into the opstream.
     */
    void storeLayout(const jsval_layout &jv, Address address) {
        storePtr(Imm64(jv.asBits), valueOf(address));
    }

    void loadFunctionPrivate(RegisterID base, RegisterID to) {
        Address privSlot(base, offsetof(JSObject, fslots) +
                               JSSLOT_PRIVATE * sizeof(Value));
        loadPtr(privSlot, to);
        lshiftPtr(Imm32(1), to);
    }

    Jump testNull(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_NULL));
    }

    Jump testNull(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_NULL));
    }

    Jump testInt32(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testInt32(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testNumber(Assembler::Condition cond, RegisterID reg) {
        cond = (cond == Assembler::Equal) ? Assembler::BelowOrEqual : Assembler::Above;
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testNumber(Assembler::Condition cond, Address address) {
        cond = (cond == Assembler::Equal) ? Assembler::BelowOrEqual : Assembler::Above;
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testPrimitive(Assembler::Condition cond, RegisterID reg) {
        cond = (cond == Assembler::NotEqual) ? Assembler::AboveOrEqual : Assembler::Below;
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testPrimitive(Assembler::Condition cond, Address address) {
        cond = (cond == Assembler::NotEqual) ? Assembler::AboveOrEqual : Assembler::Below;
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testObject(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testObject(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testDouble(Assembler::Condition cond, RegisterID reg) {
        Assembler::Condition opcond;
        if (cond == Assembler::Equal)
            opcond = Assembler::Below;
        else
            opcond = Assembler::AboveOrEqual;
        return branchPtr(opcond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_MAX_DOUBLE));
    }

    Jump testDouble(Assembler::Condition cond, Address address) {
        Assembler::Condition opcond;
        if (cond == Assembler::Equal)
            opcond = Assembler::Below;
        else
            opcond = Assembler::AboveOrEqual;
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(opcond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_MAX_DOUBLE));
    }

    Jump testBoolean(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_BOOLEAN));
    }

    Jump testBoolean(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_BOOLEAN));
    }

    Jump testString(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_STRING));
    }

    Jump testString(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_BOOLEAN));
    }
};

} /* namespace js */
} /* namespace mjit */

#endif

