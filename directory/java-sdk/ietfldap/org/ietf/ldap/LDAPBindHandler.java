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
package org.ietf.ldap;

/**
 * Performs explicit bind processing on a referral. A client may
 * specify an instance of this class for use on a single operation
 * (through the <CODE>LDAPConstraints</CODE> object) or all operations 
 * (through <CODE>LDAPConnection.setOption()</CODE>). It is typically used
 * to control the authentication mechanism used on implicit referral 
 * handling.
 */

public interface LDAPBindHandler {

    /**
     * This method is called by <CODE>LDAPConnection</CODE> when 
     * authenticating. An implementation of <CODE>LDAPBindHandler</CODE> may access 
     * the host, port, credentials, and other information in the 
     * <CODE>LDAPConnection</CODE> in order to decide on an appropriate 
     * authentication mechanism.<BR> 
     * The bind method can also interact with a user or external module. 
     * @exception org.ietf.ldap.LDAPReferralException
     * @see org.ietf.ldap.LDAPConnection#bind
     * @param ldapurls urls which may be selected to connect and bind to
     * @param conn an established connection to an LDAP server
     */

    public void bind( String[] ldapurls,
                      LDAPConnection conn ) throws LDAPReferralException;
}
