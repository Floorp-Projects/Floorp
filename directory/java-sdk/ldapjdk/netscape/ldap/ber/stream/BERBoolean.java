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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap.ber.stream;

import java.util.*;
import java.io.*;

/**
 * This class is for the Boolean object.
 *
 * <pre>
 * ENCODING RULE:
 *  tag    = 0x01
 *  length = 0x01
 *  one contents octet (non-zero indicates TRUE).
 *
 * Example 1:  (false)
 *   01 01 00
 * Example 2:  (true)
 *   01 01 FF
 * </pre>
 *
 * @version 1.0
 * @seeAlso CCITT X.209
 */
public class BERBoolean extends BERElement {
    /**
     * Internal variables
     */
    private boolean m_value = true;

    /**
     * Constructs a boolean element.
     * @param value boolean value
     */
    public BERBoolean(boolean value) {
        m_value = value;
    }

    /**
     * Constructs a boolean element from an input stream.
     * @param stream source
     * @param bytes_read array of 1 int; value incremented by
     *        number of bytes read from stream.
     * @exception IOException failed to construct
     */
    public BERBoolean(InputStream stream, int[] bytes_read) throws IOException {
        int octet = stream.read();  /* skip length */
        bytes_read[0]++;
        octet = stream.read();      /* content octet */
        bytes_read[0]++;

        if (octet > 0)
            m_value = true;
        else
            m_value = false;
    }

    /**
     * Sends the BER encoding directly to a stream.
     * @param stream output stream
     */
    public void write(OutputStream stream) throws IOException {
        stream.write(BERElement.BOOLEAN);
        stream.write(0x01);
        if (m_value)
            stream.write(0xFF);  /* non-zero means true. */
        else
            stream.write(0x00);  /* zero means false. */
    }

    /**
     * Gets the boolean value.
     * @param element type
     */
    public boolean getValue() {
        return m_value;
    }

    /**
     * Gets the element type.
     * @param element type
     */
    public int getType() {
        return BERElement.BOOLEAN;
    }

    /**
     * Gets the string representation.
     * @return string representation of tag
     */
    public String toString() {
        return "Boolean {" + m_value + "}";
    }
}
