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
import netscape.ldap.client.*;
import netscape.ldap.client.opers.*;
import java.io.*;

/**
 * Specifies additional features available in version 3 of the
 * LDAP protocol. (To view preliminary information on this work
 * in progress, see the LDAP v3 internet draft.  You can find the
 * latest version of this document listed under the Internet-Drafts
 * section of the
 * <A HREF="http://www.ietf.cnri.reston.va.us/html.charters/asid-charter.html"
 * TARGET=_blank">ASID home page</A>.)
 *
 * @version 1.0
 */
public interface LDAPv3 extends LDAPv2 {

    /**
     * Connects and authenticates to the LDAP server with the specified LDAP
     * protocol version.
     * @param version LDAP protocol version requested: currently 2 or 3
     * @param host Hostname of the LDAP server.
     * @param port Port number of the LDAP server.  To specify the
     * default, well-known port, use <CODE>DEFAULT_PORT</CODE>.
     * @param dn Distinguished name to use for authentication.
     * @param passwd Password for authentication.
     * @exception LDAPException Failed to connect and authenticate to the server.
     */
    public void connect(int version, String host, int port, String dn,
      String passwd) throws LDAPException;

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and password, with the
     * specified LDAP protocol version. If the server does not support
     * the requested protocol version, an exception is thrown.  If the
     * object has been disconnected from an LDAP server, this method
     * attempts to reconnect to the server. If the object had already
     * authenticated, the old authentication is discarded.
     * @param version LDAP protocol version requested: currently 2 or 3.
     * @param dn If non-null and non-empty, specifies that the
     * connection and all operations through it should be
     * authenticated with dn as the distinguished name.
     * @param passwd If non-null and non-empty, specifies that the
     * connection and all operations through it should be
     * authenticated with passwd as password.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(int version,
                             String dn,
                             String passwd)
                             throws LDAPException;

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and password, with the
     * specified LDAP protocol version. If the server does not support
     * the requested protocol version, an exception is thrown.  If the
     * object has been disconnected from an LDAP server, this method
     * attempts to reconnect to the server. If the object had already
     * authenticated, the old authentication is discarded.
     * @param version LDAP protocol version requested: currently 2 or 3.
     * @param dn If non-null and non-empty, specifies that the
     * connection and all operations through it should be
     * authenticated with dn as the distinguished name.
     * @param passwd If non-null and non-empty, specifies that the
     * connection and all operations through it should be
     * authenticated with passwd as password.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind(int version,
                     String dn,
                     String passwd)
                     throws LDAPException;
    
    /**
     * Performs an extended operation on the directory. Extended operations
     * are part of version 3 of the LDAP protocol.
     * <P>
     *
     * @param op LDAPExtendedOperation object specifying the OID of the
     * extended operation and the data to be used in the operation.
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see LDAPExtendedOperation
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op )
                                     throws LDAPException;

    /**
     * Renames and moves an entry in the directory.
     * @param DN Original distinguished name (DN) for the entry.
     * @param newRDN New relative distinguished name (RDN) for the entry.
     * @param newParentDN Distinguished name of the new parent entry of the
     *   specified entry.
     * @exception LDAPException Failed to rename the specified entry.
     */
    public void rename( String DN, String newRDN, String newParentDN,
      boolean deleteOldRDN ) throws LDAPException;

    /**
     * Renames and moves an entry in the directory.
     * @param DN Original distinguished name (DN) for the entry.
     * @param newRDN New relative distinguished name (RDN) for the entry.
     * @param newParentDN Distinguished name of the new parent entry of the
     *   specified entry.
     * @param cons The constraints set for the rename operation.
     * @exception LDAPException Failed to rename the specified entry.
     */
    public void rename( String DN, String newRDN, String newParentDN,
      boolean deleteOldRDN, LDAPConstraints cons ) throws LDAPException;

    /**
     * Returns an array of the latest controls (if any) from server.
     * @return An array of the controls returned by an operation,
     * or <CODE>null</CODE> if none.
     * @see netscape.ldap.LDAPControl
     */
    public LDAPControl[] getResponseControls();

    /**
     * Option specifying client controls for LDAP operations. These
     * controls are interpreted by the client and are not passed
     * to the LDAP server.
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int CLIENTCONTROLS   = 11;

    /**
     * Option specifying server controls for LDAP operations. These
     * controls are passed to the LDAP server. They may also be returned by
     * the server.
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int SERVERCONTROLS   = 12;

    /**
     * Attribute type that you can specify in the LDAPConnection
     * search method if you don't want to retrieve any of the
     * attribute types for entries found by the search.
     * @see netscape.ldap.LDAPConnection#search
     */
    public static final String NO_ATTRS = "1.1";

    /**
     * Attribute type that you can specify in the LDAPConnection
     * search method if you want to retrieve all attribute types.
     * You can use this if you want to retrieve all attributes in
     * addition to an operational attribute.  For example:
     * <P>
     *
     * <PRE>
     * ...
     * String [] MY_ATTRS = { LDAPv3.ALL_USER_ATTRS, "modifiersName",
     *     "modifyTimestamp" };
     * LDAPSearchResults res = ld.search( MY_SEARCHBASE,
     *     LDAPConnection.SCOPE_SUB, MY_FILTER, MY_ATTRS, false, cons );
     * ...
     * </PRE>
     * @see netscape.ldap.LDAPConnection#search
     */
    public static final String ALL_USER_ATTRS = "*";

}
