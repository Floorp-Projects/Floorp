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
#include "baseutils.h"

//-----------------------------/
//- Implementation of Integer -/
//-----------------------------/

/**
 * A wrapper for the primitive int type, and provides some simple
 * integer related routines
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

/**
 * Creates a new Integer initialized to 0.
**/
Integer::Integer() {
    value = 0;
} //-- Integer

/**
 * Creates a new Integer initialized to the given int value.
**/
Integer::Integer(int value) {
    this->value = value;
} //-- Integer

/**
 * Creates a new Integer based on the value of the given String
**/
Integer::Integer(const String& str) {
    Int32 val = 0;
    for (Int32 i = 0; i < str.length(); i++) {
        val = (val * 10) + (str.charAt(i) - 48);
    }
} //-- Integer

/**
 * Returns the int value of this Integer
**/

Int32 Integer::intValue() {
    return value;
} //-- intValue;

/**
 * Converts the given String to an integer
**/
int Integer::intValue(const String& src) {

    int result = 0;
    Int32 idx = 0;
    int sign = 1;

    //-- trim leading whitespace
    for ( ; idx < src.length(); idx++ )
        if ( src.charAt(idx) != ' ' ) break;

    //-- check first character for sign
    if ( idx < src.length() ) {
        Int32 ch = src.charAt(idx);
        if ( ch == '-' ) {
            sign = -1;
            ++idx;
        }
    }
    else {
        return 0; //-- we should return NaN here
    }

    //-- convert remaining to number
    for ( ; idx < src.length(); idx++ ) {
        Int32 ch = src.charAt(idx);
        if (( ch >= '0') && (ch <= '9')) {
            result = result*10;
            result += (ch-48);
        }
        else return 0;
    }
    result = result*sign;
    return result;
} //-- toInteger

/**
 * Converts the given int to a String
**/
String& Integer::toString(int value, String& dest) {

    String result;
    UNICODE_CHAR charDigit;
    Int32 tempVal = value;
    MBool isNegative = (value < 0);
    if ( isNegative ) tempVal = -value;

    if ( tempVal > 0 ) {
        while (tempVal) {
        charDigit = (tempVal % 10) + 48;
        result.append(charDigit);
        tempVal /=10;
        }
        if ( isNegative ) result.append('-');
        result.reverse();
    }
    else result.append('0');
    dest.append(result);
    return dest;
} //-- toString

/**
 * Converts the given the value of this Integer to a String
**/
String& Integer::toString(String& dest) {
    return Integer::toString(value, dest);
} //-- toString

