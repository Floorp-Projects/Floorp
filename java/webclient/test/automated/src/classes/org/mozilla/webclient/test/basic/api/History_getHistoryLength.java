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
 * History_getHistoryLength.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_getHistoryLength implements Test
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
        //this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
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
        int number_of_pages;
        try {
            number_of_pages = history.getHistoryLength();
        }catch(Exception e) {
             TestContext.registerFAILED("Exception during execution: " + e.toString());
             return;
        }
        if(number_of_pages == pages.length){
            TestContext.registerPASSED("getHistoryLength returned correct value: " + number_of_pages);
        }else{
            TestContext.registerFAILED("getHistoryLength return incorrect value: " + number_of_pages + " Correct value is: " + pages.length);
        }
    }
   
}




