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
package org.ietf.ldap.client.opers;

import java.util.*;
import org.ietf.ldap.client.*;
import org.ietf.ldap.ber.stream.*;
import java.io.*;
import java.net.*;

/**
 * This class implements the abandon request. This object
 * is sent to the ldap server.
 * <P>See RFC 1777.
 *
 * <pre>
 * AbandonRequest ::= [APPLICATION 16] MessageID
 * </pre>
 *
 * @version 1.0
 */
public class JDAPAbandonRequest implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected int m_msgid;

    /**
     * Constructs abandon request.
     * @param msgid message identifier
     */
    public JDAPAbandonRequest(int msgid) {
        m_msgid = msgid;
    }

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.ABANDON_REQUEST;
    }

    /**
     * Gets the ber representation of abandon request.
     * @return ber representation of request
     */
    public BERElement getBERElement() {
        /* Assumed m_msgid = 1. The BER encoding output
         * should be
         *
         * [*] umich-ldap-v3.3:
         *     0x50 (implicit tagged integer)
         *     0x01 (length)
         *     0x01 (message id)
         */
        BERInteger i = new BERInteger(m_msgid);
        BERTag element = new BERTag(BERTag.APPLICATION|16, i, true);
        return element;
    }

    /**
     * Retrieves the string representation of abandon request.
     * @return string representation
     */
    public String toString() {
        return "AbandonRequest {msgid=" + m_msgid + "}";
    }
}
