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
 * This class implements the filter present.
 * <pre>
 *   present [7] AttributeType
 * </pre>
 *
 * @version 1.0
 * @see RFC1777
 */
public class JDAPFilterPresent extends JDAPFilter {
    /**
     * Internal variables
     */
    private String m_type = null;

    /**
     * Constructs the filter.
     * @param type attribute type
     */
    public JDAPFilterPresent(String type) {
        super();
        m_type = type;
    }

    /**
     * Gets ber representation of the filter.
     * @return ber representation
     */
    public BERElement getBERElement() {
        BEROctetString s = new BEROctetString(m_type);
        BERTag element = new BERTag(BERTag.CONTEXT|7, s, true);
        return element;
    }

    /**
     * Gets string reprensetation of the filter.
     * @return string representation
     */
    public String toString() {
        return "JDAPFilterPresent {" + m_type + "}";
    }
}
