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
package netscape.ldap;

import netscape.ldap.client.opers.JDAPExtendedResponse;

/**
 * Represents a server response to an extended operation request.
 * 
 * @version 1.0
 */
public class LDAPExtendedResponse extends LDAPResponse {

    /**
     * Constructor
     * 
     * @param msgid message identifier
     * @param rsp  Extended operation response
     * @paarm controls Array of controls of null
     */
    LDAPExtendedResponse(int msgid, JDAPExtendedResponse rsp, LDAPControl controls[]) {
        super(msgid, rsp, controls);
    }
    
    /**
     * Returns the OID of the response.
     *
     * @return The response OID
     */
    public String  getOID() {
        JDAPExtendedResponse result = (JDAPExtendedResponse)getProtocolOp();
        return result.getID();
    }

    /**
     * Returns the raw bytes of the value part of the response.
     *
     * @return Response as a raw array of bytes
     */
    public byte[] getValue() {
        JDAPExtendedResponse result = (JDAPExtendedResponse)getProtocolOp();
        return result.getValue();
    }
}
