/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * Jonas Sicking. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "txXSLTNumber.h"
#include "primitives.h"
#include "Names.h"

class txDecimalCounter : public txFormattedCounter {
public:
    txDecimalCounter() : mMinLength(1), mGroupSize(50)
    {
    }
    
    txDecimalCounter(PRInt32 aMinLength, PRInt32 aGroupSize,
                     const String& mGroupSeparator);
    
    virtual void appendNumber(PRInt32 aNumber, String& aDest);

private:
    PRInt32 mMinLength;
    PRInt32 mGroupSize;
    String mGroupSeparator;
};

class txAlphaCounter : public txFormattedCounter {
public:
    txAlphaCounter(UNICODE_CHAR aOffset) : mOffset(aOffset)
    {
    }

    virtual void appendNumber(PRInt32 aNumber, String& aDest);
    
private:
    UNICODE_CHAR mOffset;
};

class txRomanCounter : public txFormattedCounter {
public:
    txRomanCounter(MBool aUpper) : mTableOffset(aUpper ? 30 : 0)
    {
    }

    void appendNumber(PRInt32 aNumber, String& aDest);

private:
    PRInt32 mTableOffset;
};


nsresult
txFormattedCounter::getCounterFor(const String& aToken, PRInt32 aGroupSize,
                                  const String& aGroupSeparator,
                                  txFormattedCounter*& aCounter)
{
    PRInt32 length = aToken.length();
    NS_ASSERTION(length, "getting counter for empty token");
    aCounter = 0;
    
    if (length == 1) {
        UNICODE_CHAR ch = aToken.charAt(0);
        switch (ch) {

            case 'i':
            case 'I':
                aCounter = new txRomanCounter(ch == 'I');
                break;
            
            case 'a':
            case 'A':
                aCounter = new txAlphaCounter(ch);
                break;
            
            case '1':
            default:
                // if we don't recognize the token then use "1"
                aCounter = new txDecimalCounter(1, aGroupSize, aGroupSeparator);
                break;
        }
        return aCounter ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    
    // for now, the only multi-char token we support are decimals
    PRInt32 i;
    for (i = 0; i < length-1; ++i) {
        if (aToken.charAt(i) != '0')
            break;
    }
    if (i == length-1 && aToken.charAt(i) == '1') {
        aCounter = new txDecimalCounter(length, aGroupSize, aGroupSeparator);
    }
    else {
        // if we don't recognize the token then use '1'
        aCounter = new txDecimalCounter(1, aGroupSize, aGroupSeparator);
    }

    return aCounter ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


txDecimalCounter::txDecimalCounter(PRInt32 aMinLength, PRInt32 aGroupSize,
                                   const String& aGroupSeparator)
    : mMinLength(aMinLength), mGroupSize(aGroupSize),
      mGroupSeparator(aGroupSeparator)
{
    if (mGroupSize <= 0) {
        mGroupSize = aMinLength + 10;
    }
}

void txDecimalCounter::appendNumber(PRInt32 aNumber, String& aDest)
{
    String num;

    const PRInt32 bufsize = 10; //must be able to fit an PRInt32
    UNICODE_CHAR buf[bufsize];
    PRInt32 pos = bufsize;
    while (aNumber > 0) {
        PRInt32 ch = aNumber % 10;
        aNumber /= 10;
        buf[--pos] = ch + '0';
    }

    // in case we didn't get a long enough string
    PRInt32 end  = (bufsize > mMinLength) ? bufsize - mMinLength : 0;
    while (pos > end) {
        buf[--pos] = '0';
    }
    
    // in case we *still* didn't get a long enough string.
    // this should be very rare since it only happens if mMinLength is bigger
    // then the length of any PRInt32.
    // pos will always be zero 
    PRInt32 extraPos = mMinLength;
    while (extraPos > bufsize) {
        aDest.append('0');
        --extraPos;
        if (extraPos % mGroupSize == 0) {
            aDest.append(mGroupSeparator);
        }
    }

    // copy string to buffer
    if (mGroupSize >= bufsize - pos) {
        // no grouping will occur
        aDest.append(buf + pos, (PRUint32)(bufsize - pos));
    }
    else {
        // append chars up to first grouping separator
        PRInt32 len = ((bufsize - pos - 1) % mGroupSize) + 1;
        aDest.append(buf + pos, len);
        pos += len;
        while (bufsize - pos > 0) {
            aDest.append(mGroupSeparator);
            aDest.append(buf + pos, mGroupSize);
            pos += mGroupSize;
        }
        NS_ASSERTION(bufsize == pos, "error while grouping");
    }
}


void txAlphaCounter::appendNumber(PRInt32 aNumber, String& aDest)
{
    String num;

    UNICODE_CHAR buf[11];
    buf[11] = 0;
    PRInt32 pos = 11;
    while (aNumber > 0) {
        --aNumber;
        PRInt32 ch = aNumber % 26;
        aNumber /= 26;
        buf[--pos] = ch + mOffset;
    }
    
    aDest.append(buf + pos, (PRUint32)(11 - pos));
}


const char* kTxRomanNumbers[] =
    {"", "c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm",
     "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc",
     "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix",
     "", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM",
     "", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC",
     "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};

void txRomanCounter::appendNumber(PRInt32 aNumber, String& aDest)
{
    // Numbers bigger then 3999 can't be done in roman
    if (aNumber >= 4000) {
        txDecimalCounter().appendNumber(aNumber, aDest);
        return;
    }

    while (aNumber >= 1000) {
        aDest.append(!mTableOffset ? 'm' : 'M');
        aNumber -= 1000;
    }

    PRInt32 posValue;
    
    // Hundreds
    posValue = aNumber / 100;
    aNumber %= 100;
    aDest.append(NS_ConvertASCIItoUCS2(kTxRomanNumbers[posValue +
                                                       mTableOffset]));
    // Tens
    posValue = aNumber / 10;
    aNumber %= 10;
    aDest.append(NS_ConvertASCIItoUCS2(kTxRomanNumbers[10 + posValue +
                                                       mTableOffset]));
    // Ones
    aDest.append(NS_ConvertASCIItoUCS2(kTxRomanNumbers[20 + aNumber +
                                                       mTableOffset]));
}
