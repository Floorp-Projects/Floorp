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
 * Tokenizer
 * A simple String tokenizer
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
 * <BR/>
 * <PRE>
 * Modifications:
 * 19990806: Larry Fitzpatrick
 *  - in method #nextToken():
 *    - added void return type declaration
 *    - added proper casts from Int32 to char
 * </PRE>
 *
**/

#include "Tokenizer.h"

/**
 * Creates a new Tokenizer
**/
Tokenizer::Tokenizer() {
    currentPos = 0;
    size = 0;
} //-- tokenizer

/**
 * Creates a new Tokenizer using the given source string,
 * uses the default set of delimiters {' ', '\r', '\n', '\t'};
**/
Tokenizer::Tokenizer(const String& source) {
    currentPos = 0;
    //-- copy source
    str = source;
    size = str.length();
    delimiters.append(" \n\r\t");
} //-- Tokenizer

/**
 * Creates a new Tokenizer using the given source string
 * and set of character delimiters
**/
Tokenizer::Tokenizer(const String& source, const String& delimiters) {
    currentPos = 0;
    // copy source
    str = source;
    size = str.length();
    // copy tokens
    this->delimiters.append(delimiters);
} //-- Tokenizer


/**
 * Default Destructor
**/
Tokenizer::~Tokenizer() {};

MBool Tokenizer::hasMoreTokens() {
    return (MBool)(currentPos < size);
} //-- hasMoreTokens

void Tokenizer::nextToken(String& buffer) {
    buffer.clear();
    while ( currentPos < size) {
        char ch = (char)str.charAt(currentPos);
        //-- if character is not a delimiter..append
        if (delimiters.indexOf(ch) < 0) buffer.append(ch);
        else break;
        ++currentPos;
    }
    //-- advance to next start pos
    while ( currentPos < size ) {
        char ch = (char)str.charAt(currentPos);
        //-- if character is not a delimiter, we are at
        //-- start of next token
        if (delimiters.indexOf(ch) < 0) break;
        ++currentPos;
    }
} //-- nextToken


