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

/**
 * Represents a set of operation preferences.
 * You can set these preferences for a particular operation
 * by creating an <CODE>LDAPConstraints</CODE> object,
 * specifying your preferences, and passing the object to
 * the proper <CODE>LDAPConnection</CODE> method.
 * <P>
 *
 * @version 1.0
 */
public class LDAPConstraints implements Cloneable, java.io.Serializable {

    static final long serialVersionUID = 6506767263918312029L;
    private int m_hop_limit;
    private LDAPBind m_bind_proc;
    private LDAPRebind m_rebind_proc;
    private boolean referrals;
    private int m_time_limit;
    private LDAPControl[] m_clientControls;
    private LDAPControl[] m_serverControls;

    /**
     * Constructs an <CODE>LDAPConstraints</CODE> object that specifies
     * the default set of constraints.
     */
    public LDAPConstraints() {
        m_time_limit = 0;
        referrals = false;
        m_bind_proc = null;
        m_rebind_proc = null;
        m_hop_limit = 5;
        m_clientControls = null;
        m_serverControls = null;
    }

    /**
     * Constructs a new <CODE>LDAPConstraints</CODE> object and allows you
     * to specify the constraints in that object.
     * <P>
     * @param msLimit maximum time in milliseconds to wait for results (0
     * by default, which means that there is no maximum time limit)
     * @param doReferrals specify <CODE>true</CODE> to follow referrals
     * automatically, or <CODE>False</CODE> to throw an
     * <CODE>LDAPReferralException</CODE> error if the server sends back
     * a referral (<CODE>False</CODE> by default)
     * @param rebind_proc specifies the object that
     * implements the <CODE>LDAPRebind</CODE> interface (you need to
     * define this class).  The object will be used when the client
     * follows referrals automatically.  The object provides the client
     * with a method for getting the distinguished name and password
     * used to authenticate to another LDAP server during a referral.
     * (This field is <CODE>null</CODE> by default.)
     * @param hop_limit maximum number of referrals to follow in a
     * sequence when attempting to resolve a request
     * @see netscape.ldap.LDAPConnection#setOption(int, java.lang.Object)
     */
    public LDAPConstraints( int msLimit, boolean doReferrals, 
        LDAPRebind rebind_proc, int hop_limit) {
        m_time_limit = msLimit;
        referrals = doReferrals;
        m_bind_proc = null;
        m_rebind_proc = rebind_proc;
        m_hop_limit = hop_limit;
        m_clientControls = null;
        m_serverControls = null;
    }
    
    /**
     * Constructs a new <CODE>LDAPConstraints</CODE> object and allows you
     * to specify the constraints in that object.
     * <P>
     * @param msLimit Mmaximum time in milliseconds to wait for results (0
     * by default, which means that there is no maximum time limit)
     * @param doReferrals specify <CODE>true</CODE> to follow referrals
     * automatically, or <CODE>False</CODE> to throw an
     * <CODE>LDAPReferralException</CODE> error if the server sends back
     * a referral (<CODE>False</CODE> by default)
     * @param bind_proc specifies the object that 
     * implements the <CODE>LDAPBind</CODE> interface (you need to
     * define this class).  The object will be used to authenticate to the
     * server on referrals. (This field is <CODE>null</CODE> by default.)
     * @param hop_limit maximum number of referrals to follow in a
     * sequence when attempting to resolve a request
     * @see netscape.ldap.LDAPConnection#setOption(int, java.lang.Object)
     */
    public LDAPConstraints( int msLimit, boolean doReferrals, 
        LDAPBind bind_proc, int hop_limit) {
        m_time_limit = msLimit;
        referrals = doReferrals;
        m_bind_proc = bind_proc;
        m_rebind_proc = null;
        m_hop_limit = hop_limit;
        m_clientControls = null;
        m_serverControls = null;
    }

    /**
     * Returns the maximum number of milliseconds to wait for any operation
     * under these constraints. If 0, there is no maximum time limit
     * on waiting for the operation results.
     * @return maximum number of milliseconds to wait for operation results.
     */
    public int getTimeLimit() {
        return m_time_limit;
    }


