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

/**
 * An XML Utility class
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include "String.h"
#include "baseutils.h"

#ifndef MITRE_XMLUTILS_H
#define MITRE_XMLUTILS_H

class XMLUtils {

public:

    static const String XMLNS;

    static void getNameSpace(const String& src, String& dest);
    static void getLocalPart(const String& src, String& dest);

    /**
     * Returns true if the given String is a valid XML QName
    **/
    static MBool isValidQName(String& name);

    /**
     * Normalizes the value of an XML attribute
    **/
    static void normalizeAttributeValue(String& attValue);

    /**
     * Normalizes the value of a XML processingInstruction
    **/
    static void normalizePIValue(String& attValue);

    /**
     * Strips whitespace from the given String.
     * Newlines (#xD), tabs (#x9), and consecutive spaces (#x20) are
     * converted to a single space (#x20).
     * @param data the String to strip whitespace from
     * @param dest the destination String to append the result to
    **/
    static void stripSpace (const String& data, String& dest);

    /**
     * Strips whitespace from the given String.
     * Newlines (#xD), tabs (#x9), and consecutive spaces (#x20) are
     * converted to a single space (#x20).
     * @param data the String to strip whitespace from
     * @param dest the destination String to append the result to
     * @param stripAllLeadSpace, a boolean indicating whether or not to
     * strip all leading space. If true all whitespace from the start of the
     * given String will be stripped. If false, all whitespace from the start
     * of the given String will be converted to a single space.
     * @param stripAllTrailSpace, a boolean indicating whether or not to
     * strip all trailing space. If true all whitespace at the end of the
     * given String will be stripped. If false, all whitespace at the end
     * of the given String will be converted to a single space.
    **/
    static void stripSpace (const String& data,
                            String& dest,
                            MBool stripAllLeadSpace,
                            MBool stripAllTrailSpace);

private:

    /**
     * Returns true if the given character represents an Alpha letter
    **/
    static MBool isAlphaChar(Int32 ch);

    /**
     * Returns true if the given character represents a numeric letter (digit)
    **/
    static MBool isDigit(Int32 ch);

    /**
     * Returns true if the given character is an allowable QName character
    **/
    static MBool isQNameChar(Int32 ch);

    /**
     * Returns true if the given character is an allowable NCName character
    **/
    static MBool isNCNameChar(Int32 ch);

}; //-- XMLUtils
#endif
