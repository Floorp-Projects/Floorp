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
 * This is an abstract class which should be extended
 * for use by the BERTag class in decoding application
 * specific BER tags. Since different applications may
 * define their own tag, the BER package needs a tag decoder
 * to give hints on how to decode implicitly tagged
 * objects. Each application should extend this
 * decoder.
 *
 * @version 1.0
 * @seeAlso CCITT X.209
 */
public abstract class BERTagDecoder {

    /**
     * Gets an application specific ber element from an input stream.
     * @param decoder an instance of a BERTagDecoder
     * @param tag element tag
     * @param stream source stream
     * @param bytes_read array of 1 int; value incremented by number
     *        of bytes read from stream.
     * @param implicit array of implicit flags
     */
    public abstract BERElement getElement(BERTagDecoder decoder, int tag,
        InputStream stream, int[] bytes_read, boolean[] implicit)
        throws IOException;
}
