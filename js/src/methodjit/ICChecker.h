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
 *   David Mandelin <dmandelin@mozilla.com>
 *   David Anderson <danderson@mozilla.com>
 *   Chris Leary <cdleary@mozilla.com>
 *   Jacob Bramley <Jacob.Bramely@arm.com>
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

#if !defined jsjaeger_icchecker_h__ && defined JS_METHODJIT
#define jsjaeger_icchecker_h__

#include "assembler/assembler/MacroAssembler.h"

namespace js {
namespace mjit {

#if defined DEBUG && defined JS_CPU_ARM
static inline void
CheckInstMask(void *addr, uint32 mask, uint32 expected)
{
    uint32 inst = *static_cast<uint32 *>(addr);
    JS_ASSERT((inst & mask) == expected);
}

static inline void
CheckIsLDR(JSC::CodeLocationLabel label, uint8 rd)
{
    JS_ASSERT((rd & 0xf) == rd);
    CheckInstMask(label.executableAddress(), 0xfc50f000, 0xe4100000 | (rd << 12));
}

static inline void
CheckIsBLX(JSC::CodeLocationLabel label, uint8 rsrc)
{
    JS_ASSERT((rsrc & 0xf) == rsrc);
    CheckInstMask(label.executableAddress(), 0xfff000ff, 0xe1200030 | rsrc);
}

static inline void
CheckIsStubCall(JSC::CodeLocationLabel label)
{
    CheckIsLDR(label.labelAtOffset(-4), JSC::ARMRegisters::ip);
    CheckIsLDR(label.labelAtOffset(0), JSC::ARMRegisters::r8);
    CheckIsBLX(label.labelAtOffset(4), JSC::ARMRegisters::r8);
}
#else
static inline void CheckIsStubCall(JSC::CodeLocationLabel label) {}
#endif

} /* namespace mjit */
} /* namespace js */

#endif
