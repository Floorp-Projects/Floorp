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
import javax.naming.event.*;

import netscape.ldap.*;
import netscape.ldap.controls.*;

import com.netscape.jndi.ldap.controls.NetscapeControlFactory;
import com.netscape.jndi.ldap.common.Debug;

import java.util.*;

/**
 * Implementation for the DirContext. The context also supports controls
 * through the implementation of LdapContext interface and events through
 * the implementaion of EventDirContext.
 * Semantically, the LdapContextImpl corresponds to a directory entry.
 * Thus a context is associated with a DN (m_ctxDN). Multiple contexts share
 * the same LDAPConnection which is wrapped into a LdapService object
 * (m_ldapSvc). Each context also maintains a set of environment properties
 * (m_ctxEnv). A context environment is shared among mutiple contexts using a
 * variation of copy-on-write algorithm (see common.ShareableEnv class).
 * 
 * Each context also maintains a set of LDAPSearchConstraints, as search
 * constrainsts like e.g. server controls, or max number of returned search
 * search results, are context specific. The LdapService reads the 
 * LDAPSearchConstraints from a context that makes a service request.
 */
public class LdapContextImpl implements EventDirContext, LdapContext {

	/**
	 * Context environment setting
	 */
	protected ContextEnv m_ctxEnv;

	/**
	 * DN associated with this context
	 * The default value is the root DSE ("")
	 */
	protected String m_ctxDN;

	/**
	 * Ldap Connection/Service
	 */
	protected LdapService m_ldapSvc;


	/**
	 * Ldap Connection Search Constraints
	 */
	protected LDAPSearchConstraints m_searchCons;

	// TODO Should have a constructor that accepts attributes
	
	/**
	 * Constructor
	 */
	public LdapContextImpl(Hashtable env) throws NamingException{
		m_ctxEnv = new ContextEnv(env); // no need to clone (Hashtable)env.clone());
		m_ldapSvc = new LdapService(this);
		m_ldapSvc.connect(); // BLITS but to be removed, hurts lazy resource usage
		getDN();
		getSearchConstraints();
	}

	/**
	 * Copy Constructor
	 */
	public LdapContextImpl(String ctxDN, LdapContextImpl cloneCtx) throws NamingException{
		
		m_ctxEnv = (ContextEnv)cloneCtx.m_ctxEnv.clone();

        // An instance of ldapService is shared among multiple contexts.
        // Increment the client reference count
		m_ldapSvc = cloneCtx.m_ldapSvc;
	    cloneCtx.m_ldapSvc.incrementClientCount();
		
		if (cloneCtx.getSearchConstraints().getServerControls() == null) {
			m_searchCons = cloneCtx.getSearchConstraints();
		}
		else {
			// In LdapContext Context Controls are not inherited by derived contexts
			m_searchCons = (LDAPSearchConstraints) cloneCtx.getSearchConstraints().clone();
			m_searchCons.setServerControls((LDAPControl[])null);
		}	
		
		m_ctxDN = ctxDN;
	}


    /**
     * Close the context when finalized
     */
	protected void finalize() {
	    Debug.println(1, "finalize ctx");
	    try {
	        close();
	    }
	    catch (Exception e) {}
	}    
	
	/**
	 * Disconnect the Ldap Connection if close is requested
	 * LDAP operations can not be performed any more ones
	 * the context is closed
	 */
	public void close() throws NamingException {
		m_ldapSvc.disconnect();
		m_ldapSvc = null;
	}

	/**
	 * Return LdapJdk search constraints for this context
	 */ 
	LDAPSearchConstraints getSearchConstraints() throws NamingException{
		if (m_searchCons == null) {
			LDAPSearchConstraints cons = new LDAPSearchConstraints();
			m_ctxEnv.updateSearchCons(cons);
			m_searchCons = cons;
		}	
		return m_searchCons;
	}

	/**
	 * Return DN for this context
	 */
	String getDN() throws NamingException{
		if (m_ctxDN == null) {
            LDAPUrl url = m_ctxEnv.getDirectoryServerURL();  
			if (url != null && url.getDN() != null) {
				m_ctxDN = url.getDN();
			}
			else {
				m_ctxDN = "";
			}
		}
		return m_ctxDN;
	}
	
