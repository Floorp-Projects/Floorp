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
package netscape.ldap;

import java.util.*;
import netscape.ldap.client.*;
import netscape.ldap.client.opers.*;
import netscape.ldap.ber.stream.*;
import netscape.ldap.util.*;
import java.io.*;
import java.net.*;

/**
 * Represents a connection to an LDAP server. <P>
 *
 * Use objects of this class to perform LDAP operations (such as
 * search, modify, and add) on an LDAP server. <P>
 *
 * To perform an LDAP operation on a server, you need to follow
 * these steps: <P>
 *
 * <OL>
 * <LI>Create a new <CODE>LDAPConnection</CODE> object.
 * <LI>Use the <CODE>connect</CODE> method to connect to the
 * LDAP server.
 * <LI>Use the <CODE>authenticate</CODE> method to authenticate
 * to server.
 * <LI>Perform the LDAP operation.
 * <LI>Use the <CODE>disconnect</CODE> method to disconnect from
 * the server when done.
 * </OL>
 * <P>
 *
 * All operations block until completion (with the exception of
 * the search method when the results are not all returned at
 * the same time).
 * <P>
 *
 * This class also specifies a default set of search constraints
 * (such as the maximum number of results returned in a search)
 * which apply to all operations. To get and set these constraints,
 * use the <CODE>getOption</CODE> and <CODE>setOption</CODE> methods.
 * To override these constraints for an individual search operation,
 * define a new set of constraints by creating a <CODE>LDAPSearchConstraints</CODE>
 * option and pass the object to the <CODE>search</CODE> method.
 * <P>
 *
 * If you set up your client to follow referrals automatically,
 * an operation that results in a referral will create a new connection
 * to the LDAP server identified in the referral.  In order to have
 * your client authenticate to that LDAP server automatically, you need
 * to define a class that implements the <CODE>LDAPRebind</CODE> interface.
 * In your definition of the class, you need to define a
 * <CODE>getRebindAuthentication</CODE> method that creates an <CODE>LDAPRebindAuth</CODE>
 * object containing the distinguished name and password to use for reauthentication.
 * <P>
 *
 * Most errors that occur raise the same exception (<CODE>LDAPException</CODE>).
 * In order to determine the exact problem that occurred, you can retrieve the
 * result code from this exception and compare its value against a set of defined
 * result codes.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPSearchConstraints
 * @see netscape.ldap.LDAPRebind
 * @see netscape.ldap.LDAPRebindAuth
 * @see netscape.ldap.LDAPException
 */
public class LDAPConnection implements LDAPv3, Cloneable {

    /**
     * Version of the LDAP protocol used by default.
     * <CODE>LDAP_VERSION</CODE> is 2, so your client will
     * attempt to authenticate to LDAP servers as an LDAP v2 client.
     * The following is an example of some code that prints the
     * value of this variable:
     * <P>
     *
     * <PRE>
     * LDAPConnection ld = new LDAPConnection();
     * System.out.println( "The default LDAP protocol version used is "
     *                      ld.LDAP_VERSION );
     * </PRE>
     *
     * If you want to authenticate as an LDAP v3 client,
     * use the <A HREF="#authenticate(int, java.lang.String, java.lang.String)"><CODE>authenticate(int version, String dn, String passwd)</CODE></A> method.
     * For example:
     * <P>
     *
     * <PRE>
     * ld.authenticate( 3, myDN, myPW );
     * </PRE>
     *
     * @see netscape.ldap.LDAPConnection#authenticate(int, java.lang.String, java.lang.String)
     */
    public final static int LDAP_VERSION              = 2;
    /**
     * Name of the property specifying the version of the SDK. <P>
     *
     * To get the version number, pass this name to the
     * <CODE>getProperty</CODE> method.  The SDK version number
     * is a <CODE>Float</CODE> type. For example:<P>
     * <PRE>
     *      ...
     *      Float sdkVersion = ( Float )myConn.getProperty( myConn.LDAP_PROPERTY_SDK );
     *      System.out.println( "SDK version: " + sdkVersion );
     *      ... </PRE>
     * @see netscape.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_SDK      = "version.sdk";
    /**
     * Name of the property specifying the highest supported version of
     * the LDAP protocol. <P>
     *
     * To get the version number, pass this name to the
     * <CODE>getProperty</CODE> method.  The LDAP protocol version number
     * is a <CODE>Float</CODE> type. For example:<P>
     * <PRE>
     *      ...
     *      Float LDAPVersion = ( Float )myConn.getProperty( myConn.LDAP_PROPERTY_PROTOCOL );
     *      System.out.println( "Highest supported LDAP protocol version: " + LDAPVersion );
     *      ... </PRE>
     * @see netscape.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_PROTOCOL = "version.protocol";
    /**
     * Name of the property specifying the types of authentication allowed by this
     * API (for example, anonymous authentication and simple authentication). <P>
     *
     * To get the supported types, pass this name to the
     * <CODE>getProperty</CODE> method.  The value of this property is
     * a <CODE>String</CODE> type. For example:<P>
     * <PRE>
     *      ...
     *      String authTypes = ( String )myConn.getProperty( myConn.LDAP_PROPERTY_SECURITY );
     *      System.out.println( "Supported authentication types: " + authTypes );
     *      ... </PRE>
     * @see netscape.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_SECURITY = "version.security";

    /**
     * Constants
     */
    private final static String defaultFilter = "(objectClass=*)";
    private final static LDAPSearchConstraints readConstraints = new
      LDAPSearchConstraints();

    /**
     * Internal variables
     */
    transient private LDAPSearchConstraints defaultConstraints = new
      LDAPSearchConstraints ();
    transient private Vector responseListeners;
    transient private Vector searchListeners;
    transient private boolean bound;
    transient private String host;
    transient private String[] m_hostList;
    transient private int port;
    transient private int[] m_portList;
    transient private int m_defaultPort;
    transient private String prevBoundDN;
    transient private String prevBoundPasswd;
    transient private String boundDN;
    transient private String boundPasswd;
    transient private int protocolVersion = LDAP_VERSION;
    transient private LDAPSocketFactory m_factory;
    /* th does all socket i/o for the object and any clones */
    transient private LDAPConnThread th = null;
    /* To manage received server controls on a per-thread basis,
       we keep a table of active threads and a table of controls,
       indexed by thread */
    transient private Vector m_attachedList = new Vector();
    transient private Hashtable m_responseControlTable = new Hashtable();
    transient private LDAPCache m_cache = null;
    static Hashtable m_threadConnTable = new Hashtable();

    // this handles the case when the client lost the connection with the
    // server. After the client reconnects with the server, the bound resets
    // to false. If the client used to have anonymous bind, then this boolean
    // will take care of the case whether the client should send anonymous bind
    // request to the server.
    private boolean m_anonymousBound = false;

    private Object m_security = null;
    private boolean saslBind = false;
    private Object m_mechanismDriver;
    private Properties m_securityProperties;
    private Object m_clientCB;
    private Hashtable m_methodLookup = new Hashtable();
    private LDAPConnection m_referralConnection;

    /**
     * Properties
     */
    private final static Float SdkVersion = new Float(3.1f);
    private final static Float ProtocolVersion = new Float(3.0f);
    private final static String SecurityVersion = new String("none,simple,sasl");
    private final static Float MajorVersion = new Float(3.0f);
    private final static Float MinorVersion = new Float(0.1f);
    private final static String DELIM = "#";
    private final static String PersistSearchPackageName =
      "netscape.ldap.controls.LDAPPersistSearchControl";
    private final static String EXTERNAL_MECHANISM = "SASLExternalMechanism";
    private final static String EXTERNAL_MECHANISM_PACKAGE =
      "com.netscape.sasl.mechanisms";

   /**
    * Constructs a new <CODE>LDAPConnection</CODE> object,
    * which represents a connection to an LDAP server. <P>
    *
    * Calling the constructor does not actually establish
    * the connection.  To connect to the LDAP server, use the
    * <CODE>connect</CODE> method.
    *
    * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int)
    * @see netscape.ldap.LDAPConnection#authenticate(java.lang.String, java.lang.String)
    */
    public LDAPConnection () {
        super();
        port = -1;
        m_factory = null;
    }

    /**
     * Constructs a new <CODE>LDAPConnection</CODE> object that
     * will use the specified socket factory class to create
     * socket connections. The socket factory class must implement
     * the <CODE>LDAPSocketFactory</CODE> interface. <BR>
     * (For example, the <CODE>LDAPSSLSocketFactory</CODE>
     * class implements this interface.)
     * <P>
     *
     * Note that calling the <CODE>LDAPConnection</CODE> constructor
     * does not actually establish a connection to an LDAP server.
     * To connect to an LDAP server, use the
     * <CODE>connect</CODE> method.  The socket connection will be
     * constructed when this method is called.
     * <P>
     *
     * @see netscape.ldap.LDAPSocketFactory
     * @see netscape.ldap.LDAPSSLSocketFactory
     * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int)
     * @see netscape.ldap.LDAPConnection#authenticate(java.lang.String, java.lang.String)
     * @see netscape.ldap.LDAPConnection#getSocketFactory
     * @see netscape.ldap.LDAPConnection#setSocketFactory(netscape.ldap.LDAPSocketFactory)
     */
    public LDAPConnection ( LDAPSocketFactory factory ) {
        super();
        port = -1;
        m_factory = factory;
    }

    /**
     * Finalize method, which disconnects from the LDAP server.
     * @exception LDAPException Thrown when the connection cannot be disconnected.
     */
    public void finalize() throws LDAPException
    {
        if (th != null)
            disconnect();
    }

    /**
     *  Sets the specified <CODE>LDAPCache</CODE> object as the
     *  cache for the <CODE>LDAPConnection</CODE> object.
     *  <P>
     *
     *  @param cache The <CODE>LDAPCache</CODE> object representing
     *   the cache you want used by the current connection.
     *  @see netscape.ldap.LDAPCache
     *  @see netscape.ldap.LDAPConnection#getCache
     */
    public void setCache(LDAPCache cache) {
        m_cache = cache;
    }

    /**
     * Gets the <CODE>LDAPCache</CODE> object associated with
     * the current <CODE>LDAPConnection</CODE> object.
     * <P>
     *
     * @return The <CODE>LDAPCache</CODE> object representing
     *   the cache used by the current connection.
     * @see netscape.ldap.LDAPCache
     * @see netscape.ldap.LDAPConnection#setCache(netscape.ldap.LDAPCache)
     */
    public LDAPCache getCache() {
        return m_cache;
    }

    /**
     * Gets a property of a connection. <P>
     *
     * You can get the following properties for a given connection: <P>
     * <UL TYPE="DISC">
     * <LI><CODE>LDAP_PROPERTY_SDK</CODE> <P>
     * To get the version of this SDK, get this property.  The value of
     * this property is a <CODE>Float</CODE> data type. <P>
     * <LI><CODE>LDAP_PROPERTY_PROTOCOL</CODE> <P>
     * To get the highest supported version of the LDAP protocol, get
     * this property.
     * The value of this property is a <CODE>Float</CODE> data type. <P>
     * <LI><CODE>LDAP_PROPERTY_SECURITY</CODE> <P>
     * To get a comma-separated list of the types of authentication
     * supported, get this property.  The value of this property is a
     * <CODE>String</CODE>. <P>
     * </UL>
     * <P>
     *
     * For example, the following section of code gets the version of
     * the SDK.<P>
     *
     * <PRE>
     *       ...
     *       Float sdkVersion = ( Float )myConn.getProperty( myConn.LDAP_PROPERTY_SDK );
     *       System.out.println( "SDK version: " + sdkVersion );
     *       ... </PRE>
     *
     * @param name Name of the property (for example,
     * <CODE>LDAP_PROPERTY_SDK</CODE>). <P>
     *
     * @return The value of the property. <P>
     *
     * Since the return value is an object, you
     * should recast it as the appropriate type.
     * (For example, when getting the <CODE>LDAP_PROPERTY_SDK</CODE> property,
     * recast the return value as a <CODE>Float</CODE>.) <P>
     *
     * If you pass this method an unknown property name, the method
     * returns null. <P>
     *
     * @exception LDAPException Unable to get the value of the
     * specified property. <P>
     *
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_SDK
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_PROTOCOL
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_SECURITY
     */
    public Object getProperty(String name) throws LDAPException {
        if (name.equals(LDAP_PROPERTY_SDK))
            return SdkVersion;
        else if (name.equals(LDAP_PROPERTY_PROTOCOL))
            return ProtocolVersion;
        else if (name.equals(LDAP_PROPERTY_SECURITY))
            return SecurityVersion;
        else if (name.equals("version.major"))
            return MajorVersion;
        else if (name.equals("version.minor"))
            return MinorVersion;
        else
            return null;
    }

    /**
     * This method is reserved for future use and does not currently
     * allow you to set any properties.
     * <P>
     *
     * @param name Name of the property that you want to set.
     * @param val Value that you want to set.
     * @exception LDAPException Unable to set the value of the specified
     * property.
     */
    public void setProperty(String name, Object val) throws LDAPException {
        throw new LDAPException("No property has been set");
    }

    /**
     * Sets the LDAP protocol version that your client prefers to use when
     * connecting to the LDAP server.
     * <P>
     *
     * @param version The LDAP protocol version that your client uses.
     */
    private void setProtocolVersion(int version) {
        protocolVersion = version;
    }

    /**
     * Returns the host name of the LDAP server to which you are connected.
     * @return Host name of the LDAP server.
     */
    public String getHost () {
        return host;
    }

    /**
     * Returns the port number of the LDAP server to which you are connected.
     * @return Port number of the LDAP server.
     */
    public int getPort () {
        return port;
    }

    /**
     * Returns the distinguished name (DN) used for authentication over
     * this connection.
     * @return Distinguished name used for authentication over this connection.
     */
    public String getAuthenticationDN () {
        return boundDN;
    }

    /**
     * Returns the password used for authentication over this connection.
     * @return Password used for authentication over this connection.
     */
    public String getAuthenticationPassword () {
        return boundPasswd;
    }

    /**
     * Gets the object representing the socket factory used to establish
     * a connection to the LDAP server.
     * <P>
     *
     * @return The object representing the socket factory used to
     *  establish a connection to a server.
     * @see netscape.ldap.LDAPSocketFactory
     * @see netscape.ldap.LDAPSSLSocketFactory
     * @see netscape.ldap.LDAPConnection#setSocketFactory(netscape.ldap.LDAPSocketFactory)
     */
    public LDAPSocketFactory getSocketFactory () {
        return m_factory;
    }

    /**
     * Specifies the object representing the socket factory that you
     * want to use to establish a connection to a server.
     * <P>
     *
     * @param factory The object representing the socket factory that
     *  you want to use to establish a connection to a server.
     * @see netscape.ldap.LDAPSocketFactory
     * @see netscape.ldap.LDAPSSLSocketFactory
     * @see netscape.ldap.LDAPConnection#getSocketFactory
     */
    public void setSocketFactory (LDAPSocketFactory factory) {
        m_factory = factory;
    }

    /**
     * Indicates whether the connection represented by this object
     * is open at this time.
     * @return If connected to an LDAP server over this connection,
     * returns <CODE>true</CODE>.  If not connected to an LDAP server,
     * returns <CODE>false</CODE>.
     */
    public boolean isConnected() {
        // This is the hack: If the user program calls isConnected() when
        // the thread is about to shut down, the isConnected might get called
        // before the deregisterConnection(). We add the yield() so that 
        // the deregisterConnection() will get called first. 
        // This problem only exists on Solaris.
        Thread.yield();
        return (th != null);
    }

    /**
     * Indicates whether this client has authenticated to the LDAP server
     * @return If authenticated, returns <CODE>true</CODE>. If not
     * authenticated, or if authenticated as an anonymous user (with
     * either a blank name or password), returns <CODE>false</CODE>.
     */
    public boolean isAuthenticated () {
        if (bound) {
            if ((boundDN == null) || boundDN.equals("") ||
              (boundPasswd == null) || boundPasswd.equals(""))
                return false;
        }
        return bound;
    }

