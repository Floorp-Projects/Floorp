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
package netscape.ldap;

import java.util.*;
import java.io.*;
import java.net.MalformedURLException;

/**
 * Represents an LDAP URL. The complete specification for LDAP URLs is in
 * <A HREF="http://ds.internic.net/rfc/rfc1959.txt"
 * TARGET="_blank">RFC 1959</A>. In addition, the secure ldap (ldaps://) is also
 * supported. LDAP URLs have the following format:
 *
 * <PRE>
 * "ldap[s]://" [ <I>hostName</I> [":" <I>portNumber</I>] ] "/"
 *                      <I>distinguishedName</I>
 *          ["?" <I>attributeList</I> ["?" <I>scope</I>
 *                      "?" <I>filterString</I> ] ]
 * </PRE>
 * where
 * <P>
 * <UL>
 * <LI>all text within double-quotes are literal<P>
 * <LI><CODE><I>hostName</I></CODE> and <CODE><I>portNumber</I></CODE>
 * identify the location of the LDAP server.<P>
 * <LI><CODE><I>distinguishedName</I></CODE> is the name of an entry
 * within the given directory (the entry represents the starting point
 * of the search)<P>
 * <LI><CODE><I>attributeList</I></CODE> contains a list of attributes
 * to retrieve (if null, fetch all attributes). This is a comma-delimited
 * list of attribute names.<P>
 * <LI><CODE><I>scope</I></CODE> is one of the following:
 * <UL>
 * <LI><CODE>base</CODE> indicates that this is a search only for the
 * specified entry
 * <LI><CODE>one</CODE> indicates that this is a search for matching entries
 * one level under the specified entry (and not including the entry itself)
 * <LI><CODE>sub</CODE> indicates that this is a search for matching entries
 * at all levels under the specified entry (including the entry itself)
 * </UL>
 * <P>
 * If not specified, <CODE><I>scope</I></CODE> is <CODE>base</CODE> by
 * default. <P>
 * <LI><CODE><I>filterString</I></CODE> is a human-readable representation
 * of the search criteria. This value is used only for one-level or subtree
 * searches.<P>
 * </UL>
 * <P>
 * Note that if <CODE><I>scope</I></CODE> and <CODE><I>filterString</I></CODE>
 * are not specified, an LDAP URL identifies exactly one entry in the
 * directory. <P>
 * The same encoding rules for other URLs (e.g. HTTP) apply for LDAP
 * URLs.  Specifically, any "illegal" characters are escaped with
 * <CODE>%<I>HH</I></CODE>, where <CODE><I>HH</I></CODE> represent the
 * two hex digits which correspond to the ASCII value of the character.
 * This encoding is only legal (or necessary) on the DN and filter portions
 * of the URL.
 *
 * @version 1.0
 */
public class LDAPUrl implements java.io.Serializable {

    static final long serialVersionUID = -3245440798565713641L;
    public static String defaultFilter = "(objectClass=*)";

    private String m_hostName;
    private int m_portNumber;
    private String m_DN;
    private Vector m_attributes;
    private int m_scope;
    private String m_filter;
    private String m_URL;
    private boolean m_secure;
    
    private static LDAPSocketFactory m_factory;

    /**
     * The default port number for secure LDAP connections.
     * @see netscape.ldap.LDAPUrl#LDAPUrl(java.lang.String, int, java.lang.String, java.lang.String[], int, java.lang.String, boolean)
     */    
    public static final int DEFAULT_SECURE_PORT = 636;

    /**
     * Constructs a URL object with the specified string as URL.
     * @param url LDAP search expression in URL form
     * @exception MalformedURLException failed to parse URL
     */
    public LDAPUrl (String url) throws java.net.MalformedURLException {
        m_attributes = null;
        m_scope = LDAPv2.SCOPE_BASE;
        m_filter = defaultFilter;
        m_URL = url;

        parseUrl(url);
    }

