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
 * EventRegistration_addDocLoadListener_2.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.event.*;

public class EventRegistration_addDocLoadListener_2 implements Test, DocumentLoadListener
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Navigation navigation = null;
    private EventRegistration eventReg = null;
    private String currentArg = null;
    private String[] pages = null;
    private CurrentPage curPage = null;
    private int num = 666;
    private int eventNum=0;
    private String url = null;
    private String docEvent = null;
    private boolean loading = false;
    protected static String sequence = "";

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

        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        return true;        
    }


    public void eventDispatched(WebclientEvent event)
    {
     if (event instanceof DocumentLoadEvent) 
      if (event.getType() == DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK) 
       {
	sequence=sequence+"a";
	eventNum++;
	System.out.println("EVENT -- from "+this+" eventNum:"+eventNum+" EC="+context.getExecutionCount());
	context.increaseExecutionCount();
	execute();
       };
    }


 public void execute() {      

  if (eventReg==null) return;

 StringTokenizer st=new StringTokenizer(currentArg,",");
 num=java.lang.Integer.parseInt(st.nextToken().trim());
 String url=st.nextToken().trim();


  try {
	switch (num) {
	case 0:	
		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to register null DocumentLoadListener");

		eventReg.addDocumentLoadListener(null);
		break;
	case 1: 
       		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to register several same DocumentLoadListeners");

		eventReg.addDocumentLoadListener(this);
		eventReg.addDocumentLoadListener(this);
		eventReg.addDocumentLoadListener(this);

		if (!url.equals("null")) {

		  if (!loading) {
		    navigation.loadURL(url);
		    System.out.println("LOAD");
		    loading=true;
		   }
		
		 if (context.getExecutionCount()<2) return;
		}

		if (eventNum==1) TestContext.registerPASSED("Right number of events");
		  else TestContext.registerFAILED("Bad number of events");
		break;
	case 2: 
       		context.setDefaultResult(TestContext.FAILED);
		context.setDefaultComment("We are trying to register several different DocumentLoadListeners");

		eventReg.addDocumentLoadListener(this);
		eventReg.addDocumentLoadListener(new ER3());
		eventReg.addDocumentLoadListener(new ER4());

		if (!url.equals("null")) {

		  if (!loading) {
		    navigation.loadURL(url);
		    System.out.println("LOAD");
		    loading=true;
		   }
		
		 if (context.getExecutionCount()<2) return;
		}

		break;

	}


  } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
		if (num==0) TestContext.registerPASSED("Browser doesn't crashed (exception): "+e.toString());
		 else
		     TestContext.registerFAILED("Exception during execution : "+e.toString());
     return;	
  };

 }   

}

class ER3 implements DocumentLoadListener
{


    public void eventDispatched(WebclientEvent event)
    { 
     if (event instanceof DocumentLoadEvent) 
      if (event.getType() == DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK) 
       {
	EventRegistration_addDocLoadListener_2.sequence+="b";
	System.out.println("EVENT -- from "+this);
       }
    }

}

class ER4 implements DocumentLoadListener
{


    public void eventDispatched(WebclientEvent event)
    { 
     if (event instanceof DocumentLoadEvent) 
      if (event.getType() == DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK) 
       {
	EventRegistration_addDocLoadListener_2.sequence+="c";
	System.out.println("EVENT -- from "+this);
	if (EventRegistration_addDocLoadListener_2.sequence.equals("cba")) TestContext.registerPASSED("Events passed to right listener(s)");
	  else TestContext.registerFAILED("Events passed to bad listener(s)");
       }
    }

}
