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
 * History_getCurrentHistoryIndex.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_getCurrentHistoryIndex implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private CurrentPage curPage = null;
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
        int count = context.getExecutionCount();
        if (null == history) {
            TestContext.registerFAILED("null history passed to test");
            return;
        }
        switch(count) {
        case 1:
            try {
                if(currentArg == -1) { //Default index 
                    if(history.getCurrentHistoryIndex()==(pages.length - 1)) {
                        TestContext.registerPASSED("getCurrentHistoryIndex returned correct value " + 
                                                   (pages.length -1) );
                    }else{
                        TestContext.registerFAILED("getCurrentHistoryIndex return incorrect value " + 
                                                   history.getCurrentHistoryIndex() + " instead of " + 
                                                   (pages.length-1));
                    }
                    return;
                }
                context.setDefaultResult(TestContext.FAILED);
                context.setDefaultComment("Hangs when load page N " + (currentArg + 1) +
                                          " of " + pages.length);
                history.setCurrentHistoryIndex(currentArg);
            }catch(Exception e) {
                TestContext.registerFAILED("Exception during execution: " + e.toString());
            }
            break;
        case 2:
            try {
                if(history.getCurrentHistoryIndex()==currentArg){
                    TestContext.registerPASSED("getCurrentHistoryIndex returned correct value " + 
                                               currentArg);
                }else{
                    TestContext.registerFAILED("getCurrentHistoryIndex return incorrect value " + 
                                               history.getCurrentHistoryIndex() + " instead of " + 
                                               currentArg);
                }
            }catch(Exception e) {
                TestContext.registerFAILED("Exception during execution: " + e.toString());
            }
            break;
        default:
            TestContext.registerFAILED("Very strange.Executed 3 times.");
        }
        return;
    }
   
}




