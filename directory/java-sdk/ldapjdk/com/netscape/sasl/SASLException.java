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
package com.netscape.sasl;

/*
 * Exception type returned on SASL authentication failures.
 */
public class SASLException extends Exception {
    /**
     * Constructs a default exception with no specific error information.
     */
    public SASLException() {
    }

    /**
     * Constructs a default exception with a specified string as
     * additional information.
     * @param message The additional error information.
     */
    public SASLException( String message ) {
        super( message );
    }

    /**
     * Constructs a default exception with a specified string as
     * additional information, and a result code.
     * @param message The additional error information.
     * @param resultCode The result code returned.
     */
    public SASLException( String message, int resultCode ) {
        super( message );
        this.m_resultCode = resultCode;
    }

    public int getResultCode() {
        return m_resultCode;
    }

    public String toString() {
        if (m_resultCode != -1)
            return super.toString() + " (" + m_resultCode + ")" ;
        else
            return super.toString();
    }

    private int m_resultCode = -1;
}
