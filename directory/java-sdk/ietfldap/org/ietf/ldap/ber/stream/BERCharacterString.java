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
 * This is an abstract base class for character string types.
 *
 * @version 1.0
 */
public abstract class BERCharacterString extends BERElement {
    /**
     * Internal variables
     */
    protected String m_value = null;

    /**
     * Constructs a character string element containing a buffer.
     */
    public BERCharacterString() {
    }

    /**
     * Constructs a character string element containing buffer.
     * @param buffer a string value
     */
    public BERCharacterString(String string) {
        m_value = string;
    }

    /**
     * Constructs a character string element from a byte array.
     * @param buffer buffer containing UTF8 data
     */
    public BERCharacterString(byte[] buffer) {
        try{
            m_value = new String(buffer,"UTF8");
        } catch(Throwable x)
        {}
    }

    /**
     * Constructs a character string element from an input stream
     * (for constructed encoding)
     * @param stream input stream
     * @param bytes_read array of 1 int, incremented by number of bytes read
     * @exception IOException failed to construct
     */
    public BERCharacterString(BERTagDecoder decoder, InputStream stream,
                        int[] bytes_read) throws IOException {
        int octet;
        int contents_length = super.readLengthOctets(stream, bytes_read);
        int[] component_length = new int[1];
        BERElement element = null;

        if (contents_length == -1) {
            /* Constructed - indefinite length content octets. */
            do {
                component_length[0] = 0;
                element = getElement(decoder,stream,component_length);
                if (element != null) {
                    /* element is a string of same type
                     * - add it to the existing buffer */
                    BERCharacterString octet_element = (BERCharacterString)element;
                    String string_buffer = octet_element.getValue();
                    if (m_value == null) {
                        m_value = string_buffer;
                    } else {
                        m_value = m_value + string_buffer;
                    }
                }
            } while (element != null);
        } else {
            /* Definite length content octets string. */
            bytes_read[0] += contents_length;
            while (contents_length > 0) {
                component_length[0] = 0;
                element = getElement(decoder,stream,component_length);
                if (element != null) {
                    /* element is a string of the same type
                     * - add it to the existing buffer */
                    BERCharacterString octet_element = (BERCharacterString)element;
                    String string_buffer = octet_element.getValue();
                    if (m_value == null) {
                        m_value = string_buffer;
                    } else {
                        m_value = m_value + string_buffer;
                    }
                }
                contents_length -= component_length[0];
            }
        }
    }

    /**
     * Constructs a character string element from an input stream
     * (for primitive encoding)
     * @param stream source
     * @param bytes_read array of 1 int, incremented by number of bytes read
     * @exception IOException failed to construct
     */
    public BERCharacterString(InputStream stream, int[] bytes_read)
        throws IOException {
        int contents_length = super.readLengthOctets(stream, bytes_read);

        /* Definite length content octets string. */
        if (contents_length > 0) {
            byte[] buffer = new byte[contents_length];
            for (int i = 0; i < contents_length; i++) {
                buffer[i] = (byte) stream.read();
            }
            bytes_read[0] += contents_length;
            try {
                m_value = new String(buffer,"UTF8");
            } catch(Throwable x)
            {}
        }
    }

    private byte[] byte_buf;

    /**
     * Writes BER to stream.
     * @param stream output stream
     */
    public void write(OutputStream stream) throws IOException {
      stream.write(getType());  /* tag */
        if (m_value == null) {
            sendDefiniteLength(stream, 0);
        } else {
            try {
                byte_buf =  m_value.getBytes("UTF8");
                sendDefiniteLength(stream, byte_buf.length);  /* length */
            } catch(Throwable x)
            {}
            stream.write(byte_buf,0,byte_buf.length);  /* contents */
        }
    }

    /**
     * Gets the element value.
     * @param element value
     */
    public String getValue() {
        return m_value;
    }

    /**
     * Gets the element type.
     * @param element type
     */
    public abstract int getType();

    /**
     * Gets the string representation.
     * @return string representation.
     */
    public abstract String toString();
}
