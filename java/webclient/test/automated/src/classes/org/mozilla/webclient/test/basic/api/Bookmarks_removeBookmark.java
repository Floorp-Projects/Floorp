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
 * Bookmarks_removeBookmark.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class Bookmarks_removeBookmark implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Bookmarks bookmarks = null;
    private String currentArg;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private BookmarkEntry bookmarkEntry = null;
    private TreeModel bookmarksTree = null;
    private TreeModel bookmarksTreeOld = null;

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
       if(context.getExecutionCount()==1){
           try {
                if(currentArg.indexOf("exist")!=-1){
                    bookmarkEntry = bookmarks.newBookmarkEntry("http://java.sun.com/");
                    bookmarks.addBookmark(null, bookmarkEntry);
                    bookmarksTree = bookmarks.getBookmarks();
                    bookmarks.removeBookmark(bookmarkEntry);
                    bookmarksTreeOld = bookmarks.getBookmarks();
                }
                if(currentArg.indexOf("unexist")!=-1){
                    bookmarkEntry = bookmarks.newBookmarkEntry("http://java.sun.com/");
                    bookmarksTree = bookmarks.getBookmarks();
                    bookmarks.removeBookmark(bookmarkEntry);
                    bookmarksTreeOld = bookmarks.getBookmarks();
                }
                if(currentArg.indexOf("null")!=-1){
                    bookmarksTree = bookmarks.getBookmarks();
                    bookmarks.removeBookmark(null);
                    bookmarksTreeOld = bookmarks.getBookmarks();
                }
           }catch(Exception e) {
               if(e instanceof UnimplementedException){
                  TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
                  return ;
               }
               if((e instanceof java.lang.IllegalArgumentException) && (currentArg.indexOf("null")!=-1)){
                  TestContext.registerPASSED("java.lang.IllegalArgumentException catched, but the argument is null! Exception is: " + e.toString());
                  return ;
               }
               TestContext.registerFAILED("Exception during execution: " + e.toString());
               return;
           }
           if(!currentArg.equals("exist")){
               if(bookmarksTree.equals(bookmarksTreeOld)){
                   System.out.println("oldNode equals node - true");
                   TestContext.registerPASSED("removeBookmark correct remove "+this.currentArg+" value to a Bookmarks");
                   return ;
               }else{
                   System.out.println("oldNode not equals node - false");
                   TestContext.registerFAILED("removeBookmark incorrect remove "+this.currentArg+" value to Bookmarks");
                   return ;
               }
           }else{
               if(getResult((TreeNode)bookmarksTreeOld.getRoot(), (TreeNode)bookmarksTree.getRoot())){
                   TestContext.registerPASSED("removeBookmark correct remove "+this.currentArg+" value to a Bookmarks");
                   return ;
               }else{
                   TestContext.registerFAILED("removeBookmark incorrect remove "+this.currentArg+" value to Bookmarks");
                   return ;
               }
           }
       }
       
    }
    private boolean getResult(TreeNode oldNode, TreeNode node) {
        int oldCount = bookmarksTreeOld.getChildCount(bookmarksTreeOld.getRoot());
        int count = node.getChildCount();
        TreeNode currNode = null;
        BookmarkEntry oldEntry = null;
        BookmarkEntry newEntry = null;
        boolean result = false;
        for(int i=0; i<count; i++){
            currNode = node.getChildAt(i);
            if(oldNode.getIndex(currNode)!=-1){
                oldEntry = (BookmarkEntry)oldNode.getChildAt(oldNode.getIndex(currNode));
                newEntry = (BookmarkEntry)currNode;
                if(!newEntry.isFolder()){
                    if(!oldEntry.URL.equals(newEntry.URL)){
                        System.out.println("Node is founded on oldTree but not equals currNode and i="+i);
                        result = false;
                        return false;
                    }else{
                        result  = true;
                    }
                }else{
                    result = getResult(oldNode.getChildAt(oldNode.getIndex(currNode)),currNode);
                    if(!result){
                        System.out.println("Failed because recursive is failed");
                        return false;
                    }
                }
            }else{
                if(currNode.equals(bookmarkEntry)){
                    result = true;
                }else{
                    result = false;
                    System.out.println("Return false because currNode is not equal bookmarkNode and i="+i);
                    return false;
                }
            }
        }
        return true;
    }
   
}




