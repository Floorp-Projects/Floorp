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
package netscape.ldap;

/**
 * Represents a SSL socket connection that you can use to connect to an
 * LDAP server. This interface extends the base interface LDAPSocketFactory 
 * and it provides the methods for SSL specific matters.
 * <P>
 *
 * @version 1.0
 * @see LDAPSocketFactory
 * @see LDAPConnection#LDAPConnection(netscape.ldap.LDAPSocketFactory)
 */
public interface LDAPSSLSocketFactoryExt extends LDAPSocketFactory {

    /**
     * Returns <code>true</code> if client authentication is enabled.
     * @see netscape.ldap.LDAPSSLSocketFactory#enableClientAuth
     */
    public boolean isClientAuth();

    /**
     * Returns the suite of ciphers used for SSL connections made through
     * sockets created by this factory.
     *
     * @return The suite of ciphers used.
     */
    public Object getCipherSuites();
}

