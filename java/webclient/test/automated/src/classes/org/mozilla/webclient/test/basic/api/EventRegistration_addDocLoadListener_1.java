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
 * EventRegistration_addDocLoadListener_1.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.event.*;

public class EventRegistration_addDocLoadListener_1 implements Test, TestDocLoadListener
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Navigation navigation = null;
    private EventRegistration eventReg = null;
    private String currentArg = null;
    private String[] pages = null;
    private CurrentPage curPage = null;
    private int num = 666;
    private String url = null;
    private String docEvent = null;
    private boolean loading = false;
    private int end = 0;

    public boolean initialize(TestContext context) {
	
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
//        this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
        this.currentArg = context.getCurrentTestArguments();
        try {
             eventReg = (EventRegistration)
                browserControl.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
//	eventReg.addDocumentLoadListener(this);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
	context.setDocLoadListener(this);
        return true;        
    }


    public void eventDispatched(DocumentLoadEvent event)
    {
//System.out.println("EVENT="+event.getType());
//      if (!(event instanceof DocumentLoadEvent)) return;

      switch (num) {
	case 0: if (event.getType()==DocumentLoadEvent.START_DOCUMENT_LOAD_EVENT_MASK) 
		 TestContext.registerPASSED("Right DocumentLoadEvent :START_DOCUMENT_LOAD_EVENT_MASK");
		break;
	case 1: if (event.getType()==DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK) 
		 TestContext.registerPASSED("Right DocumentLoadEvent :END_DOCUMENT_LOAD_EVENT_MASK");
		break;
	case 2: if (event.getType()==DocumentLoadEvent.START_URL_LOAD_EVENT_MASK) 
		 TestContext.registerPASSED("Right DocumentLoadEvent :START_URL_LOAD_EVENT_MASK");
		break;
	case 3: if (event.getType()==DocumentLoadEvent.END_URL_LOAD_EVENT_MASK) 
		 TestContext.registerPASSED("Right DocumentLoadEvent :END_URL_LOAD_EVENT_MASK");
		break;
	case 4: if (event.getType()==DocumentLoadEvent.PROGRESS_URL_LOAD_EVENT_MASK) 
		 TestContext.registerPASSED("Right DocumentLoadEvent :PROGRESS_URL_LOAD_EVENT_MASK");
		break;
	case 5: if (event.getType()==DocumentLoadEvent.STATUS_URL_LOAD_EVENT_MASK) 
		 TestContext.registerPASSED("Right DocumentLoadEvent :STATUS_URL_LOAD_EVENT_MASK");
		break;
	case 6: if (event.getType()==DocumentLoadEvent.UNKNOWN_CONTENT_EVENT_MASK) 
		 TestContext.registerPASSED("Right DocumentLoadEvent :UNKNOWN_CONTENT_EVENT_MASK");
		break;
	case 7: if (event.getType()==DocumentLoadEvent.FETCH_INTERRUPT_EVENT_MASK) 
		 TestContext.registerPASSED("Right DocumentLoadEvent :FETCH_INTERRUPT_EVENT_MASK");
      }
      if (event.getType()==DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK)
	{ 
	 end++;
	 execute();
	}
    }


 public void execute() {      

  if (eventReg==null) return;

 StringTokenizer st=new StringTokenizer(currentArg,",");
 num=java.lang.Integer.parseInt(st.nextToken().trim());
 docEvent=st.nextToken().trim();
 url=st.nextToken().trim();

  if (!loading) {
    navigation.loadURL(url);
    System.out.println("LOAD");
    loading=true;
   }

 if (end!=1) return;

  try {
	context.setDefaultResult(TestContext.FAILED);
	context.setDefaultComment("We are waiting for "+docEvent+" events...");

        navigation.loadURL(url);

  } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
     TestContext.registerFAILED("Exception during execution : "+e.toString());
     return;	
  };


 }   

}
