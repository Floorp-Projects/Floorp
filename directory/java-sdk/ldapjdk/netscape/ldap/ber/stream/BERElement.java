/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap.ber.stream;

import java.util.*;
import java.io.*;

/**
 * This class is for the tagged object. Nested tag is
 * allowed. A tagged element contains another BER element.
 *
 * @version 1.0
 * @seeAlso CCITT X.209
 */
public abstract class BERElement {
    /**
     * Possible element types.
     */
    public final static int BOOLEAN = 0x01;
    public final static int INTEGER = 0x02;
    public final static int BITSTRING = 0x03;
    public final static int OCTETSTRING = 0x04;
    public final static int NULL = 0x05;
    public final static int OBJECTID = 0x06;
    public final static int REAL = 0x09;
    public final static int ENUMERATED = 0x0a;
    public final static int SET = 0x31;          /* always constructed */
    public final static int SEQUENCE = 0x30;     /* always constructed */
    public final static int NUMERICSTRING = 0x12;
    public final static int PRINTABLESTRING = 0x13;
    public final static int TELETEXSTRING = 0x14;
    public final static int VIDEOTEXSTRING = 0x15;
    public final static int IA5STRING = 0x16;
    public final static int UTCTIME = 0x17;
    public final static int GRAPHICSTRING = 0x19;
    public final static int VISIBLESTRING = 0x1A;
    public final static int GENERALSTRING = 0x1B;

    /**
    * Internal (non-transmitted) tags.
    */
    public final static int TAG = -1;
    public final static int CHOICE = -2;
    public final static int ANY = -3;

    /**
     * Possible tags.
     */
    public final static int EOC = 0x00;  /* End Of Construction */
    public final static int UNIVERSAL = 0x00;
    public final static int APPLICATION = 0x40;
    public final static int CONTEXT = 0x80;
    public final static int SASLCONTEXT = 0xa0;
    public final static int PRIVATE = 0xC0;
    public final static int PRIMITIVE = 0x00;
    public final static int CONSTRUCTED = 0x20;

    public final static int MRA_OID = 0x01;
    public final static int MRA_TYPE = 0x02;
    public final static int MRA_VALUE = 0x03;
    public final static int MRA_DNATTRS = 0x04;
    public final static int EXOP_REQ_OID = 0x00;
    public final static int EXOP_REQ_VALUE = 0x01;
    public final static int EXOP_RES_OID = 0x0a;
    public final static int EXOP_RES_VALUE = 0x0b;
    public final static int SK_MATCHRULE = 0x00;
    public final static int SK_REVERSE = 0x01;
    public final static int SR_ATTRTYPE = 0x00;

    /**
     * Gets a ber element from the input stream.
     * @param decoder decoder for application specific ber
     * @param stream stream where ber encoding comes
     * @param bytes_read array of 1 int; value incremented by
     *        number of bytes fread from stream.
     * @exception IOException failed to decode an element.
     */
    public static BERElement getElement(BERTagDecoder decoder,
        InputStream stream, int[] bytes_read) throws IOException {

        BERElement element = null;
        int tag = stream.read();
        bytes_read[0] = 1;

        if (tag == EOC) {
            stream.read();    /* length octet (always zero) */
            bytes_read[0] = 1;
            element = null;
        } else if (tag == BOOLEAN)  {
            element = new BERBoolean(stream, bytes_read);
        } else if (tag == INTEGER) {
            element = new BERInteger(stream, bytes_read);
        } else if (tag == BITSTRING) {
            element = new BERBitString(stream, bytes_read);
        } else if (tag == (BITSTRING | CONSTRUCTED)) {
            element = new BERBitString(decoder, stream, bytes_read);
        } else if (tag == OCTETSTRING) {
            element = new BEROctetString(stream, bytes_read);
        } else if (tag == (OCTETSTRING | CONSTRUCTED)) {
            element = new BEROctetString(decoder, stream, bytes_read);
        } else if (tag == NULL) {
            element = new BERNull(stream, bytes_read);
        } else if (tag == OBJECTID) {
            element = new BERObjectId(stream, bytes_read);
        } else if (tag == REAL) {
            element = new BERReal(stream, bytes_read);
        } else if (tag == ENUMERATED) {
            element = new BEREnumerated(stream, bytes_read);
        } else if (tag == SEQUENCE) {
            element = new BERSequence(decoder, stream, bytes_read);
        } else if (tag == SET) {
            element = new BERSet(decoder, stream, bytes_read);
        } else if (tag == NUMERICSTRING) {
            element = new BERNumericString(stream, bytes_read);
        } else if (tag == (NUMERICSTRING | CONSTRUCTED)) {
            element = new BERNumericString(decoder, stream, bytes_read);
        } else if (tag == PRINTABLESTRING) {
            element = new BERPrintableString(stream, bytes_read);
        } else if (tag == (PRINTABLESTRING | CONSTRUCTED)) {
            element = new BERPrintableString(decoder, stream, bytes_read);
        } else if (tag == UTCTIME) {
            element = new BERUTCTime(stream, bytes_read);
        } else if (tag == (UTCTIME | CONSTRUCTED)) {
            element = new BERUTCTime(decoder, stream, bytes_read);
        } else if (tag == VISIBLESTRING) {
            element = new BERVisibleString(stream, bytes_read);
        } else if (tag == (VISIBLESTRING | CONSTRUCTED)) {
            element = new BERVisibleString(decoder, stream, bytes_read);
        } else if ((tag & (APPLICATION | PRIVATE | CONTEXT)) > 0) {
            element = new BERTag(decoder, tag, stream, bytes_read);
        } else
            throw new IOException("invalid tag " + tag);

        return element;
    }

