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
 * History_getHistory.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_getHistory implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private int currentArg = 0;
    private String[] pages = null;
    public HistoryEntry[] history_arr;

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
        boolean result = true;
        if (null == history) {
            TestContext.registerFAILED("null history passed to test");
            return;
        }
        if(context.getExecutionCount()==1){
            try{
                if(history.getCurrentHistoryIndex() == currentArg) {
                    try {
                        history_arr = history.getHistory();
                    }catch(Exception e) {
                        if(e instanceof UnimplementedException){
                            context.setDefaultResult(TestContext.UNIMPLEMENTED);
                            TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
                        }else{
                            TestContext.registerFAILED("Exception during execution: " + e.toString());
                        }
                        return;
                    }
                    for(int i=0; i<pages.length; i++){
                        try{
                            if(!history_arr[i].getURL().equals(new URL(pages[i]))){
                                result = false; 
                                TestContext.registerFAILED("getHistory return incorrect value");
                                return ;
                            }
                        }catch(MalformedURLException e){
                            TestContext.registerFAILED("Exception when constructing URL, exception is:" + e.toString());
                            return ;
                        }
                    } 
                    if(result){
                        TestContext.registerPASSED("getHistory return correct value");
                        return ;
                    }
                    
                }
                context.setDefaultResult(TestContext.FAILED);
                context.setDefaultComment("Hangs when try to load page N " + (currentArg + 1) + 
                                          " of " + pages.length);
                history.setCurrentHistoryIndex(currentArg);
            }catch(Exception e){
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return;
            }
        }
        if(context.getExecutionCount()==2){
            try {
                history_arr = history.getHistory();
            }catch(Exception e) {
                 if(e instanceof UnimplementedException){
                    context.setDefaultResult(TestContext.UNIMPLEMENTED);
                    TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
                 }else{
                    TestContext.registerFAILED("Exception during execution: " + e.toString());
                 }
                 return;
            }
            if(history_arr == null){
                System.out.println("The history_arr array is null!");
                if(pages.length == 0){
                    TestContext.registerPASSED("getHistory with pages.length = 0 return correct value - null");
                    return ;
                }else{
                    TestContext.registerFAILED("getHistory return null when pages.length =  "+pages.length);
                    return ;
                }
            }
            for(int i=0; i<pages.length; i++){
              try{
                if(!history_arr[i].getURL().equals(new URL(pages[i]))){
                     result = false; 
                     TestContext.registerFAILED("getHistory return incorrect value");
                     return ;
                }
              }catch(MalformedURLException e){
                  TestContext.registerFAILED("Exception when constructing URL, exception is:" + e.toString());
                  return ;
              }
            } 
            if(result){
                TestContext.registerPASSED("getHistory return correct value");
                return ;
            }
        }
    }
   
}




