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
 * BookmarkEntry_getChildCount.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.util.*;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class BookmarkEntry_getChildCount implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Bookmarks bookmarks = null;
    private int currentArg;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private BookmarkEntry bookmarkFolder, bookmarkEntry = null;
    private TreeModel bookmarksTree = null;
    private TreeNode node;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
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

	context.setDefaultResult(TestContext.FAILED);
	context.setDefaultComment("We are trying to get the child count");
   try {
    } catch(Exception e) {TestContext.registerFAILED("Exception during execution: " + e.toString());}

   int count=0;
   int counter = 0;
   switch(currentArg) {
    case 0: 	
       try {
           bookmarkFolder = bookmarks.newBookmarkFolder("TEST");
           bookmarkEntry = bookmarks.newBookmarkEntry("Entry1");
           bookmarks.addBookmark(bookmarkFolder, bookmarkEntry);
           bookmarkEntry = bookmarks.newBookmarkEntry("Entry2");
           bookmarks.addBookmark(bookmarkFolder, bookmarkEntry);
           count=bookmarkFolder.getChildCount();
       } catch(Exception e) {
           if (e instanceof UnimplementedException) 
               TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           else 
               TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
       if (count==2) 
           TestContext.registerPASSED("Correct child count received from just created BookmarkFolder with 2 BookmarkEntry inside");
       else 
           TestContext.registerFAILED("Incorrect child count received from just created BookmarkFolder with 2 BookmarkEntry inside, returns"+count+" instead of 2");
    break;

    case 1:
       try {
           bookmarkEntry = bookmarks.newBookmarkEntry("Entry1");
           count=bookmarkEntry.getChildCount();
       } catch(Exception e) {
           if (e instanceof UnimplementedException) 
               TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           else 
               TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
       if (count==0) 
           TestContext.registerPASSED("Correct child count (equals 0) received from just created BookmarkEntry");
       else 
           TestContext.registerFAILED("Incorrect child count received from just created BookmarkEntry, returns "+count+" instead of 0");
    break;

    case 2:
       try{
           bookmarksTree = bookmarks.getBookmarks();
           bookmarkEntry = (BookmarkEntry)bookmarksTree.getRoot();
           count = bookmarkEntry.getChildCount();
           Enumeration num = bookmarkEntry.children();
           counter = 0;
           while(num.hasMoreElements()) {
               num.nextElement();
               counter++;
           }

       } catch(Exception e) {
           if (e instanceof UnimplementedException) 
               TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           else 
               TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
       if (count==counter) 
           TestContext.registerPASSED("Correct child count "+count+" received from Root of BookmarkTree");
       else 
           TestContext.registerFAILED("Incorrect child count received = "+count+" from Root of BookmarkTree, instead of"+counter);
    break;
   }
 }
} 