	/**
	 * Return reference to the context environment
	 */
	ContextEnv getEnv() {
	    return m_ctxEnv;
	} 
	
	/**
	 * Conver object to String
	 */
	public String toString() {
	    return this.getClass().getName() + ": " + m_ctxDN;
	}
	
	/**
	 * Check if LdapURL is passed as the name paremetr to a method
	 * If that's the case, craete environment for the ldap url
	 */
    String checkLdapUrlAsName(String name) throws NamingException{
		if (name.startsWith("ldap://")) {
		    m_ctxEnv.setProperty(ContextEnv.P_PROVIDER_URL, name);		    
		    close(); // Force reconnect
		    m_ldapSvc = new LdapService(this);
		    // Return New name relative to the context
		    return "";
		}
		return name;
	}	
	 
	/**
	 * Environment operatins (javax.naming.Context interface)
	 */
	  
	public Hashtable getEnvironment() throws NamingException {
		return m_ctxEnv.getAllProperties();
	}

	public Object addToEnvironment(String propName, Object propValue) throws NamingException {
		return m_ctxEnv.updateProperty(propName, propValue, getSearchConstraints());
	}

	public Object removeFromEnvironment(String propName) throws NamingException {
		return m_ctxEnv.removeProperty(propName);
	}

	/**
	 * Name operations (javax.naming.Context interface)
	 */

	public String composeName(String name, String prefix) throws NamingException {
		return name + "," + prefix;
	}

	public Name composeName(Name name, Name prefix) throws NamingException {
		String compoundName = composeName(name.toString(), prefix.toString());
		return LdapNameParser.getParser().parse(compoundName);
	}

	public String getNameInNamespace() throws NamingException {
		return new String(m_ctxDN);
	}

	public NameParser getNameParser(String name) throws NamingException {
		return LdapNameParser.getParser();
	}

	public NameParser getNameParser(Name name) throws NamingException {
		return LdapNameParser.getParser();
	}
	 
	/**
	 * Search operations (javax.naming.DirContext interface)
     */
     
	public NamingEnumeration search(String name, String filter, SearchControls cons) throws NamingException {
		name = checkLdapUrlAsName(name);
		return m_ldapSvc.search(this, name, filter, /*attrs=*/null, cons);
	}

	public NamingEnumeration search(String name, String filterExpr, Object[] filterArgs, SearchControls cons) throws NamingException {
		name = checkLdapUrlAsName(name);
		String filter = ProviderUtils.expandFilterExpr(filterExpr, filterArgs);
		return m_ldapSvc.search(this, name, filter, /*attrs=*/null, cons);
	}

	public NamingEnumeration search(String name, Attributes matchingAttributes) throws NamingException {
		name = checkLdapUrlAsName(name);
		String filter = ProviderUtils.attributesToFilter(matchingAttributes);
		return m_ldapSvc.search(this, name, filter, /*attrs=*/null, /*jndiCons=*/null);
	}

	public NamingEnumeration search(String name, Attributes matchingAttributes, String[] attributesToReturn) throws NamingException {
		name = checkLdapUrlAsName(name);
		String filter = ProviderUtils.attributesToFilter(matchingAttributes);
		return m_ldapSvc.search(this, name, filter, attributesToReturn, /*jndiCons=*/null);
	}

	public NamingEnumeration search(Name name, String filter, SearchControls cons) throws NamingException {
		return m_ldapSvc.search(this, name.toString(), filter, /*attrs=*/null, cons);
	}

	public NamingEnumeration search(Name name, String filterExpr, Object[] filterArgs, SearchControls cons) throws NamingException {
		String filter = ProviderUtils.expandFilterExpr(filterExpr, filterArgs);
		return m_ldapSvc.search(this, name.toString(), filter, /*attrs=*/null, cons);
	}

	public NamingEnumeration search(Name name, Attributes attrs) throws NamingException {
		String filter = ProviderUtils.attributesToFilter(attrs);
		return m_ldapSvc.search(this, name.toString(), filter, /*attr=*/null, /*jndiCons=*/null);
	}

