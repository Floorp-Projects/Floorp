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
 * This abstract class serves as a base class for constructed
 * types such as sequence or set.
 * <P>See CCITT X.209.
 *
 * @version 1.0
 */
public abstract class BERConstruct extends BERElement {
    /**
     * List of BER elements in the construct.
     */
    private Vector m_elements = new Vector();

    /**
     * Constructs a construct element.
     */
    public BERConstruct() {
    }

    /**
     * Constructs a construct element from an input stream.
     * @param decoder decoder for application specific BER
     * @param stream input stream from socket
     * @param bytes_read array of 1 int; value incremented by number
     * of bytes read from stream
     * @exception IOException failed to construct
     */
    public BERConstruct(BERTagDecoder decoder, InputStream stream,
        int[] bytes_read) throws IOException {
        int contents_length = super.readLengthOctets(stream,bytes_read);
        int[] component_length = new int[1];
        if (contents_length == -1) {
            /* Constructed - indefinite length */
            BERElement element = null;
            {
                component_length[0] = 0;
                element = getElement(decoder, stream, component_length);
                if (element != null)
                    addElement(element);
            } while (element != null);
        } else {
            /* Constructed - definite length */
            bytes_read[0] += contents_length;
            while (contents_length > 0)
            {
                component_length[0] = 0;
                addElement(getElement(decoder, stream,component_length));
                contents_length -= component_length[0];
            }
        }
    }


    /**
     * Adds an element to the list.
     * @return BER encoding of the element.
     */
    public void addElement(BERElement element) {
        m_elements.addElement(element);
    }

    /**
     * Retrieves number of elements.
     * @return number of elements.
     */
    public int size() {
        return m_elements.size();
    }

    /**
     * Gets ber element at specific position.
     * @param index index of the element to get
     * @return BER element.
     */
    public BERElement elementAt(int index) {
        return (BERElement)m_elements.elementAt(index);
    }

    /**
     * Sends the BER encoding directly to a stream.
     * @param stream output stream
     * @exception IOException failed to send
     */
    public void write(OutputStream stream) throws IOException {
        stream.write(getType());

        ByteArrayOutputStream contents_stream = new ByteArrayOutputStream();
        for (int i = 0; i < m_elements.size(); i++)  {
            BERElement e = elementAt(i);
            e.write(contents_stream);
        }
        byte[] contents_buffer = contents_stream.toByteArray();
        sendDefiniteLength(stream, contents_buffer.length);
        stream.write(contents_buffer);
    }

    /**
     * Gets the element type.
     * @param element type
     */
    public abstract int getType();

    /**
     * Gets the string representation.
     * @return string representation of tag.
     */
    public abstract String toString();
}
