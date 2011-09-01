/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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

#ifndef jsion_ion_lowering_arm_inl_h__
#define jsion_ion_lowering_arm_inl_h__

#include "ion/IonLIR-inl.h"

namespace js {
namespace ion {

// Returns the virtual register of a js::Value-defining instruction. This is
// abstracted because MBox is a special value-returning instruction that
// redefines its input payload if its input is not constant. Therefore, it is
// illegal to request a box's payload by adding VREG_DATA_OFFSET to its raw id.
static inline uint32
VirtualRegisterOfPayload(MDefinition *mir)
{
    if (mir->isBox()) {
        MDefinition *inner = mir->toBox()->getOperand(0);
        if (!inner->isConstant() && inner->type() != MIRType_Double)
            return inner->id();
    }
    return mir->id() + VREG_DATA_OFFSET;
}

LUse
LIRGeneratorARM::useType(MDefinition *mir, LUse::Policy policy)
{
    JS_ASSERT(mir->id());
    JS_ASSERT(mir->type() == MIRType_Value);

    return LUse(mir->id() + VREG_TYPE_OFFSET, policy);
}

LUse
LIRGeneratorARM::usePayload(MDefinition *mir, LUse::Policy policy)
{
    JS_ASSERT(mir->id());
    JS_ASSERT(mir->type() == MIRType_Value);

    return LUse(VirtualRegisterOfPayload(mir), policy);
}

LUse
LIRGeneratorARM::usePayloadInRegister(MDefinition *mir)
{
    return usePayload(mir, LUse::REGISTER);
}

bool
LIRGeneratorARM::fillBoxUses(LInstruction *lir, size_t n, MDefinition *mir)
{
    if (!ensureDefined(mir))
        return false;
    lir->getOperand(n)->toUse()->setVirtualRegister(mir->id() + VREG_TYPE_OFFSET);
    lir->getOperand(n + 1)->toUse()->setVirtualRegister(VirtualRegisterOfPayload(mir));
    return true;
}

} // namespace ion
} // namespace js

#endif // jsion_ion_lowering_arm_h__

