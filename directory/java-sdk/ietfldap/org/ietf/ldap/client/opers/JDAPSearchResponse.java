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
import org.ietf.ldap.*;
import org.ietf.ldap.client.*;
import org.ietf.ldap.ber.stream.*;
import java.io.*;
import java.net.*;

/**
 * This class implements the search response. This object
 * is sent from the ldap server to the interface.
 * <pre>
 * entry [APPLICATION 4] SEQUENCE {
 *   objectName LDAPDN,
 *   attributes SEQUENCE OF SEQUENCE {
 *     AttributeType,
 *     SET OF AttributeValue
 *   }
 * }
 * </pre>
 *
 * Note that the following is the LDAPv3 definition:
 * <pre>
 * SearchResultEntry ::= [APPLICATION 4] SEQUENCE {
 *   objectName LDAPDN,
 *   attributes PartialAttributeList
 * }
 * PartialAttributeList ::= SEQUENCE OF SEQUENCE {
 *   type AttributeDescription,
 *   vals SET OF AttributeValue
 * }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPSearchResponse implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected String m_object_name = null;
    protected BERElement m_element = null;
    protected LDAPAttribute m_attributes[] = null;

    /**
     * Constructs search response.
     * @param element ber element of search response
     */
    public JDAPSearchResponse(BERElement element) throws IOException {
        m_element = element;

        BERTag tag = (BERTag)element;
        BERSequence seq = (BERSequence)tag.getValue();
        BEROctetString name = (BEROctetString)seq.elementAt(0);
        byte buf[] = null;
        buf = name.getValue();
        if (buf == null)
            m_object_name = null;
        else {
            try{
                m_object_name = new String(buf, "UTF8");
            } catch(Throwable x)
            {}
        }
        BERSequence attrs = (BERSequence)seq.elementAt(1);
        if (attrs.size() > 0) {
            m_attributes = new LDAPAttribute[attrs.size()];
            for (int i = 0; i < attrs.size(); i++) {
                m_attributes[i] = new LDAPAttribute(attrs.elementAt(i));
            }
        }
    }

    /**
     * Retrieves ber representation of the result.
     * @return ber representation
     */
    public BERElement getBERElement() {
        return m_element;
    }

    /**
     * Retrieves object name
     * @return object name
     */
    public String getObjectName() {
        return m_object_name;
    }

    /**
     * Retrieves attributes
     * @return attributes
     */
    public LDAPAttribute[] getAttributes() {
        return m_attributes;
    }

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.SEARCH_RESPONSE;
    }

    /**
     * Retrieve the string representation.
     * @return string representation
     */
    public String toString() {
        String s = "";
        if (m_attributes != null) {
            for (int i = 0; i < m_attributes.length; i++) {
                if (i != 0)
                    s = s + ",";
                s = s + m_attributes[i].toString();
            }
        }
        return "SearchResponse {entry='" + m_object_name + "', attributes='" +
          s + "'}";
    }
}
