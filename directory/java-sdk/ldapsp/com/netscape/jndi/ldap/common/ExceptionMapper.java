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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package com.netscape.jndi.ldap.common;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;

import netscape.ldap.*;

import java.util.Hashtable;

/**
 * Class used to map ldapjdk exceptions to JNDI NamingException
*/
public class ExceptionMapper {

    public static NamingException getNamingException(Exception origException) {
        
        if (origException instanceof NamingException) {
            return (NamingException)origException;
        }
        
        /**
         * LdapJDK exceptions
         */
        else if (origException instanceof LDAPReferralException) {
            // Should never get here. The ldapjdk referral exceptions
            // should be explicitly captured by the ecode and converted
            // into jndi LdapReferralException instances.
            return new NamingException("Provider internal error, LDAPReferralException not captured");
        }
        
        else if (origException instanceof LDAPException) {
            LDAPException ldapException = (LDAPException) origException;
            int resCode = ldapException.getLDAPResultCode();
            
            if (resCode == LDAPException.OPERATION_ERROR) {
                NamingException nameEx = new NamingException(ldapException.toString());
                nameEx.setRootCause(ldapException);
                return nameEx;
            }
            else if (resCode == LDAPException.PROTOCOL_ERROR) {
                return new CommunicationException(ldapException.toString());
            }
            else if (resCode == LDAPException.TIME_LIMIT_EXCEEDED) {
                return new TimeLimitExceededException(ldapException.toString());
            }
            else if (resCode == LDAPException.SIZE_LIMIT_EXCEEDED) {
                return new SizeLimitExceededException(ldapException.toString());
            }
            // COMPARE_FALSE should never happen, but is included here for completeness
            else if (resCode == LDAPException.COMPARE_FALSE) {
                NamingException nameEx = new NamingException(ldapException.toString());
                nameEx.setRootCause(ldapException);
                return nameEx;
            }    
            // COMPARE_TRUE should never happen, but is included here for completeness
            else if (resCode == LDAPException.COMPARE_TRUE) {
                NamingException nameEx = new NamingException(ldapException.toString());
                nameEx.setRootCause(ldapException);
                return nameEx;
            }
            else if (resCode == LDAPException.AUTH_METHOD_NOT_SUPPORTED) {
                return new AuthenticationNotSupportedException(ldapException.toString());
            }
            else if  (resCode == LDAPException.STRONG_AUTH_REQUIRED) {
                return new AuthenticationNotSupportedException(ldapException.toString());
            }
            else if (resCode == LDAPException.LDAP_PARTIAL_RESULTS) {
                return new PartialResultException(ldapException.toString());
            }
            // REFERRAL code should be in LDAPReferralException
            else if (resCode == LDAPException.REFERRAL) {
                LDAPReferralException referralEx = (LDAPReferralException)ldapException;
                // TODO create jndi LdapReferralException
            }
            else if (resCode == LDAPException.ADMIN_LIMIT_EXCEEDED) {
                return new LimitExceededException(ldapException.toString());
            }
            else if(resCode == LDAPException.UNAVAILABLE_CRITICAL_EXTENSION) {
                return new OperationNotSupportedException(ldapException.toString());
            }
            else if (resCode == LDAPException.CONFIDENTIALITY_REQUIRED) {
                return new AuthenticationNotSupportedException(
                 "A secure connection is required for this operation");
            }
            // This shoud never propagate to the caller
            else if (resCode == LDAPException.SASL_BIND_IN_PROGRESS) {
                NamingException nameEx = new NamingException(ldapException.toString());
                nameEx.setRootCause(ldapException);
                return nameEx;
            }
            else if (resCode == LDAPException.NO_SUCH_ATTRIBUTE) {
                return new NoSuchAttributeException(ldapException.toString());
            }
            else if (resCode == LDAPException.UNDEFINED_ATTRIBUTE_TYPE) {
                return  new InvalidAttributeIdentifierException(ldapException.toString());
            }
            else if (resCode == LDAPException.INAPPROPRIATE_MATCHING) {
                return new InvalidSearchFilterException(ldapException.toString());
            }
            else if (resCode == LDAPException.CONSTRAINT_VIOLATION) {
                return new InvalidSearchControlsException(ldapException.toString());
            }
            else if (resCode == LDAPException.ATTRIBUTE_OR_VALUE_EXISTS) {
                return new AttributeInUseException(ldapException.toString());
            }
            else if (resCode == LDAPException.INVALID_ATTRIBUTE_SYNTAX) {
                return new InvalidAttributeValueException(ldapException.toString());
            }
            else if (resCode == LDAPException.NO_SUCH_OBJECT) {
                return new NameNotFoundException(ldapException.toString());
            }
            else if (resCode == LDAPException.ALIAS_PROBLEM) {
                return new NamingException(ldapException.toString());
            }
            else if (resCode == LDAPException.INVALID_DN_SYNTAX) {
                return new InvalidNameException(ldapException.toString());
            }
            else if (resCode == LDAPException.IS_LEAF) {
                NamingException nameEx = new NamingException(ldapException.toString());
                nameEx.setRootCause(ldapException);
                return nameEx;
            }
            else if (resCode == LDAPException.ALIAS_DEREFERENCING_PROBLEM) {
                NamingException nameEx = new NamingException(ldapException.toString());
                nameEx.setRootCause(ldapException);
                return nameEx;
            }
            else if (resCode == LDAPException.INAPPROPRIATE_AUTHENTICATION) {
                //return new AuthenticationNotSupportedException(ldapException.toString());
                return new AuthenticationException(ldapException.toString());
            }
            else if (resCode == LDAPException.INVALID_CREDENTIALS) {
                return new AuthenticationException(ldapException.toString());
            }
            else if (resCode == LDAPException.INSUFFICIENT_ACCESS_RIGHTS) {
                return new NoPermissionException(ldapException.toString());
            }
            else if (resCode == LDAPException.BUSY) {
                return new ServiceUnavailableException(ldapException.toString());
            }
            else if (resCode == LDAPException.UNAVAILABLE) {
                return new ServiceUnavailableException(ldapException.toString());
            }
            else if (resCode == LDAPException.UNWILLING_TO_PERFORM) {
                return new OperationNotSupportedException(ldapException.toString());
            }
            else if (resCode == LDAPException.LOOP_DETECT) {
                NamingException nameEx = new NamingException(ldapException.toString());
                nameEx.setRootCause(ldapException);
                return nameEx;
            }
            else if (resCode == LDAPException.NAMING_VIOLATION) {
                return new InvalidNameException(ldapException.toString());                
            }
            else if (resCode == LDAPException.OBJECT_CLASS_VIOLATION) {
                return new SchemaViolationException(ldapException.toString());                
            }
            else if (resCode == LDAPException.NOT_ALLOWED_ON_NONLEAF) {
                return new ContextNotEmptyException(ldapException.toString());                
            }
            else if (resCode == LDAPException.NOT_ALLOWED_ON_RDN) {
                return new SchemaViolationException(ldapException.toString());                
            }
            else if (resCode == LDAPException.ENTRY_ALREADY_EXISTS) {
                return new NameAlreadyBoundException(ldapException.toString());                
            }
            else if (resCode == LDAPException.OBJECT_CLASS_MODS_PROHIBITED) {
                return new SchemaViolationException(ldapException.toString());                
            }
            else if (resCode == LDAPException.SERVER_DOWN) {
                return new CommunicationException(ldapException.toString());                
            }
            else if (resCode == LDAPException.CONNECT_ERROR) {
                return new CommunicationException(ldapException.toString());                
            }

            else {
                // All other result codes
                // 71     AFFECTS_MULTIPLE_DSAS
                // 80     OTHER
                // 89     PARAM_ERROR
                // 92     LDAP_NOT_SUPPORTED
                // 93     CONTROL_NOT_FOUND
                // 94     NO_RESULTS_RETURNED
                // 95     MORE_RESULTS_TO_RETURN
                // 96     CLIENT_LOOP                referral                
                // 97     REFERRAL_LIMIT_EXCEEDED    referral

                NamingException nameEx = new NamingException(ldapException.toString());
                nameEx.setRootCause(ldapException);
                return nameEx;
            }
        }
        
        /**
         * All other Exceptions
         */
        NamingException nameEx = new NamingException(origException.toString());
        nameEx.setRootCause(origException);
        return nameEx;
    }
}
