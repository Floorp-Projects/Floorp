/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

import java.awt.Component;
import java.awt.event.KeyEvent;

/**
 *
 * @author edburns
 */
public class WCKeyEvent extends KeyEvent {
    
    private WebclientEvent wcEvent = null;
    
    /** Creates a new instance of WCKeyEvent */
    public WCKeyEvent(Component source, int id, long when, int modifiers,
            int keyCode, char keyChar, WebclientEvent wcEvent) {
        super(source, id, when, modifiers, keyCode, keyChar);
        this.wcEvent = wcEvent;
    }
    
    public WebclientEvent getWebclientEvent() {
        return wcEvent;
    }
    
    
}
