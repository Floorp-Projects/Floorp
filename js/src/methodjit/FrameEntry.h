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

#if !defined jsjaeger_valueinfo_h__ && defined JS_METHODJIT
#define jsjaeger_valueinfo_h__

#include "jsapi.h"
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
    bool isConstant() const {
        return data.isConstant();
    }

    const jsval_layout &getConstant() const {
        JS_ASSERT(isConstant());
        return v_;
    }

    const Value &getValue() const {
        JS_ASSERT(isConstant());
        return Valueify(JSVAL_FROM_LAYOUT(v_));
    }

    bool isTypeKnown() const {
        return type.isConstant();
    }

    JSValueType getKnownType() const {
        JS_ASSERT(isTypeKnown());
        return knownType;
    }

#if defined JS_NUNBOX32
    JSValueTag getKnownTag() const {
        return v_.s.tag;
    }
#elif defined JS_PUNBOX64
    JSValueShiftedTag getKnownShiftedTag() const {
        return JSValueShiftedTag(v_.asBits & JSVAL_TAG_MASK);
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

#if defined JS_NUNBOX32
    uint32 getPayload32() const {
        //JS_ASSERT(!Valueify(v_.asBits).isDouble() || type.synced());
        return v_.s.payload.u32;
    }
#elif defined JS_PUNBOX64
    uint64 getPayload64() const {
        return v_.asBits & JSVAL_PAYLOAD_MASK;
    }
#endif

    bool isCachedNumber() const {
        return isNumber;
    }

  private:
    void setType(JSValueType type_) {
        type.setConstant();
#if defined JS_NUNBOX32
        v_.s.tag = JSVAL_TYPE_TO_TAG(type_);
#elif defined JS_PUNBOX64
        v_.asBits &= JSVAL_PAYLOAD_MASK;
        v_.asBits |= JSVAL_TYPE_TO_SHIFTED_TAG(type_);
#endif
        knownType = type_;
        JS_ASSERT(!isNumber);
    }

    void track(uint32 index) {
        clear();
        index_ = index;
        tracked = true;
    }

    void clear() {
        copied = false;
        copy = NULL;
        isNumber = false;
    }

    uint32 trackerIndex() {
        return index_;
    }

    /*
     * Marks the FE as unsynced & invalid.
     */
    void resetUnsynced() {
        clear();
        type.unsync();
        data.unsync();
#ifdef DEBUG
        type.invalidate();
        data.invalidate();
#endif
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
    void setConstant(const jsval &v) {
        clear();
        type.unsync();
        data.unsync();
        type.setConstant();
        data.setConstant();
        v_.asBits = JSVAL_BITS(v);
        Value cv = Valueify(v);
        if (cv.isDouble())
            knownType = JSVAL_TYPE_DOUBLE;
        else
            knownType = cv.extractNonDoubleType();
    }

    bool isCopied() const {
        return copied;
    }

    void setCopied() {
        JS_ASSERT(!isCopy());
        copied = true;
    }

    bool isCopy() const {
        return !!copy;
    }

    FrameEntry *copyOf() const {
        JS_ASSERT(isCopy());
        return copy;
    }

    void setNotCopied() {
        copied = false;
    }

    /*
     * Set copy index.
     */
    void setCopyOf(FrameEntry *fe) {
        JS_ASSERT_IF(fe, !fe->isConstant());
        JS_ASSERT(!isCopied());
        copy = fe;
    }

    inline bool isTracked() const {
        return tracked;
    }

    inline void untrack() {
        tracked = false;
    }

  private:
    JSValueType knownType;
    jsval_layout v_;
    RematInfo  type;
    RematInfo  data;
    uint32     index_;
    FrameEntry *copy;
    bool       copied;
    bool       isNumber;
    bool       tracked;
    char       padding[1];
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_valueinfo_h__ */

