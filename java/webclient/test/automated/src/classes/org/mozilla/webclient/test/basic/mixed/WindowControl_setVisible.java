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
 * WindowControl_setVisible.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.awt.Rectangle;
import java.awt.Frame;
import java.awt.Container;
import java.awt.Component;
import java.net.MalformedURLException;

public class WindowControl_setVisible implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private WindowControl windowControl = null;
    private String[] pages = null;
    private boolean currentArg;
    
    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        String arguments = context.getCurrentTestArguments().trim();
        if (arguments.equals("true")) {
            this.currentArg = true;
        } else {
            this.currentArg = false;
        }
        try {
            windowControl = (WindowControl)
                browserControl.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        context.setDescription("This test tries to set Browser visibility to " + currentArg);
        return true;        
    }
    public void execute() {      
        if (null != windowControl) {
            try {
                windowControl.setVisible(currentArg);
            }catch(Exception e) {
                TestContext.registerFAILED("Exception during execution: " + e.toString());
                return;
            }
            context.addDescription("\nAction performed. Browser isn't crashed on call setVisible " + currentArg);
            if (currentArg) {
                 context.addDescription("\nIf page is still shown then this test PASSED");
            } else {
                 context.addDescription("\nIf page is still shown then this test FAILED");
            }
        } else {
            TestContext.registerFAILED("null WindowControl passed to test");
        }
    }
   
}




