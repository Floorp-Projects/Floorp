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
 * BookmarkEntry_getAllowsChildren.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.util.*;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class BookmarkEntry_getAllowsChildren implements Test
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
    try {
    } catch(Exception e) {TestContext.registerFAILED("Exception during execution: " + e.toString());}

 boolean result=false;

   switch(currentArg) {
    case 0: 	
       context.setDefaultComment("We are trying to get result of getAllowsChildren() from just created BookmarkFolder");
       try {
           bookmarkFolder = bookmarks.newBookmarkFolder("TEST");
           result=false;
           result=bookmarkFolder.getAllowsChildren();
       } catch(Exception e) {
           if (e instanceof UnimplementedException) TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
             else TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
       if (result) TestContext.registerPASSED("Just created BookmarkFolder allows children");
       else TestContext.registerFAILED("Just created BookmarkFolder  doesn't allow children");
      break;

    case 1:
       context.setDefaultComment("We are trying to get result of getAllowsChildren() from just created BookmarkEntry");
       try {
           bookmarkEntry = bookmarks.newBookmarkEntry("Entry1");
           result=true;
           result=bookmarkEntry.getAllowsChildren();
       } catch(Exception e) {
           if (e instanceof UnimplementedException) TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
             else TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
       if (!result) TestContext.registerPASSED("Just created BookmarkEntry doesn't allows children");
        else TestContext.registerFAILED("Just created BookmarkEntry allows children");
      break;

    case 2: 	
       context.setDefaultComment("We are trying to get result of getAllowsChildren() from BookmarkFolder received from Bookmarks TreeModel");
       try {
           result=false;
           bookmarksTree = bookmarks.getBookmarks();
           int count = bookmarksTree.getChildCount(bookmarksTree.getRoot());
           for(int j=0;j<count;j++){
               if(!bookmarksTree.isLeaf(bookmarksTree.getChild(bookmarksTree.getRoot(), j))){
                   bookmarkFolder = (BookmarkEntry)bookmarksTree.getChild(bookmarksTree.getRoot(), j);
                   j = count;
               }
           }
           result=bookmarkFolder.getAllowsChildren();
       } catch(Exception e) {
           if (e instanceof UnimplementedException){ 
               TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           }else{ 
               e.printStackTrace();
               TestContext.registerFAILED("Exception during execution: " + e.toString());

           }
       }
       if (result) TestContext.registerPASSED("BookmarkFolder received from TreeModel allows children");
       else TestContext.registerFAILED("BookmarkFolder received from TreeModel doesn't allow children");
      break;

    case 3:
       context.setDefaultComment("We are trying to get result of getAllowsChildren() from BookmarkEntry received from Bookmarks TreeModel");
       try {
           result=true;
           bookmarksTree = bookmarks.getBookmarks();
           int count = bookmarksTree.getChildCount(bookmarksTree.getRoot());
           for(int j=0;j<count;j++){
               if(bookmarksTree.isLeaf(bookmarksTree.getChild(bookmarksTree.getRoot(), j))){
                   bookmarkEntry = (BookmarkEntry)bookmarksTree.getChild(bookmarksTree.getRoot(), j);
                   if(!bookmarkEntry.isFolder()){
                       j = count;
                   }
               }
           }
           result=bookmarkEntry.getAllowsChildren();
       } catch(Exception e) {
           if (e instanceof UnimplementedException) TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
             else TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
       if (!result) TestContext.registerPASSED("BookmarkEntry received from TreeModel doesn't allows children");
        else TestContext.registerFAILED("BookmarkEntry received from TreeModel allows children");
      break;

   }
 }
} 
