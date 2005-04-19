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

import java.awt.event.WindowListener;
import java.awt.event.MouseListener;

/**
 * <p>Allow the user to add and remove listeners on the current {@link
 * BrowserControl} instance.</p>
 */

public interface EventRegistration {
    
    // public void addContainerListener(ContainerListener containerListener); 
    // public void removeContainerListener(ContainerListener containerListener); 
    
    // public void addDocumentListener(DocumentListener documentListener); 
    // public void removeDocumentListener(DocumentListener documentListener); 
    
    /**
     * <p>Add the argument <code>listener</code> as a {@link
     * DocumentLoadListener} on the current {@link BrowserControl}.</p>
     *
     * @param listener the listener to install
     *
     * @throws NullPointerException if argument is <code>null</code>.
     */
    
    public void addDocumentLoadListener(DocumentLoadListener listener); 
    
    /**
     * <p>Remove the argument <code>listener</code> as a {@link
     * DocumentLoadListener} on the current {@link BrowserControl}.</p>
     *
     * @param listener the listener to remove
     *
     * @throws NullPointerException if argument is <code>null</code>.
     */
    
    public void removeDocumentLoadListener(DocumentLoadListener listener); 
    
    /**
     * <p>Add the argument <code>MouseListener</code> to the {@link
     * BrowserControlCanvas} for this {@link BrowserControl}.</p>
     *
     * @param listener the listener to install
     *
     * @deprecated This method has been replaced by leveraging the
     * method of the same name on {@link BrowserControlCanvas}.
     */
       
    public void addMouseListener(MouseListener listener);

    /**
     * <p>Remove the argument <code>MouseListener</code> from the {@link
     * BrowserControlCanvas} for this {@link BrowserControl}.</p>
     *
     * @param listener the listener to remove
     *
     * @deprecated This method has been replaced by leveraging the
     * method of the same name on {@link BrowserControlCanvas}.
     */
       
    public void removeMouseListener(MouseListener listener);
    
    // public void addDOMListener(DOMListener containerListener); 
    // public void removeDOMListener(DOMListener containerListener); 
    
    // public void addJavaScriptListener(JavaScriptListener containerListener); 
    // public void removeJavaScriptListener(JavaScriptListener containerListener); 
    
    // public void addWindowListener(WindowListener windowListener); 
    // public void removeWindowListener(WindowListener windowListener); 
    
    }
    
