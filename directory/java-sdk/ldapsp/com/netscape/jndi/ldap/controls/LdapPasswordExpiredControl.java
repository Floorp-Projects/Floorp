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
package com.netscape.jndi.ldap.controls;

import javax.naming.ldap.Control;
import netscape.ldap.controls.*;

/**
 * Represents an LDAP v3 server control that may be returned if a
 * password has expired, and password policy is enabled on the server.
 * The OID for this control is 2.16.840.1.113730.3.4.4.
 * <P>
 */

public class LdapPasswordExpiredControl extends LDAPPasswordExpiredControl implements Control{

    /**
     * This constractor is used by the NetscapeControlFactory
     */
    LdapPasswordExpiredControl(boolean critical, byte[] value) throws Exception{
        super(EXPIRED, critical, value);
    }
    
    /**
     * Return string message passed in the control
     * @return message string
     */
    public String getMessage() {
        return super.getMessage();
    }    

    /**
     * Retrieves the ASN.1 BER encoded value of the LDAP control.
     * Null is returned if the value is absent.
     * @return A possibly null byte array representing the ASN.1 BER
     * encoded value of the LDAP control.
     */
    public byte[] getEncodedValue() {
        return getValue();
    }    
}