    /**
     * Parse URL as defined in RFC 1959. Beyond the RFC, the secure ldap
     * (ldaps) is also supported.
     */
    private void parseUrl(String url) throws MalformedURLException {
        StringTokenizer urlParser = new StringTokenizer (url, ":/?", true);
        String currentToken;
        String str = null;

        try {
            currentToken = urlParser.nextToken();
            if (currentToken.equalsIgnoreCase ("LDAPS")) {
                m_secure = true;
            }
            else if (!currentToken.equalsIgnoreCase ("LDAP")) {
                throw new MalformedURLException ();
            }

            currentToken = urlParser.nextToken();
            if (!currentToken.equals(":")) {
                throw new MalformedURLException ();
            }
            currentToken = urlParser.nextToken();
            if (!currentToken.equals("/")) {
                throw new MalformedURLException ();
            }
            currentToken = urlParser.nextToken();
            if (!currentToken.equals("/")) {
                throw new MalformedURLException ();
            }
        
            currentToken = urlParser.nextToken();
        }
        catch (NoSuchElementException e) {
            throw new MalformedURLException ();
        }
            
        // host-port
        if (currentToken.equals ("/")) {
            m_hostName = null;
            m_portNumber = m_secure ? DEFAULT_SECURE_PORT : LDAPv2.DEFAULT_PORT;
        } else if (currentToken.equals (":")) {
                // port number without host name is not allowed
               throw new MalformedURLException ("No hostname");
        } else if (currentToken.equals ("?")) {
            throw new MalformedURLException ("No host[:port]");
        } else {
            m_hostName = currentToken;
            if (urlParser.countTokens() == 0) {
                m_portNumber = m_secure ? DEFAULT_SECURE_PORT : LDAPv2.DEFAULT_PORT;
                return;
            }
            currentToken = urlParser.nextToken (); // either ":" or "/"

            if (currentToken.equals (":")) {
                try {
                    m_portNumber = Integer.parseInt (urlParser.nextToken());
                } catch (NumberFormatException nf) {
                    throw new MalformedURLException ("Port not a number");
                } catch (NoSuchElementException ex) {
                    throw new MalformedURLException ("No port number");
                }
                    
                if (urlParser.countTokens() == 0) {
                    return;
                }
                else if (! urlParser.nextToken().equals("/")) {
                   throw new MalformedURLException ();
                }

            } else if (currentToken.equals ("/")) {
                m_portNumber = m_secure ? DEFAULT_SECURE_PORT : LDAPv2.DEFAULT_PORT;
            } else {
                // expecting ":" or "/"
                throw new MalformedURLException ();
            }
        }


        // DN
        if (!urlParser.hasMoreTokens ()) {
            return;
        }
        m_DN = decode(readNextConstruct(urlParser));
        if (m_DN.equals("?")) {
            m_DN = "";
        }
        else if (m_DN.equals("/")) {
            throw new MalformedURLException ();
        }            
            
        // attribute
        if (!urlParser.hasMoreTokens ()) {
            return;
        }        
        str = readNextConstruct(urlParser);
        if (!str.equals("?")) {
            StringTokenizer attributeParser = new
                StringTokenizer (decode(str), ", ");
            m_attributes = new Vector ();

            while (attributeParser.hasMoreTokens()) {
                m_attributes.addElement (attributeParser.nextToken());
            }
        }

        // scope
        if (!urlParser.hasMoreTokens ()) {
            return;
        }
        str = readNextConstruct(urlParser);
        if (!str.equals("?")) {
            m_scope = getScope(str);
            if (m_scope < 0) {
                throw new MalformedURLException("Bad scope:" + str);
            }
        }

        // filter
        if (!urlParser.hasMoreTokens ()) {
            return;
        }
        str = readNextConstruct(urlParser);
        m_filter = decode(str);
        checkBalancedParentheses(m_filter);
        if (!m_filter.startsWith("(") && !m_filter.endsWith(")")) {
            m_filter = "(" + m_filter + ")";
        }

        // Nothing after the filter is allowed
        if (urlParser.hasMoreTokens()) {
            throw new MalformedURLException();
        }
    }    
    