    /**
     * Decodes length octets from stream.
     * @param stream input stream to read from
     * @param bytes_read array of 1 int; value incremented by
     *        number of bytes read from stream.
     * @return length of contents or -1 if indefinite length.
     * @exception IOException failed to read octets
     */
    public static int readLengthOctets(InputStream stream, int[] bytes_read)
        throws IOException {
        int contents_length = 0;
        int octet = stream.read();
        bytes_read[0]++;

        if (octet == 0x80)
            /* Indefinite length */
            contents_length = -1;
        else if ((octet & 0x80) > 0) {
            /* Definite (long form) - num octets encoded in 7 rightmost bits */
            int num_length_octets = (octet & 0x7F);

            for (int i = 0; i < num_length_octets; i++) {
                octet = stream.read();
                bytes_read[0]++;
                contents_length = (contents_length<<8) + octet;
            }
        } else {
            /* Definite (short form) - one length octet.  Value encoded in   */
            /* 7 rightmost bits.                                             */
            contents_length = octet;
        }
        return contents_length;
    }

    /**
     * Writes length octets (definite length only) to stream.
     * Uses shortform whenever possible.
     * @param stream output stream to write to.
     * @param num_content_octets value to be encode into length octets.
     * @return number of bytes sent to stream.
     * @exception IOException failed to read octets
     */
    public static void sendDefiniteLength(OutputStream stream,
        int num_content_octets) throws IOException {

        int bytes_written = 0;

        if (num_content_octets <= 127) {
            /* Use short form */
            stream.write(num_content_octets);
        } else {
            /* Using long form:
             * Need to determine how many octets are required to
             * encode the length.
             */
            int num_length_octets = 0;
            int num = num_content_octets;
            while (num >= 0) {
                num_length_octets++;
                num = (num>>8);
                if (num <= 0)
                    break;
            }

            byte[] buffer = new byte[num_length_octets+1];
            buffer[0] = (byte)(0x80 | num_length_octets);

            num = num_content_octets;
            for (int i = num_length_octets; i > 0; i--) {
                buffer[i] = (byte)(num & 0xFF);
                num = (num>>8);
            }

            stream.write(buffer);
        }
    }

    /**
     * Reads the unsigned binary from the input stream.
     * @param stream inputstream
     * @param byte_read number of byte read
     * @param length length of the byte to be read
     * @return the value of the two complement
     * @exception IOException failed to read octets
     */
    protected int readUnsignedBinary(InputStream stream,
                 int[] bytes_read, int length) throws IOException {
        int value = 0;
        int octet;

        for (int i = 0; i < length; i++) {
            octet = stream.read();
            bytes_read[0]++;
            value = (value<<8) + octet;
        }

        return value;
    }

    /**
     * Reads the two complement representation from the input stream.
     * @param stream inputstream
     * @param byte_read number of byte read
     * @param length length of the byte to be read
     * @return the value of the two complement
     * @exception IOException failed to read octets
     */
    protected int readTwosComplement(InputStream stream,
                 int[] bytes_read, int length) throws IOException {
        int value = 0;
        if (length > 0) {
            boolean negative = false;
            int octet = stream.read();
            bytes_read[0]++;
            if ((octet & 0x80) > 0)  /* left-most bit is 1. */
                negative = true;

            for (int i = 0; i < length; i++) {
                if (i > 0) {
                    octet = stream.read();
                    bytes_read[0]++;
                }
                if (negative)
                    value = (value<<8) + (int)(octet^0xFF)&0xFF;
                else
                    value = (value<<8) + (int)(octet&0xFF);
            }
            if (negative)  /* convert to 2's complement */
                value = (value + 1) * -1;
        }
        return value;
    }

    /**
     * Converts byte to hex string.
     * @param value byte value
     * @return string representation of Hex String
     */
    public String byteToHexString(byte value) {
        if (value < 0)
            return Integer.toHexString((value & 0x7F) + 128);
        else
            return Integer.toHexString(value);
    }

    /**
     * Sends the BER encoding directly to stream.
     * @param stream output stream.
     * @return bytes written to stream.
     */
    public abstract void write(OutputStream stream) throws IOException;

    /**
     * Gets the element type.
     * @return element type
     */
    public abstract int getType();

    /**
     * Gets the string representation.
     * @return string representation of an element
     */
    public abstract String toString();
}
