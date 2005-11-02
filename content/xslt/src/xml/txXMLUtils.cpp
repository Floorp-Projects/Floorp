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
 */

/**
 * An XML utility class
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
MBool XMLUtils::isAlphaChar(PRInt32 ch) {
    if ((ch >= 'a' ) && (ch <= 'z' )) return MB_TRUE;
    if ((ch >= 'A' ) && (ch <= 'Z' )) return MB_TRUE;
    return MB_FALSE;
} //-- isAlphaChar

/**
 * Returns true if the given character represents a numeric letter (digit)
**/
MBool XMLUtils::isDigit(PRInt32 ch) {
    if ((ch >= '0') && (ch <= '9'))   return MB_TRUE;
    return MB_FALSE;
} //-- isDigit

/**
 * Returns true if the given character is an allowable QName character
**/
MBool XMLUtils::isNCNameChar(PRInt32 ch) {
    if (isDigit(ch) || isAlphaChar(ch)) return MB_TRUE;
    return (MBool) ((ch == '.') ||(ch == '_') || (ch == '-'));
} //-- isNCNameChar

/**
 * Returns true if the given character is an allowable NCName character
**/
MBool XMLUtils::isQNameChar(PRInt32 ch) {
    return (MBool) (( ch == ':') || isNCNameChar(ch));
} //-- isQNameChar

/**
 * Returns true if the given String is a valid XML QName
**/
MBool XMLUtils::isValidQName(String& name) {

    if (name.isEmpty())
        return MB_FALSE;

    if (!isAlphaChar(name.charAt(0)))
        return MB_FALSE;

    int size = name.length();
    for (int i = 1; i < size; i++) {
        if (!isQNameChar(name.charAt(i)))
            return MB_FALSE;
    }
    return MB_TRUE;
} //-- isValidQName

/**
 * Returns true if the given string has only whitespace characters
**/
MBool XMLUtils::isWhitespace(const String& text) {
    for ( int i = 0; i < text.length(); i++ ) {
        PRInt32 ch = text.charAt(i);
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
    PRInt32 size = attValue.length();
    //-- make copy of chars
    UNICODE_CHAR* chars = new UNICODE_CHAR[size];
    attValue.toUnicode(chars);
    //-- clear attValue
    attValue.clear();

    PRInt32 cc = 0;
    MBool addSpace = MB_FALSE;
    while ( cc < size) {
        UNICODE_CHAR ch = chars[cc++];
        switch (ch) {
            case ' ':
                if (!attValue.isEmpty())
                    addSpace = MB_TRUE;
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
    PRInt32 size = piValue.length();
    //-- make copy of chars
    UNICODE_CHAR* chars = new UNICODE_CHAR[size];
    piValue.toUnicode(chars);
    //-- clear attValue
    piValue.clear();

    PRInt32 cc = 0;
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
 * Is this a whitespace string to be stripped?
 * Newlines (#xD), tabs (#x9), spaces (#x20), CRs (#xA) only?
 * @param data the String to test for whitespace
**/
MBool XMLUtils::shouldStripTextnode (const String& data){
    MBool toStrip = MB_TRUE;
    for (PRInt32 i=0;toStrip && i<data.length();i++){
        switch(data.charAt(i)) {
            case 0x0020: // space
            case 0x0009: // tab
            case 0x000A: // LF
            case 0x000D: // CR
                break;
            default:
                toStrip = MB_FALSE;
                break;
        }
    }
    return toStrip;
} //-- shouldStripTextnode

