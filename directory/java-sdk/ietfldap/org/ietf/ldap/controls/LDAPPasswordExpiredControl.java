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
package org.ietf.ldap.controls;

import org.ietf.ldap.LDAPControl;
import org.ietf.ldap.LDAPException;

/**
 * Represents an LDAP v3 server control that may be returned if a
 * password has expired, and password policy is enabled on the server.
 * The OID for this control is 2.16.840.1.113730.3.4.4.
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPControl
 */
public class LDAPPasswordExpiredControl extends LDAPStringControl {
    public final static String EXPIRED = "2.16.840.1.113730.3.4.4";

    /**
     * Contructs an <CODE>LDAPPasswordExpiredControl</CODE> object. 
     * This constructor is used by <CODE>LDAPControl.register</CODE> to 
     * instantiate password expired controls.
     * <P>
     * To retrieve the message from the server, call <CODE>getMessage</CODE>.
     * @param oid this parameter must be equal to 
     * <CODE>LDAPPasswordExpiredControl.EXPIRED</CODE>
     * or an <CODE>LDAPException</CODE> is thrown
     * @param critical <code>true</code> if this control is critical
     * @param value the value associated with this control
     * @exception org.ietf.ldap.LDAPException If oid is not 
     * <CODE>LDAPPasswordExpiredControl.EXPIRED</CODE>.
     * @see org.ietf.ldap.LDAPControl#register
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
     * @param controls an array of <CODE>LDAPControl</CODE> objects,
     * representing the controls returned by the server
     * after a search. To get these controls, use the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * @return an error message string, or null if none is in the control.
     * @see org.ietf.ldap.LDAPConnection#getResponseControls
     * @deprecated LDAPPasswordExpiredControl controls are now automatically
     * instantiated.
     */
    public static String parseResponse( LDAPControl[] controls ) {
        return LDAPStringControl.parseResponse( controls, EXPIRED );
    }
 
    /**
     * Gets the message returned by the server with this control.
     * @return the message returned by the server.
     */    
    public String getMessage() {
        return m_msg;
    }
    
    public String toString() {
         StringBuffer sb = new StringBuffer("{PasswordExpiredCtrl:");
        
        sb.append(" isCritical=");
        sb.append(isCritical());
        
        sb.append(" msg=");
        sb.append(m_msg);
        
        sb.append("}");

        return sb.toString();
    }

}




