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
package netscape.ldap.controls;

import netscape.ldap.LDAPControl;
import netscape.ldap.LDAPException;

/**
 * Represents an LDAP v3 server control that may be returned if a
 * password is about to expire, and password policy is enabled on the server.
 * The OID for this control is 2.16.840.1.113730.3.4.5.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPControl
 */
public class LDAPPasswordExpiringControl extends LDAPStringControl {
    public final static String EXPIRING = "2.16.840.1.113730.3.4.5";

    /**
     * Contructs an <CODE>LDAPPasswordExpiringControl</CODE> object.
     * This constructor is used by <CODE>LDAPControl.register</CODE> to 
     * instantiate password expiring controls.
     * <P>
     * To retrieve the number of seconds until this password expires,
     * call <CODE>getSecondsToExpiration</CODE>.
     * @param oid This parameter must be 
     * <CODE>LDAPPasswordExpiringControl.EXPIRING</CODE>
     * or an <CODE>LDAPException</CODE> is thrown.
     * @param critical True if this control is critical.
     * @param value The value associated with this control.
     * @exception netscape.ldap.LDAPException If oid is not 
     * <CODE>LDAPPasswordExpiringControl.EXPIRING.</CODE>
     * @see netscape.ldap.LDAPControl#register
     */
    public LDAPPasswordExpiringControl( String oid, boolean critical, 
                                       byte[] value ) throws LDAPException {
        super( EXPIRING, critical, value );
	if ( !oid.equals( EXPIRING )) {
	    throw new LDAPException( "oid must be LDAPPasswordExpiringControl" +
				     ".EXPIRING", LDAPException.PARAM_ERROR );
	}
    }

    /**
     * Gets the number of seconds until the password expires returned by the 
     * server.
     * @return int The number of seconds until the password expires.
     * @exception java.lang.NumberFormatException If the server returned an
     * undecipherable message. In this case, use <CODE>getMessage</CODE> to 
     * retrieve the message as a string.
     */
    public int getSecondsToExpiration() {
        return Integer.parseInt( m_msg );
    }

    /**
     * Gets the value associated with this control parsed as a string.
     * @return The value associated with this control parsed as a string.
     */
    public String getMessage() {
        return m_msg;
    }

    /**
     * @param controls An array of <CODE>LDAPControl</CODE> objects,
     * representing the controls returned by the server
     * after a search.  To get these controls, use the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * @return An error message string, or null if none is in the control.
     * @see netscape.ldap.LDAPConnection#getResponseControls
     * @deprecated LDAPPasswordExpiringControl controls are now automatically
     * instantiated.
     */
    public static String parseResponse( LDAPControl[] controls ) {
        return LDAPStringControl.parseResponse( controls, EXPIRING );
    }
}
