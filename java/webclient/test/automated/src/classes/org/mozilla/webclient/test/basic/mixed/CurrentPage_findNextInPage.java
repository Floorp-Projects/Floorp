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

 
package org.mozilla.webclient.test.basic.mixed;

/*
 * CurrentPage_findNextInPage.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.Toolkit;


public class CurrentPage_findNextInPage implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private CurrentPage curPage = null;
    private int currentArg = 0;
    private String[] pages = null;
    private boolean loading = false;

    public boolean initialize(TestContext context) {
	
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
        try {
            curPage = (CurrentPage)
                browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);

            history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);

        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        return true;        
    }


    public void execute() {      
if (curPage==null) return;

Clipboard clip=java.awt.Toolkit.getDefaultToolkit().getSystemClipboard();
String findMe=null;

      switch (currentArg) {
	case 0: 
            try{
		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find next string");
		context.setDescription("If second word is selected press PASSED button, otherwise press FAILED button");

		curPage.findInPage("html", true, true);
		curPage.findNextInPage();


            } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return;
             }; 
	break;

	case 1: 
            try {
		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find next string");
		context.setDescription("If first word is selected press PASSED button, otherwise press FAILED button.Correction required corresponding to API changes");

		curPage.findInPage("html", true, true);
		curPage.findNextInPage();
		curPage.findNextInPage();


            }catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return;
            };
	break;

	case 2: 
            try {
		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find string without URL");
		context.setDescription("If nothing is selected press PASSED button, otherwise press FAILED button");
                
		curPage.findNextInPage();


            }catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
		 TestContext.registerPASSED("Browser doesn't crashed without previous find (exception): "+e);
                 return;
            };
	break;


	case 3: 
            try {
		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find next string at the end of page");
		context.setDescription("If second word is selected press PASSED button, otherwise press FAILED button");

                curPage.findInPage("html",true,true);
        	curPage.findNextInPage();
        	curPage.findNextInPage();

            }catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return;
            };
	break;

      };

//     TestContext.registerPASSED("Browser doesn't crashed with findNextInPage()");
     return;
  
    	}   
}
