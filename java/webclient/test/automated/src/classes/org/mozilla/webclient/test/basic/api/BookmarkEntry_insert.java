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
 * BookmarkEntry_insert.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class BookmarkEntry_insert implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Bookmarks bookmarks = null;
    private String currentArg;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private BookmarkEntry bookmarkEntry = null;
    private TreeModel bookmarksTree = null;
    private MutableTreeNode node, mtn;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        this.currentArg = context.getCurrentTestArguments();
        try {
            bookmarks = (Bookmarks)
                browserControl.queryInterface(BrowserControl.BOOKMARKS_NAME);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
        }
        return true;        
    }
public void execute() {      

  if (bookmarks==null) return;

StringTokenizer st=new StringTokenizer(currentArg,",");
int num=java.lang.Integer.parseInt(st.nextToken());
int n=java.lang.Integer.parseInt(st.nextToken());

	context.setDefaultResult(TestContext.FAILED);
	if (num!=0) context.setDefaultComment("We are trying to insert new node with index="+n);
	  else context.setDefaultComment("We are trying to insert new node=NULL");


   try {
        bookmarksTree = bookmarks.getBookmarks();
	node=(MutableTreeNode)bookmarksTree.getRoot();
	bookmarkEntry=bookmarks.newBookmarkEntry("www.mozilla.org");
	mtn=(MutableTreeNode)bookmarkEntry;

	node=(MutableTreeNode)node.getChildAt(0);

    } catch(Exception e) {TestContext.registerFAILED("Exception during execution: " + e.toString());}


     try {

	switch (num) {
	 case 0:
                node.insert(null,1);
		break;
	 case 1:	
                node.insert(mtn,1);
//		TODO: check; Will be done after bookmark bug fixing
//		return;
		break;
	 case 2:	
	 case 3:	
	 case 4:
                node.insert(mtn,n);
		break;

        }

       } catch(Exception e) {
       if (e instanceof UnimplementedException) TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
         else 
	  if(num==0) TestContext.registerPASSED("Browser doesn't crashed (exception): " + e.toString());
	    else TestContext.registerFAILED("Exception during execution: " + e.toString());
       }

TestContext.registerPASSED("Browser doesn't crashed");

 }
} 
