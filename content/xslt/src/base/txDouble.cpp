/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include "nsString.h"
#include "txCore.h"
#include "txXMLUtils.h"
#include <math.h>
#include <stdlib.h>
#include <algorithm>
#ifdef WIN32
#include <float.h>
#endif
#include "prdtoa.h"

/*
 * Utility class for doubles
 */

/*
 * Converts the given String to a double, if the String value does not
 * represent a double, NaN will be returned
 */
class txStringToDouble
{
public:
    typedef PRUnichar input_type;
    typedef PRUnichar value_type;
    txStringToDouble(): mState(eWhitestart), mSign(ePositive) {}

    void
    write(const input_type* aSource, uint32_t aSourceLength)
    {
        if (mState == eIllegal) {
            return;
        }
        uint32_t i = 0;
        PRUnichar c;
        for ( ; i < aSourceLength; ++i) {
            c = aSource[i];
            switch (mState) {
                case eWhitestart:
                    if (c == '-') {
                        mState = eDecimal;
                        mSign = eNegative;
                    }
                    else if (c >= '0' && c <= '9') {
                        mState = eDecimal;
                        mBuffer.Append((char)c);
                    }
                    else if (c == '.') {
                        mState = eMantissa;
                        mBuffer.Append((char)c);
                    }
                    else if (!XMLUtils::isWhitespace(c)) {
                        mState = eIllegal;
                        return;
                    }
                    break;
                case eDecimal:
                    if (c >= '0' && c <= '9') {
                        mBuffer.Append((char)c);
                    }
                    else if (c == '.') {
                        mState = eMantissa;
                        mBuffer.Append((char)c);
                    }
                    else if (XMLUtils::isWhitespace(c)) {
                        mState = eWhiteend;
                    }
                    else {
                        mState = eIllegal;
                        return;
                    }
                    break;
                case eMantissa:
                    if (c >= '0' && c <= '9') {
                        mBuffer.Append((char)c);
                    }
                    else if (XMLUtils::isWhitespace(c)) {
                        mState = eWhiteend;
                    }
                    else {
                        mState = eIllegal;
                        return;
                    }
                    break;
                case eWhiteend:
                    if (!XMLUtils::isWhitespace(c)) {
                        mState = eIllegal;
                        return;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    double
    getDouble()
    {
        if (mState == eIllegal || mBuffer.IsEmpty() ||
            (mBuffer.Length() == 1 && mBuffer[0] == '.')) {
            return MOZ_DOUBLE_NaN();
        }
        return mSign*PR_strtod(mBuffer.get(), 0);
    }
private:
    nsAutoCString mBuffer;
    enum {
        eWhitestart,
        eDecimal,
        eMantissa,
        eWhiteend,
        eIllegal
    } mState;
    enum {
        eNegative = -1,
        ePositive = 1
    } mSign;
};

double txDouble::toDouble(const nsAString& aSrc)
{
    txStringToDouble sink;
    nsAString::const_iterator fromBegin, fromEnd;
    copy_string(aSrc.BeginReading(fromBegin), aSrc.EndReading(fromEnd), sink);
    return sink.getDouble();
}

/*
 * Converts the value of the given double to a String, and places
 * The result into the destination String.
 * @return the given dest string
 */
void txDouble::toString(double aValue, nsAString& aDest)
{

    // check for special cases

    if (MOZ_DOUBLE_IS_NaN(aValue)) {
        aDest.AppendLiteral("NaN");
        return;
    }
    if (MOZ_DOUBLE_IS_INFINITE(aValue)) {
        if (aValue < 0)
            aDest.Append(PRUnichar('-'));
        aDest.AppendLiteral("Infinity");
        return;
    }

    // Mantissa length is 17, so this is plenty
    const int buflen = 20;
    char buf[buflen];

    int intDigits, sign;
    char* endp;
    PR_dtoa(aValue, 0, 0, &intDigits, &sign, &endp, buf, buflen - 1);

    // compute length
    int32_t length = endp - buf;
    if (length > intDigits) {
        // decimal point needed
        ++length;
        if (intDigits < 1) {
            // leading zeros, -intDigits + 1
            length += 1 - intDigits;
        }
    }
    else {
        // trailing zeros, total length given by intDigits
        length = intDigits;
    }
    if (aValue < 0)
        ++length;
    // grow the string
    uint32_t oldlength = aDest.Length();
    if (!aDest.SetLength(oldlength + length, mozilla::fallible_t()))
        return; // out of memory
    nsAString::iterator dest;
    aDest.BeginWriting(dest).advance(int32_t(oldlength));
    if (aValue < 0) {
        *dest = '-'; ++dest;
    }
    int i;
    // leading zeros
    if (intDigits < 1) {
        *dest = '0'; ++dest;
        *dest = '.'; ++dest;
        for (i = 0; i > intDigits; --i) {
            *dest = '0'; ++dest;
        }
    }
    // mantissa
    int firstlen = std::min<size_t>(intDigits, endp - buf);
    for (i = 0; i < firstlen; i++) {
        *dest = buf[i]; ++dest;
    }
    if (i < endp - buf) {
        if (i > 0) {
            *dest = '.'; ++dest;
        }
        for (; i < endp - buf; i++) {
            *dest = buf[i]; ++dest;
        }
    }
    // trailing zeros
    for (; i < intDigits; i++) {
        *dest = '0'; ++dest;
    }
}
