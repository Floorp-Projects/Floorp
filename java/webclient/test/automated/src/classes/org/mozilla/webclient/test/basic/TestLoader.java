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


 
package org.mozilla.webclient.test.basic;

/*
 * TestLoader.java
 */
import org.mozilla.webclient.*;
import java.util.LinkedList;

public class TestLoader extends Thread  implements DocumentLoadListener {
    private Test currentTest = null;
    private TestContext context = null;
    private TestRunner  runner = null;
    private Navigation navigation = null;
    private BrowserControl browserControl = null;
    private EventRegistration eventRegistration = null;
    private int pageCount = 0;
    private int timeout = Integer.MAX_VALUE;
    private String[] pages = null;
    private LinkedList eventList  = null;

    public TestLoader(){
    }
    public boolean initialize(TestContext context, TestRunner runner) {
        this.context = context;
        this.runner = runner;
        try {
            browserControl = context.getBrowserControl();
            eventRegistration = (EventRegistration)
                browserControl.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
            eventRegistration.addDocumentLoadListener(this);
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
            eventList = new LinkedList();
            timeout = (new Integer(context.getProperty(TestContext.DELAY_OF_CYCLE_PROP_NAME))).intValue()*1000;
            pages = context.getPagesToLoad();
            currentTest = currentTest = (Test)Class.forName(context.getTestClass()).newInstance();
            currentTest.initialize(context);
        }catch (Exception e) {
            e.printStackTrace();
		    TestContext.registerFAILED("Exception occured : " + e.toString());
            return false;
		}   
        return true;
    }
    public void run(){ 
        System.out.println("Execution thread started.");
        pageCount=0;
        if(!loadNext()) {
             execute(); 
        }
        while(true) {
            if (eventList.size() > 0) {
                DocumentLoadEvent dle = (DocumentLoadEvent)eventList.removeFirst();
                switch ((int) dle.getType()) {
                case ((int) DocumentLoadEvent.START_DOCUMENT_LOAD_EVENT_MASK):
                    transferToTest(dle);
                    break;
                case ((int) DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK):
                    if(!loadNext()) {
                        if (!transferToTest(dle)) {
                            execute(); 
                        }
                    }
                    break;
                default:
                    transferToTest(dle);
                    break;
                }
            }
            if(context.resultIsSet()){
                delete();
            }
            try {
                synchronized(this){
                    this.wait(timeout);
                }
            }catch(InterruptedException e) {
                System.out.println("InterruptedException in TestLoader" + e);
            }
        }
    }

    private boolean loadNext(){
        if (pageCount < pages.length) {
            System.out.println("Try to load " + pages[pageCount] + " of :" + pages.length);
            navigation.loadURL(pages[pageCount]);
            pageCount++;
            return true;
        }
        return false;
    }
    private boolean transferToTest(DocumentLoadEvent evt) {
        TestDocLoadListener testDLL  = context.getDocLoadListener(); 
        if (testDLL !=null) {
            testDLL.eventDispatched(evt);
            return true;
        }
        return false;
    }
    private void execute() {
        context.increaseExecutionCount();
        currentTest.execute(); 
    }

//From DocumentLoadListener
    
    public void eventDispatched(WebclientEvent event) {
        if (event instanceof DocumentLoadEvent) {
            System.out.println("Add event to the list: " + event.getType()
                               + "List length(before) is " + eventList.size());
            eventList.addLast((Object)event);
            synchronized(this) {
                this.notify();
            }
        } else {
             System.out.println("TestLoader: Strange Event " + event.getType());
        }
    }
    private void delete() {
        context.delete();
        runner.delete();
    }

}


