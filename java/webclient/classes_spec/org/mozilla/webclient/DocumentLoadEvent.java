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
 * <p>Event class for the {@link DocumentLoadListener} interface.</p>
 *
 */

public class DocumentLoadEvent extends WebclientEvent {

    /**
     * <p>The <code>type</code> of the event to indicate the start of a
     * document load.</p>
     */ 

    public static final long START_DOCUMENT_LOAD_EVENT_MASK = 1;

    /**
     * <p>The <code>type</code> of the event to indicate the end of a
     * document load.</p>
     */ 

    public static final long END_DOCUMENT_LOAD_EVENT_MASK = 1 << 2;

    /**
     * <p>If the current document is a compound document, this event
     * will be generated, indicating the start of a URL load within a
     * document load for that compound document.</p>
     */ 
    
    public static final long START_URL_LOAD_EVENT_MASK = 1 << 3;

    /**
     * <p>If the current document is a compound document, this event
     * will be generated, indicating the end of a URL load within a
     * document load for that compound document.</p>
     */ 
    
    public static final long END_URL_LOAD_EVENT_MASK = 1 << 4;

    /**
     * <p>This event indicates a progress message from the browser.</p>
     */

    public static final long PROGRESS_URL_LOAD_EVENT_MASK = 1 << 5;

    /**
     * <p>This event indicates a status message from the browser.  For
     * example, "completed."</p>
     */
    public static final long STATUS_URL_LOAD_EVENT_MASK = 1 << 6;

    /**
     * <p>This event indicates the browser encountered an unknown
     * content that it does not know how to display.</p>
     */

    public static final long UNKNOWN_CONTENT_EVENT_MASK = 1 << 7;

    /**
     * <p>This event indicates the current fetch was interrupted.</p>
     */
    
    public static final long FETCH_INTERRUPT_EVENT_MASK = 1 << 8;

    /**
     * <p>This event indicates the current fetch is the start of an Ajax 
     * transaction.</p>
     */
    
    public static final long START_AJAX_EVENT_MASK = 1 << 9;

    /**
     * <p>This event indicates the current fetch is an Ajax 
     * transaction in an error state.</p>
     */
    
    public static final long ERROR_AJAX_EVENT_MASK = 1 << 10;
    
    /**
     * <p>This event indicates the current fetch is an Ajax 
     * transaction in that has completed.</p>
     */
    
    public static final long END_AJAX_EVENT_MASK = 1 << 11;

//
// Constructors
//
    /**
     * <p>Create a new <code>DocumentLoadListener</code> instance.</p>
     *
     * @param source the source of the event, passed to
     * <code>super</code>.  This will be the {@link EventRegistration}
     * instance for this {@link BrowserControl}.
     *
     * @param newType the eventType.  Depends on the {@link
     * WebclientEventListener} sub-interface.
     *
     * @param newEventData the eventType.  Depends on the {@link
     * WebclientEventListener} sub-interface.
     */

    public DocumentLoadEvent(Object source, long newType, 
			     Object newEventData) {
	super(source, newType, newEventData);
    }
    
} // end of class DocumentLoadEvent
