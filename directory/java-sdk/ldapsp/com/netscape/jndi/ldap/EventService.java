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
import javax.naming.event.*;
import netscape.ldap.*;
import netscape.ldap.controls.*;
import java.util.*;
import com.netscape.jndi.ldap.common.*;

/**
 * Event Service. Use Persistant Search control to monitor changes on the
 * server. Because public ldapjdk APIs do not provide for multiplexing of
 * multiple requests over the same connection, a separate thread is created
 * for each monitored event
 *
 */
class EventService {

    LdapService m_ldapSvc;
    Vector m_eventThreads = new Vector();
    
    public EventService(LdapService ldapSvc) {
        m_ldapSvc = ldapSvc;
    }

    /**
     * Create a persistent search control.
     */
    private LDAPPersistSearchControl createSrchCtrl(NamingListener listener) throws NamingException{
        
        int op = 0;
        
        if (listener instanceof ObjectChangeListener) {
            op = LDAPPersistSearchControl.MODIFY;
        }
        if (listener instanceof NamespaceChangeListener) {
            op |= LDAPPersistSearchControl.ADD |
                     LDAPPersistSearchControl.DELETE |
                     LDAPPersistSearchControl.MODDN;
        }
        if (op == 0) {
            throw new NamingException("Non supported listener type " + listener.getClass().getName());
        }    
        return  new LDAPPersistSearchControl( op, /*changesOnly=*/true,
                          /*returnControls=*/true, /*isCritical=*/true );
    }

	/**
	 * Remove change event listener
	 */
	 synchronized void removeListener(NamingListener listener)  throws NamingException {
	    boolean removed = false;
	    int i=0;
	    while(i < m_eventThreads.size()) {
	        EventThread et = (EventThread)m_eventThreads.elementAt(i);
	        if (et.removeListener(listener)) {
	            removed = true;
	            
	            // If no listeners left,  stop the thread.
	            if (et.isEmpty()) {
	                et.abandonRequest();
	                et.stop();
	                if (m_eventThreads.removeElement(et)) {
	                    continue; // do not advance counter i
	                }    
	            }
	        }
	        i++;
	    }
	  
	    if (!removed) {
	        throw new NamingException("Listener not found");
	    }
	 }    
    
	/**
	 * Add change event listener
	 */
	synchronized void addListener (LdapContextImpl ctx, String name, String filter, SearchControls jndiCtrls, NamingListener l) throws NamingException{
	    Debug.println(1, "ADD LISTENER");

		// Create DN by appending the name to the current context
		String base = ctx.getDN();
		if (name.length() > 0) {
			if (base.length() > 0) {
				base = name + "," + base;
			}
			else {
				base = name;
			}
		}
				
        //Create search constraints
		LDAPConnection ld = (LDAPConnection) m_ldapSvc.getConnection().clone();
		LDAPSearchConstraints cons=ld.getSearchConstraints();
		LDAPPersistSearchControl psearchCtrl = createSrchCtrl(l);
        cons.setServerControls(psearchCtrl);

        // return obj flag is ignored in this implementation
        boolean returnObjs = jndiCtrls.getReturningObjFlag();
        
		// Because we are not returning objects  in the NamingEvent's
		// oldBinding and newBinding variables, there is no way for
		// for the listener to read attributes from the event. Thus 
		// jndiCtrls.getReturningAttributes() are ignored. Request only
		// javaClassName to be able to determine object type
		String[] attrs = new String[] { "javaclassname" }; 
			
		// Search scope
		int scope = ProviderUtils.jndiSearchScopeToLdap(jndiCtrls.getSearchScope());

		// Check if such change is already monitored
		EventThread et = null;
		for (int i=0; i < m_eventThreads.size(); i++) {
		    EventThread activeET = (EventThread) m_eventThreads.elementAt(i);
		    if (activeET.isEqualEvent(ctx, base, scope, filter, attrs, cons)) {
		        et = activeET;
		        et.addListener(l);
		        break;
		    }
		}
		
		// Create new event if reqested change is not already monitored
		if (et == null) {
		    et = new EventThread(ctx, ld, base, scope, filter, attrs, cons);
		    m_eventThreads.addElement(et);
		    et.addListener(l);
		    et.setDaemon(true);
		    et.start();
		}    
	}
	
	
	/**
	 * Change Monitoring Thread
	 */
	static private class EventThread extends Thread {
	    
