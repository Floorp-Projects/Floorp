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
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: Integer.cpp,v 1.3 2001/06/26 14:08:02 peterv%netscape.com Exp $
 */

#include "primitives.h"
#include "baseutils.h"

//-----------------------------/
//- Implementation of Integer -/
//-----------------------------/

/**
 * A wrapper for the primitive int type, and provides some simple
 * integer related routines
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.3 $ $Date: 2001/06/26 14:08:02 $
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
    PRInt32 val = 0;
    for (PRInt32 i = 0; i < str.length(); i++) {
        val = (val * 10) + (str.charAt(i) - 48);
    }
} //-- Integer

/**
 * Returns the int value of this Integer
**/

PRInt32 Integer::intValue() {
    return value;
} //-- intValue;

/**
 * Converts the given String to an integer
**/
int Integer::intValue(const String& src) {

    int result = 0;
    PRInt32 idx = 0;
    int sign = 1;

    //-- trim leading whitespace
    for ( ; idx < src.length(); idx++ )
        if ( src.charAt(idx) != ' ' ) break;

    //-- check first character for sign
    if ( idx < src.length() ) {
        PRInt32 ch = src.charAt(idx);
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
        PRInt32 ch = src.charAt(idx);
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
    PRInt32 tempVal = value;
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

