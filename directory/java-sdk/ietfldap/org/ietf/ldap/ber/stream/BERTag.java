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
 * This class is for the tagged object type. A nested tag is
 * allowed. A tagged element contains another
 * ber element.
 * <P>See CCITT X.209.
 *
 * <pre>
 * ENCODING RULE:
 *  tag    = whatever it is constructed with
 * </pre>
 *
 * @version 1.0
 */
public class BERTag extends BERElement {
    /**
     * Value of tag
     */
    private int m_tag = 0;
    /**
     * Value of element
     */
    private BERElement m_element = null;
    /**
     * Implicit or not
     */
    private boolean m_implicit = false;

    /**
     * Constructs a tag element.
     * @param tag tag value
     * @param element ber element
     * @param implicit tagged implicitly
     */
    public BERTag(int tag, BERElement element, boolean implicit) {
        m_tag = tag;
        m_element = element;
        m_implicit = implicit;
    }

    /**
     * Constructs a tag element from an input stream.
     * @param decoder decoder object for application-specific tags
     * @param tag tag value; already stripped from stream
     * @param stream source
     * @param bytes_read array of 1 int; incremented by number
     * of bytes read from stream
     * @exception IOException failed to construct
     */
    public BERTag(BERTagDecoder decoder, int tag, InputStream stream,
        int[] bytes_read) throws IOException {

        m_tag = tag;
        boolean[] implicit = new boolean[1];

        /*
         * Need to use user callback to decode contents of
         * a non-universal tagged value.
         */
        m_element = decoder.getElement(decoder, tag, stream, bytes_read,
                      implicit);
        m_implicit = implicit[0];
    }

    /**
     * Gets the element from the tagged object.
     * @return BER element.
     */
    public BERElement getValue() {
        return m_element;
    }

    /**
     * Sets the implicit tag. If it is an implicit tag,
     * the next element tag can be omitted (it will
     * not be sent to a stream or buffer).
     * @param value implicit flag
     */
    public void setImplicit(boolean value) {
        m_implicit = value;
    }

    /**
     * Sends the BER encoding directly to a stream.
     * @param stream output stream
     * @exception IOException failed to send
     */
    public void write(OutputStream stream) throws IOException {
        stream.write(m_tag);  /* bcm - assuming tag is one byte */

        ByteArrayOutputStream contents_stream = new ByteArrayOutputStream();
        m_element.write(contents_stream);
        byte[] contents_buffer = contents_stream.toByteArray();

        if (m_implicit) {
            /* Assumes tag is only one byte.  Rest of buffer is */
            /* length and contents of tagged element.           */
            stream.write(contents_buffer, 1, contents_buffer.length -1);
        } else {
            /* Send length */
            sendDefiniteLength(stream, contents_buffer.length);
            /* Send contents */
            stream.write(contents_buffer);
        }
    }

    /**
     * Gets the element type.
     * @return element type.
     */
    public int getType() {
        return BERElement.TAG;
    }

    /**
     * Gets the element tag.
     * @return element tag.
     */
    public int getTag() {
        return m_tag;
    }

    /**
     * Gets the string representation.
     * @return string representation of tag.
     */
    public String toString() {
        String s = "";
        if ((m_tag & 0xC0) == 0)
            /* bits 8 + 7 are zeros */
            s = s + "UNIVERSAL-";
        else if (((m_tag & 0x80) & (m_tag & 0x40)) > 0)
            /* bits 8 + 7 are ones */
            s = s + "PRIVATE-";
        else if ((m_tag & 0x40) > 0)
            /* bit 8 is zero, bit 7 is one */
            s = s + "APPLICATION-";
        else if ((m_tag & 0x80) > 0)
            /* bit 8 is one, bit 7 is zero */
            s = s + "CONTEXT-";
        return "[" + s + (m_tag&0x1f) + "] " + m_element.toString();
    }
}
