/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 */

/**
 * txTokenizer
 * A simple String tokenizer
**/


#ifndef MITRE_TOKENIZER_H
#define MITRE_TOKENIZER_H

#include "baseutils.h"
#include "TxString.h"

class txTokenizer
{
public:

    /*
     * Creates a new txTokenizer using the given source string
     */
    txTokenizer(const String& aSource);

    /*
     * Checks if any more tokens are avalible
     */
    MBool hasMoreTokens();

    /*
     * Sets aBuffer to value of next token
     */
    void nextToken(String& aBuffer);

private:

    PRInt32 mCurrentPos;
    PRInt32 mSize;
    String mSource;

};
#endif

