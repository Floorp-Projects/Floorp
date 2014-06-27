/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef assembler_assembler_MacroAssemblerSparc_h
#define assembler_assembler_MacroAssemblerSparc_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER && WTF_CPU_SPARC

namespace JSC {

class MacroAssemblerSparc {
public:
    static bool supportsFloatingPoint() { return true; }
};

}

#endif // ENABLE(ASSEMBLER) && CPU(SPARC)

#endif /* assembler_assembler_MacroAssemblerSparc_h */