    private void checkBalancedParentheses(String filter) throws MalformedURLException {
        int parenCnt =0;
        StringTokenizer filterParser = new StringTokenizer (filter, "()", true);
        while (filterParser.hasMoreElements()) {
            String token = filterParser.nextToken();
            if (token.equals("(")) {
                parenCnt++;
            }
            else if (token.equals(")")) {
                if (--parenCnt < 0) {
                    throw new MalformedURLException("Unbalanced filter parentheses");
                }
            }
        }
        
        if (parenCnt != 0) {
            throw new MalformedURLException("Unbalanced filter parentheses");
        }
    }
        
    /**
     * Constructs with the specified host, port, and DN.  This form is used to
     * create URL references to a particular object in the directory.
     * @param host host name of the LDAP server, or null for "nearest X.500/LDAP"
     * @param port port number of the LDAP server (use LDAPv2.DEFAULT_PORT for
     * the default port)
     * @param DN distinguished name of the object
     */
    public LDAPUrl (String host, int port, String DN) {
        initialize(host, port, DN, null, LDAPv2.SCOPE_BASE, defaultFilter, false);
    }

    /**
     * Constructs a full-blown LDAP URL to specify an LDAP search operation.
     * @param host host name of the LDAP server, or null for "nearest X.500/LDAP"
     * @param port port number of the LDAP server (use LDAPv2.DEFAULT_PORT for
     * the default port)
     * @param DN distinguished name of the object
     * @param attributes list of attributes to return. Use null for "all
     * attributes."
     * @param scope depth of search (in DN namespace). Use one of the LDAPv2 scopes:
     * SCOPE_BASE, SCOPE_ONE, or SCOPE_SUB.
     * @param filter LDAP filter string (as defined in RFC 1558). Use null for
     * no filter (this effectively makes the URL reference a single object).
     */
    public LDAPUrl (String host, int port, String DN,
        String attributes[], int scope, String filter) {

        this(host, port, DN, attributes, scope, filter, false);
    }

    /**
     * Constructs a full-blown LDAP URL to specify an LDAP search operation.
     * @param host host name of the LDAP server, or null for "nearest X.500/LDAP"
     * @param port port number of the LDAP server (use LDAPv2.DEFAULT_PORT for
     * the default port)
     * @param DN distinguished name of the object
     * @param attributes list of the attributes to return. Use null for "all
     * attributes."
     * @param scope depth of the search (in DN namespace). Use one of the LDAPv2 scopes: 
     * SCOPE_BASE, SCOPE_ONE, or SCOPE_SUB.
     * @param filter LDAP filter string (as defined in RFC 1558). Use null for
     * no filter (this effectively makes the URL reference a single object).
     */
    public LDAPUrl (String host, int port, String DN,
      Enumeration attributes, int scope, String filter) {

        initialize(host, port, DN, attributes, scope, filter, false);
    }

    /**
     * Constructs a full-blown LDAP URL to specify an LDAP search operation.
     * @param host host name of the LDAP server, or null for "nearest X.500/LDAP"
     * @param port port number of the LDAP server (use LDAPv2.DEFAULT_PORT for
     * the default non-secure port or LDAPUrl.DEFAULT_SECURE_PORT for the default
     * secure port)
     * @param DN distinguished name of the object
     * @param attributes list of the attributes to return. Use null for "all
     * attributes."
     * @param scope depth of the search (in DN namespace). Use one of the LDAPv2 scopes: 
     * SCOPE_BASE, SCOPE_ONE, or SCOPE_SUB.
     * @param filter LDAP filter string (as defined in RFC 1558). Use null for
     * no filter (this effectively makes the URL reference a single object).
     * @param secure flag if secure ldap protocol (ldaps) is to be used.
     */
    public LDAPUrl (String host, int port, String DN,
      String[] attributes, int scope, String filter, boolean secure) {

        if (attributes != null) {
            Vector list = new Vector();
            for (int k = 0; k < attributes.length; k++) {
                list.addElement(attributes[k]);
            }
            initialize(host, port, DN, list.elements(), scope, filter, secure);
        } else {
            initialize(host, port, DN, null, scope, filter, secure);
        }
    }


