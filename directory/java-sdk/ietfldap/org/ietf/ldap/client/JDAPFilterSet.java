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
 * This class implements the base class of filter "and" and filter "or".
 *
 * @version 1.0
 */
public abstract class JDAPFilterSet extends JDAPFilter {
    /**
     * Internal variables
     */
    private int m_tag;
    private Vector m_set = new Vector();

    /**
     * Constructs the filter set.
     * @param tag tag
     */
    public JDAPFilterSet(int tag) {
        super();
        m_tag = tag;
    }

    /**
     * Adds filter into the filter set.
     * @param filter adding filter
     */
    public void addElement(JDAPFilter filter) {
        m_set.addElement(filter);
    }

    /**
     * Gets the ber representation of the filter.
     * @return ber representation
     */
    public BERElement getBERElement() {
        try {
            BERSet filters = new BERSet();
            for (int i = 0; i < m_set.size(); i++) {
                JDAPFilter f = (JDAPFilter)m_set.elementAt(i);
                filters.addElement(f.getBERElement());
            }
            BERTag element = new BERTag(m_tag, filters, true);
            return element;
        } catch (IOException e) {
            return null;
        }
    }

    /**
     * Gets the filter set parameters.
     * @return set parameters
     */
    public String getParamString() {
        String s = "";
        for (int i = 0; i < m_set.size(); i++) {
            if (i != 0)
                s = s + ",";
            JDAPFilter f = (JDAPFilter)m_set.elementAt(i);
            s = s + f.toString();
        }
        return s;
    }
}
