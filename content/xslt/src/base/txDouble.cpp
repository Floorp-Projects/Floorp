/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Larry Fitzpatrick, lef@opentext.com
 *
 * Eric Du, duxy@leyou.com.cn
 *   -- added fix for FreeBSD
 *
 * NaN/Infinity code copied from the JS-library with permission from
 * Netscape Communications Corporation: http://www.mozilla.org/js
 * http://lxr.mozilla.org/seamonkey/source/js/src/jsnum.h
 *
 */

#include "primitives.h"
#include  <math.h>
#ifdef WIN32
#include <float.h>
#endif
//A trick to handle IEEE floating point exceptions on FreeBSD - E.D.
#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif
#ifndef TX_EXE
#include "prdtoa.h"
#else
#include "stdlib.h"
#include "stdio.h"
#endif

/*
 * Utility class for doubles
 */

//A trick to handle IEEE floating point exceptions on FreeBSD - E.D.
#ifdef __FreeBSD__
fp_except_t allmask = FP_X_INV|FP_X_OFL|FP_X_UFL|FP_X_DZ|FP_X_IMP|FP_X_DNML;
fp_except_t oldmask = fpsetmask(~allmask);
#endif

/*
 * Macros to workaround math-bugs bugs in various platforms
 */

#ifdef IS_BIG_ENDIAN
#define TX_DOUBLE_HI32(x)        (((PRUint32 *)&(x))[0])
#define TX_DOUBLE_LO32(x)        (((PRUint32 *)&(x))[1])
#else
#define TX_DOUBLE_HI32(x)        (((PRUint32 *)&(x))[1])
#define TX_DOUBLE_LO32(x)        (((PRUint32 *)&(x))[0])
#endif

#define TX_DOUBLE_HI32_SIGNBIT   0x80000000
#define TX_DOUBLE_HI32_EXPMASK   0x7ff00000
#define TX_DOUBLE_HI32_MANTMASK  0x000fffff

//-- Initialize Double related constants
#ifdef IS_BIG_ENDIAN
const PRUint32 nanMask[2] =    {TX_DOUBLE_HI32_EXPMASK | TX_DOUBLE_HI32_MANTMASK,
                                0xffffffff};
const PRUint32 infMask[2] =    {TX_DOUBLE_HI32_EXPMASK, 0};
const PRUint32 negInfMask[2] = {TX_DOUBLE_HI32_EXPMASK | TX_DOUBLE_HI32_SIGNBIT, 0};
#else
const PRUint32 nanMask[2] =    {0xffffffff,
                                TX_DOUBLE_HI32_EXPMASK | TX_DOUBLE_HI32_MANTMASK};
const PRUint32 infMask[2] =    {0, TX_DOUBLE_HI32_EXPMASK};
const PRUint32 negInfMask[2] = {0, TX_DOUBLE_HI32_EXPMASK | TX_DOUBLE_HI32_SIGNBIT};
#endif

const double Double::NaN = *((double*)nanMask);
const double Double::POSITIVE_INFINITY = *((double*)infMask);
const double Double::NEGATIVE_INFINITY = *((double*)negInfMask);

/*
 * Determines whether the given double represents positive or negative
 * inifinity
 */
MBool Double::isInfinite(double aDbl)
{
    return ((TX_DOUBLE_HI32(aDbl) & ~TX_DOUBLE_HI32_SIGNBIT) == TX_DOUBLE_HI32_EXPMASK &&
            !TX_DOUBLE_LO32(aDbl));
}

/*
 * Determines whether the given double is NaN
 */
MBool Double::isNaN(double aDbl)
{
    return ((TX_DOUBLE_HI32(aDbl) & TX_DOUBLE_HI32_EXPMASK) == TX_DOUBLE_HI32_EXPMASK &&
            (TX_DOUBLE_LO32(aDbl) || (TX_DOUBLE_HI32(aDbl) & TX_DOUBLE_HI32_MANTMASK)));
}

