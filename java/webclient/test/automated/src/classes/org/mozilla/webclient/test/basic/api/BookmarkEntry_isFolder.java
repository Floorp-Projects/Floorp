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
 * BookmarkEntry_isFolder.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class BookmarkEntry_isFolder implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Bookmarks bookmarks = null;
    private int currentArg;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private BookmarkEntry bookmarkEntry = null;
    private TreeModel bookmarksTree = null;

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

boolean yes=true;

  if (bookmarks==null) return;

	context.setDefaultResult(TestContext.FAILED);
	
  switch (currentArg) {
   case 0:
	context.setDefaultComment("We are trying to get isFolder value via newBookmarkEntry");
       try {
	yes=true;
        bookmarkEntry = bookmarks.newBookmarkEntry("http://java.sun.com");
	yes=bookmarkEntry.isFolder();

       } catch(Exception e) {
       if (e instanceof UnimplementedException) TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
         else TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
  if (yes==false) TestContext.registerPASSED(".isFolder() is false and it is right");
     else TestContext.registerFAILED(".isFolder() is true and it isn't right");

	break;
   case 1:
	try {
	context.setDefaultComment("We are trying to get isFolder value via newBookmarkFolder");
	yes=false;
        bookmarkEntry = bookmarks.newBookmarkFolder("My test bookmarks");
	yes=bookmarkEntry.isFolder();
       } catch(Exception e) {
       if (e instanceof UnimplementedException) TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
         else TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
  if (yes==true) TestContext.registerPASSED(".isFolder() is true and it is right");
     else TestContext.registerFAILED(".isFolder() is false and it isn't right");

      break;
   case 2:
       context.setDefaultComment("We are trying to get isFolder value of leaf received via Bookmarks TreeModel");
       try{
           yes = true;
           bookmarksTree = bookmarks.getBookmarks();
           int count = bookmarksTree.getChildCount(bookmarksTree.getRoot());
           for(int j=0;j<count;j++){
               if(bookmarksTree.isLeaf(bookmarksTree.getChild(bookmarksTree.getRoot(), j))){
                   bookmarkEntry = (BookmarkEntry)bookmarksTree.getChild(bookmarksTree.getRoot(), j);
                   j = count;
               }
           }
           yes = bookmarkEntry.isFolder();
       }catch(Exception e){
           if (e instanceof UnimplementedException) TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           else TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
       if (yes==false) TestContext.registerPASSED(".isFolder() is false and it is right");
       else TestContext.registerFAILED(".isFolder() is true and it isn't right");
       break;

   case 3:
       context.setDefaultComment("We are trying to get isFolder value of folder received via Bookmarks TreeModel");
       try{
           yes = false;
           bookmarksTree = bookmarks.getBookmarks();
           int count = bookmarksTree.getChildCount(bookmarksTree.getRoot());
           for(int j=0;j<count;j++){
               if(!bookmarksTree.isLeaf(bookmarksTree.getChild(bookmarksTree.getRoot(), j))){
                   bookmarkEntry = (BookmarkEntry)bookmarksTree.getChild(bookmarksTree.getRoot(), j);
                   j = count;
               }
           }
           yes = bookmarkEntry.isFolder();
       }catch(Exception e){
           if (e instanceof UnimplementedException) TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           else TestContext.registerFAILED("Exception during execution: " + e.toString());
       }
       if (yes==true) TestContext.registerPASSED(".isFolder() is true and it is right");
       else TestContext.registerFAILED(".isFolder() is false and it isn't right");
       break;
   }
 }
} 
