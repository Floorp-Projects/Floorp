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
 * This class implements the modify response. This object
 * is sent from the ldap server to the interface.
 * <pre>
 * ModifyResponse ::= [APPLICATION 7] LDAPResult
 * </pre>
 *
 * @version 1.0
 * @see RFC1777
 */
public class JDAPModifyResponse extends JDAPResult implements JDAPProtocolOp {
    /**
     * Constructs modify response.
     * @param element ber element of bind response
     */
    public JDAPModifyResponse(BERElement element) throws IOException {
        super(((BERTag)element).getValue());
    }

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.MODIFY_RESPONSE;
    }

    /**
     * Retrieve the string representation.
     * @return string representation
     */
    public String toString() {
        return "ModifyResponse " + super.getParamString();
    }
}
