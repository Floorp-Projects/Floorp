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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.util.*;
import java.io.*;
import java.net.MalformedURLException;
import org.ietf.ldap.factory.*;

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

    static final long serialVersionUID = -3245440798565713640L;
    public static String defaultFilter = "(objectClass=*)";

    private String _hostName;
    private int _portNumber;
    private String _DN;
    private Vector _attributes;
    private int _scope;
    private String _filter;
    private String _URL;
    private boolean _secure;
    private String[] _extensions;
    
    private static LDAPSocketFactory _factory;

    /**
     * The default port number for secure LDAP connections.
     * @see org.ietf.ldap.LDAPUrl#LDAPUrl(String, int, String, String[], int,
     * String, String[])
     */    
    public static final int DEFAULT_SECURE_PORT = 636;

    /**
     * Constructs a URL object with the specified string as URL.
     * @param url LDAP search expression in URL form
     * @exception MalformedURLException failed to parse URL
     */
    public LDAPUrl( String url ) throws java.net.MalformedURLException {
        _attributes = null;
        _scope = LDAPConnection.SCOPE_BASE;
        _filter = defaultFilter;
        _URL = url;

        parseUrl(url);
    }

    /**
     * Constructs with the specified host, port, and DN.  This form is used to
     * create URL references to a particular object in the directory.
     * @param host host name of the LDAP server, or null for "nearest X.500/LDAP"
     * @param port port number of the LDAP server (use LDAPConnection.DEFAULT_PORT for
     * the default port)
     * @param DN distinguished name of the object
     */
    public LDAPUrl( String host,
                    int port,
                    String DN ) {
        initialize( host, port, DN, null,
                    LDAPConnection.SCOPE_BASE,
                    defaultFilter,
                    null );
    }

    /**
     * Constructs a full-blown LDAP URL to specify an LDAP search operation.
     * @param host host name of the LDAP server, or null for "nearest X.500/LDAP"
     * @param port port number of the LDAP server (use LDAPConnection.DEFAULT_PORT for
     * the default non-secure port or LDAPUrl.DEFAULT_SECURE_PORT for the default
     * secure port)
     * @param DN distinguished name of the object
     * @param attributes list of the attributes to return. Use null for "all
     * attributes."
     * @param scope depth of the search (in DN namespace). Use one of the LDAPConnection scopes: 
     * SCOPE_BASE, SCOPE_ONE, or SCOPE_SUB.
     * @param filter LDAP filter string (as defined in RFC 1558). Use null for
     * no filter (this effectively makes the URL reference a single object).
     * @param extensions LDAP URL extensions specified; may be null or 
     * empty. Each extension is a type=value expression. 
     * The =value part MAY be omitted. The expression 
     * may be prefixed with '!' if it is mandatory for 
     * evaluation of the URL.
     */
    public LDAPUrl( String host,
                    int port,
                    String DN,
                    String[] attributes,
                    int scope,
                    String filter,
                    String[] extensions ) {

        if ( attributes != null ) {
            Vector list = new Vector();
            for ( int k = 0; k < attributes.length; k++ ) {
                list.addElement(attributes[k]);
            }
            initialize( host, port, DN, list.elements(), scope, filter,
                        extensions );
        } else {
            initialize( host, port, DN, null, scope, filter,
                        extensions );
        }
    }

    /**
     * Decodes a URL-encoded string. Any occurences of %HH are decoded to the
     * hex value represented.  However, this routine does NOT decode "+"
     * into " ". See RFC 1738 for full details about URL encoding/decoding.
     * @param URLEncoded a segment of a URL which was encoded using the URL
     * encoding rules
     * @exception MalformedURLException failed to parse URL
     */
    public static String decode( String URLEncoded )
        throws MalformedURLException {
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
    public static String encode( String toEncode ) {
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
     * Return the collection of attributes specified in the URL, or null
     * for "every attribute"
     * @return string array of attributes.
     */
    public String[] getAttributeArray() {
        if ( _attributes == null ) {
            return new String[0];
        } else {
            String[] attrNames = new String[_attributes.size()];
            Enumeration attrs = getAttributes();
            int i = 0;
            while ( attrs.hasMoreElements() ) {
                attrNames[i++] = (String)attrs.nextElement();
            }
            return attrNames;
        }
    }

    /**
     * Return the collection of attributes specified in the URL, or null
     * for "every attribute"
     * @return enumeration of attributes.
     */
    public Enumeration getAttributes() {
        if ( _attributes == null ) {
            return null;
        } else {
            return _attributes.elements();
        }
    }

    /**
     * Return the distinguished name encapsulated in the URL
     * @return target distinguished name.
     */
    public String getDN() {
        return _DN;
    }

    /**
     * Returns the search filter (RFC 1558), or the default if none was
     * specified.
     * @return the search filter.
     */
    public String getFilter() {
        return _filter;
    }

    /**
     * Return the host name of the LDAP server
     * @return LDAP host.
     */
    public String getHost() {
        return _hostName;
    }

    /**
     * Return the port number for the LDAP server
     * @return port number.
     */
    public int getPort() {
        return _portNumber;
    }

    /**
     * Returns the scope of the search, according to the values
     * SCOPE_BASE, SCOPE_ONE, SCOPE_SUB as defined in LDAPConnection.  This refers
     * to how deep within the directory namespace the search will look
     * @return search scope.
     */
    public int getScope() {
        return _scope;
    }

    /**
     * Returns the scope of the search. If the scope returned is -1, then
     * the given string is not for the scope.
     * @param str the string against which to compare the scope type
     * @returns the scope of the search, -1 is returned if the given string is
     * not SUB, ONE or BASE (the acceptable LDAPConnection values for scope).
     */
    private int getScope(String str) {

        int s = -1;
        if (str.equalsIgnoreCase("base"))
            s = LDAPConnection.SCOPE_BASE;
        else if (str.equalsIgnoreCase("one"))
            s = LDAPConnection.SCOPE_ONE;
        else if (str.equalsIgnoreCase("sub"))
            s = LDAPConnection.SCOPE_SUB;

        return s;
    }

    /**
     * Gets the socket factory to be used for ldaps:// URLs.
     * <P>
     * If the factory is not explicitly specified with
     * <CODE>LDAPUrl.setSocketFactory</CODE>, the method will
     * attempt the determine the default factory based on the
     * available factories in the org.ietf.ldap.factory package.
     *
     * @return the socket factory to be used for ldaps:// URLs
     */
    public static LDAPSocketFactory getSocketFactory() {

        if (_factory == null) {

            // No factory explicity set, try to determine
            // the default one.
            try {
                //  First try iPlanet JSSSocketFactory
                Class c = Class.forName("org.ietf.ldap.factory.JSSSocketFactory");
                _factory = (LDAPSocketFactory) c.newInstance();
            }
            catch (Throwable e) {
            }

            if (_factory != null) {
                return _factory;
            }

            try {
                // then try Sun JSSESocketFactory
                _factory = new JSSESocketFactory(null);
            }
            catch (Throwable e) {
            }
        }

        return _factory;
    }

    /**
     * Returns true if the secure ldap protocol is used.
     * @return true if ldaps is used.
     */
    public boolean isSecure() {
        return _secure;
    }

    /**
     * Sets the socket factory to be used for ldaps:// URLs.
     * Overrides the default factory assigned by the LDAPUrl
     * class.
     * @param the socket factory to be used for ldaps:// URLs
     * @see org.ietf.ldap.LDAPUrl#getSocketFactory
     */
    public static void setSocketFactory( LDAPSocketFactory factory ) {
        _factory = factory;
    }

    /**
     * Sets the protocol to ldaps or ldap
     *
     * @param secure <CODE>true</CODE> for ldaps
     */
    public void setSecure( boolean secure ) {
        _secure = secure;
        initialize( _hostName, _portNumber, _DN, getAttributes(), _scope,
                    _filter, _extensions );
    }

    /**
     * Returns the URL in String format
     *
     * @return the URL in String format
     */
    public String toString() {
        return _URL;
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
                _secure = true;
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
            _hostName = null;
            _portNumber = _secure ? DEFAULT_SECURE_PORT : LDAPConnection.DEFAULT_PORT;
        } else if (currentToken.equals (":")) {
                // port number without host name is not allowed
               throw new MalformedURLException ("No hostname");
        } else if (currentToken.equals ("?")) {
            throw new MalformedURLException ("No host[:port]");
        } else {
            _hostName = currentToken;
            if (urlParser.countTokens() == 0) {
                _portNumber = _secure ? DEFAULT_SECURE_PORT : LDAPConnection.DEFAULT_PORT;
                return;
            }
            currentToken = urlParser.nextToken (); // either ":" or "/"

            if (currentToken.equals (":")) {
                try {
                    _portNumber = Integer.parseInt (urlParser.nextToken());
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
                _portNumber = _secure ? DEFAULT_SECURE_PORT : LDAPConnection.DEFAULT_PORT;
            } else {
                // expecting ":" or "/"
                throw new MalformedURLException ();
            }
        }


        // DN
        if (!urlParser.hasMoreTokens ()) {
            return;
        }
        _DN = decode(readNextConstruct(urlParser));
        if (_DN.equals("?")) {
            _DN = "";
        }
        else if (_DN.equals("/")) {
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
            _attributes = new Vector ();

            while (attributeParser.hasMoreTokens()) {
                _attributes.addElement (attributeParser.nextToken());
            }
        }

        // scope
        if (!urlParser.hasMoreTokens ()) {
            return;
        }
        str = readNextConstruct(urlParser);
        if (!str.equals("?")) {
            _scope = getScope(str);
            if (_scope < 0) {
                throw new MalformedURLException("Bad scope:" + str);
            }
        }

        // filter
        if (!urlParser.hasMoreTokens ()) {
            return;
        }
        str = readNextConstruct(urlParser);
        _filter = decode(str);
        checkBalancedParentheses(_filter);
        if (!_filter.startsWith("(") && !_filter.endsWith(")")) {
            _filter = "(" + _filter + ")";
        }

        // Nothing after the filter is allowed
        if (urlParser.hasMoreTokens()) {
            throw new MalformedURLException();
        }
    }    
    
    private void checkBalancedParentheses( String filter )
        throws MalformedURLException {
        int parenCnt =0;
        StringTokenizer filterParser =
            new StringTokenizer (filter, "()", true);
        while (filterParser.hasMoreElements()) {
            String token = filterParser.nextToken();
            if (token.equals("(")) {
                parenCnt++;
            }
            else if (token.equals(")")) {
                if (--parenCnt < 0) {
                    throw new MalformedURLException(
                        "Unbalanced filter parentheses");
                }
            }
        }
        
        if (parenCnt != 0) {
            throw new MalformedURLException("Unbalanced filter parentheses");
        }
    }
        
    /**
     * Initializes URL object.
     */
    private void initialize( String host,
                             int port,
                             String DN,
                             Enumeration attributes,
                             int scope,
                             String filter,
                             String[] extensions ) {

        _hostName = host;
        _DN = DN;
        _portNumber = port;
        _filter = (filter != null) ? filter : defaultFilter;
        _scope = scope;
        _extensions = extensions;

        if (attributes != null) {
            _attributes = new Vector ();
            while (attributes.hasMoreElements()) {
                _attributes.addElement (attributes.nextElement());
            }
        } else
            _attributes = null;

        StringBuffer url = new StringBuffer (_secure ? "LDAPS://" :"LDAP://");

        if (host != null) {
            url.append (host);
            url.append (':');
            url.append (String.valueOf (port));
        }

        url.append ('/');
        url.append (encode (DN));

        if (attributes != null) {
            url.append ('?');
            Enumeration attrList = _attributes.elements();
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
              case LDAPConnection.SCOPE_BASE:
                url.append ("base"); break;
              case LDAPConnection.SCOPE_ONE:
                url.append ("one"); break;
              case LDAPConnection.SCOPE_SUB:
                url.append ("sub"); break;
            }

            url.append ('?');
            url.append (filter);
        }

        _URL = url.toString();
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

        if ( _attributes == null ) {
            if ( url._attributes != null ) {
                return false;
            }
        } else if ( _attributes.size() != url._attributes.size() ) {
            return false;
        } else {
            for( int i = 0; i < _attributes.size(); i++ ) {
                if ( _attributes.elementAt( i ) !=
                     url._attributes.elementAt( i ) ) {
                    return false;
                }
            }
        }
        return true;
    }
}
