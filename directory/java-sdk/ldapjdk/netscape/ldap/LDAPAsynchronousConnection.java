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
package netscape.ldap;

import java.util.*;
import java.io.*;

/**
 * Encapsulates a connection to an LDAP server, providing access to the input queue
 * for messages received. 
 *
 * @version 1.0
 */
public interface LDAPAsynchronousConnection {
  
    /**
     * Adds an entry to the directory.
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPSearchListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPEntry
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener add(LDAPEntry entry,
                                    LDAPResponseListener listener)
                                    throws LDAPException;
 
    /**
     * Adds an entry to the directory and allows you to specify constraints
     * for this LDAP add operation by using an <CODE>LDAPConstraints</CODE>
     * object. For example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPEntry
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener add(LDAPEntry entry,
                                    LDAPResponseListener listener,
                                    LDAPConstraints cons)
                                    throws LDAPException;


    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and password. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param dn if non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name
     * @param passwd if non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name and passwd as password
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener bind(String dn,
                                     String passwd,
                                     LDAPResponseListener listener)
                                     throws LDAPException;

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and password  and allows you
     * to specify constraints for this LDAP add operation by using an
     *  <CODE>LDAPConstraints</CODE> object. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param dn if non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name
     * @param passwd if non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name and passwd as password
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener bind(String dn,
                                     String passwd,
                                     LDAPResponseListener listener,
                                     LDAPConstraints cons) 
                                     throws LDAPException;


    
    /**
     * Deletes the entry for the specified DN from the directory.
     * 
     * @param dn distinguished name of the entry to delete
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener delete(String dn,
                                       LDAPResponseListener listener)
                                       throws LDAPException;

    /**
     * Deletes the entry for the specified DN from the directory.
     * 
     * @param dn distinguished name of the entry to delete
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener delete(String dn,
                                      LDAPResponseListener listener,
                                      LDAPConstraints cons)
                                      throws LDAPException;

    
    /**
     * Makes a single change to an existing entry in the directory.
     * For example, changes the value of an attribute, adds a new attribute
     * value, or removes an existing attribute value.<BR>
     * The LDAPModification object specifies both the change to be made and
     * the LDAPAttribute value to be changed.
     * 
     * @param dn distinguished name of the entry to modify
     * @param mod a single change to make to an entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPModification
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener modify(String dn,
                                       LDAPModification mod,
                                       LDAPResponseListener listener)
                                       throws LDAPException;   
    
    /**
     * Makes a single change to an existing entry in the directory.
     * For example, changes the value of an attribute, adds a new attribute
     * value, or removes an existing attribute value.<BR>
     * The LDAPModification object specifies both the change to be made and
     * the LDAPAttribute value to be changed.
     * 
     * @param dn distinguished name of the entry to modify
     * @param mod a single change to make to an entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPModification
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener modify(String dn,
                                       LDAPModification mod,
                                       LDAPResponseListener listener,
                                       LDAPConstraints cons)
                                       throws LDAPException;    

    /**
     * Makes a set of changes to an existing entry in the directory.
     * For example, changes attribute values, adds new attribute values, or
     * removes existing attribute values.
     * <P>
     * @param dn distinguished name of the entry to modify
     * @param mods a set of modifications to make to the entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPModificationSet
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener modify(String dn,
                                       LDAPModificationSet mods,
                                       LDAPResponseListener listener)
                                       throws LDAPException;   
    
    /**
     * Makes a set of changes to an existing entry in the directory.
     * For example, changes attribute values, adds new attribute values, or
     * removes existing attribute values.
     * 
     * @param dn distinguished name of the entry to modify
     * @param mods a set of modifications to make to the entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPModificationSet
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener modify(String dn,
                                       LDAPModificationSet mods,
                                       LDAPResponseListener listener,
                                       LDAPConstraints cons)
                                       throws LDAPException;    
    

    /**
     * Renames an existing entry in the directory.
     * 
     * @param dn current distinguished name of the entry
     * @param newRdn new relative distinguished name for the entry
     * @param deleteOldRdn if <code>true</code>, the old name is not retained as an
     * attribute value
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener rename(String dn,
                                       String newRdn,
                                       boolean deleteOldRdn,
                                       LDAPResponseListener listener)
                                       throws LDAPException;

    /**
     * Renames an existing entry in the directory.
     * 
     * @param dn current distinguished name of the entry
     * @param newRdn new relative distinguished name for the entry
     * @param deleteOldRdn if <code>true</code>, the old name is not retained as an
     * attribute value
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener rename(String dn,
                                       String newRdn,
                                       boolean deleteOldRdn,
                                       LDAPResponseListener listener,
                                       LDAPConstraints cons)
                                       throws LDAPException;
    
    /**
     * Performs the search specified by the criteria that you enter. <P>
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only those entries that are one level below the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter search filter specifying the search criteria
     * @param attrs list of attributes to return in the search results
     * @param typesOnly if <code>true</code>, returns the names but not the values of the
     * attributes found. If <code>false</code>, returns the names and values of the
     * attributes found.
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPSearchListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPAsynchronousConnection#abandon(netscape.ldap.LDAPSearchListener)
     */
    public LDAPSearchListener search(String base,
                                     int scope,
                                     String filter,
                                     String attrs[],
                                     boolean typesOnly,
                                     LDAPSearchListener listener)
                                     throws LDAPException;

