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
 * This class implements the extended match filter.
 * <pre>
 * extensibleMatch [9] AttributeValueAssertion
 * </pre>
 *
 * @version 1.0
 */
public class JDAPFilterExtensible extends JDAPFilter {
    /**
     * Constructs extensible match filter.
     * @param match Matching rule assertion
     */
    public JDAPFilterExtensible(String type, String match) {
        m_tag = BERTag.CONSTRUCTED|BERTag.CONTEXT|9;
        m_type = type;
        m_value = match;
    }

    /**
     * Gets ber representation of the filter.
     * <PRE>
     * Extended filter:   [type] [':dn'][':'oid]':='value
     *
     * BER:   extensibleMatch    [9] MatchingRuleAssertion
     *        MatchingRuleAssertion ::= SEQUENCE {
     *             matchingRule    [1] MatchingRuleID OPTIONAL,
     *             type            [2] AttributeDescription OPTIONAL,
     *             matchValue      [3] AssertionValue,
     *             dnAttributes    [4] BOOLEAN DEFAULT FALSE
     *        }
     * </PRE>
     * @return ber representation
     */
    public BERElement getBERElement() {
        String value = m_value;
        String defs = m_type;

        /* Need either ':dn' or ':'oid */
        int colonIndex = defs.lastIndexOf(':');
        if ( colonIndex == -1 ) {
            return null;
        }
        /* Is it dn or oid? */
        boolean isDN = false;
        String oid = null;
        if ( defs.regionMatches( true, colonIndex+1, "dn", 0, 2 ) )
            isDN = true;
        else
            oid = defs.substring( colonIndex+1 );

        /* Any more? */
        defs = defs.substring( 0, colonIndex );
        colonIndex = defs.lastIndexOf(':');
        if ( colonIndex >= 0 ) {
            /* Is it dn or oid? */
            if ( defs.regionMatches( true, colonIndex+1, "dn", 0, 2 ) )
                isDN = true;
            else
                oid = defs.substring( colonIndex+1 );
        }

        BERSequence seq = new BERSequence();
        BERTag tag;

        /* Was there an oid? */
        if ( oid != null ) {
            tag = new BERTag( BERTag.CONTEXT|BERTag.MRA_OID, new
                                    BEROctetString(oid), true );
            seq.addElement( tag );
        }

        /* Was there an attribute description? */
        if ( defs.length() > 0 ) {
            tag = new BERTag( BERTag.CONTEXT|BERTag.MRA_TYPE, new
                                    BEROctetString(defs), true );
            seq.addElement( tag );
        }

        /* Got to have a value */
        tag = new BERTag( BERTag.CONTEXT|BERTag.MRA_VALUE, new
                                BEROctetString(value), true );
        seq.addElement( tag );

        /* Was ':dn' specified? */
        tag = new BERTag( BERTag.CONTEXT|BERTag.MRA_DNATTRS, new
                                BERBoolean(isDN), true );
        seq.addElement( tag );

        BERTag element = new BERTag( m_tag, seq, true );
        return element;
    }

    /**
     * Retrieves the string representation of the filter.
     * @return string representation
     */
    public String toString() {
        return "JDAPFilterExtensible {" + m_value + "}";
    }

    private int m_tag;
    private String m_type = null;
    private String m_value = null;
}
