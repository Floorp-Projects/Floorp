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
package netscape.ldap.util;

import netscape.ldap.*;
import java.util.*;

/**
 * Represents an LDAP search filter, which includes the string-based
 * representation of the filter and other information retrieved from the
 * LDAP filter configuration file (or from a buffer in memory or from a URL).
 * <P>
 *
 * Although this class provides methods to create and modify LDAP
 * filters, in most cases, you do not need to use these methods.
 * In most situations, these classes are used to access individual
 * filters from filter configuration files.
 * <P>
 *
 * For example, you might do the following:
 * <P>
 *
 * <OL>
 * <LI>Connect to the LDAP server and accept a set of search criteria.
 * <LI>Create an LDAP filter configuration file.
 * <LI>Call the <CODE>LDAPFilterDescriptor</CODE> constructor to
 * read the filter configuration file into memory.
 * <LI>Call the <CODE>getFilters</CODE> method to get a list of
 * filters that match the search criteria.  This list of filters
 * is represented by an <CODE>LDAPFilterList</CODE> object.
 * <LI>Iterate through the list of filters (each filter is represented
 * by an <CODE>LDAPFilter</CODE> object), and apply the filter to
 * a search.
 * </OL>
 * <P>
 *
 * For an example of using an object of this class and for more information on
 * the filter configuration file syntax, see the documentation for <a
 * href="n.l.u.LDAPFilterDescriptor.html">LDAPFilterDescriptor</a>.
 * <P>
 *
 * @see LDAPFilterDescriptor
 * @see LDAPFilterList
 * @version 1.0
 */

public class LDAPFilter implements Cloneable {

    private static final int DEFAULT_FILTER_LENGTH = 256;

    private String m_strFilter = null;
    private String m_strDescription;        // token 4 from filter configuration file
    private int m_nScope;           // token 5 from filter configuration file
    private boolean m_bIsExact;

    private String m_strMatchPattern;       // token 1 from filter configuration file
    private String m_strDelimeter;      // token 2 from filter configuration file
    private String m_strFilterTemplate;     // token 3 from filter configuration file

    private int m_nLine;
    private String m_strSuffix;
    private String m_strPrefix;

    /**
     * Constructs an <CODE>LDAPFilter</CODE> object.  In most situations,
     * you do not need to construct an LDAPFilter object.  Instead, you
     * work with <CODE>LDAPFilter</CODE> objects created from the
     * <CODE>LDAPFilter</CODE> objects and <CODE>LDAPFilterDescriptor</CODE>
     * objects.
     * <P>
     *
     * This constructor uses the integer value for a search scope in
     * addition to other information to construct an <CODE>LDAPFilter</CODE>
     * object.  The integer value of the search scope can be one of the
     * following:
     *   <ul>
     *     <li><CODE>LDAPConnection.SCOPE_BASE</CODE>
     *     <li><CODE>LDAPConnection.SCOPE_ONE</CODE>
     *     <li><CODE>LDAPConnection.SCOPE_SUB</CODE>
     *   </ul>
     *
     * If an invalid scope is specified, the constructor throws an
     * <CODE>illegalArgumentException</CODE>.
     */
    public LDAPFilter ( String strMatchPattern,
            String strDelimeter,
            String strFilterTemplate,
            String strDescription,
                int nScope ) throws IllegalArgumentException{

        m_strMatchPattern = convertMatchPattern ( strMatchPattern );
        m_strDelimeter = strDelimeter;
        m_strFilterTemplate = strFilterTemplate;
        m_strDescription = strDescription;
        m_nScope = nScope;

    }

