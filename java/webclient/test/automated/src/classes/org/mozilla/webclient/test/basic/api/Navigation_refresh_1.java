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
 * Navigation_refresh_1.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;


public class Navigation_refresh_1 implements Test, TestDocLoadListener
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private Navigation navigation = null;
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
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);

            history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);

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
	execute();
   }

}


    public void execute() {      

System.out.println("first="+first+" second="+second);
   if (null != navigation) {

StringTokenizer st=new StringTokenizer(currentArg,",");
int num=java.lang.Integer.parseInt(st.nextToken().trim());
String host=st.nextToken().trim();
String host2=st.nextToken().trim();

if (!host.equals("null")) {
  if (!loading) {
    navigation.loadURL(host);
    System.out.println("LOAD");
    loading=true;
   }

 if (first!=1) return;
//if (context.getExecutionCount()!=2) return;
}

 if ((num==1)&&(!loaded)) {loaded=true; navigation.loadURL(host2);}
 if ((num==1)&&(second!=1)) return;

            try {
      		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to refresh URL");

                navigation.refresh(0);

            } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
                 TestContext.registerFAILED("Exception during execution: " + e.toString());
                 return;
             }; 

  TestContext.registerPASSED("Browser doesn't crashed");
  return;
  
    }	}   
}
