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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#ifndef __nanojit_NativeCommon__
#define __nanojit_NativeCommon__

namespace nanojit
{
    // In debug builds, Register is defined as a non-integer type to avoid
    // possible mix-ups with RegisterMask and integer offsets.  In non-debug
    // builds, it's defined as an integer just in case some compilers fail to
    // optimize single-element structs in the obvious way.
    //
    // Note that in either case, a Register can be initialized like so:
    //
    //   Register r = { 0 };
    //
    // In the debug case it's a struct initializer, in the non-debug case it's
    // a scalar initializer.
    //
    // XXX: The exception to all the above is that if NJ_USE_UINT32_REGISTER
    // is defined, the back-end is responsible for defining its own Register
    // type, which will probably be an enum.  This is just to give all the
    // back-ends a chance to transition smoothly.
#if defined(NJ_USE_UINT32_REGISTER)
    #define REGNUM(r) (r)

#elif defined(DEBUG) || defined(__SUNPRO_CC)
    // Always use struct declaration for 'Register' with
    // Solaris Studio C++ compiler, because it has a bug:
    // Scalar type can not be initialized by '{1}'.
    // See bug 603560.

    struct Register {
        uint32_t n;     // the register number
    };

    static inline uint32_t REGNUM(Register r) {
        return r.n;
    }

    static inline Register operator+(Register r, int c)
    {
        r.n += c;
        return r;
    }

    static inline bool operator==(Register r1, Register r2)
    {
        return r1.n == r2.n;
    }

    static inline bool operator!=(Register r1, Register r2)
    {
        return r1.n != r2.n;
    }

    static inline bool operator<=(Register r1, Register r2)
    {
        return r1.n <= r2.n;
    }

    static inline bool operator<(Register r1, Register r2)
    {
        return r1.n < r2.n;
    }

    static inline bool operator>=(Register r1, Register r2)
    {
        return r1.n >= r2.n;
    }

    static inline bool operator>(Register r1, Register r2)
    {
        return r1.n > r2.n;
    }
#else
    typedef uint32_t Register;

    static inline uint32_t REGNUM(Register r) {
        return r;
    }
#endif
} // namespace nanojit

#endif // __nanojit_NativeCommon__
