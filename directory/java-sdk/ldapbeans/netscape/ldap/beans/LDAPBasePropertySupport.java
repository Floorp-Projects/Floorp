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
package netscape.ldap.beans;

import netscape.ldap.*;
import java.util.StringTokenizer;

// This class has a bound property
import java.beans.PropertyChangeListener;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeSupport;
import java.io.Serializable;

/**
 * This is a base class that is extended by various specialized LDAP
 * Beans. It provides the common properties and accessors used by
 * them.
 */

public class LDAPBasePropertySupport implements Serializable {

    /**
     * Constructor with no parameters
     */
    public LDAPBasePropertySupport() {}

    /**
     * Returns the host to search at.
     * @return DNS name or dotted IP name of host to search at
     */
    public String getHost() {
        return _host;
    }

    /**
    * Sets host string.
    * @param theHost host name
    */
    public void setHost( String theHost ) {
        _host = theHost;
    }

    /**
     * Returns the port to search at.
     * @return Port to search at
     */
    public int getPort() {
        return _port;
    }

    /**
    * Sets port number.
    * @param thePort port
    */
    public void setPort( int thePort ) {
        _port = thePort;
    }

    /**
     * Returns the directory base to search at.
     * @return directory base to search
     */
    public String getBase() {
        return _base;
    }

    /**
    * Sets the starting base
    * @param theBase starting base
    */
    public void setBase( String theBase ) {
        _base = theBase;
    }

    /**
     * Returns the DN to authenticate as; null or empty for anonymous.
     * @return DN to authenticate as
     */
    public String getAuthDN() {
        return _authDN;
    }

    /**
     * Sets the DN to authenticate as; null or empty for anonymous.
     * @param authDN the DN to authenticate as
     */
    public void setAuthDN( String authDN ) {
        this._authDN =  authDN;
    }

    /**
     * Returns the password for the DN to authenticate as
     * @return Password of DN to authenticate as
     */
    public String getAuthPassword() {
        return _authPassword;
    }

    /**
     * Sets the password for the DN to authenticate as
     * @param authPassword the password to use in authentication
     */
    public void setAuthPassword( String authPassword ) {
        this._authPassword = authPassword;
    }

    /**
     * Returns the user name
     * @return The user name
     */
    public String getUserName() {
        return _userName;
    }

    /**
     * Set the user name. The name should be of the form "Polly Plum".
     * @param name The user name
     */
    public void setUserName( String name ) {
        _userName = name;
    }

    /**
     * Return the user ID.
     * @return name the user id
     */
    public String getUserID() {
        return _userID;
    }

    /**
     * Set the user ID.
     * @param name the value of the user id
     */
    public void setUserID( String name ) {
        _userID = name;
    }

    /**
     * Get the current search scope
     * @return the current search scope as integer
     */
    public int getScope() {
        return _scope;
    }

    /**
     * Set the search scope using an integer
     * @param scope one of LDAPConnection.SCOPE_BASE,
     * LDAPConnection.SCOPE_SUB, LDAPConnection.SCOPE_ONE
     */
    public void setScope( int scope ) {
        _scope = scope;
    }

    /**
     * Returns the search filter
     * @return search filter
     */
    public String getFilter() {
        return _filter;
    }

    /**
     * Sets the search filter
     * @param filter search filter
     */
    public void setFilter( String filter ) {
        _filter = filter;
    }

    /**
     * Returns true if debug output is on
     * @return true if debug output is on
     */
    public boolean getDebug() {
        return _debug;
    }

    /**
     * Turns debug output on or off
     * @param on true for debug output
     */
    public void setDebug( boolean on ) {
        _debug = on;
    }

    /**
     * Returns the latest error code
     * @return The latest error code
     */
    public int getErrorCode() {
        return _errCode;
    }

    /**
     * Sets an error code for retrieval by a client
     * @param code An error code
     */
    public void setErrorCode( int code ) {
        _errCode = code;
    }

    /**
     * Add a client to be notified when an authentication result is in
     * @param listener a client to be notified of changes
     */
    public void addPropertyChangeListener( PropertyChangeListener listener ) {
        printDebug( "Adding listener " + listener );
        m_propSupport.addPropertyChangeListener( listener );
    }

    /**
     * Remove a client which had requested notification on authentication
     * @param listener a client to not be notified of changes
     */
    public void removePropertyChangeListener(
                              PropertyChangeListener listener ) {
        m_propSupport.removePropertyChangeListener( listener );
    }

    /**
     * Support for bound property notification
     * @param propName Name of changed property
     * @param oldValue Previous value of property
     * @param newValue New value of property
     */
    public void firePropertyChange(
                              String propName,
                              Object oldValue,
                              Object newValue ) {

        if (m_propSupport == null)
            m_propSupport = new PropertyChangeSupport( this );

        m_propSupport.firePropertyChange( propName, oldValue, newValue );
    }

