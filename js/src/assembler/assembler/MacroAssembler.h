/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef assembler_assembler_MacroAssembler_h
#define assembler_assembler_MacroAssembler_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER

#if WTF_CPU_ARM_THUMB2
#include "assembler/assembler/MacroAssemblerARMv7.h"
namespace JSC { typedef MacroAssemblerARMv7 MacroAssembler; }

#elif WTF_CPU_ARM_TRADITIONAL
#include "assembler/assembler/MacroAssemblerARM.h"
namespace JSC { typedef MacroAssemblerARM MacroAssembler; }

#elif WTF_CPU_MIPS
#include "assembler/assembler/MacroAssemblerMIPS.h"
namespace JSC { typedef MacroAssemblerMIPS MacroAssembler; }

#elif WTF_CPU_X86
#include "assembler/assembler/MacroAssemblerX86.h"
namespace JSC { typedef MacroAssemblerX86 MacroAssembler; }

#elif WTF_CPU_X86_64
#include "assembler/assembler/MacroAssemblerX86_64.h"
namespace JSC { typedef MacroAssemblerX86_64 MacroAssembler; }

#elif WTF_CPU_SPARC
#include "assembler/assembler/MacroAssemblerSparc.h"
namespace JSC { typedef MacroAssemblerSparc MacroAssembler; }

#else
#error "The MacroAssembler is not supported on this platform."
#endif

#endif // ENABLE(ASSEMBLER)

#endif /* assembler_assembler_MacroAssembler_h */