    /**
     * Specifies whether nor not referrals are followed automatically.
     * Returns <CODE>true</CODE> if referrals are to be followed automatically,
     * or <CODE>false</CODE> if referrals throw an 
     * <CODE>LDAPReferralException</CODE>.
     * @return <CODE>true</CODE> if referrals are followed automatically, 
     * <CODE>false</CODE> if referrals throw an 
     * <CODE>LDAPReferralException</CODE>.
     */
    public boolean getReferrals() {
        return referrals;
    }

    /**
     * Returns the object that provides the mechanism for authenticating to the 
     * server on referrals. This object must implement the <CODE>LDAPBind</CODE>
     * interface.
     * @return object to use to authenticate to the server on referrals.
     * @see netscape.ldap.LDAPBind
     */
    public LDAPBind getBindProc() {
        return m_bind_proc;
    }

    /**
     * Returns the object that provides the method for getting
     * authentication information. This object must 
     * implement the <CODE>LDAPRebind</CODE> interface.
     * @return object to use to obtain information for
     * authenticating to other LDAP servers during referrals.
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPRebindAuth
     */
    public LDAPRebind getRebindProc() {
        return m_rebind_proc;
    }

    /**
     * Returns the maximum number of hops to follow during a referral.
     * @return maximum number of hops to follow during a referral.
     */
    public int getHopLimit() {
        return m_hop_limit;
    }

    /**
     * Returns any client controls to be applied by the client
     * to LDAP operations.
     * @return client controls for the client to apply to LDAP operations.
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
     * @return server controls for the server to apply to LDAP operations.
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public LDAPControl[] getServerControls() {
        return m_serverControls;
    }

    /**
     * Sets the maximum number of milliseconds to wait for any operation
     * under these constraints. If 0, there is no maximum time limit
     * on waiting for the operation results. If the time limit is exceeded,
     * an LDAPException with the result code <CODE>LDAPException.TIME_LIMIT
     * </CODE> is thrown.
     * @param msLimit Maximum number of milliseconds to wait for operation
     * results (0 by default, which means that there is no maximum time 
     * limit.)
     * @see netscape.ldap.LDAPException#LDAP_TIMEOUT
     */
    public void setTimeLimit( int msLimit ) {
        m_time_limit = msLimit;
    }


    /**
     * Specifies whether or not referrals are followed automatically.
     * Specify <CODE>true</CODE> if referrals are to be followed automatically,
     * or <CODE>false</CODE> if referrals are to throw an 
     * <CODE>LDAPReferralException</CODE>. 
     * (By default, this is set to <CODE>false</CODE>.)
     * <P>
     * If you set this to <CODE>true</CODE>, you need to create an object of 
     * this class that implements either the <CODE>LDAPRebind</CODE> or 
     * <CODE>LDAPBind</CODE> interface. The <CODE>LDAPRebind</CODE> object 
     * identifies the method for retrieving authentication information which
     * will be used when connecting to other LDAP servers during referrals. 
     * This object should be passed to the <CODE>setRebindProc</CODE> method.
     * Alternatively, the <CODE>LDAPBind</CODE> object identifies an 
     * authentication mechanism to be used instead of the default 
     * authentication mechanism when following referrals. This 
     * object should be passed to the <CODE>setBindProc</CODE> method.
     * @param doReferrals set to <CODE>true</CODE> if referrals should be 
     * followed automatically, or <CODE>False</CODE> if referrals should throw 
     * an <CODE>LDAPReferralException</CODE>
     * @see netscape.ldap.LDAPBind
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPRebindAuth
     */
    public void setReferrals( boolean doReferrals ) {
        referrals = doReferrals;
    }

    /**
     * Sets the object that provides the mechanism for authenticating 
     * to the server on referrals. This object must implement 
     * the <CODE>LDAPBind</CODE> interface.(By default, this is 
     * <CODE>null</CODE>.) This method sets the <CODE>LDAPRebind</CODE>
     * object to null for this constraint.
     * @param bind_proc object to use to authenticate to the server
     * on referrals
     * @see netscape.ldap.LDAPBind
     */
    public void setBindProc( LDAPBind bind_proc ) {
        m_bind_proc = bind_proc;
        if (bind_proc != null) {
            m_rebind_proc = null;
        }
    }

