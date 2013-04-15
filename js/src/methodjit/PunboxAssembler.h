/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_assembler64_h__ && defined JS_METHODJIT && defined JS_PUNBOX64
#define jsjaeger_assembler64_h__

#include "assembler/assembler/MacroAssembler.h"
#include "js/Value.h"
#include "methodjit/MachineRegs.h"
#include "methodjit/RematInfo.h"

namespace js {
namespace mjit {

struct Imm64 : JSC::MacroAssembler::ImmPtr
{
    Imm64(uint64_t u)
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
    {
        JS_ASSERT(type > JSVAL_TYPE_DOUBLE);
    }
};

struct ImmPayload : Imm64
{
    ImmPayload(uint64_t payload)
      : Imm64(payload)
    { }
};

class PunboxAssembler : public JSC::MacroAssembler
{
  public:
    static const uint32_t PAYLOAD_OFFSET = 0;

    static const JSC::MacroAssembler::Scale JSVAL_SCALE = JSC::MacroAssembler::TimesEight;

    template <typename T>
    T payloadOf(T address) {
        return address;
    }

    template <typename T>
    T valueOf(T address) {
        return address;
    }

    void loadInlineSlot(RegisterID objReg, uint32_t slot,
                        RegisterID typeReg, RegisterID dataReg) {
        Address address(objReg, JSObject::getFixedSlotOffset(slot));
        loadValueAsComponents(address, typeReg, dataReg);
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

    // Returns a label after the one Value load.
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
        uint64_t bits = JSVAL_TO_IMPL(val).asBits;
        move(Imm64(bits & JSVAL_TAG_MASK), type);
        move(Imm64(bits & JSVAL_PAYLOAD_MASK), payload);
    }

    void loadValuePayload(const Value &val, RegisterID payload) {
        move(Imm64(JSVAL_TO_IMPL(val).asBits & JSVAL_PAYLOAD_MASK), payload);
    }

    /*
     * Load a (64b) js::Value from 'address' into 'type' and 'payload', and
     * return a label which can be used by
     * Repatcher::patchAddressOffsetForValue to patch the address offset.
     */
    Label loadValueWithAddressOffsetPatch(Address address, RegisterID type, RegisterID payload) {
        return loadValueAsComponents(address, type, payload);
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

    /*
     * Store a (64b) js::Value from 'type' and 'payload' into 'address', and
     * return a label which can be used by
     * Repatcher::patchAddressOffsetForValueStore to patch the address offset.
     */
    DataLabel32 storeValueWithAddressOffsetPatch(RegisterID type, RegisterID payload, Address address) {
        move(type, Registers::ValueReg);
        orPtr(payload, Registers::ValueReg);
        return storePtrWithAddressOffsetPatch(Registers::ValueReg, address);
    }

    /* Overload for constant type. */
    DataLabel32 storeValueWithAddressOffsetPatch(ImmTag type, RegisterID payload, Address address) {
        move(type, Registers::ValueReg);
        orPtr(payload, Registers::ValueReg);
        return storePtrWithAddressOffsetPatch(Registers::ValueReg, address);
    }

    /* Overload for constant type and constant data. */
    DataLabel32 storeValueWithAddressOffsetPatch(const Value &v, Address address) {
        move(ImmPtr(JSVAL_TO_IMPL(v).asPtr), Registers::ValueReg);
        return storePtrWithAddressOffsetPatch(Registers::ValueReg, valueOf(address));
    }

    /* Overloaded for store with value remat info. */
    DataLabel32 storeValueWithAddressOffsetPatch(const ValueRemat &vr, Address address) {
        JS_ASSERT(!vr.isFPRegister());
        if (vr.isConstant()) {
            return storeValueWithAddressOffsetPatch(vr.value(), address);
        } else if (vr.isTypeKnown()) {
            ImmType type(vr.knownType());
            RegisterID data(vr.dataReg());
            return storeValueWithAddressOffsetPatch(type, data, address);
        } else {
            RegisterID type(vr.typeReg());
            RegisterID data(vr.dataReg());
            return storeValueWithAddressOffsetPatch(type, data, address);
        }
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
        storePtr(Imm64(JSVAL_TO_IMPL(v).asBits), valueOf(address));
    }

    template <typename T>
    void storeValue(const ValueRemat &vr, T address) {
        if (vr.isConstant())
            storeValue(vr.value(), address);
        else if (vr.isFPRegister())
            storeDouble(vr.fpReg(), address);
        else if (vr.isTypeKnown())
            storeValueFromComponents(ImmType(vr.knownType()), vr.dataReg(), address);
        else
            storeValueFromComponents(vr.typeReg(), vr.dataReg(), address);
    }

    template <typename T>
    Jump guardNotHole(T address) {
        loadTypeTag(address, Registers::ValueReg);
        return branchPtr(Equal, Registers::ValueReg, ImmType(JSVAL_TYPE_MAGIC));
    }

    void loadPrivate(Address privAddr, RegisterID to) {
        loadPtr(privAddr, to);
        lshiftPtr(Imm32(1), to);
    }

    void loadObjPrivate(RegisterID base, RegisterID to, uint32_t nfixed) {
        Address priv(base, JSObject::getPrivateDataOffset(nfixed));
        loadPtr(priv, to);
    }

    Jump testNull(Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_NULL));
    }

