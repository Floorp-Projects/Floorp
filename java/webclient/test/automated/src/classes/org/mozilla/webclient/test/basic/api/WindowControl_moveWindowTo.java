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
 * WindowControl_moveWindowTo.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.awt.Rectangle;
import java.awt.Frame;
import java.awt.Container;
import java.awt.Component;
import java.net.MalformedURLException;

public class WindowControl_moveWindowTo implements Test
{
    public static String COORD_SEPARATOR=",";
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private WindowControl windowControl = null;
    private boolean goodCase = false;
    private int x = 0;
    private int y = 0;
    private Frame tw = null;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.tw = context.getTestWindow();
        String arguments = context.getCurrentTestArguments().trim();
        StringTokenizer st = new StringTokenizer(arguments,COORD_SEPARATOR);
        if (st.countTokens() != 2) {
            TestContext.registerFAILED("Bad test arguments: " + arguments);
            return false;
        }
        x = (new Integer(st.nextToken())).intValue();
        y = (new Integer(st.nextToken())).intValue();
        Rectangle bounds = tw.getBounds();
        if((x>=0)&&(y>=0)&&(y<=bounds.height)&&(x<=bounds.width)) {
            goodCase = true;
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
                windowControl.moveWindowTo(x,y);         
            }catch(Exception e) {
                if (goodCase) {
                    TestContext.registerFAILED("Exception in legal case: " + e.toString() +
                                               "\n x = " + x + " y = " + y);
                } else {
                    TestContext.registerPASSED("Exception ocurred  x = " + x + " y = " + y);
                                              
                }
                return;
            }
            if (goodCase) {
                TestContext.registerPASSED("Browser isn't crashed on correct call ..MBI" + 
                                           " x = " + x + " y = " + y);
            }else {
                TestContext.registerPASSED("Browser isn't crashed on incorrect call" +
                                           " x = " + x + " y = " + y);
            }
            return;
        } else {
            TestContext.registerFAILED("null WindowControl passed to test");
        }
    }
   
}




