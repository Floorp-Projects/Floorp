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
import java.util.*;
import com.netscape.jndi.ldap.common.*;

/**
 * Event Service monitors changes on the server
 * Implemented with the persistant search control. Uses ldapjdk asynchronous
 * interfaces so that multiple search requests can be processed by a single
 * thread
 *
 */
class EventService implements Runnable{

    LdapService m_ldapSvc;
    Vector m_eventList = new Vector();
    Thread m_monitorThread;
    LDAPSearchListener m_msgQueue; // for asynch ldap search
    
    /**
     * Constructor
     */
    public EventService (LdapService ldapSvc) {
        m_ldapSvc = ldapSvc;
    }

	/**
	 * Add change event listener
	 */
	synchronized void addListener (LdapContextImpl ctx, String name,
                      String filter,SearchControls jndiCtrls, NamingListener l)
                      throws NamingException{
        
        EventEntry event = null;
        LDAPSearchListener sl = null; // Search listener for this request

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
        
		// Attributes in jndiCtrls.getReturningAttributes() are ignored 
        // This is because we are not returning objects in the NamingEvent
        // and thus listeners can not read attributes from the event.
        // Request only javaClassName to be able to determine object type
		String[] attrs = new String[] { "javaclassname" }; 
			
		// Search scope
		int scope = ProviderUtils.jndiSearchScopeToLdap(jndiCtrls.getSearchScope());

		// Check if such change is already monitored, search for the event entry
		for (int i=0; i < m_eventList.size(); i++) {
		    EventEntry ee = (EventEntry) m_eventList.elementAt(i);
		    if (ee.isEqualEvent(base, scope, filter, attrs, cons)) {
		        event = ee;
		        break;
		    }
		}
        
        // If event entry does not exist, send an asynch persistent search
        // request and create a new event entry
        if (event == null) {
            try {                    
                Debug.println(1, "Do persistent search for " + base);
                sl = ld.search( base, scope, filter, attrs,
                                false, /*l=*/null, cons);
                int[] ids = sl.getIDs();
                int id = ids[ids.length-1];
                event = new EventEntry(id, ctx, base, scope, filter, attrs, cons);
                m_eventList.addElement(event);
            }
            catch(Exception ex) {
                throw ExceptionMapper.getNamingException(ex);
            }
        }

        // regiter naming listener with the event
        event.addListener(l);

        // Add this search request to the m_msgQueue so it can be
        // processed by the monitor thread
        if (m_msgQueue == null) {
            m_msgQueue = sl;
        }
        else {
            m_msgQueue.merge(sl);
        }
        
		// Create new event if reqested change is not already monitored
		if (m_monitorThread == null) {
		    m_monitorThread = new Thread (this, "EventService");
		    m_monitorThread.setDaemon(true);
		    m_monitorThread.start();
		}    
	}
	
	/**
	 * Remove change event listener
	 */
	 synchronized void removeListener(NamingListener listener)throws NamingException {
	    boolean removed = false;

        // Check and the listener against all event entries. If an event is
        // left with no listeners, abandon associated ldap request
        for(int i = m_eventList.size()-1; i>=0; i--) {
	        EventEntry ee = (EventEntry)m_eventList.elementAt(i);
	        if (ee.removeListener(listener)) {
	            removed = true;

                // If no listeners left abandon persistant search and
                // delete entry
	            if (ee.isEmpty()) {
                    abandonRequest(ee.id);
	                m_eventList.removeElement(ee);
	            }
	        }
        }
        
        // Stop the monitor thread if no events are left
        // Actually, the thread should stop by itself, as when no outstanding
        // events are left LDAPSearchListener.getResponse() should return null
        if (m_eventList.size() == 0) {
            m_monitorThread = null;
        }            
	  
	    if (!removed) {
	        throw new NamingException("Listener not found");
	    }
	 }    