	    String base, filter, attrs[];
	    int scope;
	    LDAPSearchConstraints cons;
	    LDAPConnection ld;
	    EventContext src;
	    LDAPSearchResults res = null;
	    LdapContextImpl ctx;
	    boolean doRun;
	    Vector listeners = new Vector(); // vector of NamingListener
	    
	    EventThread(LdapContextImpl ctx, LDAPConnection ld, String base, int scope, String filter,
	                  String[] attrs, LDAPSearchConstraints cons) {
        
            this.ctx = ctx;
            this.ld = ld;
            this.base = base;
            this.scope = scope;
            this.filter = filter;
            this.attrs = attrs;
            this.cons = cons;
        }
        
        /**
         * Add Listsner
         */
         void addListener(NamingListener l) {
            synchronized(listeners) {
                listeners.addElement(l);
            }
         }

        /**
         * Remove listener
         */
        boolean removeListener(NamingListener l) {
            synchronized(listeners) {
                return listeners.removeElement(l);
            }
        }    

        /**
         * Abandon request. Called when no listeners left.
         * Unfortunatlly, ldapjdk does not allows us the interrupt a waiting
         * thread and do proper cleanup.
         * TODO Need improvment here!!
         */
         void abandonRequest() throws NamingException {
            try {
                if (res != null) {
                    ld.abandon(res);
                }
                ld.disconnect();
            }
            catch (Exception ex) {
                throw ExceptionMapper.getNamingException(ex);
            }
         }

        /**
         * Chech whether there are any listeners
         */
        boolean isEmpty() {
            return listeners.size() == 0;
         }    
         
        /**
         * Check whether this event paramaters are matched
         */
	    boolean isEqualEvent(LdapContextImpl ctx, String base, int scope, String filter,
	                  String[] attrs, LDAPSearchConstraints cons) {
        
            if (this.ctx   != ctx   || !this.base.equals(base) ||
                this.scope != scope || !this.filter.equals(filter)) {
                    
                return false;
            }
            // attrs[]
            if (this.attrs == null) {
                if (attrs != null) {
                    return false;
                }
            }
            else if (attrs == null) {
                return false;
            }
            else if (this.attrs.length != attrs.length) {
                return false;
            }
            else {
                for (int i=0; i < this.attrs.length; i++) {
                    // TODO not 100% ok. Arrays may contain the same names
                    // but at different positions
                    if (!this.attrs[i].equals(attrs[i])) {
                        return false;
                    }
                }
            }    

            // Check if persistant search is for the same change type
            LDAPPersistSearchControl
                psearch1 = (LDAPPersistSearchControl)this.cons.getServerControls()[0],
                psearch2 = (LDAPPersistSearchControl)cons.getServerControls()[0];
            int types1 = psearch1.getChangeTypes(),
                types2 = psearch2.getChangeTypes();                
            return (types1 == types2);
        }

