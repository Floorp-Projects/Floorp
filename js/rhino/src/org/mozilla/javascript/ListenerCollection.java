/**
 * $Id: ListenerCollection.java,v 1.2 1999/05/19 23:58:37 norris%netscape.com Exp $
 *
 * This class provides a series of methods for accessing event listeners.
 *
 *      To get in touch with the Java Mozilla team, check out:
 *       news://news.mozilla.org/netscape.public.mozilla.java
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "MozPL"); you may not use this file except in
 * compliance with the MozPL.  You may obtain a copy of the MozPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the MozPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MozPL
 * for the specific language governing rights and limitations under the
 * MozPL.
 *
 * The Initial Developer of this code under the MozPL is Ian D. Stewart.
 * Portions created by Ian D. Stewart are Copyright (C) 1998, 1999 
 * Ian D. Stewart.
 * All Rights Reserved.
 *
 * Revision history:
 * $Log: ListenerCollection.java,v $
 * Revision 1.2  1999/05/19 23:58:37  norris%netscape.com
 * Remove Java 2 dependency.
 *
 * Revision 1.1  1999/05/18 22:32:25  norris%netscape.com
 * Add submission:
 * Subject:
 *             Re: Modified Context.java
 *        Date:
 *             Sat, 15 May 1999 08:01:37 +0000
 *       From:
 *             "Ian D. Stewart" <idstewart@softhome.net>
 *         To:
 *             Norris Boyd <norris@netscape.com>
 *  References:
 *             1 , 2 , 3 , 4 , 5
 *
 *
 *
 *
 * Ian D. Stewart wrote:
 *
 *   Norris Boyd wrote:
 *
 *
 *
 *
 *     Can I help with EventListener collector?
 *
 *   Actually, I have a working implementation complete (attatched), but by all means, feel free to add any functionality you feel
 *   may be missing, or to tweak the code .
 *
 * Norris,
 *
 * After I sent I my last e-mail, I noticed some potential issues using Object[] in ListenerCollection.getListeners(Class iface).
 * I'm attatching a new version, which uses a Vector object.  This should resolve those issues.
 *
 *
 * Ian
 *
 * 
 *    KNOWN BUGS/FRUSTRATIONS:
 *      - None, currently.
 */

package org.mozilla.javascript;

import java.util.Enumeration;
import java.util.Vector;

/**
 * This class acts as central storage location for miscelanious 
 * event listeners.  It provides methods for adding, removing
 * and accessing listeners, both individually and collectively,
 * by the listener interface implemented
 *
 * Note: This class performs the same functions as 
 * javax.swing.event.EventListenerList, and is provided
 * primarily for implementations lacking the Swing packages
 *
 * @author  Ian D. Stewart
 * @since JavaScript-Java 1.4 rel 3
 */
public class ListenerCollection extends Vector {
    /**
     * Create a new ListenerCollection
     */
    public ListenerCollection() {
        super();
    } // Constructor

    /**
     * Add a new listener to the collection
     * @param  listener  the listener
     */
    public void addListener(Object listener) {
        this.addElement(listener);
    }
    
    /**
     * Remove a listener from the collection
     * @param  listener  the listener
     */
    public void removeListener(Object listener) {
        this.removeElement(listener);
    }
    
    /**
     * Returns an Enumeration of all the listeners 
     * being stored in this collection
     * @return an Enumeration of all listeners
     */
    public Enumeration getAllListeners() {
        return this.elements();
    }
    
    /**
     * Return all the listeners in this collection which
     * implement the specified interface
     *
     * @param  iface  the interface
     * @return an array of listeners which implement the given
     *     interface
     */
    public Object[] getListeners(Class iface) {
        Vector array = new Vector();
                
        for(Enumeration enum = getAllListeners();enum.hasMoreElements();) {
            Object listener = enum.nextElement();
            if(iface.isInstance(listener)) {
                array.addElement(listener);
            }
        }
        Object[] result = new Object[array.size()];
        array.copyInto(result);
        return result;
    }
} // ListenerCollection

// end of ListenerCollection.java ...

