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

import java.util.*;
import java.io.*;
import java.net.*;

/**
 * Represents a socket connection that you can use to connect to an
 * LDAP server.  You can write a class that implements this interface
 * if you want to use a TLS socket to connect to a secure server.
 * (The <CODE>LDAPSSLSocketFactory class</CODE>, which is included
 * in the <CODE>org.ietf.ldap</CODE> package, implements this
 * interface for SSL connections.)
 * <P>
 *
 * When you construct a new <CODE>LDAPConnection</CODE>
 * object, you can specify that the connection use this socket. 
 * To do this, pass the constructor an object of the class that 
 * implements this interface.
 * <P>
 *
 * @version 1.0
 * @see LDAPConnection#LDAPConnection(org.ietf.ldap.LDAPSocketFactory)
 * @see LDAPSSLSocketFactory
 */
public interface LDAPSocketFactory {
    /**
     * Returns a socket to the specified host name and port number.
     * <P>
     *
     * @param host name of the host to which you want to connect
     * @param port port number to which you want to connect
     * @exception LDAPException Failed to create the socket.
     * @see LDAPSSLSocketFactory#createSocket(java.lang.String,int)
     */
    public Socket createSocket( String host, int port )
        throws LDAPException;
}
