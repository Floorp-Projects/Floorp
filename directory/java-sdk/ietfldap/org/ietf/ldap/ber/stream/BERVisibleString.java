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
 * This class is for the VisibleString object.
 * <P>See CCITT X.209.
 *
 * <pre>
 * ENCODING RULE:
 *   Primitive Definite length.
 *   tag = 0x1A
 *   length = (short or long form)
 *   one or more contents octets
 * </pre>
 *
 * @version 1.0
 */
public class BERVisibleString extends BERCharacterString {

    /**
     * Constructs a visiblestring element.
     * @param string string
     */
    public BERVisibleString(String string) {
        m_value = string;
    }

    /**
     * Constructs a visiblestring element from buffer.
     * @param buffer buffer
     */
    public BERVisibleString(byte[] buffer) {
        super(buffer);
    }

    /**
     * Constructs a visiblestring element with the input stream.
     * (for constructed encoding)
     * @param stream input stream
     * @param bytes_read array of 1 int, incremented by number of bytes read
     * @exception IOException failed to construct
     */
    public BERVisibleString(BERTagDecoder decoder, InputStream stream,
                          int[] bytes_read) throws IOException {
        super(decoder,stream,bytes_read);
    }

    /**
     * Constructs a visiblestring element with the input stream.
     * (for primitive encoding)
     * @param stream input stream
     * @param bytes_read array of 1 int, incremented by number of bytes read
     * @exception IOException failed to construct
     */
    public BERVisibleString(InputStream stream, int[] bytes_read)
        throws IOException {
        super(stream,bytes_read);
    }

    /**
     * Gets the element type.
     * @param element type
     */
    public int getType() {
        return BERElement.VISIBLESTRING;
    }

    /**
     * Gets the string representation. Note that currently prints out
     * values in decimal form.
     * @return string representation of tag.
     */
    public String toString() {
        if (m_value == null)
            return "VisibleString (null)";

        return "VisibleString {" + m_value + "}";
    }
}
