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

public class PlugletManager_postURL_9216  implements Test
{

   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletManager_postURL_9216()
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
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext) of PlugletManager_postURL_9216");
	   System.err.println("ERROR:null context passed in execute(TestContext) of PlugletManager_postURL_9216");
	   return;
	}
	if ( context.failedIsRegistered()) {
	   return;
	}
	Vector vectorOfParameters = context.getParameters();
	PlugletManager_obj = context.getPlugletManager();
	if( PlugletManager_obj == null ) {
	   TestContext.registerFAILED("ERROR:null PlugletManager passed in execute(TestContext) of PlugletManager_postURL_9216");
	   System.err.println("ERROR:null PlugletManager passed in execute(TestContext) of PlugletManager_postURL_9216");
	   return;
	}
	PlugletManager_obj.postURL((Pluglet)vectorOfParameters.elementAt(0),(URL)vectorOfParameters.elementAt(1),((Integer)vectorOfParameters.elementAt(2)).intValue(),(byte[])vectorOfParameters.elementAt(3),((Boolean)vectorOfParameters.elementAt(4)).booleanValue(),(String)vectorOfParameters.elementAt(5),(PlugletStreamListener)vectorOfParameters.elementAt(6),(String)vectorOfParameters.elementAt(7),(URL)vectorOfParameters.elementAt(8),((Boolean)vectorOfParameters.elementAt(9)).booleanValue(),((Integer)vectorOfParameters.elementAt(10)).intValue(),(byte[])vectorOfParameters.elementAt(11));
	System.err.println("DEBUG: Void method PlugletManager_postURL(" + "," +
			   (Pluglet)vectorOfParameters.elementAt(0) + "," +
			   (URL)vectorOfParameters.elementAt(1) + "," +
			   ((Integer)vectorOfParameters.elementAt(2)).intValue() + "," +
			   (byte[])vectorOfParameters.elementAt(3) + "," +
			   ((Boolean)vectorOfParameters.elementAt(4)).booleanValue() + "," +
			   (String)vectorOfParameters.elementAt(5) + "," +
			   (PlugletStreamListener)vectorOfParameters.elementAt(6) + "," +
			   (String)vectorOfParameters.elementAt(7) + "," +
			   (URL)vectorOfParameters.elementAt(8) + "," +
			   ((Boolean)vectorOfParameters.elementAt(9)).booleanValue() + "," +
			   ((Integer)vectorOfParameters.elementAt(10)).intValue() + "," +
			   (byte[])vectorOfParameters.elementAt(11) + ")");
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
