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
package netscape.ldap.client.opers;

import java.util.*;
import netscape.ldap.client.*;
import netscape.ldap.ber.stream.*;
import java.io.*;
import java.net.*;

/**
 * This is the base class for all the request that
 * has a base dn component. The existence of this
 * class is due to the JDAPReferralThread.
 *
 * @version 1.0
 */
public abstract class JDAPBaseDNRequest {

    /**
     * Sets the base dn component in the request.
     * @param basedn base DN
     */
    public abstract void setBaseDN(String basedn);

    /**
     * Gets the base dn component in the request.
     * @return base dn
     */
    public abstract String getBaseDN();
}