    /**
     * Initializes URL object.
     */
    private void initialize (String host, int port, String DN,
      Enumeration attributes, int scope, String filter, boolean secure) {

        m_hostName = host;
        m_DN = DN;
        m_portNumber = port;
        m_filter = (filter != null) ? filter : defaultFilter;
        m_scope = scope;
        m_secure = secure;

        if (attributes != null) {
            m_attributes = new Vector ();
            while (attributes.hasMoreElements()) {
                m_attributes.addElement (attributes.nextElement());
            }
        } else
            m_attributes = null;

        StringBuffer url = new StringBuffer (secure ? "ldaps://" :"ldap://");

        if (host != null) {
            url.append (host);
            url.append (':');
            url.append (String.valueOf (port));
        }

        url.append ('/');
        url.append (encode (DN));

        if (attributes != null) {
            url.append ('?');
            Enumeration attrList = m_attributes.elements();
            boolean firstElement = true;

            while (attrList.hasMoreElements()) {
                if (!firstElement)
                    url.append (',');
                else
                    firstElement = false;

                url.append ((String)attrList.nextElement());
            }
        }

        if (filter != null) {
            if (attributes == null)
                url.append ('?');

            url.append ('?');

            switch (scope) {
              default:
              case LDAPv2.SCOPE_BASE:
                url.append ("base"); break;
              case LDAPv2.SCOPE_ONE:
                url.append ("one"); break;
              case LDAPv2.SCOPE_SUB:
                url.append ("sub"); break;
            }

            url.append ('?');
            url.append (filter);
        }

        m_URL = url.toString();
    }

    /**
     * Return the host name of the LDAP server
     * @return LDAP host.
     */
    public String getHost () {
        return m_hostName;
    }

    /**
     * Return the port number for the LDAP server
     * @return port number.
     */
    public int getPort () {
        return m_portNumber;
    }

    /**
     * Return the distinguished name encapsulated in the URL
     * @return target distinguished name.
     */
    public String getDN () {
        return m_DN;
    }

    /**
     * Return the server part of the ldap url, ldap://host:port
     * @return server url.
     */
    String getServerUrl() {
        return (m_secure ? "ldaps://" : "ldap://") +
                m_hostName + ":" + m_portNumber;
    }

    /**
     * Return the collection of attributes specified in the URL, or null
     * for "every attribute"
     * @return enumeration of attributes.
     */
    public Enumeration getAttributes () {
        if (m_attributes == null)
            return null;
        else
            return m_attributes.elements();
    }

    /**
     * Return the collection of attributes specified in the URL, or null
     * for "every attribute"
     * @return string array of attributes.
     */
    public String[] getAttributeArray () {
        if (m_attributes == null)
            return null;
        else {
            String[] attrNames = new String[m_attributes.size()];
            Enumeration attrs = getAttributes();
            int i = 0;

            while (attrs.hasMoreElements())
                attrNames[i++] = (String)attrs.nextElement();

            return attrNames;
        }
    }

    /**
     * Returns the scope of the search, according to the values
     * SCOPE_BASE, SCOPE_ONE, SCOPE_SUB as defined in LDAPv2.  This refers
     * to how deep within the directory namespace the search will look
     * @return search scope.
     */
    public int getScope () {
        return m_scope;
    }

    /**
     * Returns the scope of the search. If the scope returned is -1, then
     * the given string is not for the scope.
     * @param str the string against which to compare the scope type
     * @returns the scope of the search, -1 is returned if the given string is
     * not SUB, ONE or BASE (the acceptable LDAPv2 values for scope).
     */
    private int getScope(String str) {

        int s = -1;
        if (str.equalsIgnoreCase("base"))
            s = LDAPv2.SCOPE_BASE;
        else if (str.equalsIgnoreCase("one"))
            s = LDAPv2.SCOPE_ONE;
        else if (str.equalsIgnoreCase("sub"))
            s = LDAPv2.SCOPE_SUB;

        return s;
    }

