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
 * An exception thrown when the LDAP operation being invoked has
 * been interrupted. For example, an application might interrupt a thread that
 * is performing a search.
 *
 * @version 1.0
 */
public class LDAPInterruptedException extends LDAPException {

    /**
     * Constructs a default exception with a specified string of
     * additional information. This string appears if you call
     * the <CODE>toString()</CODE> method.
     * <P>
     *
     * @param message The additional information.
     * @see netscape.ldap.LDAPInterruptedException#toString()
     */
    LDAPInterruptedException( String message ) {
        super( message, LDAPException.OTHER, null);
    }

    /**
     * Gets the string representation of the exception.
     */
    public String toString() {
        String str = "netscape.ldap.LDAPInterruptedException: ";
        String msg = super.getMessage();
        if (msg != null) {
            str +=msg;
        }            
        return str;
    }
}