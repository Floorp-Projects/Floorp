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
 * This class is for the Integer object.
 * <P>See CCITT X.209.
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
     * number of bytes read from stream
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
     * @return string representation of tag.
     */
    public String toString() {
        return "Integer {" + getValue() + "}";
    }
}
