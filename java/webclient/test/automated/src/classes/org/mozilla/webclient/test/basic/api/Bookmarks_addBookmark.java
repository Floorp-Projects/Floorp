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
 * Bookmarks_addBookmark.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.tree.*;

public class Bookmarks_addBookmark implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Bookmarks bookmarks = null;
    private int currentArg;
    private String currentArg1;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private BookmarkEntry bookmarkEntry = null;
    private TreeModel bookmarksTree = null;
    private TreeModel bookmarksTreeOld = null;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        String input_str = context.getCurrentTestArguments();
        StringTokenizer str_tok = new StringTokenizer(input_str, ",");
        this.currentArg = new Integer(str_tok.nextToken()).intValue();
        try{
            this.currentArg1 = str_tok.nextToken();
        }catch(Exception e){
            //System.out.println("Exception catched! currentArg1=\"\"");
            this.currentArg1 = "";
        }
        if(this.currentArg1.equals("null")){
            this.currentArg1 = null; 
        }
        //this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
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
                bookmarksTreeOld = bookmarks.getBookmarks();
                //System.out.println("CurrentArg1="+currentArg1);
                if(this.currentArg1!=null){
                    if(currentArg==1){
                        bookmarkEntry = bookmarks.newBookmarkFolder(this.currentArg1);
                    }else{
                        bookmarkEntry = bookmarks.newBookmarkEntry(this.currentArg1);
                    }
                }else{
                    bookmarkEntry = null;
                }
                bookmarks.addBookmark(null, bookmarkEntry);
                bookmarksTree = bookmarks.getBookmarks();
           }catch(Exception e) {
               if(e instanceof UnimplementedException){
                  TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
                  return ;
               }
               if((e instanceof java.lang.IllegalArgumentException) && (bookmarkEntry==null)){
                  TestContext.registerPASSED("java.lang.IllegalArgumentException exception catched, but the argument is really null. Exception is: "+e.toString());
                  return ;
               }
               TestContext.registerFAILED("Exception during execution: " + e.toString());
               return;
           }
           if(getResult((TreeNode)bookmarksTreeOld.getRoot(), (TreeNode)bookmarksTree.getRoot())){
               TestContext.registerPASSED("addBookmark correct add value to a Bookmarks");
               return ;
           }else{
               TestContext.registerFAILED("AddBookmark incarrect added value to Bookmarks");
               return ;
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




