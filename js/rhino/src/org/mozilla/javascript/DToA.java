/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Patrick Beard
 * Norris Boyd
 * Roger Lawrence
 * Frank Mitchell
 * Andrew Wason
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript;

import java.math.BigInteger;

class DToA {


/* "-0.0000...(1073 zeros after decimal point)...0001\0" is the longest string that we could produce,
 * which occurs when printing -5e-324 in binary.  We could compute a better estimate of the size of
 * the output string and malloc fewer bytes depending on d and base, but why bother? */

    static final int DTOBASESTR_BUFFER_SIZE = 1078;

    static char BASEDIGIT(int digit) {
        return (char)((digit >= 10) ? 'a' - 10 + digit : '0' + digit);
    }

    static byte[] byteValue(long x)
    {
        byte result[] = new byte[8];
        for (int i = 7; i >= 0; i--) {
            result[i] = (byte)(x & 0xFF);
            x >>= 4;
        }
        return result;
    }

    static final int Frac_mask = 0xfffff;
    static final int Exp_shift = 20;
    static final int Exp_msk1 = 0x100000;
    static final int Bias = 1023;
    static final int P = 53;

    static final int Exp_shift1 = 20;
    static final int Exp_mask  = 0x7ff00000;
    static final int Bndry_mask  = 0xfffff;
    static final int Log2P = 1;


    static int lo0bits(int y)
    {
        int k;
        int x = y;

        if ((x & 7) != 0) {
            if ((x & 1) != 0)
                return 0;
            if ((x & 2) != 0) {
                return 1;
            }
            return 2;
        }
        k = 0;
        if ((x & 0xffff) == 0) {
            k = 16;
            x >>>= 16;
        }
        if ((x & 0xff) == 0) {
            k += 8;
            x >>>= 8;
        }
        if ((x & 0xf) == 0) {
            k += 4;
            x >>>= 4;
        }
        if ((x & 0x3) == 0) {
            k += 2;
            x >>>= 2;
        }
        if ((x & 1) == 0) {
            k++;
            x >>>= 1;
            if ((x & 1) == 0)
                return 32;
        }
        return k;
    }

    static void stuffBits(byte bits[], int offset, int val)
    {
        bits[offset] = (byte)(val >> 24);
        bits[offset + 1] = (byte)(val >> 16);
        bits[offset + 2] = (byte)(val >> 8);
        bits[offset + 3] = (byte)(val);
    }

    /* converts d to bits * 2^e format */
    static int d2b(double d, byte bits[])
    {
        int e, k, y, z, de;
        long dBits = Double.doubleToLongBits(d);
        int d0 = (int)(dBits >>> 32);
        int d1 = (int)(dBits);

        z = d0 & Frac_mask;
        d0 &= 0x7fffffff;   /* clear sign bit, which we ignore */

        if ((de = (int)(d0 >>> Exp_shift)) != 0)
            z |= Exp_msk1;

        if ((y = d1) != 0) {
            k = lo0bits(y);
            y >>>= k;
            if (k != 0) {
                stuffBits(bits, 4, y | z << (32 - k));
                z >>= k;
            }
            else
                stuffBits(bits, 4, y);
            stuffBits(bits, 0, z);
        }
        else {
    //        JS_ASSERT(z);
            k = lo0bits(z);
            z >>>= k;
            stuffBits(bits, 4, z);
            k += 32;
        }
        if (de != 0) {
            e = de - Bias - (P-1) + k;
        }
        else {
            e = de - Bias - (P-1) + 1 + k;
        }
        return e;
    }