    /**
     * Returns the search filter (RFC 1558), or the default if none was
     * specified.
     * @return the search filter.
     */
    public String getFilter () {
        return m_filter;
    }

    /**
     * Returns a valid string representation of this LDAP URL.
     * @return the LDAP search expression in URL form.
     */
    public String getUrl () {
        return m_URL;
    }

    /**
     * Returns true if the secure ldap protocol is used.
     * @return true if ldaps is used.
     */
    public boolean isSecure() {
        return m_secure;
    }

    /**
     * Gets the socket factory to be used for ldaps:// URLs.
     * <P>
     * If the factory is not explicitly specified with
     * <CODE>LDAPUrl.setSocketFactory</CODE>, the method will
     * attempt the determine the default factory based on the
     * available factories in the netscape.ldap.factory package.
     *
     * @return the socket factory to be used for ldaps:// URLs
     */
    public static LDAPSocketFactory getSocketFactory() {

        if (m_factory == null) {

            // No factory explicitly set, try to determine
            // the default one.
            try {
                //  First try Mozilla JSSSocketFactory
                Class c = Class.forName("netscape.ldap.factory.JSSSocketFactory");
                m_factory = (LDAPSocketFactory) c.newInstance();
            }
            catch (Throwable e) {
            }

            if (m_factory != null) {
                return m_factory;
            }

            try {
                // then try Sun JSSESocketFactory
                Class c = Class.forName("netscape.ldap.factory.JSSESocketFactory");
                m_factory = (LDAPSocketFactory) c.newInstance();
            }
            catch (Throwable e) {
            }
        }

        return m_factory;
    }

    /**
     * Sets the socket factory to be used for ldaps:// URLs.
     * Overrides the default factory assigned by the LDAPUrl
     * class.
     * @param the socket factory to be used for ldaps:// URLs
     * @see netscape.ldap.LDAPUrl#getSocketFactory
     */
    public static void setSocketFactory(LDAPSocketFactory factory) {
        m_factory = factory;
    }
            
    /**
     * Reads next construct from the given string parser.
     * @param parser the string parser
     * @return the next construct which can be an attribute, scope or filter.
     * @exception java.net.MalformedURLException Get thrown when the url format
     *            is incorrect.
     */
    private String readNextConstruct(StringTokenizer parser) throws
        MalformedURLException {

        try {
            if (parser.hasMoreTokens()) {
                String tkn = parser.nextToken();
                if (tkn.equals("?")) { // empty construct
                    return tkn;
                }
                else if (parser.hasMoreTokens()){
                    // Remove '?' delimiter
                    String delim = parser.nextToken();
                    if (!delim.equals("?")) {
                        throw new MalformedURLException();
                    }
                }
                
                return tkn;
            }
        } catch (NoSuchElementException e) {
            throw new MalformedURLException();
        }

        return null;
    }

    /**
     * Parses hex character into integer.
     */
    private static int hexValue (char hexChar) throws MalformedURLException {
        if (hexChar >= '0' && hexChar <= '9')
            return hexChar - '0';
        if (hexChar >= 'A' && hexChar <= 'F')
            return hexChar - 'A' + 10;
        if (hexChar >= 'a' && hexChar <= 'f')
            return hexChar - 'a' + 10;

        throw new MalformedURLException ();
    }

    private static char hexChar (int hexValue) {
        if (hexValue < 0 || hexValue > 0xF)
            return 'x';

        if (hexValue < 10)
            return (char)(hexValue + '0');

        return (char)((hexValue - 10) + 'a');
    }

