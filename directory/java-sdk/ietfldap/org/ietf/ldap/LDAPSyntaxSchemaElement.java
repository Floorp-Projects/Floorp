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
package org.ietf.ldap;

import java.util.*;

/**
 * Helper class supporting schema elements that include syntax
 * definitions - attributes and matching rules
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPAttributeSchema
 * @see org.ietf.ldap.LDAPMatchingRuleSchema
 **/

class LDAPSyntaxSchemaElement extends LDAPSchemaElement {

    static final long serialVersionUID = 6086340702503710702L;

    /**
     * Construct a blank element.
     */
    LDAPSyntaxSchemaElement() {
        super();
    }

    /**
     * Gets the syntax of the schema element
     * @return One of the following values:
     * <UL>
     * <LI><CODE>cis</CODE> (case-insensitive string)
     * <LI><CODE>ces</CODE> (case-exact string)
     * <LI><CODE>binary</CODE> (binary data)
     * <LI><CODE>int</CODE> (integer)
     * <LI><CODE>telephone</CODE> (telephone number -- identical to cis,
     * but blanks and dashes are ignored during comparisons)
     * <LI><CODE>dn</CODE> (distinguished name)
     * <LI><CODE>unknown</CODE> (not a known syntax)
     * </UL>
     */
    int getSyntax() {
        return syntax;
    }

    /**
     * Gets the syntax of the attribute type in dotted-decimal format,
     * for example "1.2.3.4.5"
     * @return The attribute syntax in dotted-decimal format.
     */
    String getSyntaxString() {
        return syntaxString;
    }

    /**
     * Convert from enumerated syntax types to an OID
     * @param syntax One of the enumerated syntax types
     * @return The OID corresponding to the internal type
     */
    static String internalSyntaxToString( int syntax ) {
        String s;
        if ( syntax == cis ) {
            s = cisString;
        } else if ( syntax == binary ) {
            s = binaryString;
        } else if ( syntax == ces ) {
            s = cesString;
        } else if ( syntax == telephone ) {
            s = telephoneString;
        } else if ( syntax == dn ) {
            s = dnString;
        } else if ( syntax == integer ) {
            s = intString;
        } else {
            s = null;
        }
        return s;
    }

    /**
     * Convert from enumerated syntax type to a user-friendly
     * string
     * @param syntax One of the enumerated syntax types
     * @return A user-friendly syntax description
     */
    String syntaxToString() {
        String s;
        if ( syntax == cis ) {
            s = "case-insensitive string";
        } else if ( syntax == binary ) {
            s = "binary";
        } else if ( syntax == integer ) {
            s = "integer";
        } else if ( syntax == ces ) {
            s = "case-exact string";
        } else if ( syntax == telephone ) {
            s = "telephone";
        } else if ( syntax == dn ) {
            s = "distinguished name";
        } else {
            s = syntaxString;
        }
        return s;
    }

    /**
     * Convert from an OID to one of the enumerated syntax types
     * @param syntax A dotted-decimal OID
     * @return The internal enumerated type corresponding to the
     * OID; <CODE>unknown</CODE> if it is not one of the known
     * types
     */
    int syntaxCheck( String syntax ) {
        int i = unknown;
        if ( syntax == null ) {
        } else if ( syntax.equals( cisString ) ) {
            i = cis;
        } else if ( syntax.equals( binaryString ) ) {
            i = binary;
        } else if ( syntax.equals( cesString ) ) {
            i = ces;
        } else if ( syntax.equals( intString ) ) {
            i = integer;
        } else if ( syntax.equals( telephoneString ) ) {
            i = telephone;
        } else if ( syntax.equals( dnString ) ) {
            i = dn;
        }
        return i;
    }
    
    public String toString() {
        return "LDAPSyntaxSchemaElement: " + getSyntaxString();
    }

    int syntax = unknown;
    String syntaxString = null;
}
