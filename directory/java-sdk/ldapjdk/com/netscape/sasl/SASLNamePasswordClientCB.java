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

/**
 * Represents an object which provides more information for authentication can
 * be called by a Mechanism driver. Typically the object provides an ID and
 * a password.
 */
public interface SASLNamePasswordClientCB extends SASLClientCB {
    /**
     * The UI may or may not be popped up and the user is allowed to enter
     * the information. It returns true unless the operation was cancelled.
     * @param defaultID A default which may be used in selecting credentials.
     * @param serverFQDN The fully qualified domain name of the host to which 
     *     authentication is being attempted. Used with kerberos.
     * @param protocol "IMAP", "POP", etc. Used with kerberos.
     * @param prompt Textual information to be provided to the client for
     *     obtaining an ID and password. It may be localized.
     * @return true if the operation is successful, otherwise, false.
     */
    public boolean promptNamePassword(String defaultID, String serverFQDN,
      String protocol, String prompt);

    /**
     * Retrieve the ID.
     * @return ID
     */
    public String getID();

    /**
     * Retrieve the password.
     * @return password
     */
    public String getPassword();
}

