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
 * Normalizes the value of an XML attribute
**/
void XMLUtils::normalizeAttributeValue(String& attValue) {
    Int32 size = attValue.length();
    //-- make copy of chars
    char* chars = new char[size+1];
    attValue.toChar(chars);
    //-- clear attValue
    attValue.clear();

    Int32 cc = 0;
    MBool addSpace = MB_FALSE;
    while ( cc < size) {
        char ch = chars[cc++];
        switch (ch) {
            case ' ':
                if ( attValue.length() > 0) addSpace = MB_TRUE;
                break;
            case '\r':
                break;
            case '\n':
                attValue.append("&#xA;");
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
    char* chars = new char[size+1];
    piValue.toChar(chars);
    //-- clear attValue
    piValue.clear();

    Int32 cc = 0;
    char prevCh = '\0';
    while ( cc < size) {
        char ch = chars[cc++];
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

    char lastToken, token;
    Int32 oldSize = data.length();
    char* chars = new char[oldSize+1];
    data.toChar(chars);

    lastToken = '\0';
    Int32 total = 0;

    // indicates we have seen at least one
    // non whitespace charater
    MBool validChar = MB_FALSE;

    for (int i = 0; i < oldSize; i++) {
        token = chars[i];
        switch(token) {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                token = ' ';
                if (stripAllLeadSpace && (!validChar)) break;
                if (lastToken != token) chars[total++] = token;
                break;
            default:
                chars[total++] = token;
                validChar = MB_TRUE;
                break;
        }
        lastToken = token;
    }

    //-- remove last trailing space if necessary
    if (stripAllTrailSpace)
        if ((total > 0) && (chars[total-1] == ' ')) --total;

    if (validChar) {
        chars[total] = '\0'; //-- add Null terminator
        dest.append(chars);
    }
    delete chars;
} //-- stripSpace

