/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is ListenerCollection, released
 * May 15, 1998.
 *
 * The Initial Developer of the Original Code is Ian D. Stewart.
 * Portions created by Ian D. Stewart are Copyright (C) 1998, 1999
 * Ian D. Stewart.
 * Rights Reserved.
 *
 * Contributor(s):
 * Ian D. Stewart
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */


/* This class provides a series of methods for accessing event listeners. */

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

