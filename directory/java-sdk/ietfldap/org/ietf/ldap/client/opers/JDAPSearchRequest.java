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
 * This class implements the search request. This object is
 * sent to the ldap server.
 * <pre>
 * SearchRequest ::= [APPLICATION 3] SEQUENCE {
 *   baseObject LDAPDN,
 *   scope ENUMERATED {
 *     baseObject (0),
 *     singleLevel (1),
 *     wholeSubtree (2)
 *   },
 *   derefAliases ENUMERATED {
 *     neverDerefAliases (0),
 *     derefInSearching (1),
 *     DerefFindingBaseObj (2),
 *     DerefAlways (3)
 *   },
 *   sizeLimit INTEGER(0..maxInt),
 *   timeLimit INTEGER(0..maxInt),
 *   attrsOnly BOOLEAN,
 *   filter Filter,
 *   attributes SEQUENCE OF AttributeType
 * }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPSearchRequest extends JDAPBaseDNRequest
    implements JDAPProtocolOp {
    /**
     * search scope
     */
    public final static int BASE_OBJECT = 0;
    public final static int SINGLE_LEVEL = 1;
    public final static int WHOLE_SUBTREE = 2;
    /**
     * alias dereference
     */
    public final static int NEVER_DEREF_ALIASES = 0;
    public final static int DEREF_IN_SEARCHING = 1;
    public final static int DEREF_FINDING_BASE_OBJ = 2;
    public final static int DEREF_ALWAYS = 3;

    public final static String DEFAULT_FILTER = "(objectclass=*)";
    /**
     * Private variables
     */
    protected String m_base_dn = null;
    protected int m_scope;
    protected int m_deref;
    protected int m_size_limit;
    protected int m_time_limit;
    protected boolean m_attrs_only;
    protected String m_filter = null;
    protected JDAPFilter m_parsedFilter = null; 
    protected String m_attrs[] = null;

    /**
     * Constructs search request.
     * @param base_dn base object entry relative to the search
     * @param scope scope of the search
     * @param deref how alias objects should be handled
     * @param size_limit maximum number of entries
     * @param time_limit maximum time (in time) allowed
     * @param attrs_only should return type only
     * @param filter string filter based on RFC1558
     * @param attrs list of attribute types
     * @exception IllegalArgumentException if the filter has bad syntax
     * 
     */
    public JDAPSearchRequest(String base_dn, int scope, int deref,
        int size_limit, int time_limit, boolean attrs_only,
        String filter, String attrs[]) throws IllegalArgumentException {
        m_base_dn = base_dn;
        m_scope = scope;
        m_deref = deref;
        m_size_limit = size_limit;
        m_time_limit = time_limit;
        m_attrs_only = attrs_only;
        m_filter = (filter == null) ? DEFAULT_FILTER : filter;
        m_parsedFilter = JDAPFilter.getFilter(
            JDAPFilterOpers.convertLDAPv2Escape(m_filter));
        if (m_parsedFilter == null){
            throw new IllegalArgumentException("Bad search filter");
        }        
        m_attrs = attrs;
    }

    /**
     * Retrieves the protocol operation type.
     * @return operation type
     */
    public int getType() {
        return JDAPProtocolOp.SEARCH_REQUEST;
    }

    /**
     * Sets the base dn component.
     * @param basedn base dn
     */
    public void setBaseDN(String basedn) {
        m_base_dn = basedn;
    }

    /**
     * Gets the base dn component.
     * @return base dn
     */
    public String getBaseDN() {
        return m_base_dn;
    }

    /**
     * Gets the ber representation of search request.
     * @return ber representation of request.
     */
    public BERElement getBERElement() {
        /* Assumed that searching with the following criteria:
         *     base object = "c=ca"
         *     filter      = FilterPresent(objectClass)
         * [*] zoomit server v1.0 (search on c=ca)
         *     0x63 0x81 0x8d              ([APPLICATION 3])
         *     0x30 0x81 0x8a 0x04 0x00    (seq of)
         *     0x0a 0x01 0x02
         *     0x0a 0x01 0x00
         *     0x02 0x01 0x65
         *     0x02 0x01 0x1e
         *     0x01 0x01 0x00
         *     0xa0 0x3b
         *     0x30 0x39 0xa5 0x0d
         *     0x30 0x0b 0x04 0x07 P A G E _ D N
         *     0x04 0x00
         *     0xa3 0x0e
         *     0x30 0x0c 0x04 0x08 P A G E _ K E Y
         *     0x04 0x00 0xa3 0x18
         *     0x30 0x16 0x04 0x0e z c A n y A t t r i b u t e
         *     0x04 0x04 c = c a
         *     0x30 0x3a 0x04 0x0b O b j e c t C l a s s
         *     0x04 0x11 a l i a s e d O b j e c t N a m e
         *     0x04 0x07 d s e T y p e
         *     0x04 0x0f t e l e p h o n e N u m b e r
         * [*] umich-ldap-v3.3:
         *     0x63 0x24          ([APPLICATION 3])
         *     0x04 0x04 c = c a  (base object - OctetString)
         *     0x0a 0x01 0x00     (scope - Enumerated)
         *     0x0a 0x01 0x00     (derefAlias - Enumerated)
         *     0x02 0x01 0x00     (size limit - Integer)
         *     0x02 0x01 0x00     (time limit - Integer)
         *     0x01 0x01 0x00     (attr only - Boolean)
         *     0x87 0x0b o b j e c t C l a s s (filter)
         *     0x30 0x00          (attrs - Sequence of OctetString)
         */
        BERSequence seq = new BERSequence();
        seq.addElement(new BEROctetString(m_base_dn));
        seq.addElement(new BEREnumerated(m_scope));
        seq.addElement(new BEREnumerated(m_deref));
        seq.addElement(new BERInteger(m_size_limit));
        seq.addElement(new BERInteger(m_time_limit));
        seq.addElement(new BERBoolean(m_attrs_only));
        seq.addElement(m_parsedFilter.getBERElement());
        BERSequence attr_type_list = new BERSequence();
        if (m_attrs != null) {
            for (int i = 0; i < m_attrs.length; i++) {
                attr_type_list.addElement(new BEROctetString(m_attrs[i]));
            }
        }
        seq.addElement(attr_type_list);
        BERTag element = new BERTag(BERTag.APPLICATION|BERTag.CONSTRUCTED|3,
          seq, true);
        return element;
    }

    /**
     * Retrieves the string representation of the request.
     * @return string representation
     */
    public String toString() {
        String s = null;
        if (m_attrs != null) {
            s = "";
            for (int i = 0; i < m_attrs.length; i++) {
                if (i != 0)
                    s = s + "+";
                s = s + m_attrs[i];
            }
        }
        return "SearchRequest {baseObject=" + m_base_dn + ", scope=" +
          m_scope + ", derefAliases=" + m_deref + ",sizeLimit=" +
          m_size_limit + ", timeLimit=" + m_time_limit + ", attrsOnly=" +
          m_attrs_only + ", filter=" + m_filter + ", attributes=" +
          s + "}";
    }
}
