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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap;

import java.util.*;
import java.io.*;

/**
 * This interface summarizes the basic functionality available in the
 * Lightweight Directory Access Protocol (LDAP) version 2. (See
 * <A HREF="http://www.cis.ohio-state.edu/htbin/rfc/rfc1777.html" TARGET="_blank">RFC 1777</A>
 * for the definition of the protocol.)
 * <P>
 *
 * In the general model for this protocol, objects exist under a directory
 * in a particular server. Objects are identified by unique, hierarchical names
 * called Distinguished Names, commonly abreviated "DN". An example of a DN:
 * <pre>
 *  cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=us
 * </pre>
 * Objects have attributes, of the form
 * <pre>
 *  attributeName = attributeValue(s)
 * </pre>
 * Attribute names must be Strings, and attribute values can be any 8-bit
 * sequence (Strings or binary values).
 *
 * @version 1.0
 */
public interface LDAPv2 {
    /**
     * The default port number for LDAP servers.  You can specify
     * this identifier when calling the <CODE>LDAPConnection.connect</CODE>
     * method to connect to an LDAP server.
     * @see netscape.ldap.LDAPConnection#connect
     */
    public final static int DEFAULT_PORT = 389;

    /**
     * Option specifying how aliases are dereferenced.
     * <P>
     *
     * This option can have one of the following values:
     * <UL>
     * <LI><A HREF="#DEREF_NEVER"><CODE>DEREF_NEVER</CODE></A>
     * <LI><A HREF="#DEREF_FINDING"><CODE>DEREF_FINDING</CODE></A>
     * <LI><A HREF="#DEREF_SEARCHING"><CODE>DEREF_SEARCHING</CODE></A>
     * <LI><A HREF="#DEREF_ALWAYS"><CODE>DEREF_ALWAYS</CODE></A>
     * </UL>
     * <P>
     *
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int DEREF = 2;

    /**
     * Option specifying the maximum number of search results that
     * can be returned.
     * <P>
     *
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int SIZELIMIT = 3;

    /**
     * Option specifying the maximum number of milliseconds to
     * wait for an operation to be completed.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int TIMELIMIT = 4;

    /**
     * Option specifying the maximum number of milliseconds the 
     * server should wait when returning search results. 
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int SERVER_TIMELIMIT = 5;

    /**
     * Option specifying whether or not referrals to other LDAP
     * servers are followed automatically.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPRebindAuth
     */
    public static final int REFERRALS = 8;

    /**
     * Option specifying the object containing the method for
     * getting authentication information (the distinguished name
     * and password) used during a referral.  For example, when
     * referred to another LDAP server, your client uses this object
     * to obtain the DN and password.  Your client authenticates to
     * the LDAP server using this DN and password.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPRebindAuth
     */
    public static final int REFERRALS_REBIND_PROC = 9;

    /**
     * Option specifying the maximum number of referrals to follow
     * in a sequence when requesting an LDAP operation.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int REFERRALS_HOP_LIMIT   = 10;
    
    /**
     * Option specifying the object containing the method for
     * authenticating to the server.  
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     * @see netscape.ldap.LDAPBind
     */
    public static final int BIND = 13;

    /**
     * Option specifying the version of the LDAP protocol
     * used by your client when interacting with the LDAP server.
     * If no version is set, the default version is 2.  If you
     * are planning to use LDAP v3 features (such as controls
     * or extended operations), you should set this version to 3
     * or specify version 3 as an argument to the <CODE>authenticate</CODE>
     * method of the <CODE>LDAPConnection</CODE> object.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     * @see netscape.ldap.LDAPConnection#authenticate(int, java.lang.String, java.lang.String)
     */
    public static final int PROTOCOL_VERSION = 17;

    /**
     * Option specifying the number of results to return at a time.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int BATCHSIZE = 20;


    /*
     * Valid options for Scope
     */

    /**
     * Specifies that the scope of a search includes
     * only the base DN (distinguished name).
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     */
    public static final int SCOPE_BASE = 0;

    /**
     * Specifies that the scope of a search includes
     * only the entries one level below the base DN (distinguished name).
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)   */
    public static final int SCOPE_ONE = 1;

    /**
     * Specifies that the scope of a search includes
     * the base DN (distinguished name) and all entries at all levels
     * beneath that base.
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)   */
    public static final int SCOPE_SUB = 2;


    /*
     * Valid options for Dereference
     */

    /**
     * Specifies that aliases are never dereferenced.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int DEREF_NEVER = 0;

    /**
     * Specifies that aliases are dereferenced when searching the
     * entries beneath the starting point of the search (but
     * not when finding the starting entry).
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int DEREF_SEARCHING = 1;

    /**
     * Specifies that aliases are dereferenced when finding the
     * starting point for the search (but not when searching
     * under that starting entry).
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int DEREF_FINDING = 2;

    /**
     * Specifies that aliases are always dereferenced.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int DEREF_ALWAYS = 3;

    /**
     * Connects to the LDAP server.
     * @param host Hostname of the LDAP server.
     * @param port Port number of the LDAP server.  To specify the
     * default, well-known port, use <CODE>DEFAULT_PORT</CODE>.
     * @exception LDAPException Failed to connect to the server.
     */
    public void connect (String host, int port) throws LDAPException;

