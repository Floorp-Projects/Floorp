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
import org.ietf.ldap.ber.stream.*;
import java.io.*;

/**
 * This class implements the attribute value assertion filter.
 *
 * @version 1.0
 */
public abstract class JDAPFilterAVA extends JDAPFilter {
    /**
     * Internal variables
     */
    private int m_tag;
    private JDAPAVA m_ava = null;

    /**
     * Constructs base filter for other attribute value assertion.
     * @param tag attribute tag
     * @param ava attribute value assertion
     */
    public JDAPFilterAVA(int tag,
        JDAPAVA ava) {
        m_tag = tag;
        m_ava = ava;
    }

    /**
     * Get attribute value assertion.
     * @return value assertion
     */
    public JDAPAVA getAVA() {
        return m_ava;
    }

    /**
     * Gets the ber representation of the filter.
     * @return ber representation
     */
    public BERElement getBERElement() {
        BERTag element = new BERTag(m_tag, m_ava.getBERElement(), true);
        return element;
    }
}
