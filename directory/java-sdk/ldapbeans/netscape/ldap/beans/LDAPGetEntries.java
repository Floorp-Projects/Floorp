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
import netscape.ldap.beans.LDAPBasePropertySupport;
import java.util.Enumeration;
import java.util.Vector;
import java.io.Serializable;
import java.awt.event.*;


/**
 * Invisible Bean that just takes a host, port, directory base,
 * search string, and optional authentication name and password,
 * and returns a list of all matching DNs. The search has the scope
 * "SUB", which means that it will find an entry anywhere at or
 * below the directory base, unless a different scope is specified.
 * <BR><BR>
 * Optionally, a client can register as a PropertyChangeListener
 * and will be notified when the values are available.
 *<BR><BR>
 * A null result means no matching DNs were found. The reason is
 * available through getErrorCode(), which returns one of
 * the following:
 *<PRE>
 *     OK
 *     INVALID_PARAMETER
 *     CONNECT_ERROR
 *     AUTHENTICATION_ERROR
 *     PROPERTY_NOT_FOUND
 *     AMBIGUOUS_RESULTS
 *</PRE>
 *
 */
public class LDAPGetEntries extends LDAPBasePropertySupport implements Serializable {

    /**
     * Constructor with no parameters
     */
    public LDAPGetEntries() {
        super();
    }

    /**
    * Constructor with host, port, and base initializers
    * @param theHost host string
    * @param thePort port number
    * @param theBase directory base string
    */
    public LDAPGetEntries( String theHost, int thePort, String theBase ) {
        setHost( theHost );
        setPort( thePort );
        setBase( theBase );
    }

    /**
    * Constructor with host, port, base, and scope initializers
    * @param theHost host string
    * @param thePort port number
    * @param theBase directory base string
    * @param theScope one of LDAPConnection.SCOPE_BASE,
    * LDAPConnection.SCOPE_SUB, LDAPConnection.SCOPE_ONE
    */
    public LDAPGetEntries( String theHost,
                           int thePort,
                           String theBase,
                           int theScope ) {
        setHost( theHost );
        setPort( thePort );
        setBase( theBase );
        setScope( theScope );
    }

    private void notifyResult( String error ) {
        firePropertyChange( "error", _errorMsg, error );
        _errorMsg = error;
    }

    private void notifyResult( String[] newResult ) {
        String sNewResult = convertToString( newResult );
        firePropertyChange( "result", _result, newResult );
        _sResult = sNewResult;
        _result = newResult;
    }

    /**
     * Returns the name of the attribute to retrieve
     * @return attribute name to retrieve
     */
    public String getAttribute() {
        return _attribute;
    }

    /**
     * Sets the attribute to retrieve
     */
    public void setAttribute( String attr ) {
        _attribute = attr;
    }

    public void setResultString( String sNewValue ) {
        _sResult = sNewValue;
    }

    public String getResultString() {
        return _sResult;
    }

    /**
     * Searches and returns values for a specified attribute
     * @param host host string
     * @param port port number
     * @param base directory base string
     * @param scope one of LDAPConnection.SCOPE_BASE,
     * LDAPConnection.SCOPE_SUB, LDAPConnection.SCOPE_ONE
     * @param filter search filter
     * @param attribute name of property to return values for
     * @return Array of values for the property
     */
    public String[] getEntries( String host,
                                int port,
                                String base,
                                int scope,
                                String filter) {
        setHost( host );
        setPort( port );
        setBase( base );
        setScope( scope );
        setFilter( filter );
        return getEntries();
    }

    /**
     * Searches and returns values for a specified attribute
     * @param host host string
     * @param port port number
     * @param base directory base string
     * @param scope one of LDAPConnection.SCOPE_BASE,
     * LDAPConnection.SCOPE_SUB, LDAPConnection.SCOPE_ONE
     * @param userName The user name
     * @param userid The user id
     * @return Array of DNs
     */
    public String[] getEntries( String host,
                                int port,
                                String base,
                                int scope,
                                String userid,
                                String userName) {
        setHost( host );
        setPort( port );
        setBase( base );
        setScope( scope );
        if (userName == null)
            userName = new String("");
        setUserName( userName );
        if (userid == null)
            userid = new String("");
        setUserID( userid );
        return getEntries();
    }

    // Added this method in order to get exposed in BDK
    public void getEntries(ActionEvent x) {
        getEntries();
    }

