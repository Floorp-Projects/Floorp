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
 * This class is for PrintableString type.
 *
 * @version 1.0
 * @seeAlso CCITT X.209
 */
public class BERPrintableString extends BERCharacterString {

    /**
     * Constructs an octetstring element containing buffer.
     * @param buffer string containing 'octets'
     */
    public BERPrintableString(String string) {
        m_value = string;
    }

    /**
     * Constructs a printablestring element from buffer.
     * @param buffer buffer
     */
    public BERPrintableString(byte[] buffer) {
        super(buffer);
    }

    /**
     * Constructs a printablestring element with the input stream.
     * (for constructed encoding)
     * @param stream input stream
     * @param bytes_read array of 1 int, incremented by number of bytes read.
     * @exception IOException failed to construct
     */
    public BERPrintableString(BERTagDecoder decoder, InputStream stream,
        int[] bytes_read) throws IOException {
        super(decoder,stream,bytes_read);
    }

    /**
     * Constructs a printablestring element with the input stream.
     * (for primitive encoding)
     * @param stream input stream
     * @param bytes_read array of 1 int, incremented by number of bytes read.
     * @exception IOException failed to construct
     */
    public BERPrintableString(InputStream stream, int[] bytes_read)
        throws IOException {
        super(stream,bytes_read);
    }


    /**
     * Gets the element type.
     * @param element type
     */
    public int getType() {
        return BERElement.PRINTABLESTRING;
    }

    /**
     * Gets the string representation. Note that currently prints out
     * values in decimal form.
     * @return string representation of tag
     */
    public String toString() {
        if (m_value == null)
            return "PrintableString (null)";
        else
            return "PrintableString {" + m_value + "}";
    }
}
