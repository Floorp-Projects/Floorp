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
    
    txDecimalCounter(int32_t aMinLength, int32_t aGroupSize,
                     const nsAString& mGroupSeparator);
    
    virtual void appendNumber(int32_t aNumber, nsAString& aDest);

private:
    int32_t mMinLength;
    int32_t mGroupSize;
    nsString mGroupSeparator;
};

class txAlphaCounter : public txFormattedCounter {
public:
    explicit txAlphaCounter(char16_t aOffset) : mOffset(aOffset)
    {
    }

    virtual void appendNumber(int32_t aNumber, nsAString& aDest);
    
private:
    char16_t mOffset;
};

class txRomanCounter : public txFormattedCounter {
public:
    explicit txRomanCounter(bool aUpper) : mTableOffset(aUpper ? 30 : 0)
    {
    }

    void appendNumber(int32_t aNumber, nsAString& aDest);

private:
    int32_t mTableOffset;
};


nsresult
txFormattedCounter::getCounterFor(const nsAFlatString& aToken,
                                  int32_t aGroupSize,
                                  const nsAString& aGroupSeparator,
                                  txFormattedCounter*& aCounter)
{
    int32_t length = aToken.Length();
    NS_ASSERTION(length, "getting counter for empty token");
    aCounter = 0;
    
    if (length == 1) {
        char16_t ch = aToken.CharAt(0);
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
    int32_t i;
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


txDecimalCounter::txDecimalCounter(int32_t aMinLength, int32_t aGroupSize,
                                   const nsAString& aGroupSeparator)
    : mMinLength(aMinLength), mGroupSize(aGroupSize),
      mGroupSeparator(aGroupSeparator)
{
    if (mGroupSize <= 0) {
        mGroupSize = aMinLength + 10;
    }
}

void txDecimalCounter::appendNumber(int32_t aNumber, nsAString& aDest)
{
    const int32_t bufsize = 10; //must be able to fit an int32_t
    char16_t buf[bufsize];
    int32_t pos = bufsize;
    while (aNumber > 0) {
        int32_t ch = aNumber % 10;
        aNumber /= 10;
        buf[--pos] = ch + '0';
    }

    // in case we didn't get a long enough string
    int32_t end  = (bufsize > mMinLength) ? bufsize - mMinLength : 0;
    while (pos > end) {
        buf[--pos] = '0';
    }
    
    // in case we *still* didn't get a long enough string.
    // this should be very rare since it only happens if mMinLength is bigger
    // then the length of any int32_t.
    // pos will always be zero 
    int32_t extraPos = mMinLength;
    while (extraPos > bufsize) {
        aDest.Append(char16_t('0'));
        --extraPos;
        if (extraPos % mGroupSize == 0) {
            aDest.Append(mGroupSeparator);
        }
    }

    // copy string to buffer
    if (mGroupSize >= bufsize - pos) {
        // no grouping will occur
        aDest.Append(buf + pos, (uint32_t)(bufsize - pos));
    }
    else {
        // append chars up to first grouping separator
        int32_t len = ((bufsize - pos - 1) % mGroupSize) + 1;
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


void txAlphaCounter::appendNumber(int32_t aNumber, nsAString& aDest)
{
    char16_t buf[12];
    buf[11] = 0;
    int32_t pos = 11;
    while (aNumber > 0) {
        --aNumber;
        int32_t ch = aNumber % 26;
        aNumber /= 26;
        buf[--pos] = ch + mOffset;
    }
    
    aDest.Append(buf + pos, (uint32_t)(11 - pos));
}


const char* const kTxRomanNumbers[] =
    {"", "c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm",
     "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc",
     "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix",
     "", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM",
     "", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC",
     "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};

void txRomanCounter::appendNumber(int32_t aNumber, nsAString& aDest)
{
    // Numbers bigger then 3999 and negative numbers can't be done in roman
    if (uint32_t(aNumber) >= 4000) {
        txDecimalCounter().appendNumber(aNumber, aDest);
        return;
    }

    while (aNumber >= 1000) {
        aDest.Append(!mTableOffset ? char16_t('m') : char16_t('M'));
        aNumber -= 1000;
    }

    int32_t posValue;
    
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
