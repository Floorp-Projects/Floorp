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
 * Copyright (C) 2002 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package netscape.ldap;

import java.util.*;
import java.io.*;
import java.net.*;

/**
 * A socket factory interface for supporting the start TLS LDAPv3
 * extension (RFC 2830).
 * <P>
 *
 * @version 1.0
 * @since LDAPJDK 4.17
 */
public interface LDAPTLSSocketFactory extends LDAPSocketFactory {
    /**
     * Creates an SSL socket layered over an existing socket.
     * 
     * Used for the start TLS operations (RFC2830).
     *
     * @param s An existing non-SSL socket
     * @return A SSL socket layered over the input socket
     * @exception LDAPException on error creating socket
     * @see LDAPConnection#startTLS
     */
    public Socket makeSocket(Socket s) throws LDAPException;
}
