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
import org.ietf.ldap.client.*;
import org.ietf.ldap.client.opers.*;
import java.io.*;

/**
 * Indicates that an error has occurred.  An <CODE>LDAPException</CODE>
 * can result from physical problems (such as network errors) as well as
 * problems with LDAP operations (for example, if the LDAP add operation
 * fails because of duplicate entry).
 * <P>
 *
 * Most errors that occur throw this type of exception.  In order to determine
 * the cause of the error, you can call the <CODE>getResultCode()</CODE>
 * method to get the specific result code and compare this code against
 * the result codes defined as fields in this class. (For example, if
 * the result code matches the value of the field
 * <CODE>LDAPException.TIME_LIMIT_EXCEEDED</CODE>, the time limit passed
 * before the search operation could be completed.)
 * <P>
 *
 * This exception includes methods for getting an error message that
 * corresponds to the LDAP result code (for example, "Timelimit exceeded"
 * for <CODE>LDAPException.TIME_LIMIT_EXCEEDED</CODE>).  These error
 * messages are specified in the following files:
 * <PRE>netscape/ldap/errors/ErrorCodes_<I>locale_string</I>.props</PRE>
 * where <I>locale_string</I> is the name of the locale that includes
 * the language and country, but not the variant.
 * <P>
 *
 * For example:
 * <PRE>netscape/ldap/errors/ErrorCodes_en_US.props</PRE>
 *
 * The LDAP Java classes get this locale name by calling the
 * <CODE>java.util.Locale.toString</CODE> method for the specified
 * locale and ignoring the variant.  If no locale is specified, the
 * LDAP Java classes use the <CODE>java.util.Locale.getDefault</CODE>
 * method to get the locale of the local host system.
 * <P>
 *
 * In order to get error messages for different locales, you need to
 * provide files containing the error messages for those locales.
 * The files should be located in the <CODE>netscape/ldap/errors</CODE>
 * directory and should use the naming convention specified above.
 * <P>
 *
 * The following is a list of LDAP result codes:
 * <PRE>
 * Result
 *  Code   Defined Value
 * ======  =============
 *   0     <A HREF="#SUCCESS">SUCCESS</A>
 *   1     <A HREF="#OPERATION_ERROR">OPERATION_ERROR</A>
 *   2     <A HREF="#PROTOCOL_ERROR">PROTOCOL_ERROR</A>
 *   3     <A HREF="#TIME_LIMIT_EXCEEDED">TIME_LIMIT_EXCEEDED</A>
 *   4     <A HREF="#SIZE_LIMIT_EXCEEDED">SIZE_LIMIT_EXCEEDED</A>
 *   5     <A HREF="#COMPARE_FALSE">COMPARE_FALSE</A>
 *   6     <A HREF="#COMPARE_TRUE">COMPARE_TRUE</A>
 *   7     <A HREF="#AUTH_METHOD_NOT_SUPPORTED">AUTH_METHOD_NOT_SUPPORTED</A>
 *   8     <A HREF="#STRONG_AUTH_REQUIRED">STRONG_AUTH_REQUIRED</A>
 *   9     <A HREF="#LDAP_PARTIAL_RESULTS">LDAP_PARTIAL_RESULTS</A>
 *  10     <A HREF="#REFERRAL">REFERRAL</A> (LDAP v3)
 *  11     <A HREF="#ADMIN_LIMIT_EXCEEDED">ADMIN_LIMIT_EXCEEDED</A> (LDAP v3)
 *  12     <A HREF="#UNAVAILABLE_CRITICAL_EXTENSION">UNAVAILABLE_CRITICAL_EXTENSION</A> (LDAP v3)
 *  13     <A HREF="#CONFIDENTIALITY_REQUIRED">CONFIDENTIALITY_REQUIRED</A> (LDAP v3)
 *  14     <A HREF="#SASL_BIND_IN_PROGRESS">SASL_BIND_IN_PROGRESS</A> (LDAP v3)
 *  16     <A HREF="#NO_SUCH_ATTRIBUTE">NO_SUCH_ATTRIBUTE</A>
 *  17     <A HREF="#UNDEFINED_ATTRIBUTE_TYPE">UNDEFINED_ATTRIBUTE_TYPE</A>
 *  18     <A HREF="#INAPPROPRIATE_MATCHING">INAPPROPRIATE_MATCHING</A>
 *  19     <A HREF="#CONSTRAINT_VIOLATION">CONSTRAINT_VIOLATION</A>
 *  20     <A HREF="#ATTRIBUTE_OR_VALUE_EXISTS">ATTRIBUTE_OR_VALUE_EXISTS</A>
 *  21     <A HREF="#INVALID_ATTRIBUTE_SYNTAX">INVALID_ATTRIBUTE_SYNTAX</A>
 *  32     <A HREF="#NO_SUCH_OBJECT">NO_SUCH_OBJECT</A>
 *  33     <A HREF="#ALIAS_PROBLEM">ALIAS_PROBLEM</A>
 *  34     <A HREF="#INVALID_DN_SYNTAX">INVALID_DN_SYNTAX</A>
 *  35     <A HREF="#IS_LEAF">IS_LEAF</A>
 *  36     <A HREF="#ALIAS_DEREFERENCING_PROBLEM">ALIAS_DEREFERENCING_PROBLEM</A>
 *  48     <A HREF="#INAPPROPRIATE_AUTHENTICATION">INAPPROPRIATE_AUTHENTICATION</A>
 *  49     <A HREF="#INVALID_CREDENTIALS">INVALID_CREDENTIALS</A>
 *  50     <A HREF="#INSUFFICIENT_ACCESS_RIGHTS">INSUFFICIENT_ACCESS_RIGHTS</A>
 *  51     <A HREF="#BUSY">BUSY</A>
 *  52     <A HREF="#UNAVAILABLE">UNAVAILABLE</A>
 *  53     <A HREF="#UNWILLING_TO_PERFORM">UNWILLING_TO_PERFORM</A>
 *  54     <A HREF="#LOOP_DETECT">LOOP_DETECT</A>
 *  64     <A HREF="#NAMING_VIOLATION">NAMING_VIOLATION</A>
 *  65     <A HREF="#OBJECT_CLASS_VIOLATION">OBJECT_CLASS_VIOLATION</A>
 *  66     <A HREF="#NOT_ALLOWED_ON_NONLEAF">NOT_ALLOWED_ON_NONLEAF</A>
 *  67     <A HREF="#NOT_ALLOWED_ON_RDN">NOT_ALLOWED_ON_RDN</A>
 *  68     <A HREF="#ENTRY_ALREADY_EXISTS">ENTRY_ALREADY_EXISTS</A>
 *  69     <A HREF="#OBJECT_CLASS_MODS_PROHIBITED">OBJECT_CLASS_MODS_PROHIBITED</A>
 *  71     <A HREF="#AFFECTS_MULTIPLE_DSAS">AFFECTS_MULTIPLE_DSAS</A> (LDAP v3)
 *  80     <A HREF="#OTHER">OTHER</A>
 *  81     <A HREF="#SERVER_DOWN">SERVER_DOWN</A>
 *  85     <A HREF="#LDAP_TIMEOUT">LDAP_TIMEOUT</A>
 *  89     <A HREF="#PARAM_ERROR">PARAM_ERROR</A>
 *  91     <A HREF="#CONNECT_ERROR">CONNECT_ERROR</A>
 *  92     <A HREF="#LDAP_NOT_SUPPORTED">LDAP_NOT_SUPPORTED</A>
 *  93     <A HREF="#CONTROL_NOT_FOUND">CONTROL_NOT_FOUND</A>
 *  94     <A HREF="#NO_RESULTS_RETURNED">NO_RESULTS_RETURNED</A>
 *  95     <A HREF="#MORE_RESULTS_TO_RETURN">MORE_RESULTS_TO_RETURN</A>
 *  96     <A HREF="#CLIENT_LOOP">CLIENT_LOOP</A>
 *  97     <A HREF="#REFERRAL_LIMIT_EXCEEDED">REFERRAL_LIMIT_EXCEEDED</A>
 *  100    <A HREF="#INVALID_RESPONSE">INVALID_RESPONSE</A>
 *  101    <A HREF="#AMBIGUOUS_RESPONSE">AMBIGUOUS_RESPONSE</A>
 *  112    <A HREF="#TLS_NOT_SUPPORTED">TLS_NOT_SUPPORTED</A>
 * </PRE>
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPReferralException
 */
