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
 * Larry Fitzpartick, OpenText, lef@opentext.com
 *  -- 19990806
 *    -- added void return type declaration for ::nextToken()
 *    -- added proper cast from PRInt32 to char in ::nextToken()
 * $Id: Tokenizer.cpp,v 1.3 2001/06/26 14:09:39 peterv%netscape.com Exp $
 */

/**
 * Tokenizer
 * A simple String tokenizer
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.3 $ $Date: 2001/06/26 14:09:39 $
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

