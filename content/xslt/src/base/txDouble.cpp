/*
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

//----------------------------/
//- Implementation of Double -/
//----------------------------/
/**
 * A wrapper for the primitive double type, and provides some simple
 * floating point related routines
**/

//A trick to handle IEEE floating point exceptions on FreeBSD - E.D.
#ifdef __FreeBSD__
fp_except_t allmask = FP_X_INV|FP_X_OFL|FP_X_UFL|FP_X_DZ|FP_X_IMP|FP_X_DNML;
fp_except_t oldmask = fpsetmask(~allmask);
#endif

/**
 * Macros to workaround math-bugs bugs in various platforms
**/

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

/**
 * Creates a new Double with it's value initialized from the given String.
 * The String will be converted to a double. If the String does not
 * represent an IEEE 754 double, the value will be initialized to NaN
**/
Double::Double(const String& string) {
    this->value = toDouble(string);
} //-- Double



/**
 * Returns the value of this Double as a double
**/
double Double::doubleValue() {
    return this->value;
} //-- doubleValue

/**
 * Returns the value of this Double as an int
**/
int Double::intValue() {
    return (int)value;
} //-- intValue

/**
 * Determines whether the given double represents positive or negative
 * inifinity
**/
MBool Double::isInfinite(double dbl) {
    return ((TX_DOUBLE_HI32(dbl) & ~TX_DOUBLE_HI32_SIGNBIT) == TX_DOUBLE_HI32_EXPMASK &&
            !TX_DOUBLE_LO32(dbl));
} //-- isInfinite

/**
 * Determines whether the given double is NaN
**/
MBool Double::isNaN(double dbl) {
    return ((TX_DOUBLE_HI32(dbl) & TX_DOUBLE_HI32_EXPMASK) == TX_DOUBLE_HI32_EXPMASK &&
            (TX_DOUBLE_LO32(dbl) || (TX_DOUBLE_HI32(dbl) & TX_DOUBLE_HI32_MANTMASK)));
} //-- isNaN

/**
 * Determines whether the given double is negative
**/
MBool Double::isNeg(double dbl) {
    return (TX_DOUBLE_HI32(dbl) & TX_DOUBLE_HI32_SIGNBIT) != 0;
} //-- isNeg

/**
 * Converts the given String to a double, if the String value does not
 * represent a double, NaN will be returned
**/
double Double::toDouble(const String& src) {

    double dbl = 0.0;
    double multiplier = 10.0;
    PRInt32 idx = 0;

    double sign = 1.0;

    //-- trim leading whitespace
    for ( ; idx < src.length(); idx++ )
        if ( src.charAt(idx) != ' ' ) break;

    //-- check first character for sign
    if ( idx < src.length() ) {
        PRInt32 ch = src.charAt(idx);
        if ( ch == '-' ) {
            sign = -1.0;
            ++idx;
        }
    }
    else {
        return Double::NaN;
    }

    //-- convert remaining to number
    for ( ; idx < src.length(); idx++ ) {

        PRInt32 ch = src.charAt(idx);

        if (( ch >= '0') && (ch <= '9')) {
             if ( multiplier > 1.0 ) {
                dbl = dbl*multiplier;
                dbl += (double) (ch-48);
             }
             else {
                 dbl += multiplier * (ch-48);
                 multiplier = multiplier * 0.1;
             }
        }
        else if ( ch == '.') {
            if ( multiplier < 1.0 ) return Double::NaN;
            multiplier = 0.1;
        }
        else return Double::NaN;
    }
    dbl = dbl*sign;
    return dbl;
} //-- toDouble


/**
 * Converts the value of the given double to a String, and places
 * The result into the destination String.
 * @return the given dest string
**/
String& Double::toString(double value, String& dest) {

    //-- check for special cases

    if ( isNaN(value) ) {
        dest.append("NaN");
        return dest;
    }
    if ( isInfinite(value) ) {
        if (value < 0) dest.append('-');
        dest.append("Infinity");
        return dest;
    }

    MBool isNegative = (MBool)(value<0.0);
    double val = value;
    if ( isNegative ) val = val * -1.0;

    double ival = 0;
    double fval = modf(val, &ival);

    String iStr;

    int temp = (int)ival;


    if ( temp > 0.0 ) {
        while ( temp > 0.0 ) {
            iStr.append( (char) ((temp % 10)+48) );
            temp = temp / 10;
        }
        if ( isNegative ) iStr.append('-');
        iStr.reverse();
    }
    else iStr.append('0');

    iStr.append('.');
    if ( fval > 0.0 ) {
        while ( fval > 0.0000001 ) {
            fval = fval*10.0;
            fval = modf(fval, &ival);
            iStr.append( (char) (ival+48) );
        }
    }
    else iStr.append('0');

    dest.append(iStr);
    return dest;
} //-- toString

