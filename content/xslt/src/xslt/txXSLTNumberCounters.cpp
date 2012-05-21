/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txXSLTNumber.h"
#include "nsReadableUtils.h"
#include "txCore.h"

class txDecimalCounter : public txFormattedCounter {
public:
    txDecimalCounter() : mMinLength(1), mGroupSize(50)
    {
    }
    
    txDecimalCounter(PRInt32 aMinLength, PRInt32 aGroupSize,
                     const nsAString& mGroupSeparator);
    
    virtual void appendNumber(PRInt32 aNumber, nsAString& aDest);

private:
    PRInt32 mMinLength;
    PRInt32 mGroupSize;
    nsString mGroupSeparator;
};

class txAlphaCounter : public txFormattedCounter {
public:
    txAlphaCounter(PRUnichar aOffset) : mOffset(aOffset)
    {
    }

    virtual void appendNumber(PRInt32 aNumber, nsAString& aDest);
    
private:
    PRUnichar mOffset;
};

class txRomanCounter : public txFormattedCounter {
public:
    txRomanCounter(bool aUpper) : mTableOffset(aUpper ? 30 : 0)
    {
    }

    void appendNumber(PRInt32 aNumber, nsAString& aDest);

private:
    PRInt32 mTableOffset;
};


nsresult
txFormattedCounter::getCounterFor(const nsAFlatString& aToken,
                                  PRInt32 aGroupSize,
                                  const nsAString& aGroupSeparator,
                                  txFormattedCounter*& aCounter)
{
    PRInt32 length = aToken.Length();
    NS_ASSERTION(length, "getting counter for empty token");
    aCounter = 0;
    
    if (length == 1) {
        PRUnichar ch = aToken.CharAt(0);
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
                aCounter = new txDecimalCounter(1, aGroupSize,
                                                aGroupSeparator);
                break;
        }
        return aCounter ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    
    // for now, the only multi-char token we support are decimals
    PRInt32 i;
    for (i = 0; i < length-1; ++i) {
        if (aToken.CharAt(i) != '0')
            break;
    }
    if (i == length-1 && aToken.CharAt(i) == '1') {
        aCounter = new txDecimalCounter(length, aGroupSize, aGroupSeparator);
    }
    else {
        // if we don't recognize the token then use '1'
        aCounter = new txDecimalCounter(1, aGroupSize, aGroupSeparator);
    }

    return aCounter ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


txDecimalCounter::txDecimalCounter(PRInt32 aMinLength, PRInt32 aGroupSize,
                                   const nsAString& aGroupSeparator)
    : mMinLength(aMinLength), mGroupSize(aGroupSize),
      mGroupSeparator(aGroupSeparator)
{
    if (mGroupSize <= 0) {
        mGroupSize = aMinLength + 10;
    }
}

void txDecimalCounter::appendNumber(PRInt32 aNumber, nsAString& aDest)
{
    const PRInt32 bufsize = 10; //must be able to fit an PRInt32
    PRUnichar buf[bufsize];
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
        aDest.Append(PRUnichar('0'));
        --extraPos;
        if (extraPos % mGroupSize == 0) {
            aDest.Append(mGroupSeparator);
        }
    }

    // copy string to buffer
    if (mGroupSize >= bufsize - pos) {
        // no grouping will occur
        aDest.Append(buf + pos, (PRUint32)(bufsize - pos));
    }
    else {
        // append chars up to first grouping separator
        PRInt32 len = ((bufsize - pos - 1) % mGroupSize) + 1;
        aDest.Append(buf + pos, len);
        pos += len;
        while (bufsize - pos > 0) {
            aDest.Append(mGroupSeparator);
            aDest.Append(buf + pos, mGroupSize);
            pos += mGroupSize;
        }
        NS_ASSERTION(bufsize == pos, "error while grouping");
    }
}


void txAlphaCounter::appendNumber(PRInt32 aNumber, nsAString& aDest)
{
    PRUnichar buf[12];
    buf[11] = 0;
    PRInt32 pos = 11;
    while (aNumber > 0) {
        --aNumber;
        PRInt32 ch = aNumber % 26;
        aNumber /= 26;
        buf[--pos] = ch + mOffset;
    }
    
    aDest.Append(buf + pos, (PRUint32)(11 - pos));
}


const char* const kTxRomanNumbers[] =
    {"", "c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm",
     "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc",
     "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix",
     "", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM",
     "", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC",
     "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};

void txRomanCounter::appendNumber(PRInt32 aNumber, nsAString& aDest)
{
    // Numbers bigger then 3999 and negative numbers can't be done in roman
    if (PRUint32(aNumber) >= 4000) {
        txDecimalCounter().appendNumber(aNumber, aDest);
        return;
    }

    while (aNumber >= 1000) {
        aDest.Append(!mTableOffset ? PRUnichar('m') : PRUnichar('M'));
        aNumber -= 1000;
    }

    PRInt32 posValue;
    
    // Hundreds
    posValue = aNumber / 100;
    aNumber %= 100;
    AppendASCIItoUTF16(kTxRomanNumbers[posValue + mTableOffset], aDest);
    // Tens
    posValue = aNumber / 10;
    aNumber %= 10;
    AppendASCIItoUTF16(kTxRomanNumbers[10 + posValue + mTableOffset], aDest);
    // Ones
    AppendASCIItoUTF16(kTxRomanNumbers[20 + aNumber + mTableOffset], aDest);
}
