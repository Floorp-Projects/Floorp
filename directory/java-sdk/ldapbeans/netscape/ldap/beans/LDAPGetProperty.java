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
package netscape.ldap.beans;

import netscape.ldap.*;
import netscape.ldap.beans.LDAPBasePropertySupport;
import java.util.Enumeration;
import java.util.Vector;
import java.io.Serializable;
import java.beans.*;
import java.awt.event.*;


/**
 * Invisible Bean that just takes a name and password, host and
 * port, and directory base and attribute name, and returns an
 * attribute's values from a Directory Server. The values are
 * assumed to be strings, and are returned as an array. The
 * search has the scope "SUB", which means that it will find
 * an entry anywhere at or below the directory base.
 * <BR><BR>
 * Optionally, a client can register as a PropertyChangeListener
 * and will be notified when the values are available.
 *<BR><BR>
 * A null result means the property fetch failed. The reason is
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
 */
public class LDAPGetProperty extends LDAPBasePropertySupport implements
  Serializable {

    /**
     * Constructor with no parameters
     */
    public LDAPGetProperty() {}

    /**
    * Constructor with host, port, and base initializers
    * @param theHost host string
    * @param thePort port number
    * @param theBase directory base string
    */
    public LDAPGetProperty( String theHost, int thePort, String theBase ) {
        setHost( theHost );
        setPort( thePort );
        setBase( theBase );
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

    private void notifyResult( String[] newResult ) {
        String sNewResult = convertToString( newResult );
        firePropertyChange( "result", _result, newResult );
        _sResult = sNewResult;
        _result = newResult;
    }

    private void notifyResult( Vector newResult ) {
        firePropertyChange( "result", _resultV, newResult );
        _resultV = (Vector)newResult.clone();
    }

    private void notifyResult( String error ) {
        firePropertyChange( "error", _errorMsg, error );
        _errorMsg = error;
    }

    public void setDNs(PropertyChangeEvent evt) {
        Object obj = (Object)evt.getNewValue();
        if ((obj != null) && (obj instanceof String[])) {
            String[] strings = (String[])obj;
            if (strings.length == 0)
                return;
            _dns = new String[strings.length];
            System.arraycopy(obj, 0, _dns, 0, strings.length);
            System.out.println("length of dns -> "+_dns.length);
        }
    }

    /**
     * Searches and returns values for a specified attribute
     * @param host host string
     * @param port port number
     * @param base directory base string
     * @param filter search filter
     * @param attribute name of property to return values for
     * @return Array of values for the property
     */
    public String[] getProperty( String host, int port, String base,
                                 String filter, String attribute) {
        setHost( host );
        setPort( port );
        setBase( base );
        setFilter( filter );
        setAttribute( attribute );
        return getProperty();
    }

    // Added this method in order to get exposed in BDK
    public void getProperty(ActionEvent x) {
        getProperty();
    }

    /**
     * Searches and returns values of a previously registered property,
     * using previously set parameters
     * @return Array of values for the property
     */
    public String[] getProperty() {
        if ( (_attribute.length() < 1) || (getFilter().length() < 1) ) {
            printDebug( "Invalid attribute name or filter" );
            setErrorCode( INVALID_PARAMETER );
            notifyResult( (String[])null );
            return null;
        }

        String[] res = null;
        LDAPConnection m_ldc;
        try {
            m_ldc = new LDAPConnection();
            printDebug("Connecting to " + getHost() +
                               " " + getPort());
            connect( m_ldc, getHost(), getPort());
        } catch (Exception e) {
            printDebug( "Failed to connect to " + getHost() + ": "
                        + e.toString() );
            setErrorCode( CONNECT_ERROR );
            notifyResult( (String[])null );
            return null;
        }

        // Authenticate?
        if (_dns != null) {
            for (int i=0; i<_dns.length; i++) {
                try {
                    m_ldc.authenticate(_dns[i], getAuthPassword());
                    break;
                } catch (Exception e) {
                    if (i == _dns.length-1) {
                        printDebug( "Failed to authenticate to " +
                                    getHost() + ": " + e.toString() );
                        setErrorCode( AUTHENTICATION_ERROR );
                        notifyResult( (String[])null );
                        return null;
                    }
                }
            }
        } else if ( (!getAuthDN().equals("")) && (!getAuthPassword().equals("")) ) {
            printDebug( "Authenticating " + getAuthDN() + " - " +
                                getAuthPassword() );
            try {
                m_ldc.authenticate( getAuthDN(), getAuthPassword() );
            } catch (Exception e) {
                printDebug( "Failed to authenticate to " +
                                    getHost() + ": " + e.toString() );
                setErrorCode( AUTHENTICATION_ERROR );
                notifyResult( (String[])null );
                return null;
            }
        }

        int numDataEntries = 0;
        // Search
        try {
            String[] attrs = new String[1];
            attrs[0] = _attribute;
            LDAPSearchResults results = m_ldc.search(getBase(),
                                                     getScope(),
                                                     getFilter(),
                                                     attrs, false);

            // Should be only one result, at most
            LDAPEntry currEntry = null;
            LDAPEntry entry = null;
            while ( results.hasMoreElements() ) {
                try {
                    currEntry = results.next();
                    if (numDataEntries == 0)
                        entry = currEntry;
                    if (++numDataEntries > 1) {
                        printDebug( "More than one entry found for " +
                                getFilter() );
                        setErrorCode( AMBIGUOUS_RESULTS );
                        break;
                    }
                } catch (LDAPException e) {
                    if (getDebug())
                        notifyResult(e.toString());
                    continue;
                }
            }
            if (numDataEntries == 1) {
                printDebug( "... " + entry.getDN() );
                // Good - exactly one entry found; get the attribute
                // Treat DN as a special case
                if ( _attribute.equalsIgnoreCase( "dn" ) ) {
                    res = new String[1];
                    res[0] = entry.getDN();
                    setErrorCode( OK );
                } else {
                    LDAPAttributeSet attrset = entry.getAttributeSet();
                    Enumeration attrsenum = attrset.getAttributes();
                    if (attrsenum.hasMoreElements()) {
                        LDAPAttribute attr =
                            (LDAPAttribute)attrsenum.nextElement();
                        printDebug( attr.getName() + " = " );
                        // Get the values as strings
                        Enumeration valuesenum = attr.getStringValues();
                        if (valuesenum != null) {
                          // Create a string array for the results
                            Vector v = new Vector();
                            while (valuesenum.hasMoreElements()) {
                                String val = (String)valuesenum.nextElement();
                                v.addElement( val );
                                printDebug( "\t\t" + val );
                            }
                            res = new String[v.size()];
                            v.copyInto( res );
                            setErrorCode( OK );
                        } else {
                            Enumeration byteEnum = attr.getByteValues();
                            Vector v = new Vector();
                            while (byteEnum.hasMoreElements()) {
                                byte[] val = (byte[])byteEnum.nextElement();
                                v.addElement( val );
                                printDebug( "\t\t" + val );
                            }
                            setErrorCode( OK );
                            notifyResult(v);
                            return (res = null);
                        }
                    } else {
                        printDebug( "No properties found for " +
                                    _attribute );
                        setErrorCode( PROPERTY_NOT_FOUND );
                    }
                }
            }
        } catch (Exception e) {
            if (getDebug()) {
                printDebug( "Failed to search for " + getFilter() + ": "
                            + e.toString() );
            }
            setErrorCode( PROPERTY_NOT_FOUND );
        }

        if (numDataEntries == 0) {
            printDebug( "No entries found for " + getFilter() );
            setErrorCode( PROPERTY_NOT_FOUND );
        }

        // Disconnect
        try {
            if ( (m_ldc != null) && m_ldc.isConnected() )
                m_ldc.disconnect();
        } catch ( Exception e ) {
        }

        notifyResult( res );
        return res;
    }

  /**
   * The main body if we run it as application instead of applet.
   * @param args list of arguments
   */
    public static void main(String args[]) {
        if (args.length != 5) {
            System.out.println( "Usage: LDAPGetProperty host port base" +
                                " filter attribute" );
            System.exit(1);
        }
        LDAPGetProperty app = new LDAPGetProperty();
        app.setHost( args[0] );
        app.setPort( java.lang.Integer.parseInt( args[1] ) );
        app.setBase( args[2] );
        app.setFilter( args[3] );
        app.setAttribute( args[4] );
        String[] response = app.getProperty();
        if ( response != null ) {
            for( int i = 0; i < response.length; i++ )
                System.out.println( "\t" + response[i] );
        }
        System.exit(0);
    }

    /*
     * Variables
     */
    private String[] _dns = null;
    private String _attribute = new String("cn");
    transient private String[] _result;
    private Vector _resultV = null;
    private String _sResult = null;
    private String _errorMsg = null;
}
