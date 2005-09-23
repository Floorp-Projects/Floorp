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
package netscape.ldap.util;

import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 *  Represents an LDAPIntFilterSet object.  This is an internal object that
 *  should never be instantiated directly by the developer.
 */

public class LDAPIntFilterSet {

    private Vector m_vLDAPIntFilterList;
    private String m_strTag;

    private Matcher m_matcher = null;
    /**
     * Return a Vector of filters that match botht the tag pattern
     * (in Perl5Pattern form), and the string strValue.  This method
     * should only be called by LDAPFilterDescriptor().
     */

     // remember, we have the string (m_strTag), the pattern has
     // been precompiled by the LDAPFilterDescriptor (patTag)
    Vector getFilters ( Pattern patTag,
                String matcherValue ) {
        Vector vRet = new Vector();

        if ( m_matcher == null ) {
            m_matcher = patTag.matcher(m_strTag);
        }

        // Check to see if the strTag (converted into patTag)
        // matches the tag string from the file (converted into
        // m_matcherTag)
        if ( m_matcher.find() ) {
            LDAPIntFilterList tmpIntFilterList;
            LDAPFilter tmpFilter;
            for ( int i = 0; i < m_vLDAPIntFilterList.size(); i++ ) {
                tmpIntFilterList =
                  (LDAPIntFilterList)m_vLDAPIntFilterList.elementAt ( i );

                if ( tmpIntFilterList.MatchFilter ( matcherValue ) ) {
                    for (int j=0; j < tmpIntFilterList.numFilters(); j++ ) {
                        vRet.addElement ( tmpIntFilterList.getFilter ( j ));
                    }
                    // potential BUGBUG, i'm not sure if we want
                    // to get out  of this loop now or if we just
                    // want to get out of the external loop.  For
                    // now, go with the former.
                    return vRet;
                }
            }
        }

        return vRet;
    }

    /**
     * Create an LDAPIntFilterSet with a given Tag string.  The tag
     * string specifies which applications or query types should use
     * this filter set.  It is normally a single token on a line by
     * itself in the filter configuration file. <p>
     * For more information about the filter configuration file, see
     * the man page for ldapfilter.conf.
     */
    public LDAPIntFilterSet ( String strTag ) {
        m_strTag = strTag;
        m_vLDAPIntFilterList = new Vector();
    }

    /**
     * Add a new filter to this filter set.
     *
     * @exception netscape.ldap.util.BadFilterException
     *    If the regular expression pattern given in the first token
     *    is bad.
     */
    void newFilter ( LDAPFilter filter ) throws BadFilterException {
        LDAPIntFilterList tmpFilterList = new LDAPIntFilterList( filter );
        m_vLDAPIntFilterList.addElement ( tmpFilterList );
    }

    /**
     * Append a new filter to the existing set.  This happens when the
     * LDAPFilterDescriptor object reads a line from the filter
     * configuration file that has 2 or 3 tokens.
     */
    void appendFilter ( LDAPFilter filter ) {
        ((LDAPIntFilterList)m_vLDAPIntFilterList.lastElement()).AddFilter ( filter );
    }



    /**
     * Return true if this filter set matches the regular expression
     * string that is passed in.
     */
    boolean match ( String strTagPat ) {
    	return Pattern.matches(strTagPat, m_strTag);
    }

    /**
     * Print out the String representation of this object.  It calls
     * the toString() method of all the LDAPFilter objects contained
     * within it's set.
     *
     * @see LDAPFilter#toString()
     */
    public String toString() {
        StringBuffer strBuf = new StringBuffer ( 2000 );
        strBuf.append ( "  strTag: " + m_strTag + "\n" );
        for ( int i = 0; i < m_vLDAPIntFilterList.size(); i++ ) {
            strBuf.append ( "  filter #: " + i + "\n" );
            strBuf.append (
                ((LDAPIntFilterList)m_vLDAPIntFilterList.elementAt(i)).toString() );
            strBuf.append ( "\n" );
        }
        return strBuf.toString();
    }
}
