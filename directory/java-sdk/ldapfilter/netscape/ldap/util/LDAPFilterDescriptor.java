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

import java.io.*;
import java.util.*;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.regex.PatternSyntaxException;
import java.net.*;
import netscape.ldap.*;

/**
 * Represents an LDAP filter configuration file read into memory.
 * <P>
 *
 * Once you read in a filter file to create an object of this class,
 * you can access the filter information through the methods that create
 * <CODE>LDAPFilterList</CODE> and <CODE>LDAPFilter</CODE> objects.
 * (You do not need to manually construct these objects yourself.)
 * <P>
 *
 * This class (along with the other LDAP filter classes) provide
 * functionality equivalent to the LDAP filter functions in the LDAP C API.
 * <p>
 *
 * The format of the file/URL/buffer must be that as defined in the
 * ldapfilter.conf(5) man page from the University of Michigan LDAP-3.3
 * distribution.  <p>
 *
 * The LDAP filter classes provide a powerful way to configure LDAP clients
 * by modifying a configuration file.<p>
 *
 * The following is a short example for how to use the
 * LDAP filter classes.
 *
 * <pre>
 *
 * // ... Setup LDAPConnection up here ...
 * <p>
 *
 * LDAPFilterDescriptor filterDescriptor;
 * <p>
 *
 * // Create the LDAPFilterDescriptor given the file
 * // "ldapfilter.conf".
 * try {
 *    filterDescriptor = new LDAPFilterDescriptor ( "ldapfilter.conf" );
 *     <p>
 *
 *    // Now retrieve the Filters in the form of an
 *    // LDAPFilterList
 *     LDAPFilterList filterList = new filterDescriptor.getFilters("match_tag", "string_user_typed");
 *     <p>
 *
 *    // For each filter, do the search.  Normally, you wouldn't
 *    // do the search if the first filter matched, but this is
 *    // just showing the enumeration type aspects of
 *    // LDAPFilterList
 *     LDAPFilter filter;
 *     while ( filterList.hasMoreElements() ) {
 *      filter = filterList.next();
 *      LDAPResults results = LDAPConnection.search (
 *                      strBase,                // base DN
 *                      filter.getScope(),      // scope
 *                      filter.getFilter(),     // completed filter
 *                      null,                   // all attribs
 *                      false );                // attrsOnly?
 *     }
 *     <p>
 *
 *     // ...more processing here...
 * } catch ( BadFilterException e ) {
 *      System.out.println ( e.toString() );
 *      System.exit ( 0 );
 * } catch ( IOException e ) {
 *     // ...handle exception here...
 * }
 * </pre>
 *
 *
 * @see LDAPFilterList
 * @see LDAPFilter
 * @version 1.0
 */
public class LDAPFilterDescriptor {

    private Vector m_vFilterSet = new Vector();

    private String m_strLine;
    private int m_nLine;

    private String m_strPrefix;
    private String m_strAffix;
    private LDAPIntFilterSet m_tmpFilterSet = null;
    private String m_strLastMatchPattern = null;
    private String m_strLastDelimiter = null;

    /**
     * The Default scope is used when a scope is not defined
     * in the filter file.  The scope is the only "optional" parameter
     * in the file.
     */
    private static final int DEFAULT_SCOPE = LDAPConnection.SCOPE_SUB;

    /**
     * Creates an LDAPFilterDescriptor object from an existing filter
     * configuration file.  This file has the format as defined in the
     * ldapfilter.conf(5) man page.
     *
     * @exception netscape.ldap.util.BadFilterException
     *    One of the filters was not generated properly.  Most likely
     *    this is due to an improperly formatted ldapfilter.conf file.
     */
    public LDAPFilterDescriptor ( String strFile )
            throws FileNotFoundException, BadFilterException {
        DataInputStream inputStream =
        new DataInputStream ( new FileInputStream ( strFile ) );
        BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
        init( reader );
    }

