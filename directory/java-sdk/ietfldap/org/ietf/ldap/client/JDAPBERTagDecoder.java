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
package org.ietf.ldap.client;

import java.util.*;
import org.ietf.ldap.ber.stream.*;
import java.io.*;

/**
 * This class is to help BER libraries to make decision
 * on how to decode an implicit object.
 */
public class JDAPBERTagDecoder extends BERTagDecoder {
    /**
     * Gets an application specific ber element from the stream.
     * @param buffer ber encoding buffer
     * @param stream input stream
     * @param bytes_read number of bytes read
     * @param implicit to indicate a tag implicit or not
     */
    public BERElement getElement(BERTagDecoder decoder, int tag,
        InputStream stream, int[] bytes_read, boolean[] implicit)
        throws IOException {
        BERElement element = null;
        switch (tag) {
            case 0x60:  /* [APPLICATION 0] For Bind Request */
            case 0x61:  /* [APPLICATION 1] Bind Response */
            case 0x63:  /* [APPLICATION 3] Search Request
                         * If doing search without bind first,
                 * x500.arc.nasa.gov returns tag [APPLICATION 3]
                 * in Search Response. Gee.
                         */
            case 0x64:  /* [APPLICATION 4] Search Response */
            case 0x65:  /* [APPLICATION 5] Search Result */
            case 0x67:  /* [APPLICATION 7] Modify Response */
            case 0x69:  /* [APPLICATION 9] Add Response */
            case 0x6a:  /* [APPLICATION 10] Del Request */
            case 0x6b:  /* [APPLICATION 11] Del Response */
            case 0x6d:  /* [APPLICATION 13] ModifyRDN Response */
            case 0x6f:  /* [APPLICATION 15] Compare Response */
            case 0x78:  /* [APPLICATION 23] Extended Response */
            case 0x73:  /* [APPLICATION 19] SearchResultReference */
                element = new BERSequence(decoder, stream, bytes_read);
                implicit[0] = true;
            break;
            case 0x80:  /* [APPLICATION 16] 64+16 */
                element = new BERInteger(stream, bytes_read);
                implicit[0] = true;
            break;
            /* 16/02/97 MS specific */
            case 0x85:  /* Context Specific [5]:
                 * (a) Handle Microsoft v3 referral bugs! (Response)
                 * (b) Handle Microsoft v3 supportedVersion in Bind
                 *     response
                 */
                element = new BERInteger(stream, bytes_read);
                implicit[0] = true;
            break;
            case 0x87:  /* Context Specific [7]:
                 * Handle Microsoft Filter "present" in
                 * search request.
                 */
                element = new BEROctetString(decoder, stream, bytes_read);
                implicit[0] = true;
            break;
            case 0x8a:  /* Context Specific [10]:
                         * Handle extended response
                         */
                element = new BEROctetString(decoder, stream, bytes_read);
                implicit[0] = true;
            break;
            case 0x8b:  /* Context Specific [11]:
                         * Handle extended response
                         */
                element = new BEROctetString(decoder, stream, bytes_read);
                implicit[0] = true;
            break;
            case 0xa3:  /* Context Specific <Construct> [3]:
                 * Handle Microsoft v3 sasl bind request
                 */
                element = new BERSequence(decoder, stream, bytes_read);
                implicit[0] = true;
            break;
            case 0xa7:  /* Context Specific <Construct> [7]:
                 * Handle Microsoft v3 serverCred in
                 * bind response. MS encodes it as SEQUENCE OF
                 * while it should be CHOICE OF.
                 */
                element = new BERSequence(decoder, stream, bytes_read);
                implicit[0] = true;
            break;
            case 0xa0:  /* Context Specific <Construct> [0]:
                 * v3 Server Control.
                 * SEQUENCE of SEQUENCE of {OID  [critical] [value]}
                 */
                element = new BERSequence(decoder, stream, bytes_read);
                implicit[0] = true;
            break;
            default:
                throw new IOException();
        }
        return element;
    }
}
