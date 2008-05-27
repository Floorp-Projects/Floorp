/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
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

#ifndef __txdouble_h__
#define __txdouble_h__

//A trick to handle IEEE floating point exceptions on FreeBSD - E.D.
#ifdef __FreeBSD__
#include <ieeefp.h>
#ifdef __alpha__
static fp_except_t allmask = FP_X_INV|FP_X_OFL|FP_X_UFL|FP_X_DZ|FP_X_IMP;
#else
static fp_except_t allmask = FP_X_INV|FP_X_OFL|FP_X_UFL|FP_X_DZ|FP_X_IMP|FP_X_DNML;
#endif
static fp_except_t oldmask = fpsetmask(~allmask);
#endif

/**
 * Macros to workaround math-bugs bugs in various platforms
 */

/**
 * Stefan Hanske <sh990154@mail.uni-greifswald.de> reports:
 *  ARM is a little endian architecture but 64 bit double words are stored
 * differently: the 32 bit words are in little endian byte order, the two words
 * are stored in big endian`s way.
 */

#if defined(__arm) || defined(__arm32__) || defined(__arm26__) || defined(__arm__)
#if !defined(__VFP_FP__)
#define FPU_IS_ARM_FPA
#endif
#endif

typedef union txdpun {
    struct {
#if defined(IS_LITTLE_ENDIAN) && !defined(FPU_IS_ARM_FPA)
        PRUint32 lo, hi;
#else
        PRUint32 hi, lo;
#endif
    } s;
    PRFloat64 d;
} txdpun;

#if (__GNUC__ == 2 && __GNUC_MINOR__ > 95) || __GNUC__ > 2
/**
 * This version of the macros is safe for the alias optimizations
 * that gcc does, but uses gcc-specific extensions.
 */
#define TX_DOUBLE_HI32(x) (__extension__ ({ txdpun u; u.d = (x); u.s.hi; }))
#define TX_DOUBLE_LO32(x) (__extension__ ({ txdpun u; u.d = (x); u.s.lo; }))

#else // __GNUC__

/* We don't know of any non-gcc compilers that perform alias optimization,
 * so this code should work.
 */

#if defined(IS_LITTLE_ENDIAN) && !defined(FPU_IS_ARM_FPA)
#define TX_DOUBLE_HI32(x)        (((PRUint32 *)&(x))[1])
#define TX_DOUBLE_LO32(x)        (((PRUint32 *)&(x))[0])
#else
#define TX_DOUBLE_HI32(x)        (((PRUint32 *)&(x))[0])
#define TX_DOUBLE_LO32(x)        (((PRUint32 *)&(x))[1])
#endif

#endif // __GNUC__

#define TX_DOUBLE_HI32_SIGNBIT   0x80000000
#define TX_DOUBLE_HI32_EXPMASK   0x7ff00000
#define TX_DOUBLE_HI32_MANTMASK  0x000fffff

#define TX_DOUBLE_IS_NaN(x)                                                \
((TX_DOUBLE_HI32(x) & TX_DOUBLE_HI32_EXPMASK) == TX_DOUBLE_HI32_EXPMASK && \
 (TX_DOUBLE_LO32(x) || (TX_DOUBLE_HI32(x) & TX_DOUBLE_HI32_MANTMASK)))

#ifdef IS_BIG_ENDIAN
#define TX_DOUBLE_NaN {{TX_DOUBLE_HI32_EXPMASK | TX_DOUBLE_HI32_MANTMASK,   \
                        0xffffffff}}
#else
#define TX_DOUBLE_NaN {{0xffffffff,                                         \
                        TX_DOUBLE_HI32_EXPMASK | TX_DOUBLE_HI32_MANTMASK}}
#endif

#if defined(XP_WIN)
#define TX_DOUBLE_COMPARE(LVAL, OP, RVAL)                                  \
    (!TX_DOUBLE_IS_NaN(LVAL) && !TX_DOUBLE_IS_NaN(RVAL) && (LVAL) OP (RVAL))
#else
#define TX_DOUBLE_COMPARE(LVAL, OP, RVAL) ((LVAL) OP (RVAL))
#endif

#endif /* __txdouble_h__ */
