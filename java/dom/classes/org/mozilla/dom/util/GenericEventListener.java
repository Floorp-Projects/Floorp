/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 *
 * Contributor(s):
 *
 */
package org.mozilla.dom.util;

import java.io.PrintStream;
import org.w3c.dom.events.Event;
import org.w3c.dom.events.EventListener;

public class GenericEventListener implements EventListener {

    private String name;
    private PrintStream ps;

    public GenericEventListener() {
	this("GenericEventListener", 
	     new PrintStream(System.out));
    }

    public GenericEventListener(String name) {
	this(name, new PrintStream(System.out));
    }

    public GenericEventListener(PrintStream ps) {
	this("GenericEventListener", ps);
    }

    public GenericEventListener(String name, PrintStream ps) {
	this.name = name;
	this.ps = ps;	
    }

    /**
     * This method is called whenever an event occurs of the type for which the 
     * <code> EventListener</code> interface was registered. 
     * @param event The <code>Event</code> contains contextual information about 
     *   the event.  It also contains the <code>returnValue</code> and 
     *   <code>cancelBubble</code> properties which are used in determining 
     *   proper event flow.
     */
    public void handleEvent(Event event) {
	try {
	    if (ps != null) {
		ps.println(name + ": got event " + event);
	    }
	} catch (Exception e) {
	    if (ps != null) {
		ps.println(name + ": exception in handleEvent " + e);
	    }
	}
    }
}