    /**
     * Constructs an <CODE>LDAPFilter</CODE> object.  In most situations,
     * you do not need to construct an LDAPFilter object.  Instead, you
     * work with <CODE>LDAPFilter</CODE> objects created from the
     * <CODE>LDAPFilter</CODE> objects and <CODE>LDAPFilterDescriptor</CODE>
     * objects.
     * <P>
     *
     * This constructor uses the string value for a search scope in
     * addition to other information to construct an <CODE>LDAPFilter</CODE>
     * object.  The string value of the search scope can be one of the
     * following:
     *   <ul>
     *     <li><CODE>&quot;base&quot;</CODE>
     *     <li><CODE>&quot;onelevel&quot;</CODE>
     *     <li><CODE>&quot;subtree&quot;</CODE>
     *   </ul>
     *
     * If an invalid scope is specified, the constructor throws an
     * <CODE>illegalArgumentException</CODE>.
     */
    public LDAPFilter ( String strMatchPattern,
            String strDelimeter,
            String strFilterTemplate,
            String strDescription,
                String strScope )
            throws IllegalArgumentException {
        if ( strScope.equals ( "base" ) ) {
            m_nScope = LDAPConnection.SCOPE_BASE;

        } else if ( strScope.equals ( "onelevel" ) ) {
            m_nScope = LDAPConnection.SCOPE_ONE;

        } else if ( strScope.equals ( "subtree" ) ) {
            m_nScope = LDAPConnection.SCOPE_SUB;
        }

        m_strMatchPattern = strMatchPattern;
        m_strDelimeter = strDelimeter;
        m_strFilterTemplate = strFilterTemplate;
        m_strDescription = strDescription;
    }

    /**
     * Print out a string representation of the LDAPFilter.
     * Basically, it prints out the appropriate fields.
     * <P>
     *
     * For example, suppose you called the method in this way:
     * <P>
     *
     * <PRE>System.out.println(filter.toString());</PRE>
     *
     * The resulting output might look like this:
     * <P>
     *
     * <PRE>
     *      matchPtn: "@"
     *      delim:    " "
     *      filttmpl: "(mail=%v*)"
     *      descript: "start of email address"
     *      scope: "LDAPConnection.SCOPE_SUB"
     *      line:     "32"
     *      FILTER:   "(mail=babs@aceindustry.com*)"
     * </PRE>
     */
    public String toString() {
        StringBuffer strBuf = new StringBuffer ( 300 );
        strBuf.append ( "      matchPtn: \"" + m_strMatchPattern+"\"\n" );
        strBuf.append ( "      delim:    \"" + m_strDelimeter+"\"\n" );
        strBuf.append ( "      filttmpl: \"" + m_strFilterTemplate+"\"\n" );
        strBuf.append ( "      descript: \"" + m_strDescription+"\"\n" );
        switch ( m_nScope ) {
            case LDAPConnection.SCOPE_BASE:
                strBuf.append ( "      scope: \"LDAPConnection.SCOPE_BASE\"\n" );
            break;
            case LDAPConnection.SCOPE_ONE:
                strBuf.append ( "      scope: \"LDAPConnection.SCOPE_ONE\"\n" );
            break;
            case LDAPConnection.SCOPE_SUB:
                strBuf.append ( "      scope: \"LDAPConnection.SCOPE_SUB\"\n" );
            break;
        }
        strBuf.append ( "      line:     \"" + m_nLine+"\"\n" );
        strBuf.append ( "      FILTER:   \"" + m_strFilter+"\"\n" );

        return strBuf.toString();
    }

    /**
     * Sets up the filter string, given the string <CODE>strValue</CODE>.
     * If the <CODE>strPrefix</CODE> and <CODE>strSuffix</CODE> arguments
     * are non-null strings, they are prepended and appended
     * to the filter string (respectively).
     * <P>
     *
     * This string, which is available through the <CODE>getFilter()</CODE>
     * method, should be suitable for use as search criteria when
     * calling the <CODE>LDAPConnection.search()</CODE> method.
     * <P>
     *
     * <b>Notes:</b>
     * <P>
     *
     * <UL>
     * <LI>This method <i>does not</i> maintain the affixes
     * set with the <CODE>LDAPFilterDescriptor.setFilterAffixes</CODE>
     * method, so you
     * need to explicitly define any filter prefixes or suffixes here.<p>
     *
     * <LI> This method only uses the
     * strPrefix and strSuffix for this invocation of setupFilter.  It
     * does not redefine strPrefix and strSuffix for later
     * invocations. <p>
     * </UL>
     * <P>
     *
     * @see netscape.ldap.util.LDAPFilterDescriptor#setFilterAffixes
     * @see #setFilterAffixes
     */
    public void setupFilter ( String strValue, String strPrefix,
                String strSuffix ) {
        createFilterString ( strValue, strPrefix, strSuffix );
    }

