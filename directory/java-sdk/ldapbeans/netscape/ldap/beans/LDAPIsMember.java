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
import netscape.ldap.util.*;
import java.util.Enumeration;
import java.util.StringTokenizer;
import java.io.Serializable;
import java.awt.event.*;

/**
 * Invisible Bean that just takes a host and port, optional
 * authentication name and password, and DN of a group and another DN
 * which might be a member of the group, and returns true or
 * false, depending on whether the second DN is a member of the first.
 * <BR>
 * Also handles the case of dynamic groups by derefencing the URL
 * and searching for membership based on the url search.
 * <BR>
 * It doesn't handle nested groups.
 * <BR><BR>
 * A false result means the member could not be identified as
 * belonging to the group. The exact reason is
 * available through getErrorCode(), which returns one of
 * the following:
 *<PRE>
 *     OK
 *     INVALID_PARAMETER
 *     CONNECT_ERROR
 *     AUTHENTICATION_ERROR
 *     PROPERTY_NOT_FOUND
 *     AMBIGUOUS_RESULTS
 *     NO_SUCH_OBJECT
 *</PRE>
 */
public class LDAPIsMember extends LDAPBasePropertySupport
                          implements Serializable {

    /**
     * Constructor with no parameters
     */
    public LDAPIsMember() {}

    /**
    * Constructor with host, port, and group DN initializers
    * @param host host string
    * @param port port number
    * @param group distinguished name of the group
    */
    public LDAPIsMember( String host, int port, String group ) {
        setHost( host );
        setPort( port );
        setGroup( group );
    }

    /**
    * Constructor with host, port, authentication DN and password
    * and group DN initializers
    * @param host host string
    * @param port port number
    * @param dn fully qualified distinguished name to authenticate
    * @param password password for authenticating the dn
    * @param group distinguished name of the group
    */
    public LDAPIsMember( String host, int port,
                         String dn, String password, String theGroup ) {
        setHost( host );
        setPort( port );
        setGroup( theGroup );
        setAuthDN( dn );
        setAuthPassword( password );
    }

    private void notifyResult( String newResult ) {
        firePropertyChange( "result", _result, newResult );
        _result = newResult;
    }

    /**
     * Checks if an entity (specified by distinguished name) is a
     * member of a particular group (specified by distinguished name)
     * @return true if the specified member belongs to the group
     */
    public boolean isMember() {
        String host = getHost();
        int port = getPort();
        String dn = getAuthDN();
        String password = getAuthPassword();
        String group = getGroup();
        String member = getMember();
        _result = new String("");

        if ( (host == null) || (host.length() < 1)  ) {
            printDebug( "Invalid host name" );
            setErrorCode( INVALID_PARAMETER );
            notifyResult(null);
            return false;
        }

        if ( (member == null) || (group == null) ||
             (member.length() < 1) || (group.length() < 1) ) {
            printDebug( "Invalid member or group name" );
            setErrorCode( INVALID_PARAMETER );
            notifyResult(null);
            return false;
        }

        LDAPConnection m_ldc;
        boolean isMember = false;
        try {
            m_ldc = new LDAPConnection();
            printDebug("Connecting to " + host +
                               " " + port);
            connect( m_ldc, getHost(), getPort());
        } catch (Exception e) {
            printDebug( "Failed to connect to " + host + ": "
                        + e.toString() );
            setErrorCode( CONNECT_ERROR );
            notifyResult(null);
            return false;
        }

        // Authenticate?
        if ( (dn != null) && (password != null) &&
             (dn.length() > 0) && (password.length() > 0) ) {
            printDebug( "Authenticating " + dn + " - " + password );
            try {
                m_ldc.authenticate( dn, password );
            } catch (Exception e) {
                printDebug( "Failed to authenticate to " +
                                    host + ": " + e.toString() );
                setErrorCode( AUTHENTICATION_ERROR );
                notifyResult(null);
                return false;
            }
        }

        int numDataEntries = 0;
        // Search
        try {
            String[] attrs = new String[4];
            attrs[0] = "member";
            attrs[1] = "uniqueMember";
            attrs[2] = "memberOfGroup";
            attrs[3] = "memberurl";
            LDAPSearchResults results =
                m_ldc.search( group,
                              LDAPConnection.SCOPE_BASE,
                              "objectclass=*",
                              attrs, false);

            // Should be only one result, at most
            LDAPEntry entry = null;
            LDAPEntry currEntry = null;
            while ( results.hasMoreElements() ) {
                try {
                    currEntry = (LDAPEntry)results.next();
                    if (numDataEntries == 0)
                        entry = currEntry;
                    if (++numDataEntries > 1) {
                        printDebug( "More than one entry found for " +
                                getFilter() );
                        setErrorCode( AMBIGUOUS_RESULTS );
                        break;
                    }
                } catch (LDAPReferralException e) {
                    if (getDebug()) {
                        notifyResult("Referral URLs: ");
                        LDAPUrl refUrls[] = e.getURLs();
                        for (int i = 0; i < refUrls.length; i++)
                            notifyResult(refUrls[i].getUrl());
                    }
                    continue;
                } catch (LDAPException e) {
                    if (getDebug())
                        notifyResult(e.toString());
                    continue;
                }
            }
            if (numDataEntries == 1) {
                printDebug( "... " + entry.getDN() );
                String normMember = normalizeDN( member );
                // Good - exactly one entry found; get the attributes
                LDAPAttributeSet attrset = entry.getAttributeSet();
                Enumeration attrsenum = attrset.getAttributes();
                while ( attrsenum.hasMoreElements() && !isMember ) {
                    LDAPAttribute attr =
                        (LDAPAttribute)attrsenum.nextElement();
                    printDebug( attr.getName() + " = " );
                    boolean urlHandler =
                        attr.getName().equalsIgnoreCase("memberurl");
                    /* Get the values as strings.
                       The following code also handles dynamic
                       groups by calling URLMatch to see if an entry
                       DN is found via a URL search.
                       This is transparent to the caller of the bean.
                    */
                    Enumeration valuesenum = attr.getStringValues();
                    if (valuesenum != null) {
                        while (valuesenum.hasMoreElements()) {
                            String val = (String)valuesenum.nextElement();
                            if (urlHandler) {
                                if ( URLMatch(m_ldc, val, normMember) ) {
                                    isMember = true;
                                    setErrorCode( OK );
                                    break;
                                }
                            } 
                            printDebug( "\t\t" + val );
                            String normFound = normalizeDN( val );
                            if ( normMember.equals( normFound ) ) {
                                isMember = true;
                              setErrorCode( OK );
                              break;
                            }
                        }
                    } else {
                        setErrorCode(PROPERTY_NOT_FOUND);
                        printDebug("Failed to do string conversion for "+
                                   attr.getName());
                    }
                }
                if ( !isMember )
                    setErrorCode( PROPERTY_NOT_FOUND );
            }
        } catch (Exception e) {
            printDebug( "Failed to search for " + group + ": "
                                    + e.toString() );
            setErrorCode( NO_SUCH_OBJECT );
        }

        if (numDataEntries == 0) {
            printDebug( "No entries found for " + group );
            setErrorCode( NO_SUCH_OBJECT );
        }

        try {
            if ( (m_ldc != null) && m_ldc.isConnected() )
                m_ldc.disconnect();
        } catch ( Exception e ) {
        }

        if (isMember)
            notifyResult("Y");
        else
            notifyResult("N");

        return isMember;
    }

    /**
     * Checks if an entity (specified by distinguished name) is a
     * member of a particular group (specified by distinguished name)
     * @param host host string
     * @param port port number
     * @param dn fully qualified distinguished name to authenticate;
     * can be null or ""
     * @param password password for authenticating the dn; can be null
     * or ""
     * @param group distinguished name of the group
     * @param member distinguished name of member to be checked
     * @return true if the specified member belongs to the group
     */
    public boolean isMember( String host, int port,
                             String dn, String password,
                             String group, String member ) {
        setHost(host);
        setPort(port);
        setAuthDN(dn);
        setAuthPassword(password);
        setGroup(group);
        setMember(member);
        return isMember();
    }

    /**
     * Checks if an entity (specified by distinguished name) is a
     * member of a particular group (specified by distinguished name)
     * @return true if the specified member belongs to the group
     */
    public void isMember(ActionEvent e) {
        isMember();
    }

    /**
     * Returns the distinguished name of the group
     * @return group name
     */
    public String getGroup() {
        return _group;
    }

    /**
     * Sets the distinguished name of the group
     * @param group group name
     */
    public void setGroup( String group ) {
        _group = group;
    }

    /**
     * Returns the distinguished name of the member
     * @return member name
     */
    public String getMember() {
        return _member;
    }

    /**
     * Sets the distinguished name of the member
     * @param member member name
     */
    public void setMember( String member ) {
        _member = member;
    }

    private String normalizeDN( String dn ) {
        return new DN( dn ).toRFCString().toUpperCase();
    }

    /**
     * Return true if normMember is result of url search.
     * Urls from dynamic groups do not typically contain
     * the host and port so we need to fix them before
     * constructing an LDAP URL.  
     * current ldap:///.... make ldap://host:port/...
     **/
     private boolean URLMatch(LDAPConnection ld, String URL,
                              String normMemberDN) {
        String cURL = URL;
        boolean isMember = false;
        int loc = URL.indexOf(":///");
        if ( loc > 0) {
            cURL = URL.substring(0,loc) + "://" + ld.getHost() + 
                ":" + ld.getPort() + URL.substring(loc+3);
        }
        printDebug("URLMatch: url = " + cURL +
                   ", member DN = " + normMemberDN);
        LDAPUrl ldapurl;
        try {
            ldapurl = new LDAPUrl(cURL);
            printDebug("URL ->"+ldapurl.getUrl());
        } catch (java.net.MalformedURLException murl) {
            printDebug("bad URL");
            return isMember;
        }
        
        try {
            LDAPSearchResults results = ld.search(ldapurl);
            String entry = "";
            while ( results.hasMoreElements() && !isMember ) {
                try {
                    entry = ((LDAPEntry)results.next()).getDN();
                    String normEntry = normalizeDN( entry );
                    if (normEntry.equals(normMemberDN)) {
                        isMember = true;
                        break;
                    }
                } catch (LDAPReferralException e) {
                    if (getDebug()) {
                        notifyResult("Referral URLs: ");
                        LDAPUrl refUrls[] = e.getURLs();
                        for (int i = 0; i < refUrls.length; i++)
                            notifyResult(refUrls[i].getUrl());
                    }
                    continue;
                } catch (LDAPException e) {
                    if (getDebug())
                        notifyResult(e.toString());
                    continue;
                }
            }
        } catch (LDAPException lde) {
            printDebug("Failed search for url " + ldapurl.getUrl());
            setErrorCode(NO_SUCH_OBJECT);
        }
        
        return isMember;
     }

  /**
   * The main body if we run it as application instead of applet.
   * @param args list of arguments
   */
    public static void main(String args[]) {
        if (args.length != 4) {
            System.out.println( "Usage: LDAPIsMember host port group" +
                                " member" );
            System.exit(1);
        }
        LDAPIsMember app = new LDAPIsMember();
        app.setHost( args[0] );
        app.setPort( java.lang.Integer.parseInt( args[1] ) );
        app.setGroup( args[2] );
        app.setMember( args[3] );
        boolean response = app.isMember();
        if ( response == false )
            System.out.println( "Not a member" );
        else
            System.out.println( "Is a member" );
        System.exit(0);
    }

    /*
     * Variables
     */
    public static final int OK = 0;
    public static final int INVALID_PARAMETER = 1;
    public static final int CONNECT_ERROR = 2;
    public static final int AUTHENTICATION_ERROR = 3;
    public static final int PROPERTY_NOT_FOUND = 4;
    public static final int AMBIGUOUS_RESULTS = 5;
    public static final int NO_SUCH_OBJECT = 5;
    private String _group = new String("");
    private String _member = new String("");
    private String _result = new String("");
}
