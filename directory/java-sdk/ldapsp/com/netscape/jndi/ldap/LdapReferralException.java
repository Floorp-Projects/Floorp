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
package com.netscape.jndi.ldap;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;

import netscape.ldap.*;

import java.util.*;

/**
 * A wrapper for the ldapjdk LDAPReferralException
 *
 */
class LdapReferralException extends javax.naming.ldap.LdapReferralException {
    
    LDAPReferralException m_ldapEx;
    LdapContextImpl m_srcCtx;
    int m_referralIdx = 0;
    
    public LdapReferralException(LdapContextImpl srcCtx, LDAPReferralException ldapEx) {
        m_srcCtx = srcCtx;
        m_ldapEx = ldapEx;
    }
    
    public Object getReferralInfo() {
        return m_ldapEx.getURLs()[m_referralIdx].toString();
    }
    
    public Context getReferralContext() throws NamingException{
        Hashtable env = m_srcCtx.getEnv().getAllProperties();
        env.put(ContextEnv.P_PROVIDER_URL, m_ldapEx.getURLs()[m_referralIdx].toString());
        return new LdapContextImpl(env);
    }

    public Context getReferralContext(Hashtable env) throws NamingException{
        return getReferralContext(env, null);
    }
    
    public Context getReferralContext(Hashtable env, Control[] reqCtls) throws NamingException{
        
        env.put(ContextEnv.P_PROVIDER_URL, m_ldapEx.getURLs()[m_referralIdx].toString());
        if (reqCtls != null) {
             env.put(ContextEnv.P_CONNECT_CTRLS, reqCtls);
        }             
        return new LdapContextImpl(env);
    }
    

    /**
     * Skip the referral to be processed
     */
    public boolean skipReferral() {
        int maxIdx = m_ldapEx.getURLs().length - 1;
        if (m_referralIdx < maxIdx) {
            m_referralIdx++;
            return true;
        }
        return false;
    }    

    public void retryReferral() {
    }        
}
