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
 * BookmarkEntry_getChildAt.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.util.*;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class BookmarkEntry_getChildAt implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Bookmarks bookmarks = null;
    private String currentArg;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private BookmarkEntry bookmarkFolder, bookmarkEntry = null;
    private TreeModel bookmarksTree = null;
    private TreeNode node;

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
  int index=java.lang.Integer.parseInt(st.nextToken());

	context.setDefaultResult(TestContext.FAILED);
	context.setDefaultComment("We are trying to get child at index="+index);
    try {
       
	} catch(Exception e) {
        TestContext.registerFAILED("Exception during execution: " + e.toString());
        return;
    }
    switch(num) {
    case 0: 	
       try {
           bookmarkEntry = bookmarks.newBookmarkEntry("Entry1");
           Object child = bookmarkEntry.getChildAt(0);
           if(child == null) {
               TestContext.registerPASSED("getChildAt invoked on BookmarkEntry return null, but doesn't throw exception");
           }
           return;
       } catch(Exception e) {
           if (e instanceof UnimplementedException) {
               TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           }else {
               e.printStackTrace();
               TestContext.registerPASSED("getChildAt invoked on BookmarkEntry throw exception " + e.toString());
           }
           return;
       }
    case 1:
       try {
           try {
               bookmarkFolder = bookmarks.newBookmarkFolder("TEST");
               bookmarkEntry = bookmarks.newBookmarkEntry("Entry1");
               bookmarks.addBookmark(bookmarkFolder, bookmarkEntry);
           }catch(Exception e) {
               e.printStackTrace();
               TestContext.registerFAILED("Exception occured, when prepared to test " + e.toString());
               return;
           }
           BookmarkEntry b = (BookmarkEntry)bookmarkFolder.getChildAt(0);
           if(b.equals(bookmarkEntry)) {
               TestContext.registerPASSED("getChildAt return correct child");
           }else {
               TestContext.registerFAILED("getChildAt return child, that different from original");
           }
           return;
       } catch(Exception e) {
           if (e instanceof UnimplementedException) {
               TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           }else {
               e.printStackTrace();
               TestContext.registerFAILED("getChildAt invoked with correct index, but throw exception " + e.toString());
           }
           return;
       }
    case 2:
        try {
            TreeModel tm = bookmarks.getBookmarks();
            Object root = tm.getRoot();
            Object node = null;
            bookmarkEntry = null;
            for(int i=0;i<tm.getChildCount(root);i++) {
                node = tm.getChild(root,i);
                if(tm.getChildCount(node) >2) {
                    bookmarkEntry=(BookmarkEntry)node;
                    i = tm.getChildCount(root);
                }
            }
            if(bookmarkEntry == null) {
                TestContext.registerFAILED("Can't find suitable BookmarkFolder in existing bookmarks");
                return;
            }
            TreeNode child = bookmarkEntry.getChildAt(2);
            if (child!=null) {
                TestContext.registerPASSED("Correct child was returned for valid index.");
            }else {
                TestContext.registerFAILED("NULL child was returned for valid index.");
            }
            return;
        } catch(Exception e) {
            if (e instanceof UnimplementedException) {
                TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
            }else {
                e.printStackTrace();
                TestContext.registerFAILED("getChildAt invoked with correct index, but throw exception " + e.toString());
            }
            return;
        }
    case 3:
    case 4:
        try {
            bookmarkFolder = bookmarks.newBookmarkFolder("TEST");
            Object child = bookmarkFolder.getChildAt(index);
            if(child == null) {
                TestContext.registerPASSED("getChildAt invoked with invalid index: " + index + ", return null but doesn't throw exception");
            } else {
                TestContext.registerFAILED("getChildAt invoked with invalid index: " + index + ", but return NOT null and do don't throw exception :(");
            }
            return;
        } catch(Exception e) {
            if (e instanceof UnimplementedException) {
                TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
            }else {
                e.printStackTrace();
                TestContext.registerPASSED("getChildAt invoked with invalid index: " + index + " and throw exception " + e.toString());
            }
            return;
        }
    }
}
} 

