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
 * This class is for the Choice object. Note that this class will be
 * used by the client.
 *
 * <pre>
 * ENCODING RULE:
 *   Encoding is the encoding of the chosen type.
 * </pre>
 *
 * @version 1.0
 * @seeAlso CCITT X.209
 */
public class BERChoice extends BERElement {
    /**
     * Internal variables
     */
    private BERElement m_value = null;

    /**
     * Constructs a choice element.
     * @param value BERElement value
     */
    public BERChoice(BERElement value) {
        m_value = value;
    }

    /**
     * Constructs a choice element with the input stream.
     * Note that with the current decoding architecture choice types
     * will not be decoded as choices but rather only as the types
     * chosen.  The following method will never be called.
     * @param stream input stream
     * @param bytes_read array of 1 int; value incremented by
     *        number of bytes read from stream.
     * @exception IOException failed to construct
     */
    public BERChoice(BERTagDecoder decoder, InputStream stream,
                     int[] bytes_read) throws IOException {
        m_value = getElement(decoder, stream, bytes_read);
    }

    /**
     * Sends the BER encoding of the chosen type directly to stream.
     * @param stream output stream
     * @exception IOException failed to construct
     */
    public void write(OutputStream stream) throws IOException {
        m_value.write(stream);
    }

    /**
     * Gets the value of the chosen type.
     * @param element type
     */
    public BERElement getValue() {
        return m_value;
    }

    /**
     * Gets the element type.
     * @param element type
     */
    public int getType() {
        return BERElement.CHOICE;
    }

    /**
     * Gets the string representation.
     * @return string representation of tag
     */
    public String toString() {
        return "CHOICE {" + m_value + "}";
    }
}
