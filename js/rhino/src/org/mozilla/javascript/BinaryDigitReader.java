
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package org.mozilla.javascript;

final class BinaryDigitReader {
    int lgBase;			// Logarithm of base of number
    int digit;			// Current digit value in radix given by base
    int digitPos;		// Bit position of last bit extracted from digit
    String digits;		// String containing the digits
    int start;			// Index of the first remaining digit
    int end;			// Index past the last remaining digit

    BinaryDigitReader(int base, String digits, int start, int end) {
        lgBase = 0;
        while (base != 1) {
            lgBase++;
            base >>= 1;
        }
        digitPos = 0;
        this.digits = digits;
        this.start = start;
        this.end = end;
    }

    /* Return the next binary digit from the number or -1 if done */
    int getNextBinaryDigit()
    {
        if (digitPos == 0) {
            if (start == end)
                return -1;
    
            char c = digits.charAt(start++);
            if ('0' <= c && c <= '9')
                digit = c - '0';
            else if ('a' <= c && c <= 'z')
                digit = c - 'a' + 10;
            else digit = c - 'A' + 10;
            digitPos = lgBase;
        }
        return digit >> --digitPos & 1;
    }
}
