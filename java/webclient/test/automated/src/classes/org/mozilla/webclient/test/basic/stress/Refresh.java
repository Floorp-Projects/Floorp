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
 * Refresh.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.Toolkit;


public class Refresh implements Test, TestDocLoadListener
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private Navigation navigation = null;
    private CurrentPage curPage = null;
    private String currentArg = null;
    private String[] pages = null;
    private boolean loaded = true;
    private boolean loading = false;
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
  if (event.getType()==1) {
System.out.println("START arrived");
    second++; 
    loaded=true;
 if (second>1)  execute();
    }
  if (event.getType()==4) {
System.out.println("END arrived");
   first++;
 if (first==1) execute();
  }
  
}


    public void execute() {      

if (curPage==null) return;

String url=null;

StringTokenizer st=new StringTokenizer(currentArg,",");
//int num=java.lang.Integer.parseInt(st.nextToken());
url=st.nextToken().trim();

context.setDefaultResult(TestContext.FAILED);
context.setDefaultComment("refresh");

  if (!loading) {
    navigation.loadURL(url);
    loading=true;
   }

if(first<1) return;


try {
if (second==500) TestContext.registerPASSED("Browser doesn't crashed and page is refreshed 500 times");
navigation.refresh(0);
return;

// if (loaded) {loaded=false;navigation.refresh(0);return;}

     }
 catch (Exception e) {
	 TestContext.registerFAILED("Exception during execution: "+e);
      return;
  }
 }   
}
