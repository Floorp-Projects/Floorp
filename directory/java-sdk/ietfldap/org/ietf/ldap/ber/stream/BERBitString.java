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
import java.util.BitSet;
import java.io.*;

/**
 * This class is for the BitString object. Note that the BitSet class
 * has a bug: size() returns the size of the internal allocated memory
 * rather than the number of bits. Current work-around is to maintain
 * the number of bits ourselves in m_value_num_bits.
 * Change is required when BitSet is fixed.
 * <P>See CCITT X.209.
 *
 *
 * <pre>
 * ENCODING RULE:
 *   Primitive Definite length.
 *   tag = 0x03
 * </pre>
 *
 * @version 1.0
 */
public class BERBitString extends BERElement {
    /**
     * Internal variables
     */
    private BitSet m_value;
    private int m_value_num_bits;

    /**
     * Constructs a boolean element.
     * @param value boolean value
     */
    public BERBitString(BitSet value) {
        m_value = value;
    }

    /**
     * Constructs a bitstring element from an input stream
     * (for constructed encodings).
     * @param stream source
     * @param bytes_read array of 1 int; value incremented by
     * number of bytes read from stream
     * @exception IOException failed to construct
     */
    public BERBitString(BERTagDecoder decoder, InputStream stream,
                        int[] bytes_read) throws IOException {
        int octet;
        int contents_length = super.readLengthOctets(stream, bytes_read);
        int[] component_length = new int[1];
        BERElement element = null;

        if (contents_length == -1) {
            /* Constructed - indefinite length. */
            {
                component_length[0] = 0;
                element = getElement(decoder,stream,component_length);
                if (element != null) {
                    /* element is a bitstring - add it to the existing BitSet */
                    BERBitString bit_string_element = (BERBitString)element;

                    BitSet new_bit_set = new BitSet(m_value_num_bits +
                                          bit_string_element.getSize());

                    for (int i = 0; i<m_value_num_bits; i++)
                        if (m_value.get(i))
                            new_bit_set.set(i);
                    for (int j = 0; j<bit_string_element.getSize(); j++)
                        if (bit_string_element.getValue().get(j))
                            new_bit_set.set(m_value_num_bits+j);
                    m_value = new_bit_set;
                    m_value_num_bits += bit_string_element.getSize();
                }
            } while (element != null);
        } else {
            /* Constructed - definite length */
            bytes_read[0] += contents_length;
            while (contents_length > 0) {
                component_length[0] = 0;
                element = getElement(decoder,stream,component_length);
                if (element != null) {
                    /* element is a bitstring - add it to the existing BitSet */
                    BERBitString bit_string_element = (BERBitString)element;

                    BitSet new_bit_set = new BitSet(m_value_num_bits +
                                          bit_string_element.getSize());
                    for (int i = 0; i<m_value_num_bits; i++)
                        if (m_value.get(i))
                            new_bit_set.set(i);
                    for (int j = 0; j<bit_string_element.getSize(); j++)
                        if (bit_string_element.getValue().get(j))
                            new_bit_set.set(m_value_num_bits+j);
                    m_value = new_bit_set;
                    m_value_num_bits += bit_string_element.getSize();
                }
                contents_length -= component_length[0];
            }
        }
    }

    /**
     * Constructs a bitstring element from an input stream
     * (for primitive encodings).
     * @param stream source
     * @param bytes_read array of 1 int; value incremented by
     * number of bytes read from stream
     * @exception IOException failed to construct
     */
    public BERBitString(InputStream stream, int[] bytes_read)
               throws IOException {
        /* Primitive - definite length content octets string. */

        int octet;
        int contents_length = super.readLengthOctets(stream, bytes_read);


        /* First content octect doesn't encode any of
         * the string - it encodes the number of unused
         * bits in the final content octet.
         */
        int last_unused_bits = stream.read();
        bytes_read[0]++;
        contents_length--;

        m_value_num_bits = ((contents_length-1)*8) + (8-last_unused_bits);
        m_value = new BitSet();

        int bit_num = 0;
        for (int i = 0; i < contents_length-1; i++) {
            octet = stream.read();
            int mask = 0x80;
            for (int j = 0; j < 8; j++) {
                if ((octet & mask) > 0) {
                    m_value.set(bit_num);
                }
                else
                    m_value.clear(bit_num);
                bit_num++;
                mask = mask / 2;
            }
        }

        octet = stream.read();  /* last content octet */
        int mask = 0x80;
        for (int j = 0; j < 8-last_unused_bits; j++) {
            if ((octet & mask) > 0)
                m_value.set(bit_num);
            else
                m_value.clear(bit_num);
            bit_num++;
            mask = mask / 2;
        }

        bytes_read[0] += contents_length;
    }

    /**
     * Sends the BER encoding directly to a stream.
     * Always sends in primitive form.
     * @param stream output stream
     */
    public void write(OutputStream stream) throws IOException {
        stream.write(BERElement.BITSTRING);

        //int num_bits = m_value.size();   /* number of bits to send */
        int num_bits = m_value_num_bits;

        /* Number of bits unused int the last contents octet */
        int last_unused_bits = 8 - (num_bits % 8);

        /* Figure out the number of content octets */
        int num_content_octets = (int)(num_bits/8) + 1;
        if (last_unused_bits > 0)
            num_content_octets += 1;
        stream.write(num_content_octets);    /* length octet */

        stream.write(last_unused_bits);    /* first content octet */

        for (int i = 0; i < (int)(num_bits/8); i++) {
            int new_octet = 0;
            int bit = 0x80;
            for (int j = 0; j < 8; j++) {
                if (m_value.get(i*8+j))
                    new_octet += bit;
                bit = bit/2;
            }
            stream.write(new_octet);
        }

        /*
         * Last octet may not use all bits. If last octet DOES use all
         * bits then it has already been written above.
         */
        if (last_unused_bits > 0) {
            int new_octet = 0;
            int bit = 0x80;
            for (int j = 0; j < last_unused_bits; j++) {
                if (m_value.get(((int)(num_bits/8))*8+j))
                    new_octet += bit;
                bit = bit/2;
            }
            stream.write(new_octet);
        }
    }

    /**
     * Gets the bitstring value.
     * @param element type
     */
    public BitSet getValue() {
        return m_value;
    }

    /**
     * Gets the number of bits.
     * @return bit numbers.
     */
    public int getSize() {
        return m_value_num_bits;
    }

    /**
     * Gets the element type.
     * @param element type
     */
    public int getType() {
        return BERElement.BITSTRING;
    }

    /**
     * Gets the string representation.
     * @return string representation of tag.
     */
    public String toString() {
        String hex_string = "";
        int octet;

        //int num_bits = m_value.size();
        int num_bits = m_value_num_bits;
        for (int i = 0; i < (int)(num_bits/8); i++) {
            octet = 0;
            int bit = 0x80;
            for (int j = 0; j < 8; j++) {
                if (m_value.get(i*8+j))
                    octet += bit;
                bit = bit/2;
            }
            hex_string += " " + (byte)octet;
        }

        int bit = 0x80;
        octet = 0;
        for (int k = 0; k < num_bits-(int)(num_bits/8); k++) {
            if (m_value.get(((int)(num_bits/8))*8+k))
                octet += bit;
            bit = bit/2;
        }
        hex_string += " " + (byte)octet;

        return "Bitstring {" + hex_string + " }";
    }
}
