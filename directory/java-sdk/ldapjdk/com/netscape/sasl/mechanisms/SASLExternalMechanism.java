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

package com.netscape.sasl.mechanisms;

import com.netscape.sasl.*;

/**
 * This class provides the implementation of the EXTERNAL mechanism driver.
 * This mechanism is passed in the SASL External bind request to retrieve the
 * current result code from the server.
 */
public class SASLExternalMechanism implements SASLClientMechanismDriver {

    /**
     * Default constructor
     */
    public SASLExternalMechanism() {
        m_mechanismName = MECHANISM_NAME;
    }

    /**
     * This method prepares a byte array to use for the initial request to 
     * authenticate.
     * @param id Protocol-dependent identification, for this class, it is ignored.
     * @param protocol A protocol supported by the mechanism driver, e.g.
     *        "pop3", "ldap"
     * @param serverName For this class, it is ignored.
     * @param props Additional configuration for the session. For this class, 
     *        it is ignored.
     * @param authCB An optional object which can be invoked by the mechanism
     *        driver to acquire additional authentication information, such
     *        as user name and password. For this class, it is ignored.
     * @return An byte array. For the case of the SASL External bind, it is
     *         always null. 
     * @exception SASLException Never thrown by this class.
     */
    public byte[] startAuthentication(String id, String protocol,
      String serverName, java.util.Properties props, SASLClientCB authCB)
      throws SASLException {
        return null;
    }

    /**
     * Returns the name of mechanism driver.
     * @return The mechanism name.
     */
    public String getMechanismName() {
        return m_mechanismName;
    }

    /**
     * The protocol driver prepares an appropriate next request to submit
     * to the server based on the challenge received from the server.
     * @param challenge For this class, it is ignored.
     * @return Request to submit to server. For this class, it is always null.
     * @exception SASLException Never thrown by this class.
     */
    public byte[] evaluateResponse(byte[] challenge) throws SASLException {
        return null;
    }

    /**
     * The method may be called at any time to determine if the authentication
     * process is finished.
     * @return <CODE>true</CODE> if authentication is complete. For this class,
     * always returns <CODE>true</CODE>.
     */
    public boolean isComplete() {
        return true;
    }

    /**
     * The protocol driver calls the method to obtain an object capable of
     * encoding/decoding data content for the rest of the session (or until
     * there is a new round of authentication). An exception is thrown if
     * authentication is not yet complete.
     * @return A SASLSecurityLayer object. For this class, it is always null.
     * @exception SASLException Never thrown by this class.
     */
    public SASLSecurityLayer getSecurityLayer() throws SASLException {
        return null;
    }

    private final static String LDAP_PROTOCOL = "LDAP";
    private final static String MECHANISM_NAME = "EXTERNAL";
    private String m_packageName;
    private String m_mechanismName;
}
