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
 * CurrentPage_findInPage_1.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.Toolkit;


public class CurrentPage_findInPage_1 implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private Navigation navigation = null;
    private CurrentPage curPage = null;
    private String currentArg = null;
    private String[] pages = null;
    private boolean loaded, loading = false;

    public boolean initialize(TestContext context) {
	
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
//        this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
	this.currentArg = context.getCurrentTestArguments();

        try {
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);

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

StringTokenizer st=new StringTokenizer(currentArg,",");
int num=java.lang.Integer.parseInt(st.nextToken());
String url=st.nextToken();
String findMe=null;


if (!url.equals("null")) {

  if (!loading) {
    navigation.loadURL(url);
    System.out.println("LOAD");
    loading=true;
   }

 if (context.getExecutionCount()!=2) return;
}

Clipboard clip=java.awt.Toolkit.getDefaultToolkit().getSystemClipboard();
DataFlavor df=null;
try {
     df=new DataFlavor(java.lang.Class.forName("java.lang.String"),null);
 } catch(Exception e) {System.out.println("DataFlavor exception "+e.toString());};
System.out.println("clip="+clip);

      switch (num) {
	case 0: 
            try{
     		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find null string");

                findMe=null;
		curPage.findInPage(findMe, true, false);

            } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
		 TestContext.registerPASSED("Browser doesn't crashed with null search string (exception): "+e);
                 return;
             }; 
	break;

	case 1: 
            try {
     		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find empty string");

		findMe="";
		curPage.findInPage(findMe, true, false);

            }catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
		 TestContext.registerPASSED("Browser doesn't crashed with empty search string (exception): "+e);
                 return;
            };
	break;

	case 2: 
            try {
     		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find existed string");

	        clip.setContents(new StringSelection("test string"),null);

                curPage.findInPage("html",true,true);
                curPage.copyCurrentSelectionToSystemClipboard();

		StringSelection o2=(StringSelection)clip.getContents(df);
		findMe=(String)o2.getTransferData(df);

		if (findMe.equals("html")) {
			TestContext.registerPASSED("Equal clipboard values before and after copyCurrentSelectionToSystemClipboard() invocation");
			return;
		} else {
			TestContext.registerFAILED("Different clipboard values before and after copyCurrentSelectionToSystemClipboard() invocation with selection");
			return;
		};

            }catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return;
            };
//	break;

	case 3: 
            try {
    		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find string without URL");

		curPage.findInPage("html", true, false);

             }catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
		 TestContext.registerPASSED("Browser doesn't crashed without URL (exception): "+e);
                 return;
            };

	break;

	case 4: 
            try {
    		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to find unexisted string");

		findMe="badtext";
		curPage.findInPage(findMe, true, false);
 
             }catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
		 TestContext.registerPASSED("Browser doesn't crashed with unexisted search string (exception): "+e);
                 return;
            };

	break;

      };

     TestContext.registerPASSED("Browser doesn't crashed with findInPage value="+findMe);
     return;
  
    	}   
}
