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

public class PlugletPeer_getValue_3  implements Test
{

   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletPeer_getValue_3()
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
	
	if( context == null ) {
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext)" +
				      " of PlugletPeer_getValue_3");
	   return;
	}
	Vector vectorOfParameters = context.getParameters();
	if( vectorOfParameters == null ) {
	   TestContext.registerFAILED("ERROR:null parameters vector passed in execute(TestContext)" +
				      " of PlugletPeer_getValue_3");
	   return;
	}
	PlugletPeer_obj = context.getPlugletPeer();
	if( PlugletPeer_obj == null ) {
	   TestContext.registerFAILED("ERROR:null PlugletPeer passed in execute(TestContext)" +
				      " of PlugletPeer_getValue_3");
	   return;
	}
        String variables = context.getProperty("VARIABLES");
        System.err.println("Variables is \"" + variables + "\"");
	StringTokenizer st  = new StringTokenizer(variables,",");
        int[] goodVars = new int[st.countTokens()];
	for(int i=0;i<goodVars.length;i++) {
	  goodVars[i] = (new Integer(st.nextToken().trim())).intValue();
	  System.err.println("Goodvars  is \"" + goodVars[i] + "\"");
	}
	int var = ((Integer)vectorOfParameters.elementAt(0)).intValue();
	String rt =	PlugletPeer_obj.getValue(var);
	for(int i=0;i<goodVars.length;i++) {
	  if(var == goodVars[i]) {
	    TestContext.registerFAILED("PlugletPeer_getValue(" + var + ") returns \"" + rt + 
				       "\" value\n Test is't calibrated for real variables.");
	    return;
	  }
	}
	TestContext.registerPASSED("getValue doesn't crashed on illegal value \"" + var + 
				   "\" and returns \"" + rt + "\" value");
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
