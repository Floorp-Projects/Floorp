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
 * $Id: txXMLUtils.h,v 1.6 2005/11/02 07:33:37 peterv%netscape.com Exp $
 */

/**
 * An XML Utility class
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
**/

#ifndef MITRE_XMLUTILS_H
#define MITRE_XMLUTILS_H

#include "TxString.h"
#include "baseutils.h"

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
     * Returns true if the given string has only whitespace characters
    **/
    static MBool isWhitespace(const String& text);

    /**
     * Normalizes the value of an XML attribute
    **/
    static void normalizeAttributeValue(String& attValue);

    /**
     * Normalizes the value of a XML processingInstruction
    **/
    static void normalizePIValue(String& attValue);

    /**
     * Is this a whitespace string to be stripped?
     * Newlines (#xD), tabs (#x9), spaces (#x20), CRs (#xA) only?
     * @param data the String to test for whitespace
    **/
    static MBool shouldStripTextnode (const String& data);

private:

    /**
     * Returns true if the given character represents an Alpha letter
    **/
    static MBool isAlphaChar(PRInt32 ch);

    /**
     * Returns true if the given character represents a numeric letter (digit)
    **/
    static MBool isDigit(PRInt32 ch);

    /**
     * Returns true if the given character is an allowable QName character
    **/
    static MBool isQNameChar(PRInt32 ch);

    /**
     * Returns true if the given character is an allowable NCName character
    **/
    static MBool isNCNameChar(PRInt32 ch);

}; //-- XMLUtils
#endif

