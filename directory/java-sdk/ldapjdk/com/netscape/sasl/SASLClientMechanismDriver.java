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
package com.netscape.sasl;

/**
 *
 *<PRE>
 *                                               Mechanism Drivers
 *  ---------------     -------------------      -----------------
 *  | Application |-----| Protocol Driver |------| MD5           |
 *  ---------------     -------------------   |  -----------------
 *                                            |
 *                                            |  -----------------
 *                                            |--| Kerberos v5   |
 *                                            |  -----------------
 *                                            |
 *                                            |  -----------------
 *                                            |--| PKCS-11       |
 *                                            |  -----------------
 *                                            |
 *
 *                                            |
 *
 *                                            |
 *                                            |  - - - - - - - - -
 *                                            |--| xxxYYYxxx     |
 *                                               - - - - - - - - -
 *</PRE>
 * An application chooses a Protocol Driver specific to the
 * protocol it wants to use, and specifies one or more acceptable
 * mechanisms. The Protocol Driver controls the socket, and knows
 * the format/packaging of bytes sent down and received from the
 * socket, but does not know how to authenticate or to encrypt/
 * decrypt the bytes. It uses one of of the Mechanism Drivers
 * to help it perform authentication, where all parameters to
 * be used in encryption from then on are determined. In a protocol-
 * specific way, the Protocol Driver examines each byte string received
 * from the server to determine if the authentication process has
 * been completed. If not, the byte string is passed to the Mechanism
 * Driver to be interpreted as a server challenge; the Mechanism
 * Driver returns an appropriate response, which the Protocol Driver
 * can encode in a protocol-specific way and return to the server.
 *<P>
 * If the Protocol Driver concludes from the byte string received from
 * the server that authentication is complete, it may query
 * the Mechanism Driver if it considers the authentication process
 * complete, in order to thwart early completion messages inserted by
 * and intruder.
 *<P>
 * On completed
 * authentication, the Protocol Driver receives from the Mechanism
 * Driver a Security Layer Driver object. From this point on,
 * the Protocol Driver passes byte arrays received from its socket
 * to the Security Layer Driver object for decoding before
 * returning them to the application, and passes
 * application byte arrays to the Security Layer Driver object
 * for encryption before passing them down the socket.
 *<P>
 * A complication here is that some authentication methods may
 * require additional user/application input (at least on the client
 * side). That means that a Mechanism Driver may need to call up to
 * an application during the authentication process. In the following,
 * an interface SASLAuthenticationCB has been defined, allowing
 * an application to (if necessary) provide a user with prompts and
 * obtain additional information required to continue the process.
 *<P>
 * For LDAP, the Protocol Driver can be considered built in to
 * the LDAPConnection class (actually it is more likely an object
 * to which an LDAPConnection object has a reference).
 *<P>
 * However,
 * there should be a generalized framework for registering and
 * finding Mechanism Drivers. Maybe best to do something like
 * content and protocol handlers in java: look for them in some
 * predefined place in the general class hierarchy, e.g.
 * netscape.security.mechanisms. So if a Protocol Driver is
 * asked to use "GSSAPI", it would attempt to instantiate
 * netscape.security.mechanisms.gssapi. A non-standard place can
 * also be specified, e.g. "myclasses.mechanisms.GSSAPI".
 * This functionality should be folded into a mechanism driver
 * factory, which knows where to find candidate classes for
 * instantiation.
 *<P>
 * The Mechanism Drivers are protocol-independent, and don't deal
 * directly with network connections, just byte arrays, so they
 * should be implemented in a generalizable way for all protocols.
 *<P>
 * A Security Layer Driver typically inherits a State object from
 * the Mechanism Driver, where parameters and resolutions reached
 * during authentication have been stored.
 *<P>
 * One way to allow specifying an open-ended list of parameters is
 * with a Properties object. That is what is done in the following.
 *
 * @author rweltman@netscape.com
 * @version 1.0
 */

public interface SASLClientMechanismDriver {

    /**
     * This method prepares a byte array to use for the initial
     * request to authenticate. A SASLException is thrown if the driver
     * cannot initiate authentication with the supplied parameters.
     * @param id Protocol-dependent identification, e.g. user name or
     * distinguished name.
     * @param protocol A protocol supported by the mechanism driver, e.g.
     * "pop3", "ldap"
     * @param serverName Only used in kerberos, currently: fully qualified
     * name of server to authenticate to.
     * @param props Additional configuration for the session, e.g.
     *<PRE>
     *    "security.policy.encryption.minimum"    Minimum key length;
     *                                            default 0
     *    "security.policy.encryption.maximum"    Maximum key length;
     *                                            default 256
     *    "security.policy.server_authentication" True if server must
     *                                            authenticate to client;
     *                                            default <CODE>false</CODE>
     *    "security.ip.local"                     For kerberos v4; no default
     *    "security.ip.remote"                    For kerberos v4; no default
     *    "security.maxbuffer"                    Reject frames larger than
     *                                            this; default 0 (client
     *                                            will not use the security
     *                                            layer)
     *</PRE>
     * @param authCB An optional object which can be invoked by the
     * mechanism driver to acquire additional authentication information,
     * such as user name and password.
     * @return A byte array to be used for the initial authentication. It
     * may be <CODE>null</CODE> for a standard initial sequence in some
     * protocols, such as POP, SMTP, and IMAP.
     * @exception SASLException if an initial authentication request can
     * not be formulated with the supplied parameters.
     */
    public byte[] startAuthentication( String id,
                                       String protocol,
                                       String serverName,
                                       java.util.Properties props,
                                       SASLClientCB authCB )
                                       throws SASLException;

    /**
     * If a challenge is received from the server during the
     * authentication process, this method is called by the
     * Protocol Driver to prepare an appropriate next request to submit
     * to the server.
     * @param challenge Received server challenge.
     * @return Request to submit to server.
     * @exception SASLException if the server challenge could not
     * be handled or the driver is unable for other reasons to
     * continue the authentication process.
     */
    public byte[] evaluateResponse( byte[] challenge )
                                   throws SASLException;

    /**
     * The following method may be called at any time to determine if
     * the authentication process is finished. Typically, the protocol
     * driver will not do this until it has received something
     * from the server which indicates (in a protocol-specific manner)
     * that the process
     * has completed.
     * @return <CODE>true</CODE> if authentication is complete.
     */
    public boolean isComplete();

    /**
     * Once authentication is complete, the Protocol Driver calls the
     * following method to obtain an object capable of encoding/decoding
     * data content for the rest of the session (or until there is a
     * new round of authentication). An exception is thrown if
     * authentication is not yet complete.
     * @return A SASLSecurityLayer object capable of doing
     * encoding/decoding during the session.
     * @exception SASLException if no security layer has been negotiated
     * or if authentication is not complete.
     */
    public SASLSecurityLayer getSecurityLayer() throws SASLException;

    /**
     * Report the name of this driver, e.g. "GSSAPI".
     * @return The name of the mechanism driver, without any location
     * qualification.
     */
    public String getMechanismName();
}