    /**
     * Connects and authenticates to the LDAP server.
     * @param host Hostname of the LDAP server.
     * @param port Port number of the LDAP server.  To specify the
     * default, well-known port, use <CODE>DEFAULT_PORT</CODE>.
     * @param dn Distinguished name to use for authentication.
     * @param passwd Password for authentication.
     * @exception LDAPException Failed to connect and authenticate to the server.
     */
    public void connect (String host, int port, String dn, String passwd)
        throws LDAPException;

    /**
     * Disconnects from the LDAP server.  Subsequent operational calls
     * will first try to re-establish the connection to the same LDAP server.
     * @exception LDAPException Failed to disconnect from the server.
     */
    public void disconnect() throws LDAPException;

    /** 
     * Notifies the server to not send additional results associated with this
     * <CODE>LDAPSearchResults</CODE> object, and discards any results already 
     * received.
     * @param results LDAPSearchResults object returned from a search.
     * @exception LDAPException Failed to notify the server.
     */
    public void abandon(LDAPSearchResults results) throws LDAPException;

    /**
     * Authenticates user with the LDAP server.
     * @param Dn Distinguished name to use for authentication.
     * @param passwd Password for authentication.
     * @exception LDAPException Failed to authenticate to the server.
     */
    public void authenticate (String DN, String passwd) throws LDAPException;

    /**
     * Authenticates user with the LDAP server.
     * @param Dn Distinguished name to use for authentication.
     * @param passwd Password for authentication.
     * @exception LDAPException Failed to authenticate to the server.
     */
    public void bind (String DN, String passwd) throws LDAPException;

    /**
     * Read the entry corresponding to the specified distinguished name (DN).
     * @param DN Distinguished name of the entry to retrieve.
     * @exception LDAPException Failed to retrieve the specified entry.
     */
    public LDAPEntry read (String DN) throws LDAPException;

    /**
     * Read the entry corresponding to the specified distinguished name (DN),
     * and retrieve only the specified attributes.
     * @param DN Distinguished name of the entry to retrieve.
     * @param attrs Names of attributes to retrieve.
     * @exception LDAPException Failed to retrieve the specified entry.
     */
    public LDAPEntry read (String DN, String attrs[]) throws LDAPException;

    /**
     * Read the entry corresponding to the specified distinguished name (DN),
     * and retrieve only the specified attributes.
     * @param DN Distinguished name of the entry to retrieve.
     * @param attrs Names of attributes to retrieve.
     * @param cons The constraints set for the read operation.
     * @exception LDAPException Failed to retrieve the specified entry.
     */
    public LDAPEntry read (String DN, String attrs[], LDAPSearchConstraints cons)
      throws LDAPException;

    /**
     * Searches for entries in the directory.
     * @param base Starting point for the search in the directory
     *   (distinguished name).
     * @param scope Indicates whether the scope of the search includes
     *   only the base DN (equivalent to a read operation), only the entries
     *   one level below the base DN, or all entries at all levels beneath
     *   the base DN (including the base DN itself).
     * @param filter String which describes the search criteria. The format
     *   of the string is described fully in
     *   <A HREF="http://www.cis.ohio-state.edu/htbin/rfc/rfc1558.html" TARGET="_blank">RFC 1558</A>.
     * @param attrs Names of the attributes to return for each matching
     *   directory entry.  If <CODE>null</CODE>, all attributes are returned.
     * @param attrsOnly If <CODE>true</CODE>, the search will return only the names of
     *   the attributes (and not their values).
     * @exception LDAPException Failed to complete the requested search.
     */
    public LDAPSearchResults search (String base, int scope, String filter,
      String[] attrs, boolean attrsOnly) throws LDAPException;

    /**
     * Searches for entries in the directory.
     * @param base Starting point for the search in the directory
     *   (distinguished name).
     * @param scope Indicates whether the scope of the search includes
     *   only the base DN (equivalent to a read operation), only the entries
     *   one level below the base DN, or all entries at all levels beneath
     *   the base DN (including the base DN itself).
     * @param filter String which describes the search criteria. The format
     *   of the string is described fully in
     *   <A HREF="http://www.cis.ohio-state.edu/htbin/rfc/rfc1558.html" TARGET="_blank">RFC 1558</A>.
     * @param attrs Names of the attributes to return for each matching
     *   directory entry.  If <CODE>null</CODE>, all attributes are returned.
     * @param attrsOnly If <CODE>true</CODE>, the search will return only the names of
     *   the attributes (and not their values).
     * @param cons Constraints specific to the search (for example, the maximum number
     *   of entries to return or the maximum time to wait for the search operation to complete).
     * @exception LDAPException Failed to complete the requested search.
     */
    public LDAPSearchResults search (String base, int scope, String filter,
      String[] attrs, boolean attrsOnly, LDAPSearchConstraints cons)
      throws LDAPException;

