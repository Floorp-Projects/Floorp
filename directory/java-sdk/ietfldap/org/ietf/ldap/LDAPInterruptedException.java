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
package org.ietf.ldap;

/**
 * An exception thrown when the LDAP operation being invoked has
 * been interrupted. For example, an application might interrupt a thread that
 * is performing a search.
 *
 * @version 1.0
 */
public class LDAPInterruptedException extends LDAPException {

    static final long serialVersionUID = 5267455101797397456L;

    /**
     * Constructs a default exception with a specified string of
     * additional information. This string appears if you call
     * the <CODE>toString()</CODE> method.
     * <P>
     *
     * @param message the additional information
     * @see org.ietf.ldap.LDAPInterruptedException#toString()
     */
    LDAPInterruptedException( String message ) {
        super( message, LDAPException.OTHER, (Throwable)null);
    }

    /**
     * Gets the string representation of the exception.
     */
    public String toString() {
        String str = "org.ietf.ldap.LDAPInterruptedException: ";
        String msg = super.getMessage();
        if (msg != null) {
            str +=msg;
        }            
        return str;
    }
}
