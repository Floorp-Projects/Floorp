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
import org.ietf.ldap.client.*;
import org.ietf.ldap.ber.stream.*;
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
