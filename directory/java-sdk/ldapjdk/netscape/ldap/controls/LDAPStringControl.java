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

/**
 * Represents an LDAP v3 server control that contains a string as its
 * only value. This is to be used as a base class by real such controls.
 */
abstract class LDAPStringControl extends LDAPControl {
    protected String m_msg = null;

    LDAPStringControl() {
      super();
    }

    /**
     * Constructs a <CODE>LDAPStringControl</CODE> object, and stores the 
     * value as a string. To retrieve this string value, use 
     * <CODE>getMsg()</CODE>.
     * @param oid The oid of defining this control.
     * @param critical True if this control is critical.
     * @param value The value associated with this control.
     * @see netscape.ldap.LDAPcontrol
     */
    public LDAPStringControl( String oid, boolean critical, byte[] value ) {
        super( oid, critical, value );
	
	if ( value != null ) {
	    try {  
	        m_msg = new String( value, "UTF8" );
	    } catch ( java.io.IOException e ) {
	    }
	}
    }

    /**
     * Parses a response control sent by the server and
     * retrieves a string.
     * <P>
     *
     * You can get the controls returned by the server by using the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * <P>
     *
     * @param controls An array of <CODE>LDAPControl</CODE> objects,
     * representing the controls returned by the server
     * after a search.  To get these controls, use the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * @param type The OID of the control to look for.
     * @return A message string, or null if the server did
     * not return a string.
     * @see netscape.ldap.LDAPConnection#getResponseControls
     */
    public static String parseResponse( LDAPControl[] controls, String type ) {
        String msg = null;
        LDAPControl cont = null;
        /* See if there is a password control in the array */
        for( int i = 0; (controls != null) && (i < controls.length); i++ ) {
            if ( controls[i].getID().equals( type ) ) {
                cont = controls[i];
                break;
            }
        }
        if ( cont != null ) {
            /* Suck out the data and return it */
            try {
                msg = new String( cont.getValue(), "UTF8" );
            } catch ( Exception e ) {
            }
        }
        return msg;
    }
}
