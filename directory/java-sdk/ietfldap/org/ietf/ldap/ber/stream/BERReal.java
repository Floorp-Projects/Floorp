/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap.ber.stream;

import java.util.*;
import java.io.*;

/**
 * This class is for the Real object.
 * <P>See CCITT X.209.
 *
 * <pre>
 * ENCODING RULE:
 *  tag    = 0x09
 * </pre>
 *
 * @version 1.0
 */
public class BERReal extends BERElement {
    /**
     * Constants: special ASN.1 real values
     */
    public final static float PLUS_INFINITY = 1.0f/0.0f;
    public final static float MINUS_INFINITY = -1.0f/0.0f;

    /**
     * Value of element
     */
    private float m_value = 0;

    /**
     * Constructs a real element with a value
     * @param value float value
     */
    public BERReal(float value) {
        m_value = value;
    }

    /**
     * Constructs a real element from an input stream.
     * @param stream source
     * @param bytes_read array of 1 int; value incremented by
     *        number of bytes read from stream.
     * @exception IOException failed to construct
     */
    public BERReal(InputStream stream, int[] bytes_read) throws IOException {
        int length = super.readLengthOctets(stream, bytes_read);

        if (length == 0)
            /* zero length indicates value is zero */
            m_value = 0;
        else {
            int octet = stream.read(); /* first content octet */
            bytes_read[0]++;

            if (octet == 0x40)  /* bit 8 = 1 */
                m_value = PLUS_INFINITY;
            else if (octet == 0x41)  /* bit 8 = 1 and bit 1 = 1 */
                m_value = MINUS_INFINITY;
            else if ((octet & 0x80) > 0) {
                /* bit 8 = 1 */
                /* Binary encoding */
                /* M = S*N*2F where S = -1 or 1, 0<= F <= 3. */
                int sign;
                int base;
                int number;
                int f;        /* binary scaling factor */
                int exponent;
                int mantissa;
                int num_exponent_octets;
                int contents_length_left;

                if ((octet & 0x40) > 0) sign = -1;
                else sign = 1;

                if ((octet & 0x20) > 0) {
                    if ((octet & 0x10) > 0) {
                        /* bits 6+5 = 11 */
                        /* reserved for future use */
                        base = 0;
                    } else {
                        /* bits 6+5 = 10 */
                        base = 16;
                    }
                } else if ((octet & 0x10) > 0) {
                    /* bits 6+5 = 01 */
                    base = 8;
                } else {
                    /* bits 6+5 = 00 */
                    base = 2;
                }

                if ((octet & 0x08) > 0) {
                    if ((octet & 0x04) > 0) {
                        /* bits 4+3 = 11 */
                        f = 3;
                    } else {
                        /* bits 4+3 = 10 */
                        f = 2;
                    }
                } else if ((octet & 0x04) > 0) {
                    /* bits 4+3 = 01 */
                    f = 1;
                } else {
                    /* bits 4+3 = 00 */
                    f = 0;
                }

                if ((octet & 0x02) > 0) {
                    if ((octet & 0x01) > 0) {
                        /* bits 2+1 = 11 */
                        /* Following octet encodes the number of octets used to
                         * encode the exponent.
                         */
                        num_exponent_octets = stream.read();
                        bytes_read[0]++;
                        exponent = readTwosComplement(stream,bytes_read,num_exponent_octets);
                    } else {
                        /* bits 2+1 = 10 */
                        num_exponent_octets = 3;
                        exponent = readTwosComplement(stream,bytes_read,num_exponent_octets);
                    }
                } else if ((octet & 0x01) > 0) {
                    /* bits 2+1 = 01 */
                    num_exponent_octets = 2;
                    exponent = readTwosComplement(stream,bytes_read,num_exponent_octets);
                } else {
                    /* bits 2+1 = 00 */
                    num_exponent_octets = 1;
                    exponent = readTwosComplement(stream,bytes_read,num_exponent_octets);
                }

                contents_length_left = length - 1 - num_exponent_octets;
                number = readUnsignedBinary(stream, bytes_read, contents_length_left);

                mantissa = (int)(sign * number * Math.pow(2, f));
                m_value = mantissa * (float)Math.pow((double)base,(double)exponent);
            } else {
                /* bits 8 + 7 = 00 */
                /* ISO 6093 field */
                /*** NOTE: It has been agreed that this feature will
                 *         not be provided right now.
                 */
                 throw new IOException("real ISO6093 not supported. ");
            }
        }
    }

    /**
     * Sends the BER encoding directly to a stream.
     * @param stream output stream
     * @exception IOException failed to write
     */
    public void write(OutputStream stream) throws IOException {
        if (m_value == 0) {
            stream.write(BERElement.REAL);
            stream.write(0x00);     /* length */
        } else if (m_value == PLUS_INFINITY) {
            stream.write(BERElement.REAL);
            stream.write(0x01);     /* length */
            stream.write(0x40);     /* contents */
        } else if (m_value == MINUS_INFINITY) {
            stream.write(BERElement.REAL);
            stream.write(0x01);     /* length */
            stream.write(0x41);     /* contents */
        } else {
            /* Non-special real value */
            /* NOTE: currently always sends as a base 2 binary encoding
             * (see X.2 09 section 10.5.)
             *
             * M = S * N * 2F
             *
             * Simple encoding always uses F = 1.
             */
             // bcm - incomplete.
        }
    }

    /**
     * Gets the element type.
     * @return element type
     */
    public int getType() {
        return BERElement.REAL;
    }

    /**
     * Gets the string representation.
     * @return string representation of tag
     */
    public String toString() {
        return "Real {" + m_value + "}";
    }
}
