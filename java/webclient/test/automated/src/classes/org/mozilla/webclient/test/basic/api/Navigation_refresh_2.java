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
 * Navigation_refresh_2.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.io.*;


public class Navigation_refresh_2 implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private Navigation navigation = null;
    private String currentArg = null;
    private String[] pages = null;


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
        return true;        
    }


 public void execute() {      

if (context.getExecutionCount()>1) return;

  if (null != navigation) {

StringTokenizer st=new StringTokenizer(currentArg,",");
long flags=java.lang.Long.parseLong(st.nextToken().trim());

  try {
	context.setDefaultResult(TestContext.FAILED);
	context.setDefaultComment("We are trying to refresh URL");

/*File f=new File("d:\\blackwood\\webclient\\automated\\html\\test\\basic\\bastest1.html");
File f2=new File("d:\\blackwood\\webclient\\automated\\html\\test\\basic\\old");
f.renameTo(f2));

File f3=new File("d:\\blackwood\\webclient\\automated\\html\\test\\basic\\bastest1.html");
File f1=new File("d:\\blackwood\\webclient\\automated\\html\\test\\basic\\bastest2.html");
f1.renameTo(f3);
*/

   navigation.refresh(flags);

/*if (context.getExecutionCount()==1){
f3.renameTo(f1);
f2.renameTo(f);
}*/

  } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
		 if ((e instanceof org.mozilla.util.RangeException)||(flags<0))
                  TestContext.registerPASSED("Incorrect flags raises exception: " + e.toString());}

  TestContext.registerPASSED("Browser doesn't crashed");
  return;
 
    }	}   
}
