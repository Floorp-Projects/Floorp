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
package netscape.ldap.client;

import java.util.*;
import netscape.ldap.ber.stream.*;
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