	public NamingEnumeration search(Name name, Attributes matchingAttributes, String[] attributesToReturn) throws NamingException {
		String filter = ProviderUtils.attributesToFilter(matchingAttributes);
		return m_ldapSvc.search(this, name.toString(), filter, attributesToReturn, /*jndiCons=*/null);
	}	

	/**
	 * Attribute Operations (javax.naming.DirContext interface)
	 */

	public Attributes getAttributes(String name) throws NamingException {
		name = checkLdapUrlAsName(name);
		return m_ldapSvc.readAttrs(this, name, null);
	}

	public Attributes getAttributes(String name, String[] attrIds) throws NamingException {
		name = checkLdapUrlAsName(name);
		return m_ldapSvc.readAttrs(this, name, attrIds);
	}

	public Attributes getAttributes(Name name) throws NamingException {
		return m_ldapSvc.readAttrs(this, name.toString(), null);

	}

	public Attributes getAttributes(Name name, String[] attrIds) throws NamingException {
		return m_ldapSvc.readAttrs(this, name.toString(), attrIds);

	}

	public void modifyAttributes(String name, int mod_op, Attributes attrs) throws NamingException {
		name = checkLdapUrlAsName(name);
		m_ldapSvc.modifyEntry(this, name, AttributesImpl.jndiAttrsToLdapModSet(mod_op, attrs));
	}

	public void modifyAttributes(String name, ModificationItem[] mods) throws NamingException {
		name = checkLdapUrlAsName(name);
		m_ldapSvc.modifyEntry(this, name, AttributesImpl.jndiModsToLdapModSet(mods));
	}

	public void modifyAttributes(Name name, int mod_op, Attributes attrs) throws NamingException {
		m_ldapSvc.modifyEntry(this, name.toString(), AttributesImpl.jndiAttrsToLdapModSet(mod_op, attrs));

	}

	public void modifyAttributes(Name name, ModificationItem[] mods) throws NamingException {
		m_ldapSvc.modifyEntry(this, name.toString(), AttributesImpl.jndiModsToLdapModSet(mods));
	}

	/**
	 * Ldap entry operations (javax.naming.DirContext interface)
	 */

	public Context createSubcontext(String name) throws NamingException {
		// Directory entry must have attributes
		throw new OperationNotSupportedException();
	}

	public Context createSubcontext(Name name) throws NamingException {
		// Directory entry must have attributes
		throw new OperationNotSupportedException();
	}

	public DirContext createSubcontext(String name, Attributes attrs) throws NamingException {
		name = checkLdapUrlAsName(name);
		return m_ldapSvc.addEntry(this, name, AttributesImpl.jndiAttrsToLdapAttrSet(attrs));
	}

	public DirContext createSubcontext(Name name, Attributes attrs) throws NamingException {
		return m_ldapSvc.addEntry(this, name.toString(), AttributesImpl.jndiAttrsToLdapAttrSet(attrs));

	}

	public void destroySubcontext(String name) throws NamingException {
		name = checkLdapUrlAsName(name);
		m_ldapSvc.delEntry(this, name);
	}

	public void destroySubcontext(Name name) throws NamingException {
		m_ldapSvc.delEntry(this, name.toString());

	}

	/**
	 * Naming Bind/Rename operations
	 * (javax.naming.Context, javax.naming.DirContext interface)
	 */

	public void bind(String name, Object obj) throws NamingException {		
		name = checkLdapUrlAsName(name);
		m_ldapSvc.addEntry(this, name.toString(),
		    ObjectMapper.objectToAttrSet(obj, name, this, /*attrs=*/null));
	}	

	public void bind(Name name, Object obj) throws NamingException {
		bind(name.toString(), obj);
	}

	public void bind(String name, Object obj, Attributes attrs) throws NamingException {
		name = checkLdapUrlAsName(name);
		m_ldapSvc.addEntry(this, name.toString(),
		    ObjectMapper.objectToAttrSet(obj, name, this, attrs));
	}

	public void bind(Name name, Object obj, Attributes attrs) throws NamingException {
		bind(name.toString(), obj, attrs);
	}

