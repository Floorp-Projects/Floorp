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
 * BookmarkEntry_children.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.util.*;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class BookmarkEntry_children implements Test
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
  switch(currentArg) {
  case 0: 	
      try {
          try{
              bookmarkFolder = bookmarks.newBookmarkFolder("TEST");
              bookmarkEntry = bookmarks.newBookmarkEntry("Entry1");
              bookmarks.addBookmark(bookmarkFolder, bookmarkEntry);
              bookmarkEntry = bookmarks.newBookmarkEntry("Entry2");
              bookmarks.addBookmark(bookmarkFolder, bookmarkEntry);
          } catch (Exception e) {
              e.printStackTrace();
              TestContext.registerFAILED("Exception during test folder initialization: " + e.toString());
              return;
          }
          context.setDefaultResult(TestContext.FAILED);
          context.setDefaultComment("Hung up when trying to get the children of newly created folder");
          Enumeration num=bookmarkFolder.children(); 
          int count = 0;
          while(num.hasMoreElements()) {
              num.nextElement();
              count++;
          }
          if (count==2) {
              TestContext.registerPASSED("Correct Enumeration object was returned.");
          }else {
              TestContext.registerFAILED("Incorrect Enumeration object was returned.");
          }
      } catch(Exception e) {
          if (e instanceof UnimplementedException) { 
              TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
          }else {
              TestContext.registerFAILED("Exception during execution: " + e.toString());
          }
          return;
      }
      break;

    case 1:
       try {
           bookmarkEntry = bookmarks.newBookmarkEntry("Entry0");
           Enumeration num=bookmarkEntry.children();
           try {
               num.nextElement();
           }catch(NoSuchElementException e) {
               TestContext.registerPASSED("Empty Enumeration object was returned and child count is 0");
               return;
           }
           TestContext.registerFAILED("Not empty Enumeration object was returned, but child count should be 0");
           return;
       } catch(Exception e) {
           e.printStackTrace();
           if (e instanceof UnimplementedException) {
               TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           } else {
               TestContext.registerFAILED("Exception during execution: " + e.toString());
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
               if(tm.getChildCount(node) >1) {
                   bookmarkEntry=(BookmarkEntry)node;
                   i = tm.getChildCount(root);
               }
           }
           if(bookmarkEntry == null) {
               TestContext.registerFAILED("Can't find suitable BookmarkFolder in existing bookmarks");
               return;
           }
           Enumeration num=bookmarkEntry.children();
           int count = 0;
           while(num.hasMoreElements()) {
               num.nextElement();
               count++;
           }
           if (count == tm.getChildCount(node)) {
              TestContext.registerPASSED("Correct Enumeration object was returned.");
          }else {
              TestContext.registerFAILED("Incorrect Enumeration object was returned.");
          }
       } catch(Exception e) {
           if (e instanceof UnimplementedException) {
               TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
           } else {
               TestContext.registerFAILED("Exception during execution: " + e.toString());
           }
           return;
       }
       break;
   }
  
 }
} 

