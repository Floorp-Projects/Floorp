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
public class LDAPSearchConstraints extends LDAPConstraints 
                                   implements Cloneable {

    private int deref;
    private int maxRes;
    private int batch;
    private int serverTimeLimit;
    transient private int m_maxBacklog = 100;

    /**
     * Constructs an <CODE>LDAPSearchConstraints</CODE> object that specifies
     * the default set of search constraints.
     */
    public LDAPSearchConstraints() {
        super();
        deref = 0;
        maxRes = 1000;
        batch = 1;
        serverTimeLimit = 0;
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
     * define this class).  The object will be used when the client
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
        super(msLimit, doReferrals, rebind_proc, hop_limit);
        deref = dereference;
        maxRes = maxResults;
        batch = batchSize;
    }

    /**
     * Constructs a new <CODE>LDAPSearchConstraints</CODE> object and allows you
     * to specify the search constraints in that object.
     * <P>
     * @param msLimit Maximum time in milliseconds to wait for results (0
     * by default, which means that there is no maximum time limit)
     * @param timeLimit Maximum time in seconds for the server to spend
     * processing a search request (0 by default for no limit)
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
     * @param rebind_proc Specifies the object that
     * implements the <CODE>LDAPRebind</CODE> interface. 
     * The object will be used when the client
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
    public LDAPSearchConstraints( int msLimit, int timeLimit,
                                  int dereference,
                                  int maxResults, boolean doReferrals,
                                  int batchSize,
                                  LDAPRebind rebind_proc,
                                  int hop_limit) {
        super(msLimit, doReferrals, rebind_proc, hop_limit);
        serverTimeLimit = timeLimit;
        deref = dereference;
        maxRes = maxResults;
        batch = batchSize;
    }

    /**
     * Constructs a new <CODE>LDAPSearchConstraints</CODE> object and allows you
     * to specify the search constraints in that object.
     * <P>
     * @param msLimit Maximum time in milliseconds to wait for results (0
     * by default, which means that there is no maximum time limit)
     * @param timeLimit Maximum time in seconds for the server to spend
     * processing a search request (0 by default for no limit)
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
     * @param bind_proc Specifies the object that
     * implements the <CODE>LDAPBind</CODE> interface (you need to
     * define this class).  The object will be used to authenticate
     * to the server on referrals.
     * (This field is <CODE>null</CODE> by default.)
     * @param hop_limit Maximum number of referrals to follow in a
     * sequence when attempting to resolve a request.
     * @see netscape.ldap.LDAPConnection#setOption(int, java.lang.Object)
     * @see netscape.ldap.LDAPConnection#search(netscape.ldap.LDAPUrl, netscape.ldap.LDAPSearchConstraints)
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     */
    public LDAPSearchConstraints( int msLimit, int timeLimit,
                                  int dereference,
                                  int maxResults, boolean doReferrals,
                                  int batchSize,
                                  LDAPBind bind_proc,
                                  int hop_limit) {
        super(msLimit, doReferrals, bind_proc, hop_limit);
        serverTimeLimit = timeLimit;
        deref = dereference;
        maxRes = maxResults;
        batch = batchSize;
    }

    /**
     * Returns the maximum number of seconds to wait for the server to
     * spend on a search operation.If 0, there is no time limit.
     * @return Maximum number of seconds for the server to spend.
     */
    public int getServerTimeLimit() {
        return serverTimeLimit;
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
     * Returns the suggested number of results to return at a time during
     * search. This should be 0 if intermediate results are not needed, and
     * 1 if results are to be processed as they come in.
     * @return Number of results to return at a time.
     */
    public int getBatchSize() {
        return batch;
    }

    /**
     * Sets the maximum number of seconds for the server to spend
     * returning search results. If 0, there is no time limit.
     * @param limit Maximum number of seconds for the server to spend.
     * (0 by default, which means that there is no maximum time limit.)
     */
    public void setServerTimeLimit( int limit ) {
        serverTimeLimit = limit;
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
     * Sets the suggested number of results to return at a time during search.
     * This should be 0 if intermediate results are not needed, and 1 if
     * results are to be processed as they come in. (By default, this is 1.)
     * @param batchSize Number of results to return at a time.
     */
    public void setBatchSize( int batchSize ) {
        batch = batchSize;
    }

    /**
     * Set the maximum number of unread entries any search listener can
     * have before we stop reading from the server.
     * @param backlog The maximum number of unread entries per listener
     * @deprecated Use <CODE>LDAPConnection.getOption()</CODE>
     */
    public void setMaxBacklog( int backlog ) {
        m_maxBacklog = backlog;
    }

    /**
     * Get the maximum number of unread entries any search listener can
     * have before we stop reading from the server.
     * @return The maximum number of unread entries per listener
     * @deprecated Use <CODE>LDAPConnection.setOption()</CODE>
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

        o.serverTimeLimit = this.serverTimeLimit;
        o.deref = this.deref;
        o.maxRes = this.maxRes;
        o.batch = this.batch;
        
        o.setHopLimit(this.getTimeLimit());
        o.setReferrals(this.getReferrals());
        o.setTimeLimit(this.getTimeLimit());
        
        if (this.getBindProc() != null) {
            o.setBindProc(this.getBindProc());
        } else {
            o.setRebindProc(this.getRebindProc());
        }
        
        LDAPControl[] tClientControls = this.getClientControls();
        LDAPControl[] oClientControls;

        if ( (tClientControls != null) &&
             (tClientControls.length > 0) ) {
            oClientControls = new LDAPControl[tClientControls.length]; 
            for( int i = 0; i < tClientControls.length; i++ )
                oClientControls[i] = (LDAPControl)tClientControls[i].clone();
            o.setClientControls(oClientControls);
        }

        LDAPControl[] tServerControls = this.getServerControls();
        LDAPControl[] oServerControls;

        if ( (tServerControls != null) && 
             (tServerControls.length > 0) ) {
            oServerControls = new LDAPControl[tServerControls.length];
            for( int i = 0; i < tServerControls.length; i++ )
                oServerControls[i] = (LDAPControl)tServerControls[i].clone();
            o.setServerControls(oServerControls);
        }

        return o;
    }
}
