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
 * password has expired, and password policy is enabled on the server.
 * The OID for this control is 2.16.840.1.113730.3.4.4.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPControl
 */
public class LDAPPasswordExpiredControl extends LDAPStringControl {
    public final static String EXPIRED = "2.16.840.1.113730.3.4.4";

    /**
     * Contructs an <CODE>LDAPPasswordExpiredControl</CODE> object. 
     * This constructor is used by <CODE>LDAPControl.register</CODE> to 
     * instantiate password expired controls.
     * <P>
     * To retrieve the message from the server, call <CODE>getMessage</CODE>.
     * @param oid This parameter must be equal to 
     * <CODE>LDAPPasswordExpiredControl.EXPIRED</CODE>
     * or a <CODE>LDAPException</CODE> is thrown.
     * @param critical True if this control is critical.
     * @param value The value associated with this control.
     * @exception netscape.ldap.LDAPException If oid is not 
     * <CODE>LDAPPasswordExpiredControl.EXPIRED</CODE>.
     * @see netscape.ldap.LDAPControl#register
     */
    public LDAPPasswordExpiredControl( String oid, boolean critical, 
                                       byte[] value ) throws LDAPException {
        super( EXPIRED, critical, value );
	if ( !oid.equals( EXPIRED )) {
	    throw new LDAPException( "oid must be LDAPPasswordExpiredControl." +
				     "PWEXPIRED", LDAPException.PARAM_ERROR);
	}

    }

    /**
     * @param controls An array of <CODE>LDAPControl</CODE> objects,
     * representing the controls returned by the server
     * after a search.  To get these controls, use the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * @return An error message string, or null if none is in the control.
     * @see netscape.ldap.LDAPConnection#getResponseControls
     * @deprecated LDAPPasswordExpiredControl controls are now automatically
     * instantiated.
     */
    public static String parseResponse( LDAPControl[] controls ) {
        return LDAPStringControl.parseResponse( controls, EXPIRED );
    }
 
    /**
     * Gets the message returned by the server with this control
     * @return The message returned by the server
     */    
    public String getMessage() {
        return m_msg;
    }
}