	public void rebind(String name, Object obj) throws NamingException {
		rebind(name, obj, /*attrs=*/null);
	}

	public void rebind(Name name, Object obj) throws NamingException {
		rebind(name.toString(), obj, null);
	}

	public void rebind(String name, Object obj, Attributes attrs) throws NamingException {
		name = checkLdapUrlAsName(name);
		try {
			bind(name, obj, attrs);
		}
		catch (NameAlreadyBoundException ex) {
			unbind(name);
			bind(name, obj, attrs);
		}
	}

	public void rebind(Name name, Object obj, Attributes attrs) throws NamingException {
		rebind(name.toString(), obj, attrs);
	}

	public void rename(String oldName, String newName) throws NamingException {
	    oldName = checkLdapUrlAsName(oldName);
	    LdapNameParser parser = LdapNameParser.getParser();
	    Name oldNameObj = parser.parse(oldName);
        Name newNameObj = parser.parse(newName);
        rename(oldNameObj, newNameObj);
	}

	public void rename(Name oldName, Name newName) throws NamingException {
	    // Can rename only RDN	    
        if (newName.size() != oldName.size()) {
            throw new InvalidNameException("Invalid name " + newName);
        }
        Name oldPrefix = oldName.getPrefix(oldName.size() -1);
        Name newPrefix = newName.getPrefix(oldName.size() -1);
        if (!newPrefix.equals(oldPrefix)) {
            throw new InvalidNameException("Invalid name " + newName);
        }
		m_ldapSvc.changeRDN(this, oldName.toString(), newName.get(newName.size()-1));
	}

	public void unbind(String name) throws NamingException {
		name = checkLdapUrlAsName(name);
		// In ldap every entry is naming context
		destroySubcontext(name);
	}

	public void unbind(Name name) throws NamingException {
		// In ldap every entry is naming context
		destroySubcontext(name);
	}

	/**
	 * List Operations (javax.naming.Context interface)
	 */

	public NamingEnumeration list(String name) throws NamingException {
		name = checkLdapUrlAsName(name);
		return m_ldapSvc.listEntries(this, name, /*returnBindings=*/false);
	}

	public NamingEnumeration list(Name name) throws NamingException {
		return m_ldapSvc.listEntries(this, name.toString(), /*returnBindings=*/false);
	}

	public NamingEnumeration listBindings(String name) throws NamingException {
		name = checkLdapUrlAsName(name);
		return m_ldapSvc.listEntries(this, name, /*returnBindings=*/true);
	}

	public NamingEnumeration listBindings(Name name) throws NamingException {
		return m_ldapSvc.listEntries(this, name.toString(), /*returnBindings=*/true);
	}

	/**
	 * Lookup Operations (javax.naming.Context interface)
	 */

	public Object lookup(String name) throws NamingException {
		name = checkLdapUrlAsName(name);
		return m_ldapSvc.lookup(this, name);
	}

	public Object lookup(Name name) throws NamingException {
		return m_ldapSvc.lookup(this, name.toString());
	}

	public Object lookupLink(String name) throws NamingException {
		throw new OperationNotSupportedException();
	}

	public Object lookupLink(Name name) throws NamingException {
		throw new OperationNotSupportedException();
	}


	/**
	 * Schema Operations (javax.naming.DirContext interface)
	 */
	public DirContext getSchema(String name) throws NamingException {
		name = checkLdapUrlAsName(name);
		return m_ldapSvc.getSchema(name);
	}

	public DirContext getSchema(Name name) throws NamingException {
		return m_ldapSvc.getSchema(name.toString());
	}

	public DirContext getSchemaClassDefinition(String name) throws NamingException {
		name = checkLdapUrlAsName(name);
		throw new OperationNotSupportedException();
	}

	public DirContext getSchemaClassDefinition(Name name) throws NamingException {
		return getSchemaClassDefinition(name.toString());
	}

	/**
	 * Naming Event methods javax.naming.event.EventDirContext interface)
	 */
	public void addNamingListener(String target, int scope, NamingListener l) throws NamingException {
	    EventService eventSvc = m_ldapSvc.getEventService();
	    String filter = LdapService.DEFAULT_FILTER;
	    SearchControls ctls = new SearchControls();
	    ctls.setSearchScope(scope);
	    eventSvc.addListener(this, target, filter, ctls, l);
	}