    Jump testNull(Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testNull(cond, Registers::ValueReg);
    }

    Jump testUndefined(Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_UNDEFINED));
    }

    Jump testUndefined(Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testUndefined(cond, Registers::ValueReg);
    }

    Jump testInt32(Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testInt32(Condition cond, Address address) {
        loadTypeTag(address, Registers::ValueReg);
        return testInt32(cond, Registers::ValueReg);
    }

    Jump testNumber(Condition cond, RegisterID reg) {
        cond = (cond == Equal) ? Below : AboveOrEqual;
        return branchPtr(cond, reg,
                         ImmTag(JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET));
    }

    Jump testNumber(Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testNumber(cond, Registers::ValueReg);
    }

    Jump testPrimitive(Condition cond, RegisterID reg) {
        cond = (cond == Equal) ? Below : AboveOrEqual;
        return branchPtr(cond, reg,
                         ImmTag(JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET));
    }

    Jump testPrimitive(Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testPrimitive(cond, Registers::ValueReg);
    }

    Jump testObject(Condition cond, RegisterID reg) {
        cond = (cond == Equal) ? AboveOrEqual : Below;
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testObject(Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testObject(cond, Registers::ValueReg);
    }

    Jump testGCThing(RegisterID reg) {
        return branchPtr(AboveOrEqual, reg, ImmTag(JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET));
    }

    Jump testGCThing(Address address) {
        loadValue(address, Registers::ValueReg);
        return branchPtr(AboveOrEqual, Registers::ValueReg,
                         ImmTag(JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET));
    }

    Jump testDouble(Condition cond, RegisterID reg) {
        cond = (cond == Equal) ? BelowOrEqual : Above;
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_MAX_DOUBLE));
    }

    Jump testDouble(Condition cond, Address address) {
        loadValue(address, Registers::ValueReg);
        return testDouble(cond, Registers::ValueReg);
    }

    Jump testBoolean(Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_BOOLEAN));
    }

    Jump testBoolean(Condition cond, Address address) {
        loadTypeTag(address, Registers::ValueReg);
        return testBoolean(cond, Registers::ValueReg);
    }

    Jump testMagic(Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_MAGIC));
    }

    Jump testString(Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmTag(JSVAL_SHIFTED_TAG_STRING));
    }

    Jump testString(Condition cond, Address address) {
        loadTypeTag(address, Registers::ValueReg);
        return testString(cond, Registers::ValueReg);
    }

    Jump testPrivate(Condition cond, Address address, void *ptr) {
        uint64_t valueBits = PrivateValue(ptr).asRawBits();
        return branchPtr(cond, address, ImmPtr((void *) valueBits));
    }

    void compareValue(Address one, Address two, RegisterID T0, RegisterID T1,
                      Vector<Jump> *mismatches) {
        loadValue(one, T0);
        mismatches->append(branchPtr(NotEqual, T0, two));
    }

    void breakDouble(FPRegisterID srcDest, RegisterID typeReg, RegisterID dataReg) {
        m_assembler.movq_rr(srcDest, typeReg);
        move(Registers::PayloadMaskReg, dataReg);
        andPtr(typeReg, dataReg);
        xorPtr(dataReg, typeReg);
    }

    void fastLoadDouble(RegisterID dataReg, RegisterID typeReg, FPRegisterID fpReg) {
        move(typeReg, Registers::ValueReg);
        orPtr(dataReg, Registers::ValueReg);
        m_assembler.movq_rr(Registers::ValueReg, fpReg);
    }

    void loadStaticDouble(const double *dp, FPRegisterID dest, RegisterID scratch) {
        union DoublePun {
            double d;
            uint64_t u;
        } pun;
        pun.d = *dp;
        move(ImmPtr(reinterpret_cast<void*>(pun.u)), scratch);
        m_assembler.movq_rr(scratch, dest);
    }

    template <typename T>
    Jump fastArrayLoadSlot(T address, bool holeCheck,
                           MaybeRegisterID typeReg, RegisterID dataReg)
    {
        Jump notHole;
        if (typeReg.isSet()) {
            loadValueAsComponents(address, typeReg.reg(), dataReg);
            if (holeCheck)
                notHole = branchPtr(Equal, typeReg.reg(), ImmType(JSVAL_TYPE_MAGIC));
        } else {
            if (holeCheck) {
                loadTypeTag(address, Registers::ValueReg);
                notHole = branchPtr(Equal, Registers::ValueReg, ImmType(JSVAL_TYPE_MAGIC));
            }
            loadPayload(address, dataReg);
        }
        return notHole;
    }
};

typedef PunboxAssembler ValueAssembler;

} /* namespace mjit */
} /* namespace js */

#endif

