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

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.awt.event.MouseListener;
import java.awt.event.MouseEvent;
import java.awt.event.InputEvent;
import java.awt.Component;
import org.mozilla.webclient.WCMouseEvent;
import org.mozilla.webclient.WebclientEvent;
import org.mozilla.webclient.WebclientEventListener;

import java.util.Properties;

/**

 * This class wraps the user provided instance of
 * java.awt.event.MouseListener so it can be installed as a
 * WebclientEventListener.  Note that we implement MouseListener so we
 * can be detected by the webclient event system.  We don't do anything
 * with these methods here, though.

 */

public class WCMouseListenerImpl extends Object implements WebclientEventListener, MouseListener
{
//
// Protected Constants
//

//
// Class Variables
//

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

/**

 * The MouseListener for which I proxy.

 */ 

protected MouseListener mouseListener;

//
// Constructors and Initializers    
//

WCMouseListenerImpl(MouseListener yourListener)
{
    super();
    // ParameterCheck.nonNull();
    mouseListener = yourListener;
}

//
// Class methods
//

//
// General Methods
//

MouseListener getMouseListener()
{
    return mouseListener;
}

//
// Methods from WebclientEventListener
// 

/**

 * Here's where we take the WebclientEvent from mozilla, generate an
 * appropriate WCMouseEvent instance, and call the
 * appropriate listener event.

 */

public void eventDispatched(WebclientEvent event)
{
    ParameterCheck.nonNull(event);
    WCMouseEvent mouseEvent;
    Properties props = (Properties) event.getEventData();
    int modifiers = 0, x = -1, y = -1, clickCount = 0;
    String str;
    boolean bool;
    if (null != props) { 
    if (null != (str = props.getProperty("ClientX"))) {
        x = Integer.valueOf(str).intValue();
    }
    if (null != (str = props.getProperty("ClientY"))) {
        y = Integer.valueOf(str).intValue();
    }
    if (null != (str = props.getProperty("ClickCount"))) {
        clickCount = Integer.valueOf(str).intValue();
    }
    if (null != (str = props.getProperty("Button"))) {
        int button = Integer.valueOf(str).intValue();
        if (1 == button) {
            modifiers += InputEvent.BUTTON1_MASK;
        }
        if (2 == button) {
            modifiers += InputEvent.BUTTON2_MASK;
        }
        if (3 == button) {
            modifiers += InputEvent.BUTTON3_MASK;
        }
    }
    if (null != (str = props.getProperty("Alt"))) {
        bool = Boolean.valueOf(str).booleanValue();
        if (bool) {
            modifiers += InputEvent.ALT_MASK;
        }
    }
    if (null != (str = props.getProperty("Ctrl"))) {
        bool = Boolean.valueOf(str).booleanValue();
        if (bool) {
            modifiers += InputEvent.CTRL_MASK;
        }
    }
    if (null != (str = props.getProperty("Meta"))) {
        bool = Boolean.valueOf(str).booleanValue();
        if (bool) {
            modifiers += InputEvent.META_MASK;
        }
    }
    if (null != (str = props.getProperty("Shift"))) {
        bool = Boolean.valueOf(str).booleanValue();
        if (bool) {
            modifiers += InputEvent.SHIFT_MASK;
        }
    }
    }
    switch ((int) event.getType()) {
    case (int) WCMouseEvent.MOUSE_DOWN_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) event.getSource(),
                             MouseEvent.MOUSE_PRESSED, -1,
                             modifiers, x, y, clickCount, false, event);
        mouseListener.mousePressed(mouseEvent);
        break;
    case (int) WCMouseEvent.MOUSE_UP_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) event.getSource(),
                             MouseEvent.MOUSE_RELEASED, -1,
                             modifiers, x, y, clickCount, false, event);
        mouseListener.mouseReleased(mouseEvent);
        break;
    case (int) WCMouseEvent.MOUSE_CLICK_EVENT_MASK:
    case (int) WCMouseEvent.MOUSE_DOUBLE_CLICK_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) event.getSource(),
                             MouseEvent.MOUSE_CLICKED, -1,
                             modifiers, x, y, clickCount, false, event);
        mouseListener.mouseClicked(mouseEvent);
        break;
    case (int) WCMouseEvent.MOUSE_OVER_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) event.getSource(),
                             MouseEvent.MOUSE_ENTERED, -1,
                             modifiers, x, y, clickCount, false, event);
        mouseListener.mouseEntered(mouseEvent);
        break;
    case (int) WCMouseEvent.MOUSE_OUT_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) event.getSource(),
                             MouseEvent.MOUSE_EXITED, -1,
                             modifiers, x, y, clickCount, false, event);
        mouseListener.mouseExited(mouseEvent);
        break;
    }
}

//
// From MouseListener
// 

public void mouseClicked(MouseEvent e)
{
    Assert.assert_it(false, "This method should not be called.");
}

public void mouseEntered(MouseEvent e)
{
    Assert.assert_it(false, "This method should not be called.");
}

public void mouseExited(MouseEvent e)
{
    Assert.assert_it(false, "This method should not be called.");
}

public void mousePressed(MouseEvent e)
{
    Assert.assert_it(false, "This method should not be called.");
}

public void mouseReleased(MouseEvent e)
{
    Assert.assert_it(false, "This method should not be called.");
}

} // end of class WCMouseListenerImpl
