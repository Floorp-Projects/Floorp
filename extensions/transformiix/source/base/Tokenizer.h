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
 *  - for method #nextToken():
 *    - added void return type declaration
 * </PRE>
 *
**/


#include "baseutils.h"
#include "String.h"


#ifndef MITRE_TOKENIZER_H
#define MITRE_TOKENIZER_H

class Tokenizer {

public:

      //----------------/
     //- Constructors -/
    //----------------/

    /**
     * Creates a new Tokenizer
    **/
    Tokenizer();

    /**
     * Creates a new Tokenizer using the given source string,
     * uses the default set of tokens (' ', '\r', '\n', '\t');
    **/
    Tokenizer(const String& source);

    /**
     * Creates a new Tokenizer using the given source string
     * and set of character tokens
    **/
    Tokenizer(const String& source, const String& tokens);


    /**
     * Default Destructor
    **/
    virtual ~Tokenizer();

    MBool hasMoreTokens();

    void nextToken(String& buffer);

private:

    Int32 currentPos;
    Int32 size;
    String str;
    String delimiters;

}; //-- Tokenizer
#endif
