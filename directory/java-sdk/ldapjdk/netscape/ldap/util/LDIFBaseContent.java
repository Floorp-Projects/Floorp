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
package netscape.ldap.util;

import netscape.ldap.LDAPControl;

/**
 *
 * An object of this class represents the content of an LDIF record.
 * This class implements the <CODE>LDIFContent</CODE> interface, but
 * it is abstract and must be extended for the various record types.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.util.LDIFRecord#getContent
 */
public abstract class LDIFBaseContent
                      implements LDIFContent, java.io.Serializable {

    static final long serialVersionUID = -8542611537447295949L;

    /**
     * Internal variables
     */
    private LDAPControl[] m_controls = null;

    /**
     * Blank constructor for deserialization
     */
    public LDIFBaseContent() {
    }

    /**
     * Retrieves the list of controls specified in the content
     * of the LDIF record, if any
     * @return An array of <CODE>LDAPControl</CODE> objects that
     * represent any controls specified in the the LDIF record,
     * or <CODE>null</CODE> if none were specified.
     */
    public LDAPControl[] getControls() {
        return m_controls;
    }

    /**
     * Sets the list of controls
     * @param controls An array of <CODE>LDAPControl</CODE> objects
     * or <CODE>null</CODE> if none are to be specified.
     */
    public void setControls( LDAPControl[] controls ) {
        m_controls = controls;
    }

    /**
     * Get the OIDs of all controls, if any, as a string
     *
     * @return The OIDs of all controls, if any, as a string,
     * or an empty string if there are no controls
     */
    protected String getControlString() {
        String s = "";
        if ( getControls() != null ) {
            s += ' ';
            LDAPControl[] controls = getControls();
            int len = controls.length;
            for( int i = 0; i < len; i++ ) {
                s += controls[i].toString();
                if ( i < (len-1) ) {
                    s += ' ';
                }
            }
        }
        return s;
    }
}
