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
 * Bookmarks_newBookmarkEntry.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;

public class Bookmarks_newBookmarkEntry implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private Bookmarks bookmarks = null;
    private String currentArg = null;
    private CurrentPage curPage = null;
    private String[] pages = null;
    private BookmarkEntry bookmarkEntry = null;

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
                bookmarkEntry = bookmarks.newBookmarkEntry(currentArg);
           }catch(Exception e) {
               if(e instanceof UnimplementedException){
                  TestContext.registerUNIMPLEMENTED("This method is unimplemented yet");
                  return ;
               }
               TestContext.registerFAILED("Exception during execution: " + e.toString());
               return ;
           }
           System.out.println("URL is: " + bookmarkEntry.getProperties().getProperty(bookmarkEntry.URL));
           System.out.println("NAME is: " + bookmarkEntry.getProperties().getProperty(bookmarkEntry.NAME));
           System.out.println("DESCRIPTION is: " + bookmarkEntry.getProperties().getProperty(bookmarkEntry.DESCRIPTION));
           try{
               if(new URL(bookmarkEntry.getProperties().getProperty(bookmarkEntry.URL)).equals(new URL(currentArg))){
                   TestContext.registerPASSED("newBookmarkEntry construct a correct BookmarkEntry, url="+bookmarkEntry.getProperties().getProperty(bookmarkEntry.URL)+" name is:"+bookmarkEntry.getProperties().getProperty(bookmarkEntry.NAME));
                   return ;
               }else{
                   TestContext.registerFAILED("newBookmarkEntry construct a incorrect BookmarkEntry");
                   return ;
               }
           }catch(MalformedURLException e){
               if((currentArg.equals(""))&&(bookmarkEntry.getProperties().getProperty(bookmarkEntry.URL).equals(""))){
                   TestContext.registerPASSED("newBookmarkEntry construct a correct BookmarkEntry, url="+bookmarkEntry.getProperties().getProperty(bookmarkEntry.URL)+" name is:"+bookmarkEntry.getProperties().getProperty(bookmarkEntry.NAME));
                   return ;
               }
               TestContext.registerFAILED("Exception when constructing URL from: " + bookmarkEntry.getProperties().getProperty(bookmarkEntry.URL) + 
                                              " and " + currentArg);
               return ;
           }
       }
       
    }  
   
}




