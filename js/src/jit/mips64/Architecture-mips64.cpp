/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/mips64/Architecture-mips64.h"

#include "jit/RegisterSets.h"

namespace js {
namespace jit {

FloatRegisters::Encoding
FloatRegisters::FromName(const char* name)
{
    for (size_t i = 0; i < Total; i++) {
        if (strcmp(GetName(Encoding(i)), name) == 0)
            return Encoding(i);
    }

    return Invalid;
}

FloatRegister
FloatRegister::singleOverlay() const
{
    MOZ_ASSERT(!isInvalid());
    if (kind_ == Codes::Double)
        return FloatRegister(reg_, Codes::Single);
    return *this;
}

FloatRegister
FloatRegister::doubleOverlay() const
{
    MOZ_ASSERT(!isInvalid());
    if (kind_ != Codes::Double)
        return FloatRegister(reg_, Codes::Double);
    return *this;
}

FloatRegisterSet
FloatRegister::ReduceSetForPush(const FloatRegisterSet& s)
{
    LiveFloatRegisterSet mod;
    for (FloatRegisterIterator iter(s); iter.more(); iter++) {
        if ((*iter).isSingle()) {
            // Even for single size registers save complete double register.
            mod.addUnchecked((*iter).doubleOverlay());
        } else {
            mod.addUnchecked(*iter);
        }
    }
    return mod.set();
}

uint32_t
FloatRegister::GetPushSizeInBytes(const FloatRegisterSet& s)
{
    FloatRegisterSet ss = s.reduceSetForPush();
    uint64_t bits = ss.bits();
    // We are only pushing double registers.
    MOZ_ASSERT((bits & 0xffffffff) == 0);
    uint32_t ret =  mozilla::CountPopulation32(bits >> 32) * sizeof(double);
    return ret;
}
uint32_t
FloatRegister::getRegisterDumpOffsetInBytes()
{
    return id() * sizeof(double);
}

} // namespace ion
} // namespace js

