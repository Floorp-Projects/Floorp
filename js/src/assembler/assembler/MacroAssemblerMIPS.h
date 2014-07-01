/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 MIPS Technologies, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MIPS TECHNOLOGIES, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL MIPS TECHNOLOGIES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef assembler_assembler_MacroAssemblerMIPS_h
#define assembler_assembler_MacroAssemblerMIPS_h

#if ENABLE(ASSEMBLER) && CPU(MIPS)

#include "assembler/wtf/Platform.h"

namespace JSC {

class MacroAssemblerMIPS {
public:
    static bool supportsFloatingPoint()
    {
#if WTF_MIPS_DOUBLE_FLOAT
        return true;
#else
        return false;
#endif
    }
};

}

#endif // ENABLE(ASSEMBLER) && CPU(MIPS)

#endif /* assembler_assembler_MacroAssemblerMIPS_h */
