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
package org.ietf.ldap.client;

import java.util.*;
import java.io.*;
import org.ietf.ldap.ber.stream.*;

/**
 * This class implements the attribute value assertion.
 * This object is used with filters.
 * <pre>
 * AttributeValueAssertion ::= SEQUENCE {
 *   attributType AttributeType,
 *   attributValue AttributeValue
 * }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPAVA {
    /**
     * Internal variables
     */
    protected String m_type = null;
    protected String m_val = null;

    /**
     * Constructs the attribute value assertion.
     * @param type attribute type
     * @param val attribute value
     */
    public JDAPAVA(String type, String val) {
        m_type = type;
        m_val = val;
    }

    /**
     * Retrieves the AVA type.
     * @return AVA type
     */
    public String getType() {
        return m_type;
    }

    /**
     * Retrieves the AVA value.
     * @return AVA value
     */
    public String getValue() {
        return m_val;
    }

    /**
     * Retrieves the ber representation.
     * @return ber representation
     */
    public BERElement getBERElement() {
        BERSequence seq = new BERSequence();
        seq.addElement(new BEROctetString(m_type));

        seq.addElement(JDAPFilterOpers.getOctetString(m_val));

        return seq;
    }

    /**
     * Retrieves the string representation parameters.
     * @return string representation parameters
     */
    public String getParamString() {
        return "{type=" + m_type + ", value=" + m_val + "}";
    }

    /**
     * Retrieves the string representation.
     * @return string representation
     */
    public String toString() {
        return "JDAPAVA " + getParamString();
    }
}
