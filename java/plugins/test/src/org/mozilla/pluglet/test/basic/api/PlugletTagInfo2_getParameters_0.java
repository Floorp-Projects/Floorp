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

public class PlugletTagInfo2_getParameters_0  implements Test
{
   public static boolean actionPerformed = false;
   private static Properties attrsForVerify = null;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletTagInfo2_getParameters_0()
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
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext) of PlugletTagInfo2_getParameters_0");
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
	attrsForVerify = PlugletTagInfo2_obj.getParameters();
	actionPerformed = true;
	System.err.println("PlugletTagInfo2_getParameters returns \"" + attrsForVerify + "\" value");
   }
   public static void verifyParameters(String attrString) 
   {
     if(attrString.equals("")) { //No parameters set
       if(attrsForVerify.isEmpty()) {
	   TestContext.registerPASSED("Empty properties returned when PARAMS isn't set."); 
       } else {
	   TestContext.registerFAILED("Not empty properties \"" + attrsForVerify + "\" returned when PARAMS isn't set."); 
       }
       return;
     }
     Properties attr = new Properties();
     try {
       attr.load((InputStream)new ByteArrayInputStream(attrString.getBytes()));
     } catch (Exception e) {
       TestContext.registerFAILED("Exception " + e + " ocurred");
       return;
     }
     if (attrsForVerify == null) {
       TestContext.registerFAILED("null value returned by PlugletTagInfo2_getParameters");
       return;
     }
     for (Enumeration e = attr.propertyNames();e.hasMoreElements();) {
       String pName = (String)e.nextElement();
       if ((attrsForVerify.getProperty(pName,null)) == null) {
	 TestContext.registerFAILED("Attribute \"" + pName + "\" not found in returned Parameters");
	 return;
       } else {
	 if (!attr.getProperty(pName).equals(attrsForVerify.getProperty(pName))) {
	   TestContext.registerFAILED("Values of attribute \"" + pName + "\" are different");
	   return;
	 }
       }
     }
     TestContext.registerPASSED("All Parameters transfered from JavaScript exists in Parameters, " + 
				   " returned by PlugletTagInfo2_getParameters");

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






