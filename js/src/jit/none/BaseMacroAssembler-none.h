/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_none_BaseMacroAssembler_none_h
#define jit_none_BaseMacroAssembler_none_h

// Shim to act like a JSC MacroAssembler for platforms where no MacroAssembler exists.

// IonSpewer.h is included through MacroAssembler implementations for other
// platforms, so include it here to avoid inadvertent build bustage.
#include "jit/IonSpewer.h"

namespace JSC {

class MacroAssemblerNone
{
  public:
    static bool supportsFloatingPoint() { return false; }
};

} // namespace JSC

#endif /* jit_none_BaseMacroAssembler_none_h */