public class LDAPException extends Exception
                           implements Serializable {

    static final long serialVersionUID = -9215007872184847924L;

    /**
     * (0) The operation completed successfully.
     */
    public final static int SUCCESS                      = 0;

    /**
     * (1) An internal error occurred in the LDAP server.
     */
    public final static int OPERATION_ERROR              = 1;

    /**
     * (2) A LDAP server could not correctly interpret the request
     * sent by your client because the request does not strictly
     * comply with the LDAP protocol. (For example, the data
     * was not correctly BER-encoded, or a specified value -- such
     * as the search scope or modification type -- does not
     * comply with the LDAP protocol.  If you invent your own
     * search scope, for instance, this result code might be returned.<P>
     */
    public final static int PROTOCOL_ERROR               = 2;

    /**
     * (3) The search operation could not be completed within
     * the maximum time limit.  You can specify the maximum time
     * limit by calling the <CODE>LDAPConnection.setOption</CODE>
     * method or the <CODE>LDAPSearchConstraints.setServerTimeLimit</CODE>
     * method.<P>
     *
     * @see org.ietf.ldap.LDAPConnection#setOption
     * @see org.ietf.ldap.LDAPSearchConstraints#setServerTimeLimit
     */
    public final static int TIME_LIMIT_EXCEEDED          = 3;

    /**
     * (4) The search found more than the maximum number of results.
     * You can specify the maximum number of results by calling the
     * <CODE>LDAPConnection.setOption</CODE> method or the
     * <CODE>LDAPSearchConstraints.setSizeLimit</CODE> method.<P>
     *
     * @see org.ietf.ldap.LDAPConnection#setOption
     * @see org.ietf.ldap.LDAPSearchConstraints#setMaxResults
     */
    public final static int SIZE_LIMIT_EXCEEDED          = 4;

    /**
     * (5) Value returned by an LDAP compare operation if the
     * specified attribute and value is not found in the entry
     * (no matching value found).
     *
     * @see org.ietf.ldap.LDAPConnection#compare
     */
    public final static int COMPARE_FALSE                = 5;

    /**
     * (6) Value returned by an LDAP compare operation if the
     * specified attribute and value is found in the entry
     * (matching value found).
     *
     * @see org.ietf.ldap.LDAPConnection#compare
     */
    public final static int COMPARE_TRUE                 = 6;

    /**
     * (7) The specified authentication method is not supported
     * by the LDAP server that you are connecting to.  The
     * <CODE>LDAPConnection</CODE> class is implemented so that
     * <CODE>LDAPConnection.authenticate</CODE> always
     * uses the LDAP_AUTH_SIMPLE method of authentication.
     * (<CODE>LDAPConnection.authenticate</CODE> does not
     * allow you to select the method of authentication.)<P>
     */
    public final static int AUTH_METHOD_NOT_SUPPORTED    = 7;

    /**
     * (8) A stronger authentication method (more than LDAP_AUTH_SIMPLE)
     * is required by the LDAP server that you are connecting to.
     * The <CODE>LDAPConnection</CODE> class is implemented so that
     * <CODE>LDAPConnection.authenticate</CODE> always
     * uses the LDAP_AUTH_SIMPLE method of authentication.
     * (<CODE>LDAPConnection.authenticate</CODE> does not
     * allow you to select the method of authentication.)<P>
     */
    public final static int STRONG_AUTH_REQUIRED         = 8;

    /**
     * (9) The LDAP server is referring your client to another
     * LDAP server.  If you set up the <CODE>LDAPConnection</CODE>
     * options or the <CODE>LDAPConstraints</CODE> options
     * for automatic referral, your client will automatically
     * connect and authenticate to the other LDAP server.
     * (This <CODE>LDAPException</CODE> will not be raised.)
     * <P>
     *
     * (To set up automatic referrals in an <CODE>LDAPConnection</CODE>
     * object, set the <CODE>LDAPConnection.REFERRALS</CODE> option
     * to <CODE>true</CODE> and the LDAPConnection.REFERRALS_REBIND_PROC</CODE>
     * option to the object containing the method for retrieving
     * authentication information (in other words, the distinguished
     * name and password to use when authenticating to other LDAP servers).
     * <P>
     *
     * If instead you set <CODE>LDAPConnection.REFERRALS</CODE>
     * to <CODE>false</CODE> (or if you set
     * <CODE>LDAPConstraints.setReferrals</CODE> to <CODE>false</CODE>,
     * an <CODE>LDAPReferralException</CODE> is raised.
     * <P>
     *
     * If an error occurs during the referral process, an
     * <CODE>LDAPException</CODE> with this result code
     * (<CODE>LDAP_PARTIAL_RESULTS</CODE>) is raised.
     * <P>
     *
     * @see org.ietf.ldap.LDAPConnection#setOption
     * @see org.ietf.ldap.LDAPConstraints#setReferralFollowing
     * @see org.ietf.ldap.LDAPConstraints#setReferralHandler
     * @see org.ietf.ldap.LDAPReferralHandler
     * @see org.ietf.ldap.LDAPAuthHandler
     * @see org.ietf.ldap.LDAPAuthProvider
     * @see org.ietf.ldap.LDAPReferralException
     */
    public final static int LDAP_PARTIAL_RESULTS         = 9;

    /**
     * (10) [LDAP v3] The server does not hold the requested entry.
     * The referral field of the server's response contains a
     * reference to another server (or set of servers), which
     * your client can access through LDAP or other protocols.
     * Typically, these references are LDAP URLs that identify
     * the server that may contain the requested entry.
     * <P>
     *
     * When this occurs, a <CODE>LDAPReferralException</CODE>
     * is thrown. You can catch this exception and call the
     * <CODE>getURLs</CODE> method to get the list of LDAP
     * URLs from the exception.
     * <P>
     *
     * @see org.ietf.ldap.LDAPReferralException
     */
    public final static int REFERRAL                     = 10;

    /**
     * (11) [LDAP v3] The adminstrative limit on the
     * maximum number of entries to return was exceeded.
     * In the Netscape Directory Server 3.0, this
     * corresponds to the "look through limit" for
     * the server.  This is the maximum number of
     * entries that the server will check through
     * when determining which entries match the
     * search filter and scope.
     * <P>
     */
    public final static int ADMIN_LIMIT_EXCEEDED         = 11;

    /**
     * (12) [LDAP v3] The server received an LDAP v3 control
     * that is marked critical and either (1) is not
     * recognized or supported by the server, or
     * (2) is inappropriate for the operation requested.
     * The Netscape Directory Server 3.0 also returns
     * this result code if the client specifies a
     * matching rule that is not supported by the server.
     * <P>
     *
     * @see org.ietf.ldap.LDAPControl
     */
    public final static int UNAVAILABLE_CRITICAL_EXTENSION = 12;

    /**
     * (13) [LDAP v3] A secure connection is required for
     * this operation.
     */
    public final static int CONFIDENTIALITY_REQUIRED         = 13;

    /**
     * (14) [LDAP v3] While authenticating your client
     * by using a SASL (Simple Authentication Security Layer)
     * mechanism, the server requires the client to send
     * a new SASL bind request (specifying the same SASL
     * mechanism) to continue the authentication process.
     */
    public final static int SASL_BIND_IN_PROGRESS        = 14;

    /**
     * (16) The specified attribute could not be found.
     */
    public final static int NO_SUCH_ATTRIBUTE            = 16;

    /**
     * (17) The specified attribute is not defined.
     */
    public final static int UNDEFINED_ATTRIBUTE_TYPE     = 17;

    /**
     * (18) An inappropriate type of matching was used.
     */
    public final static int INAPPROPRIATE_MATCHING       = 18;

    /**
     * (19) An internal error occurred in the LDAP server.
     */
    public final static int CONSTRAINT_VIOLATION         = 19;

    /**
     * (20) The value that you are adding to an attribute
     * already exists in the attribute.
     */
    public final static int ATTRIBUTE_OR_VALUE_EXISTS    = 20;

    /**
     * (21) The request contains invalid syntax.
     */
    public final static int INVALID_ATTRIBUTE_SYNTAX     = 21;

    /**
     * (32) The entry specified in the request does not exist.
     */
    public final static int NO_SUCH_OBJECT               = 32;

    /**
     * (33) An problem occurred with an alias.
     */
    public final static int ALIAS_PROBLEM                = 33;

    /**
     * (34) The specified distinguished name (DN) uses invalid syntax.
     */
    public final static int INVALID_DN_SYNTAX            = 34;

    /**
     * (35) The specified entry is a "leaf" entry (it has no entries
     * beneath it in the directory tree).
     */
    public final static int IS_LEAF                      = 35;

    /**
     * (36) An error occurred when dereferencing an alias.
     */
    public final static int ALIAS_DEREFERENCING_PROBLEM  = 36;

    /**
     * (48) The authentication presented to the server is inappropriate.
     * This result code might occur, for example, if your client
     * presents a password and the corresponding entry has no
     * userpassword attribute.
     */
    public final static int INAPPROPRIATE_AUTHENTICATION = 48;

    /**
     * (49) The credentials presented to the server for authentication
     * are not valid.  (For example, the password sent to the server
     * does not match the user's password in the directory.)
     */
    public final static int INVALID_CREDENTIALS          = 49;

    /**
     * (50) The client is authenticated as a user who does not have the
     * access privileges to perform this operation.
     */
    public final static int INSUFFICIENT_ACCESS_RIGHTS   = 50;

    /**
     * (51) The LDAP server is busy.
     */
    public final static int BUSY                         = 51;

    /**
     * (52) The LDAP server is unavailable.
     */
    public final static int UNAVAILABLE                  = 52;

    /**
     * (53) The LDAP server is unable to perform the specified operation.
     */
    public final static int UNWILLING_TO_PERFORM         = 53;

    /**
     * (54) A loop has been detected.
     */
    public final static int LOOP_DETECT                  = 54;

    /**
     * (60) The "server-side sorting" control
     * was not included with the "virtual list view"
     * control in the search request.
     */
    public final static int SORT_CONTROL_MISSING         = 60;

    /**
     * (61) An index range error occurred.
     */
    public final static int INDEX_RANGE_ERROR            = 61;

    /**
     * (64) A naming violation has occurred.
     */
    public final static int NAMING_VIOLATION             = 64;

    /**
     * (65) The requested operation will add or change
     * data so that the data no longer complies with
     * the schema.
     */
    public final static int OBJECT_CLASS_VIOLATION       = 65;

    /**
     * (66) The requested operation can only be performed
     * on an entry that has no entries beneath it in the
     * directory tree (in other words, a "leaf" entry).
     * <P>
     *
     * For example, you cannot delete or rename an entry
     * if the entry has subentries beneath it.
     * <P>
     */
    public final static int NOT_ALLOWED_ON_NONLEAF       = 66;

    /**
     * (67) The specified operation cannot be performed on
     * a relative distinguished name (RDN).
     */
    public final static int NOT_ALLOWED_ON_RDN           = 67;

    /**
     * (68) The specified entry already exists.  You might receive
     * this error if, for example, you attempt to add an entry
     * that already exists or if you attempt to change the name
     * of an entry to the name of an entry that already exists.
     */
    public final static int ENTRY_ALREADY_EXISTS         = 68;

    /**
     * (69) You cannot modify the specified object class.
     */
    public final static int OBJECT_CLASS_MODS_PROHIBITED = 69;

    /**
     * (71) [LDAP v3] The client attempted to move an entry
     * from one LDAP server to another by requesting a "modify
     * DN" operation.  In general, clients should not be able
     * to arbitrarily move entries and subtrees between servers.
     * <P>
     *
     * @see org.ietf.ldap.LDAPConnection#rename(java.lang.String, java.lang.String, java.lang.String, boolean)
     * @see org.ietf.ldap.LDAPConnection#rename(java.lang.String, java.lang.String, java.lang.String, boolean, LDAPConstraints)
     */
    public final static int AFFECTS_MULTIPLE_DSAS        = 71;

    /**
     * (80) General result code for other types of errors
     * that may occur.
     */
    public final static int OTHER                        = 80;

    /**
     * (81) The LDAP server cannot be contacted.
     */
    public final static int SERVER_DOWN                  = 0x51;

    /**
     * (85) The operation could not be completed within the
     * maximum time limit. You can specify the maximum time limit
     * by calling the <CODE>LDAPConstraints.setTimeLimit</CODE>
     * method.<P>
     *
     * @see org.ietf.ldap.LDAPConstraints#setTimeLimit
     */
    public final static int LDAP_TIMEOUT                 = 0x55;


    /**
     * (89) When calling a constructor or method from your client,
     * one or more parameters were incorrectly specified.
     */
    public final static int PARAM_ERROR                  = 0x59;

    /**
     * (91) Your LDAP client failed to connect to the LDAP server.
     */
    public final static int CONNECT_ERROR                = 0x5b;

    /**
     * (92) The request is not supported by this version of the LDAP protocol.
     */
    public final static int LDAP_NOT_SUPPORTED           = 0x5c;

    /**
     * (93) The requested control is not found.
     * <P>
     *
     * @see org.ietf.ldap.LDAPControl
     */
    public final static int CONTROL_NOT_FOUND            = 0x5d;

    /**
     * (94) No results have been returned from the server.
     */
    public final static int NO_RESULTS_RETURNED          = 0x5e;

    /**
     * (95) More results are being returned from the server.
     */
    public final static int MORE_RESULTS_TO_RETURN       = 0x5f;

    /**
     * (96) Your LDAP client detected a loop in the referral.
     */
    public final static int CLIENT_LOOP                  = 0x60;

    /**
     * (97) The number of sequential referrals (for example,
     * the client may be referred first from LDAP server A to
     * LDAP server B, then from LDAP server B to LDAP server C,
     * and so on) has exceeded the maximum number of referrals
     * (the <CODE>LDAPConnection.REFERRALS_HOP_LIMIT</CODE> option).
     * <P>
     *
     * @see org.ietf.ldap.LDAPConnection#REFERRALS_HOP_LIMIT
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public final static int REFERRAL_LIMIT_EXCEEDED      = 0x61;

    /**
     * (100) Invalid response
     */
    public final static int INVALID_RESPONSE             = 0x64;

    /**
     * (101) Ambiguous response
     */
    public final static int AMBIGUOUS_RESPONSE           = 0x65;

    /**
     * (112) This implementation does not support TLS
     */
    public final static int TLS_NOT_SUPPORTED            = 0x70;

    /**
     * Internal variables
     */
    private int resultCode = -1;
    private String errorMessage = null;
    private String matchedDN = null;
    private Throwable rootException = null;
    private Locale m_locale = Locale.getDefault();
    private static Hashtable cacheResource = new Hashtable();
    private static final String baseName = "org/ietf/ldap/errors/ErrorCodes";

    /**
     * Constructs a default exception with no specific error information.
     */
    public LDAPException() {
    }

    /**
     * Constructs an exception with a specified string of
     * additional information. This string appears if you call
     * the <CODE>toString()</CODE> method.
     * <P>
     *
     * This form is used for lower-level errors.
     * It is recommended that you always use one of the constructors
     * that takes a result code as a parameter. (If your exception is
     * thrown, any code that catches the exception may need to extract
     * the result code from the exception.)
     * <P>
     * @param message the additional error information
     * @see org.ietf.ldap.LDAPException#toString()
     */
    public LDAPException( String message ) {
        super( message );
    }

    /**
     * Constructs an exception with a result code and
     * a specified string of additional information.  This string
     * appears if you call the <CODE>toString()</CODE> method.
     * The result code that you set is accessible through the
     * <CODE>getResultCode()</CODE> method.
     * <P>
     *
     * @param message the additional error information to specify
     * @param resultCode the result code returned from the
     * operation that caused this exception
     * @see org.ietf.ldap.LDAPException#toString()
     * @see org.ietf.ldap.LDAPException#getResultCode()
     */
    public LDAPException( String message, int resultCode ) {
        super( message );
        this.resultCode = resultCode;
    }

    /**
     * Constructs an exception with a result code, a specified
     * string of additional information, and a string containing
     * information passed back from the server.
     * <P>
     *
     * After you construct the <CODE>LDAPException</CODE> object,
     * the result code and messages will be accessible through the
     * following ways:
     * <P>
     * <UL>
     * <LI>The first string of additional information appears if you
     * call the <CODE>toString()</CODE> method. <P>
     * <LI>The result code that you set is accessible through the
     * <CODE>getResultCode()</CODE> method. <P>
     * <LI>The string of server error information that you set
     * is accessible through the <CODE>getLDAPErrorMessage</CODE>
     * method. <P>
     * </UL>
     * <P>
     *
     * Use this form of the constructor
     * for higher-level LDAP operational errors.
     * @param message the additional error information to specify
     * @param resultCode the result code returned from the
     * operation that caused this exception
     * @param serverErrorMessage error message specifying additional
     * information returned from the server
     * @see org.ietf.ldap.LDAPException#toString()
     * @see org.ietf.ldap.LDAPException#getResultCode()
     * @see org.ietf.ldap.LDAPException#getLDAPErrorMessage()
     */
    public LDAPException( String message,
                          int resultCode,
                          String serverErrorMessage ) {
        super( message );
        this.resultCode = resultCode;
        this.errorMessage = serverErrorMessage;
    }

    /**
     * Constructs an exception with a result code, a specified
     * string of additional information, and a root cause exception.
     * <P>
     *
     * After you construct the <CODE>LDAPException</CODE> object,
     * the result code and messages will be accessible through the
     * following ways:
     * <P>
     * <UL>
     * <LI>The first string of additional information appears if you
     * call the <CODE>toString()</CODE> method. <P>
     * <LI>The result code that you set is accessible through the
     * <CODE>getResultCode()</CODE> method. <P>
     * </UL>
     * <P>
     *
     * Use this form of the constructor
     * for higher-level LDAP operational errors.
     * @param message the additional error information to specify
     * @param resultCode the result code returned from the
     * operation that caused this exception
     * @param rootException An exception which caused the failure, if any
     * @see org.ietf.ldap.LDAPException#toString()
     * @see org.ietf.ldap.LDAPException#getResultCode()
     * @see org.ietf.ldap.LDAPException#getLDAPErrorMessage()
     */
    public LDAPException( String message,
                          int resultCode,
                          Throwable rootException ) {
        super( message );
        this.resultCode = resultCode;
        this.rootException = rootException;
    }

    /**
     * Constructs an exception with a result code, a specified
     * string of additional information, and the DN of the
     * closest matching entry. The latter only if the exception was thrown
     * because an entry could not be found (for example, if
     * <CODE>cn=Babs Jensen,ou=People,c=Airius.com</CODE> could not be found
     * but <CODE>ou=People,c=Airius.com</CODE> is a valid directory entry,
     * the &quot;matched DN&quot; is <CODE>ou=People,c=Airius.com</CODE>.
     * <P>
     *
     * After you construct the <CODE>LDAPException</CODE> object,
     * the result code and messages will be accessible through the
     * following ways:
     * <P>
     * <UL>
     * <LI>This string of additional information appears if you
     * call the <CODE>toString()</CODE> method. <P>
     * <LI>The result code that you set is accessible through the
     * <CODE>getResultCode()</CODE> method. <P>
     * <LI>The string of server error information that you set
     * is accessible through the <CODE>getLDAPErrorMessage</CODE>
     * method. <P>
     * <LI>The matched DN that you set is accessible through the
     * <CODE>getMatchedDN</CODE> method.<P>
     * </UL>
     * <P>
     *
     * This form is used for higher-level LDAP operational errors.
     * @param message the additional error information
     * @param resultCode the result code returned
     * @param matchedDN maximal subset of a specified DN which could be
     * matched by the server
     * @see org.ietf.ldap.LDAPException#toString()
     * @see org.ietf.ldap.LDAPException#getResultCode()
     * @see org.ietf.ldap.LDAPException#getLDAPErrorMessage()
     * @see org.ietf.ldap.LDAPException#getMatchedDN()
     */
    public LDAPException( String message,
                          String serverErrorMessage,
                          String matchedDN ) {
        super( message );
        this.resultCode = resultCode;
        this.matchedDN = matchedDN;
    }

    /**
     * Constructs an exception with a result code, a specified
     * string of additional information, a string containing
     * information passed back from the server, and the DN of the
     * closest matching entry. The latter only if the exception was thrown
     * because an entry could not be found (for example, if
     * <CODE>cn=Babs Jensen,ou=People,c=Airius.com</CODE> could not be found
     * but <CODE>ou=People,c=Airius.com</CODE> is a valid directory entry,
     * the &quot;matched DN&quot; is <CODE>ou=People,c=Airius.com</CODE>.
     * <P>
     *
     * After you construct the <CODE>LDAPException</CODE> object,
     * the result code and messages will be accessible through the
     * following ways:
     * <P>
     * <UL>
     * <LI>This string of additional information appears if you
     * call the <CODE>toString()</CODE> method. <P>
     * <LI>The result code that you set is accessible through the
     * <CODE>getResultCode()</CODE> method. <P>
     * <LI>The string of server error information that you set
     * is accessible through the <CODE>getLDAPErrorMessage</CODE>
     * method. <P>
     * <LI>The matched DN that you set is accessible through the
     * <CODE>getMatchedDN</CODE> method.<P>
     * </UL>
     * <P>
     *
     * This form is used for higher-level LDAP operational errors.
     * @param message the additional error information
     * @param resultCode the result code returned
     * @param serverErrorMessage error message specifying additional information
     * returned from the server
     * @param matchedDN maximal subset of a specified DN which could be
     * matched by the server
     * @see org.ietf.ldap.LDAPException#toString()
     * @see org.ietf.ldap.LDAPException#getResultCode()
     * @see org.ietf.ldap.LDAPException#getLDAPErrorMessage()
     * @see org.ietf.ldap.LDAPException#getMatchedDN()
     */
    public LDAPException( String message,
                          int resultCode,
                          String serverErrorMessage,
                          String matchedDN ) {
        super( message );
        this.resultCode = resultCode;
        this.errorMessage = serverErrorMessage;
        this.matchedDN = matchedDN;
    }

    /**
     * Constructs an exception with a result code, a specified
     * string of additional information, a string containing
     * information passed back from the server, and a possible root
     * exception.
     * <P>
     *
     * After you construct the <CODE>LDAPException</CODE> object,
     * the result code and messages will be accessible through the
     * following ways:
     * <P>
     * <UL>
     * <LI>This string of additional information appears if you
     * call the <CODE>toString()</CODE> method. <P>
     * <LI>The result code that you set is accessible through the
     * <CODE>getResultCode()</CODE> method. <P>
     * <LI>The string of server error information that you set
     * is accessible through the <CODE>getLDAPErrorMessage</CODE>
     * method. <P>
     * </UL>
     * <P>
     *
     * This form is used for higher-level LDAP operational errors.
     * @param message the additional error information
     * @param resultCode the result code returned
     * @param serverErrorMessage error message specifying additional information
     * returned from the server
     * @param rootException An exception which caused the failure, if any
     * @see org.ietf.ldap.LDAPException#toString()
     * @see org.ietf.ldap.LDAPException#getResultCode()
     * @see org.ietf.ldap.LDAPException#getLDAPErrorMessage()
     * @see org.ietf.ldap.LDAPException#getMatchedDN()
     */
    public LDAPException( String message,
                          int resultCode,
                          String serverErrorMessage,
                          Throwable rootException ) {
        super( message );
        this.resultCode = resultCode;
        this.errorMessage = serverErrorMessage;
        this.rootException = rootException;
    }

    /**
     * Returns the root exception that caused this exception.
     * @return The possibly null root exception that caused this exception.
     */
    public Throwable getCause() {
        return rootException;
    }

    /**
     * Returns the error message from the last error, if this message
     * is available (that is, if this message was set).  If the message
     * was not set, this method returns <CODE>null</CODE>.
     * <P>
     *
     * Note that this message is rarely set.  (In order to set this message,
     * the code constructing this exception must have called the constructor
     * <CODE>LDAPException(String, int, String)</CODE>.  The last argument,
     * which is additional error information returned from the server,
     * is the string returned by <CODE>getLDAPErrorMessage</CODE>.
     * <P>
     *
     * In most cases, if you want information about
     * the error generated, you should call the <CODE>toString()</CODE>
     * method instead.
     * <P>
     *
     * @return the error message of the last error (or <CODE>null</CODE>
     * if no message was set).
     * @see org.ietf.ldap.LDAPException#toString()
     */
    public String getLDAPErrorMessage () {
        return errorMessage;
    }

    /**
     * Returns the maximal subset of a DN which could be matched by the
     * server. This only the case if the server returned one of the following
     * errors:
     * <UL>
     * <LI><CODE>NO_SUCH_OBJECT</CODE>
     * <LI><CODE>ALIAS_PROBLEM</CODE>
     * <LI><CODE>INVALID_DN_SYNTAX</CODE>
     * <LI><CODE>ALIAS_DEREFERENCING_PROBLEM</CODE>
     * </UL>
     * </PRE>
     * For example, if the DN <CODE>cn=Babs Jensen, o=People, c=Airius.com</CODE>
     * could not be found by the DN <CODE>o=People, c=Airius.com</CODE>
     * could be found, the matched DN is
     * <CODE>o=People, c=Airius.com</CODE>.
     * <P>
     *
     * If the exception does not specify a matching DN,
     * this method returns <CODE>null</CODE>.
     * @return the maximal subset of a DN which could be matched,
     * or <CODE>null</CODE> if the error is not one of the above.
     */
    public String getMatchedDN () {
        return matchedDN;
    }

    /**
     * Returns the result code from the last error that occurred.
     * This result code is defined as a public final static int member
     * of this class.  Note that this value is not always valid.
     * -1 indicates that the result code is invalid.
     * @return the LDAP result code of the last operation.
     */
    public int getResultCode () {
        return resultCode;
    }

    /**
     * Prints this exception's stack trace to <tt>System.err</tt>.
     * If this exception has a root exception; the stack trace of the
     * root exception is printed to <tt>System.err</tt> instead.
     */
    public void printStackTrace() {
        printStackTrace( System.err );
    }

    /**
     * Prints this exception's stack trace to a print stream.
     * If this exception has a root exception; the stack trace of the
     * root exception is printed to the print stream instead.
     * @param ps The non-null print stream to which to print.
     */
    public void printStackTrace(java.io.PrintStream ps) {
        if ( rootException != null ) {
            String superString = super.toString();
            synchronized ( ps ) {
                ps.print(superString
                         + (superString.endsWith(".") ? "" : ".")
                         + "  Root exception is ");
                rootException.printStackTrace( ps );
            }
        } else {
            super.printStackTrace( ps );
        }
    }

    /**
     * Prints this exception's stack trace to a print writer.
     * If this exception has a root exception; the stack trace of the
     * root exception is printed to the print writer instead.
     * @param ps The non-null print writer to which to print.
     */
    public void printStackTrace( java.io.PrintWriter pw ) {
        if ( rootException != null ) {
            String superString = super.toString();
            synchronized (pw) {
                pw.print(superString
                         + (superString.endsWith(".") ? "" : ".")
                         + "  Root exception is ");
                rootException.printStackTrace( pw );
            }
        } else {
            super.printStackTrace( pw );
        }
    }

    /**
     * Returns the error message describing the error code (for this
     * exception). The error message is specific to the default locale
     * for this system.  (The LDAP Java classes determine the default
     * locale by calling the <CODE>java.util.Locale.getDefault</CODE>
     * method and retrieve the error messages from the following file:
     * <PRE>netscape/ldap/error/ErrorCodes_<I>locale_name</I>.props</PRE>
     * where <I>locale_name</I> is the language and country (concatenated
     * and delimited by an underscore) of the default locale. For example:
     * <PRE>netscape/ldap/error/ErrorCodes_en_US.props</PRE>
     *
     * @return the error message describing the error code for this
     * exception in the default locale.
     */
    public String resultCodeToString() {
        return resultCodeToString( resultCode, m_locale );
    }

   /**
     * Returns the error message describing the error code for this
     * exception. The error message for the specified locale is retrieved
     * from the following file:
     * <PRE>netscape/ldap/error/ErrorCodes_<I>locale_name</I>.props</PRE>
     * where <I>locale_name</I> is the language and country (concatenated
     * and delimited by an underscore) of the default locale. For example:
     * <PRE>netscape/ldap/error/ErrorCodes_en_US.props</PRE>
     *
     * @param l the <CODE>java.util.Locale</CODE> object representing the
     * locale of the error message to retrieve
     * @return the error message describing the current error code
     * in the specified locale.
     */
    public String resultCodeToString( Locale l ) {
        return resultCodeToString( resultCode, l );
    }

    /**
     * Returns the error message describing the specified error code.
     * The error message is specific to the default locale
     * for this system.  (The LDAP Java classes determine the default
     * locale by calling the <CODE>java.util.Locale.getDefault</CODE>
     * method and retrieve the error messages from the following file:
     * <PRE>netscape/ldap/error/ErrorCodes_<I>locale_name</I>.props</PRE>
     * where <I>locale_name</I> is the language and country (concatenated
     * and delimited by an underscore) of the default locale. For example:
     * <PRE>netscape/ldap/error/ErrorCodes_en_US.props</PRE>
     *
     * @param code the error code for which to get the
     * corresponding error message
     * @return error message describing the specified error code for
     * the default locale.
     */
    public static String resultCodeToString( int code ) {
        return resultCodeToString( code, Locale.getDefault() );
    }

    /**
     * Returns the error message describing the specified error code.
     * The error message for the specified locale is retrieved from
     * the following file:
     * <PRE>netscape/ldap/error/ErrorCodes_<I>locale_name</I>.props</PRE>
     * where <I>locale_name</I> is the language and country (concatenated
     * and delimited by an underscore) of the default locale. For example:
     * <PRE>netscape/ldap/error/ErrorCodes_en_US.props</PRE>
     *
     * @param code the error code for which to get the
     * corresponding error
     * @param locale the <CODE>java.util.Locale</CODE> object representing the
     * locale of the error message that you want to retrieve
     * @return error message describing the specified error code for
     * the specified locale.
     */
    public synchronized static String resultCodeToString( int code,
                                                          Locale locale ) {
        try {
            String localeStr = locale.toString();
            PropertyResourceBundle p =
               (PropertyResourceBundle)cacheResource.get(localeStr);

            if (p == null) {
                p = LDAPResourceBundle.getBundle(baseName);

                if (p != null)
                    cacheResource.put(localeStr, p);
            }

            if (p != null) {
                return (String)p.handleGetObject(Integer.toString(code));
            }
        } catch (IOException e) {
            System.out.println("Cannot open resource file for LDAPException "+
              baseName);
        }

        return null;
    }

    /**
     * Gets the string representation of the exception, which
     * includes the result code, the message sent back from
     * the LDAP server, the portion of the DN that the server
     * could find in the directory (if applicable), and the
     * error message corresponding to this result code.
     * <P>
     *
     * For example:
     *
     * <PRE>org.ietf.ldap.LDAPException: error result (32); server error message; matchedDN = ou=people,o=airius.com; No such object</PRE>
     *
     * In this example, <CODE>error result</CODE> is the string of
     * additional information specified in the exception, <CODE>32</CODE> is
     * the result code, <CODE>server error message</CODE> is the additional
     * information from the server specified in the exception, the
     * matched DN is <CODE>ou=people,o=airius.com</CODE>, and the error message
     * corresponding to the result code <CODE>32</CODE> is <CODE>No such
     * object</CODE>.
     * <P>
     *
     * The error message corresponding to the error code can also be
     * retrieved by using the <CODE>resultCodeToString</CODE> method.
     * Note that this method can generate error messages specific to
     * a current locale.
     * <P>
     *
     * @return string representation of exception.
     * @see org.ietf.ldap.LDAPException#resultCodeToString(int)
     */
    public String toString() {
        String str = super.toString() + " (" + resultCode + ")" ;
        if ( (errorMessage != null) && (errorMessage.length() > 0) ) {
            str += "; " + errorMessage;
        }
        if ( (matchedDN != null) && (matchedDN.length() > 0) ) {
            str += "; matchedDN = " + matchedDN;
        }
        String errorStr = null;
        if ( ((errorStr = resultCodeToString(m_locale)) != null) &&
             (errorStr.length() > 0) ) {
            str += "; " + errorStr;
        }
        if ( (rootException != null) && (rootException != this) ) {
            str += " [Root exception is " + rootException.toString() + "]";
        }
        return str;
    }
}