/*
 * Determines whether the given double is negative
 */
MBool Double::isNeg(double aDbl)
{
    return (TX_DOUBLE_HI32(aDbl) & TX_DOUBLE_HI32_SIGNBIT) != 0;
}

/*
 * Converts the given String to a double, if the String value does not
 * represent a double, NaN will be returned
 */
double Double::toDouble(const String& aSrc)
{
    PRInt32 idx = 0;
    PRInt32 len = aSrc.length();
    MBool digitFound = MB_FALSE;

    double sign = 1.0;

    // leading whitespace
    while (idx < len &&
           (aSrc.charAt(idx) == ' ' ||
            aSrc.charAt(idx) == '\n' ||
            aSrc.charAt(idx) == '\r' ||
            aSrc.charAt(idx) == '\t'))
        ++idx;

    // sign char
    if (idx < len && aSrc.charAt(idx) == '-')
        ++idx;

    // integer chars
    while (idx < len &&
           aSrc.charAt(idx) >= '0' &&
           aSrc.charAt(idx) <= '9') {
        ++idx;
        digitFound = MB_TRUE;
    }

    // decimal separator
    if (idx < len && aSrc.charAt(idx) == '.') {
        ++idx;

        // fraction chars
        while (idx < len &&
               aSrc.charAt(idx) >= '0' &&
               aSrc.charAt(idx) <= '9') {
            ++idx;
            digitFound = MB_TRUE;
        }
    }

    // ending whitespace
    while ((aSrc.charAt(idx) == ' ' ||
            aSrc.charAt(idx) == '\n' ||
            aSrc.charAt(idx) == '\r' ||
            aSrc.charAt(idx) == '\t') &&
           idx < len)
        ++idx;

    // "."==NaN, ".0"=="0."==0
    if (digitFound && idx == len) {
        char* buf = aSrc.toCharArray();
        double res = buf ? atof(buf) : Double::NaN;
        delete [] buf;
        return res;
    }

    return Double::NaN;
}

/*
 * Converts the value of the given double to a String, and places
 * The result into the destination String.
 * @return the given dest string
 */
String& Double::toString(double aValue, String& aDest)
{

    // check for special cases

    if (isNaN(aValue)) {
        aDest.append("NaN");
        return aDest;
    }
    if (isInfinite(aValue)) {
        if (aValue < 0)
            aDest.append('-');
        aDest.append("Infinity");
        return aDest;
    }

    int bufsize;
    if (fabs(aValue) > 1)
        bufsize = (int)log10(fabs(aValue)) + 30;
    else
        bufsize = 30;
    
    char* buf = new char[bufsize];
    if (!buf) {
        NS_ASSERTION(0, "out of memory");
        return aDest;
    }

#ifndef TX_EXE

    PRIntn intDigits, sign;
    char* endp;
    PR_dtoa(aValue, 0, 0, &intDigits, &sign, &endp, buf, bufsize-1);

    if (sign)
        aDest.append('-');

    int i;
    for (i = 0; i < endp - buf; i++) {
        if (i == intDigits)
            aDest.append('.');
        aDest.append(buf[i]);
    }
    
    for (; i < intDigits; i++)
        aDest.append('0');

#else

    sprintf(buf, "%1.10f", aValue);

    MBool deciPassed = MB_FALSE;
    MBool printDeci = MB_FALSE;
    int zeros=0;
    int i;
    for (i = 0; buf[i]; i++) {
        if (buf[i] == '.') {
            deciPassed = MB_TRUE;
            printDeci = MB_TRUE;
        }
        else if (deciPassed && buf[i] == '0') {
            zeros++;
        }
        else {
            if (printDeci) {
                aDest.append('.');
                printDeci = MB_FALSE;
            }

            for ( ;zeros ;zeros--)
                aDest.append('0');

            aDest.append(buf[i]);
        }
    }

#endif
    
    delete [] buf;
    
    return aDest;
}
