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
 * Wrapper for the LDAPSearchResult. Convert each LDAPEntry entry into
 * a JNDI Binding. Bindings are accessed through the NamingEnumeration
 * interface
 */
class BindingEnum implements NamingEnumeration {

    LDAPSearchResults m_res;
    LdapContextImpl m_ctx;
    Name m_ctxName;

    public BindingEnum(LDAPSearchResults res, LdapContextImpl ctx)  throws NamingException {
        m_res = res;
        m_ctx = ctx;
        try {
            m_ctxName = LdapNameParser.getParser().parse(m_ctx.m_ctxDN);
        }
        catch ( NamingException e ) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    public Object next() throws NamingException{
        try {
            LDAPEntry entry = m_res.next();
            String name = LdapNameParser.getRelativeName(m_ctxName, entry.getDN());
            Object obj = ObjectMapper.entryToObject(entry, m_ctx);
            String className = obj.getClass().getName();
            return new Binding(name, className, obj, /*isRelative=*/true);

        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(m_ctx, e);
        }    
        catch ( LDAPException e ) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    public Object nextElement() {
        try {
            return next();
        }
        catch ( Exception e ) {
            System.err.println( "Error in BindingEnum.nextElement(): " + e.toString() );
            e.printStackTrace(System.err);
            return null;
        }
    }

    public boolean hasMore() throws NamingException{
        return m_res.hasMoreElements();
    }

    public boolean hasMoreElements() {
        return m_res.hasMoreElements();
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