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
 * CurrentPage_getDOM.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.awt.datatransfer.*;
import java.awt.Toolkit;
import org.w3c.dom.Document;
import org.w3c.dom.Node;

public class CurrentPage_getDOM implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private Navigation navigation = null;
    private CurrentPage curPage = null;
    private String currentArg = null;
    private String[] pages = null;
    private boolean loading = false;


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
        return true;        
    }


    public void execute() {      

if (curPage==null) return;

String url=null;
Clipboard clip=java.awt.Toolkit.getDefaultToolkit().getSystemClipboard();
StringTokenizer st=new StringTokenizer(currentArg,",");

Document doc;
Node node1,node2;

url=st.nextToken().trim();

if (!url.equals("null")) {

  if (!loading) {
    navigation.loadURL(url);
    System.out.println("LOAD");
    loading=true;
   }

 if (context.getExecutionCount()!=2) return;
}

String findMe="";

  try {
	context.setDefaultResult(TestContext.FAILED);
	context.setDefaultComment("We are trying to get DOM");

	doc=curPage.getDOM();
	node1=doc.getElementsByTagName("b").item(0);
	node2=doc.getElementsByTagName("i").item(0);
	node1.getParentNode().insertBefore(node1,node2);

        curPage.findInPage("TESTIt is a",true,true);
        curPage.copyCurrentSelectionToSystemClipboard();

	DataFlavor df=new DataFlavor(java.lang.Class.forName("java.lang.String"),null);
	StringSelection o2=(StringSelection)clip.getContents(df);
	findMe=(String)o2.getTransferData(df);
 
	if (!url.equals("null")){
	 if (findMe.equals("TESTIt is a")) {
	   TestContext.registerPASSED("Right getDOM() value");
	   return;
	 } else {
	   TestContext.registerFAILED("Bad getDOM() value");
	   return;
	 };
	};

   } catch(Exception e) {
               if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
               else if (url.equals("null")) TestContext.registerPASSED("Browser doesn't crashed (exception): "+e);
     		else TestContext.registerFAILED("Exception during execution : "+e.toString());
     return;	
  };

TestContext.registerPASSED("Browser doesn't crashed");

 }   
}
