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


 
package org.mozilla.webclient.test.basic.stress;

/*
 * LongBack.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class LongBack implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private CurrentPage curPage = null;
    private String[] pages = null;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
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
       if(context.getExecutionCount()<pages.length){
           System.out.println("##DEbug:AVM: Do " + context.getExecutionCount() + " BACK.");
           context.setDefaultResult(TestContext.FAILED);
           context.setDefaultComment("Hangs when do " + context.getExecutionCount() + " BACK. Of " + pages.length);
           history.back();
       } else {
           try{
               curPage = (CurrentPage)browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
               if(new URL(curPage.getCurrentURL()).equals(new URL(pages[0]))){
                   TestContext.registerPASSED("All BACK performed:" + pages.length);
               }else{
                   TestContext.registerFAILED("Strange page " + curPage.getCurrentURL() +
                                              " loaded instead of " + pages[0]);
               }
           }catch(MalformedURLException e){
               TestContext.registerFAILED("Exception when constructing URL from: " + curPage.getCurrentURL() + 
                                          " and " + pages[pages.length]);
           }catch(Exception e) {
               TestContext.registerFAILED("Exception during execution: " + e.toString());
           }
           TestRunner tr = context.getRunner();
           context.delete();
           tr.delete();
           return;
       }
    }


}
