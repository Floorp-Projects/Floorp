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


#ifndef __nanojit_RegAlloc__
#define __nanojit_RegAlloc__


namespace nanojit
{
    class RegAlloc
    {
    public:
        RegAlloc()
        {
            clear();
        }

        void clear()
        {
            VMPI_memset(this, 0, sizeof(*this));
        }

        bool isFree(Register r) const
        {
            return (free & rmask(r)) != 0;
        }

        void addFree(Register r)
        {
            NanoAssert(!isFree(r));
            free |= rmask(r);
        }

        void removeFree(Register r)
        {
            NanoAssert(isFree(r));
            free &= ~rmask(r);
        }

        void addActive(Register r, LIns* v)
        {
            //  Count++;
            NanoAssert(v);
            NanoAssert(active[REGNUM(r)] == NULL);
            active[REGNUM(r)] = v;
            useActive(r);
        }

        void useActive(Register r)
        {
            NanoAssert(active[REGNUM(r)] != NULL);
            usepri[REGNUM(r)] = priority++;
        }

        void removeActive(Register r)
        {
            //registerReleaseCount++;
            NanoAssert(active[REGNUM(r)] != NULL);

            // remove the given register from the active list
            active[REGNUM(r)] = NULL;
        }

        void retire(Register r)
        {
            NanoAssert(active[REGNUM(r)] != NULL);
            active[REGNUM(r)] = NULL;
            free |= rmask(r);
        }

        int32_t getPriority(Register r) {
            NanoAssert(active[REGNUM(r)]);
            return usepri[REGNUM(r)];
        }

        LIns* getActive(Register r) const {
            return active[REGNUM(r)];
        }

        // Return a mask containing the active registers.  For each register
        // in this set, getActive(register) will be a nonzero LIns pointer.
        RegisterMask activeMask() const {
            return ~free & managed;
        }

        debug_only( bool        isConsistent(Register r, LIns* v) const; )

        // Some basics:
        //
        // - 'active' indicates which registers are active at a particular
        //   point, and for each active register, which instruction
        //   defines the value it holds.  At the start of register
        //   allocation no registers are active.
        //
        // - 'free' indicates which registers are free at a particular point
        //   and thus available for use.  At the start of register
        //   allocation most registers are free;  those that are not
        //   aren't available for general use, e.g. the stack pointer and
        //   frame pointer registers.
        //
        // - 'managed' is exactly this list of initially free registers,
        //   ie. the registers managed by the register allocator.
        //
        // - Each LIns has a "reservation" which includes a register value,
        //   'reg'.  Combined with 'active', this provides a two-way
        //   mapping between registers and LIR instructions.
        //
        // - Invariant 1: each register must be in exactly one of the
        //   following states at all times:  unmanaged, free, or active.
        //   In terms of the relevant fields:
        //
        //   * A register in 'managed' must be in 'active' or 'free' but
        //     not both.
        //
        //   * A register not in 'managed' must be in neither 'active' nor
        //     'free'.
        //
        // - Invariant 2: the two-way mapping between active registers and
        //   their defining instructions must always hold in both
        //   directions and be unambiguous.  More specifically:
        //
        //   * An LIns can appear at most once in 'active'.
        //
        //   * An LIns named by 'active[R]' must have an in-use
        //     reservation that names R.
        //
        //   * And vice versa:  an LIns with an in-use reservation that
        //     names R must be named by 'active[R]'.
        //
        //   * If an LIns's reservation names 'deprecated_UnknownReg' then LIns
        //     should not be in 'active'.
        //
        LIns*           active[LastRegNum + 1]; // active[REGNUM(r)] = LIns that defines r
        int32_t         usepri[LastRegNum + 1]; // used priority. lower = more likely to spill.
        RegisterMask    free;       // Registers currently free.
        RegisterMask    managed;    // Registers under management (invariant).
        int32_t         priority;

        DECLARE_PLATFORM_REGALLOC()
    };

    // Return the lowest numbered Register in mask.
    inline Register lsReg(RegisterMask mask) {
        // This is faster than it looks; we rely on the C++ optimizer
        // to strip the dead branch and inline just one alternative.
        Register r = { (sizeof(RegisterMask) == 4) ? lsbSet32(mask) : lsbSet64(mask) };
        return r;
    }

    // Return the highest numbered Register in mask.
    inline Register msReg(RegisterMask mask) {
        // This is faster than it looks; we rely on the C++ optimizer
        // to strip the dead branch and inline just one alternative.
        Register r = { (sizeof(RegisterMask) == 4) ? msbSet32(mask) : msbSet64(mask) };
        return r;
    }

    // Clear bit r in mask, then return lsReg(mask).
    inline Register nextLsReg(RegisterMask& mask, Register r) {
        return lsReg(mask &= ~rmask(r));
    }

    // Clear bit r in mask, then return msReg(mask).
    inline Register nextMsReg(RegisterMask& mask, Register r) {
        return msReg(mask &= ~rmask(r));
    }
}
#endif // __nanojit_RegAlloc__
