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
package netscape.ldap;

/**
 * Represents a set of search preferences.
 * You can set these preferences for a particular search
 * by creating an <CODE>LDAPSearchConstraints</CODE> object,
 * specifying your preferences, and passing the object to
 * the <CODE>LDAPConnection.search</CODE> method.
 * <P>
 *
 * @version 1.0
 */
public class LDAPSearchConstraints implements Cloneable {

    private int timeLimit;
    private int deref;
    private int maxRes;
    private boolean referrals;
    private int batch;
    private LDAPRebind m_rebind_proc;
    private int m_hop_limit;
    private LDAPControl[] m_clientControls;
    private LDAPControl[] m_serverControls;
    transient private int m_maxBacklog = 100;

    /**
     * Constructs an <CODE>LDAPSearchConstraints</CODE> object that specifies
     * the default set of search constraints.
     */
    public LDAPSearchConstraints() {
        timeLimit = 0;
        deref = 0;
        maxRes = 1000;
        referrals = false;
        batch = 1;
        m_rebind_proc = null;
        m_hop_limit = 10;
        m_clientControls = null;
        m_serverControls = null;
    }

    /**
     * Constructs a new <CODE>LDAPSearchConstraints</CODE> object and allows you
     * to specify the search constraints in that object.
     * <P>
     * @param msLimit Maximum time in milliseconds to wait for results (0
     * by default, which means that there is no maximum time limit)
     * @param dereference Either <CODE>LDAPv2.DEREF_NEVER</CODE>,
     * <CODE>LDAPv2.DEREF_FINDING</CODE>,
     * <CODE>LDAPv2.DEREF_SEARCHING</CODE>, or
     * <CODE>LDAPv2.DEREF_ALWAYS</CODE> (see LDAPConnection.setOption).
     * <CODE>LDAPv2.DEREF_NEVER</CODE> is the default.
     * @param maxResults Maximum number of search results to return
     * (1000 by default)
     * @param doReferrals Specify <CODE>true</CODE> to follow referrals
     * automatically, or <CODE>False</CODE> to throw an
     * <CODE>LDAPReferralException</CODE> error if the server sends back
     * a referral (<CODE>False</CODE> by default)
     * @param batchSize Specify the number of results to return at a time
     * (1 by default)
     * @param rebind_proc Specifies the object of the class that
     * implements the <CODE>LDAPRebind</CODE> interface (you need to
     * define this class).  The object will be using when the client
     * follows referrals automatically.  The object provides the client
     * with a method for getting the distinguished name and password
     * used to authenticate to another LDAP server during a referral.
     * (This field is <CODE>null</CODE> by default.)
     * @param hop_limit Maximum number of referrals to follow in a
     * sequence when attempting to resolve a request.
     * @see netscape.ldap.LDAPConnection#setOption(int, java.lang.Object)
     * @see netscape.ldap.LDAPConnection#search(netscape.ldap.LDAPUrl, netscape.ldap.LDAPSearchConstraints)
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     */
    public LDAPSearchConstraints( int msLimit, int dereference,
        int maxResults, boolean doReferrals, int batchSize,
        LDAPRebind rebind_proc, int hop_limit) {
        timeLimit = msLimit;
        deref = dereference;
        maxRes = maxResults;
        referrals = doReferrals;
        batch = batchSize;
        m_rebind_proc = rebind_proc;
        m_hop_limit = hop_limit;
        m_clientControls = null;
        m_serverControls = null;
    }

    /**
     * Returns the maximum number of milliseconds to wait for any operation
     * under these search constraints. If 0, there is no maximum time limit
     * on waiting for the operation results.
     * @return Maximum number of milliseconds to wait for operation results.
     */
    public int getTimeLimit() {
        return timeLimit*1000;
    }

    /**
     * Specifies how aliases should be dereferenced.
     * @return <CODE>LDAPv2.DEREF_NEVER</CODE> to
     * never follow ("dereference") aliases,
     * <CODE>LDAPv2.DEREF_FINDING</CODE> to dereference when finding
     * the starting point for the search (but not when searching
     * under that starting entry), <CODE>LDAPv2.DEREF_SEARCHING</CODE>
     * to dereference when searching the entries beneath the
     * starting point of the search (but not when finding the starting
     * entry), or <CODE>LDAPv2.DEREF_ALWAYS</CODE> to always
     * dereference aliases.
     */
    public int getDereference() {
        return deref;
    }

