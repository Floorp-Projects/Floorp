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
 * EventRegistration_removeMouseListener_2.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.event.*;
import java.awt.*;

public class EventRegistration_removeMouseListener_2 implements Test, MouseListener, DocumentLoadListener
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Navigation navigation = null;
    private EventRegistration eventReg = null;
    private String currentArg = null;
    private String[] pages = null;
    private int num = 666;
    private String url = null;
    private String docEvent = null;
    private boolean loading = false;

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
	eventReg.addMouseListener(this);
        context.setDescription("Click your mouse in the center of window.\n After that you can press CONTINUE button.");
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}

        return true;        
    }

/***************************************************************/
public void mouseClicked(java.awt.event.MouseEvent e)
{
 TestContext.registerFAILED("Events passed to the removed listener");
}
public void mouseEntered(java.awt.event.MouseEvent e)
{

}
public void mousePressed(java.awt.event.MouseEvent e)
{

}
public void mouseExited(java.awt.event.MouseEvent e)
{

}
public void mouseReleased(java.awt.event.MouseEvent e)
{

}
/****************************************************************/


public void eventDispatched(WebclientEvent event)
{
System.out.println("EVENT="+event.getType());
     if (event instanceof DocumentLoadEvent) 
      if (event.getType() == DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK) 
       {
	context.increaseExecutionCount();
	execute();
       };
}



 public void execute() {      

  if (eventReg==null) return;

// StringTokenizer st=new StringTokenizer(currentArg,",");
// num=java.lang.Integer.parseInt(st.nextToken().trim());
// docEvent=st.nextToken().trim();
// url=st.nextToken().trim();

  try {
        eventReg.removeMouseListener(this);
   	context.setDefaultResult(TestContext.PASSED);
	context.setDefaultComment("Listener was removed");
        
  } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else
     TestContext.registerFAILED("Exception during execution : "+e.toString());
     return;	
  };

 }   

}
