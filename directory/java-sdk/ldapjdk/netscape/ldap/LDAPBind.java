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

/**
 * Used to do explicit bind processing on a referral. A client may
 * specify an instance of this class to be used on a single operation
 * (through the <CODE>LDAPConstraints</CODE> object) or for all operations 
 * (through <CODE>LDAPConnection.setOption()</CODE>). It is typically used
 * to control the authentication mechanism used on implicit referral 
 * handling.
 */

public interface LDAPBind {

    /**
     * This method is called by <CODE>LDAPConnection</CODE> when 
     * authenticating. An implementation may access the host, port, 
     * credentials, and other information in the <CODE>LDAPConnection</CODE>
     * to decide on an appropriate authentication mechanism, and/or may 
     * interact with a user or external module. 
     * @exception netscape.ldap.LDAPException
     * @see netscape.ldap.LDAPConnection#bind
     * @param conn An established connection to an LDAP server.
     */

    public void bind(LDAPConnection conn) throws LDAPException;
}