    public static String JS_dtobasestr(int base, double d)
    {
        char[] buffer;       /* The output string */
        int p;               /* index to current position in the buffer */
        int pInt;            /* index to the beginning of the integer part of the string */

        int q;
        int digit;
        double di;           /* d truncated to an integer */
        double df;           /* The fractional part of d */

//        JS_ASSERT(base >= 2 && base <= 36);

        buffer = new char[DTOBASESTR_BUFFER_SIZE];

        p = 0;
        if (d < 0.0) {
            buffer[p++] = '-';
            d = -d;
        }

        /* Check for Infinity and NaN */
        if (Double.isNaN(d))
            return "NaN";
        else
            if (Double.isInfinite(d))
                return "Infinity";

        /* Output the integer part of d with the digits in reverse order. */
        pInt = p;
        di = (int)d;
        BigInteger b = new BigInteger(byteValue((long)di));
        String intDigits = b.toString(base);
        intDigits.getChars(0, intDigits.length(), buffer, p);
        p += intDigits.length();

        df = d - di;
        if (df != 0.0) {
            /* We have a fraction. */

            byte bits[] = new byte[8];

            buffer[p++] = '.';

            long dBits = Double.doubleToLongBits(d);
            int word0 = (int)(dBits >> 32);
            int word1 = (int)(dBits);

            int e = d2b(df, bits);
            b = new BigInteger(bits);
//            JS_ASSERT(e < 0);
            /* At this point df = b * 2^e.  e must be less than zero because 0 < df < 1. */

            int s2 = -(word0 >>> Exp_shift1 & Exp_mask >> Exp_shift1);
            if (s2 == 0)
                s2 = -1;
            s2 += Bias + P;
            /* 1/2^s2 = (nextDouble(d) - d)/2 */
//            JS_ASSERT(-s2 < e);
            bits = new byte[1];
            bits[0] = 1;
            BigInteger mlo = new BigInteger(bits);
            BigInteger mhi = mlo;
            if ((word1 == 0) && ((word0 & Bndry_mask) == 0)
                && ((word0 & (Exp_mask & Exp_mask << 1)) != 0)) {
                /* The special case.  Here we want to be within a quarter of the last input
                   significant digit instead of one half of it when the output string's value is less than d.  */
                s2 += Log2P;
                bits[0] = 1<<Log2P;
                mhi = new BigInteger(bits);
            }

            b = b.shiftLeft(e + s2);
            bits[0] = 1;
            BigInteger s = new BigInteger(bits);
            s = s.shiftLeft(s2);
            /* At this point we have the following:
             *   s = 2^s2;
             *   1 > df = b/2^s2 > 0;
             *   (d - prevDouble(d))/2 = mlo/2^s2;
             *   (nextDouble(d) - d)/2 = mhi/2^s2. */
            bits[0] = (byte)base;       // s'ok since base < 36
            BigInteger bigBase = new BigInteger(bits);

            System.out.println("s2 = " + s2 + ", e = " + e + ", b = " + b.toString());
            System.out.println("mhi = " + mhi.toString());
            System.out.println("mlo = " + mlo.toString());

            boolean done = false;
            do {
                b = b.multiply(bigBase);
                BigInteger[] divResult = b.divideAndRemainder(s);
                b = divResult[1];
                digit = (char)(divResult[0].intValue());
                if (mlo == mhi)
                    mlo = mhi = mlo.multiply(bigBase);
                else {
                    mlo = mlo.multiply(bigBase);
                    mhi = mhi.multiply(bigBase);
                }

                /* Do we yet have the shortest string that will round to d? */
                int j = b.compareTo(mlo);
                /* j is b/2^s2 compared with mlo/2^s2. */
                BigInteger delta = s.subtract(mhi);
                int j1 = (delta.signum() <= 0) ? 1 : b.compareTo(delta);
                /* j1 is b/2^s2 compared with 1 - mhi/2^s2. */
                if (j1 == 0 && ((word1 & 1) == 0)) {
                    if (j > 0)
                        digit++;
                    done = true;
                } else
                if (j < 0 || (j == 0 && ((word1 & 1) == 0))) {
                    if (j1 > 0) {
                        /* Either dig or dig+1 would work here as the least significant digit.
                           Use whichever would produce an output value closer to d. */
                        b = b.shiftLeft(1);
                        j1 = b.compareTo(s);
                        if (j1 > 0) /* The even test (|| (j1 == 0 && (digit & 1))) is not here because it messes up odd base output
                                     * such as 3.5 in base 3.  */
                            digit++;
                    }
                    done = true;
                } else if (j1 > 0) {
                    digit++;
                    done = true;
                }
//                JS_ASSERT(digit < (uint32)base);
                buffer[p++] = BASEDIGIT(digit);
            } while (!done);
        }

        return new String(buffer, 0, p);
    }

}

