/* 
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
 */


package org.mozilla.pluglet.test.basic.api;

import java.util.*;
import java.io.*;
import java.net.*;
import org.mozilla.pluglet.test.basic.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.*;

public class PlugletManager_getURL_128  implements Test
{
   private static Pluglet         plugletInstance = null;
   private static URL                         url = null;
   private static String                   target = null;
   private static PlugletStreamListener	 listener = null;
   private static String                  altHost = null;
   private static URL                     referer = null;
   private static Boolean               JSEnabled = null;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
   */
   public PlugletManager_getURL_128()
   {
   }


   /**
    *
    ***********************************************************
    *  Starting point of application
    *
    *  @param   args    Array of command line arguments
    *
    ***********************************************************
   */
   public static void main(String[] args)
   {
   }

   /**
    ***********************************************************
    *
    *  Execute Method 
    *
    *  @param   contex  TestContext reference with properties and parameters    
    *  @return          void, the result set to true or false via context
    *
    ***********************************************************
   */
   public void execute(TestContext context)
   {	
	Properties paramProps = null;
	PlugletManager PlugletManager_obj = null;
	
	if( context == null ) {
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext) of PlugletManager_getURL_128");
	   System.err.println("ERROR:null context passed in execute(TestContext) of PlugletManager_getURL_128");
	   return;
	}
	if ( context.failedIsRegistered()) {
	   return;
	}
	Vector vectorOfParameters = context.getParameters();
	PlugletManager_obj = context.getPlugletManager();
	if( PlugletManager_obj == null ) {
	   TestContext.registerFAILED("ERROR:null PlugletManager passed in execute(TestContext) of PlugletManager_getURL_128");
	   System.err.println("ERROR:null PlugletManager passed in execute(TestContext) of PlugletManager_getURL_128");
	   return;
	}
	//Initialization of parameters
	plugletInstance = (Pluglet)vectorOfParameters.elementAt(0);
	url = (URL)vectorOfParameters.elementAt(1);
        target = (String)vectorOfParameters.elementAt(2);
	listener = (PlugletStreamListener)vectorOfParameters.elementAt(3);
	altHost = (String)vectorOfParameters.elementAt(4);
        referer = (URL)vectorOfParameters.elementAt(5);
        JSEnabled = (Boolean)vectorOfParameters.elementAt(6);

        //Call tested method
	PlugletManager_obj.getURL(plugletInstance,url,target,listener,altHost,referer,JSEnabled.booleanValue());
	
	TestContext.printLog("DEBUG: Void method PlugletManager_getURL(" +
			   plugletInstance + "," +
			   url + "," +
			   target + "," +
			   listener + "," +
			   altHost + "," +
			   referer + "," +
			   JSEnabled + ")");
   }

   public static void checkResult_target(String winName, boolean isTop) 
   {
     if (target.equals("_top")&&isTop) {
       TestContext.registerPASSED("All data was posted in _top window.");
       return;
     }
     if (target.equals(winName)) {
       TestContext.registerPASSED("All data was posted in right window with name: " + winName);
     } else {
       TestContext.registerFAILED("Data was posted to the wrong window with name: " + winName +
				  " instead of: " + target);
     }
   }

   public static void checkResult_referer(String ref) 
   {
     if (ref.equals(referer)) {
       TestContext.registerPASSED("Right HTTP_REFERER passed to CGI script: " + ref);
     } else {
       TestContext.registerFAILED("Wrong HTTP_REFERER: \"" + ref +
				  "\" instead of: \"" + referer + "\"");
     }
   }

   public static void setResult(boolean res,String comment) 
   {
     if(res) {
       TestContext.registerPASSED("Result from JavaScript: " + comment);
     } else {
       TestContext.registerFAILED("Result from JavaScript: " + comment);
     }
   }
   /**
    *
    ***********************************************************
    *  Routine where OS specific checks are made. 
    *
    *  @param   os      OS Name (SunOS/Linus/MacOS/...)
    ***********************************************************
    *
   */
   private void osRoutine(String os)
   {
     if(os == null) return;

     os = os.trim();
     if(os.compareTo("SunOS") == 0) {}
     else if(os.compareTo("Linux") == 0) {}
     else if(os.compareTo("Windows") == 0) {}
     else if(os.compareTo("MacOS") == 0) {}
     else {}
   }
}

