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
 * Lidong, lidong520@263.net
 *    -- unicode bug fix
 *
 * $Id: txXMLUtils.cpp,v 1.3 2005/11/02 07:33:36 kvisco%ziplink.net Exp $
 */
/**
 * An XML utility class
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include "XMLUtils.h"

  //------------------------------/
 //- Implementation of XMLUtils -/
//------------------------------/

const String XMLUtils::XMLNS = "xmlns";

void XMLUtils::getNameSpace(const String& src, String& dest) {

    //-- anything preceding ':' is the namespace part of the name
    int idx = src.indexOf(':');
    if ( idx > 0 ) {
        //-- create new String to prevent any chars in dest from being
        //-- lost
        String tmp;
        src.subString(0,idx, tmp);
        dest.append(tmp);
    }
    else dest.append("");

} //-- getNameSpace

void XMLUtils::getLocalPart(const String& src, String& dest) {

    //-- anything after ':' is the local part of the name
    int idx = src.indexOf(':');
    if ( idx < -1 ) idx = -1;
    //-- create new String to prevent any chars in dest from being
    //-- lost
    String tmp;
    src.subString(idx+1, tmp);
    dest.append(tmp);

} //-- getLocalPart

/**
 * Returns true if the given character represents an Alpha letter
**/
MBool XMLUtils::isAlphaChar(Int32 ch) {
    if ((ch >= 'a' ) && (ch <= 'z' )) return MB_TRUE;
    if ((ch >= 'A' ) && (ch <= 'Z' )) return MB_TRUE;
    return MB_FALSE;
} //-- isAlphaChar

/**
 * Returns true if the given character represents a numeric letter (digit)
**/
MBool XMLUtils::isDigit(Int32 ch) {
    if ((ch >= '0') && (ch <= '9'))   return MB_TRUE;
    return MB_FALSE;
} //-- isDigit

/**
 * Returns true if the given character is an allowable QName character
**/
MBool XMLUtils::isNCNameChar(Int32 ch) {
    if (isDigit(ch) || isAlphaChar(ch)) return MB_TRUE;
    return (MBool) ((ch == '.') ||(ch == '_') || (ch == '-'));
} //-- isNCNameChar

/**
 * Returns true if the given character is an allowable NCName character
**/
MBool XMLUtils::isQNameChar(Int32 ch) {
    return (MBool) (( ch == ':') || isNCNameChar(ch));
} //-- isQNameChar

/**
 * Returns true if the given String is a valid XML QName
**/
MBool XMLUtils::isValidQName(String& name) {

    int size = name.length();
    if ( size == 0 ) return MB_FALSE;
    else if ( !isAlphaChar(name.charAt(0))) return MB_FALSE;
    else {
        for ( int i = 1; i < size; i++) {
            if ( ! isQNameChar(name.charAt(i))) return MB_FALSE;
        }
    }
    return MB_TRUE;
} //-- isValidQName

/**
 * Returns true if the given string has only whitespace characters
**/
MBool XMLUtils::isWhitespace(const String& text) {
    for ( int i = 0; i < text.length(); i++ ) {
        Int32 ch = text.charAt(i);
        switch ( ch ) {
            case ' '  :
            case '\n' :
            case '\r' :
            case '\t' :
                break;
            default:
                return MB_FALSE;
        }
    }
    return MB_TRUE;
} //-- isWhitespace

/**
 * Normalizes the value of an XML attribute
**/
void XMLUtils::normalizeAttributeValue(String& attValue) {
    Int32 size = attValue.length();
    //-- make copy of chars
    UNICODE_CHAR* chars = new UNICODE_CHAR[size];
    attValue.toUnicode(chars);
    //-- clear attValue
    attValue.clear();

    Int32 cc = 0;
    MBool addSpace = MB_FALSE;
    while ( cc < size) {
        UNICODE_CHAR ch = chars[cc++];
        switch (ch) {
            case ' ':
                if ( attValue.length() > 0) addSpace = MB_TRUE;
                break;
            case '\r':
                break;
            case '\n':
                attValue.append("&#xA;");
                break;
            case '\'':
                attValue.append("&apos;");
                break;
            case '\"':
                attValue.append("&quot;");
                break;
            default:
                if ( addSpace) {
                    attValue.append(' ');
                    addSpace = MB_FALSE;
                }
                attValue.append(ch);
                break;
        }
    }
    delete chars;
} //-- normalizeAttributeValue

/**
 * Normalizes the value of a XML processing instruction
**/
void XMLUtils::normalizePIValue(String& piValue) {
    Int32 size = piValue.length();
    //-- make copy of chars
    UNICODE_CHAR* chars = new UNICODE_CHAR[size];
    piValue.toUnicode(chars);
    //-- clear attValue
    piValue.clear();

    Int32 cc = 0;
    UNICODE_CHAR prevCh = 0x0000;
    while ( cc < size) {
        UNICODE_CHAR ch = chars[cc++];
        switch (ch) {
            case '>':
                if ( prevCh == '?' ) {
                    piValue.append(' ');
                }
                piValue.append(ch);
                break;
            default:
                piValue.append(ch);
                break;
        }
        prevCh = ch;
    }
    delete chars;
} //-- noramlizePIValue

/**
 * Strips whitespace from the given String.
 * Newlines (#xD), tabs (#x9), and consecutive spaces (#x20) are
 * converted to a single space (#x20).
 * @param data the String to strip whitespace from
 * @param dest the destination String to append the result to
**/
void XMLUtils::stripSpace (const String& data, String& dest) {
    stripSpace(data,dest,MB_FALSE,MB_FALSE);
} //-- stripSpace

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
void XMLUtils::stripSpace
    (   const String& data,
        String& dest,
        MBool stripAllLeadSpace,
        MBool stripAllTrailSpace    )
{

    UNICODE_CHAR lastToken, token;

    lastToken = 0x0000;
    Int32 len   = data.length();
    Int32 oldLen = dest.length();

    // indicates we have seen at least one
    // non whitespace charater
    MBool validChar = MB_FALSE;


    for (Int32 i = 0; i < len; i++) {
        token = data.charAt(i);
        switch(token) {
            case 0x0020: // space
            case 0x0009: // tab
            case 0x000A: // LF
            case 0x000D: // CR
                token = 0x0020;
                if (stripAllLeadSpace && (!validChar)) break;
                if (lastToken != token) dest.append(token);
                break;
            default:
                dest.append(token);
                validChar = MB_TRUE;
                break;
        }
        lastToken = token;
    }

    //-- remove last trailing space if necessary
    if (stripAllTrailSpace) {
        len = dest.length();
        if ( (len > oldLen)  && (dest.charAt(len-1) == 0x0020) ) dest.setLength(len-1);
    }

} //-- stripSpace