	public void addNamingListener(Name target, int scope, NamingListener l) throws NamingException {
		addNamingListener(target.toString(), scope, l);
	}

	public void addNamingListener(String target, String filter, SearchControls ctls, NamingListener l)throws NamingException {
	    EventService eventSvc = m_ldapSvc.getEventService();
	    eventSvc.addListener(this, target, filter, ctls, l);

	}

	public void addNamingListener(Name target, String filter, SearchControls ctls, NamingListener l)throws NamingException {
		addNamingListener(target.toString(), filter, ctls, l);
	}

	public void addNamingListener(String target, String filterExpr, Object[] filterArgs, SearchControls ctls, NamingListener l)throws NamingException {
	    EventService eventSvc = m_ldapSvc.getEventService();
	    String filter = ProviderUtils.expandFilterExpr(filterExpr, filterArgs);
	    eventSvc.addListener(this, target, filter, ctls, l);
	}

	public void addNamingListener(Name target, String filterExpr, Object[] filterArgs, SearchControls ctls, NamingListener l)throws NamingException {
		addNamingListener(target.toString(), filterExpr, filterArgs, ctls, l);
	}

	public void removeNamingListener(NamingListener l) throws NamingException {
	    EventService eventSvc = m_ldapSvc.getEventService();
	    eventSvc.removeListener(l);
	}

    public boolean targetMustExist() {
		return true;
    }

	/**
	 * LdapContext methods (javax.naming.ldap.LdapContext interface)
	 */
	public ExtendedResponse extendedOperation(ExtendedRequest req)throws NamingException {
		throw new OperationNotSupportedException();
	}

	public Control[] getRequestControls() throws NamingException {
		LDAPControl[] ldapCtls = m_searchCons.getServerControls();
		if (ldapCtls == null) {
		    return null;
		}
        Control[] ctls = new Control[ldapCtls.length];
		for (int i=0; i < ldapCtls.length; i++) {
            ctls[i] = (Control) ldapCtls[i];
		}
        return ctls;
	}

	public void setRequestControls(Control[] reqCtls) throws NamingException {
		LDAPControl[] ldapCtls = new LDAPControl[reqCtls.length];
		for (int i=0; i < reqCtls.length; i++) {
		    try {
		        ldapCtls[i] = (LDAPControl) reqCtls[i];
		    }
		    catch (ClassCastException ex) {
		        throw new NamingException(
		            "Unsupported control type " + reqCtls[i].getClass().getName());
            }
        }
        
        getSearchConstraints().setServerControls(ldapCtls);           
	}

	public Control[] getResponseControls() throws NamingException {
		LDAPControl[] ldapCtls = m_ldapSvc.getConnection().getResponseControls();
		if (ldapCtls == null) {
		    return null;
		}
		// Parse raw controls
		Control[] ctls = new Control[ldapCtls.length];
		for (int i=0; i < ldapCtls.length; i++) {
		    ctls[i] = NetscapeControlFactory.getControlInstance(ldapCtls[i]);
		    if (ctls[i] == null) {
		        throw new NamingException("Unsupported control " + ldapCtls[i].getID());
            }
        }
        return ctls;
	}

	public LdapContext newInstance(Control[] reqCtls) throws NamingException {
		LdapContextImpl clone = new LdapContextImpl(m_ctxDN, this);
		// This controls are to be set on the the LDAPConnection
		clone.m_ctxEnv.setProperty(ContextEnv.P_CONNECT_CTRLS, reqCtls);
		return clone;
	}

    public void reconnect(Control[] reqCtls) throws NamingException {
		close();
		m_ldapSvc = new LdapService(this);
		// This controls are to be set on the the LDAPConnection
		m_ctxEnv.setProperty(ContextEnv.P_CONNECT_CTRLS, reqCtls);
		m_ldapSvc.connect();
	}
    
	public Control[] getConnectControls() {
		return (Control[])m_ctxEnv.getProperty(ContextEnv.P_CONNECT_CTRLS);
	}
}
