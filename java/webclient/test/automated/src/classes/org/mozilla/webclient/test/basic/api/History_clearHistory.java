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
 * History_clearHistory.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_clearHistory implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private Navigation navigation= null;
    private int currentArg = 0;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private HistoryEntry[] history_arr ;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        //this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
        try {
            history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        return true;        
    }
    public void execute() {      
        if(context.getExecutionCount()==1){
            try {
                 history.clearHistory();
                 //curPage = (CurrentPage)browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
            }catch(Exception e) {
                 if(e instanceof UnimplementedException){
                    TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
                 }else{
                    TestContext.registerFAILED("Exception during execution: " + e.toString());
                 }
                 return;
            }
            if(history.getHistoryLength()==0){
                 TestContext.registerPASSED("clearHistory() really clean history");
                 return ;
            }else{
                 TestContext.registerFAILED("clearHistory() do not clean history and history.length= " + history.getHistoryLength());
                 return ;
            } 
        }
        if(context.getExecutionCount()==2){
                 TestContext.registerFAILED("clearHistory() do not clean history and history.length= " + history.getHistoryLength());
        
        }

    }
   
}