    /**
     * Sets up the filter string, given the string <CODE>strValue</CODE>.
     * This string, which is available through the <CODE>getFilter()</CODE>
     * method, should be suitable for use as search criteria when
     * calling the <CODE>LDAPConnection.search()</CODE> method.
     * <P>
     *
     * <b>Note:</b> If you want to specify a filter prefix and suffix,
     * you need to explicitly define them by calling the
     * <CODE>setFilterAffixes()</CODE> method.
     *
     * @see netscape.ldap.util.LDAPFilterDescriptor#setFilterAffixes
     * @see #setFilterAffixes
     *
     */
    public void setupFilter ( String strValue ) {
        createFilterString ( strValue, null, null );
    }

    /**
     * Create the filter string which can be used in
     * LDAPConnection.search() and others given the parameter
     * strValue.  If strPrefix and strSuffix are non-null strings,
     * prepend strPrefix and append strSuffix.
     */
    void createFilterString ( String strValue, String strPrefix,
                 String strSuffix ) {
        StringTokenizer strTok =
            new StringTokenizer ( strValue, m_strDelimeter );

        // Initialize an array of broken up values so that we
        // can reference  them directly.
        String[] aValues = new String[strTok.countTokens()];
        int nTokens = strTok.countTokens();
        for ( int i = 0; i < nTokens; i++ ) {
            aValues[i] = strTok.nextToken();
        }

        StringBuffer sbFilter = new StringBuffer ( DEFAULT_FILTER_LENGTH);
        if ( strPrefix != null ) {
            sbFilter.append ( strPrefix );
        }
        char[] cFilterTemplate = m_strFilterTemplate.toCharArray();
        int i = 0;
        while ( i < cFilterTemplate.length ) {
            if ( cFilterTemplate[i] == '%' ) {
                i++;
                if ( cFilterTemplate[i] == 'v' ) {
                    if ( i == (cFilterTemplate.length-1) ) {
                        sbFilter.append ( strValue );
                        break;
                    }
                    i++;
                    switch ( cFilterTemplate[i] ) {
                        case '$':
                            sbFilter.append ( aValues[aValues.length] );
                        break;

                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                            int nValue = Integer.parseInt
                                ( new Character
                                  (cFilterTemplate[i]).toString() );
                            nValue--;
                            i++;
                            if ( cFilterTemplate[i] == '-' ) {
                                i++;
                                if ( Character.isDigit ( cFilterTemplate[i] )) {
                                int nValue2 = Integer.parseInt
                                    ( new Character
                                    (cFilterTemplate[i]).toString() );
                                nValue2--;
                                for ( int j = nValue; j <= nValue2; j++ ) {
                                    sbFilter.append ( aValues[j] );
                                    sbFilter.append
                                    ( ( j == nValue2 ) ? "" : " " );
                                }
                                } else {
                                for ( int j = nValue; j < aValues.length;j++ ) {
                                    sbFilter.append ( aValues[j] );
                                    sbFilter.append
                                        ( ( j == aValues.length - 1 ) ? "" : " " );
                                }
                                sbFilter.append ( cFilterTemplate[i]);
                                }

                            } else {
                                sbFilter.append ( aValues[nValue] );
                                sbFilter.append ( cFilterTemplate[i] );
                            }
                        break;

                        // We just got a plain old %v, so insert the
                        // strValue
                        default:
                            sbFilter.append ( strValue );
                            sbFilter.append ( cFilterTemplate[i] );
                        break;
                    }

                } else {
                    sbFilter.append ( "%" );
                    sbFilter.append ( cFilterTemplate[i] );
                }
            } else {
                sbFilter.append ( cFilterTemplate[i] );
            }
            i++;
        }
        if ( strSuffix != null ) {
            sbFilter.append ( strSuffix );
        }
        m_strFilter = sbFilter.toString();
    }


    /**
     * Makes a copy of this <CODE>LDAPFilter</CODE> object.
     */
    public Object clone() {
        try {
            return super.clone();
        } catch ( CloneNotSupportedException e ) {
            // this shouldn't happen, since we are Cloneable
            throw new InternalError();
        }
    }

    /**
     * Set the line number from which this filter was created.  This
     * is used only when the LDAPFilter is created when an
     * LDAPFilterDescriptor is initialized from a file/URL/buffer.
     */
    void setLine ( int nLine ) {
        m_nLine = nLine;
    }