    /**
     * Searches and returns values of a previously registered property,
     * using previously set parameters
     * @return Array of values for the property
     */
    public String[] getEntries() {
        boolean invalid = false;
        if ((getUserName().length() < 1) && (getUserID().length() < 1) &&
          (getFilter().length() < 1)) {
            printDebug("No user name or user ID");
            invalid = true;
        } else if ( (getHost().length() < 1) || (getBase().length() < 1) ) {
            printDebug( "Invalid host name or search base" );
            invalid = true;
        }
        if ( invalid ) {
            setErrorCode( INVALID_PARAMETER );
            notifyResult( (String)null);
            return null;
        }

        if (getFilter().length() < 1) {
            String filter = new String("");
            if ((getUserName().length() > 1) && (getUserID().length() > 1)) {
                filter = "(|(cn="+getUserName()+")(uid="+getUserID()+"))";
            } else if (getUserName().length() > 1) {
                filter = "cn="+getUserName();
            } else if (getUserID().length() > 1) {
                filter = "uid="+getUserID();
            }
            setFilter(filter);
        }

        String[] res = null;
        LDAPConnection m_ldc = new LDAPConnection();
        try {
            try {
                printDebug("Connecting to " + getHost() +
                           " " + getPort());
                connect( m_ldc, getHost(), getPort());
            } catch (Exception e) {
                printDebug( "Failed to connect to " + getHost() + ": " +
                            e.toString() );
                setErrorCode( CONNECT_ERROR );
                notifyResult( (String)null );
                m_ldc = null;
                throw( new Exception() );
            }

            // Authenticate?
            if ( (!getAuthDN().equals("")) &&
                 (!getAuthPassword().equals("")) ) {

                printDebug( "Authenticating " + getAuthDN() );
                try {
                    m_ldc.authenticate( getAuthDN(), getAuthPassword() );
                } catch (Exception e) {
                    printDebug( "Failed to authenticate: " + e.toString() );
                    setErrorCode( AUTHENTICATION_ERROR );
                    notifyResult( (String)null );
                    throw( new Exception() );
                }
            }

            // Search
            try {
                printDebug("Searching " + getBase() +
                           " for " + getFilter() + ", scope = " + getScope());
                String[] attrs = { _attribute };
                LDAPSearchResults results = m_ldc.search( getBase(),
                                                          getScope(),
                                                          getFilter(),
                                                          attrs,
                                                          false);

                // Create a vector for the results
                Vector v = new Vector();
                LDAPEntry entry = null;
                while ( results.hasMoreElements() ) {
                    try {
                        entry = results.next();
                    } catch (LDAPException e) {
                        if (getDebug())
                            notifyResult(e.toString());
                        continue;
                    }
                    // Add the DN to the list
                    String value = "";
                    if ( _attribute.equals("dn") ) {
                        value = entry.getDN();
                    } else {
                        LDAPAttribute attr = entry.getAttribute( _attribute );
                        if ( attr != null ) {
                            Enumeration vals = attr.getStringValues();
                            if ( (vals != null) && (vals.hasMoreElements()) ) {
                                value = (String)vals.nextElement();
                            }
                        }
                    }
                    v.addElement( value );
                    printDebug( "... " + value );
                }
                // Pull out the DNs and create a string array
                if ( v.size() > 0 ) {
                    res = new String[v.size()];
                    v.copyInto( res );
                    v.removeAllElements();
                    setErrorCode( OK );
                } else {
                    printDebug( "No entries found for " + getFilter() );
                    setErrorCode( PROPERTY_NOT_FOUND );
                }
            } catch (Exception e) {
                if (getDebug()) {
                    printDebug( "Failed to search for " + getFilter() + ": " +
                                e.toString() );
                }
                setErrorCode( PROPERTY_NOT_FOUND );
            }
        } catch (Exception e) {
        }

        // Disconnect
        try {
            if ( (m_ldc != null) && m_ldc.isConnected() )
                m_ldc.disconnect();
        } catch ( Exception e ) {
        }

        // Notify any clients with a PropertyChangeEvent
        notifyResult( res );
        return res;
    }

  /**
   * The main body if we run it as application instead of applet.
   * @param args list of arguments
   */
    public static void main(String args[]) {
        String[] scope = { "base", "one", "sub" };
        int scopeIndex = -1;
        for( int i = 0; (i < scope.length) && (args.length == 5); i++ ) {
            if ( args[3].equalsIgnoreCase(scope[i]) ) {
                scopeIndex = i;
                break;
            }
        }
            
        if ( scopeIndex < 0 ) {
            System.out.println( "Usage: LDAPGetEntries host port base" +
                                " scope filter" );
            System.exit(1);
        }
        LDAPGetEntries app = new LDAPGetEntries();
        app.setHost( args[0] );
        app.setPort( java.lang.Integer.parseInt( args[1] ) );
        app.setBase( args[2] );
        app.setScope( scopeIndex );
        app.setFilter( args[4] );
        String[] response = app.getEntries();
        if ( response != null ) {
            for( int i = 0; i < response.length; i++ )
                System.out.println( "\t" + response[i] );
        }
        System.exit(0);
    }

    /*
     * Variables
     */
    private String _attribute = "dn";
    private String[] _result;
    private String _sResult = null;
    private String _errorMsg = null;
}
