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

import org.ietf.ldap.client.opers.JDAPProtocolOp;
import org.ietf.ldap.client.opers.JDAPResult;

/**
 * Represents the response to a particular LDAP operation.
 * 
 * @version 1.0
 */
public class LDAPResponse extends LDAPMessage {

    static final long serialVersionUID = 5822205242593427418L;

    /**
     * Constructor
     * 
     * @param msgid message identifier
     * @param rsp operation response
     * @param controls array of controls or null
     */
    LDAPResponse(int msgid, JDAPProtocolOp rsp, LDAPControl controls[]) {
        super(msgid, rsp, controls);
    }
    
    /**
     * Returns any error message in the response.
     *
     * @return the error message of the last error (or <CODE>null</CODE>
     * if no message was set).
     */
    public String getErrorMessage() {
        JDAPResult result = (JDAPResult) getProtocolOp();
        return result.getErrorMessage();
    }

    /**
     * Returns the partially matched DN field, if any, in a server response.
     *
     * @return the maximal subset of a DN to match,
     * or <CODE>null</CODE>.
     */
    public String getMatchedDN() {
	JDAPResult result = (JDAPResult) getProtocolOp();
        return result.getMatchedDN();
    }
    
    /**
     * Returns all referrals, if any, in a server response.
     *
     * @return a list of referrals or <CODE>null</CODE>.
     */
    public String[] getReferrals() {
        JDAPResult result = (JDAPResult) getProtocolOp();
        return result.getReferrals();
    }    

    /**
     * Returns the result code in a server response.
     *
     * @return the result code.
     */
    public int getResultCode() {
        JDAPResult result = (JDAPResult) getProtocolOp();
        return result.getResultCode();
    }
}
