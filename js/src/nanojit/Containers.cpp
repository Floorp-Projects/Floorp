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

#include "nanojit.h"

#ifdef FEATURE_NANOJIT

namespace nanojit
{
    BitSet::BitSet(Allocator& allocator, int nbits)
        : allocator(allocator)
        , cap((nbits+63)>>6)
        , bits((int64_t*)allocator.alloc(cap * sizeof(int64_t)))
    {
        reset();
    }

    void BitSet::reset()
    {
        for (int i=0, n=cap; i < n; i++)
            bits[i] = 0;
    }

    bool BitSet::setFrom(BitSet& other)
    {
        int c = other.cap;
        if (c > cap)
            grow(c);
        int64_t *bits = this->bits;
        int64_t *otherbits = other.bits;
        int64_t newbits = 0;
        for (int i=0; i < c; i++) {
            int64_t b = bits[i];
            int64_t b2 = otherbits[i];
            newbits |= b2 & ~b; // bits in b2 that are not in b
            bits[i] = b|b2;
        }
        return newbits != 0;
    }

    /** keep doubling the bitset length until w fits */
    void BitSet::grow(int w)
    {
        int cap2 = cap;
        do {
            cap2 <<= 1;
        } while (w >= cap2);
        int64_t *bits2 = (int64_t*) allocator.alloc(cap2 * sizeof(int64_t));
        int j=0;
        for (; j < cap; j++)
            bits2[j] = bits[j];
        for (; j < cap2; j++)
            bits2[j] = 0;
        cap = cap2;
        bits = bits2;
    }
}

#endif // FEATURE_NANOJIT
