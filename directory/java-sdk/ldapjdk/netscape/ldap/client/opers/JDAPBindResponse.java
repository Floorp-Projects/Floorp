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
 * This class implements the bind response. This object
 * is sent from the ldap server to the interface.
 * <pre>
 * BindResponse = [APPLICATION 1] LDAPResult
 * </pre>
 *
 * Note that LDAPv3.0 Bind Response is structured as follows:
 * <pre>
 * BindResponse ::= [APPLICATION 1] SEQUENCE {
 *   COMPONENTS OF LDAPResult,
 *   serverCreds [7] SaslCredentials OPTIONAL
 * }
 * </pre>
 *
 */
public class JDAPBindResponse extends JDAPResult implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected String m_credentials = null;

    /**
     * Constructs bind response.
     * @param element ber element of bind response
     */
    public JDAPBindResponse(BERElement element) throws IOException {
        super(((BERTag)element).getValue());
        /* LDAPv3 Sasl Credentials support */
        BERSequence s = (BERSequence)((BERTag)element).getValue();
        if (s.size() <= 3)
            return;
        BERElement e = s.elementAt(3);
        if (e.getType() == BERElement.TAG) {
			BERElement el = ((BERTag)e).getValue();
			if (el instanceof BERSequence)
			{
				el = ((BERSequence)el).elementAt(0);
			}
            BEROctetString str = (BEROctetString)el;
            try{
                m_credentials = new String(str.getValue(),"UTF8");
            } catch(Throwable x)
            {}
        }
    }

    /**
     * Retrieves Sasl Credentials. LDAPv3 support.
     * @return credentials
     */
    public String getCredentials() {
        return m_credentials;
    }

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.BIND_RESPONSE;
    }

    /**
     * Retrieve the string representation.
     * @return string representation
     */
    public String toString() {
        return "BindResponse " + super.getParamString();
    }
}