    protected void printDebug( String s ) {
        if ( _debug )
            System.out.println( s );
    }

    /**
     * Sets up basic connection privileges for Communicator if necessary,
     * and connects
     * @param host Host to connect to.
     * @param port Port number.
     * @exception LDAPException from connect()
     */
    protected void connect( LDAPConnection conn, String host, int port )
        throws LDAPException {
        boolean needsPrivileges = true;
        /* Running standalone? */
        SecurityManager sec = System.getSecurityManager();
        printDebug( "Security manager = " + sec );
        if ( sec == null ) {
            printDebug( "No security manager" );
            /* Not an applet, we can do what we want to */
            needsPrivileges = false;
        /* Can't do instanceof on an abstract class */
        } else if ( sec.toString().startsWith("java.lang.NullSecurityManager") ) {
            printDebug( "No security manager" );
            /* Not an applet, we can do what we want to */
            needsPrivileges = false;
        } else if ( sec.toString().startsWith(
            "netscape.security.AppletSecurity" ) ) {

            /* Connecting to the local host? */
            try {
                if ( host.equalsIgnoreCase(
                    java.net.InetAddress.getLocalHost().getHostName() ) )
                    needsPrivileges = false;
            } catch ( java.net.UnknownHostException e ) {
            }
        }

        if ( needsPrivileges ) {
            /* Running as applet. Is PrivilegeManager around? */
            String mgr = "netscape.security.PrivilegeManager";
            try {
                Class c = Class.forName( mgr );
                java.lang.reflect.Method[] m = c.getMethods();
                if ( m != null ) {
                    for( int i = 0; i < m.length; i++ ) {
                        if ( m[i].getName().equals( "enablePrivilege" ) ) {
                            try {
                                Object[] args = new Object[1];
                                args[0] = new String( "UniversalConnect" );
                                m[i].invoke( null, args );
                                printDebug( "UniversalConnect enabled" );
                                args[0] = new String( "UniversalPropertyRead" );
                                m[i].invoke( null, args );
                                printDebug( "UniversalPropertyRead enabled" );
                            } catch ( Exception e ) {
                                printDebug( "Exception on invoking " +
                                            "enablePrivilege: " +
                                            e.toString() );
                                break;
                            }
                            break;
                        }
                    }
                }
            } catch ( ClassNotFoundException e ) {
                printDebug( "no " + mgr );
            }
        }

        conn.connect( host, port );
        setDefaultReferralCredentials( conn );
    }

    protected void setDefaultReferralCredentials(
        LDAPConnection conn ) {
        final LDAPConnection m_conn = conn;
        LDAPRebind rebind = new LDAPRebind() {
            public LDAPRebindAuth getRebindAuthentication(
                String host,
                int port ) {
                    return new LDAPRebindAuth( 
                        m_conn.getAuthenticationDN(),
                        m_conn.getAuthenticationPassword() );
                }
        };
        try {
            conn.setOption(LDAPConnection.REFERRALS, Boolean.TRUE);
            conn.setOption(LDAPConnection.REFERRALS_REBIND_PROC, rebind);
        } catch (LDAPException e) {
            //will never happen
        }
    }

    /**
     * Utility method to convert an array of Strings to a single String
     * with line feeds between elements.
     * @param aResult The array of Strings to convert
     * @return A String with the elements separated by line feeds
     */
    public String convertToString( String[] aResult ) {
        String sResult = "";
        if ( null != aResult ) {
            for ( int i = 0; i < aResult.length; i++ ) {
                sResult += aResult[i] + "\n";
            }
        }
        return sResult;
    }

    /*
     * Variables
     */
    /* Error codes from search operations, etc */
    public static final int OK = 0;
    public static final int INVALID_PARAMETER = 1;
    public static final int CONNECT_ERROR = 2;
    public static final int AUTHENTICATION_ERROR = 3;
    public static final int PROPERTY_NOT_FOUND = 4;
    public static final int AMBIGUOUS_RESULTS = 5;
    public static final int NO_SUCH_OBJECT = 6;

    private boolean _debug = false;
    private int _errCode = 0;
    private String _host = new String("localhost");
    private int _port = 389;
    private int _scope = LDAPConnection.SCOPE_SUB;
    private String _base = new String("");
    private String _filter = new String("");
    private String _authDN = new String("");
    private String _authPassword = new String("");
    private String _userName = new String("");
    private String _userID = new String("");
    transient private PropertyChangeSupport m_propSupport =
              new PropertyChangeSupport( this );
}

