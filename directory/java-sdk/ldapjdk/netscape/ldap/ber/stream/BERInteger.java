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
 * This class is for the Integer object.
 *
 * <pre>
 * ENCODING RULE:
 *   Primitive Definite length.
 *   tag = 0x02
 *   length = (short or long form)
 *   one or more contents octets hold integer
 *   value in two's complement
 *
 * Example 1:  (zero)
 *   02 01 00
 * Example 2:  (1)
 *   02 01 01
 * Example 3:  (300 - short form)
 *   02 02 01 2C
 * Example 4:  (300 - long form)
 *   02 84 00 00 01 2C
 * </pre>
 *
 * @version 1.0
 * @seeAlso CCITT X.209
 */
public class BERInteger extends BERIntegral {

    /**
     * Constructs a integer element.
     * @param value integer value
     */
    public BERInteger(int value) {
        super(value);
    }

    /**
     * Constructs an integer element with the input stream.
     * @param stream input stream
     * @param bytes_read array of 1 int; value incremented by
     *        number of bytes read from stream
     * @exception IOException failed to construct
     */
    public BERInteger(InputStream stream, int[] bytes_read) throws IOException {
        super(stream, bytes_read);
    }

    /**
     * Gets the element type.
     * @param element type
     */
    public int getType() {
        return BERElement.INTEGER;
    }

    /**
     * Gets the string representation.
     * @return string representation of tag
     */
    public String toString() {
        return "Integer {" + getValue() + "}";
    }
}
