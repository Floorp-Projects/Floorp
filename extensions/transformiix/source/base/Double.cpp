/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */



#include "primitives.h"

//----------------------------/
//- Implementation of Double -/
//----------------------------/
/**
 * A wrapper for the primitive double type, and provides some simple
 * floating point related routines
 * @author <a href="mailto:lef@opentext.com">Larry Fitzpatrick</a>
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

double d0 = 0.0;

const double Double::NaN = (d0/d0);
const double Double::NEGATIVE_INFINITY = (-1.0/d0);
const double Double::POSITIVE_INFINITY = (1.0/d0);

/**
 * Creates a new Double with it's value initialized to 0;
**/
Double::Double() {
    value = 0;
} //-- Double

/**
 * Creates a new Double with it's value initialized to the given double
**/
Double::Double(double dbl) {
    this->value = dbl;
} //-- Double

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
 * Determins whether the given double represents positive or negative
 * inifinity
**/
MBool Double::isInfinite(double dbl) {
    return  (MBool)((dbl == POSITIVE_INFINITY ) || (dbl == NEGATIVE_INFINITY));
} //-- isInfinite

/**
 * Determins whether this Double's value represents positive or
 * negative inifinty
**/
MBool Double::isInfinite() {
    return  (MBool)(( value == POSITIVE_INFINITY ) || (value == NEGATIVE_INFINITY));
} //-- isInfinite

/**
 * Determins whether the given double is NaN
**/
MBool Double::isNaN(double dbl) {
#ifdef MOZILLA
    return (MBool) _isnan(dbl);
#else
    return (MBool) isnan(dbl);
#endif
} //-- isNaN

/**
 * Determins whether this Double's value is NaN
**/
MBool Double::isNaN() {
#ifdef MOZILLA
    return (MBool) _isnan(value);
#else
    return (MBool) isnan(value);
#endif
} //-- isNaN

/**
 * Converts the given String to a double, if the String value does not
 * represent a double, NaN will be returned
**/
double Double::toDouble(const String& src) {

    double dbl = 0.0;
    double fraction   = 1.0;
    double multiplier = 10.0;
    Int32 idx = 0;

    double sign = 1.0;

    //-- trim leading whitespace
    for ( ; idx < src.length(); idx++ )
        if ( src.charAt(idx) != ' ' ) break;

    //-- check first character for sign
    if ( idx < src.length() ) {
        Int32 ch = src.charAt(idx);
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

        Int32 ch = src.charAt(idx);

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
 * Converts the value of this Double to a String, and places
 * The result into the destination String.
 * @return the given dest string
**/
String& Double::toString(String& dest) {
    return toString(value, dest);
} //-- toString

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


