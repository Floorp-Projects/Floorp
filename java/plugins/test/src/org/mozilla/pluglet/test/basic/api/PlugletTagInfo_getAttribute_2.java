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

public class PlugletTagInfo_getAttribute_2  implements Test
{
   private static String attrName = null;
   private static String attrVal = null;
   public static boolean actionPerformed = false;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletTagInfo_getAttribute_2()
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
	if( context == null ) {
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext)" +
				      " of PlugletTagInfo_getAttribute_2");
	   return;
	}
	Vector vectorOfParameters = context.getParameters();
	PlugletTagInfo PlugletTagInfo_obj = context.getPlugletPeer().getTagInfo();
	attrName = (String)vectorOfParameters.elementAt(0);
	attrVal = PlugletTagInfo_obj.getAttribute(attrName);
	actionPerformed = true;
	System.err.println("PlugletTagInfo_getAttribute(" + attrName + ") returns \"" + attrVal + "\" value");
   }
   public static void verifyAttribute(String attrString) 
   {
     Properties attrReal = new Properties();
     if (attrName == null) {
       if (attrVal == null) {
	 TestContext.registerPASSED("Pluglet doesn't crashed when call getAttribute(null) and return null value");
       } else {
	 TestContext.registerFAILED("Pluglet doesn't crashed when call getAttribute(null)" +
				    " but return not null value + \"" + attrVal + "\"");
       }
       return;
     }
     try {
       attrReal.load((InputStream)new ByteArrayInputStream(attrString.getBytes()));
     } catch (Exception e) {
       TestContext.registerFAILED("Exception " + e + " ocurred");
       return;
     }
     if (attrReal.getProperty(attrName,null) == null) {
       if (attrVal == null) {
	 TestContext.registerPASSED("null value returned for unexisting attribute " + attrName);
       } else {
	 TestContext.registerFAILED("Not null value returned for unexisting attribute " + attrName);
       }
       return;
     }
     if (attrReal.getProperty(attrName,null).equals(attrVal)) {
       TestContext.registerPASSED(attrVal + " value returned for existing attribute " + attrName);
     } else {
       TestContext.registerFAILED("Wrong value " + attrVal + " returned for attribute " + attrName +
				  " .Good value is " + attrReal.getProperty(attrName,null));
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





