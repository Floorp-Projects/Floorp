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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.io.Serializable;

import org.ietf.ldap.client.opers.JDAPExtendedResponse;

/**
 * Represents a server response to an extended operation request.
 * 
 * @version 1.0
 */
public class LDAPExtendedResponse extends LDAPResponse
                                  implements Serializable {

    static final long serialVersionUID = -3813049515964705320L;

    /**
     * Constructor
     * 
     * @param msgid message identifier
     * @param rsp extended operation response
     * @paarm controls array of controls or null
     */
    LDAPExtendedResponse( int msgid,
                          JDAPExtendedResponse rsp,
                          LDAPControl controls[] ) {
        super(msgid, rsp, controls);
    }
    
    /**
     * Returns the OID of the response
     *
     * @return the response OID
     */
    public String getID() {
        JDAPExtendedResponse result = (JDAPExtendedResponse)getProtocolOp();
        return result.getID();
    }

    /**
     * Returns the raw bytes of the value part of the response
     *
     * @return response as a raw array of bytes
     */
    public byte[] getValue() {
        JDAPExtendedResponse result = (JDAPExtendedResponse)getProtocolOp();
        return result.getValue();
    }
}
