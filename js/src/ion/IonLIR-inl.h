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

#ifndef jsion_lir_inl_h__
#define jsion_lir_inl_h__

namespace js {
namespace ion {

#define LIROP(name)                                                         \
    L##name *LInstruction::to##name()                                       \
    {                                                                       \
        JS_ASSERT(is##name());                                              \
        return static_cast<L##name *>(this);                                \
    }
    LIR_OPCODE_LIST(LIROP)
#undef LIROP

#define LALLOC_CAST(type)                                                   \
    L##type *LAllocation::to##type() {                                      \
        JS_ASSERT(is##type());                                              \
        return static_cast<L##type *>(this);                                \
    }
#define LALLOC_CONST_CAST(type)                                             \
    const L##type *LAllocation::to##type() const {                          \
        JS_ASSERT(is##type());                                              \
        return static_cast<const L##type *>(this);                          \
    }

LALLOC_CAST(Use)
LALLOC_CONST_CAST(Use)
LALLOC_CONST_CAST(GeneralReg)
LALLOC_CONST_CAST(FloatReg)
LALLOC_CONST_CAST(StackSlot)
LALLOC_CONST_CAST(Argument)
LALLOC_CONST_CAST(ConstantIndex)

#undef LALLOC_CAST

#ifdef JS_NUNBOX32
static inline signed
OffsetToOtherHalfOfNunbox(LDefinition::Type type)
{
    JS_ASSERT(type == LDefinition::TYPE || type == LDefinition::PAYLOAD);
    signed offset = (type == LDefinition::TYPE)
                    ? PAYLOAD_INDEX - TYPE_INDEX
                    : TYPE_INDEX - PAYLOAD_INDEX;
    return offset;
}

static inline void
AssertTypesFormANunbox(LDefinition::Type type1, LDefinition::Type type2)
{
    JS_ASSERT((type1 == LDefinition::TYPE && type2 == LDefinition::PAYLOAD) ||
              (type2 == LDefinition::TYPE && type1 == LDefinition::PAYLOAD));
}

static inline unsigned
OffsetOfNunboxSlot(LDefinition::Type type)
{
    if (type == LDefinition::PAYLOAD)
        return NUNBOX32_PAYLOAD_OFFSET / STACK_SLOT_SIZE;
    return NUNBOX32_TYPE_OFFSET / STACK_SLOT_SIZE;
}

// Note that stack indexes for LStackSlot are modelled backwards, so a
// double-sized slot starting at 2 has its next word at 1, *not* 3.
static inline unsigned
BaseOfNunboxSlot(LDefinition::Type type, unsigned slot)
{
    if (type == LDefinition::PAYLOAD)
        return slot + (NUNBOX32_PAYLOAD_OFFSET / STACK_SLOT_SIZE);
    return slot + (NUNBOX32_TYPE_OFFSET / STACK_SLOT_SIZE);
}
#endif

} // namespace ion
} // namespace js

#endif // jsion_lir_inl_h__