    /**
     * Creates an LDAPFilterDescriptor object from an existing
     * StringBuffer.  This file has the format as defined in the
     * ldapfilter.conf(5) man page.
     *
     * @exception netscape.ldap.util.BadFilterException
     *    One of the filters was not generated properly.  Most likely
     *    this is due to an improperly formatted ldapfilter.conf file.
     */
    public LDAPFilterDescriptor ( StringBuffer strBuffer )
            throws BadFilterException {
        init( strBuffer );
    }

    /**
     * Creates an LDAPFilterDescriptor object from a URL.
     * This file has the format as defined in the
     * ldapfilter.conf(5) man page.
     *
     * @exception netscape.ldap.util.BadFilterException
     *    One of the filters was not generated properly.  Most likely
     *    this is due to an improperly formatted ldapfilter.conf file.
     */
    public LDAPFilterDescriptor ( URL url )
            throws IOException, BadFilterException {
        URLConnection urlc = url.openConnection();
        DataInputStream inputStream =
        new DataInputStream ( urlc.getInputStream() );
        BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
        init( reader );
    }



    /**
     * This function initializes the LDAPFilterDescriptor.  It's
     * called internally, and should never be called directly by the
     * developer.
     */
    private void init ( Object inputObj)
            throws BadFilterException {

        String strCommentPattern = "^\\s*#|$";
        String strDataPattern = "\\s*(?:\"([^\"]*)\")|([^\\s]*)\\s*";
        Pattern patComment;
        Pattern patData;
        Vector vStrings = new Vector ( 5 );

        try {
            patComment = Pattern.compile ( strCommentPattern );
            patData = Pattern.compile ( strDataPattern );
        } catch ( PatternSyntaxException e ) {
            // This should NEVER happen...
            System.out.println ( "FATAL Error, couldn't compile pattern");
            System.out.println ( "  " + e.getMessage() );
            return;
        }

        // Setup some temporary variables.
        m_nLine = 0;
        try {
            if (inputObj instanceof StringBuffer) {
                StringBuffer ibuffer = (StringBuffer)inputObj;
                StringBuffer buffer = new StringBuffer();
                for (int i=0; i<ibuffer.length(); i++) {
                    if (ibuffer.charAt(i) == '\n') {
                        m_strLine = buffer.toString();
                        m_nLine++;
                        setFilter(patComment, patData, vStrings);
                        buffer = new StringBuffer();
                    } else
                        buffer.append(ibuffer.charAt(i));
                    }
            } else {
                while ((m_strLine=((BufferedReader)inputObj).readLine()) != null) {
                    m_nLine++;
                    setFilter(patComment, patData, vStrings);
                }
            }

            // BUGBUG: Fixed. 7-28-97
            // We're done with the input, so we need to append the
            // last temporary FilterSet into the list of FilterSets.
            if ( m_tmpFilterSet != null ) {
                m_vFilterSet.addElement ( m_tmpFilterSet );
            }

        } catch ( IOException e ) {
        }
    }

