/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient;

import java.util.EventObject;

/**
 * <p>Base event class for browser specific events coming from the
 * browser.</p>
 */

public class WebclientEvent extends EventObject {

    //
    // Attribute ivars
    //
    
    private long type;
    
    private Object eventData;
    
    //
    // Constructors
    //

    /**
     * <p>Construct a new instance with the given <code>source</code>,
     * <code>type</code>, and <code>eventData</code>.  This method is
     * typically not called by user code.</p>
     *
     * @param source the source of the event, passed to
     * <code>super</code>.  Depends on the {@link
     * WebclientEventListener} sub-interface.
     *
     * @param newType the eventType.  Depends on the {@link
     * WebclientEventListener} sub-interface.
     *
     * @param newEventData the eventType.  Depends on the {@link
     * WebclientEventListener} sub-interface.
     */
    
    public WebclientEvent(Object source, long newType, 
			  Object newEventData) {
	super(source);
	type = newType;
	eventData = newEventData;
    }

    /**
     * <p>Return the type of this event.  Depends on the {@link
     * WebclientEventListener} sub-interface. </p>
     */
    
    public long getType() {
	return type;
    }
    
    /**
     * <p>Return the event data for this event.  Depends on the {@link
     * WebclientEventListener} sub-interface. </p>
     */

    public Object getEventData() {
	return eventData;
    }
    
} // end of class WebclientLoadEvent
