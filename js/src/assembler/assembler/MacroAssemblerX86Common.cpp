/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "assembler/wtf/Platform.h"

/* SSE checks only make sense on Intel platforms. */
#if WTF_CPU_X86 || WTF_CPU_X86_64

#include "assembler/assembler/MacroAssemblerX86Common.h"

using namespace JSC;
MacroAssemblerX86Common::SSECheckState MacroAssemblerX86Common::s_sseCheckState = NotCheckedSSE;

#ifdef DEBUG
bool MacroAssemblerX86Common::s_floatingPointDisabled = false;
bool MacroAssemblerX86Common::s_SSE3Disabled = false;
bool MacroAssemblerX86Common::s_SSE4Disabled = false;
#endif

#endif /* WTF_CPU_X86 || WTF_CPU_X86_64 */

