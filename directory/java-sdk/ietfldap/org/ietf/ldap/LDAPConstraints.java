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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.io.Serializable;
import java.util.Hashtable;

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
public class LDAPConstraints implements Cloneable, Serializable {

    static final long serialVersionUID = 6506767263918312029L;
    private int _hop_limit = 5;
    private LDAPReferralHandler _referralHandler = null;
    private boolean referrals = false;
    private int _time_limit = 0;
    private LDAPControl[] _serverControls = null;
    private Hashtable _properties = null;

    /**
     * Constructs an <CODE>LDAPConstraints</CODE> object that specifies
     * the default set of constraints.
     */
    public LDAPConstraints() {
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
     * @param handler specifies the object that
     * implements the <CODE>LDAPReferralHandler</CODE> interface (you need to
     * define this class). The object will be used when the client
     * follows referrals automatically. See org.ietf.ldap.LDAPAuthHandler
     * and org.ietf.ldap.LDAPReferralHandler.
     * (This field is <CODE>null</CODE> by default.)
     * @param hop_limit maximum number of referrals to follow in a
     * sequence when attempting to resolve a request
     * @see org.ietf.ldap.LDAPConnection#setOption(int, java.lang.Object)
     */
    public LDAPConstraints( int msLimit, boolean doReferrals, 
                            LDAPReferralHandler handler,
                            int hop_limit ) {
        _time_limit = msLimit;
        referrals = doReferrals;
        _referralHandler = handler;
        _hop_limit = hop_limit;
        _serverControls = null;
    }
    
    /**
     * Returns the maximum number of milliseconds to wait for any operation
     * under these constraints. If 0, there is no maximum time limit
     * on waiting for the operation results.
     * @return maximum number of milliseconds to wait for operation results.
     */
    public int getTimeLimit() {
        return _time_limit;
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
    public boolean getReferralFollowing() {
        return referrals;
    }

    /**
     * Returns the object that provides the mechanism for authenticating to the 
     * server on referrals. This object must implement the
     * <CODE>LDAPReferralHandler</CODE> interface.
     * @return object to use to authenticate to the server on referrals
     * @see org.ietf.ldap.LDAPReferralHandler
     */
    public LDAPReferralHandler getReferralHandler() {
        return _referralHandler;
    }

    /**
     * Returns the maximum number of hops to follow during a referral.
     * @return maximum number of hops to follow during a referral.
     */
    public int getHopLimit() {
        return _hop_limit;
    }

    /**
     * Returns any server controls to be applied by the server
     * to LDAP operations.
     * @return server controls for the server to apply to LDAP operations.
     * @see org.ietf.ldap.LDAPControl
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public LDAPControl[] getControls() {
        return _serverControls;
    }

    /**
     * Gets a property of a constraints object which has been assigned with 
     * setProperty. Null is returned if the property is not defined.
     *
     * @param name Name of the property to retrieve
     */
    public Object getProperty( String name ) {
        if ( _properties == null ) {
            return null;
        }
        return _properties.get( name );
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
     * @see org.ietf.ldap.LDAPException#LDAP_TIMEOUT
     */
    public void setTimeLimit( int msLimit ) {
        _time_limit = msLimit;
    }


    /**
     * Specifies whether or not referrals are followed automatically.
     * Specify <CODE>true</CODE> if referrals are to be followed automatically,
     * or <CODE>false</CODE> if referrals are to throw an 
     * <CODE>LDAPReferralException</CODE>. 
     * (By default, this is set to <CODE>false</CODE>.)
     * <P>
     * If you set this to <CODE>true</CODE>, you need to create an object of 
     * this class that implements either the <CODE>LDAPAuthHandler</CODE> or 
     * <CODE>LDAPBind</CODE> interface. The <CODE>LDAPAuthProvider</CODE> object 
     * identifies the method for retrieving authentication information which
     * will be used when connecting to other LDAP servers during referrals. 
     * This object should be passed to the <CODE>setReferralHandler</CODE> method.
     * Alternatively, the <CODE>LDAPBind</CODE> object identifies an 
     * authentication mechanism to be used instead of the default 
     * authentication mechanism when following referrals. This 
     * object should be passed to the <CODE>setBindHandler</CODE> method.
     * @param doReferrals set to <CODE>true</CODE> if referrals should be 
     * followed automatically, or <CODE>False</CODE> if referrals should throw 
     * an <CODE>LDAPReferralException</CODE>
     * @see org.ietf.ldap.LDAPBindHandler
     * @see org.ietf.ldap.LDAPAuthHandler
     * @see org.ietf.ldap.LDAPAuthProvider
     */
    public void setReferralFollowing( boolean doReferrals ) {
        referrals = doReferrals;
    }

    /**
     * Specifies the object that provides the method for getting
     * authentication information.  This object must belong to a class
     * that implements the <CODE>LDAPReferralHandler</CODE> interface.
     * (By default, this is <CODE>null</CODE>.) This method sets the 
     * <CODE>LDAPReferralHandler</CODE> object to null for this constraint. 
     * @param handler object to use to obtain information for
     * authenticating to other LDAP servers during referrals
     */
    public void setReferralHandler( LDAPReferralHandler handler ) {
        _referralHandler = handler;
    }

    /**
     * Sets maximum number of hops to follow in sequence during a referral.
     * (By default, this is 5.)
     * @param hop_limit maximum number of hops to follow during a referral
     */
    public void setHopLimit( int hop_limit ) {
        _hop_limit = hop_limit;
    }

    /**
     * Sets a server control for LDAP operations.
     * @param control server control for LDAP operations
     * @see org.ietf.ldap.LDAPControl
     */
    public void setControls( LDAPControl control ) {
        _serverControls = new LDAPControl[1];
        _serverControls[0] = control;
    }

    /**
     * Sets an array of server controls for LDAP operations.
     * @param controls an array of server controls for LDAP operations
     * @see org.ietf.ldap.LDAPControl
     */
    public void setControls( LDAPControl[] controls ) {
        _serverControls = controls;
    }

    /**
     * Sets a property of the constraints object. 
     * No property names have been defined at this time, but the mechanism 
     * is in place in order to support revisional as well as dynamic and 
     * proprietary extensions to operation modifiers.
     *
     * @param name Name of the property to set
     * @param value Value to assign to the property
     */
    public void setProperty( String name, Object value ) throws LDAPException {
        if ( _properties == null ) {
            _properties = new Hashtable();
        }
        _properties.put( name, value );
    }

    /**
     * Return a string representation of the object for debugging
     *
     * @return A string representation of the object
     */
    public String toString() {
        StringBuffer sb = new StringBuffer("LDAPConstraints {");
        sb.append("time limit " + getTimeLimit() + ", ");
        sb.append("referrals " + getReferralFollowing() + ", ");
        sb.append("hop limit " + getHopLimit() + ", ");
        sb.append("referral handler " + getReferralHandler() + ", ");
        LDAPControl[] controls = getControls();
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
        LDAPConstraints o = new LDAPConstraints();

        o._time_limit = this._time_limit;
        o.referrals = this.referrals;
        o._referralHandler = this._referralHandler;
        o._hop_limit = this._hop_limit;
        if ( (this._serverControls != null) && 
             (this._serverControls.length > 0) ) {
            o._serverControls = new LDAPControl[this._serverControls.length];
            for( int i = 0; i < this._serverControls.length; i++ )
                o._serverControls[i] = 
                    (LDAPControl)this._serverControls[i].clone();
        }
        return o;
    }
}
