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

public class PlugletTagInfo2_getParameter_2  implements Test
{
   private static String paramName = null;
   private static String paramVal = null;
   public static boolean actionPerformed = false;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletTagInfo2_getParameter_2()
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
        PlugletTagInfo2 PlugletTagInfo2_obj = null;
	if( context == null ) {
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext)" +
				      " of PlugletTagInfo2_getParameter_2");
	   return;
	}
	Vector vectorOfParameters = context.getParameters();
	PlugletTagInfo PlugletTagInfo_obj = context.getPlugletPeer().getTagInfo();
	if (PlugletTagInfo_obj instanceof PlugletTagInfo2) {
          PlugletTagInfo2_obj = (PlugletTagInfo2)PlugletTagInfo_obj;
        } else {
          TestContext.registerFAILED("Instance of PlugletTagInfo instead of PlugletTagInfo2 returned");
          return;
        }
	paramName = (String)vectorOfParameters.elementAt(0);
	paramVal = PlugletTagInfo2_obj.getParameter(paramName);
	actionPerformed = true;
	System.err.println("PlugletTagInfo2_getParameter(" + paramName + ") returns \"" + paramVal + "\" value");
   }
   public static void verifyParameter(String paramString) 
   {
     Properties paramReal = new Properties();
     if (paramName == null) {
       if (paramVal == null) {
	 TestContext.registerPASSED("Pluglet doesn't crashed when call getParameter(null) and return null value");
       } else {
	 TestContext.registerFAILED("Pluglet doesn't crashed when call getParameter(null)" +
				    " but return not null value + \"" + paramVal + "\"");
       }
       return;
     }
     try {
       paramReal.load((InputStream)new ByteArrayInputStream(paramString.getBytes()));
     } catch (Exception e) {
       TestContext.registerFAILED("Exception " + e + " ocurred");
       return;
     }
     if (paramReal.getProperty(paramName,null) == null) {
       if (paramVal == null) {
	 TestContext.registerPASSED("null value returned for unexisting Parameter " + paramName);
       } else {
	 TestContext.registerFAILED("Not null value returned for unexisting Parameter " + paramName);
       }
       return;
     }
     if (paramReal.getProperty(paramName,null).equals(paramVal)) {
       TestContext.registerPASSED(paramVal + " value returned for existing Parameter " + paramName);
     } else {
       TestContext.registerFAILED("Wrong value " + paramVal + " returned for Parameter " + paramName +
				  " .Good value is " + paramReal.getProperty(paramName,null));
     }
     return;
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





