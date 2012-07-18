/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_valueinfo_h__ && defined JS_METHODJIT
#define jsjaeger_valueinfo_h__

#include "jsapi.h"
#include "jsnum.h"
#include "jstypes.h"
#include "methodjit/MachineRegs.h"
#include "methodjit/RematInfo.h"
#include "assembler/assembler/MacroAssembler.h"

namespace js {
namespace mjit {

class FrameEntry
{
    friend class FrameState;
    friend class ImmutableSync;

  public:

    /* Accessors for entries which are known constants. */

    bool isConstant() const {
        if (isCopy())
            return false;
        return data.isConstant();
    }

    const jsval_layout &getConstant() const {
        JS_ASSERT(isConstant());
        return v_;
    }

    Value getValue() const {
        JS_ASSERT(isConstant());
        return IMPL_TO_JSVAL(v_);
    }

#if defined JS_NUNBOX32
    uint32_t getPayload() const {
        JS_ASSERT(isConstant());
        return v_.s.payload.u32;
    }
#elif defined JS_PUNBOX64
    uint64_t getPayload() const {
        JS_ASSERT(isConstant());
        return v_.asBits & JSVAL_PAYLOAD_MASK;
    }
#endif

    /* For a constant double FrameEntry, truncate to an int32. */
    void convertConstantDoubleOrBooleanToInt32(JSContext *cx) {
        JS_ASSERT(isConstant());
        JS_ASSERT(isType(JSVAL_TYPE_DOUBLE) || isType(JSVAL_TYPE_BOOLEAN));

        int32_t value;
        ToInt32(cx, getValue(), &value);

        Value newValue = Int32Value(value);
        setConstant(newValue);
    }

    /*
     * Accessors for entries whose type is known. Any entry can have a known
     * type, and constant entries must have one.
     */

    bool isTypeKnown() const {
        return backing()->type.isConstant();
    }

    /*
     * The known type should not be used in generated code if it is JSVAL_TYPE_DOUBLE.
     * In such cases either the value is constant, in memory or in a floating point register.
     */
    JSValueType getKnownType() const {
        JS_ASSERT(isTypeKnown());
        return backing()->knownType;
    }

#if defined JS_NUNBOX32
    JSValueTag getKnownTag() const {
        JS_ASSERT(backing()->v_.s.tag != JSVAL_TAG_CLEAR);
        return backing()->v_.s.tag;
    }
#elif defined JS_PUNBOX64
    JSValueShiftedTag getKnownTag() const {
        return JSValueShiftedTag(backing()->v_.asBits & JSVAL_TAG_MASK);
    }
#endif

    // Return true iff the type of this value is definitely known to be type_.
    bool isType(JSValueType type_) const {
        return isTypeKnown() && getKnownType() == type_;
    }

    // Return true iff the type of this value is definitely known not to be type_.
    bool isNotType(JSValueType type_) const {
        return isTypeKnown() && getKnownType() != type_;
    }

    // Return true if the type of this value is definitely type_, or is unknown
    // and thus potentially type_ at runtime.
    bool mightBeType(JSValueType type_) const {
        return !isNotType(type_);
    }

    /* Accessors for entries which are copies of other mutable entries. */

    bool isCopy() const { return !!copy; }
    bool isCopied() const { return copied != 0; }

    const FrameEntry *backing() const {
        return isCopy() ? copyOf() : this;
    }

    bool hasSameBacking(const FrameEntry *other) const {
        return backing() == other->backing();
    }

  private:
    void setType(JSValueType type_) {
        JS_ASSERT(!isCopy() && type_ != JSVAL_TYPE_UNKNOWN);
        type.setConstant();
#if defined JS_NUNBOX32
        v_.s.tag = JSVAL_TYPE_TO_TAG(type_);
#elif defined JS_PUNBOX64
        v_.asBits &= JSVAL_PAYLOAD_MASK;
        v_.asBits |= JSVAL_TYPE_TO_SHIFTED_TAG(type_);
#endif
        knownType = type_;
    }

    void track(uint32_t index) {
        copied = 0;
        copy = NULL;
        index_ = index;
        tracked = true;
    }

    void clear() {
        JS_ASSERT(copied == 0);
        if (copy) {
            JS_ASSERT(copy->copied != 0);
            copy->copied--;
            copy = NULL;
        }
    }

    uint32_t trackerIndex() {
        return index_;
    }

    /*
     * Marks the FE as unsynced & invalid.
     */
    void resetUnsynced() {
        clear();
        type.unsync();
        data.unsync();
        type.invalidate();
        data.invalidate();
    }

    /*
     * Marks the FE as synced & in memory.
     */
    void resetSynced() {
        clear();
        type.setMemory();
        data.setMemory();
    }

    /*
     * Marks the FE as having a constant.
     */
    void setConstant(const Value &v) {
        clear();
        type.unsync();
        data.unsync();
        type.setConstant();
        data.setConstant();
        v_ = JSVAL_TO_IMPL(v);
        if (v.isDouble())
            knownType = JSVAL_TYPE_DOUBLE;
        else
            knownType = v.extractNonDoubleType();
    }

    FrameEntry *copyOf() const {
        JS_ASSERT(isCopy());
        JS_ASSERT_IF(!copy->temporary, copy < this);
        return copy;
    }

    /*
     * Set copy index.
     */
    void setCopyOf(FrameEntry *fe) {
        clear();
        copy = fe;
        if (fe) {
            type.invalidate();
            data.invalidate();
            fe->copied++;
        }
    }

    inline bool isTracked() const {
        return tracked;
    }

    inline void untrack() {
        tracked = false;
    }

    inline bool dataInRegister(AnyRegisterID reg) const {
        JS_ASSERT(!copy);
        return reg.isReg()
            ? (data.inRegister() && data.reg() == reg.reg())
            : (data.inFPRegister() && data.fpreg() == reg.fpreg());
    }

  private:
    JSValueType knownType;
    jsval_layout v_;
    RematInfo  type;
    RematInfo  data;
    uint32_t   index_;
    FrameEntry *copy;
    bool       tracked;
    bool       temporary;

    /* Number of copies of this entry. */
    uint32_t   copied;

    /*
     * Offset of the last loop in which this entry was written or had a loop
     * register assigned.
     */
    uint32_t   lastLoop;
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_valueinfo_h__ */