    /**
     * Specifies the object that provides the method for getting
     * authentication information.  This object must belong to a class
     * that implements the <CODE>LDAPRebind</CODE> interface.
     * (By default, this is <CODE>null</CODE>.) This method sets the 
     * <CODE>LDAPBind</CODE> object to null for this constraint. 
     * @param rebind_proc object to use to obtain information for
     * authenticating to other LDAP servers during referrals
     */
    public void setRebindProc( LDAPRebind rebind_proc ) {
        m_rebind_proc = rebind_proc;
        if (rebind_proc != null) {
            m_bind_proc = null;
        }
    }

    /**
     * Sets maximum number of hops to follow in sequence during a referral.
     * (By default, this is 5.)
     * @param hop_limit maximum number of hops to follow during a referral
     */
    public void setHopLimit( int hop_limit ) {
        m_hop_limit = hop_limit;
    }

    /**
     * Sets a client control for LDAP operations.
     * @param control client control for LDAP operations
     * @see netscape.ldap.LDAPControl
     */
    public void setClientControls( LDAPControl control ) {
        m_clientControls = new LDAPControl[1];
        m_clientControls[0] = control;
    }

    /**
     * Sets an array of client controls for LDAP operations.
     * @param controls array of client controls for LDAP operations
     * @see netscape.ldap.LDAPControl
     */
    public void setClientControls( LDAPControl[] controls ) {
        m_clientControls = controls;
    }

    /**
     * Sets a server control for LDAP operations.
     * @param control server control for LDAP operations
     * @see netscape.ldap.LDAPControl
     */
    public void setServerControls( LDAPControl control ) {
        m_serverControls = new LDAPControl[1];
        m_serverControls[0] = control;
    }

    /**
     * Sets an array of server controls for LDAP operations.
     * @param controls an array of server controls for LDAP operations
     * @see netscape.ldap.LDAPControl
     */
    public void setServerControls( LDAPControl[] controls ) {
        m_serverControls = controls;
    }

    /**
     * Return a string representation of the object for debugging
     *
     * @return A string representation of the object
     */
    public String toString() {
        StringBuffer sb = new StringBuffer("LDAPConstraints {");
        sb.append("time limit " + getTimeLimit() + ", ");
        sb.append("referrals " + getReferrals() + ", ");
        sb.append("hop limit " + getHopLimit() + ", ");
        sb.append("bind_proc " + getBindProc() + ", ");
        sb.append("rebind_proc " + getRebindProc());
        LDAPControl[] controls = getClientControls();
        if ( controls != null ) {
            sb.append(", client controls ");
            for (int i =0; i < controls.length; i++) {
                sb.append(controls[i].toString());
                if ( i < (controls.length - 1) ) {
                    sb.append(" ");
                }
            }
        }
        controls = getServerControls();
        if ( controls != null ) {
            sb.append(", server controls ");
            for (int i =0; i < controls.length; i++) {
                sb.append(controls[i].toString());
                if ( i < (controls.length - 1) ) {
                    sb.append(" ");
                }
            }
        }
        sb.append('}');

        return sb.toString();
    }

    /**
     * Makes a copy of an existing set of constraints.
     * @return a copy of an existing set of constraints
     */
    public Object clone() {
        try {         
            LDAPConstraints o = (LDAPConstraints) super.clone();

            if ( (this.m_clientControls != null) &&
                 (this.m_clientControls.length > 0) ) {
                o.m_clientControls = new LDAPControl[this.m_clientControls.length];
                for( int i = 0; i < this.m_clientControls.length; i++ )
                    o.m_clientControls[i] = 
                        (LDAPControl)this.m_clientControls[i].clone();
            }
            if ( (this.m_serverControls != null) && 
                 (this.m_serverControls.length > 0) ) {
                o.m_serverControls = new LDAPControl[this.m_serverControls.length];
                for( int i = 0; i < this.m_serverControls.length; i++ )
                    o.m_serverControls[i] = 
                        (LDAPControl)this.m_serverControls[i].clone();
            }
            return o;
        }
        catch (CloneNotSupportedException ex) {
            // shold never happen, the class is Cloneable
            return null;
        }
    }
}
