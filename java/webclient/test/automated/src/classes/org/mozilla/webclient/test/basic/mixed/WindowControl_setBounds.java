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

public class WindowControl_setBounds implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private WindowControl windowControl = null;
    private String[] pages = null;
	
    private int x_top = 0;
    private int y_top = 0;
    private int x_bot = 0;
    private int y_bot = 0;
    private Rectangle r = null;
    private String rect = null;
    
    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        String arguments = context.getCurrentTestArguments().trim();

	try {
            StringTokenizer tokens = new StringTokenizer(arguments, ",|");
            if (!arguments.equals("null") && tokens.countTokens() != 4) {
                throw(new Exception("Test arguments are incorrect: "+arguments));
            }
	
            if (arguments.equals("null")) {
               r = null;
            } else {
               x_top = Integer.parseInt(tokens.nextToken());
               y_top = Integer.parseInt(tokens.nextToken());
               x_bot = Integer.parseInt(tokens.nextToken());
               y_bot = Integer.parseInt(tokens.nextToken());
               r = new Rectangle(x_top, y_top, x_bot, y_bot);	
               rect = "(" + x_top+","+y_top + ") - (" + x_bot+","+y_bot + ")";
           }
	    context.getTestWindow().setBackground(java.awt.Color.gray);

            windowControl = (WindowControl)
               browserControl.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        } catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
	}
        context.setDescription("This test tries to set browser window bounds to the rectangle: "+rect+"\n");
        return true;        
    }
    public void execute() {      
        if (null != windowControl) {
            try {
		context.addDescription("Window must resize to the bounds rectangle "+rect+"\n");
		windowControl.setBounds(r);
		context.addDescription("Did it really resize ?\n");
            }catch(Exception e) {
                TestContext.registerFAILED("Exception during execution: " + e.toString());
                return;
            }
        } else {
            TestContext.registerFAILED("null WindowControl passed to test");
        }
    }
   
}




