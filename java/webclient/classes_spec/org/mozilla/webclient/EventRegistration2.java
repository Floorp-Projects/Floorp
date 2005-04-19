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
 * Contributor(s):  Kyle Yuan <kyle.yuan@sun.com>
 */

package org.mozilla.webclient;

import java.awt.event.KeyListener;

/**
 * <p>Extended event registration features.</p>
 */

public interface EventRegistration2  extends EventRegistration {

    /**
     * <p>Set the argument <code>listener</code> as the one and only
     * {@link NewWindowListener} for the current {@link
     * BrowserControl}.</p>
     *
     * @param listener the listener to install
     */
    
    public void setNewWindowListener(NewWindowListener listener); 
    
    /**
     * <p>Add the argument listener as a {@link NewWindowListener} to
     * the current {@link BrowserControl}.</p>
     *
     * @param listener the listener to install
     *
     * @deprecated Use {@link #setNewWindowListener} instead.
     * Implementations must implement this method as a call to {@link
     * #setNewWindowListener} passing null, followed by a call to {@link
     * #setNewWindowListener} passing the argument listener.
     */
    public void addNewWindowListener(NewWindowListener listener);
    
    /**
     * <p>Remove the argument listener from the current {@link
     * BrowserControl}.</p>
     *
     * @param listener the listener to remove
     *
     * @deprecated Use {@link #setNewWindowListener} passing <code>null</code>.
     */
    public void removeNewWindowListener(NewWindowListener listener);

    /**
     * <p>Add the argument <code>listener</code> as a
     * <code>KeyListener</code> for the current {@link
     * BrowserControl}.</p>
     *
     * @param listener the listener to install
     */ 
    
    public void addKeyListener(KeyListener listener); 

    /**
     * <p>Remove the argument <code>listener</code> from the current
     * {@link BrowserControl}.</p>
     *
     * @param listener the listener to remove
     */ 
    
    public void removeKeyListener(KeyListener listener); 
    
}
