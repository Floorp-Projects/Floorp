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

/**

 * <P> This java.awt.event.MouseEvent subclass allows the user to access the
 * WebclientEvent.  This eventData of this WebclientEvent, if non-null,
 * is a java.util.Properties instance that contains information about
 * this event.  </P>

 * <P>The following are some interesting keys:</P>

 * <UL>

 * <LI> href
 * </LI>

 * <LI> #text
 * </LI>

 * </UL>

 */

public class WCMouseEvent extends java.awt.event.MouseEvent
{

public static final long MOUSE_DOWN_EVENT_MASK = 1;
public static final long MOUSE_UP_EVENT_MASK = 1 << 2;
public static final long MOUSE_CLICK_EVENT_MASK = 1 << 3;
public static final long MOUSE_DOUBLE_CLICK_EVENT_MASK = 1 << 4;
public static final long MOUSE_OVER_EVENT_MASK = 1 << 5;
public static final long MOUSE_OUT_EVENT_MASK = 1 << 6;

//
// Constructors
//

private WebclientEvent webclientEvent;

public WCMouseEvent(Component source, int id, long when, int modifiers, int x, 
                  int y, int clickCount, boolean popupTrigger, 
                  WebclientEvent wcEvent)
{
    // PENDING(edburns): pull out the clickcount and such from the
    // Properties object, pass this data on to the constructor.
    super(source, id, when, modifiers, x, y, clickCount, popupTrigger);
    webclientEvent = wcEvent;
}

public WebclientEvent getWebclientEvent()
{
    return webclientEvent;
}

} // end of class WCMouseEvent
