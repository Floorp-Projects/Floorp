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
 * Navigation_loadFromStream.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.io.InputStream;
import java.util.Properties;
import java.io.ByteArrayInputStream;
import java.net.URL;
import java.net.MalformedURLException;


public class Navigation_loadFromStream implements Test
{
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private History history = null;
    private Navigation navigation = null;
    private int currentArg = 0;
    private String[] pages = null;


    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        this.currentArg = (new Integer(context.getCurrentTestArguments())).intValue();
        try {
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);

            history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);

        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        return true;        
    }


    public void execute() {
        InputStream is = null;
        String contentType = "text/html";
        String uri = "www.sun.com";
        int contentLength = -1;
        Properties props = null;
        byte[] buf = {'1','2','3','4','5'};
        System.out.println("Navigation_loadFromStream. Execution count is " + context.getExecutionCount());
        if (navigation == null) {
            TestContext.registerFAILED("Navigation is null at execution stage");
            return;
        }
        if(context.getExecutionCount() > 1) {
            TestContext.registerPASSED("Browser doesn't crashed, some data loaded");
            return;
        }
        switch (currentArg) {
        case 0: 
            try {
                context.setDefaultResult(TestContext.FAILED);
                context.setDefaultComment("Hang up when try to call loadFromStream with null stream");
                navigation.loadFromStream(is,uri,contentType,contentLength,props);
            } catch(Exception e) {
              if(e instanceof UnimplementedException)
                  TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
              else
                  TestContext.registerPASSED("Browser doesn't crashed when try to invoke loadFromStream with null stream (exception): " + e.toString());
              return;
          }; 
          break;
    
        case 1: 
            try {
                context.setDefaultResult(TestContext.FAILED);
                context.setDefaultComment("Hung up when try to invoke loadFromStream with null ContentType");
                
                contentType=null;
                is = new ByteArrayInputStream(buf);
                navigation.loadFromStream(is,uri,contentType,contentLength,props);
            }catch(Exception e) {
                if(e instanceof UnimplementedException)
                    TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
                else
                    TestContext.registerPASSED("Browser doesn't crashed when try to invoke loadFromStream with null contentType (exception): " + e.toString());
                return;
            };
            break;
        case 2: 
            try {
                context.setDefaultResult(TestContext.FAILED);
                context.setDefaultComment("Hung up when try to invoke loadFromStream with null uri");
                uri=null;
                is = new ByteArrayInputStream(buf);
                navigation.loadFromStream(is,uri,contentType,contentLength,props);
            }catch(Exception e) {
                if(e instanceof UnimplementedException)
                    TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
                else
                    TestContext.registerPASSED("Browser doesn't crashed when try to invoke loadFromStream with null URI (exception): " + e.toString());
                return;
            };
            break;
           
        case 3:
            //Valid usage ...
            try {
                context.setDefaultResult(TestContext.FAILED);
                context.setDefaultComment("Hung up when try to invoke loadFromStream with valid parameters");
                uri="file://dummy_test_data";
                is = new ByteArrayInputStream(buf);
                navigation.loadFromStream(is,uri,contentType,contentLength,props);
            }catch(Exception e) {
                if(e instanceof UnimplementedException)
                    TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
                else
                    TestContext.registerFAILED("Method throw exception: " + e.toString());
                return;
            };
            break; 
            
        case 4:
            //Valid usage ...
            try {
                context.setDefaultResult(TestContext.FAILED);
                context.setDefaultComment("Hung up when try to invoke loadFromStream with empty properties");
                props = new Properties();
                uri="file://dummy_test_data";
                is = new ByteArrayInputStream(buf);
                navigation.loadFromStream(is,uri,contentType,contentLength,props);
            }catch(Exception e) {
                if(e instanceof UnimplementedException)
                    TestContext.registerUNIMPLEMENTED("This method doesn't implemented");
                else
                    TestContext.registerFAILED("Method throw exception: " + e.toString());
                return;
            };
            break; 
        };
        return;
  
    }  
}






