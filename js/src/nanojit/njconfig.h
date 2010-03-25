/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __njconfig_h__
#define __njconfig_h__

#include "avmplus.h"

// Do not include nanojit.h here; this file should be usable without it.

#ifdef FEATURE_NANOJIT

namespace nanojit
{
    /***
     * A struct used to configure the assumptions that Assembler can make when
     * generating code. The ctor will fill in all fields with the most reasonable
     * values it can derive from compiler flags and/or runtime detection, but
     * the embedder is free to override any or all of them as it sees fit.
     * Using the ctor-provided default setup is guaranteed to provide a safe
     * runtime environment (though perhaps suboptimal in some cases), so an embedder
     * should replace these values with great care.
     *
     * Note that although many fields are used on only specific architecture(s),
     * this struct is deliberately declared without ifdef's for them, so (say) ARM-specific
     * fields are declared everywhere. This reduces build dependencies (so that this
     * files does not require nanojit.h to be included beforehand) and also reduces
     * clutter in this file; the extra storage space required is trivial since most
     * fields are single bits.
     */
    struct Config
    {
    public:
        // fills in reasonable default values for all fields.
        Config();

        // ARM architecture to assume when generate instructions for (currently, 5 <= arm_arch <= 7)
        uint8_t arm_arch;

        // If true, use CSE.
        uint32_t cseopt:1;

        // Can we use SSE2 instructions? (x86-only)
        uint32_t i386_sse2:1;

        // Can we use cmov instructions? (x86-only)
        uint32_t i386_use_cmov:1;

        // Should we use a virtual stack pointer? (x86-only)
        uint32_t i386_fixed_esp:1;

        // Whether or not to generate VFP instructions. (ARM only)
        uint32_t arm_vfp:1;

        // @todo, document me
        uint32_t arm_show_stats:1;

        // If true, use softfloat for all floating point operations,
        // whether or not an FPU is present. (ARM only for now, but might also includes MIPS in the future)
        uint32_t soft_float:1;
    };
}

#endif // FEATURE_NANOJIT
#endif // __njconfig_h__
