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

public class PlugletStreamInfo_getContentType_0  implements Test
{
   private static String contentType = null;
   public static boolean actionPerformed = false;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletStreamInfo_getContentType_0()
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
	PlugletStreamInfo PlugletStreamInfo_obj = null;
	
	if( context == null ) {
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext)" +
				      " of PlugletStreamInfo_getContentType_0");
	   return;
	}
	if ( context.failedIsRegistered()) {
	   return;
	}
	PlugletStreamInfo_obj = context.getPlugletStreamInfo();
	if(PlugletStreamInfo_obj == null) {
	   TestContext.registerFAILED("ERROR:null PlugletStreamInfo passed in execute(TestContext)" +
					   " of PlugletStreamInfo_getContentType_0");
	   return;
	}
	contentType = PlugletStreamInfo_obj.getContentType();
	actionPerformed = true;
	System.err.println("DEBUG: PlugletStreamInfo_getContentType returns \"" + contentType + "\" value");
   }
   public static void verifyContentType(String type)
   {
     if (contentType == null) {
       TestContext.registerFAILED("null contentType value returned.");
       return;
     }
     if (type.equals(contentType)) {
       TestContext.registerPASSED("PlugletStreamInfo_getContentType returns \"" + contentType + "\" value");
     } else {
       TestContext.registerFAILED("PlugletStreamInfo_getContentType returns \"" + contentType + 
				  "\" value instead of \"" + type + "\"");
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
