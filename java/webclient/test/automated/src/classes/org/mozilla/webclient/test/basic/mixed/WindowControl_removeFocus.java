/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Client QA Team, St. Petersburg, Russia
 */


 
package org.mozilla.webclient.test.basic.mixed;

/*
 * WindowControl_removeFocus.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.awt.Rectangle;
import java.awt.Frame;
import java.awt.Container;
import java.awt.Component;
import java.awt.event.FocusListener;
import java.awt.event.FocusEvent;
import java.net.MalformedURLException;
import java.awt.*;
import java.awt.event.*;

public class WindowControl_removeFocus implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private WindowControl windowControl = null;

    private Component browserControlCanvas = null;
    private boolean removeFocusCalled = false;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();      
        try {
            browserControlCanvas = (Component)browserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
            browserControlCanvas.addFocusListener(new FocusListener() {
						public void focusGained(FocusEvent e) { System.out.println("Focus gained: "+removeFocusCalled+" : "+e); }
						public void focusLost(FocusEvent e) { System.out.println("Focus lost: "+removeFocusCalled+" : "+e); }
					});
            windowControl = (WindowControl)
                browserControl.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
	}
        context.setDescription("This tests invoke method \"removeFocus\"\n");

	Button b = new Button("RemoveFocus");
	b.addActionListener(new ActionListener() {
				public void actionPerformed(ActionEvent e) {
					try {
						if (windowControl != null)
							windowControl.removeFocus();
					} catch(Exception ex) {
               if(ex instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
		else TestContext.registerFAILED("Exception during execution: " + ex.toString());
	                			return;
					}
				}
			});
	context.addButton(b);
        return true;        
    }
    public void execute() {      
        if (null != windowControl) {
            context.addDescription("Press <RemoveFocus> button. \nIf \"Focus lost\" message appeared in the textarea test PASSED. ?");
            return;
        } 
        TestContext.registerFAILED("null WindowControl passed to test");
    }
   
}




