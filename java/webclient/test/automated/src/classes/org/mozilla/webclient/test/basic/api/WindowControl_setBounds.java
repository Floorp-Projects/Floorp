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


 
package org.mozilla.webclient.test.basic.api;

/*
 * WindowControl_setBounds.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.awt.Rectangle;
import java.awt.Frame;
import java.awt.Container;
import java.awt.Component;
import java.net.MalformedURLException;

public class WindowControl_setBounds implements Test
{
    public static String COORD_SEPARATOR="|";
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private WindowControl windowControl = null;
    private Rectangle currentArg = null;
    private String[] pages = null;
    private Frame tw = null;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        this.tw = context.getTestWindow();
        String arguments = context.getCurrentTestArguments().trim();
        if (arguments.equals("null")) {
            this.currentArg = null;
        } else {
            StringTokenizer st = new StringTokenizer(arguments,COORD_SEPARATOR);
            if (st.countTokens() != 4) {
                TestContext.registerFAILED("Bad test arguments: " + arguments);
                return false;
            }
            int[] coord = new int[st.countTokens()];
            for(int i=0;st.hasMoreTokens();i++) {
                coord[i] = (new Integer(st.nextToken())).intValue();
            }
            this.currentArg = new Rectangle(coord[0],coord[1],coord[2],coord[3]);
            
        }
        try {
            windowControl = (WindowControl)
                browserControl.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        return true;        
    }
    public void execute() {      
        if (null != windowControl) {
            String url = null;
            try {
                windowControl.setBounds(currentArg);         
            }catch(Exception e) {
                if (currentArg != null) {
                    TestContext.registerFAILED("Exception during execution: " + e.toString());
                } else {
                    TestContext.registerPASSED("Exception ocurred, when null Rectangle.\n"+
                                               "But browser isn't crashed");
                }
                return;
            } 
            TestContext.registerPASSED("Browser isn't crashed on correct call ..MBI");
        } else {
            TestContext.registerFAILED("null WindowControl passed to test");
        }
    }
   
}




