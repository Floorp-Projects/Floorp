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
 * History_getForwardList.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class History_getForwardList implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private int currentArg = 0;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private HistoryEntry[] forwardList = null;
    private boolean result = true;

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
          if (null != history) {
            try {
                context.setDefaultResult(TestContext.FAILED);
                context.setDefaultComment("Hangs when try to load page N " + (currentArg + 1) + 
                                          " of " + pages.length);
                if(currentArg!=-1){
                    history.setCurrentHistoryIndex(currentArg);
                }else{
                    forwardList = history.getForwardList();
                    if(forwardList.length == 0){
                        TestContext.registerPASSED("getForwardList return correct value - empty array when list is empty");
                        return ;
                    }else{
                        TestContext.registerFAILED("getForwardList return incorrect value when list is empty");
                        return ;
                    }
                }
            }catch(Exception e) {
                 if(e instanceof UnimplementedException){
                     TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
                     return;
                 }
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return ;
            }
          }else{
              TestContext.registerFAILED("null history passed to test");
          }
      }

      if(context.getExecutionCount()==2) {
        try {
             forwardList = history.getForwardList();
        }catch(Exception e) {
             if(e instanceof UnimplementedException){
                TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
             }else{
                TestContext.registerFAILED("Exception during execution: " + e.toString());
             }
             return ;
        }
        if(forwardList==null){
           if(currentArg+1==pages.length) {
              TestContext.registerPASSED("getForwardList return correct value when list is empty");
              return ;
           }else{
              TestContext.registerFAILED("getForwardList return null when list is not empty");
              return ;
          }
        }else{
          try{
              if(forwardList.length == (pages.length-currentArg)){
                  for(int i=currentArg; i<pages.length; i++){
                       if(!forwardList[i-currentArg].getURL().equals(new URL(pages[i]))){
                           result = false; 
                           TestContext.registerFAILED("getForwardList return incorrect value when list is not empty");
                           return ;
                       }
                  }
              }else{
                  TestContext.registerFAILED("The length of forward list and real forward list is not equal!");
                  return ;
              }
          }catch(MalformedURLException e) {
                     TestContext.registerFAILED("Exception when constructing URL, exception is: " +e.toString());
                     return ;
          }
          if(result){
              TestContext.registerPASSED("getForwardList return correct value when list is not empty");
          }

       }
      }      
    }
   
}




