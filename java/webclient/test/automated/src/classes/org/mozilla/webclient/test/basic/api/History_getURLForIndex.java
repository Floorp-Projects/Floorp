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
 * History_getURLForIndex.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_getURLForIndex implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private int currentArg = 0;
    private String[] pages = null;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
        try {
            history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        return true;        
    }
    public void execute() {
        String exceptionDuringExecution = null;
        String url = null;
        if ( context.failedIsRegistered() ) {
            return;
        }
        if ( history == null) {
            TestContext.registerFAILED("null history passed to test");
            return;
        }
        try {
            url = history.getURLForIndex(currentArg);
            System.out.println("CurrentArg = "+currentArg+" and url = "+url);
        }catch(Exception e) {
            exceptionDuringExecution = e.toString();
        }
        if ((currentArg>=0)&&(currentArg<=pages.length)) { //If use valid index
            if(exceptionDuringExecution != null) {
                TestContext.registerFAILED("Exception during execution when try to get url with valid index: " + 
                                           currentArg + " :(" + exceptionDuringExecution + ")");
                return;
            }
            try {
                if ((new URL(pages[currentArg])).equals((new URL(url)))) {
                    TestContext.registerPASSED("Good url returned: " + url);
                }else {
                    TestContext.registerFAILED("Wrong url: " + url + " istead of " + pages[currentArg]);
                }
            } catch (MalformedURLException e) {
                TestContext.registerFAILED("Exception when constructing URL from: " + url + 
                                           " and " + pages[currentArg]);
            }
            return;
        }
        //If try to get invalid index
        if(exceptionDuringExecution != null) {
            TestContext.registerPASSED("(" + exceptionDuringExecution + ") ocurred. But index was invalid: " + currentArg);
        } else {
            TestContext.registerPASSED("Browser isn't crashed on invalid index: " + currentArg);
        } 
    }
   
}




