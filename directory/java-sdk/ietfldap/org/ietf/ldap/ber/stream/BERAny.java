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
 * This class is for the "any" object that is unknown to the
 * BER package but conforms to BER rules.
 * <P>See CCITT X.209.
 *
 *
 * <pre>
 * Encoding Rule:
 *   The encoding is that of the particular implementation.
 * </pre>
 *
 * @version 1.0
 */
public class BERAny extends BERElement {
    /**
     * Internal variables
     */
    private BERElement m_value = null;

    /**
     * Constructs an "any" element.
     * @param value BERElement value
     */
    public BERAny(BERElement value) {
        m_value = value;
    }

    /**
     * Constructs an "any" element from an input stream.
     * Note that with the current decoding architecture "any" types
     * will not be decoded as any's but rather as the particular
     * implementation.  The following method will never be called.
     * @param stream input stream
     * @param bytes_read array of 1 int; value incremented by
     * number of bytes read from stream
     * @exception IOException failed to construct
     */
    public BERAny(BERTagDecoder decoder, InputStream stream,
                      int[] bytes_read) throws IOException {
        m_value = getElement(decoder, stream, bytes_read);
    }

    /**
     * Sends the BER encoding directly to an output stream.
     * @param stream output stream
     */
    public void write(OutputStream stream) throws IOException {
        m_value.write(stream);
    }

    /**
     * Gets the element type.
     * @param element type
     */
    public int getType() {
        return BERElement.ANY;
    }

    /**
     * Gets the string representation.
     * @return string representation of tag.
     */
    public String toString() {
        return "ANY {" + m_value + "}";
    }
}
