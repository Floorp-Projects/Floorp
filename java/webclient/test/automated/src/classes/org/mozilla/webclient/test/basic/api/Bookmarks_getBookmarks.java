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
 * Bookmarks_getBookmarks.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class Bookmarks_getBookmarks implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Bookmarks bookmarks = null;
    private String currentArg = null;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private BookmarkEntry bookmarkEntry = null;
    private TreeModel bookmarksTree = null;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        this.currentArg = context.getCurrentTestArguments();
        if(this.currentArg.equals("null")){
            this.currentArg = null; 
        }
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
       if(context.getExecutionCount()==1){
           try {
                bookmarkEntry = bookmarks.newBookmarkEntry("http://java.sun.com");
                bookmarks.addBookmark(null, bookmarkEntry);
                bookmarksTree = bookmarks.getBookmarks();
           }catch(Exception e) {
               if(e instanceof UnimplementedException){
                  TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
               }else{
                  TestContext.registerFAILED("Exception during execution: " + e.toString());
               }
               return;
           }
           if(bookmarksTree != null && bookmarksTree.getIndexOfChild(bookmarksTree.getRoot(), bookmarkEntry)!=-1){
               System.out.println("Index of child is: "+bookmarksTree.getIndexOfChild(bookmarksTree.getRoot(), bookmarkEntry));
               System.out.println("GetchildCount return: "+bookmarksTree.getChildCount(bookmarksTree.getRoot()));
               TestContext.registerPASSED("getBookmarks return correct TreeModel ");
               return ;
           }else{
               TestContext.registerFAILED("getBookmarks return incorrect TreeModel ");
               return ;
           }
       }
       
    }  
   
}