        /**
         * Issue persistent search request. Loop waiting for change notifications
         */
        public void run() {
            doRun = true;
            try {
                Debug.println(1, "Do persistent search for " + base);
                res = ld.search( base, scope, filter, attrs, false, cons);

                /* Loop through the results until finished. */
                while ( res.hasMoreElements() && doRun) {

                    /* Get the next modified directory entry. */
                    LDAPEntry modEntry = res.next();
                    
                    Debug.println(1, "Changed " + modEntry.getDN());

                    /* Get any entry change controls. */
                    LDAPControl[] ctrls = ld.getResponseControls();

                    // Can not create event without change control
                    if (ctrls == null) {
                        throw new NamingException(
                        "Can not create NamingEvent, no change control info");
                    }

                    LDAPEntryChangeControl changeCtrl =
                    LDAPPersistSearchControl.parseResponse(ctrls);
                    
                    // Can not create event without change control
                    if (changeCtrl == null || changeCtrl.getChangeType() == -1) {
                        throw new NamingException(
                        "Can not create NamingEvent, no change control info");
                    }       

                    NamingEvent event = createNamingEvent(modEntry, changeCtrl);
                    dispatchEvent(event);
                }
            }
            catch(Exception ex) {
                NamingException nameEx = ExceptionMapper.getNamingException(ex);
                dispatchEvent(new NamingExceptionEvent(ctx, nameEx));
            }
        }

        /**
         * Dispatch naming event to all listeners
         */
        void dispatchEvent(EventObject event) {
            NamingListener[] dispatchList = null;
            
            // Copy listeners so that list can be modifed during dispatching
            synchronized (listeners) {
                dispatchList = new NamingListener[listeners.size()];
                for (int i=0; i < dispatchList.length; i++) {
                    dispatchList[i] = (NamingListener)listeners.elementAt(i);
                }
            }
            
            // dispatch to all listeners
            for (int i=0; i < dispatchList.length; i++) {
                if (event instanceof NamingEvent) {
                    ((NamingEvent)event).dispatch(dispatchList[i]);
                }
                else {
                    ((NamingExceptionEvent)event).dispatch(dispatchList[i]);
                }    
            }
        }

        /**
         * Create naming event from the ldap entry and change control
         */
        NamingEvent createNamingEvent(LDAPEntry entry, LDAPEntryChangeControl changeCtrl) throws NamingException{

            Binding oldBd = null, newBd = null;            
            int eventType = -1;
            Object changeInfo = null;
            String oldName = null, newName = null;
            
            // Get the class name from the entry
            String className = ObjectMapper.getClassName(entry);

            // Get information on the type of change made
            int changeType = changeCtrl.getChangeType();
            switch ( changeType ) {
                case LDAPPersistSearchControl.ADD:
                    eventType = NamingEvent.OBJECT_ADDED;
                    newName = LdapNameParser.getRelativeName(ctx.m_ctxDN, entry.getDN());
                    break;
                case LDAPPersistSearchControl.DELETE:
                    eventType = NamingEvent.OBJECT_REMOVED;
                    oldName = LdapNameParser.getRelativeName(ctx.m_ctxDN, entry.getDN());
                    break;
                case LDAPPersistSearchControl.MODIFY:
                    eventType = NamingEvent.OBJECT_CHANGED;
                    oldName = newName = LdapNameParser.getRelativeName(ctx.m_ctxDN, entry.getDN());
                    break;
                case LDAPPersistSearchControl.MODDN:
                    eventType = NamingEvent.OBJECT_RENAMED;
                    // Get the previous DN of the entry
                    String oldDN = changeCtrl.getPreviousDN();
                    if ( oldDN != null ) {
                        oldName = LdapNameParser.getRelativeName(ctx.m_ctxDN, oldDN);
                    }
                    // newName might be outside the context for which the listener has registred
                    try {
                        newName = LdapNameParser.getRelativeName(ctx.m_ctxDN, entry.getDN());
                    }
                    catch (NamingException ex) {}
                    break;
            }

            // Pass the change log number as event's change info
            // If the change log number is not present the value is -1
            changeInfo = new Integer(changeCtrl.getChangeNumber());


            if (oldName != null) {
                oldBd = new Binding(oldName, className, /*obj=*/null, /*isRelative=*/true);
            }    
            if (newName!= null) {
                newBd = new Binding(newName, className, /*obj=*/null, /*isRelative=*/true);
            }    
            return new NamingEvent(ctx, eventType, newBd, oldBd, changeInfo);
        }
    }    
}    