    /**
     * Connects to the specified host and port.  If this LDAPConnection object
     * represents an open connection, the connection is closed first
     * before the new connection is opened.
     * <P>
     *
     * For example, the following section of code establishes a connection with
     * the LDAP server running on the host ldap.netscape.com and the port 389.
     * <P>
     *
     * <PRE>
     * String ldapHost = "ldap.netscape.com";
     * int ldapPort = 389;
     * LDAPConnection myConn = new LDAPConnection();
     * try {
     *     myConn.connect( ldapHost, ldapPort );
     * } catch ( LDAPException e ) {
     *     System.out.println( "Unable to connect to " + ldapHost +
     *                         " at port " + ldapPort );
     *     return;
     * }
     * System.out.println( "Connected to " + ldapHost + " at port " + ldapPort )
     * </PRE>
     *
     * @param host Host name of the LDAP server that you want to connect to.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). Your client application or applet
     * will attempt to contact each host in the order you specify until a
     * connection is established. For example, you can specify the following
     * values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * @param port Port number of the LDAP server that you want to connect to.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @exception LDAPException The connection failed.
     */
    public void connect(String host, int port) throws LDAPException {
      connect( host, port, null, null, defaultConstraints, false );
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server.  If this LDAPConnection object
     * represents an open connection, the connection is closed first
     * before the new connection is opened.
     * <P>
     *
     * For example, the following section of code establishes a connection
     * with the LDAP server running on ldap.netscape.com at port 389.  The
     * example also attempts to authenticate the client as Barbara Jensen.
     * <P>
     *
     * <PRE>
     * String ldapHost = "ldap.netscape.com";
     * int ldapPort = 389;
     * String myDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String myPW = "hifalutin";
     * LDAPConnection myConn = new LDAPConnection();
     * try {
     *     myConn.connect( ldapHost, ldapPort, myDN, myPW );
     * } catch ( LDAPException e ) {
     *     switch( e.getLDAPResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified user does not exist." );
     *             break;
     *         case e.INVALID_CREDENTIALS:
     *             System.out.println( "Invalid password." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Failed to connect to " + ldapHost + " at port " + ldapPort );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Connected to " + ldapHost + " at port " + ldapPort );
     * </PRE>
     *
     * @param host Host name of the LDAP server that you want to connect to.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). Your client application or applet
     * will attempt to contact each host in the order you specify until a
     * connection is established. For example, you can specify the following
     * values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * @param port Port number of the LDAP server that you want to connect to.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @param dn Distinguished name used for authentication
     * @param passwd Password used for authentication
     * @exception LDAPException The connection or authentication failed.
     */
    public void connect(String host, int port, String dn, String passwd)
        throws LDAPException {
        connect(host, port, dn, passwd, defaultConstraints, true);
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server.  If this LDAPConnection object
     * represents an open connection, the connection is closed first
     * before the new connection is opened. This method allows the user to 
     * specify the preferences for the bind operation.
     * 
     * @param host Host name of the LDAP server that you want to connect to.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). Your client application or applet
     * will attempt to contact each host in the order you specify until a
     * connection is established. For example, you can specify the following
     * values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * @param port Port number of the LDAP server that you want to connect to.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @param dn Distinguished name used for authentication
     * @param passwd Password used for authentication
     * @param cons Preferences for the bind operation.
     * @exception LDAPException The connection or authentication failed.
     */
    public void connect(String host, int port, String dn, String passwd,
        LDAPSearchConstraints cons) throws LDAPException {
        connect(host, port, dn, passwd, cons, true);
    }

    private void connect(String host, int port, String dn, String passwd,
      LDAPSearchConstraints cons, boolean doAuthenticate) 
        throws LDAPException {
        if (th != null) disconnect ();
        if ((host == null) || (host.equals("")))
            throw new LDAPException ( "no host for connection",
                                  LDAPException.PARAM_ERROR );

        /* Parse the list of hosts */
        m_defaultPort = port;
        StringTokenizer st = new StringTokenizer( host );
        m_hostList = new String[st.countTokens()];
        m_portList = new int[st.countTokens()];
        int i = 0;
        while( st.hasMoreTokens() ) {
            String s = st.nextToken();
            int colon = s.indexOf( ':' );
            if ( colon > 0 ) {
                m_hostList[i] = s.substring( 0, colon );
                m_portList[i] = Integer.parseInt( s.substring( colon+1 ) );
            } else {
                m_hostList[i] = s;
                m_portList[i] = m_defaultPort;
            }
            i++;
        }

        /* Try each possible host until a connection attempt doesn't cause
           an exception */
        LDAPException conex = null;
        for( i = 0; i < m_hostList.length; i++ ) {
            try {
                this.host = m_hostList[i];
                this.port = m_portList[i];
                connect ();
                conex = null;
                break;
            } catch ( LDAPException e ) {
                conex = e;
            }
        }
        if ( conex != null ) {
            /* All connection attempts failed */
            this.host = m_hostList[0];
            this.port = m_defaultPort;
            throw conex;
        }

        if (doAuthenticate)
            authenticate(dn, passwd, cons);
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server, with the specified LDAP
     * protocol version. If the server does not support the requested
     * protocol version, an exception is thrown. If this LDAPConnection
     * object represents an open connection, the connection is closed first
     * before the new connection is opened. This is equivalent to
     * <CODE>connect(host, port)</CODE> followed by <CODE>authenticate(version, dn, passwd)</CODE>.<P>
     *
     * @param version LDAP protocol version requested: currently 2 or 3
     * @param host Contains a hostname or dotted string representing
     *   the IP address of a host running an LDAP server to connect to.
     *   Alternatively, it may contain a list of host names, space-delimited.
     *   Each host name may include a trailing colon and port number. In the
     *   case where more than one host name is specified, each host name in
     *   turn will be contacted until a connection can be established.<P>
     *
     * <PRE>
     *   Examples:
     *      "directory.knowledge.com"
     *      "199.254.1.2"
     *      "directory.knowledge.com:1050 people.catalog.com 199.254.1.2"
     * </PRE>
     * <P>
     * @param port Contains the TCP or UDP port number to connect to or contact.
     *   The default LDAP port is 389. "port" is ignored for any host name which
     *   includes a colon and port number.
     * @param dn If non-null and non-empty, specifies that the connection and
     *   all operations through it should be authenticated with dn as the
     *   distinguished name.
     * @param passwd If non-null and non-empty, specifies that the connection and
     *   all operations through it should be authenticated with dn as the
     *   distinguished name and passwd as password.
     * @exception LDAPException The connection or authentication failed.
     */
    public void connect(int version, String host, int port, String dn,
        String passwd) throws LDAPException {

        connect(version, host, port, dn, passwd, defaultConstraints);
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server, with the specified LDAP
     * protocol version. If the server does not support the requested
     * protocol version, an exception is thrown. This method allows the user
     * to specify preferences for the bind operation. If this LDAPConnection
     * object represents an open connection, the connection is closed first
     * before the new connection is opened. This is equivalent to
     * <CODE>connect(host, port)</CODE> followed by <CODE>authenticate(version, dn, passwd)</CODE>.<P>
     *
     * @param version LDAP protocol version requested: currently 2 or 3
     * @param host Contains a hostname or dotted string representing
     *   the IP address of a host running an LDAP server to connect to.
     *   Alternatively, it may contain a list of host names, space-delimited.
     *   Each host name may include a trailing colon and port number. In the
     *   case where more than one host name is specified, each host name in
     *   turn will be contacted until a connection can be established.<P>
     *
     * <PRE>
     *   Examples:
     *      "directory.knowledge.com"
     *      "199.254.1.2"
     *      "directory.knowledge.com:1050 people.catalog.com 199.254.1.2"
     * </PRE>
     * <P>
     * @param port Contains the TCP or UDP port number to connect to or contact.
     *   The default LDAP port is 389. "port" is ignored for any host name which
     *   includes a colon and port number.
     * @param dn If non-null and non-empty, specifies that the connection and
     *   all operations through it should be authenticated with dn as the
     *   distinguished name.
     * @param passwd If non-null and non-empty, specifies that the connection and
     *   all operations through it should be authenticated with dn as the
     *   distinguished name and passwd as password.
     * @param cons Preferences for the bind operation.
     * @exception LDAPException The connection or authentication failed.
     */
    public void connect(int version, String host, int port, String dn,
        String passwd, LDAPSearchConstraints cons) throws LDAPException {

        setProtocolVersion(version);
        connect(host, port, dn, passwd, cons);
    }

    /**
     * Internal routine to connect with internal params
     * @exception LDAPException failed to connect
     */
    private synchronized void connect () throws LDAPException {
        if (th != null) return;

        if (host == null || port < 0)
            throw new LDAPException ( "no connection parameters",
                                 LDAPException.PARAM_ERROR );

        th = getNewThread(host, port, m_factory, m_cache);
    }

    private synchronized LDAPConnThread getNewThread(String host, int port,
      LDAPSocketFactory factory, LDAPCache cache) throws LDAPException {

        LDAPConnThread newThread = null;
        Vector v = null;

        synchronized(m_threadConnTable) {

            Enumeration keys = m_threadConnTable.keys();
            boolean connExist = false;

            // transverse each thread
            while (keys.hasMoreElements()) {
                LDAPConnThread connThread = (LDAPConnThread)keys.nextElement();
                Vector connVector = (Vector)m_threadConnTable.get(connThread);
                Enumeration enumv = connVector.elements();

                // transverse through each LDAPConnection under the same thread
                while (enumv.hasMoreElements()) {
                    LDAPConnection conn = (LDAPConnection)enumv.nextElement();

                    // this is not the brand new connection
                    if (conn.equals(this)) {
                        connExist = true;

                        if (!connThread.isAlive()) {
                            // need to move all the LDAPConnections under the dead thread
                            // to the new thread
                            try {
                                newThread = new LDAPConnThread(host, port, factory, cache);
                                newThread.setMaxBacklog(
                                    getSearchConstraints().getMaxBacklog() );
                                v = (Vector)m_threadConnTable.remove(connThread);
                                break;
                            } catch (Exception e) {
                                throw new LDAPException ("unable to establish connection",
                                    LDAPException.UNAVAILABLE);
                            }
                        }
                        break;
                    }
                }

                if (connExist) break;
            }

            // if this connection is new or the corresponding thread for the current
            // connection is dead
            if (!connExist) {
                try {
                    newThread = new LDAPConnThread(host, port, factory, cache);
                    newThread.setMaxBacklog( getSearchConstraints().getMaxBacklog() );
                    v = new Vector();
                    v.addElement(this);
                } catch (Exception e) {
                    throw new LDAPException ("unable to establish connection",
                        LDAPException.UNAVAILABLE);
                }
            }

            // add new thread to the table
            if (newThread != null) {
                m_threadConnTable.put(newThread, v);
                for (int i=0, n=v.size(); i<n; i++) {
                    LDAPConnection c = (LDAPConnection)v.elementAt(i);
                    newThread.register(c);
                    c.th = newThread;
                }
            }
        }

        authenticateSSLConnection();
        return newThread;
    }

    /**
     * Do certificate-based authentication if client authentication was
     * specified at construction time.
     * @exception LDAPException if certificate-based authentication fails.
     */
    private void authenticateSSLConnection() throws LDAPException {

        // if this is SSL
        if ((m_factory != null) &&
          (m_factory instanceof LDAPSSLSocketFactoryExt)) {

            boolean isClientAuth =
              ((LDAPSSLSocketFactoryExt)m_factory).isClientAuth();

            if (isClientAuth)
                authenticate(null, EXTERNAL_MECHANISM,
                  EXTERNAL_MECHANISM_PACKAGE, null, null);
        }
    }

    /**
     * Abandons a current search operation, notifying the server not
     * to send additional search results.
     *
     * @param searchResults The search results returned when the search
     * was started.
     * @exception LDAPException Failed to notify the LDAP server.
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     * @see netscape.ldap.LDAPSearchResults
     */
    public void abandon( LDAPSearchResults searchResults )
                             throws LDAPException {
        if ( (th == null) || (searchResults == null) )
            return;

        searchResults.abandon();
        int id = searchResults.getID();
        for (int i=0; i<3; i++) {
            try {
                /* Tell listener thread to discard results */
                th.abandon( id );

                /* Tell server to stop sending results */
                th.sendRequest(new JDAPAbandonRequest(id), null, defaultConstraints);

                /* No response is forthcoming */
                break;
            } catch (NullPointerException ne) {
                // do nothing
            }
        }
        if (th == null)
            throw new LDAPException("Failed to send abandon request to the server.",
              LDAPException.OTHER);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password.
     * If you are not already connected to the LDAP server, this
     * method attempts to reconnect to the server.
     * <P>
     *
     * For example, the following section of code authenticates the
     * client as Barbara Jensen.  The code assumes that the client
     * has already established a connection with an LDAP server.
     * <P>
     *
     * <PRE>
     * String myDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String myPW = "hifalutin";
     * try {
     *     myConn.authenticate( myDN, myPW );
     * } catch ( LDAPException e ) {
     *     switch( e.getLDAPResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified user does not exist." );
     *             break;
     *         case e.INVALID_CREDENTIALS:
     *             System.out.println( "Invalid password." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Failed to authentice as " + myDN );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Authenticated as " + myDN );
     * </PRE>
     *
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, String passwd) throws LDAPException {
        authenticate(protocolVersion, dn, passwd, defaultConstraints);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password. The
     * default protocol version (version 2) is used. If the server
     * doesn't support the default version, an LDAPException is thrown
     * with the error code PROTOCOL_ERROR. This method allows the
     * user to specify the preferences for the bind operation.
     *
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @param cons Preferences for the bind operation.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, String passwd,
      LDAPSearchConstraints cons) throws LDAPException {
        authenticate(protocolVersion, dn, passwd, cons);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password, and
     * requesting that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR.
     *
     * @param version Required LDAP protocol version.
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(int version, String dn, String passwd)
      throws LDAPException {
        authenticate(version, dn, passwd, defaultConstraints);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password, and
     * requesting that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR. This method allows the user to specify the
     * preferences for the bind operation.
     *
     * @param version Required LDAP protocol version.
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @param cons Preferences for the bind operation.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(int version, String dn, String passwd,
      LDAPSearchConstraints cons) throws LDAPException {
        prevBoundDN = boundDN;
        prevBoundPasswd = boundPasswd;
        boundDN = dn;
        boundPasswd = passwd;

        if ((prevBoundDN == null) || (prevBoundPasswd == null))
            m_anonymousBound = true;
        else
            m_anonymousBound = false;

        bind (version, true, cons);
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn If non-null and non-empty, specifies that the connection and
     * all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param mechanism A single mechanism name, e.g. "GSSAPI".
     * @param packageName A package from which to instantiate the Mechanism
     * Driver, e.g. "myclasses.SASL.mechanisms". If null, a system default
     * is used.
     * @param getter A class which may be called by the Mechanism Driver to
     * obtain additional information required.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, String mechanism, String packageName,
        Properties props, Object getter) throws LDAPException {

        try {
            Object[] args = new Object[2];
            args[0] = mechanism;
            args[1] = packageName;
            String[] argNames = new String[2];
            argNames[0] = "java.lang.String";
            argNames[1] = "java.lang.String";

            // Get a mechanism driver
            m_mechanismDriver = invokeMethod(null,
              "com.netscape.sasl.SASLMechanismFactory", "getMechanismDriver",
              args, argNames);

        } catch (Exception e) {
            throw new LDAPException(e.toString(), LDAPException.OTHER);
        }

        m_securityProperties = props;
        m_clientCB = getter;
        boundDN = dn;
        saslBind(true);
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn If non-null and non-empty, specifies that the connection and
     * all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param mechanisms A list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param packageName A package from which to instantiate the Mechanism
     * Driver, e.g. "myclasses.SASL.mechanisms". If null, a system default
     * is used.
     * @param getter A class which may be called by the Mechanism Driver to
     * obtain additional information required.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, String[] mechanisms, String packageName,
        Properties props, Object getter) throws LDAPException {

        for (int i=0; i<mechanisms.length; i++) {
            try {
                authenticate(dn, mechanisms[i], packageName, props, getter);
                if (m_mechanismDriver != null)
                    return;
            } catch (LDAPException e) {
            }
        }

        throw new LDAPException("Failed to get mechanism driver",
          LDAPException.OTHER);
    }

    private void saslBind(boolean rebind) throws LDAPException {

        saslBind = true;
        protocolVersion = 3;

        if (th == null) {
            bound = false;
            th = null;
            connect ();
        }

        // if the connection has already been bound and need to be rebinded
        if (bound && rebind) {
            // if the thread has more than one LDAPConnection, then the connection
            // should disconnect first. Otherwise, we can reuse the same thread
            // and do the rebind again.
            if (th.getClientCount() > 1) {
                disconnect();
                connect();
            }
        }

        if ((bound && rebind) || (!bound)) {
            try {
                // Get the initial request to start authentication
                Object[] arg1 = new Object[5];
                arg1[0] = boundDN;
                arg1[1] = "LDAP";
                arg1[2] = null;
                arg1[3] = m_securityProperties;
                arg1[4] = m_clientCB;

                String[] argNames = new String[5];
                argNames[0] = "java.lang.String";
                argNames[1] = "java.lang.String";
                argNames[2] = "java.lang.String";
                argNames[3] = "java.util.Properties";
                argNames[4] = "com.netscape.sasl.SASLClientCB";

                String mechanismName = m_mechanismDriver.getClass().getName();
                byte[] outVals = (byte[])invokeMethod(m_mechanismDriver,
                  mechanismName, "startAuthentication", arg1, argNames);


                boolean isExternal = isExternalMechanism(mechanismName);
                int resultCode = LDAPException.SASL_BIND_IN_PROGRESS;
                JDAPBindResponse response = null;
                while (!checkForSASLBindCompletion(resultCode)) {
                    response = saslBind(outVals);
                    resultCode = response.getResultCode();

                    if (isExternal)
                        continue;

                    String challenge = response.getCredentials();
                    byte[] b = challenge.getBytes();

                    Object[] arg2 = new Object[1];
                    arg2[0] = b;
                    String[] arg2Names = new String[1];
                    arg2Names[0] = "[B";  //class name for byte array

                    outVals = (byte[])invokeMethod(m_mechanismDriver,
                        mechanismName, "evaluateResponse", arg2, arg2Names);
                }

                // Make sure authentication REALLY is complete
                Boolean bool = (Boolean)invokeMethod(m_mechanismDriver,
                    mechanismName, "isComplete", null, null);
                if (!bool.booleanValue()) {
                    // Authentication session hijacked!
                    throw new LDAPException("The server indicates that " +
                        "authentication is successful, but the SASL driver " +
                        "indicates that authentication is not yet done.",
                        LDAPException.OTHER);
                }

                m_security = invokeMethod(m_mechanismDriver,
                    mechanismName, "getSecurityLayer", null, null);
                th.setSecurityLayer(m_security);
                updateThreadConnTable();
            } catch (LDAPException e) {
                throw e;
            } catch (Exception e) {
                throw new LDAPException(e.toString(), LDAPException.OTHER);
            }
        }
    }

    private boolean isExternalMechanism(String name) {
        String s = name;
        int dot = name.lastIndexOf( '.' );
        if ( (dot >= 0) && (dot < (name.length()-1)) ) {
            s = name.substring( dot + 1 );
        }
        return s.equals( EXTERNAL_MECHANISM );
    }

    private Object invokeMethod(Object obj, String packageName,
      String methodName, Object[] args, String[] argNames)
      throws LDAPException {
        try {
            java.lang.reflect.Method m = getMethod(packageName, methodName,
              argNames);
            if (m != null) {
                return (m.invoke(obj, args));
            }
        } catch (Exception e) {
            throw new LDAPException("Invoking "+methodName+": "+
              e.toString(), LDAPException.PARAM_ERROR);
        }

        return null;
    }

    private java.lang.reflect.Method getMethod(String packageName,
      String methodName, String[] args) throws LDAPException {
        try {
            java.lang.reflect.Method method = null;
            String suffix = "";
            if (args != null)
                for (int i=0; i<args.length; i++)
                    suffix = suffix+args[i].getClass().getName();
            String key = packageName+"."+methodName+"."+suffix;
            if ((method = (java.lang.reflect.Method)(m_methodLookup.get(key)))
              != null)
                return method;

            Class c = Class.forName(packageName);
            java.lang.reflect.Method[] m = c.getMethods();
            for (int i = 0; i < m.length; i++ ) {
                Class[] params = m[i].getParameterTypes();
                if ((m[i].getName().equals(methodName)) &&
                    signatureCorrect(params, args)) {
                    m_methodLookup.put(key, m[i]);
                    return m[i];
                }
            }
            throw new LDAPException("Method " + methodName + " not found in " +
              packageName);
        } catch (ClassNotFoundException e) {
            throw new LDAPException("Class "+ packageName + " not found");
        }
    }

    private boolean signatureCorrect(Class params[], String args[]) {
        if (args == null)
            return true;
        if (params.length != args.length)
            return false;
        for (int i=0; i<params.length; i++) {
            if (!params[i].getName().equals(args[i]))
                return false;
        }
        return true;
    }

    private boolean checkForSASLBindCompletion(int resultCode)
      throws LDAPException{

        if (resultCode == LDAPException.SUCCESS) {
            return true;
        } else if (resultCode == LDAPException.SASL_BIND_IN_PROGRESS) {
            return false;
        } else {
            throw new LDAPException("Authentication failed", resultCode);
        }
    }

    private JDAPBindResponse saslBind(byte[] credentials) throws LDAPException {
        saslBind = true;
        if (th == null) connect ();

        LDAPResponseListener myListener = getResponseListener ();
        JDAPMessage response;

        try {
            String name = (String)invokeMethod(m_mechanismDriver,
              m_mechanismDriver.getClass().getName(), "getMechanismName",
              null, null);
            sendRequest(new JDAPBindRequest(protocolVersion, boundDN,
              name, credentials),
              myListener, defaultConstraints);
            response = myListener.getResponse();

            JDAPProtocolOp protocolOp = response.getProtocolOp();
            if (protocolOp instanceof JDAPBindResponse)
                return (JDAPBindResponse)protocolOp;
            else
                throw new LDAPException("Unknown response from the server during SASL bind",
                  LDAPException.OTHER);
        } finally {
            releaseResponseListener(myListener);
        }
    }

    /**
     * Internal routine. Binds to the LDAP server.
     * @param version protocol version to request from server
     * @param rebind True if the rebind operation is requested, otherwise, false.
     * @exception LDAPException failed to bind
     */
    private void bind (int version, boolean rebind,
      LDAPSearchConstraints cons) throws LDAPException {
        saslBind = false;

        if (th == null) {
            bound = false;
            th = null;
            connect ();
            // special case: the connection currently is not bound, and now there is
            // a bind request. The connection needs to reconnect if the thread th has
            // more than one LDAPConnection.
        } else if (!bound && rebind && th.getClientCount() > 1) {
            disconnect();
            connect();
        }

        // if the connection is still intact and no rebind request
        if (bound && !rebind)
            return;

        // if the connection was lost and did not have any kind of bind
        // operation and the current one does not request any bind operation (ie,
        // no authenticate has been called)
        if (!m_anonymousBound &&
         ((boundDN == null) || (boundPasswd == null)) &&
         !rebind)
            return;

        if (bound && rebind) {
            if (protocolVersion == version) {
                if (m_anonymousBound && ((boundDN == null) || (boundPasswd == null)))
                    return;

                if (!m_anonymousBound && (boundDN != null) && (boundPasswd != null) &&
                    boundDN.equals(prevBoundDN) &&
                    boundPasswd.equals(prevBoundPasswd))
                    return;
            }

            // if get to here, means that we need to do rebind since previous and
            // current credentials are not the same.
            // if the current connection is the only connection for the th thread,
            // then reuse this current connection. otherwise, disconnect the current
            // one (ie, detach from the current th) and reconnect again (ie, have
            // a new th).
            if (th.getClientCount() > 1) {
                disconnect();
                connect();
            }
        }

        protocolVersion = version;

        LDAPResponseListener myListener = getResponseListener ();
        JDAPMessage response;
        try {
            if (m_referralConnection != null) {
                m_referralConnection.disconnect();
                m_referralConnection = null;
            }
            sendRequest(new JDAPBindRequest(protocolVersion, boundDN, boundPasswd),
                myListener, cons);
            response = myListener.getResponse();
            checkMsg(response);
        } catch (LDAPReferralException e) {
            m_referralConnection = createReferralConnection(e, cons);
        } finally {
            releaseResponseListener(myListener);
        }

        updateThreadConnTable();
    }

    private void updateThreadConnTable() {
        synchronized(m_threadConnTable) {
            if (m_threadConnTable.containsKey(th)) {
                Vector v = (Vector)m_threadConnTable.get(th);
                for (int i=0, n=v.size(); i<n; i++) {
                    LDAPConnection conn = (LDAPConnection)v.elementAt(i);
                    conn.bound = true;
                }
            } else
                printDebug("Thread table does not contain the specified thread");
        }
    }

    private void sendRequest(JDAPProtocolOp oper, LDAPResponseListener myListener,
      LDAPSearchConstraints cons) throws LDAPException {

        // retry three times if we got the nullPointer exception.
        // Dont remove this. The following situation might happen:
        // Before sendRequest get invoked, it is possible that the LDAPConnThread
        // encountered the networkError and called the deregisterConnection which
        // in turn set the "th" thread to be null.
        for (int i=0; i<3; i++) {
            try {
                th.sendRequest(oper, myListener, cons);
                break;
            } catch(NullPointerException ne) {
                // do nothing
            }
        }
        if (th == null)
            throw new LDAPException("The connection is not available",
                LDAPException.OTHER);
    }

    /**
     * Internal routine. Binds to the LDAP server.
     * @exception LDAPException failed to bind
     */
    private void bind (LDAPSearchConstraints cons) throws LDAPException {
        if (saslBind)
            saslBind(false);
        else
            bind (protocolVersion, false, cons);
    }

    /**
     * Disconnects from the LDAP server. Before you can perform LDAP operations
     * again, you need to reconnect to the server by calling
     * <CODE>connect</CODE>.
     * @exception LDAPException Failed to disconnect from the LDAP server.
     * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int)
     * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int, java.lang.String, java.lang.String)
     */
    public synchronized void disconnect() throws LDAPException {
        if (m_referralConnection != null) {
            m_referralConnection.disconnect();
            m_referralConnection = null;
        }

        if (th == null)
            throw new LDAPException ( "unable to disconnect() without connecting",
                                  LDAPException.OTHER );
        if (m_cache != null) {
            m_cache.cleanup();
            m_cache = null;
        }
        deleteThreadConnEntry();
        deregisterConnection();
    }

    private void deleteThreadConnEntry() {
        synchronized (m_threadConnTable) {
            Enumeration keys = m_threadConnTable.keys();
            while (keys.hasMoreElements()) {
                LDAPConnThread connThread = (LDAPConnThread)keys.nextElement();
                Vector connVector = (Vector)m_threadConnTable.get(connThread);
                Enumeration enumv = connVector.elements();
                while (enumv.hasMoreElements()) {
                    LDAPConnection c = (LDAPConnection)enumv.nextElement();
                    if (c.equals(this)) {
                        connVector.removeElement(c);
                        if (connVector.size() == 0)
                            m_threadConnTable.remove(connThread);
                        return;
                    }
                }
            }
        }
    }

    /**
     *  Decrement the reference count for the connection
     */
    synchronized void deregisterConnection() {
        th.deregister(this);
        th = null;
        bound = false;
    }

    /**
     * Reads the entry for the specified distiguished name (DN) and retrieves all
     * attributes for the entry.
     * <P>
     *
     * For example, the following section of code reads the entry for
     * Barbara Jensen and retrieves all attributes for that entry.
     * <P>
     *
     * <PRE>
     * String findDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * LDAPEntry foundEntry = null;
     * try {
     *     foundEntry = myConn.read( findDN );
     * } catch ( LDAPException e ) {
     *     switch( e.getLDAPResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified entry does not exist." );
     *             break;
     *         case e.LDAP_PARTIAL_RESULTS:
     *             System.out.println( "Entry served by a different LDAP server." );
     *             break;
     *         case e.INSUFFICIENT_ACCESS_RIGHTS:
     *             System.out.println( "You do not have the access rights to perform this operation." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Could not read the specified entry." );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Found the specified entry." );
     * </PRE>
     *
     * @param DN Distinguished name of the entry that you want to retrieve.
     * @exception LDAPException Failed to find or read the specified entry
     * from the directory.
     * @return LDAPEntry Returns the specified entry or raises an exception
     * if the entry is not found.
     */
    public LDAPEntry read (String DN) throws LDAPException {
        return read (DN, null, defaultConstraints);
    }

    /**
     * Reads the entry for the specified distiguished name (DN) and retrieves all
     * attributes for the entry. This method allows the user to specify the
     * preferences for the read operation.
     * <P>
     *
     * For example, the following section of code reads the entry for
     * Barbara Jensen and retrieves all attributes for that entry.
     * <P>
     *
     * <PRE>
     * String findDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * LDAPEntry foundEntry = null;
     * try {
     *     foundEntry = myConn.read( findDN );
     * } catch ( LDAPException e ) {
     *     switch( e.getLDAPResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified entry does not exist." );
     *             break;
     *         case e.LDAP_PARTIAL_RESULTS:
     *             System.out.println( "Entry served by a different LDAP server." );
     *             break;
     *         case e.INSUFFICIENT_ACCESS_RIGHTS:
     *             System.out.println( "You do not have the access rights to perform this operation." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Could not read the specified entry." );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Found the specified entry." );
     * </PRE>
     *
     * @param DN Distinguished name of the entry that you want to retrieve.
     * @param cons Preferences for the read operation.
     * @exception LDAPException Failed to find or read the specified entry
     * from the directory.
     * @return LDAPEntry Returns the specified entry or raises an exception
     * if the entry is not found.
     */
    public LDAPEntry read (String DN, LDAPSearchConstraints cons)
        throws LDAPException {
        return read (DN, null, cons);
    }

    /**
     * Reads the entry for the specified distinguished name (DN) and
     * retrieves only the specified attributes from the entry.
     * <P>
     *
     * For example, the following section of code reads the entry for
     * Barbara Jensen and retrieves only the <CODE>cn</CODE> and
     * <CODE>sn</CODE> attributes.
     * The example prints out all attributes that have been retrieved
     * (the two specified attributes).
     * <P>
     *
     * <PRE>
     * String findDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * LDAPEntry foundEntry = null;
     * String getAttrs[] = { "cn", "sn" };
     * try {
     *      foundEntry = myConn.read( findDN, getAttrs );
     * } catch ( LDAPException e ) {
     *      switch( e.getLDAPResultCode() ) {
     *           case e.NO_SUCH_OBJECT:
     *               System.out.println( "The specified entry does not exist." );
     *               break;
     *           case e.LDAP_PARTIAL_RESULTS:
     *               System.out.println( "Entry served by a different LDAP server." );
     *               break;
     *           case e.INSUFFICIENT_ACCESS_RIGHTS:
     *               System.out.println( "You do not have the access " +
     *                                   "rights to perform this operation." );
     *               break;
     *           default:
     *               System.out.println( "Error number: " + e.getLDAPResultCode() );
     *               System.out.println( "Could not read the specified entry." );
     *               break;
     *      }
     *      return;
     * }
     *
     * LDAPAttributeSet foundAttrs = foundEntry.getAttributeSet();
     * int size = foundAttrs.size();
     * Enumeration enumAttrs = foundAttrs.getAttributes();
     * System.out.println( "Attributes: " );
     *
     * while ( enumAttrs.hasMoreElements() ) {
     *      LDAPAttribute anAttr = ( LDAPAttribute )enumAttrs.nextElement();
     *      String attrName = anAttr.getName();
     *      System.out.println( "\t" + attrName );
     *      Enumeration enumVals = anAttr.getStringValues();
     *      while ( enumVals.hasMoreElements() ) {
     *           String aVal = ( String )enumVals.nextElement();
     *           System.out.println( "\t\t" + aVal );
     *      }
     * }
     * </PRE>
     *
     * @param DN Distinguished name of the entry that you want to retrieve.
     * @param attrs Names of attributes that you want to retrieve.
     * @return LDAPEntry Returns the specified entry (or raises an
     * exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     */
    public LDAPEntry read (String DN, String attrs[]) throws LDAPException {
        return read(DN, attrs, defaultConstraints);
    }

    public LDAPEntry read (String DN, String attrs[],
        LDAPSearchConstraints cons) throws LDAPException {
        LDAPSearchResults results = search (DN, LDAPv2.SCOPE_BASE,
          defaultFilter, attrs, false, cons);
        if (results == null)
            return null;
        return results.next ();
    }

    /**
     * Reads the entry specified by the LDAP URL. <P>
     *
     * When you call this method, a new connection is created automatically,
     * using the host and port specified in the URL. After finding the entry,
     * the method closes this connection (in other words, it disconnects from
     * the LDAP server). <P>
     *
     * If the URL specifies a filter and scope, these are not used.
     * Of the information specified in the URL, this method only uses
     * the LDAP host name and port number, the base distinguished name (DN),
     * and the list of attributes to return. <P>
     *
     * The method returns the entry specified by the base DN. <P>
     *
     * (Note: If you want to search for more than one entry, use the
     * <CODE>search( LDAPUrl )</CODE> method instead.) <P>
     *
     * For example, the following section of code reads the entry specified
     * by the LDAP URL.
     * <P>
     *
     * <PRE>
     * String flatURL = "ldap://alway.mcom.com:3890/cn=Barbara Jenson,ou=Product Development,o=Ace Industry,c=US?cn,sn,mail";
     * LDAPUrl myURL;
     * try {
     *    myURL = new LDAPUrl( flatURL );
     * } catch ( java.net.MalformedURLException e ) {
     *    System.out.println( "BAD URL!!!  BAD, BAD, BAD URL!!!" );
     *    return;
     * }
     * LDAPEntry myEntry = null;
     * try {
     *    myEntry = myConn.read( myURL );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getLDAPResultCode();
     *    switch( errCode ) {
     *        case ( e.NO_SUCH_OBJECT ):
     *            System.out.println( "The specified entry " + myDN +
     *                                " does not exist in the directory." );
     *            return;
     *        default:
     *            System.out.println( "An internal error occurred." );
     *            return;
     *    }
     * }
     * </PRE>
     *
     * @param toGet LDAP URL specifying the entry that you want to read.
     * @return LDAPEntry Returns the entry specified by the URL (or raises
     * an exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     * @see netscape.ldap.LDAPUrl
     * @see netscape.ldap.LDAPConnection#search(netscape.ldap.LDAPUrl)
     */
    public static LDAPEntry read (LDAPUrl toGet) throws LDAPException {
        String host = toGet.getHost ();
        int port = toGet.getPort();

        if (host == null)
            throw new LDAPException ( "no host for connection",
                                    LDAPException.PARAM_ERROR );

        String[] attributes = toGet.getAttributeArray ();
        String DN = toGet.getDN();
        LDAPEntry returnValue;

        LDAPConnection connection = new LDAPConnection ();
        connection.connect (host, port);

        returnValue = connection.read (DN, attributes);
        connection.disconnect ();

        return returnValue;
    }

    /**
     * Performs the search specified by the LDAP URL. <P>
     *
     * For example, the following section of code searches for all entries under
     * the <CODE>ou=Product Development,o=Ace Industry,c=US</CODE> subtree of a
     * directory.  The example gets and prints the mail attribute for each entry
     * found. <P>
     *
     * <PRE>
     * String flatURL = "ldap://alway.mcom.com:3890/ou=Product Development,o=Ace Industry,c=US?mail?sub?objectclass=*";
     * LDAPUrl myURL;
     * try {
     *    myURL = new LDAPUrl( flatURL );
     * } catch ( java.net.MalformedURLException e ) {
     *    System.out.println( "Incorrect URL syntax." );
     *    return;
     * }
     *
     * LDAPSearchResults myResults = null;
     * try {
     *    myResults = myConn.search( myURL );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getLDAPResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     *
     * while ( myResults.hasMoreElements() ) {
     *    LDAPEntry myEntry = myResults.next();
     *    String nextDN = myEntry.getDN();
     *    System.out.println( nextDN );
     *    LDAPAttributeSet entryAttrs = myEntry.getAttributeSet();
     *    Enumeration attrsInSet = entryAttrs.getAttributes();
     *    while ( attrsInSet.hasMoreElements() ) {
     *        LDAPAttribute nextAttr = (LDAPAttribute)attrsInSet.nextElement();
     *        String attrName = nextAttr.getName();
     *        System.out.print( "\t" + attrName + ": " );
     *        Enumeration valsInAttr = nextAttr.getStringValues();
     *        while ( valsInAttr.hasMoreElements() ) {
     *            String nextValue = (String)valsInAttr.nextElement();
     *            System.out.println( nextValue );
     *        }
     *    }
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param toGet LDAP URL representing the search that you want to perform.
     * @return LDAPSearchResults The results of the search as an enumeration.
     * @exception LDAPException Failed to complete the search specified by
     * the LDAP URL.
     * @see netscape.ldap.LDAPUrl
     * @see netscape.ldap.LDAPSearchResults
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
     */
    public static LDAPSearchResults search (LDAPUrl toGet) throws LDAPException {
        return search (toGet, null);
    }

    /**
     * Performs the search specified by the LDAP URL. This method also
     * allows you to specify constraints for the search (such as the
     * maximum number of entries to find or the
     * maximum time to wait for search results). <P>
     *
     * As part of the search constraints, you can specify whether or not you
     * want the results delivered all at once or in smaller batches.
     * If you specify that you want the results delivered in smaller
     * batches, each iteration blocks until the next batch of results is
     * returned. <P>
     *
     * For example, the following section of code retrieves the first 5
     * matching entries for the search specified by the LDAP URL.  The
     * example accomplishes this by creating a new set of search
     * constraints where the maximum number of search results is 5. <P>
     *
     * <PRE>
     * LDAPSearchConstraints mySearchConstraints = myConn.getSearchConstraints();
     * mySearchConstraints.setMaxResults( 5 );
     * String flatURL = "ldap://alway.mcom.com:3890/ou=Product Development,o=Ace Industry,c=US?mail?sub?objectclass=*";
     * LDAPUrl myURL;
     * try {
     *    myURL = new LDAPUrl( flatURL );
     * } catch ( java.net.MalformedURLException e ) {
     *    System.out.println( "Incorrect URL syntax." );
     *    return;
     * }
     * LDAPSearchResults myResults = null;
     * try {
     *    myResults = myConn.search( myURL, mySearchConstraints );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getLDAPResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param toGet LDAP URL representing the search that you want to run.
     * @param cons Constraints specific to the search.
     * @return LDAPSearchResults The results of the search as an enumeration.
     * @exception LDAPException Failed to complete the search specified
     * by the LDAP URL.
     * @see netscape.ldap.LDAPUrl
     * @see netscape.ldap.LDAPSearchResults
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
     */
    public static LDAPSearchResults search (LDAPUrl toGet,
        LDAPSearchConstraints cons) throws LDAPException {
        String host = toGet.getHost ();
        int port = toGet.getPort();

        if (host == null)
            throw new LDAPException ( "no host for connection",
                                    LDAPException.PARAM_ERROR );

        String[] attributes = toGet.getAttributeArray ();
        String DN = toGet.getDN();
        String filter = toGet.getFilter();
        if (filter == null) {
            filter = "(objectClass=*)";
        }
        int scope = toGet.getScope ();

        LDAPConnection connection = new LDAPConnection ();
        connection.connect (host, port);

        LDAPSearchResults results;
        if (cons != null)
            results = connection.search (DN, scope, filter, attributes, false, cons);
        else
            results = connection.search (DN, scope, filter, attributes, false);

        results.closeOnCompletion(connection);

        return results;
    }

    /**
     * Performs the search specified by the criteria that you enter. <P>
     *
     * For example, the following section of code searches for all entries under
     * the <CODE>ou=Product Development,o=Ace Industry,c=US</CODE> subtree of a
     * directory.  The example gets and prints the mail attribute for each entry
     * found. <P>
     *
     * <PRE>
     * String myBaseDN = "ou=Product Development,o=Ace Industry,c=US";
     * String myFilter="(objectclass=*)";
     * String[] myAttrs = { "mail" };
     *
     * LDAPSearchResults myResults = null;
     * try {
     *    myResults = myConn.search( myBaseDN, LDAPv2.SCOPE_SUB, myFilter, myAttrs, false );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getLDAPResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     *
     * while ( myResults.hasMoreElements() ) {
     *    LDAPEntry myEntry = myResults.next();
     *    String nextDN = myEntry.getDN();
     *    System.out.println( nextDN );
     *    LDAPAttributeSet entryAttrs = myEntry.getAttributeSet();
     *    Enumeration attrsInSet = entryAttrs.getAttributes();
     *    while ( attrsInSet.hasMoreElements() ) {
     *        LDAPAttribute nextAttr = (LDAPAttribute)attrsInSet.nextElement();
     *        String attrName = nextAttr.getName();
     *        System.out.println( "\t" + attrName + ":" );
     *        Enumeration valsInAttr = nextAttr.getStringValues();
     *        while ( valsInAttr.hasMoreElements() ) {
     *            String nextValue = (String)valsInAttr.nextElement();
     *            System.out.println( "\t\t" + nextValue );
     *        }
     *    }
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base The base distinguished name to search from
     * @param scope The scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter Search filter specifying the search criteria.
     * @param attrs List of attributes that you want returned in the
     * search results.
     * @param attrsOnly If true, returns the names but not the values of the
     * attributes found.  If false, returns the names and values for
     * attributes found
     * @return LDAPSearchResults The results of the search as an enumeration.
     * @exception LDAPException Failed to complete the specified search.
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
     */
    public LDAPSearchResults search( String base, int scope, String filter,
        String[] attrs, boolean attrsOnly ) throws LDAPException {
        return search( base, scope, filter, attrs, attrsOnly, defaultConstraints);
    }

    /**
     * Performs the search specified by the criteria that you enter.
     * This method also allows you to specify constraints for the search
     * (such as the maximum number of entries to find or the
     * maximum time to wait for search results). <P>
     *
     * As part of the search constraints, you can specify whether or not
     * you want the
     * results delivered all at once or in smaller batches.  If you
     * specify that you want the results delivered in smaller batches,
     * each iteration blocks until the
     * next batch of results is returned. <P>
     *
     * For example, the following section of code retrieves the first 5 entries
     * matching the specified search criteria.  The example accomplishes
     * this by creating a new set of search constraints where the maximum
     * number of search results is 5. <P>
     *
     * <PRE>
     * String myBaseDN = "ou=Product Development,o=Ace Industry,c=US";
     * String myFilter="(objectclass=*)";
     * String[] myAttrs = { "mail" };
     * LDAPSearchConstraints mySearchConstraints = myConn.getSearchConstraints();
     * mySearchConstraints.setMaxResults( 5 );
     *
     * LDAPSearchResults myResults = null;
     * try {
     *    myResults = myConn.search( myBaseDN, LDAPv2.SCOPE_SUB, myFilter, myAttrs, false, mySearchConstraints );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getLDAPResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base The base distinguished name to search from
     * @param scope The scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter Search filter specifying the search criteria.
     * @param attrs List of attributes that you want returned in the search
     * results.
     * @param cons Constraints specific to this search (for example, the
     * maximum number
     * of entries to return).
     * @param attrsOnly If true, returns the names but not the values of the
     * attributes found.  If false, returns the names and  values for
     * attributes found
     * @return LDAPSearchResults The results of the search as an enumeration.
     * @exception LDAPException Failed to complete the specified search.
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
     */
    public LDAPSearchResults search( String base, int scope, String filter,
        String[] attrs, boolean attrsOnly, LDAPSearchConstraints cons )
        throws LDAPException {

        if (cons == null)
            cons = defaultConstraints;

        LDAPSearchResults returnValue = new LDAPSearchResults(this,
            cons, base, scope, filter, attrs, attrsOnly);
        Vector cacheValue = null;
        Long key = null;
        boolean isKeyValid = true;

        try {
            // get entry from cache which is a vector of JDAPMessages
            if (m_cache != null)
            {
                // create key for cache entry using search arguments
                key = m_cache.createKey(host, port, base, filter, scope, attrs, boundDN, cons);

                cacheValue = (Vector)m_cache.getEntry(key);

                if (cacheValue != null)
                    return (new LDAPSearchResults(cacheValue, this, cons, base, scope,
                        filter, attrs, attrsOnly));
            }
        } catch (LDAPException e) {
            isKeyValid = false;
            printDebug("Exception: "+e);
        }

        bind(cons);

        LDAPSearchListener myListener = getSearchListener ( cons );
        int deref = cons.getDereference();

        JDAPSearchRequest request = new JDAPSearchRequest (base,
            scope, deref, cons.getMaxResults(), cons.getTimeLimit(),
            attrsOnly, filter, attrs);

        synchronized(myListener) {
            boolean success = false;
            try {
                sendRequest (request, myListener, cons);
                success = true;
            } finally {
                if (!success)
                releaseSearchListener (myListener);
            }

            // if using cache, then need to add the key to the search listener.
            // The search listener retrieves the key and then add the key and
            // a vector of results to the hashtable.
            if ((m_cache != null) && (isKeyValid))
                myListener.setKey(key);
        }


        /* Synchronous search if all requested at once */
        if ( cons.getBatchSize() == 0 ) {
            try {
                /* Block until all results are in */
                JDAPMessage response = myListener.getResponse ();
                Enumeration results = myListener.getSearchResults ();

                try {
                    checkSearchMsg(returnValue, response, cons, base, scope,
                                   filter, attrs, attrsOnly);
                } catch ( LDAPException ex ) {
                    /* Was the exception caused by a bad referral? */
                    JDAPProtocolOp op = response.getProtocolOp();
                    if ( (op instanceof JDAPSearchResultReference) ||
                         (op instanceof JDAPSearchResult) ){
                        System.err.println( "LDAPConnection.checkSearchMsg: " +
                                            "ignoring bad referral" );
                    } else {
                        throw ex;
                    }
                }

                while (results.hasMoreElements ()) {
                    JDAPMessage msg = (JDAPMessage)results.nextElement();

                    checkSearchMsg(returnValue, msg, cons, base, scope, filter, attrs,
                        attrsOnly);
                }
            } catch (LDAPException ee) {
                throw ee;
            } finally {
                releaseSearchListener (myListener);
            }
        } else {
            /*
            * Asynchronous to retrieve one at a time, check to make sure
            * the search didn't fail
            */
            JDAPMessage firstResult = myListener.nextResult ();
            if (firstResult == null) {
                firstResult = myListener.getResponse ();

                try {
                    checkSearchMsg(returnValue, firstResult, cons, base, scope,
                      filter, attrs, attrsOnly);
                } finally {
                    releaseSearchListener (myListener);
                }
            } else {
                /* First result could be a bad referral, ultimately causing
                   a NO_SUCH_OBJECT exception. Don't want to miss all
                   following results, so skip it. */
                try {
                    checkSearchMsg(returnValue, firstResult, cons, base,
                                   scope, filter, attrs, attrsOnly);
                } catch ( LDAPException ex ) {
                    /* Was the exception caused by a bad referral? */
                    if (firstResult.getProtocolOp() instanceof
                        JDAPSearchResultReference) {
                        System.err.println( "LDAPConnection.checkSearchMsg: " +
                                            "ignoring bad referral" );
                    } else {
                        throw ex;
                    }
                }

                LDAPControl[] controls = (LDAPControl[])getOption(LDAPv3.SERVERCONTROLS, cons);

                for (int i=0; (controls != null) && (i<controls.length); i++) {
                    if ((controls[i].getClass().getName()).equals(PersistSearchPackageName))
                    {
                        returnValue.associatePersistentSearch (myListener);
                        return returnValue;
                    }
                }
                /* we let this listener get garbage collected.. */
                returnValue.associate (myListener);
            }
        }
        return returnValue;
    }

    void checkSearchMsg(LDAPSearchResults value, JDAPMessage msg,
        LDAPSearchConstraints cons, String dn, int scope, String filter,
        String attrs[], boolean attrsOnly) throws LDAPException {
        try {
            checkMsg (msg);
            // not the JDAPResult
            if (msg.getProtocolOp().getType() != JDAPProtocolOp.SEARCH_RESULT) {
                value.add (msg.getProtocolOp());
            }
        } catch (LDAPReferralException e) {
            Vector res = new Vector();
            performReferrals(e, cons, JDAPProtocolOp.SEARCH_REQUEST, dn,
                scope, filter, attrs, attrsOnly, null, null, null, res);

            // the size of the vector can be more than 1 because it is possible
            // to visit more than one referral url to retrieve the entries
            for (int i=0; i<res.size(); i++)
                value.addReferralEntries((LDAPSearchResults)res.elementAt(i));

            res = null;
        } catch (LDAPException e) {
            if ((e.getLDAPResultCode() == LDAPException.ADMIN_LIMIT_EXCEEDED) ||
                (e.getLDAPResultCode() == LDAPException.TIME_LIMIT_EXCEEDED) ||
                (e.getLDAPResultCode() == LDAPException.SIZE_LIMIT_EXCEEDED)) {
                value.add(e);
            } else
                throw e;
        }
    }

    /**
     * Checks to see if an entry contains an attribute with a specified value.
     * Returns <CODE>true</CODE> if the entry has the value. Returns
     * <CODE>false</CODE> if the entry does not have the value or the
     * attribute. To represent the value that you want compared, you need
     * to create an <CODE>LDAPAttribute</CODE> object.<P>
     *
     * Note that only string values can be compared. <P>
     *
     * For example, the following section of code checks to see if the entry
     * "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US" contains
     * the attribute "mail" with the value "bjensen@aceindustry.com".
     *
     * <PRE>
     * ...
     * LDAPConnection myConn = new LDAPConnection();
     * ...
     * String myDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String nameOfAttr = "mail";
     * String valOfAttr = "bjensen@aceindustry.com";
     * LDAPAttribute cmpThisAttr = new LDAPAttribute( nameOfAttr, valOfAttr );
     * boolean hasValue = myConn.compare( myDN, cmpThisAttr );
     * if ( hasValue ) {
     *     System.out.println( "Attribute and value found in entry." );
     * } else {
     *     System.out.println( "Attribute and value not found in entry." );
     * }
     * ...</PRE>
     *
     * @param DN The distinguished name of the entry that you want to use in
     * the comparison.
     * @param attr The attribute that you want to compare against the entry.
     * (The method checks to see if the entry has an attribute with the same name
     * and value as this attribute.)
     * @return true if the entry contains the specified attribute and value.
     * @exception LDAPException Failed to perform the comparison.
     * @see netscape.ldap.LDAPAttribute
     */
    public boolean compare( String DN, LDAPAttribute attr )
        throws LDAPException {
        return compare(DN, attr, defaultConstraints);
    }

    public boolean compare( String DN, LDAPAttribute attr,
        LDAPSearchConstraints cons) throws LDAPException {
        bind(cons);

        LDAPResponseListener myListener = getResponseListener ();
        Enumeration en = attr.getByteValues();
        byte val[] = (byte[])en.nextElement();
        String debug = "";
        try {
            debug = new String(val, "UTF8");
        } catch(Throwable x)
        {}

        JDAPAVA ass = new JDAPAVA(attr.getName(), debug);
        //JDAPAVA ass = new JDAPAVA(attr.getName(), new String(val,0));

        JDAPMessage response;
        try {
            sendRequest (new JDAPCompareRequest (DN, ass), myListener, cons);
            response = myListener.getResponse ();

            int resultCode = ((JDAPResult)response.getProtocolOp()).getResultCode();
            if (resultCode == JDAPResult.COMPARE_FALSE)
                return false;
            if (resultCode == JDAPResult.COMPARE_TRUE)
                return true;

            checkMsg (response);

        } catch (LDAPReferralException e) {
            Vector res = new Vector();
            performReferrals(e, cons, JDAPProtocolOp.COMPARE_REQUEST,
              DN, 0, null, null, false, null, null, attr, res);
            boolean bool = false;
            if (res.size() > 0)
               bool = ((Boolean)res.elementAt(0)).booleanValue();
            res = null;
            return bool;
        } finally {
            releaseResponseListener (myListener);
        }
        return false; /* this should never be executed */
    }

    /**
     * Adds an entry to the directory. <P>
     *
     * Before using this method, you need to create an
     * <CODE>LDAPEntry</CODE> object and use it to specify the
     * distinguished name and attributes of the new entry. Make sure
     * to specify values for all required attributes in the
     * entry. If all required attributes are not specified and the LDAP server
     * checks the entry against the schema, an <CODE>LDAPException</CODE>
     * may be thrown (where the LDAP result code is
     * <CODE>OBJECT_CLASS_VIOLATION</CODE>).<P>
     *
     * For example, the following section of code creates an
     * <CODE>LDAPEntry</CODE> object for a new entry and uses the object
     * to add the new entry to the directory. Because the definition of
     * the LDAP <CODE>inetOrgPerson</CODE> class specifies that the
     * <CODE>cn</CODE>, <CODE>sn</CODE>, and <CODE>objectclass</CODE>
     * attributes are required, these attributes are specified as part
     * of the new entry.  (<CODE>mail</CODE> is not required but is shown
     * here as an example of specifying additional attributes.)
     * <P>
     *
     * <PRE>
     * ...
     * String myDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     *
     * LDAPAttribute attr1 = new LDAPAttribute( "cn", "Barbara Jensen" );
     * LDAPAttribute attr2 = new LDAPAttribute( "sn", "Jensen" );
     * LDAPAttribute attr3 = new LDAPAttribute( "objectclass", "top" );
     * LDAPAttribute attr4 = new LDAPAttribute( "objectclass", "person" );
     * LDAPAttribute attr5 = new LDAPAttribute( "objectclass", "organizationalPerson" );
     * LDAPAttribute attr6 = new LDAPAttribute( "objectclass", "inetOrgPerson" );
     * LDAPAttribute attr7 = new LDAPAttribute( "mail", "bjensen@aceindustry.com" );
     *
     * LDAPAttributeSet myAttrs = new LDAPAttributeSet();
     * myAttrs.add( attr1 );
     * myAttrs.add( attr2 );
     * myAttrs.add( attr3 );
     * myAttrs.add( attr4 );
     * myAttrs.add( attr5 );
     * myAttrs.add( attr6 );
     * myAttrs.add( attr7 );
     *
     * LDAPEntry myEntry = new LDAPEntry( myDN, myAttrs );
     *
     * myConn.add( myEntry );
     * ... </PRE>
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry.
     * @exception LDAPException Failed to add the specified entry to the
     * directory.
     * @see netscape.ldap.LDAPEntry
     */
    public void add( LDAPEntry entry ) throws LDAPException {
        add(entry, defaultConstraints);
    }

    /**
     * Adds an entry to the directory and allows you to specify preferences
     * for this LDAP add operation by using an
     * <CODE>LDAPSearchConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry.
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to add the specified entry to the
     * directory.
     * @see netscape.ldap.LDAPEntry
     * @see netscape.ldap.LDAPSearchConstraints
     */
    public void add( LDAPEntry entry, LDAPSearchConstraints cons )
        throws LDAPException {
        bind (cons);

        LDAPResponseListener myListener = getResponseListener ();
        LDAPAttributeSet attrs = entry.getAttributeSet ();
        LDAPAttribute[] attrList = new LDAPAttribute[attrs.size()];
        for( int i = 0; i < attrs.size(); i++ )
            attrList[i] = (LDAPAttribute)attrs.elementAt( i );
        int attrPosition = 0;
        JDAPMessage response;
        try {
            sendRequest (new JDAPAddRequest (entry.getDN(), attrList),
                        myListener, cons);
            response = myListener.getResponse();
            checkMsg (response);
        } catch (LDAPReferralException e) {
            performReferrals(e, cons, JDAPProtocolOp.ADD_REQUEST,
                null, 0, null, null, false, null, entry, null, null);
        } finally {
            releaseResponseListener (myListener);
        }
    }

    /**
     * Performs an extended operation on the directory.  Extended operations
     * are part of version 3 of the LDAP protocol.<P>
     *
     * Note that in order for the extended operation to work, the server
     * that you are connecting to must support LDAP v3 and must be configured
     * to process the specified extended operation.
     *
     * @param op LDAPExtendedOperation object specifying the OID of the
     * extended operation and the data to be used in the operation.
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see netscape.ldap.LDAPExtendedOperation
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op )
        throws LDAPException {

        return extendedOperation(op, defaultConstraints);
    }

    /**
     * Performs an extended operation on the directory.  Extended operations
     * are part of version 3 of the LDAP protocol. This method allows the
     * user to set the preferences for the operation.<P>
     *
     * Note that in order for the extended operation to work, the server
     * that you are connecting to must support LDAP v3 and must be configured
     * to process the specified extended operation.
     *
     * @param op LDAPExtendedOperation object specifying the OID of the
     * extended operation and the data to be used in the operation.
     * @param cons Preferences for the extended operation.
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see netscape.ldap.LDAPExtendedOperation
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op,
      LDAPSearchConstraints cons) throws LDAPException {
        bind (cons);

        LDAPResponseListener myListener = getResponseListener ();
        JDAPMessage response = null;
        byte[] results = null;
        String resultID;

        try {
            sendRequest ( new JDAPExtendedRequest( op.getID(), op.getValue() ),
                         myListener, cons );
            response = myListener.getResponse();
            checkMsg (response);
            JDAPExtendedResponse res = (JDAPExtendedResponse)response.getProtocolOp();
            results = res.getValue();
            resultID = res.getID();
        } catch (LDAPReferralException e) {
            return performExtendedReferrals( e, cons, op );
        } finally {
            releaseResponseListener (myListener);
        }
        return new LDAPExtendedOperation( resultID, results );
    }

    /**
     * Makes a single change to an existing entry in the directory
     * (for example, changes the value of an attribute, adds a new
     * attribute value, or removes an existing attribute value). <P>
     *
     * Use the <CODE>LDAPModification</CODE> object to specify the change
     * that needs to be made and the <CODE>LDAPAttribute</CODE> object
     * to specify the attribute value that needs to be changed. The
     * <CODE>LDAPModification</CODE> object allows you add an attribute
     * value, change an attibute value, or remove an attribute
     * value. <P>
     *
     * For example, the following section of code changes Barbara Jensen's email
     * address in the directory to babs@aceindustry.com. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     *
     * LDAPAttribute attrEmail = new LDAPAttribute( "mail", "babs@aceindustry.com" );
     * LDAPModification singleChange = new LDAPModification( LDAPModification.REPLACE, attrEmail );
     *
     * myConn.modify( myEntryDN, singleChange );
     * ... </PRE>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mod A single change to be made to the entry
     * @exception LDAPException Failed to make the specified change to the
     * directory entry.
     * @see netscape.ldap.LDAPModification
     */
    public void modify( String DN, LDAPModification mod ) throws LDAPException {
        modify(DN, mod, defaultConstraints);
    }

   /**
    * Makes a single change to an existing entry in the directory and
    * allows you to specify preferences for this LDAP modify operation
    * by using an <CODE>LDAPSearchConstraints</CODE> object. For
    * example, you can specify whether or not to follow referrals.
    * You can also apply LDAP v3 controls to the operation.
    * <P>
    *
    * @param DN The distinguished name of the entry that you want to modify
    * @param mod A single change to be made to the entry
    * @param cons The set of preferences that you want applied to this operation.
    * @exception LDAPException Failed to make the specified change to the
    * directory entry.
    * @see netscape.ldap.LDAPModification
    * @see netscape.ldap.LDAPSearchConstraints
    */
    public void modify( String DN, LDAPModification mod,
        LDAPSearchConstraints cons ) throws LDAPException {
        LDAPModification[] mods = new LDAPModification [1];
        mods[0] = mod;
        modify (DN, mods, cons);
    }

    /**
     * Makes a set of changes to an existing entry in the directory
     * (for example, changes attribute values, adds new attribute values,
     * or removes existing attribute values). <P>
     *
     * Use the <CODE>LDAPModificationSet</CODE> object to specify the set
     * of changes that needs to be made.  Changes are specified in terms
     * of attribute values.  Each attribute value to be modified, added,
     * or removed must be specified by an <CODE>LDAPAttribute</CODE> object.
     * <P>
     *
     * For example, the following section of code changes Barbara Jensen's
     * title, adds a telephone number to the entry, and removes the room
     * number from the entry. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     *
     * LDAPModificationSet manyChanges = new LDAPModificationSet();
     * LDAPAttribute attrTelephoneNumber = new LDAPAttribute( "telephoneNumber",
     *                                                        "555-1212" );
     * manyChanges.add( LDAPModification.ADD, attrTelephoneNumber );
     * LDAPAttribute attrRoomNumber = new LDAPAttribute( "roomnumber", "222" );
     * manyChanges.add( LDAPModification.DELETE, attrRoomNumber );
     * LDAPAttribute attrTitle = new LDAPAttribute( "title",
     *                                       "Manager of Product Development" );
     * manyChanges.add( LDAPModification.REPLACE, attrTitle );
     *
     * myConn.modify( myEntryDN, manyChanges );
     * ... </PRE>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mods A set of changes to be made to the entry
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModificationSet
     */
    public void modify (String DN, LDAPModificationSet mods)
        throws LDAPException {
        modify(DN, mods, defaultConstraints);
    }

    /**
     * Makes a set of changes to an existing entry in the directory and
     * allows you to specify preferences for this LDAP modify operation
     * by using an <CODE>LDAPSearchConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mods A set of changes to be made to the entry
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModificationSet
     * @see netscape.ldap.LDAPSearchConstraints
     */
     public void modify (String DN, LDAPModificationSet mods,
         LDAPSearchConstraints cons) throws LDAPException {
         LDAPModification[] modList = new LDAPModification[mods.size()];
         for( int i = 0; i < mods.size(); i++ )
             modList[i] = mods.elementAt( i );
         modify (DN, modList, cons);
     }

    /**
     * Makes a set of changes to an existing entry in the directory
     * (for example, changes attribute values, adds new attribute values,
     * or removes existing attribute values). <P>
     *
     * Use an array of <CODE>LDAPModification</CODE> objects to specify the
     * changes that need to be made.  Each change must be specified by
     * an <CODE>LDAPModification</CODE> object, and each attribute value
     * to be modified, added, or removed must be specified by an
     * <CODE>LDAPAttribute</CODE> object. <P>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mods An array of objects representing the changes to be made
     * to the entry
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModification
     */
    public void modify (String DN, LDAPModification[] mods)
        throws LDAPException {
        modify(DN, mods, defaultConstraints);
    }

    /**
     * Makes a set of changes to an existing entry in the directory and
     * allows you to specify preferences for this LDAP modify operation
     * by using an <CODE>LDAPSearchConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mods An array of objects representing the changes to be made
     * to the entry
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModification
     * @see netscape.ldap.LDAPSearchConstraints
     */
    public void modify (String DN, LDAPModification[] mods,
         LDAPSearchConstraints cons) throws LDAPException {
         bind (cons);

         LDAPResponseListener myListener = getResponseListener ();
         JDAPMessage response = null;
         try {
             sendRequest (new JDAPModifyRequest (DN, mods), myListener, cons);
             response = myListener.getResponse();
             checkMsg (response);
         } catch (LDAPReferralException e) {
             performReferrals(e, cons, JDAPProtocolOp.MODIFY_REQUEST,
               DN, 0, null, null, false, mods, null, null, null);
         } finally {
             releaseResponseListener (myListener);
         }
    }

    /**
     * Deletes the entry for the specified DN from the directory. <P>
     *
     * For example, the following section of code deletes the entry for
     * Barbara Jensen from the directory. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * myConn.delete( myEntryDN );
     * ... </PRE>
     *
     * @param DN Distinguished name identifying the entry that you want
     * to remove from the directory.
     * @exception LDAPException Failed to delete the specified entry from
     * the directory.
     */
    public void delete( String DN ) throws LDAPException {
        delete(DN, defaultConstraints);
    }

    /**
     * Deletes the entry for the specified DN from the directory and
     * allows you to specify preferences for this LDAP delete operation
     * by using an <CODE>LDAPSearchConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN Distinguished name identifying the entry that you want
     * to remove from the directory.
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to delete the specified entry from
     * the directory.
     * @see netscape.ldap.LDAPSearchConstraints
     */
    public void delete( String DN, LDAPSearchConstraints cons )
        throws LDAPException {
        bind (cons);

        LDAPResponseListener myListener = getResponseListener ();
        JDAPMessage response;
        try {
            sendRequest (new JDAPDeleteRequest (DN), myListener, cons);
            response = myListener.getResponse();
            checkMsg (response);
        } catch (LDAPReferralException e) {
            performReferrals(e, cons, JDAPProtocolOp.DEL_REQUEST,
              DN, 0, null, null, false, null, null, null, null);
        } finally {
            releaseResponseListener (myListener);
        }
    }

    /**
     * Renames an existing entry in the directory. <P>
     *
     * You can specify whether or not the original name of the entry is
     * retained as a value in the entry. For example, suppose you rename
     * the entry "cn=Barbara" to "cn=Babs".  You can keep "cn=Barbara"
     * as a value in the entry so that the cn attribute has two values: <P>
     *
     * <PRE>
     *       cn=Barbara
     *       cn=Babs
     * </PRE>
     * The following example renames an entry.  The old name of the entry
     * is kept as a value in the entry. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String newRDN = "cn=Babs Jensen";
     * myConn.rename( myEntryDN, newRDN, false );
     * ... </PRE>
     *
     * @param DN Current distinguished name of the entry.
     * @param newRDN New relative distinguished name for the entry (for example,
     * "cn=newName").
     * @param deleteOldRDN If <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @exception LDAPException Failed to rename the specified entry.
     */
    public void rename (String DN, String newRDN, boolean deleteOldRDN )
        throws LDAPException {
        rename(DN, newRDN, null, deleteOldRDN);
    }

    /**
     * Renames an existing entry in the directory. <P>
     *
     * You can specify whether or not the original name of the entry is
     * retained as a value in the entry. For example, suppose you rename
     * the entry "cn=Barbara" to "cn=Babs".  You can keep "cn=Barbara"
     * as a value in the entry so that the cn attribute has two values: <P>
     *
     * <PRE>
     *       cn=Barbara
     *       cn=Babs
     * </PRE>
     * The following example renames an entry.  The old name of the entry
     * is kept as a value in the entry. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String newRDN = "cn=Babs Jensen";
     * myConn.rename( myEntryDN, newRDN, false );
     * ... </PRE>
     *
     * @param DN Current distinguished name of the entry.
     * @param newRDN New relative distinguished name for the entry (for example,
     * "cn=newName").
     * @param deleteOldRDN If <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to rename the specified entry.
     */
    public void rename (String DN, String newRDN, boolean deleteOldRDN,
        LDAPSearchConstraints cons )
          throws LDAPException {
        rename(DN, newRDN, null, deleteOldRDN, cons);
    }

    /**
     * Renames an existing entry in the directory and (optionally)
     * changes the location of the entry in the directory tree.<P>
     *
     * <B>NOTE: </B>Netscape Directory Server 3.0 does not support the
     * capability to move an entry to a different location in the
     * directory tree.  If you specify a value for the <CODE>newParentDN</CODE>
     * argument, an <CODE>LDAPException</CODE> will be thrown.
     * <P>
     *
     * @param DN Current distinguished name of the entry.
     * @param newRDN New relative distinguished name for the entry (for example,
     * "cn=newName").
     * @param newParentDN If not null, the distinguished name for the
     * entry under which the entry should be moved (for example, to move
     * an entry under the Accounting subtree, specify this argument as
     * "ou=Accounting, o=Ace Industry, c=US").
     * @param deleteOldRDN If <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @exception LDAPException Failed to rename the specified entry.
     */
     public void rename(String dn,
                        String newRDN,
                        String newParentDN,
                        boolean deleteOldRDN) throws LDAPException {
          rename(dn, newRDN, newParentDN, deleteOldRDN, defaultConstraints);
     }

    /**
     * Renames an existing entry in the directory and (optionally)
     * changes the location of the entry in the directory tree. Also
     * allows you to specify preferences for this LDAP modify DN operation
     * by using an <CODE>LDAPSearchConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * <B>NOTE: </B>Netscape Directory Server 3.0 does not support the
     * capability to move an entry to a different location in the
     * directory tree.  If you specify a value for the <CODE>newParentDN</CODE>
     * argument, an <CODE>LDAPException</CODE> will be thrown.
     * <P>
     *
     * @param DN Current distinguished name of the entry.
     * @param newRDN New relative distinguished name for the entry (for example,
     * "cn=newName").
     * @param newParentDN If not null, the distinguished name for the
     * entry under which the entry should be moved (for example, to move
     * an entry under the Accounting subtree, specify this argument as
     * "ou=Accounting, o=Ace Industry, c=US").
     * @param deleteOldRDN If <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to rename the specified entry.
     * @see netscape.ldap.LDAPSearchConstraints
     */
    public void rename (String DN,
                           String newRDN,
                           String newParentDN,
                           boolean deleteOldRDN,
                           LDAPSearchConstraints cons)
        throws LDAPException {
        bind (cons);

        LDAPResponseListener myListener = getResponseListener ();
        JDAPMessage response;
        try {
            JDAPModifyRDNRequest request = null;
            if ( newParentDN != null )
                request = new JDAPModifyRDNRequest (DN,
                                                newRDN,
                                                deleteOldRDN,
                                                newParentDN);
            else
                request = new JDAPModifyRDNRequest (DN,
                                                newRDN,
                                                deleteOldRDN);
            sendRequest (request, myListener, cons);
            response = myListener.getResponse();
            checkMsg (response);
        } catch (LDAPReferralException e) {
            performReferrals(e, cons, JDAPProtocolOp.MODIFY_RDN_REQUEST,
              DN, 0, newRDN, null, deleteOldRDN, null, null, null, null);
        } finally {
            releaseResponseListener (myListener);
        }
    }

    /**
     * Returns the value of the specified option for this
     * <CODE>LDAPConnection</CODE> object. <P>
     *
     * These options represent the search constraints for the current connection.
     * To get all search constraints for the current connection, call the
     * <CODE>getSearchConstraints</CODE> method.
     * <P>
     *
     * By default, the search constraints apply to all searches performed
     * through the current connection.  You can change these search constraints:
     * <P>
     *
     * <UL>
     * <LI>If you want to override these constraints only for a particular
     * search, create an <CODE>LDAPSearchConstraints</CODE> object with
     * your new constraints and pass it to the
     * <CODE>LDAPConnection.search</CODE> method.
     * <P>
     *
     * <LI>If you want to override these constraints for all searches
     * performed under the current connection, call the
     * <CODE>setOption</CODE> method to change the search constraint.
     * <P>
     *
     * </UL>
     * <P>
     *
     * For example, the following section of code gets and prints the
     * maximum number of search results that are returned for searches
     * performed through this connection.  (This applies to all searches
     * unless a different set of search constraints is specified in an
     * <CODE>LDAPSearchConstraints</CODE> object.)
     * <P>
     *
     * <PRE>
     * LDAPConnection ld = new LDAPConnection();
     * int sizeLimit = ( (Integer)ld.getOption( LDAPv2.SIZELIMIT ) ).intValue();
     * System.out.println( "Maximum number of results: " + sizeLimit );
     * </PRE>
     *
     * @param option You can specify one of the following options:
     * <TABLE CELLPADDING=5>
     * <TR VALIGN=BASELINE ALIGN=LEFT>
     * <TH>Option</TH><TH>Data Type</TH><TH>Description</TH></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.PROTOCOL_VERSION</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the version of the LDAP protocol used by the
     * client.
     * <P>By default, the value of this option is 2.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.DEREF</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies when your client dereferences aliases.
     *<PRE>
     * Legal values for this option are:
     *
     * DEREF_NEVER       Aliases are never dereferenced.
     *
     * DEREF_FINDING     Aliases are dereferenced when find-
     *                   ing the starting point for the
     *                   search (but not when searching
     *                   under that starting entry).
     *
     * DEREF_SEARCHING   Aliases are dereferenced when
     *                   searching the entries beneath the
     *                   starting point of the search (but
     *                   not when finding the starting
     *                   entry).
     *
     * DEREF_ALWAYS      Aliases are always dereferenced.
     *</PRE>
     * <P>By default, the value of this option is
     * <CODE>DEREF_NEVER</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.SIZELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to return.
     * If this option is set to 0, there is no maximum limit.
     * <P>By default, the value of this option is 1000.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.TIMELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of milliseconds to wait for results
     * before timing out. If this option is set to 0, there is no maximum
     * time limit.
     * <P>By default, the value of this option is 0.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS</CODE></TD>
     * <TD><CODE>Boolean</CODE></TD>
     * <TD>Specifies whether or not your client follows referrals automatically.
     * If <CODE>true</CODE>, your client follows referrals automatically.
     * If <CODE>false</CODE>, an <CODE>LDAPReferralException</CODE> is raised
     * when referral is detected.
     * <P>By default, the value of this option is <CODE>false</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS_REBIND_PROC</CODE></TD>
     * <TD><CODE>LDAPRebind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPRebind</CODE> interface.  You must define this class and
     * the <CODE>getRebindAuthentication</CODE> method that will be used to
     * get the distinguished name and password to use for authentication.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS_HOP_LIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of referrals in a sequence that
     * your client will follow.  (For example, if REFERRALS_HOP_LIMIT is 5,
     * your client will follow no more than 5 referrals in a row when resolving
     * a single LDAP request.)
     * <P>By default, the value of this option is 10.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.BATCHSIZE</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the number of search results to return at a time.
     * (For example, if BATCHSIZE is 1, results are returned one at a time.)
     * <P>By default, the value of this option is 1.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv3.CLIENTCONTROLS</CODE></TD>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the client controls that may affect the handling of LDAP
     * operations in the LDAP classes. These controls are used by the client
     * and are not passed to the LDAP server. At this time, no client controls
     * are defined for clients built with the Netscape LDAP classes. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv3.SERVERCONTROLS</CODE></TD>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the server controls that are passed to the LDAP
     * server on each LDAP operation. Not all servers support server
     * controls; a particular server may or may not support a given
     * server control.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>MAXBACKLOG</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to accumulate in an
     * LDAPSearchResults before suspending the reading of input from the server.
     * <P>By default, the value of this option is 100.</TD></TR>
     * </TABLE><P>
     * @return The value for the option wrapped in an object.  (You
     * need to cast the returned value as its appropriate type. For
     * example, when getting the SIZELIMIT option, cast the returned
     * value as an <CODE>Integer</CODE>.)
     * @exception LDAPException Failed to get the specified option.
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPSearchConstraints
     * @see netscape.ldap.LDAPReferralException
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getSearchConstraints
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     */
    public Object getOption( int option ) throws LDAPException {
        if (option == LDAPv2.PROTOCOL_VERSION) {
            return new Integer(protocolVersion);
        }

        return getOption(option, defaultConstraints);
    }

    private static Object getOption( int option, LDAPSearchConstraints cons )
        throws LDAPException {
        switch (option) {
            case LDAPv2.DEREF:
              return new Integer (cons.getDereference());
            case LDAPv2.SIZELIMIT:
              return new Integer (cons.getMaxResults());
            case LDAPv2.TIMELIMIT:
              return new Integer (cons.getTimeLimit());
            case LDAPv2.REFERRALS:
              return new Boolean (cons.getReferrals());
            case LDAPv2.REFERRALS_REBIND_PROC:
              return cons.getRebindProc();
            case LDAPv2.REFERRALS_HOP_LIMIT:
              return new Integer (cons.getHopLimit());
            case LDAPv2.BATCHSIZE:
              return new Integer (cons.getBatchSize());
            case LDAPv3.CLIENTCONTROLS:
              return cons.getClientControls();
            case LDAPv3.SERVERCONTROLS:
              return cons.getServerControls();
            case MAXBACKLOG:
              return new Integer (cons.getMaxBacklog());
            default:
              throw new LDAPException ( "invalid option",
                                        LDAPException.PARAM_ERROR );
        }
    }

    /**
     * Sets the value of the specified option for this
     * <CODE>LDAPConnection</CODE> object. <P>
     *
     * These options represent the search constraints for the current
     * connection.
     * To get all search constraints for the current connection, call the
     * <CODE>getSearchConstraints</CODE> method.
     * <P>
     *
     * By default, the option that you set applies to all subsequent
     * searches performed through the current connection. If you want to
     * set a constraint only for a particular search, create an
     * <CODE>LDAPSearchConstraints</CODE> object with your new constraints
     * and pass it to the <CODE>LDAPConnection.search</CODE> method.
     * <P>
     *
     * For example, the following section of code changes the constraint for
     * the maximum number of search results that are returned for searches
     * performed through this connection.  (This applies to all searches
     * unless a different set of search constraints is specified in an
     * <CODE>LDAPSearchConstraints</CODE> object.)
     * <P>
     *
     * <PRE>
     * LDAPConnection ld = new LDAPConnection();
     * Integer newLimit = new Integer( 20 );
     * ld.setOption( LDAPv2.SIZELIMIT, newLimit );
     * System.out.println( "Changed the maximum number of results to " + newLimit.intValue() );
     * </PRE>
     *
     * @param option You can specify one of the following options:
     * <TABLE CELLPADDING=5>
     * <TR VALIGN=BASELINE ALIGN=LEFT>
     * <TH>Option</TH><TH>Data Type</TH><TH>Description</TH></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.PROTOCOL_VERSION</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the version of the LDAP protocol used by the
     * client.
     * <P>By default, the value of this option is 2.  If you want
     * to use LDAP v3 features (such as extended operations or
     * controls), you need to set this value to 3. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.DEREF</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies when your client dereferences aliases.
     *<PRE>
     * Legal values for this option are:
     *
     * DEREF_NEVER       Aliases are never dereferenced.
     *
     * DEREF_FINDING     Aliases are dereferenced when find-
     *                   ing the starting point for the
     *                   search (but not when searching
     *                   under that starting entry).
     *
     * DEREF_SEARCHING   Aliases are dereferenced when
     *                   searching the entries beneath the
     *                   starting point of the search (but
     *                   not when finding the starting
     *                   entry).
     *
     * DEREF_ALWAYS      Aliases are always dereferenced.
     *</PRE>
     * <P>By default, the value of this option is
     * <CODE>DEREF_NEVER</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.SIZELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to return.
     * If this option is set to 0, there is no maximum limit.
     * <P>By default, the value of this option is 1000.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.TIMELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of milliseconds to wait for results
     * before timing out. If this option is set to 0, there is no maximum
     * time limit.
     * <P>By default, the value of this option is 0.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS</CODE></TD>
     * <TD><CODE>Boolean</CODE></TD>
     * <TD>Specifies whether or not your client follows referrals automatically.
     * If <CODE>true</CODE>, your client follows referrals automatically.
     * If <CODE>false</CODE>, an <CODE>LDAPReferralException</CODE> is
     * raised when a referral is detected.
     * <P>By default, the value of this option is <CODE>false</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS_REBIND_PROC</CODE></TD>
     * <TD><CODE>LDAPRebind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPRebind</CODE>
     * interface.  You must define this class and the
     * <CODE>getRebindAuthentication</CODE> method that will be used to get
     * the distinguished name and password to use for authentication.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS_HOP_LIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of referrals in a sequence that
     * your client will follow.  (For example, if REFERRALS_HOP_LIMIT is 5,
     * your client will follow no more than 5 referrals in a row when resolving
     * a single LDAP request.)
     * <P>By default, the value of this option is 10.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.BATCHSIZE</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the number of search results to return at a time.
     * (For example, if BATCHSIZE is 1, results are returned one at a time.)
     * <P>By default, the value of this option is 1.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv3.CLIENTCONTROLS</CODE></TD>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the client controls that may affect handling of LDAP
     * operations in the LDAP classes. These controls are used by the client
     * and are not passed to the server. At this time, no client controls
     * are defined for clients built with the Netscape LDAP classes. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv3.SERVERCONTROLS</CODE></TD>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the server controls that are passed to the LDAP
     * server on each LDAP operation. Not all servers support server
     * controls; a particular server may or may not support a particular
     * control.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>MAXBACKLOG</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to accumulate in an
     * LDAPSearchResults before suspending the reading of input from the server.
     * <P>By default, the value of this option is 100.</TD></TR>
     * </TABLE><P>
     * @param value The value to assign to the option.  The value must be
     * the java.lang object wrapper for the appropriate parameter
     * (e.g. boolean->Boolean,
     *   integer->Integer)
     * @exception LDAPException Failed to set the specified option.
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPSearchConstraints
     * @see netscape.ldap.LDAPReferralException
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getSearchConstraints
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     */
    public void setOption( int option, Object value ) throws LDAPException {
        if (option == LDAPv2.PROTOCOL_VERSION) {
            setProtocolVersion(((Integer)value).intValue());
            return;
        }
        setOption(option, value, defaultConstraints);
        if ( (option == MAXBACKLOG) && (th != null) ) {
            int val = ((Integer)value).intValue();
            if ( val >= 1 ) {
                th.setMaxBacklog( val );
            }
        }
    }

    private static void setOption( int option, Object value, LDAPSearchConstraints cons ) throws LDAPException {
      try {
        switch (option) {
        case LDAPv2.DEREF:
          cons.setDereference(((Integer)value).intValue());
          return;
        case LDAPv2.SIZELIMIT:
          cons.setMaxResults(((Integer)value).intValue());
          return;
        case LDAPv2.TIMELIMIT:
          cons.setTimeLimit(((Integer)value).intValue());
          return;
        case LDAPv2.REFERRALS:
          cons.setReferrals(((Boolean)value).booleanValue());
          return;
        case LDAPv2.REFERRALS_REBIND_PROC:
          cons.setRebindProc((LDAPRebind)value);
          return;
        case LDAPv2.REFERRALS_HOP_LIMIT:
          cons.setHopLimit(((Integer)value).intValue());
          return;
        case LDAPv2.BATCHSIZE:
          cons.setBatchSize(((Integer)value).intValue());
          return;
        case LDAPv3.CLIENTCONTROLS:
          if ( value == null )
            cons.setClientControls( (LDAPControl[]) null );
          else if ( value instanceof LDAPControl )
            cons.setClientControls( (LDAPControl) value );
          else if ( value instanceof LDAPControl[] )
            cons.setClientControls( (LDAPControl[])value );
          else
            throw new LDAPException ( "invalid LDAPControl",
                                      LDAPException.PARAM_ERROR );
          return;
        case LDAPv3.SERVERCONTROLS:
          if ( value == null )
            cons.setServerControls( (LDAPControl[]) null );
          else if ( value instanceof LDAPControl )
            cons.setServerControls( (LDAPControl) value );
          else if ( value instanceof LDAPControl[] )
            cons.setServerControls( (LDAPControl[])value );
          else
            throw new LDAPException ( "invalid LDAPControl",
                                      LDAPException.PARAM_ERROR );
          return;
        case MAXBACKLOG:
            int val = ((Integer)value).intValue();
            if ( val < 1 ) {
                throw new LDAPException ( "MAXBACKLOG must be at least 1",
                                          LDAPException.PARAM_ERROR );
            } else {
                cons.setMaxBacklog(((Integer)value).intValue());
            }
            return;
        default:
          throw new LDAPException ("invalid option",
                                   LDAPException.PARAM_ERROR );
        }
      } catch (ClassCastException cc) {
        throw new LDAPException ("invalid option value",
                                 LDAPException.PARAM_ERROR );
      }
    }

    /**
     * Returns an array of the latest controls (if any) from server.
     * @return An array of the controls returned by an operation, or
     * null if none.
     * @see netscape.ldap.LDAPControl
     */
    public LDAPControl[] getResponseControls() {
      LDAPControl[] controls = null;

      /* Get the latest controls returned for our thread */
      synchronized(m_responseControlTable) {
          Vector responses = (Vector)m_responseControlTable.get(th);

          if (responses != null) {
              // iterate through each response control
              for (int i=0,size=responses.size(); i<size; i++) {
                  ResponseControl responseObj =
                    (ResponseControl)responses.elementAt(i);

                  // check if the response belongs to this connection
                  if (responseObj.getConnection().equals(this)) {
                      controls = responseObj.getControls();
                      responses.removeElementAt(i);
                      break;
                  }
              }
          }
      }

      return controls;
    }

    /**
     * Returns the set of search constraints that apply to all searches
     * performed through this connection (unless you specify a different
     * set of search constraints when calling the <CODE>search</CODE>
     * method). <P>
     *
     * Note that if you want to get individual constraints (rather than
     * getting the
     * entire set of constraints), call the <CODE>getOption</CODE> method.
     * <P>
     *
     * Typically, you might call the <CODE>getSearchConstraints</CODE> method
     * if you want to create a slightly different set of search constraints
     * that you want to apply to a particular search.
     * <P>
     *
     * For example, the following section of code changes the maximum number
     * of results to 10 for a specific search. Rather than construct a new
     * set of search constraints from scratch, the example gets the current
     * settings for the connections and just changes the setting for the
     * maximum results.
     * <P>
     *
     * Note that this change only applies to the searches performed with this
     * custom set of constraints.  All other searches performed through this
     * connection use the original set of search constraints.
     * <P>
     *
     * <PRE>
     * ...
     * LDAPSearchConstraints myOptions = ld.getSearchConstraints();
     * myOptions.setMaxResults( 10 );
     * String[] myAttrs = { "objectclass" };
     * LDAPSearchResults myResults = ld.search( "o=Ace Industry,c=US",
     *                                          LDAPv2.SCOPE_SUB,
     *                                          "(objectclass=*)",
     *                                          myAttrs,
     *                                          false,
     *                                          myOptions );
     * ...
     * </PRE>
     *
     * @return The <CODE>LDAPSearchConstraints</CODE> object representing the
     * set of search constraints that apply (by default) to all searches
     * performed through this connection.
     * @see netscape.ldap.LDAPSearchConstraints
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     */
    public LDAPSearchConstraints getSearchConstraints () {
        return defaultConstraints;
    }

    /**
     * Get a new listening agent from the internal buffer of available agents.
     * These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @return response listener object
     */
    private synchronized LDAPResponseListener getResponseListener () {
        if (responseListeners == null)
            responseListeners = new Vector (5);

        LDAPResponseListener l;
        if ( responseListeners.size() < 1 ) {
            l = new LDAPResponseListener ( this );
        }
        else {
            l = (LDAPResponseListener)responseListeners.elementAt (0);
            responseListeners.removeElementAt (0);
        }
        return l;
    }

    /**
     * Get a new search listening agent from the internal buffer of available
     * agents. These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @return A search response listener object
     */
    private synchronized LDAPSearchListener getSearchListener (
        LDAPSearchConstraints cons )
    {
        if (searchListeners == null) {
            searchListeners = new Vector (5);
        }

        LDAPSearchListener l;
        if ( searchListeners.size() < 1 ) {
            l = new LDAPSearchListener ( this, cons );
        }
        else {
            l = (LDAPSearchListener)searchListeners.elementAt (0);
            searchListeners.removeElementAt (0);
        }
        return l;
    }

    /**
     * Put a listening agent into the internal buffer of available agents.
     * These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @param l Listener to buffer
     */
    private synchronized void releaseResponseListener (LDAPResponseListener l)
    {
        if (responseListeners == null)
            responseListeners = new Vector (5);

        l.reset ();
        responseListeners.addElement (l);
    }

    /**
     * Put a search listening agent into the internal buffer of available
     * agents. These objects are used to make the asynchronous LDAP
     * operations synchronous.
     * @param Listener to buffer
     */
    synchronized void releaseSearchListener (LDAPSearchListener l)
    {
        if (searchListeners == null)
            searchListeners = new Vector (5);

        l.reset ();
        searchListeners.addElement (l);
    }

    /**
     * Checks the message (assumed to be a return value).  If the resultCode
     * is anything other than SUCCESS, it throws an LDAPException describing
     * the server's (error) response.
     * @param m Server response to validate
     * @exception LDAPException failed to check message
     */
    void checkMsg (JDAPMessage m) throws LDAPException {
      if (m.getProtocolOp() instanceof JDAPResult) {
          JDAPResult response = (JDAPResult)(m.getProtocolOp());
          int resultCode = response.getResultCode ();

          if (resultCode == JDAPResult.SUCCESS)
              return;

          if (resultCode == JDAPResult.REFERRAL)
              throw new LDAPReferralException ("referral", resultCode,
                response.getReferrals());

          if (resultCode == JDAPResult.LDAP_PARTIAL_RESULTS)
              throw new LDAPReferralException ("referral", resultCode,
                response.getErrorMessage());
          else
              throw new LDAPException ("error result", resultCode,
                response.getErrorMessage(),
                response.getMatchedDN());

      } else if (m.getProtocolOp() instanceof JDAPSearchResultReference) {
          String[] referrals =
            ((JDAPSearchResultReference)m.getProtocolOp()).getUrls();
          throw new LDAPReferralException ("referral",
            JDAPResult.SUCCESS, referrals);
      } else
          return;
    }

    /**
     * Set response controls for the current connection for a particular 
     * thread. Get the oldest returned controls and remove them from the 
     * queue. If the connection is executing a persistent search, there may
     * be more than one set of controls in the queue. For any other 
     * operation, there will only ever be at most one set of controls 
     * (controls from any earlier operation are replaced by controls 
     * received on the latest operation on this connection by this thread).
     * @param current The target thread.
     * @param con The server response controls.
     */
    void setResponseControls( LDAPConnThread current, ResponseControl con ) {
        synchronized(m_responseControlTable) {
            Vector v = (Vector)m_responseControlTable.get(current);

            // if the current thread already contains response controls from
            // a previous operation
            if ((v != null) && (v.size() > 0)) {

                // look at each response control
                for (int i=v.size()-1; i>=0; i--) {
                    ResponseControl response = 
                      (ResponseControl)v.elementAt(i);
    
                    // if this response control belongs to this connection
                    if (response.getConnection().equals(this)) {
 
                        // if the given control is null or 
                        // the given control is not null and the current 
                        // control does not correspond to the new JDAPMessage
                        if ((con == null) || 
                          (con.getMsgID() != response.getMsgID()))
                            v.removeElement(response);

                        // For the same connection, if the message id from the 
                        // given control is the same as the one in the queue,
                        // those controls in the queue will not get removed
                        // since they come from the persistent search control
                        // which allows more than one control response for
                        // one persistent search operation.
                    }
                }
            } else {
                if (con != null)
                    v = new Vector();
            }          

            if (con != null) {
                v.addElement(con);
                m_responseControlTable.put(current, v);
            }

            /* Do some garbage collection: check if any attached threads have
               exited */
            /* Now check all threads in the list */
            Enumeration e = m_attachedList.elements();
            while( e.hasMoreElements() ) {
                LDAPConnThread aThread = (LDAPConnThread)e.nextElement();
                if ( !aThread.isAlive() ) {
                    m_responseControlTable.remove( aThread );
                    m_attachedList.removeElement( aThread );
                }
            }
        }
        /* Make sure we're registered */
        if ( m_attachedList.indexOf( current ) < 0 )
            m_attachedList.addElement( current );
    }

    /**
     * Set up connection for referral.
     * @param u referral URL
     * @param cons search constraints
     * @return new LDAPConnection, already connected and authenticated
     */
    private LDAPConnection prepareReferral( LDAPUrl u,
                                            LDAPSearchConstraints cons )
        throws LDAPException {
        LDAPConnection connection = new LDAPConnection (this.getSocketFactory());
        connection.setOption(REFERRALS, new Boolean(true));
        connection.setOption(REFERRALS_REBIND_PROC,
                              cons.getRebindProc());

        // need to set the protocol version which gets passed to connection
        connection.setOption(PROTOCOL_VERSION,
                              new Integer(protocolVersion));

        connection.setOption(REFERRALS_HOP_LIMIT,
                              new Integer(cons.getHopLimit()-1));
        connection.connect (u.getHost(), u.getPort());
        return connection;
    }

    /**
     * Establish the LDAPConnection to the referred server. This one is used
     * for bind operation only since we need to keep this new connection for
     * the subsequent operations. This new connection will be destroyed in
     * two circumstances: disconnect gets called or the client binds as
     * someone else.
     * @return The new LDAPConnection to the referred server
     */
    LDAPConnection createReferralConnection(LDAPReferralException e,
      LDAPSearchConstraints cons) throws LDAPException {
        if (cons.getHopLimit() <= 0)
            throw new LDAPException("exceed hop limit",
              e.getLDAPResultCode(), e.getLDAPErrorMessage());
        if (!cons.getReferrals()) {
            throw e;
        }

        LDAPUrl u[] = e.getURLs();
        // If there are no referrals (because the server isn't set up for
        // them), give up here
        if ((u == null) || (u.length <= 0) || (u[0].equals("")))
            throw new LDAPException("No target URL in referral",
              LDAPException.NO_RESULTS_RETURNED);

        LDAPConnection connection = null;
        connection = prepareReferral(u[0], cons);
        String DN = u[0].getDN();
        if ((DN == null) || (DN.equals("")))
            DN = boundDN;
        connection.authenticate(protocolVersion, DN, boundPasswd);
        return connection;
    }

    /**
     * Performs the referral.
     * @param e referral exception
     * @param cons search constraints
     */
    void performReferrals(LDAPReferralException e,
        LDAPSearchConstraints cons, int ops,
        /* unions of different operation parameters */
        String dn, int scope, String filter, String types[],
        boolean attrsOnly, LDAPModification mods[], LDAPEntry entry,
        LDAPAttribute attr,
        /* result */
        Vector results
        ) throws LDAPException {

        if (cons.getHopLimit() <= 0)
            throw new LDAPException("exceed hop limit",
              e.getLDAPResultCode(), e.getLDAPErrorMessage());
        if (!cons.getReferrals()) {
            if (ops == JDAPProtocolOp.SEARCH_REQUEST) {
                LDAPSearchResults res = new LDAPSearchResults();
                res.add(e);
                results.addElement(res);
                return;
            } else
                throw e;
        }

        LDAPUrl u[] = e.getURLs();
        // If there are no referrals (because the server isn't set up for
        // them), give up here
        if ( u == null )
            return;

        for (int i = 0; i < u.length; i++) {
            String newDN = u[i].getDN();
            String DN = null;
            if ((newDN == null) || (newDN.equals("")))
                DN = dn;
            else
                DN = newDN;
            // If this was a one-level search, and a direct subordinate
            // has a referral, there will be a "?base" in the URL, and
            // we have to rewrite the scope from one to base
            if ( u[i].getUrl().indexOf("?base") > -1 ) {
                scope = SCOPE_BASE;
            }

            LDAPSearchResults res = null;
            LDAPSearchConstraints newcons = (LDAPSearchConstraints)cons.clone();
            newcons.setHopLimit( cons.getHopLimit()-1 );

            try {
                if ((m_referralConnection != null) && 
                  (u[i].getHost().equals(m_referralConnection.host)) &&
                  (u[i].getPort() == m_referralConnection.port)) {
                    performReferrals(m_referralConnection, newcons, ops, DN, 
                      scope, filter, types, attrsOnly, mods, entry, attr, 
                      results);
                    continue;
                }
            } catch (LDAPException le) {
                if (le.getLDAPResultCode() != 
                  LDAPException.INSUFFICIENT_ACCESS_RIGHTS) {
                    throw le;
                }
            }

            LDAPConnection connection = null;
            connection = prepareReferral( u[i], cons );
            if (cons.getRebindProc() == null) {
                connection.authenticate (null, null);
            } else {
                LDAPRebindAuth auth =
                  cons.getRebindProc().getRebindAuthentication(u[i].getHost(),
                  u[i].getPort());
                connection.authenticate(auth.getDN(), auth.getPassword());
            }
            performReferrals(connection, newcons, ops, DN, scope, filter,
              types, attrsOnly, mods, entry, attr, results);
        }
    }

    void performReferrals(LDAPConnection connection, 
      LDAPSearchConstraints cons, int ops, String dn, int scope,
      String filter, String types[], boolean attrsOnly,
      LDAPModification mods[], LDAPEntry entry, LDAPAttribute attr,
      Vector results) throws LDAPException {
 
        LDAPSearchResults res = null;
        try {
            switch (ops) {
                case JDAPProtocolOp.SEARCH_REQUEST:

                    res = connection.search(dn, scope, filter,
                                            types, attrsOnly, cons);
                    if (res != null) {
                        res.closeOnCompletion(connection);
                        results.addElement(res);
                    } else {
                        if ((m_referralConnection == null) ||
                          (!connection.equals(m_referralConnection)))
                            connection.disconnect();
                    }
                    break;
                case JDAPProtocolOp.MODIFY_REQUEST:
                    connection.modify(dn, mods, cons);
                    break;
                case JDAPProtocolOp.ADD_REQUEST:
                    if ((dn != null) && (!dn.equals("")))
                         entry.setDN(dn);
                    connection.add(entry, cons);
                    break;
                case JDAPProtocolOp.DEL_REQUEST:
                    connection.delete(dn, cons);
                    break;
                case JDAPProtocolOp.MODIFY_RDN_REQUEST:
                    connection.rename(dn, filter /* newRDN */, 
                      attrsOnly /* deleteOld */, cons);
                    break;
                case JDAPProtocolOp.COMPARE_REQUEST:
                    boolean bool = connection.compare(dn, attr, cons);
                    results.addElement(new Boolean(bool));
                    break;
                default:
                    /* impossible */
                    break;
            }
        } catch (LDAPException ee) {
            throw ee;
        } finally {
            if ((connection != null) && 
              ((ops != JDAPProtocolOp.SEARCH_REQUEST) || (res == null)) &&
                ((m_referralConnection == null) || 
                 (!connection.equals(m_referralConnection))))
                connection.disconnect();
        }
    }

    /**
     * Performs the referral.
     * @param e referral exception
     * @param cons search constraints
     */
    private LDAPExtendedOperation performExtendedReferrals(
        LDAPReferralException e,
        LDAPSearchConstraints cons, LDAPExtendedOperation op )
        throws LDAPException {

        if (cons.getHopLimit() <= 0)
            throw new LDAPException("exceed hop limit",
              e.getLDAPResultCode(), e.getLDAPErrorMessage());
        if (!cons.getReferrals()) {
            throw e;
        }

        LDAPUrl u[] = e.getURLs();
        // If there are no referrals (because the server isn't set up for
        // them), give up here
        if ( u == null )
            return null;

        for (int i = 0; i < u.length; i++) {
            try {
                LDAPConnection connection = prepareReferral( u[i], cons );
                LDAPExtendedOperation results =
                    connection.extendedOperation( op );
                connection.disconnect();
                return results; /* return right away if operation is successful */
            } catch (LDAPException ee) {
                continue;
            }
        }
        return null;
    }

    /**
     * Creates and returns an new <CODE>LDAPConnection</CODE> object that
     * contains the same information as the current connection, including:
     * <UL>
     * <LI>the default search constraints
     * <LI>host name and port number of the LAAP server
     * <LI>the DN and password used to authenticate to the LDAP server
     * </UL>
     * <P>
     * @return The <CODE>LDAPconnection</CODE> object representing the
     * new object.
     */
    public synchronized Object clone() {
        try {
            LDAPConnection c = (LDAPConnection)super.clone();

            if (this.th == null)
                this.bind(defaultConstraints);

            c.defaultConstraints =
                (LDAPSearchConstraints)defaultConstraints.clone();
            c.responseListeners = null;
            c.searchListeners = null;
            c.bound = this.bound;
            c.host = this.host;
            c.port = this.port;
            c.boundDN = this.boundDN;
            c.boundPasswd = this.boundPasswd;
            c.prevBoundDN = this.prevBoundDN;
            c.prevBoundPasswd = this.prevBoundPasswd;
            c.m_anonymousBound = this.m_anonymousBound;
            c.m_factory = this.m_factory;
            c.th = this.th; /* share current connection thread */

            synchronized (m_threadConnTable) {
                Vector v = (Vector)m_threadConnTable.get(this.th);
                if (v != null) {
                    v.addElement(c);
                }
                else {
                    printDebug("Failed to clone");
                    return null;
                }
            }

            c.th.register(c);
            return c;
        } catch (Exception e) {
        }
        return null;
    }

    /**
     * This is called when a search result has been retrieved from the incoming
     * queue. We use the notification to unblock the listener thread, if it
     * is waiting for the backlog to lighten.
     */
    void resultRetrieved() {
        if ( th != null ) {
            th.resultRetrieved();
        }
    }

    /**
     * Sets up basic connection privileges for Communicator.
     * @return true if in Communicator and connections okay.
     */
    private static boolean checkCommunicator() {
        try {
            java.lang.reflect.Method m = LDAPCheckComm.getMethod(
              "netscape.security.PrivilegeManager", "enablePrivilege");
            if (m == null) {
                printDebug("Method is null");
                return false;
            }

            Object[] args = new Object[1];
            args[0] = new String("UniversalConnect");
            m.invoke( null, args);
            printDebug( "UniversalConnect enabled" );
            return true;
        } catch (LDAPException e) {
            printDebug("Exception: "+e.toString());
        } catch (Exception ie) {
            printDebug("Exception on invoking " + "enablePrivilege: "+ie.toString());
        }
        return false;
    }

    /**
     * Reports if the class is running in a Netscape browser.
     * @return <CODE>true</TRUE> if the class is running in a Netscape browser.
     */
    public static boolean isNetscape() {
        return isCommunicator;
    }

    static void printDebug( String msg ) {
        if ( debug ) {
            System.out.println( msg );
        }
    }

    /**
     * Prints out the LDAP Java SDK version and the highest LDAP
     * protocol version supported by the SDK. To view this
     * information, open a terminal window, and enter:
     * <PRE>java netscape.ldap.LDAPConnection
     * </PRE>
     * @param args Not currently used.
     */
    public static void main(String[] args) {
        System.out.println("LDAP SDK Version is "+SdkVersion);
        System.out.println("LDAP Protocol Version is "+ProtocolVersion);
    }

    /**
     * Option specifying the maximum number of unread entries to be cached in any
     * LDAPSearchResults without suspending reading from the server.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int MAXBACKLOG   = 30;

    private static boolean isCommunicator = checkCommunicator();
    private static final boolean debug = false;
}

/*
 * This object represents the value of the m_responseControlTable hashtable.
 * It stores the response controls and its corresponding LDAPConnection and
 * the message ID for its corresponding JDAPMessage.
 */
class ResponseControl {
    private LDAPConnection m_connection;
    private int m_messageID;
    private LDAPControl[] m_controls;

    public ResponseControl(LDAPConnection conn, int msgID, 
      LDAPControl[] controls) {

        m_connection = conn;
        m_messageID = msgID;
        m_controls = new LDAPControl[controls.length];
        for (int i=0; i<controls.length; i++) 
            m_controls[i] = controls[i];
    }

    public int getMsgID() {
        return m_messageID;
    }

    public LDAPControl[] getControls() {
        return m_controls;
    }

    public LDAPConnection getConnection() {
        return m_connection;
    }
}

/**
 * Multiple LDAPConnection clones can share a single physical connection,
 * which is maintained by a thread.
 *
 * New Design (09/05/97):
 *
 *   +----------------+
 *   | LDAPConnection | --------+
 *   +----------------+         |
 *                              |
 *   +----------------+         |        +----------------+
 *   | LDAPConnection | --------+------- | LDAPConnThread |
 *   +----------------+         |        +----------------+
 *                              |
 *   +----------------+         |
 *   | LDAPConnection | --------+
 *   +----------------+
 *
 * All LDAPConnections send requests and get responses from
 * LDAPConnThread (a thread).
 */
class LDAPConnThread extends Thread {

    /**
     * Constants
     */
    private final static int MAXMSGID = Integer.MAX_VALUE;

    /**
     * Internal variables
     */
    transient private int m_highMsgId;
    transient private InputStream m_serverInput;
    transient private OutputStream m_serverOutput;
    transient private Hashtable m_requests;
    transient private Hashtable m_messages = null;
    transient private Vector m_registered;
    transient private boolean m_disconnected = false;
    transient private LDAPCache m_cache = null;
    transient private boolean m_failed = false;
    private Object m_securityLayer = null;
    private Socket m_socket = null;
    private int m_maxBacklog = 100;

    /**
     * Constructs a connection thread that maintains connection to the
     * LDAP server.
     * @param host LDAP host name
     * @param port LDAP port number
     * @param factory LDAP socket factory
     */
    public LDAPConnThread(String host, int port, LDAPSocketFactory factory,
      LDAPCache cache) throws LDAPException {
        m_highMsgId = 0;
        m_requests = new Hashtable ();
        m_registered = new Vector ();
        m_cache = cache;
        if (m_cache != null)
            m_messages = new Hashtable ();

        /* Connect to LDAP server */
        try {
            /* If we are to create a socket ourselves, make sure it has
               sufficient privileges to connect to the desired host */
            if (factory == null) {
                m_socket = new Socket (host, port);
//                m_socket.setSoLinger( false, -1 );
            } else {
                m_socket = factory.makeSocket(host, port);
            }
            m_serverInput = new BufferedInputStream (m_socket.getInputStream());
            m_serverOutput = new BufferedOutputStream (m_socket.getOutputStream());
        } catch (IOException e) {

            // a kludge to make the thread go away. Since the thread has already
            // been created, the only way to clean up the thread is to call the
            // start() method. Otherwise, the exit method will be never called
            // because the start() was never called. In the run method, the stop
            // method calls right away if the m_failed is set to true.
            m_failed = true;
            start();
            throw new LDAPException ( "failed to connect to server " + host,
                                      LDAPException.CONNECT_ERROR );
        }
        start(); /* start the thread */
    }

    /**
     * Sends LDAP request via this connection thread.
     * @param request request to send
     * @param toNotify response listener to invoke when the response
     *          is ready
     */
    synchronized void sendRequest (JDAPProtocolOp request,
        LDAPResponseListener toNotify, LDAPSearchConstraints cons)
         throws LDAPException {
        if (m_serverOutput == null)
            throw new LDAPException ( "not connected to a server",
                                  LDAPException.OTHER );

        JDAPMessage msg;
        LDAPControl lc[] = cons.getServerControls();
        if ( (lc != null) && (lc.length > 0) ) {
            JDAPControl[] jc = new JDAPControl[lc.length];
            for( int i = 0; i < lc.length; i++ )
                jc[i] = new JDAPControl( lc[i].getID(),
                                     lc[i].isCritical(),
                                     lc[i].getValue() );
            msg = new JDAPMessage(allocateId(), request, jc);
        }
        else
            msg = new JDAPMessage(allocateId(), request);
        if ( toNotify != null ) {
            if (!(request instanceof JDAPAbandonRequest ||
                  request instanceof JDAPUnbindRequest)) {
                /* Only worry about toNotify if we expect a response... */
                this.m_requests.put (new Integer (msg.getId()), toNotify);
                /* Notify the backlog checker that there may be another outstanding
                   request */
                resultRetrieved(); 
            }
            toNotify.setID( msg.getId() );
        }

        try {
            msg.write (m_serverOutput);
            m_serverOutput.flush ();
        } catch (IOException e) {
            networkError(e);
        }
    }

    /**
     * Register with this connection thread.
     * @param conn LDAP connection
     */
    public synchronized void register(LDAPConnection conn) {
        if (!m_registered.contains(conn))
            m_registered.addElement(conn);
    }

    void setSecurityLayer(Object layer) {
        m_securityLayer = layer;
    }

    synchronized int getClientCount() {
        return m_registered.size();
    }

    /**
     * De-Register with this connection thread. If all the connection
     * is deregistered. Then, this thread should be killed.
     * @param conn LDAP connection
     */
    public synchronized void deregister(LDAPConnection conn) {
        m_registered.removeElement(conn);
        if (m_registered.size() == 0) {
            try {
                LDAPSearchConstraints cons = conn.getSearchConstraints();
                sendRequest (new JDAPUnbindRequest (), null, cons);
                cleanUp();
                this.stop();
                this.sleep(100); /* give enough time for threads to shutdown */
            } catch (Exception e) {
                LDAPConnection.printDebug(e.toString());
            }
        }
    }

    /**
     * Clean ups before shutdown the thread.
     */
    private void cleanUp() {
        if (!m_disconnected) {
            try {
                m_serverOutput.close ();
            } catch (Exception e) {
            } finally {
                m_serverOutput = null;
            }

            try {
                m_serverInput.close ();
            } catch (Exception e) {
            } finally {
                m_serverInput = null;
            }

            try {
                m_socket.close ();
            } catch (Exception e) {
            } finally {
                m_socket = null;
            }
        
            m_disconnected = true;

            /**
             * Notify all the registered about this bad moment.
             * IMPORTANT: This needs to be done at last. Otherwise, the socket
             * input stream and output stream might never get closed and the whole
             * task will get stuck in the stop method when we tried to stop the
             * LDAPConnThread.
             */

            if (m_registered != null) {
                Vector registerCopy = (Vector)m_registered.clone();

                Enumeration cancelled = registerCopy.elements();

                while (cancelled.hasMoreElements ()) {
                    LDAPConnection c = (LDAPConnection)cancelled.nextElement();
                    c.deregisterConnection();
                }
            }
            m_registered = null;
            m_messages = null;
            m_requests.clear();
        }
    }

    /**
     * Set and get the maximum number of unread entries any search listener can
     * have before we stop reading from the server.
     */
    void setMaxBacklog( int backlog ) {
        m_maxBacklog = backlog;
    }

    int getMaxBacklog() {
        return m_maxBacklog;
    }

    /**
     * Sleep if there is a backlog of search results
     */
    private void checkBacklog() {
        boolean doWait;
        do {
            doWait = false;
            Enumeration listeners = m_requests.elements();
            while( listeners.hasMoreElements() ) {
                LDAPResponseListener l =
                    (LDAPResponseListener)listeners.nextElement();
                // If there are any threads waiting for a regular response
                // message, we have to go read the next incoming message
                if ( !(l instanceof LDAPSearchListener) ) {
                    doWait = false;
                    break;
                }
                // If the backlog of any search thread is too great,
                // wait for it to diminish, but if this is a synchronous
                // search, then just keep reading
                LDAPSearchListener sl = (LDAPSearchListener)l;
                if ( (sl.getConstraints().getBatchSize() != 0) &&
                     (sl.getCount() >= m_maxBacklog) ) {
                    doWait = true;
                }
            }
            if ( doWait ) {
                synchronized(this) {
                    try {
                        wait();
                    } catch (InterruptedException e ) {
                    }
                }
            }
        } while ( doWait );
    }

    /**
     * This is called when a search result has been retrieved from the incoming
     * queue. We use the notification to unblock the listener thread, if it
     * is waiting for the backlog to lighten.
     */
    synchronized void resultRetrieved() {
        notifyAll();
    }

    /**
     * Reads from the LDAP server input stream for incoming LDAP messages.
     */
    public void run() {
        // if there is a problem of establishing connection to the server,
        // stop the thread right away...
        if (m_failed)
            this.stop();

        JDAPMessage msg = null;
        JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();

        while (true) {
            yield();
            int[] nread = new int[1];
            nread[0] = 0;
            // Avoid too great a backlog of results
            checkBacklog();
            try  {
                BERElement element = BERElement.getElement(decoder, m_serverInput,
                                                           nread);
                msg = new JDAPMessage(element);

                // passed in the ber element size to approximate the size of the cache
                // entry, thereby avoiding serialization of the entry stored in the
                // cache
                processResponse (msg, nread[0]);
            } catch (Exception e)  {
                networkError(e);
            }

            if (m_disconnected)
                break;
        }
    }

    /**
     * Allocates a new LDAP message ID.  These are arbitrary numbers used to
     * correlate client requests with server responses.
     * @return new unique msgId
     */
    private synchronized int allocateId () {
        m_highMsgId = (m_highMsgId + 1) % MAXMSGID;
        return m_highMsgId;
    }

    /**
     * When a response arrives from the LDAP server, it is processed by
     * this routine.  It will pass the message on to the listening object
     * associated with the LDAP msgId.
     * @param incoming New message from LDAP server
     */
    private synchronized void processResponse (JDAPMessage incoming, int size) {
        Integer messageID = new Integer (incoming.getId());
        LDAPResponseListener l = (LDAPResponseListener)m_requests.get (messageID);

        if (l == null) return; /* nobody is waiting for this response (!) */


        /* Were there any controls for this client? */
        LDAPControl[] con = checkControls( incoming );
        ResponseControl responseControl = null;
        if (con != null)
            responseControl = new ResponseControl(l.getConnection(),
              incoming.getId(), con);
        l.getConnection().setResponseControls( this, responseControl );

        JDAPProtocolOp op = incoming.getProtocolOp ();

        Vector v = null;


        if ((op instanceof JDAPSearchResponse) ||
           (op instanceof JDAPSearchResultReference)) {

            ((LDAPSearchListener)l).addSearchResult (incoming);
            Long key = ((LDAPSearchListener)l).getKey();

            if ((m_cache != null) && (key != null)) {
                // get the vector containing the JDAPmessages for the specified messageID
                v = (Vector)m_messages.get(messageID);

                if (v == null)
                {
                    v = new Vector();
                    // keeps track of the total size of all JDAPMessages belonging to the
                    // same messageID, now the size is 0
                    v.addElement(new Long(0));
                }

                // add the size of the current JDAPmessage to the lump sum
                // assume the size of JDAPMessage is more or less the same as the size
                // of LDAPEntry. Eventually LDAPEntry object gets stored in the cache
                // instead of JDAPMessage object.
                long entrySize = ((Long)v.firstElement()).longValue() + size;

                // update the lump sum located in the first element of the vector
                v.setElementAt(new Long(entrySize), 0);

                // convert JDAPMessage object into LDAPEntry which is stored to the
                // end of the Vector
                v.addElement(constructLDAPEntry(incoming));

                // replace the entry
                m_messages.put(messageID, v);
            }
        } else {
            l.setResponse (incoming);
            if (l instanceof LDAPSearchListener) {
                Long key = ((LDAPSearchListener)l).getKey();

                if (key != null) {
                    boolean fail = false;
                    JDAPProtocolOp protocolOp = incoming.getProtocolOp();
                    if (protocolOp instanceof JDAPSearchResult) {
                        JDAPResult res = (JDAPResult)protocolOp;
                        if (res.getResultCode() > 0)
                            fail = true;
                    }

                    if ((!fail) && (m_cache != null))  {

                        // that is. Collects all the JDAPMessages for the specified messageID
                        // no need to keep track of this entry. Remove it.
                        v = (Vector)m_messages.remove(messageID);

                        // If v is null, meaning there are no search results from the
                        // server
                        if (v == null) {
                            v = new Vector();

                            // set the entry size to be 0
                            v.addElement(new Long(0));
                        }

                        try {
                            // add the new entry with key and value (a vector of
                            // LDAPEntry objects)
                            m_cache.addEntry(key, v);
                        } catch (LDAPException e) {
                            System.out.println("Exception: "+e.toString());
                        }
                    }
                }
            }
            m_requests.remove (messageID);
        }
    }

    /**
     * Construct the LDAPEntry from the JDAPMessage object.
     * @param msg The JDAPMessage object
     * @return The LDAPEntry object required for the cache entry.
     */
    private LDAPEntry constructLDAPEntry(JDAPMessage msg) {
        JDAPProtocolOp op = msg.getProtocolOp();
        JDAPSearchResponse sr = (JDAPSearchResponse)op;
        LDAPAttribute[] lattrs = sr.getAttributes();
        LDAPAttributeSet attrs;
        if ( lattrs != null )
            attrs = new LDAPAttributeSet( lattrs );
        else
            attrs = new LDAPAttributeSet();

        String dn = sr.getObjectName();
        return(new LDAPEntry( dn, attrs ));
    }

    /**
     * Extract controls from message and stash them away for retrieval
     * by client.
     * @param response The results of an operation, possibly containing
     * Controls.
     */
    private LDAPControl[] checkControls( JDAPMessage response ) {
        LDAPControl[] con = null;
        if ( response != null ) {
            JDAPControl[] jc = response.getControls();
            if ( (jc != null) && (jc.length > 0) ) {
                con = new LDAPControl[jc.length];
                for( int i = 0; i < jc.length; i++ )
                    con[i] = new LDAPControl( jc[i].getID(),
                                          jc[i].isCritical(),
                                          jc[i].getValue() );
            }
        }
        return con;
    }

    /**
     * Stop dispatching responses for a particular message ID.
     * @param id Message ID for which to discard responses.
     */
    void abandon (int id ) {
        m_requests.remove( new Integer(id) );
    }

    /**
     * Handles network errors.  Basically shuts down the whole connection.
     * @param e The exception which was caught while trying to read from
     * input stream.
     */
    private synchronized void networkError (Exception e) {
        try {

            // notify each listener that the server is down.
            Enumeration requests = m_requests.elements();
            while (requests.hasMoreElements()) {
                LDAPResponseListener listener =
                    (LDAPResponseListener)requests.nextElement();
                listener.setException(new LDAPException("Server down", LDAPException.OTHER));
            }
            cleanUp();

        } catch (NullPointerException ee) {
          System.err.println("Exception: "+ee.toString());
        }

        /**
         * Notify all the registered about this bad moment.
         * IMPORTANT: This needs to be done at last. Otherwise, the socket
         * input stream and output stream might never get closed and the whole
         * task will get stuck in the stop method when we tried to stop the
         * LDAPConnThread.
         */

        if (m_registered != null) {
            Vector registerCopy = (Vector)m_registered.clone();

            Enumeration cancelled = registerCopy.elements();

            while (cancelled.hasMoreElements ()) {
              LDAPConnection c = (LDAPConnection)cancelled.nextElement();
              c.deregisterConnection();
            }
        }
    }
}

