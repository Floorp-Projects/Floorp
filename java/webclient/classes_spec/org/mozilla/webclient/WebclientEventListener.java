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

/**
 * <p>The base interface for many, but not all, events coming from the
 * browser.  For example, it is possible to add a
 * <code>java.awt.event.MouseListener</code> or
 * <code>java.awt.event.KeyListener</code> to the {@link
 * BrowserControlCanvas}.  Doing so will cause events specific to those
 * interfaces to be generated.  See the sub-interface javadoc for more
 * details.</p>
 */

public interface WebclientEventListener {
    
    /**
     * <p>This method will be called by the browser when a particular
     * event occurs.  The nature of the argument <code>event</code>
     * depends on the particular <code>WebclientEventListener</code>
     * sub-interface that is implemented.  See the sub-interface javadoc
     * for more details.</p>
     *
     * @param event the event object for this event
     */
    
    public void eventDispatched(WebclientEvent event);
    
} // end of interface WebclientEventListener