    /**
     * Returns the maximum number of search results to be returned; 0 means
     * no limit.
     * @return Maximum number of search results to be returned.
     */
    public int getMaxResults() {
        return maxRes;
    }

    /**
     * Specifies whether nor not referrals are followed automatically.
     * Returns <CODE>true</CODE> if referrals are to be followed automatically,
     * or <CODE>false</CODE> if referrals throw an <CODE>LDAPReferralException</CODE>.
     * @return <CODE>true</CODE> if referrals are followed automatically, <CODE>False</CODE>
     * if referrals throw an <CODE>LDAPReferralException</CODE>.
     */
    public boolean getReferrals() {
        return referrals;
    }

    /**
     * Returns the suggested number of results to return at a time during
     * search. This should be 0 if intermediate results are not needed, and
     * 1 if results are to be processed as they come in.
     * @return Number of results to return at a time.
     */
    public int getBatchSize() {
        return batch;
    }

    /**
     * Returns the object that provides the method for getting
     * authentication information.  This object must belong to a class
     * that implements the <CODE>LDAPRebind</CODE> interface.
     * @return Object to be used to obtain information for
     * authenticating to other LDAP servers during referrals.
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPRebindAuth
     */
    public LDAPRebind getRebindProc() {
        return m_rebind_proc;
    }

    /**
     * Returns the maximum number of hops to follow during a referral.
     * @return Maximum number of hops to follow during a referral.
     */
    public int getHopLimit() {
        return m_hop_limit;
    }

    /**
     * Returns any client controls to be applied by the client
     * to LDAP operations.
     * @return Client controls to be applied by the client to LDAP operations.
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public LDAPControl[] getClientControls() {
        return m_clientControls;
    }

    /**
     * Returns any server controls to be applied by the server
     * to LDAP operations.
     * @return Server controls to be applied by the server to LDAP operations.
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public LDAPControl[] getServerControls() {
        return m_serverControls;
    }

    /**
     * Sets the maximum number of milliseconds to wait for any operation
     * under these search constraints. If 0, there is no maximum time limit
     * on waiting for the operation results.
     * @param msLimit Maximum number of milliseconds to wait for operation results.
     * (0 by default, which means that there is no maximum time limit.)
     */
    public void setTimeLimit( int msLimit ) {
        if (msLimit != 0)
            timeLimit = Math.max( 1, (msLimit + 500) / 1000 );
    }

    /**
     * Sets a preference indicating how aliases should be dereferenced.
     * @param dereference <CODE>LDAPv2.DEREF_NEVER</CODE> to
     * never follow ("dereference") aliases,
     * <CODE>LDAPv2.DEREF_FINDING</CODE> to dereference when finding
     * the starting point for the search (but not when searching
     * under that starting entry), <CODE>LDAPv2.DEREF_SEARCHING</CODE>
     * to dereference when searching the entries beneath the
     * starting point of the search (but not when finding the starting
     * entry), or <CODE>LDAPv2.DEREF_ALWAYS</CODE> to always
     * dereference aliases.
     */
    public void setDereference( int dereference ) {
        deref = dereference;
    }

    /**
     * Sets the maximum number of search results to be returned; 0 means
     * no limit. (By default, this is set to 1000.)
     * @param maxResults Maximum number of search results to be returned.
     */
    public void setMaxResults( int maxResults ) {
        maxRes = maxResults;
    }

    /**
     * Specifies whether nor not referrals are followed automatically.
     * Returns <CODE>true</CODE> if referrals are to be followed automatically,
     * or <CODE>false</CODE> if referrals throw an <CODE>LDAPReferralException</CODE>.
     * (By default, this is set to <CODE>false</CODE>.)
     * <P>
     * If you set this to <CODE>true</CODE>, you need to define a class that implements
     * the <CODE>LDAPRebind</CODE> interface.  You need to create an object of this class
     * and pass the object to the <CODE>setRebindProc</CODE> method.  This identifies the method
     * for retrieving authentication information, which is used when connecting to other LDAP
     * servers during referrals.
     * @param doReferrals Set to <CODE>true</CODE> if referrals should be followed automatically,
     * or <CODE>False</CODE> if referrals should throw an <CODE>LDAPReferralException</CODE>.
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPRebindAuth
     */
    public void setReferrals( boolean doReferrals ) {
        referrals = doReferrals;
    }

