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

public class PlugletPeer_setWindowSize_9  implements Test
{
   private static int expectedX=-1;
   private static int expectedY=-1;
   public static boolean actionPerformed = false;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletPeer_setWindowSize_9()
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
	PlugletPeer PlugletPeer_obj = null;
	
	if ( context == null ) {
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext)" +
				      " of PlugletPeer_setWindowSize_9");
	   return;
	}
	if ( context.failedIsRegistered() ) {
	   return;
	}
	Vector vectorOfParameters = context.getParameters();
	PlugletPeer_obj = context.getPlugletPeer();
	if ( PlugletPeer_obj == null ) {
	   TestContext.registerFAILED("ERROR:null PlugletPeer passed in execute(TestContext)" +
				      " of PlugletPeer_setWindowSize_9");
	   return;
	}
	expectedX=((Integer)vectorOfParameters.elementAt(0)).intValue();
	expectedY=((Integer)vectorOfParameters.elementAt(1)).intValue();
	TestContext.printLog("Call Void method PlugletPeer_setWindowSize(" + expectedX + "," + expectedY + ")");
	PlugletPeer_obj.setWindowSize(expectedX,expectedY);
	actionPerformed = true;
   }
  /**
    *
    ***********************************************************
    *  This routine called from JavaScript. 
    *
    *  @param curX, curY - current values for window outher size
    ***********************************************************
    *
    */
   public static void verifySize(int curX,int curY) {

     TestContext.printLog("After test execution: windowX = " + curX + " windowY = " + curY + "\n");
     if ((expectedX == curX) && (expectedY == curY)) {
       TestContext.registerPASSED("PlugletPeer.setWindowSize passed\n");
     } else {
       TestContext.registerFAILED("PlugletPeer.setWindowSize failed\n");
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
