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
 * History_setCurrentHistoryIndex.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_setCurrentHistoryIndex implements Test
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
        if(context.getExecutionCount()==1){
            if(pages.length!=0) {
                try {
                    if(currentArg<=pages.length && currentArg>=0){
                        context.setDefaultResult(TestContext.FAILED);
                        context.setDefaultComment("Try to call setCurrentHistoryIndex(" + currentArg + 
                                                 ") when history length is " + pages.length + 
                                                 ". But page doesn't loaded");
                        history.setCurrentHistoryIndex(currentArg);
                    }else{
                        context.setDefaultResult(TestContext.PASSED);
                        context.setDefaultComment("Try to call setCurrentHistoryIndex(" + currentArg + 
                                                 ") when history length is " + pages.length + 
                                                 ". Browser isn't crashed");
                        history.setCurrentHistoryIndex(currentArg);
                        return ;
                    }
                }catch(Exception e) {
                    if(e instanceof org.mozilla.util.RangeException){
                        if(currentArg<0){
                            TestContext.registerPASSED("Exception occured,but the index is out of range: " +currentArg+ " Exception is: "+ e.toString());
                        }else{
                            TestContext.registerFAILED("org.mozilla.util.RangeException is thrown, but the index is: " +currentArg+ " Exception is: " + e.toString());
                        }
                    }else{
                        TestContext.registerFAILED("Exception during execution: " + e.toString());
                        return;
                    }
                }
            } else {
               try{
                   context.setDefaultResult(TestContext.PASSED);
                   context.setDefaultComment("Try to call setCurrentHistoryIndex(" + currentArg + 
                                            ") when history is empty");
                   history.setCurrentHistoryIndex(currentArg);
                   return ;
               }catch(Exception e){
                  if(e instanceof org.mozilla.util.RangeException){
                      if(currentArg<0){
                          TestContext.registerPASSED("Exception occured,but the index is out of range: " +currentArg+ " Exception is: "+ e.toString());
                      }else{
                          TestContext.registerFAILED("org.mozilla.util.RangeException is thrown, but the index is: " +currentArg+ " Exception is: " + e.toString());
                      }
                  }else{
                      TestContext.registerFAILED("Exception during execution: " + e.toString());
                      return;
                  }
               }
            }
        }
        if(context.getExecutionCount()==2){
            try{
                curPage = (CurrentPage)browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
                try{
                    if(new URL(curPage.getCurrentURL()).equals(new URL(pages[currentArg]))){
                        TestContext.registerPASSED("setCurrentHistoryIndex returned correct value");
                    }else{
                        TestContext.registerFAILED("setCurrentHistoryIndex return incorrect value");
                    }
                }catch(MalformedURLException e) {
                    TestContext.registerFAILED("Exception when constructing URL from: " + curPage.getCurrentURL() + 
                                                   " and " + pages[currentArg]);
                }
            }catch(Exception e){
                  if(e instanceof org.mozilla.util.RangeException){
                      if(currentArg<0){
                          TestContext.registerPASSED("Exception occured,but the index is out of range: " +currentArg+ " Exception is: "+ e.toString());
                      }else{
                          TestContext.registerFAILED("org.mozilla.util.RangeException is thrown, but the index is: " +currentArg+ " Exception is: " + e.toString());
                      }
                  }else{
                      TestContext.registerFAILED("Exception during execution: " + e.toString());
                      return;
                  }
            }
        }
    }
   
}




