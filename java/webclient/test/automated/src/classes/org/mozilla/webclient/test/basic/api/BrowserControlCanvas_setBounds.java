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
 * BrowserControlCanvas_setBounds.java
 */

import org.mozilla.webclient.test.basic.*;
import org.mozilla.webclient.*;
import java.util.StringTokenizer;
import java.net.URL;
import java.net.MalformedURLException;
import java.lang.reflect.*;
import java.security.*;
import java.awt.Rectangle;


public class BrowserControlCanvas_setBounds implements Test
{
    
    private TestContext context = null;
    private BrowserControl browserControl = null;
    private BrowserControlCanvas browserCanvas = null;
    private int currentArg1 = 0;
    private int currentArg2 = 0;
    private int currentArg3 = 0;
    private int currentArg4 = 0;
    private String[] pages = null;

    public boolean initialize(TestContext context) {
        this.context = context;
        this.browserControl = context.getBrowserControl();
        this.pages = context.getPagesToLoad();
        try {
            String input_str = context.getCurrentTestArguments();
            System.out.println(input_str);
            StringTokenizer str_tok = new StringTokenizer(input_str, ",");
            this.currentArg1 = new Integer(str_tok.nextToken()).intValue();
            System.out.println("currentArg1 = "+currentArg1);
            this.currentArg2 = new Integer(str_tok.nextToken()).intValue();
            System.out.println("currentArg2 = "+currentArg2);
            this.currentArg3 = new Integer(str_tok.nextToken()).intValue();
            System.out.println("currentArg3 = "+currentArg3);
            this.currentArg4 = new Integer(str_tok.nextToken()).intValue();
            System.out.println("currentArg4 = "+currentArg4);
            browserCanvas = (BrowserControlCanvas)
                browserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
        }catch (Exception e) {
            TestContext.registerFAILED("Exception: " + e.toString());
            return false;
		}
        return true;        
    }
    public void execute() {      
       context.setDefaultResult(TestContext.PASSED);

       try{
            browserCanvas.setBounds(currentArg1, currentArg2, currentArg3, currentArg4);
            if(getResult()){
                TestContext.registerPASSED("setBounds sets bounds correctly");
            }else{
                TestContext.registerFAILED("setBounds do not sets bounds correctly");
            }
       }catch(Exception e) {
            TestContext.registerFAILED("Exception during execution: " + e.toString());
            return;
       }
       
    }
    public boolean getResult(){

       Rectangle rect_res = (Rectangle)AccessController.doPrivileged(new PrivilegedAction(){
           public Object run() {
               Rectangle rect = null;
               try {
                   final Class clazz = (Class.forName("java.awt.Component"));
                   final BrowserControlCanvas browserCanvas1 = browserCanvas;
                   Class[] param = new Class[]{};
                   final Method m = clazz.getDeclaredMethod("getBounds",param);
                   System.out.println("Method: " + m.toString());
                   m.setAccessible(true);
                   rect = (Rectangle)m.invoke(browserCanvas1,null);
               } catch(Exception e) {
                   System.out.println("Exception on INVOKE: " +e.toString());
                   e.printStackTrace();
               }
               return rect;
           }
       });
       if(rect_res==null){
           System.out.println("rect_res is null!!!!");
           return false;
       }
       double h = rect_res.getHeight();
       double w = rect_res.getWidth();
       double x = rect_res.getX();
       double y = rect_res.getY();
       System.out.println("H="+h+" W="+w+" X="+x+" Y="+y);
       if(x==currentArg1&&y==currentArg2&&w==currentArg3&&h==currentArg4){
           return true;
       }
       return false;
    }
   
}







