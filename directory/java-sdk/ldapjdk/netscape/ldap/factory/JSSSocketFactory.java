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

import netscape.ldap.*;

import org.mozilla.jss.ssl.SSLSocket;
import org.mozilla.jss.ssl.SSLCertificateApprovalCallback.ValidityStatus;
import org.mozilla.jss.ssl.SSLCertificateApprovalCallback;
import org.mozilla.jss.crypto.X509Certificate;
import org.mozilla.jss.crypto.AlreadyInitializedException;
import org.mozilla.jss.CryptoManager;

/**
 * Creates an SSL socket connection to a server, using the Netscape/Mozilla
 * JSS package.
 * This class implements the <CODE>LDAPSocketFactory</CODE>
 * interface.
 * <P>
 * By default, the factory uses "secmod.db", "key*.db" and "cert*.db"
 * databases in the current directory. If you need to override this default
 * setting, then you should use the constructor <CODE>JSSSocketFactory(certdbDir)</CODE>.
 * 
 * @version 1.1
 * @see LDAPSocketFactory
 * @see LDAPConnection#LDAPConnection(netscape.ldap.LDAPSocketFactory)
 */

public class JSSSocketFactory implements Serializable,
                                         LDAPTLSSocketFactory,
                                         SSLCertificateApprovalCallback
{

    static final long serialVersionUID = -6926469178017736903L;

    /**
     * Constructs a new <CODE>JSSSocketFactory</CODE>, initializing the
     * JSS security system if it has not already been initialized.
     * <p>
     * The current directory is assumed to be the certificate database directory.
     * 
     * @exception LDAPException on initialization error
     * @see netscape.ldap.factory.JSSSocketFactory#JSSSocketFactory(java.lang.String)
     */
    public JSSSocketFactory() throws LDAPException{
        initialize(".");
    }

    /**
     * Constructs a new <CODE>JSSSocketFactory</CODE>, initializing the
     * JSS security system if it has not already been initialized.
     *
     * @param certdbDir The full path, relative or absolute, of the certificate
     * database directory
     * @exception LDAPException on initialization error
     */
    public JSSSocketFactory( String certdbDir ) throws LDAPException{
        initialize( certdbDir );
    }

    /**
     * Initialize the JSS security subsystem.
     * <P>
     * This method allows you to override the current directory as the
     * default certificate database directory. The directory is expected
     * to contain <CODE>secmod.db</CODE>, <CODE>key*.db</CODE> and
     * <CODE>cert*.db</CODE> files as the security module database, key database
     * and certificate database respectively.
     * <P>
     * The method may be called only once, before the first instance of 
     * <CODE>JSSSocketFactory</CODE> is created. When creating the first
     * instance, the constructor will automatically initialize the JSS 
     * security subsystem using the defaults, unless it is already initialized.
     * <P>
     * @param certdbDir The full path, relative or absolute, of the certificate
     * database directory.
     * @exception LDAPException on initialization error
     * @see netscape.ldap.factory.JSSSocketFactory#JSSSocketFactory(String)
     */
    public static void initialize( String certdbDir ) throws LDAPException {
        try {
            CryptoManager.initialize( certdbDir );
        } catch (AlreadyInitializedException e) {
            // This is ok
        } catch (Exception e) {
            throw new LDAPException("Failed to initialize JSSSocketFactory: "
                                    + e.getMessage(), LDAPException.OTHER);
        }
    }

    /**
     * Creates an SSL socket
     *
     * @param host Host name or IP address of SSL server
     * @param port Port numbers of SSL server
     * @return A socket for an encrypted session
     * @exception LDAPException on error creating socket
     */
    public Socket makeSocket( String host, int port ) throws LDAPException {
        SSLSocket socket = null;
        try {

            socket = new SSLSocket( host, // address
                                    port, // port
                                    null, // localAddress
                                    0,    // localPort
                                    this, // certApprovalCallback
                                    null  // clientCertSelectionCallback
            );

            socket.forceHandshake();

        }
        catch (UnknownHostException e) {
            throw new LDAPException("JSSSocketFactory.makeSocket - Unknown host: " + host,
                                    LDAPException.CONNECT_ERROR);
                    
        }
        catch (Exception e) {
            throw new LDAPException("JSSSocketFactory.makeSocket " +
                                    host + ":" + port + ", " + e.getMessage(),
                                    LDAPException.CONNECT_ERROR);
        }

        return socket;
    }

    /**
     * The default implementation of the SSLCertificateApprovalCallback
     * interface.
     * <P>
     * This default implementation always returns true. If you need to
     * verify the server certificate validity, then you should override
     * this method.
     * <P>
     * @param serverCert X509 Certificate
     * @param status The validity of the server certificate
     * @return <CODE>true</CODE>, by default we trust the certificate
     */
    public boolean approve(X509Certificate serverCert,
                           ValidityStatus status) {
        
        return true;
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
    public Socket makeSocket(Socket s) throws LDAPException {
        SSLSocket socket = null;
        String host = s.getInetAddress().getHostName();
        int port = s.getPort();
        try {
            socket = new SSLSocket( s,
                                    host,
                                    this, // certApprovalCallback
                                    null  // clientCertSelectionCallback
            );

            socket.forceHandshake();

        } catch (Exception e) {
            throw new LDAPException("JSSSocketFactory - start TLS, " + e.getMessage(),
                                    LDAPException.TLS_NOT_SUPPORTED);
        }

        return socket;        
    }
}
