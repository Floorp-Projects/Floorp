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
        m_nLine = m_nLine;
    }
}


