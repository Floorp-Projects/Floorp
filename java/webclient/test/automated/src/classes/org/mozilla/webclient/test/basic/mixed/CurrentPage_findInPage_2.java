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
 * CurrentPage_findInPage_2.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.Toolkit;


public class CurrentPage_findInPage_2 implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private CurrentPage curPage = null;
    private String currentArg = null;
    private String[] pages = null;


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

        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        return true;        
    }


    public void execute() {      

if (curPage==null) return;

String findMe=null;
Boolean forw, mCase;
String fromClip=null;
int num=0;

Clipboard clip=java.awt.Toolkit.getDefaultToolkit().getSystemClipboard();
StringTokenizer st=new StringTokenizer(currentArg,",");

num=java.lang.Integer.parseInt(st.nextToken().trim());
findMe=st.nextToken();
forw=new Boolean(st.nextToken().trim());
mCase=new Boolean(st.nextToken().trim());

//System.out.println("1="+findMe+" 2="+forw.booleanValue()+" 3="+mCase.booleanValue());

   try{
	context.setDefaultResult(TestContext.FAILED);
	context.setDefaultComment("We are trying to find string: "+findMe);

	switch (num) {
	 case 0:context.setDescription("If second word is selected press PASSED button, otherwise press FAILED button");break;
	 case 1:context.setDescription("If first word is selected press PASSED button, otherwise press FAILED button");break;
	 case 2:context.setDescription("If fourth word is selected press PASSED button, otherwise press FAILED button");break;
	 case 3:context.setDescription("If fifth word is selected press PASSED button, otherwise press FAILED button");break;
	}

	curPage.findInPage(findMe, forw.booleanValue(), mCase.booleanValue());


/*	if (mCase.booleanValue()) {
	  if (fromClip.equals("Html")) {
	     TestContext.registerPASSED("Right result for findInPage() method");
	     return;
  	  } else TestContext.registerFAILED("Bad result for findInPage() method");
        } else {
	  if (fromClip.equals("html")) {
	     TestContext.registerPASSED("Right result for findInPage() method");
	     return;
	};
          };
*/
      } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
        TestContext.registerFAILED("Exception during execution: " + e.toString());
        return;
   }; 

//     TestContext.registerPASSED("Browser doesn't crashed with FindInPage value="+findMe);
     return;
  
    	}   
}
