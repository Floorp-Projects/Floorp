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
 * WindowControl_createWindow.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.awt.Rectangle;
import java.awt.Frame;
import java.awt.Container;
import java.awt.Component;
import java.net.MalformedURLException;

public class WindowControl_createWindow implements Test
{
    public static String PARAM_SEPARATOR="|";
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private WindowControl windowControl = null;
    private int nativeWS = 0;
    private Rectangle coords;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        String arguments = context.getCurrentTestArguments().trim();
        StringTokenizer st = new StringTokenizer(arguments,PARAM_SEPARATOR);
        int[] coord = new int[st.countTokens()-1];
        try {
            windowControl = (WindowControl)
                browserControl.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
        }
        if (st.nextToken().equals("well")) {
            nativeWS = windowControl.getNativeWebShell();
        } else {
            nativeWS = (new Integer(st.nextToken())).intValue();
        }
        String fCoord = st.nextToken();
        if (fCoord.equals("null")) {
            this.coords=null;
        }else { 
            coord[0] = (new Integer(fCoord)).intValue();
            for(int i=1;st.hasMoreTokens();i++) {
                coord[i] = (new Integer(st.nextToken())).intValue();
            }
            this.coords = new Rectangle(coord[0],coord[1],coord[2],coord[3]);
        }
        return true;        
    }
    public void execute() {      
        if (null != windowControl) {
            try {
                windowControl.createWindow(nativeWS,coords);
            }catch(Exception e) {
                TestContext.registerFAILED("Exception during execution: " + e.toString());
                return;
            }
            TestContext.registerPASSED("Browser isn't crashed .. MBI");
            return;
        } else {
            TestContext.registerFAILED("null WindowControl passed to test");
        }
    }
   
}




