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
import org.ietf.ldap.client.*;
import org.ietf.ldap.client.opers.*;
import org.ietf.ldap.ber.stream.*;
import org.ietf.ldap.util.*;
import java.io.*;
import java.net.*;
import javax.security.auth.callback.CallbackHandler;
//import javax.security.sasl.*;
import com.sun.security.sasl.preview.*;

/**
 * Authenticates to a server using SASL
 */
public class LDAPSaslBind implements LDAPBindHandler, Serializable {

    static final long serialVersionUID = -7615315715163655443L;

    /**
     * Construct an object which can authenticate to an LDAP server
     * using the specified name and a specified SASL mechanism.
     *
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param mechanisms array of mechanism names, e.g. { "GSSAPI", "SKEY" }
     * @param props optional additional properties of the desired
     * authentication mechanism, e.g. minimum security level
     * @param cbh a class which may be called by the SASL framework to
     * obtain additional required information
     */
    public LDAPSaslBind( String dn,
                         String[] mechanisms,
                         Map props,
                         CallbackHandler cbh ) {
        _dn = dn;
        _mechanisms = mechanisms;
        _props = props;
        _cbh = cbh;
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the parameters that were provided to the
     * constructor. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param ldc an active connection to a server, which will have
     * the new authentication state on return from the method
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind( LDAPConnection ldc ) throws LDAPException {
        if ( _props == null ) {
            _props = new HashMap();
        }
        String packageNames = (String)_props.get( CLIENTPKGS );
        if ( packageNames == null ) {
            packageNames = System.getProperty( CLIENTPKGS );
        }
        if ( packageNames == null ) {
            packageNames = ldc.DEFAULT_SASL_PACKAGE;
        }
        StringTokenizer st = new StringTokenizer( packageNames, "|" );
        while( st.hasMoreTokens() ) {
            String packageName = st.nextToken();
            try {
                _saslClient = getClient( ldc, packageName );
                if ( _saslClient != null ) {
                    break;
                }
            } catch ( LDAPException e ) {
                if ( !st.hasMoreTokens() ) {
                    throw e;
                }
            }
        }
        if ( _saslClient != null ) {
            bind( ldc, true );
            return;
        } else {
            ldc.printDebug( "LDAPSaslBind.bind: getClient " +
                            "returned null" );
        }
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the parameters that were provided to the
     * constructor. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param ldapurls urls which may be selected to connect and bind to
     * @param ldc an active connection to a server, which will have
     * the new authentication state on return from the method
     * @exception LDAPReferralException Failed to authenticate to the LDAP server
     */
    public void bind( String[] ldapurls,
                      LDAPConnection conn ) throws LDAPReferralException {
        try {
            // ??? What to do with the URLs?
            bind( conn );
        } catch ( LDAPReferralException ex ) {
            throw ex;
        } catch ( LDAPException ex ) {
            // ???
            throw new LDAPReferralException( ex.getMessage(),
                                             ex.getResultCode(),
                                             ex.getLDAPErrorMessage() );
        }
    }

    /**
     * Get a SaslClient object from the Sasl framework
     *
     * @param ldc contains the host name
     * @param packageName package containing a ClientFactory
     * @return a SaslClient supporting one of the mechanisms
     * of the member variable _mechanisms.
     * @exception LDAPException on error producing a client
     */
    private Object getClient( LDAPConnection ldc, String packageName )
        throws LDAPException {
        try {
            Object[] args = new Object[6];
            args[0] = _mechanisms;
            args[1] = _dn;
            args[2] = "ldap";
            args[3] = ldc.getHost();
            args[4] = _props;
            args[5] = _cbh;
            String[] argNames = new String[6];
            argNames[0] = "[Ljava.lang.String;";
            argNames[1] = "java.lang.String";
            argNames[2] = "java.lang.String";
            argNames[3] = "java.lang.String";
            argNames[4] = "java.util.Map";
//            argNames[4] = "java.util.Hashtable";
            argNames[5] = CALLBACK_HANDLER;

            // Get a mechanism driver
            return DynamicInvoker.invokeMethod( null,
                                                "javax.security.sasl.Sasl",
                                                "createSaslClient",
                                                args, argNames );

        } catch ( Exception e ) {
            ldc.printDebug( "LDAPSaslBind.getClient: " +
                            packageName+".Sasl.createSaslClient: " +
                            e );
            throw new LDAPException( e.toString(), LDAPException.OTHER );
        }
    }

    void bind( LDAPConnection ldc, boolean rebind )
        throws LDAPException {

        if ( (ldc.isConnected() && rebind) ||
             !ldc.isConnected() ) {
            try {
                String className = _saslClient.getClass().getName();
                ldc.printDebug( "LDAPSaslBind.bind: calling " +
                                className+".hasInitialResponse" );
                // Get initial response if any
                byte[] outVals = null;
                if ( hasInitialResponse() ) {
                    outVals = evaluateChallenge( new byte[0] );
                }

                String mechanismName = getMechanismName();
                ldc.printDebug( "LDAPSaslBind.bind: mechanism " +
                                "name is " +
                                mechanismName );
                boolean isExternal = isExternalMechanism( mechanismName );
                ldc.printDebug( "LDAPSaslBind.bind: calling " +
                                "saslBind" );
                JDAPBindResponse response =
                    saslBind( ldc, mechanismName, outVals );
                int resultCode = response.getResultCode();
                while ( !checkForSASLBindCompletion( ldc, resultCode ) ) {
                    if ( isExternal ) {
                        continue;
                    }
                    byte[] b = response.getCredentials();
                    if ( b == null ) {
                        b = new byte[0];
                    }
                    outVals = evaluateChallenge( b );
                    System.out.println( "SaslClient.evaluateChallenge returned [" + ((outVals != null) ? new String( outVals ) : "null") + "] for [" +
                                        new String( b ) + "]" );

                    if ( resultCode == LDAPException.SUCCESS ) {
                        // we're done; don't expect to send another BIND
                        if ( outVals != null ) {
                            throw new LDAPException(
                                "Protocol error: attempting to send " +
                                "response after completion" );
                        }
                        break;
                    }
                    response = saslBind( ldc, mechanismName, outVals );
                    resultCode = response.getResultCode();
                }

                // Make sure authentication REALLY is complete
                if ( !isComplete() ) {
                    // Authentication session hijacked!
                    throw new LDAPException( "The server indicates that " +
                                             "authentication is successful" +
                                             ", but the SASL driver " +
                                             "indicates that authentication" +
                                             " is not yet done.",
                                             LDAPException.OTHER );
                } else if ( resultCode == LDAPException.SUCCESS ) {
                    // Has a security layer been negotiated?
                    String qop = getNegotiatedProperty( QOP );
                    if ( (qop != null) &&
                         (qop.equalsIgnoreCase("auth-int") ||
                          qop.equalsIgnoreCase("auth-conf")) ) {

                        // Use SaslClient.wrap() and SaslClient.unwrap() for
                        // future communication with server
                        ldc.setInputStream(
                            new SecureInputStream( ldc.getInputStream(),
                                                   _saslClient ) );
                        ldc.setOutputStream(
                            new SecureOutputStream( ldc.getOutputStream(),
                                                    _saslClient ) );
                    }                
                    ldc.markConnAsBound();
                }

            } catch (LDAPException e) {
                throw e;
            } catch (Exception e) {
                throw new LDAPException(e.toString(), LDAPException.OTHER);
            }
        }
    }

    boolean isExternalMechanism( String name ) {
        return name.equalsIgnoreCase( LDAPConnection.EXTERNAL_MECHANISM );
    }

    // Wrapper functions for dynamically invoking methods of SaslClient
    private boolean checkForSASLBindCompletion( LDAPConnection ldc,
                                                int resultCode )
        throws LDAPException {

        ldc.printDebug( "LDAPSaslBind.bind: saslBind " +
                        "returned " + resultCode );
        if ( isComplete() ) {
            if ( (resultCode == LDAPException.SUCCESS) ||
                 (resultCode == LDAPException.SASL_BIND_IN_PROGRESS) ) {
                return true;
            } else {
                throw new LDAPException( "Authentication failed", resultCode );
            }
        } else {
            return false;
        }
    }

    private boolean hasInitialResponse()
        throws LDAPException {
        if ( !_useReflection ) {
            return ((SaslClient)_saslClient).hasInitialResponse();
        } else {
            return ((Boolean)DynamicInvoker.invokeMethod(
                _saslClient,
                _saslClient.getClass().getName(),
                "hasInitialResponse",
                null,
                null)).booleanValue();
        }
    }

    private String getMechanismName()
        throws LDAPException {
        if ( !_useReflection ) {
            return ((SaslClient)_saslClient).getMechanismName();
        } else {
            return (String)DynamicInvoker.invokeMethod(
                _saslClient,
                _saslClient.getClass().getName(),
                "getMechanismName",
                null,
                null );
        }
    }

    private boolean isComplete()
        throws LDAPException {
        if ( !_useReflection ) {
            return ((SaslClient)_saslClient).isComplete();
        } else {
            return ((Boolean)DynamicInvoker.invokeMethod(
                _saslClient,
                _saslClient.getClass().getName(),
                "isComplete",
                null,
                null)).booleanValue();
        }
    }

    private byte[] evaluateChallenge( byte[] b )
        throws LDAPException {
        try {
            if ( !_useReflection ) {
                return ((SaslClient)_saslClient).evaluateChallenge( b );
            } else {
                Object[] args = { b };
                String[] argNames = { "[B" }; // class name for byte array
                return (byte[])DynamicInvoker.invokeMethod(
                    _saslClient,
                    _saslClient.getClass().getName(),
                    "evaluateChallenge",
                    args,
                    argNames );
            }
        } catch ( Exception e ) {
            throw new LDAPException( "",
                                     LDAPException.PARAM_ERROR,
                                     e );
        }
    }

    private String getNegotiatedProperty( String propName )
        throws LDAPException {
        try {
            if ( !_useReflection ) {
                return ((SaslClient)_saslClient).getNegotiatedProperty( propName );
            } else {
                Object[] args = { propName };
                String[] argNames = { "String" };
                return (String)DynamicInvoker.invokeMethod(
                    _saslClient,
                    _saslClient.getClass().getName(),
                    "getNegotiatedProperty",
                    args,
                    argNames );
            }
        } catch ( Exception e ) {
            throw new LDAPException( "",
                                     LDAPException.PARAM_ERROR,
                                     e );
        }
    }

    private JDAPBindResponse saslBind( LDAPConnection ldc,
                                       String mechanismName,
                                       byte[] credentials )
        throws LDAPException {

        LDAPResponseQueue myListener = ldc.getResponseListener();

        try {
            ldc.sendRequest( new JDAPBindRequest( 3,
                                                  _dn,
                                                  mechanismName,
                                                  credentials ),
                             myListener, ldc.getConstraints() );
            LDAPMessage response = myListener.getResponse();

            JDAPProtocolOp protocolOp = response.getProtocolOp();
            if ( protocolOp instanceof JDAPBindResponse ) {
                return (JDAPBindResponse)protocolOp;
            } else {
                throw new LDAPException( "Unknown response from the " +
                                         "server during SASL bind",
                                         LDAPException.OTHER );
            }
        } finally {
            ldc.releaseResponseListener( myListener );
        }
    }

    private static final String CALLBACK_HANDLER =
        "javax.security.auth.callback.CallbackHandler";
    static final String CLIENTPKGS =
        "javax.security.sasl.client.pkgs";
    private static final String QOP = "javax.security.sasl.qop";
    private static final String REFLECTION_PROP =
        "org.ietf.ldap.sasl.reflect";
    private String _dn;
    private String[] _mechanisms;
    private Map _props = null;
    private CallbackHandler _cbh;
    private Object _saslClient = null;
    private static boolean _useReflection =
    ( System.getProperty(REFLECTION_PROP) != null );

    // ??? Security layer support not implemented
    class SecureInputStream extends InputStream {
        public SecureInputStream( InputStream is, Object saslClient ) {
            _input = is;
            _saslClient = saslClient;
        }
        public int read() throws IOException {
            return _input.read();
        }
        private InputStream _input;
        private Object _saslClient;
    }

    class SecureOutputStream extends OutputStream {
        public SecureOutputStream( OutputStream os, Object saslClient ) {
            _output = os;
            _saslClient = saslClient;
        }
        public void write( int b ) throws IOException {
            _output.write( b );
        }
        private OutputStream _output;
        private Object _saslClient;
    }
}
