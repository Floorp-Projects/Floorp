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

import netscape.ldap.client.opers.JDAPProtocolOp;
import netscape.ldap.client.opers.JDAPResult;

/**
 * Represents the response to a particular LDAP operation.
 * 
 * @version 1.0
 */
public class LDAPResponse extends LDAPMessage {

    /**
     * Constructor
     * 
     * @param msgid message identifier
     * @param rsp Operation response
     * @param controls Array of controls of null
     */
    LDAPResponse(int msgid, JDAPProtocolOp rsp, LDAPControl controls[]) {
        super(msgid, rsp, controls);
    }
    
    /**
     * Returns any error message in the response.
     *
     * @return The error message of the last error (or <CODE>null</CODE>
     * if no message was set).
     */
    public String  getErrorMessage() {
        JDAPResult result = (JDAPResult) getProtocolOp();
        return result.getErrorMessage();
    }

    /**
     * Returns the partially matched DN field, if any, in a server response.
     *
     * @return The maximal subset of a DN which could be matched,
     * or <CODE>null</CODE>.
     */
    public String getMatchedDN() {
	JDAPResult result = (JDAPResult) getProtocolOp();
        return result.getMatchedDN();
    }
    
    /**
     * Returns all referrals, if any, in a server response.
     *
     * @return A list of referrals or <CODE>null</CODE>.
     */
    public String[] getReferrals() {
        JDAPResult result = (JDAPResult) getProtocolOp();
        return result.getReferrals();
    }    

    /**
     * Returns the result code in a server response.
     *
     * @return The result code.
     */
    public int getResultCode() {
        JDAPResult result = (JDAPResult) getProtocolOp();
        return result.getResultCode();
    }
}