    /**
     * The ldapfilter.conf specifies match patterns in a funny way.
     * A "." means "any character" except when used inside of a set of
     * square brackets "[]", in which case, the "." means just a
     * plain old period (not a * special character).
     *
     * This function converts periods inside of a set of square
     * brackets into a "\." as per normal regexp code.
     */
    String convertMatchPattern ( String strMatchPattern ) {
        StringBuffer sb = new StringBuffer ( strMatchPattern.length() + 1);
        char[] a_cMatchPattern = strMatchPattern.toCharArray();
        boolean bInBrackets = false;
        for ( int i = 0; i < a_cMatchPattern.length; i++ ) {
            if ( a_cMatchPattern[i] == '.' ) {
                if ( bInBrackets ) {
                    sb.append ( "\\" );
                }
            } else if ( a_cMatchPattern[i] == '[' ) {
                bInBrackets = true;
            } else if ( a_cMatchPattern[i] == ']' ) {
                bInBrackets = false;
            }
            sb.append ( a_cMatchPattern[i] );
        }
        return sb.toString();
    }


    /**
     * Returns the filter string.   This method will return null if the
     * filter string has not been calculated by the <CODE>setupFilter()</CODE>,
     * <CODE>getFilter (strValue)</CODE>, or <CODE>getFilter (strValue,
     * strPrefix, strSuffix )</CODE> methods.
     *
     * @see #setupFilter
     * @see #getFilter
     */
    public String getFilter () {
        return m_strFilter;
    }

    /**
     * Create a filter string given a string value.  This method uses
     * any Prefixes or Suffixes that have been set by the
     * setFilterAffixes() method.<p>
     *
     * This is the same as doing:
     * <pre>
     *   setupFilter ( strValue );
     *   getFilter();
     * </pre>
     */
    public String getFilter ( String strValue ) {
        createFilterString ( strValue, m_strPrefix, m_strSuffix );
        return m_strFilter;
    }


    /**
     * Create a filter string given a string value.  If strPrefix
     * and/or strSuffix is non-null, these values are prepended and
     * appended to the returned string.<p>
     *
     * This is the same as doing:
     * <pre>
     *   setupFilter ( strValue, strPrefix, strSuffix );
     *   getFilter();
     * </pre>
     */
    public String getFilter ( String strValue, String strPrefix,
                        String strSuffix ) {
        createFilterString ( strValue, strPrefix, strSuffix );
        return m_strFilter;
    }

    /**
     * Return this filter's match pattern.  The match pattern is
     * found as the first token in a filter configuration line in the
     * ldapfilter.conf file.
     */
    public String getMatchPattern() {
        return m_strMatchPattern;
    }

    /**
     * Return this filter's delimeter.  The delmimeter is
     * found as the second token in a filter configuration line in the
     * ldapfilter.conf file.
     */
    public String getDelimeter() {
        return m_strDelimeter;
    }

    /**
     * Return this filter's filter template.  The filter template is
     * found as the third token in a filter configuration line in the
     * ldapfilter.conf file.
     */
    public String getFilterTemplate() {
        return m_strFilterTemplate;
    }

    /**
     * Return this filter's description.  The description is
     * found as the fourth token in a filter configuration line in the
     * ldapfilter.conf file.
     */
    public String getDescription() {
        return m_strDescription;
    }

    /**
     * Return this filter's scope.  The scope is
     * found as the fifth (optional) token in a filter configuration
     * line in the ldapfilter.conf file.
     */
    public String getScope() {
        switch ( m_nScope ) {
            case LDAPConnection.SCOPE_BASE:
                return "base";

            case LDAPConnection.SCOPE_ONE:
                return "onelevel";

            case LDAPConnection.SCOPE_SUB:
                return "subtree";

            default:
                return "UNKNOWN!";
        }
    }

    /**
     * Return this filter's line number.  The line number is
     * mostly a debugging variable to let the developer know which
     * filter from the filter configuration file is being used.
     */
    public String getLineNumber() {
        return Integer.toString ( m_nLine );
    }

    public void setFilterAffixes ( String strPrefix, String strSuffix ) {
        m_strPrefix = strPrefix;
        m_strSuffix = strSuffix;
    }
}
