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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package netscape.ldap.factory;

import java.net.*; 
import java.io.*; 
import javax.net.ssl.*; 
import netscape.ldap.*;

/**
 * Creates an SSL socket connection to a server, using the JSSE package
 * from Sun. This class implements the <CODE>LDAPSocketFactory</CODE>
 * interface.
 * <P>
 *
 * @version 1.0
 * @see LDAPSocketFactory
 * @see LDAPConnection#LDAPConnection(netscape.ldap.LDAPSocketFactory)
 */
public class JSSESocketFactory implements LDAPTLSSocketFactory,
                                          java.io.Serializable {

    static final long serialVersionUID = 6834205777733266610L;

    protected SSLSocketFactory factory = null;

    // Optional explicit cipher suites to use
    protected String[] suites = null;

    /**
     * Default factory constructor
     */
    public JSSESocketFactory() {
        this.factory = (SSLSocketFactory) SSLSocketFactory.getDefault();
    }
  
    /**
     * Factory constructor
     *
     * @param suites Cipher suites to attempt to use with the server;
     * if <code>null</code>, use any cipher suites available in the
     * JSSE package
     */
    public JSSESocketFactory( String[] suites ) {
        this.suites = suites;
        this.factory = (SSLSocketFactory)SSLSocketFactory.getDefault();
    }
  
    /**
     * Factory constructor
     * @param sf the SSL socketfactory to use
     */
    public JSSESocketFactory( SSLSocketFactory factory) {
        this.factory = factory;
    }

    /**
     * Factory constructor
     * @param suites Cipher suites to attempt to use with the server;
     * if <code>null</code>, use any cipher suites available in the
     * JSSE package
     * @param sf the SSL socketfactory to use
     */
    public JSSESocketFactory( String[] suites, SSLSocketFactory factory) {
        this.suites = suites;
        this.factory = factory;
    }

    /**
     * Creates an SSL socket.
     *
     * @param host Host name or IP address of SSL server
     * @param port Port numbers of SSL server
     * @return A socket for an encrypted session
     * @exception LDAPException on error creating socket
     */
    public Socket makeSocket(String host, int port)
        throws LDAPException { 

        SSLSocket sock = null;

        try {
            sock = (SSLSocket)factory.createSocket(host, port);

            if (suites != null) {
                sock.setEnabledCipherSuites(suites);
            }
            
            // Start handshake manually to immediately expose potential
            // SSL errors as exceptions. Otherwise, handshake will take
            // place first time the data are written to the socket.
            sock.startHandshake();

        } catch (UnknownHostException e) {
            throw new LDAPException("JSSESocketFactory.makeSocket - Unknown host: " + host,
                                    LDAPException.CONNECT_ERROR);
        } catch (IOException f) {
            throw new LDAPException("JSSESocketFactory.makeSocket " +
                                    host + ":" + port + ", " + f.getMessage(),
                                    LDAPException.CONNECT_ERROR);
        }

        return sock;
    }

    /**
     * Creates an SSL socket layered over an existing socket.
     * 
     * Used for the startTLS implementation (RFC2830).
     *
     * @param s An existing non-SSL socket
     * @return A SSL socket layered over the input socket
     * @exception LDAPException on error creating socket
     * @since LDAPJDK 4.17
     */
    public Socket makeSocket(Socket s)
        throws LDAPException { 
  
        SSLSocket sock = null;
        String host = s.getInetAddress().getHostName();
        int port = s.getPort();

        try {
            sock = (SSLSocket)factory.createSocket(s, host, port, /*autoClose=*/ true);

            if (suites != null) {
                sock.setEnabledCipherSuites(suites);
            }
            
            sock.startHandshake();

        } catch (IOException f) {
            throw new LDAPException("JSSESocketFactory - start TLS, " + f.getMessage(),
                                    LDAPException.TLS_NOT_SUPPORTED);
        }

        return sock;
    }
}