    /**
     * Performs the search specified by the criteria that you enter.
     * This method also allows you to specify constraints for the search
     * (such as the maximum number of entries to find or the
     * maximum time to wait for search results). <P>
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only those entries that are one level below the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter search filter specifying the search criteria
     * @param attrs list of attributes to return in the search results
     * @param typesOnly if <code>true</code>, returns the names but not the values of the
     * attributes found.  If <code>false</code>, returns the names and  values for
     * attributes found.
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to this search (for example, the
     * maximum number of entries to return)
     * @return LDAPSearchListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPAsynchronousConnection#abandon(netscape.ldap.LDAPSearchListener)
     */
    public LDAPSearchListener search(String base,
                                     int scope,
                                     String filter,
                                     String attrs[],
                                     boolean typesOnly,
                                     LDAPSearchListener listener,
                                      LDAPSearchConstraints cons)
                                     throws LDAPException;
    
    /**
     * Compares an attribute value with one in the directory. The result
     * is obtained by calling <CODE>getResultCode</CODE> on the 
     * <CODE>LDAPResponse</CODE> from the <CODE>LDAPResponseListener</CODE>.
     * The code will be <CODE>LDAPException.COMPARE_TRUE</CODE> or 
     * <CODE>LDAPException.COMPARE_FALSE</CODE>. 
     * 
     * @param dn distinguished name of the entry to compare
     * @param attr attribute with a value to compare
     * @param listener handler for messages returned from a server in response
     * to the specified request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to the specified request.
     * @exception LDAPException Failed to send request.
     */
    public LDAPResponseListener compare(String dn, 
                                        LDAPAttribute attr, 
                                        LDAPResponseListener listener)
                                        throws LDAPException;
    
    /**
     * Compares an attribute value with one in the directory. The result
     * is obtained by calling <CODE>getResultCode</CODE> on the 
     * <CODE>LDAPResponse</CODE> from the <CODE>LDAPResponseListener</CODE>.
     * The code will be <CODE>LDAPException.COMPARE_TRUE</CODE> or 
     * <CODE>LDAPException.COMPARE_FALSE</CODE>. 
     * 
     * @param dn distinguished name of the entry to compare
     * @param attr attribute with a value to compare
     * @param listener handler for messages returned from a server in response
     * to the specified request. If it is null, a listener object is created internally.
     * @param cons constraints specific to this operation
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to the specified request.
     * @exception LDAPException Failed to send request.
     */
    public LDAPResponseListener compare(String dn, 
                                        LDAPAttribute attr, 
                                        LDAPResponseListener listener,
                                        LDAPConstraints cons) 
                                        throws LDAPException;
    
    /**
     * Cancels the ldap request with the specified id and discards
     * any results already received.
     * 
     * @param id an LDAP request ID
     * @exception LDAPException Failed to send request.
     */
    public void abandon(int id) throws LDAPException;
    
    /**
     * Cancels all outstanding search requests associated with this
     * LDAPSearchListener object and discards any results already received.
     * 
     * @param searchlistener a search listener returned from a search
     * @exception LDAPException Failed to send request.
     */
    public void abandon(LDAPSearchListener searchlistener)
                        throws LDAPException;
}
