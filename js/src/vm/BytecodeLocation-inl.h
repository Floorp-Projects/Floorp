/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BytecodeLocation_inl_h
#define vm_BytecodeLocation_inl_h

#include "vm/BytecodeLocation.h"

#include "vm/JSScript.h"

namespace js {

inline bool
BytecodeLocation::isValid(const JSScript* script) const {
    // Note: Don't create a new BytecodeLocation during the implementation of this, as it
    // is used in the constructor, and will recurse forever.
    return script->contains(*this) ||toRawBytecode() == script->codeEnd();
}

inline bool
BytecodeLocation::isInBounds(const JSScript* script) const {
    return script->contains(*this);
}

}

#endif
