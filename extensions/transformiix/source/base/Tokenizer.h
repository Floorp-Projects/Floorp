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
 *    -- 19990806, added void return type declaration for ::nextToken()
 *
 * $Id: Tokenizer.h,v 1.3 2000/04/12 22:31:03 nisheeth%netscape.com Exp $
 */

/**
 * Tokenizer
 * A simple String tokenizer
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.3 $ $Date: 2000/04/12 22:31:03 $
**/


#include "baseutils.h"
#include "TxString.h"


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