    private void setFilter(Pattern patComment, Pattern patData,
      Vector vStrings) throws IOException, BadFilterException {
        Matcher dataMatcher;
        LDAPFilter tmpFilter = null;

        if ( ! patComment.matcher(m_strLine).lookingAt() ) {
//        	System.out.println("comment pattern " + patComment.pattern() +
//        					   " does not match " + m_strLine);
        	dataMatcher = patData.matcher(m_strLine);
            // System.out.println ( "\nNEW LINE: " + m_strLine );
            if ( ! vStrings.isEmpty() ) {
                vStrings.removeAllElements();
            }

            while ( dataMatcher.find() ) {
                // Within this while loop, we're looking for
                // all the data tokens.  Our regular
                // expression is setup to look for words
                // separated by whitespace or sets of
                // characters in quotataion marks.  A remnant
                // of the regexp is that we have two
                // backreferences, only one will have data at
                // any time.
            	int groupCount = dataMatcher.groupCount();
                for ( int i = 1; i <= groupCount; i++ ) {
                    if ( dataMatcher.group(i) != null ) {
                        if ( ! dataMatcher.group(i).equals ( "" ) ) {
//                            System.out.println ( "Match #" + i +
//                            		": \"" + dataMatcher.group(i) + "\"" );
                            vStrings.addElement ( dataMatcher.group(i));
                        }
                    }
                }
            }

            switch ( vStrings.size() ) {
            case 1:
                // If the current filter set is not null,
                // add it to the filter set vector.
                if ( m_tmpFilterSet != null ) {
                    m_vFilterSet.addElement ( m_tmpFilterSet );
                }

                // Now create a new filterset.
                m_tmpFilterSet = new LDAPIntFilterSet
                    ( (String)vStrings.elementAt ( 0 ) );

            break;

                // Two tokens mens we're the the second (or
                // higher line in a token.  We need to append
                // the information stored in the list onto
                // the tmpFilter.
            case 2:

                if ( ( m_strLastMatchPattern != null ) &&
                    ( m_strLastDelimiter != null ) ) {
                    tmpFilter = new LDAPFilter(
                    m_strLastMatchPattern,
                    m_strLastDelimiter,
                    (String)vStrings.elementAt ( 0 ),
                    (String)vStrings.elementAt ( 1 ),
                    DEFAULT_SCOPE );
                    tmpFilter.setLine ( m_nLine );
                    if ( m_tmpFilterSet != null ) {
                        m_tmpFilterSet.appendFilter ( tmpFilter );
                    } else {
                        throw MakeException ( "Attempting to add a filter to a null filterset" );
                    }
                } else {
                    throw MakeException ( "Attempting to create a relative filter with no preceeding full filter" );
                }
            break;

            // Three tokens means we're the second (or
            // higher line in a filter.  create a new
            // filter grabbing info from the last filter
            case 3:

                if ( ( m_strLastMatchPattern != null ) &&
                ( m_strLastDelimiter != null ) ) {
                    tmpFilter = new LDAPFilter (
                        m_strLastMatchPattern,
                        m_strLastDelimiter,
                        (String)vStrings.elementAt ( 0 ),
                        (String)vStrings.elementAt ( 1 ),
                        (String)vStrings.elementAt ( 2 ) );
                    tmpFilter.setLine ( m_nLine );


                    if ( m_tmpFilterSet != null ) {
                        m_tmpFilterSet.appendFilter ( tmpFilter );
                    } else {
                        throw MakeException
                            ("Attempting to add a filter to a null filterset");
                    }

                } else {
                    throw MakeException
                        ("Attempting to create a relative filter with no preceeding full filter" );
                }
            break;

            // 4 tokens means this is the first line in a
            // token.   All data is new.  However, we're using
            // the default scope.
            case 4:
                tmpFilter = new LDAPFilter (
                    (String)vStrings.elementAt ( 0 ),
                    (String)vStrings.elementAt ( 1 ),
                    (String)vStrings.elementAt ( 2 ),
                    (String)vStrings.elementAt ( 3 ),
                    DEFAULT_SCOPE );
                tmpFilter.setLine ( m_nLine );
                m_strLastMatchPattern = (String)vStrings.elementAt ( 0 );
                m_strLastDelimiter = (String)vStrings.elementAt ( 1 );
                if ( m_tmpFilterSet != null ) {
                    m_tmpFilterSet.newFilter ( tmpFilter );
                } else {
                    throw MakeException
                        ("Attempting to add a filter to a null filterset");
                }
            break;

            // 5 tokens means this is the first line in a
            // token.   All data is new.
            case 5:
                tmpFilter = new LDAPFilter (
                    (String)vStrings.elementAt ( 0 ),
                    (String)vStrings.elementAt ( 1 ),
                    (String)vStrings.elementAt ( 2 ),
                    (String)vStrings.elementAt ( 3 ),
                    (String)vStrings.elementAt ( 4 ) );
                tmpFilter.setLine ( m_nLine );
                m_strLastMatchPattern = (String)vStrings.elementAt ( 0 );
                m_strLastDelimiter = (String)vStrings.elementAt ( 1 );
                if ( m_tmpFilterSet != null ) {
                    m_tmpFilterSet.newFilter ( tmpFilter );
                } else {
                    throw MakeException
                        ("Attempting to add a filter to a null filterset");
                }

            break;

            default:
                throw MakeException ( "Wrong number of tokens (" + vStrings.size() + ")" );

            //break;

            }
        }
    }

