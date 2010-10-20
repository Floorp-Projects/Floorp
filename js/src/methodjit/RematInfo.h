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

#if !defined jsjaeger_remat_h__ && defined JS_METHODJIT
#define jsjaeger_remat_h__

#include "jscntxt.h"
#include "assembler/assembler/MacroAssembler.h"

/* Lightweight version of FrameEntry. */
struct ValueRemat {
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    union {
        struct {
            union {
                RegisterID  reg;
                JSValueType knownType;
            } type;
            RegisterID data : 5;
            bool isTypeKnown : 1;
        } s;
        jsval v;
    } u;
    bool isConstant : 1;
    bool isDataSynced : 1;
    bool isTypeSynced : 1;

    RegisterID dataReg() {
        JS_ASSERT(!isConstant);
        return u.s.data;
    }

    RegisterID typeReg() {
        JS_ASSERT(!isConstant && !u.s.isTypeKnown);
        return u.s.type.reg;
    }

    bool isType(JSValueType type_) const {
        JS_ASSERT(!isConstant);
        return u.s.isTypeKnown && u.s.type.knownType == type_;
    }
};

/*
 * Describes how to rematerialize a value during compilation.
 */
struct RematInfo {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    enum SyncState {
        SYNCED,
        UNSYNCED
    };

    enum RematType {
        TYPE,
        DATA
    };

    /* Physical location. */
    enum PhysLoc {
        /*
         * Backing bits are in memory. No fast remat.
         */
        PhysLoc_Memory = 0,

        /* Backing bits are known at compile time. */
        PhysLoc_Constant,

        /* Backing bits are in a register. */
        PhysLoc_Register,

        /* Backing bits are invalid/unknown. */
        PhysLoc_Invalid
    };

    void setRegister(RegisterID reg) {
        reg_ = reg;
        location_ = PhysLoc_Register;
    }

    RegisterID reg() const {
        JS_ASSERT(inRegister());
        return reg_;
    }

    void setMemory() {
        location_ = PhysLoc_Memory;
        sync_ = SYNCED;
    }

    void invalidate() {
        location_ = PhysLoc_Invalid;
    }

    void setConstant() { location_ = PhysLoc_Constant; }

    bool isConstant() const { return location_ == PhysLoc_Constant; }
    bool inRegister() const { return location_ == PhysLoc_Register; }
    bool inMemory() const { return location_ == PhysLoc_Memory; }
    bool synced() const { return sync_ == SYNCED; }
    void sync() {
        JS_ASSERT(!synced());
        sync_ = SYNCED;
    }
    void unsync() {
        sync_ = UNSYNCED;
    }

    void inherit(const RematInfo &other) {
        reg_ = other.reg_;
        location_ = other.location_;
    }

  private:
    /* Set if location is PhysLoc_Register. */
    RegisterID reg_;

    /* Remat source. */
    PhysLoc location_;

    /* Sync state. */
    SyncState sync_;
};

#endif

