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



#ifndef MITRE_PRIMITIVES_H
#define MITRE_PRIMITIVES_H

#include "MITREObject.h"
#include "baseutils.h"
#include "String.h"
#include  <math.h>

#ifdef MOZILLA
#include <float.h>
#endif

/**
 * A wrapper for the primitive double type, and provides some simple
 * floating point related routines
 * @author <a href="mailto:lef@opentext.com">Larry Fitzpatrick</a>
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
class Double : public MITREObject {

public:

    static const double NaN;
    static const double POSITIVE_INFINITY;
    static const double NEGATIVE_INFINITY;

    /**
     * Creates a new Double with it's value initialized to 0;
    **/
    Double();

    /**
     * Creates a new Double with it's value initialized to the given double
    **/
    Double(double dbl);

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
     * Determins whether the given double represents positive or negative
     * inifinity
    **/
    static MBool isInfinite(double dbl);

    /**
     * Determins whether this Double's value represents positive or
     * negative inifinty
    **/
    MBool isInfinite();

    /**
     * Determins whether the given double is NaN
    **/
    static MBool isNaN(double dbl);

    /**
     * Determins whether this Double's value is NaN
    **/
    MBool isNaN();

    /**
     * Converts the value of this Double to a String, and places
     * The result into the destination String.
     * @return the given dest string
    **/
    String& toString(String& dest);

    /**
     * Converts the value of the given double to a String, and places
     * The result into the destination String.
     * @return the given dest string
    **/
    static String& Double::toString(double value, String& dest);


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
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
class Integer : public MITREObject {
public:

    /**
     * Creates a new Integer initialized to 0.
    **/
    Integer();

    /**
     * Creates a new Integer initialized to the given int value.
    **/
    Integer(Int32 integer);

    /**
     * Creates a new Integer based on the value of the given String
    **/
    Integer::Integer(const String& str);

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

    Int32 value;

    /**
     * converts the given String to an int
    **/
    static int intValue(const String& src);

}; //-- Integer

#endif
