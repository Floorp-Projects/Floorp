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
 * Larry Fitzpatrick, OpenText, lef@opentext.com
 *    --
 *
 * Eric Du, duxy@leyou.com.cn
 *   -- added fix for FreeBSD
 *
 * NaN/Infinity code copied from the JS-library with permission from
 * Netscape Communications Corporation: http://www.mozilla.org/js
 * http://lxr.mozilla.org/seamonkey/source/js/src/jsnum.h
 *
 */


#ifndef MITRE_PRIMITIVES_H
#define MITRE_PRIMITIVES_H

#include "TxObject.h"
#include "baseutils.h"
#include "TxString.h"

/**
 * A wrapper for the primitive double type, and provides some simple
 * floating point related routines
**/
class Double : public TxObject {

public:

    static const double NaN;
    static const double POSITIVE_INFINITY;
    static const double NEGATIVE_INFINITY;

    /**
     * Creates a new Double with it's value initialized from the given String.
     * The String will be converted to a double. If the String does not
     * represent an IEEE 754 double, the value will be initialized to NaN
    **/
    Double(const String& string);

    /**
     * Returns the value of this Double as a double
    **/
    double doubleValue();

    /**
     * Returns the value of this Double as an int
    **/
    int    intValue();

    /**
     * Determines whether the given double represents positive or negative
     * inifinity
    **/
    static MBool isInfinite(double dbl);

    /**
     * Determines whether the given double is NaN
    **/
    static MBool isNaN(double dbl);

    /**
     * Determines whether the given double is negative
    **/
    static MBool isNeg(double dbl);

    /**
     * Converts the value of the given double to a String, and places
     * The result into the destination String.
     * @return the given dest string
    **/
    static String& toString(double value, String& dest);


private:
    double value;
    /**
     * Converts the given String to a double, if the String value does not
     * represent a double, NaN will be returned
    **/
    static double toDouble(const String& str);
};


/**
 * A wrapper for the primitive int type, and provides some simple
 * integer related routines
**/
class Integer : public TxObject {
public:

    /**
     * Creates a new Integer initialized to 0.
    **/
    Integer();

    /**
     * Creates a new Integer initialized to the given int value.
    **/
    Integer(PRInt32 integer);

    /**
     * Creates a new Integer based on the value of the given String
    **/
    Integer(const String& str);

    /**
     * Returns the int value of this Integer
    **/
    int intValue();

    /**
     * Converts the value of this Integer to a String
    **/
    String& toString(String& dest);

    /**
     * Converts the given int to a String
    **/
    static String& toString(int value, String& dest);

private:

    PRInt32 value;

    /**
     * converts the given String to an int
    **/
    static int intValue(const String& src);

}; //-- Integer

#endif

