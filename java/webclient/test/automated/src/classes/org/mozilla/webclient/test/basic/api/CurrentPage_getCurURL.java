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
 * CurrentPage_getCurURL.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.Toolkit;


public class CurrentPage_getCurURL implements Test, TestDocLoadListener
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private Navigation navigation = null;
    private CurrentPage curPage = null;
    private String currentArg = null;
    private String[] pages = null;
    private boolean loading, loaded = false;
    private int first, second=0;	

    public boolean initialize(TestContext context) {
	
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
//        this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
        this.currentArg = context.getCurrentTestArguments();
        try {
            curPage = (CurrentPage)
                browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);

            history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);

            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);

        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
	context.setDocLoadListener(this);
        return true;        
    }

public void eventDispatched(DocumentLoadEvent event)
{
  if (event.getType()==4) {
	first++; 
	execute();
   }
  if (event.getType()==1) {
    second++; 
    if (second==2) {
	loaded=true;
	execute();
    }
  }
}


    public void execute() {      
System.out.println("first="+first+" secone="+second);
if (curPage==null) return;

String url, url2=null;

Clipboard clip=java.awt.Toolkit.getDefaultToolkit().getSystemClipboard();
StringTokenizer st=new StringTokenizer(currentArg,",");
int num=java.lang.Integer.parseInt(st.nextToken());
url=st.nextToken().trim();
url2=st.nextToken().trim();

if (!url.equals("null")) {
  if (!loading) {
    navigation.loadURL(url);
    System.out.println("LOAD="+url);
    loading=true;
   }

 if (first!=1) return;
}

if ((num==1)&&(second!=2)) {
	System.out.println("LOAD 2");
	navigation.loadURL(url2);
	return;
}

if (num==2) {
 try {
	context.setDefaultResult(TestContext.FAILED);
	context.setDefaultComment("We are trying to get URL without loaded URL");

	curPage.getCurrentURL();
     }
 catch (Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
	 TestContext.registerPASSED("Browser doesn't crashed without URL (exception): "+e);
      return;
  }
 TestContext.registerPASSED("Browser doesn't crashed during invokation of getCurrentURL without URL");
}


 try{
	context.setDefaultResult(TestContext.FAILED);
	context.setDefaultComment("We are trying to get URL: "+url);

     if (url.equals(curPage.getCurrentURL())) TestContext.registerPASSED("Right URL value="+url);
	else TestContext.registerFAILED("Bad URL value="+curPage.getCurrentURL()+" instead of "+url);

    } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
        TestContext.registerFAILED("Exception during execution: " + e.toString());
  }; 

 }   
}