    /**
     * just a utility method to create an exception.
     */
    private BadFilterException MakeException ( String strMsg ) {
        return new BadFilterException
          ( "BadFilterException while creating Filters,\n" +
            "Line Number: " + m_nLine +
            ",\n --> " + m_strLine + "\nThe error is: " +
            strMsg, m_nLine );
    }

        /**
     * Output a text dump of this filter descriptor.  It cycles
     * through all of the internal LDAPIntFilterSet objects and calls
     * their toString() methods.
     *
     * @see LDAPIntFilterSet#toString
     */
    public String toString() {
        StringBuffer strBuf = new StringBuffer ( 4000 );
        for ( int i = 0; i < m_vFilterSet.size(); i++ ) {
            strBuf.append ( "Filter Set number: " + i + "\n" );
            strBuf.append (
                ((LDAPIntFilterSet)m_vFilterSet.elementAt ( i )).toString() +
            "\n" );
            strBuf.append ( "\n" );
            //System.out.println ( (m_vFilterSet.elementAt ( i )).toString());
        }
        return strBuf.toString();
    }

    /**
     * Return all the filters which match the strTagPat (regular
     * expression), and the user input (strValue)
     */
    public LDAPFilterList getFilters ( String strTagPat, String strValue )
        throws IllegalArgumentException {

        strTagPat = strTagPat.trim();
        strValue = strValue.trim();

        if ( ( strTagPat == null ) || ( strTagPat.equals ("") ) ) {
            throw new IllegalArgumentException
                    ( "The Tag Pattern can not be null" );
        }

        if ( ( strValue == null ) || ( strValue.equals ("") ) ) {
            throw new IllegalArgumentException ( "The Value can not be null" );
        }

        LDAPFilterList retList = new LDAPFilterList();

        Pattern patTag;    // The strTagPat that's compiled

        // first we need to make a new regexp from the strTagPat
        // For efficiency, we're precompiling the strTagPat into
        // a pattern here.  That pattern doesn't change, the Tag string
        // changes per LDAPFIlterSet.
        try {
            patTag = Pattern.compile ( strTagPat );
        } catch ( PatternSyntaxException e ) {
            throw new IllegalArgumentException
            ( "The parameter: " + strTagPat + " is not valid" );
        }

        // We "ask" each of the filterset's to see if there is
        // a matching filter.
        boolean bMatched = false;
        int i = 0;
        while ( ! bMatched ) {
            Vector vMatchingFilters =
            ((LDAPIntFilterSet)m_vFilterSet.elementAt ( i )).getFilters
                (patTag, strValue );

            if ( vMatchingFilters.size() > 0 ) {
                for ( int j = 0; j < vMatchingFilters.size(); j++ ) {
                    LDAPFilter tmpFilter =
                    (LDAPFilter)
                        ((LDAPFilter)
                        vMatchingFilters.elementAt ( j )).clone();
                    tmpFilter.setupFilter ( strValue, m_strPrefix,
                                  m_strAffix );
                    bMatched = true; // this really doesn't matter.
                    retList.add ( tmpFilter );
                }
                return retList;
            }
            i++;
        }
        return null;
    }


        /**
     * Prepend the parameter (strPrefix) and append the second
     * parameter (strAffix) to every filter that is returned by the
     * getFilters() method.  <p>
     */
    public void setFilterAffixes ( String strPrefix, String strAffix ) {
        m_strPrefix = strPrefix;
        m_strAffix = strAffix;
    }
}

