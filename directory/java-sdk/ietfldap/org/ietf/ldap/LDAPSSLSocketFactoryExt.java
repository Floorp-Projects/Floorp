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
 * Represents an SSL socket connection that you can use to connect to an
 * LDAP server. This interface extends the base interface LDAPSocketFactory 
 * and provides SSL-specific methods.
 * <P>
 *
 * @version 1.0
 * @see LDAPSocketFactory
 * @see LDAPConnection#LDAPConnection(org.ietf.ldap.LDAPSocketFactory)
 */
public interface LDAPSSLSocketFactoryExt extends LDAPSocketFactory {

    /**
     * Returns the suite of ciphers used for SSL connections. These connections 
     * are made through sockets created by the LDAPSSLSocketFactory.
     *
     * @return the suite of ciphers used.
     */
    public Object getCipherSuites();
    
    /**
     * Returns <code>true</code> if client authentication is enabled.
     * @see org.ietf.ldap.LDAPSSLSocketFactory#enableClientAuth
     */
    public boolean isClientAuth();
}

