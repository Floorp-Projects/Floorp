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
 * History_getHistoryEntry.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_getHistoryEntry implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private HistoryEntry history_entry = null;
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
        if (null != history) {
            //int number_of_pages;
            try {
                history_entry = history.getHistoryEntry(currentArg);
                if(history_entry==null){
                    TestContext.registerPASSED("getHistoryEntry returned correct value");
                }else{
                    if(new URL(history_entry.getURL()).equals(new URL(pages[currentArg]))){
                        TestContext.registerPASSED("getHistoryEntry returned correct value");
                    }else{
                        TestContext.registerFAILED("getHistoryEntry return incorrect value");
                    }
                }
            }catch(Exception e) {
                 if(e instanceof UnimplementedException){
                    TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
                 }
                 if(e instanceof org.mozilla.util.RangeException){
                     if(currentArg<0){
                         TestContext.registerPASSED("Exception occured,but the index is out of range: " +currentArg+ " Exception is: "+ e.toString());
                     }else{
                         TestContext.registerFAILED("org.mozilla.util.RangeException is thrown, but the index is: " +currentArg+ " Exception is: " + e.toString());
                     }
                 }
                 else{
                    TestContext.registerFAILED("Exception during execution: " + e.toString());
                 }
                 return;
            }
        } else {
            TestContext.registerFAILED("null history passed to test");
        }
    }
   
}




