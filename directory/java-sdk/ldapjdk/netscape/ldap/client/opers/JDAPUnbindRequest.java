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
package netscape.ldap.client.opers;

import java.util.*;
import netscape.ldap.client.*;
import netscape.ldap.ber.stream.*;
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
