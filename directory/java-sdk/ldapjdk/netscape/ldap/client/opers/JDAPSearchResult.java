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
 * This class implements the search result. This object
 * is sent from the ldap server to the interface. Note
 * that search response is separated into search response
 * and search result. The search response contains the
 * result from the server, while the research result indicates
 * the end of the search response sequence.
 * <pre>
 * resultCode [APPLICATION 5] LDAPResult
 * </pre>
 *
 * @version 1.0
 */
public class JDAPSearchResult extends JDAPResult implements JDAPProtocolOp {
    /**
     * Constructs search result.
     * @param element ber element of search result
     */
    public JDAPSearchResult(BERElement element) throws IOException {
        super(((BERTag)element).getValue());
    }

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.SEARCH_RESULT;
    }

    /**
     * Retrieve the string representation.
     * @return string representation
     */
    public String toString() {
        return "SearchResult " + super.getParamString();
    }
}
