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
package org.ietf.ldap.client.opers;

import java.util.*;
import org.ietf.ldap.ber.stream.*;
import java.io.*;
import java.net.*;

/**
 * This is the base interface for all protocol operation
 * that is embedded in a JDAPMessage (LDAP Message in RCF1777).
 * Each protocol operation must implement this interface.
 * <P>See RFC 1777.
 *
 * @version 1.0
 */
public interface JDAPProtocolOp {
    /**
     * Protocol operation types
     */
    public final static int BIND_REQUEST        = 0;
    public final static int BIND_RESPONSE       = 1;
    public final static int UNBIND_REQUEST      = 2;
    public final static int SEARCH_REQUEST      = 3;
    public final static int SEARCH_RESPONSE     = 4;
    public final static int SEARCH_RESULT       = 5;
    public final static int MODIFY_REQUEST      = 6;
    public final static int MODIFY_RESPONSE     = 7;
    public final static int ADD_REQUEST         = 8;
    public final static int ADD_RESPONSE        = 9;
    public final static int DEL_REQUEST         = 10;
    public final static int DEL_RESPONSE        = 11;
    public final static int MODIFY_RDN_REQUEST  = 12;
    public final static int MODIFY_RDN_RESPONSE = 13;
    public final static int COMPARE_REQUEST     = 14;
    public final static int COMPARE_RESPONSE    = 15;
    public final static int ABANDON_REQUEST     = 16;
    public final static int SEARCH_RESULT_REFERENCE = 19;
    public final static int EXTENDED_REQUEST    = 23;
    public final static int EXTENDED_RESPONSE   = 24;

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType();

    /**
     * Retrieves the ber representation of the operation.
     * @return ber representation
     */
    public BERElement getBERElement();

    /**
     * Retrieves the string representation of the operation.
     * @return string representation
     */
    public String toString();
}
