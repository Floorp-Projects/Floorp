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
 * This class implements the unbind request and is a protocol operation.
 * A protocol operation is embedded within JDAPMessage which is sent
 * between client and server. This object is sent to the ldap server.
 * <pre>
 * UnbindRequest ::= [APPLICATION 2] NULL
 * </pre>
 *
 * @version 1.0
 */
public class JDAPUnbindRequest implements JDAPProtocolOp {
    /**
     * Constructs bind request.
     */
    public JDAPUnbindRequest() {
    }

    /**
     * Retrieves the protocol operation type.
     * @return operation type
     */
    public int getType() {
        return JDAPProtocolOp.UNBIND_REQUEST;
    }

    /**
     * Gets the ber representation of the unbind rquest.
     * @return ber representation
     */
    public BERElement getBERElement() {
        /*
         * [*] umich-ldap-v3.3:
         *     0x42 0x00
         * [*] umich-ldap-v3.0:
         */
        BERNull n = new BERNull();
        BERTag element = new BERTag(BERTag.APPLICATION|2, n, true);
        return element;
    }

    /**
     * Retrieves the string representation of unbind operation.
     * @return string representation
     */
    public String toString() {
        return "UnbindRequest {}";
    }
}
