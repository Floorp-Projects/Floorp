/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 *
 */

/*
 * txTokenizer
 * A simple String tokenizer
 */

#include "Tokenizer.h"

/*
 * Creates a new Tokenizer using the given source string
 */
txTokenizer::txTokenizer(const String& aSource)
{
    mCurrentPos = 0;
    mSource = aSource;
    mSize = mSource.length();

    // Advance to start pos
    while (mCurrentPos < mSize) {
        UNICODE_CHAR ch = mSource.charAt(mCurrentPos);
        // If character is not a whitespace, we are at start of first token
        if (ch != ' ' && ch != '\n' &&
            ch != '\r' && ch != '\t')
            break;
        ++mCurrentPos;
    }
}

MBool txTokenizer::hasMoreTokens()
{
    return mCurrentPos < mSize;
}

void txTokenizer::nextToken(String& aBuffer)
{
    aBuffer.clear();
    while (mCurrentPos < mSize) {
        UNICODE_CHAR ch = mSource.charAt(mCurrentPos++);
        // If character is not a delimiter we append it
        if (ch == ' ' || ch == '\n' ||
            ch == '\r' || ch == '\t')
            break;
        aBuffer.append(ch);
    }

    // Advance to next start pos
    while (mCurrentPos < mSize) {
        UNICODE_CHAR ch = mSource.charAt(mCurrentPos);
        // If character is not a whitespace, we are at start of next token
        if (ch != ' ' && ch != '\n' &&
            ch != '\r' && ch != '\t')
            break;
        ++mCurrentPos;
    }
}

