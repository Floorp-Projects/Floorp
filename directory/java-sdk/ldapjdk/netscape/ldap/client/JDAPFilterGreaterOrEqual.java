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
 * This class implements the greater or equal filter.
 * <pre>
 * greaterOrEqual [5] AttributeValueAssertion
 * </pre>
 *
 * @version 1.0
 * @see RFC1777
 */
public class JDAPFilterGreaterOrEqual extends JDAPFilterAVA {
    /**
     * Constructs greater or equal filter.
     * @param ava attribute value assertion
     */
    public JDAPFilterGreaterOrEqual(JDAPAVA ava) {
        super(BERTag.CONSTRUCTED|BERTag.CONTEXT|5, ava);
    }

    /**
     * Retrieves the string representation of the filter.
     * @return string representation
     */
    public String toString() {
        return "JDAPFilterGreaterOrEqual {" +
          super.getAVA().toString() + "}";
    }
}
