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
 * History_canBack.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_canBack implements Test
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
        boolean result; 
        if(context.getExecutionCount()==1){
            try {
                 if(pages.length==0){
                     result = history.canBack();
                     if(!result){
                         TestContext.registerPASSED("canBack return false when history is empty");
                     }else{
                         TestContext.registerFAILED("canBack returned true when history is empty");
                     }
                     return ;
                 }else{
                     context.setDefaultResult(TestContext.FAILED);
                     context.setDefaultComment("Hangs when loading page N " + (currentArg + 1) +
                                               " of " + pages.length);
                     history.setCurrentHistoryIndex(currentArg);
                 }
            }catch(Exception e) {
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return;
            }
        }
        if(context.getExecutionCount()==2){
            try {
                 result = history.canBack();
            }catch(Exception e) {
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return;
            }
            if(currentArg>0){
                if(result){
                     TestContext.registerPASSED("canBack returned correct value when it's really can back");
                     return ;
                 }else{
                     TestContext.registerFAILED("canBack returned false when it's really can back");
                     return ;
                }
            }else{ 
                if(!result){
                     TestContext.registerPASSED("canBack returned correct value when it's really cannot back");
                     return ;
                 }else{
                     TestContext.registerFAILED("canBack returned true when it's really cannot back");
                     return ;
                }
            }
        }
    }
   
}