    /**
     * Sets the suggested number of results to return at a time during search.
     * This should be 0 if intermediate results are not needed, and 1 if
     * results are to be processed as they come in. (By default, this is 1.)
     * @param batchSize Number of results to return at a time.
     */
    public void setBatchSize( int batchSize ) {
        batch = batchSize;
    }

    /**
     * Specifies the object that provides the method for getting
     * authentication information.  This object must belong to a class
     * that implements the <CODE>LDAPRebind</CODE> interface.
     * (By default, this is <CODE>null</CODE>.)
     * @param rebind_proc Object to be used to obtain information for
     * authenticating to other LDAP servers during referrals.
     */
    public void setRebindProc( LDAPRebind rebind_proc ) {
        m_rebind_proc = rebind_proc;
    }

    /**
     * Sets maximum number of hops to follow in sequence during a referral.
     * (By default, this is 10.)
     * @param hop_limit Maximum number of hops to follow during a referral.
     */
    public void setHopLimit( int hop_limit ) {
        m_hop_limit = hop_limit;
    }

    /**
     * Sets a client control for LDAP operations.
     * @param control Client control for LDAP operations.
     * @see netscape.ldap.LDAPControl
     */
    public void setClientControls( LDAPControl control ) {
        m_clientControls = new LDAPControl[1];
        m_clientControls[0] = control;
    }

    /**
     * Sets an array of client controls for LDAP operations.
     * @param controls Array of client controls for LDAP operations.
     * @see netscape.ldap.LDAPControl
     */
    public void setClientControls( LDAPControl[] controls ) {
        m_clientControls = controls;
    }

    /**
     * Sets a server control for LDAP operations.
     * @param control Server control for LDAP operations.
     * @see netscape.ldap.LDAPControl
     */
    public void setServerControls( LDAPControl control ) {
        m_serverControls = new LDAPControl[1];
        m_serverControls[0] = control;
    }

    /**
     * Sets an array of server controls for LDAP operations.
     * @param controls An array of server controls for LDAP operations.
     * @see netscape.ldap.LDAPControl
     */
    public void setServerControls( LDAPControl[] controls ) {
        m_serverControls = controls;
    }

    /**
     * Set the maximum number of unread entries any search listener can
     * have before we stop reading from the server.
     * @param backlog The maximum number of unread entries per listener
     */
    public void setMaxBacklog( int backlog ) {
        m_maxBacklog = backlog;
    }

    /**
     * Get the maximum number of unread entries any search listener can
     * have before we stop reading from the server.
     * @return The maximum number of unread entries per listener
     */
    public int getMaxBacklog() {
        return m_maxBacklog;
    }

    /**
     * Makes a copy of an existing set of search constraints.
     * @returns A copy of an existing set of search constraints.
     */
    public Object clone() {
        LDAPSearchConstraints o = new LDAPSearchConstraints();

        o.timeLimit = this.timeLimit;
        o.deref = this.deref;
        o.maxRes = this.maxRes;
        o.referrals = this.referrals;
        o.batch = this.batch;
        o.m_rebind_proc = this.m_rebind_proc;
        o.m_hop_limit = this.m_hop_limit;
        if ( (this.m_clientControls != null) && (this.m_clientControls.length > 0) ) {
            o.m_clientControls = new LDAPControl[this.m_clientControls.length];
            for( int i = 0; i < this.m_clientControls.length; i++ )
                o.m_clientControls[i] = this.m_clientControls[i];
        }
        if ( (this.m_serverControls != null) && (this.m_serverControls.length > 0) ) {
            o.m_serverControls = new LDAPControl[this.m_serverControls.length];
            for( int i = 0; i < this.m_serverControls.length; i++ )
                o.m_serverControls[i] = this.m_serverControls[i];
        }
        return o;
    }
}