    /**
     * Abandon LDAP request with the specified message ID
     */
     private void abandonRequest(int id) {
        LDAPConnection ldc = m_ldapSvc.getConnection();
        try {
	        ldc.abandon(id);
        }
        catch (LDAPException ex) {}
     }
	     
     
    /**
     * Main monitor thread loop. Wait for persistent search change notifications
     */
    public void run() {
        
        LDAPMessage msg = null;

        
        while (m_monitorThread != null) {

            try {
                
                // Block untill a message is received
                msg = m_msgQueue.getResponse();
                
            }
            catch (LDAPException ex) {
                processNetworkError(ex);
            }

            // Terminate if no more requests left
            if (msg == null) {
                Debug.println(1, "No more messages, bye");
                m_monitorThread = null;
                return;
            }                        
          
            synchronized (EventService.this) {
                
                EventEntry eventEntry = getEventEntry(msg.getId());                

                // If no listeners, abandon this message id
                if (eventEntry == null) {
                    Debug.println(1, "Received ldap msg with unknown id="+msg.getId());

                    if (! (msg instanceof LDAPResponse)) {
                        abandonRequest(msg.getId());
                    }                    
                    continue;
                }
                
                // Check for error message ...
                if (msg instanceof LDAPResponse) {
			        processResponseMsg((LDAPResponse) msg, eventEntry);
                }
                    
                // ... or referral ...
                else if (msg instanceof LDAPSearchResultReference) {
                    processSearchResultRef((LDAPSearchResultReference) msg, eventEntry);
                }                   

            
                // ... then must be a LDAPSearchResult carrying change control
                else if (msg instanceof LDAPSearchResult) {
                    processSearchResultMsg((LDAPSearchResult) msg, eventEntry);
                }
            }
        } // end of synchronized block            
    }
    
    /**
     * On network error, create NamingExceptionEvent and delever it to all
     * listeners on all events.
     */
    private void processNetworkError(LDAPException ex) {
        NamingException nameEx = ExceptionMapper.getNamingException(ex);
        for(int i=0; i<m_eventList.size(); i++) {
            EventEntry ee = (EventEntry)m_eventList.elementAt(i);
            dispatchEvent(new NamingExceptionEvent(ee.ctx, nameEx), ee);
        }
    }                
    
    /**
     * Response message carries a LDAP error. Response with the code 0 (SUCCESS),
     * should never be received as persistent search never completes, it has to
     * be abandon. Referral messages are ignored
     */
    private void processResponseMsg(LDAPResponse rsp, EventEntry ee) {
        if (rsp.getResultCode() == 0) {
            return;  // this should never happen, but  just in case
        }
        else if (rsp.getResultCode() == LDAPException.REFERRAL) {
            return; // ignore referral
        }
        
        LDAPException ex = new LDAPException( "error result",rsp.getResultCode(),
                           rsp.getErrorMessage(), rsp.getMatchedDN());
        NamingException nameEx = ExceptionMapper.getNamingException(ex);
        dispatchEvent(new NamingExceptionEvent(ee.ctx, nameEx), ee);
    }        
    
    /**
     * Process change notification attached as the change control to the message
     */
    private void processSearchResultMsg(LDAPSearchResult res, EventEntry ee) {
        LDAPEntry modEntry = res.getEntry();
	    	   
        Debug.println(1, "Changed " + modEntry.getDN());

        /* Get any entry change controls. */
        LDAPControl[] ctrls = res.getControls();

        // Can not create event without change control
        if (ctrls == null) {
            NamingException ex = new NamingException(
            "Can not create NamingEvent, no change control info");
            dispatchEvent(new NamingExceptionEvent(ee.ctx, ex), ee);
        }

        // Multiple controls might be in the message
        for (int i=0; i < ctrls.length; i++) {
            LDAPEntryChangeControl changeCtrl = null;

            if (ctrls[i] instanceof LDAPEntryChangeControl) {
                changeCtrl = (LDAPEntryChangeControl) ctrls[i];

                // Can not create event without change control
                if (changeCtrl.getChangeType() == -1) {
                    NamingException ex = new NamingException(
                    "Can not create NamingEvent, no change control info");
                    dispatchEvent(new NamingExceptionEvent(ee.ctx, ex), ee);
                }
      
                // Convert control into a NamingEvent and dispatch to listeners
                try {
                    NamingEvent event = createNamingEvent(ee.ctx, modEntry, changeCtrl);
                    dispatchEvent(event, ee);
                }
                catch (NamingException ex) {
                    dispatchEvent(new NamingExceptionEvent(ee.ctx, ex), ee);
                }
            }
        }                       
    }
    
