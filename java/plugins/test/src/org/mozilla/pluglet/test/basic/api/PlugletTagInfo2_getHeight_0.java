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

public class PlugletTagInfo2_getHeight_0  implements Test
{
   private static int height = -1;
   public static boolean actionPerformed = false;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletTagInfo2_getHeight_0()
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
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext) of PlugletTagInfo2_getHeight_0");
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

	height = PlugletTagInfo2_obj.getHeight();
	actionPerformed = true;
	System.err.println("PlugletTagInfo2_getHeight returns \"" + height + "\" value");
   }
   public static void verifyHeight(int realHeight) 
   {
     if (height == realHeight) {
       TestContext.registerPASSED("Good value \"" + height + 
				  "\" was returned by PlugletTagInfo2_getHeight");
     } else {
       TestContext.registerFAILED("PlugletTagInfo2_getHeight returns \"" + height + "\" value " +
				     "instead of \"" + realHeight + "\"");
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