    /**
     * Decodes a URL-encoded string. Any occurences of %HH are decoded to the
     * hex value represented.  However, this routine does NOT decode "+"
     * into " ". See RFC 1738 for full details about URL encoding/decoding.
     * @param URLEncoded a segment of a URL which was encoded using the URL
     * encoding rules
     * @exception MalformedURLException failed to parse URL
     */
    public static String decode (String URLEncoded) throws
        MalformedURLException {
        StringBuffer decoded = new StringBuffer (URLEncoded);
        int srcPos = 0, dstPos = 0;

        try {
            while (srcPos < decoded.length()) {
                if (decoded.charAt (srcPos) != '%') {
                    if (srcPos != dstPos)
                        decoded.setCharAt (dstPos, decoded.charAt (srcPos));
                    srcPos++;
                    dstPos++;
                    continue;
                }
                decoded.setCharAt (dstPos, (char)
                  ((hexValue(decoded.charAt (srcPos+1))<<4) |
                  (hexValue(decoded.charAt (srcPos+2)))));
                dstPos++;
                srcPos += 3;
            }
        } catch (StringIndexOutOfBoundsException sioob) {
            // Indicates that a "%" character occured without the following HH
            throw new MalformedURLException ();
        }

        /* 070497 Url problems submitted by Netscape */
        /* decoded.setLength (dstPos+1); */
        decoded.setLength (dstPos);
        return decoded.toString ();
    }

    /**
     * Encodes an arbitrary string. Any illegal characters are encoded as
     * %HH.  However, this routine does NOT decode "+" into " " (this is a HTTP
     * thing, not a general URL thing).  Note that, because Sun's URLEncoder
     * does do this encoding, we can't use it.
     * See RFC 1738 for full details about URL encoding/decoding.
     * @param toEncode an arbitrary string to encode for embedding within a URL
     */
    public static String encode (String toEncode) {
      StringBuffer encoded = new StringBuffer (toEncode.length()+10);

        for (int currPos = 0; currPos < toEncode.length(); currPos++) {
            char currChar = toEncode.charAt (currPos);
            if ((currChar >= 'a' && currChar <= 'z') ||
              (currChar >= 'A' && currChar <= 'Z') ||
              (currChar >= '0' && currChar <= '9') ||
              ("$-_.+!*'(),".indexOf (currChar) > 0)) {
                // this is the criteria for "doesn't need to be encoded" (whew!)
                encoded.append (currChar);
            } else {
                encoded.append ("%");
                encoded.append (hexChar ((currChar & 0xF0) >> 4));
                encoded.append (hexChar (currChar & 0x0F));
            }
        }

        return encoded.toString();
    }

    /**
     * Returns the URL in String format
     *
     * @return the URL in String format
     */
    public String toString() {
        return getUrl();
    }

    /**
     * Reports if the two objects represent the same URL
     *
     * @param url the object to be compared to
     * @return <CODE>true</CODE> if the two are equivalent
     */
    public boolean equals( LDAPUrl url ) {
        if ( getHost() == null ) {
            if ( url.getHost() != null ) {
                return false;
            }
        } else if ( !getHost().equals( url.getHost() ) ) {
            return false;
        }
        if ( getPort() != url.getPort() ) {
            return false;
        }
        if ( getDN() == null ) {
            if ( url.getDN() != null ) {
                return false;
            }
        } else if ( !getDN().equals( url.getDN() ) ) {
            return false;
        }
        if ( getFilter() == null ) {
            if ( url.getFilter() != null ) {
                return false;
            }
        } else if ( !getFilter().equals( url.getFilter() ) ) {
            return false;
        }
        if ( getScope() != url.getScope() ) {
            return false;
        }

        if ( m_attributes == null ) {
            if ( url.m_attributes != null ) {
                return false;
            }
        } else if ( m_attributes.size() != url.m_attributes.size() ) {
            return false;
        } else {
            for( int i = 0; i < m_attributes.size(); i++ ) {
                if ( m_attributes.elementAt( i ) !=
                     url.m_attributes.elementAt( i ) ) {
                    return false;
                }
            }
        }
        return true;
    }
}
