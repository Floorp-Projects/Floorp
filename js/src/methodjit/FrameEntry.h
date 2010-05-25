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
#include "MachineRegs.h"
#include "assembler/assembler/MacroAssembler.h"

namespace js {
namespace mjit {

struct RematInfo {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    /* Physical location. */
    enum PhysLoc {
        /* Backed by another entry in the stack. */
        PhysLoc_Copy,

        /* Backing bits are known at compile time. */
        PhysLoc_Constant,

        /* Backing bits are in a register. */
        PhysLoc_Register,

        /* Backing bits are in memory. */
        PhysLoc_Memory
    };

    void setRegister(RegisterID reg, bool synced) {
        reg_ = reg;
        location_ = PhysLoc_Register;
        synced_ = synced;
    }

    bool isCopy() { return location_ == PhysLoc_Copy; }
    void setMemory() { synced_ = true; }
    void setConstant() { location_ = PhysLoc_Constant; }
    void unsync() { synced_ = false; }
    bool isConstant() { return location_ == PhysLoc_Constant; }
    bool inRegister() { return location_ == PhysLoc_Register; }
    RegisterID reg() { return reg_; }

    RegisterID reg_;
    PhysLoc    location_;
    bool       synced_;
};

class FrameEntry
{
    friend class FrameState;

  public:
    bool isConstant() {
        return data.isConstant();
    }

    const jsval &getConstant() {
        JS_ASSERT(isConstant());
        return v_;
    }

    bool isTypeConstant() {
        return type.isConstant();
    }

    uint32 getTypeTag() {
        return v_.mask;
    }

    uint32 copyOf() {
        JS_ASSERT(type.isCopy() || data.isCopy());
        return index_;
    }

  private:
    void setConstant(const jsval &v) {
        type.setConstant();
        type.unsync();
        data.setConstant();
        data.unsync();
        v_ = v;
    }

  private:
    RematInfo  type;
    RematInfo  data;
    jsval      v_;
    uint32     index_;
    uint32     copies;
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_valueinfo_h__ */