    /**
     * Search continuation messages are ignored.
     */
    private void processSearchResultRef(LDAPSearchResultReference ref, EventEntry ee) {
        ; // Do nothing, message ignored, do not dispatch NamingExceptionEvent
    }

    /**
     * Find event entry by message ID
     */
    private EventEntry getEventEntry(int id) {
	    for (int i=0; i < m_eventList.size(); i++) {
		    EventEntry ee = (EventEntry) m_eventList.elementAt(i);
            if (ee.id == id) {
		        return ee;
		    }
		}            
        return null;
    }
    
    /**
     * Dispatch naming event to all listeners
     */
    private void dispatchEvent(EventObject event, EventEntry eventEntry) {
        NamingListener[] dispatchList = null;

        // Copy listeners so that list can be modifed during dispatching
        synchronized (eventEntry) {
            dispatchList = new NamingListener[eventEntry.listeners.size()];
            for (int i=0; i < dispatchList.length; i++) {
                dispatchList[i] = (NamingListener)eventEntry.listeners.elementAt(i);
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
     * Create naming event from a change control
     */
    private NamingEvent createNamingEvent(LdapContextImpl ctx, LDAPEntry entry,
                        LDAPEntryChangeControl changeCtrl)throws NamingException{

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

    /**
     * Create a persistent search control.
     */
    private LDAPPersistSearchControl createSrchCtrl(NamingListener listener)
                                     throws NamingException{
        
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
            throw new NamingException("Non supported listener type " +
                listener.getClass().getName());
        }    

        return  new LDAPPersistSearchControl( op, /*changesOnly=*/true,
                          /*returnControls=*/true, /*isCritical=*/true );
    }


    /**
     * Inner class that represents a binding between a change event,
     * described with a set of search parameters, and a list of listeners
     */
    static private class EventEntry {
	    
	    LdapContextImpl ctx;
        String base, filter, attrs[];
	    int scope;
	    LDAPSearchConstraints cons;
        int id; // ldap message id
        Vector listeners   = new Vector(); // vector of NamingListener
	    
        /**
         * Constructor
         */
	    EventEntry(int id, LdapContextImpl ctx, String base, int scope,
                   String filter, String[] attrs, LDAPSearchConstraints cons) {

            this.id = id;
            this.ctx = ctx;
            this.base = base;
            this.scope = scope;
            this.filter = filter;
            this.attrs = attrs;
            this.cons = cons;
        }
        
        /**
         * Add Listsner
         */
        synchronized void addListener(NamingListener l) {
            listeners.addElement(l);
         }

        /**
         * Remove listener
         */
        synchronized boolean removeListener(NamingListener l) {
            return listeners.removeElement(l);
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
	    boolean isEqualEvent(String base, int scope, String filter,
	                  String[] attrs, LDAPSearchConstraints cons) {
        
            if (!this.base.equals(base) || this.scope != scope ||
                !this.filter.equals(filter)) {
                    
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
                // Attr sets may be the same but in diffferent order
                for (int i=0; i < this.attrs.length; i++) {
                    boolean found = false;
                    for (int j=0; j < this.attrs.length; j++) {
                        if (this.attrs[i].equals(attrs[j])) {
                            found = true;
                            break;
                        }                            
                    }
                    if (!found) {
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
        
        public String toString() {
            LDAPPersistSearchControl
                psearch = (LDAPPersistSearchControl)cons.getServerControls()[0];

            String str = "[EventEntry] base=" + base + " scope=" + scope +
                         " filter=" + filter + " attrs={";
            for (int i=0; i < attrs.length; i++) {
                if (i>0) {
                    str += " ";
                }
                str += attrs[i];
            }
            str += "} chanageTypes=" + psearch.getChangeTypes();
            str += " listeners=" + listeners.size();
            str += " id=" + id;
            return str;
        }
    }
}    
