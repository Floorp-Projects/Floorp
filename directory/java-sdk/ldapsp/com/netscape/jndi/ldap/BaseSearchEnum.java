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
package com.netscape.jndi.ldap;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;
import com.netscape.jndi.ldap.common.ExceptionMapper;

import netscape.ldap.*;

import java.util.*;

/**
 * Wrapper for the LDAPSearchResult that implements all NamingEnumeration methods 
 * except next() (left to be implemented by subclasses). Because LDAPJDK does
 * not provide for capability to ignoral referrals, the class is using hasMore()
 * method to read ahead search results and "ignore" referrals if required.
 * Base class for BindingEnum, NameClassPairEnum and SearchResultEnum
 */
abstract class BaseSearchEnum implements NamingEnumeration {

    LDAPSearchResults m_res;
    LdapContextImpl m_ctx;
    Name m_ctxName;
    private LDAPEntry nextEntry;
    private LDAPException nextException;
    
    public BaseSearchEnum(LDAPSearchResults res, LdapContextImpl ctx) throws NamingException{
        m_res = res;
        m_ctx = ctx;
        try {
            m_ctxName = LdapNameParser.getParser().parse(m_ctx.m_ctxDN);
        }
        catch ( NamingException e ) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    LDAPEntry nextLDAPEntry() throws NamingException{
        if (nextException == null && nextEntry == null) {
            hasMore();
        }            
        try {        
            if (nextException != null) {
                if (nextException instanceof LDAPReferralException) {
                    throw new LdapReferralException(m_ctx,
                              (LDAPReferralException)nextException);
                }
                else {
                    throw ExceptionMapper.getNamingException(nextException);
                }
            }
            return nextEntry;
        }
        finally {
            nextException = null;
            nextEntry = null;
        }       
    }

    public Object nextElement() {
        try {
            return next();
        }
        catch ( Exception e ) {
            System.err.println( "Error in BaseSearchEnum.nextElement(): " + e.toString() );
            e.printStackTrace(System.err);
            return null;
        }        
    }

    public boolean hasMore() throws NamingException{
        
        if (nextEntry != null || nextException != null) {
            return true;
        }
        
        if (m_res.hasMoreElements()) {
            try {
                nextEntry = m_res.next();
                return true;
            }
            catch (LDAPReferralException e) {
                boolean ignoreReferrals = m_ctx.m_ctxEnv.ignoreReferralsMode();
                if (ignoreReferrals) {
                    
                    return hasMore();
                    
                    // PARTIAL_SEARCH_RESULT should be thrown according to the
                    // Implmentation Guidelines for LDAPSP, but is not done by
                    // the Sun LDAPSP 1.2, so we not doing it either.
                    //nextException = new LDAPException("Ignoring referral", 9);
                    //return true;
                }
                else {
                    nextException = e;
                    return true;
                }       
            }
            catch ( LDAPException e ) {
                nextException = e;
                return true;
            }
        }
        return false;
    }

    public boolean hasMoreElements() {
        try {
            return hasMore();
        }
        catch ( Exception e ) {
            System.err.println( "Error in BaseSearchEnum.hasMoreElements(): " + e.toString() );
            e.printStackTrace(System.err);
            return false;
        }        

    }

    public void close() throws NamingException{
        try {
            m_ctx.m_ldapSvc.getConnection().abandon(m_res);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }
}

