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
import com.netscape.jndi.ldap.controls.NetscapeControlFactory;
import com.netscape.jndi.ldap.common.ExceptionMapper;
import netscape.ldap.*;
import java.util.*;

/**
 * A wrapper for the LDAPSeatchResults. Convert a LDAPSearchResults enumeration
 * of LDAPEntries into a JNDI NamingEnumeration of JNDI SearchResults.
 */
class SearchResultEnum extends BaseSearchEnum {

    boolean m_returnObjs; // ReturningObjFlag in SearchControls
    String[] m_userBinaryAttrs;
    
    public SearchResultEnum(LDAPSearchResults res, boolean returnObjs, LdapContextImpl ctx) throws NamingException{
        super(res, ctx);
        m_returnObjs = returnObjs;
        m_userBinaryAttrs = ctx.m_ctxEnv.getUserDefBinaryAttrs();        
    }

    public Object next() throws NamingException{
        LDAPEntry entry = nextLDAPEntry();
        String name = LdapNameParser.getRelativeName(m_ctxName, entry.getDN());
        Object obj = (m_returnObjs) ? ObjectMapper.entryToObject(entry, m_ctx) : null;
        Attributes attrs = new AttributesImpl(entry.getAttributeSet(), m_userBinaryAttrs);
            
        // check for response controls
        LDAPControl[] ldapCtls = m_res.getResponseControls();
        if (ldapCtls != null) {
            // Parse raw controls
            Control[] ctls = new Control[ldapCtls.length];
            for (int i=0; i < ldapCtls.length; i++) {
                ctls[i] = NetscapeControlFactory.getControlInstance(ldapCtls[i]);
                if (ctls[i] == null) {
                    throw new NamingException("Unsupported control " + ldapCtls[i].getID());
                }
            }

            SearchResultWithControls searchRes = 
                new SearchResultWithControls(name, obj, attrs);
            searchRes.setControls(ctls);

            return searchRes;
        }
        else { // no controls
            return new SearchResult(name, obj, attrs);
        }    
    }
}
