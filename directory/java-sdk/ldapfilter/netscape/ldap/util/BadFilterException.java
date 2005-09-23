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
package netscape.ldap.util;

/**
 * The exception thrown when there is a problem with either an LDAPFilter
 * or with the File/URL/Buffer form which we're creating the LDAPFilter.
 *
 * @see LDAPFilter
 * @see LDAPFilterDescriptor
 * @version 1.0
 */
public class BadFilterException extends Exception {

    private String m_strException;
    private int m_nLine = -1;

    /**
     * Creates an <b>Unknown</b> BadFilterException
     */
    public BadFilterException () {
        m_strException = "Unknown Error";
    }

    /**
     * Creates a BadFilterException with the
     * given string
     */
    public BadFilterException ( String s ) {
        m_strException = s;
    }

    /**
     * Creates a BadFilterException with the
     * given string and line number
     */
    public BadFilterException ( String s, int nErrorLineNumber ) {
        m_strException = s;
        m_nLine = nErrorLineNumber;
    }

    /**
     * Returns the exception string.
     */
    public String toString() {
        return m_strException;
    }


    /**
     * If appropriate, return the line number of the ldapfilter.conf
     * file (or url or buffer) where this error occurred.  This method
     * will return -1 if the line number was not set.
     */
    public int getErrorLineNumber() {
        return m_nLine;
    }

    /**
     * Set the line number in the ldapfilter.conf file/url/buffer where
     * this error occurred.
     */
    void setErrorLineNumber ( int nErrorLineNumber ) {
        m_nLine = nErrorLineNumber;
    }
}