    /**
     * Compares the given entry's attribute value to the specified
     * attribute value.
     * @param DN Distinguished name of the entry that you want compared
     *   against the specified attribute value.
     * @param attr Attribute name and value to use in the comparison.
     * @exception LDAPException Failed to perform the comparison.
     */
    public boolean compare (String DN, LDAPAttribute attr) throws LDAPException;

    /**
     * Compares the given entry's attribute value to the specified
     * attribute value.
     * @param DN Distinguished name of the entry that you want compared
     *   against the specified attribute value.
     * @param attr Attribute name and value to use in the comparison.
     * @param cons The constraints set for the compare operation.
     * @exception LDAPException Failed to perform the comparison.
     */
    public boolean compare (String DN, LDAPAttribute attr,
      LDAPConstraints cons) throws LDAPException;

    /**
     * Adds an entry to the directory.
     * @param entry New entry to add to the directory.
     * @exception LDAPException Failed to add the entry to the directory.
     */
    public void add (LDAPEntry entry) throws LDAPException;

    /**
     * Adds an entry to the directory.
     * @param entry New entry to add to the directory.
     * @param cons The constraints set for the add operation.
     * @exception LDAPException Failed to add the entry to the directory.
     */
    public void add (LDAPEntry entry, LDAPConstraints cons)
      throws LDAPException;

    /**
     * Modifies an attribute of a directory entry.
     * @param DN Distinguished name identifying the entry to be modified.
     * @param mod The modification to be made.
     * @exception LDAPException Failed to modify the specified entry.
     */
    public void modify (String DN, LDAPModification mod) throws LDAPException;

    /**
     * Modifies an attribute of a directory entry.
     * @param DN Distinguished name identifying the entry to be modified.
     * @param mod The modification to be made.
     * @param cons The constraints set for the modify operation.
     * @exception LDAPException Failed to modify the specified entry.
     */
    public void modify (String DN, LDAPModification mod,
      LDAPConstraints cons) throws LDAPException;

    /**
     * Modifies the attributes of a directory entry.
     * @param DN Distinguished name identifying the entry to be modified.
     * @param mod List of the modifications to be made.
     * @exception LDAPException Failed to modify the specified entry.
     */
    public void modify (String DN, LDAPModificationSet mods ) throws LDAPException;

    /**
     * Modifies the attributes of a directory entry.
     * @param DN Distinguished name identifying the entry to be modified.
     * @param mod List of the modifications to be made.
     * @param cons The constraints set for the modify operation.
     * @exception LDAPException Failed to modify the specified entry.
     */
    public void modify (String DN, LDAPModificationSet mods,
      LDAPConstraints cons ) throws LDAPException;

    /**
     * Removes an entry from the directory.
     * @param DN Distinguished name identifying the entry to remove.
     * @exception LDAPException Failed to remove the entry from the directory.
     */
    public void delete( String DN ) throws LDAPException;

    /**
     * Removes an entry from the directory.
     * @param DN Distinguished name identifying the entry to remove.
     * @param cons The constraints set for the delete operation.
     * @exception LDAPException Failed to remove the entry from the directory.
     */
    public void delete( String DN, LDAPConstraints cons )
      throws LDAPException;

    /**
     * Changes the name of an entry in the directory.
     * @param DN Original distinguished name (DN) of entry.
     * @param newRDN New relative distinguished name (RDN) of the entry.
     * @param deleteOldRDN Specifies whether or not the original RDN remains
     *   as an attribute of the entry. If <CODE>true</CODE>, the original RDN
     *   is no longer an attribute of the entry.
     * @exception LDAPException Failed to rename the entry in the directory.
     */
    public void rename ( String DN, String newRDN, boolean deleteOldRDN )
        throws LDAPException;

    /**
     * Changes the name of an entry in the directory.
     * @param DN Original distinguished name (DN) of entry.
     * @param newRDN New relative distinguished name (RDN) of the entry.
     * @param deleteOldRDN Specifies whether or not the original RDN remains
     *   as an attribute of the entry. If <CODE>true</CODE>, the original RDN
     *   is no longer an attribute of the entry.
     * @param cons The constraints set for the rename operation.
     * @exception LDAPException Failed to rename the entry in the directory.
     */
    public void rename ( String DN, String newRDN, boolean deleteOldRDN,
      LDAPConstraints cons ) throws LDAPException;

    /**
     * Retrieves an option that applies to the connection.
     * The particular meaning may be implementation-dependent.
     * The standard options are the options described by
     * the <CODE>LDAPSearchConstraints</CODE> and <CODE>LDAPConstraints</CODE>
     * classes.
     * @exception LDAPException Failed to retrieve the value of the specified option.
     */
    public Object getOption( int option ) throws LDAPException;

    /**
     * Sets an option that applies to the connection.
     * The particular meaning may be implementation-dependent.
     * The standard options are the options described by
     * the <CODE>LDAPSearchConstraints</CODE> and <CODE>LDAPConstraints</CODE>
     * classes.
     * @exception LDAPException Failed to set the specified option.
     */
    public void setOption( int option, Object value ) throws LDAPException;
}
