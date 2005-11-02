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

#include "nsString.h"
#include "primitives.h"
#include "XMLUtils.h"
#include <math.h>
#include <stdlib.h>
#ifdef WIN32
#include <float.h>
#endif
#include "prdtoa.h"

/*
 * Utility class for doubles
 */

//A trick to handle IEEE floating point exceptions on FreeBSD - E.D.
#ifdef __FreeBSD__
#include <ieeefp.h>
#ifdef __alpha__
fp_except_t allmask = FP_X_INV|FP_X_OFL|FP_X_UFL|FP_X_DZ|FP_X_IMP;
#else
fp_except_t allmask = FP_X_INV|FP_X_OFL|FP_X_UFL|FP_X_DZ|FP_X_IMP|FP_X_DNML;
#endif
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
class txStringToDouble
{
public:
    typedef PRUnichar input_type;
    typedef PRUnichar value_type;
    txStringToDouble(): mState(eWhitestart), mSign(ePositive) {}

    PRUint32
    write(const input_type* aSource, PRUint32 aSourceLength)
    {
        if (mState == eIllegal) {
            return aSourceLength;
        }
        PRUint32 i = 0;
        PRUnichar c;
        for ( ; i < aSourceLength; ++i) {
            c = aSource[i];
            switch (mState) {
                case eWhitestart:
                    if (c == '-') {
                        mState = eDecimal;
                        mSign = eNegative;
                    }
                    else if (c >= '0' && c <= '9') {
                        mState = eDecimal;
                        mBuffer.Append((char)c);
                    }
                    else if (c == '.') {
                        mState = eMantissa;
                        mBuffer.Append((char)c);
                    }
                    else if (!XMLUtils::isWhitespace(c)) {
                        mState = eIllegal;
                        return aSourceLength;
                    }
                    break;
                case eDecimal:
                    if (c >= '0' && c <= '9') {
                        mBuffer.Append((char)c);
                    }
                    else if (c == '.') {
                        mState = eMantissa;
                        mBuffer.Append((char)c);
                    }
                    else if (XMLUtils::isWhitespace(c)) {
                        mState = eWhiteend;
                    }
                    else {
                        mState = eIllegal;
                        return aSourceLength;
                    }
                    break;
                case eMantissa:
                    if (c >= '0' && c <= '9') {
                        mBuffer.Append((char)c);
                    }
                    else if (XMLUtils::isWhitespace(c)) {
                        mState = eWhiteend;
                    }
                    else {
                        mState = eIllegal;
                        return aSourceLength;
                    }
                    break;
                case eWhiteend:
                    if (!XMLUtils::isWhitespace(c)) {
                        mState = eIllegal;
                        return aSourceLength;
                    }
                    break;
                default:
                    break;
            }
        }
        return aSourceLength;
    }

    double
    getDouble()
    {
        if (mState == eIllegal || mBuffer.IsEmpty() ||
            (mBuffer.Length() == 1 && mBuffer[0] == '.')) {
            return Double::NaN;
        }
        return mSign*PR_strtod(mBuffer.get(), 0);
    }
private:
    nsCAutoString mBuffer;
    enum {
        eWhitestart,
        eDecimal,
        eMantissa,
        eWhiteend,
        eIllegal
    } mState;
    enum {
        eNegative = -1,
        ePositive = 1
    } mSign;
};

double Double::toDouble(const nsAString& aSrc)
{
    txStringToDouble sink;
    nsAString::const_iterator fromBegin, fromEnd;
    copy_string(aSrc.BeginReading(fromBegin), aSrc.EndReading(fromEnd), sink);
    return sink.getDouble();
}

/*
 * Converts the value of the given double to a String, and places
 * The result into the destination String.
 * @return the given dest string
 */
void Double::toString(double aValue, nsAString& aDest)
{

    // check for special cases

    if (isNaN(aValue)) {
        aDest.Append(NS_LITERAL_STRING("NaN"));
        return;
    }
    if (isInfinite(aValue)) {
        if (aValue < 0)
            aDest.Append(PRUnichar('-'));
        aDest.Append(NS_LITERAL_STRING("Infinity"));
        return;
    }

    int bufsize;
    if (fabs(aValue) > 1)
        bufsize = (int)log10(fabs(aValue)) + 30;
    else
        bufsize = 30;
    
    char* buf = new char[bufsize];
    if (!buf) {
        NS_ASSERTION(0, "out of memory");
        return;
    }

    PRIntn intDigits, sign;
    char* endp;
    PR_dtoa(aValue, 0, 0, &intDigits, &sign, &endp, buf, bufsize-1);

    if (sign)
        aDest.Append(PRUnichar('-'));

    int i;
    for (i = 0; i < endp - buf; i++) {
        if (i == intDigits)
            aDest.Append(PRUnichar('.'));
        aDest.Append(PRUnichar(buf[i]));
    }
    
    for (; i < intDigits; i++)
        aDest.Append(PRUnichar('0'));

    delete [] buf;
}
