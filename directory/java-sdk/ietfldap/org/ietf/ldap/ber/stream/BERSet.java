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
 * This class is for the Set object. A set object can contain
 * a set of BER elements.
 * <P>See CCITT X.209.
 *
 * <pre>
 * ENCODING RULE:
 *  tag    = 0x31 (always constructed)
 * </pre>
 *
 * @version 1.0
 */
public class BERSet extends BERConstruct {
    /**
     * Constructs a set element.
     * @exception failed to construct
     */
    public BERSet() throws IOException {
    }

    /**
     * Constructs a set element from an input stream.
     * @param decoder decoder for application-specific BER
     * @param stream source
     * @param bytes_read array of 1 int; value incremented by number
     * of bytes read from stream
     * @exception IOException failed to construct
     */
    public BERSet(BERTagDecoder decoder, InputStream stream,
        int[] bytes_read) throws IOException {
        super(decoder, stream,bytes_read);
    }

    /**
     * Sends the BER encoding directly to a stream.
     * @param stream output stream
     * @exception IOException failed to write
     */
    public void write(OutputStream stream) throws IOException {
        super.write(stream);
    }

    /**
     * Gets the element type.
     * @return element type.
     */
    public int getType() {
        return BERElement.SET;
    }

    /**
     * Gets the string representation.
     * @return string representation of tag.
     */
    public String toString() {
        String elements = "";
        for (int i = 0; i < super.size(); i++) {
            if (i != 0)
                elements = elements + ", ";
            elements = elements + ((BERElement)super.elementAt(i)).toString();
        }
        return "Set {" + elements + "}";
    }
}
