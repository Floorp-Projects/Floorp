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

public class PlugletPeer_newStream_4  implements Test
{
   private static String targetWindow = null; 
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletPeer_newStream_4()
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
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext) " + 
				      "of PlugletPeer_newStream_4");
	   return;
	}
	if (context.failedIsRegistered()) {
	  return;
	}
	Vector vectorOfParameters = context.getParameters();
	PlugletPeer_obj = context.getPlugletPeer();
	if( PlugletPeer_obj == null ) {
	   TestContext.registerFAILED("ERROR:null PlugletPeer passed in execute(TestContext) " +
				      "of PlugletPeer_newStream_4");
	  
	   return;
	}
	targetWindow = (String)vectorOfParameters.elementAt(1);
	OutputStream rt =	PlugletPeer_obj.newStream((String)vectorOfParameters.elementAt(0),targetWindow);
	if ( rt == null ) {
	   TestContext.registerFAILED("PlugletPeer_newStream(" + (String)vectorOfParameters.elementAt(0) + 
			   "," + targetWindow + ") returns null");
	   return;
	}
	TestContext.printLog("DEBUG: PlugletPeer_newStream(" + (String)vectorOfParameters.elementAt(0) + 
			   "," + targetWindow + ") returns \"" + rt + "\" value");
	//Prepare to writing to new stream
	String inputFileName = context.getProperty("INPUT_FILE");
	String testDir = context.getTestDir();
	String fileSeparator = System.getProperty("file.separator");
	String fullName = testDir + fileSeparator + inputFileName;
	if (inputFileName == null) {
	   TestContext.registerFAILED("Property \"INPUT_FILE\" isn't set. So test can't read input file");
	   return;
	}
	FileInputStream fis = null;
	File inputFile = new File(fullName);
	int dataLength = (int)inputFile.length();
	int dataRead = 0;
	byte[] buf = new byte[dataLength];
	try {
	   fis = new FileInputStream(inputFile);
	} catch (Exception e) {
	   TestContext.registerFAILED("Can't open file for post to new stream: " + fullName);
	   e.printStackTrace();
	   return;
	}
	try{
	   while(dataRead<dataLength) 
	   {
	     dataRead += fis.read(buf,dataRead,dataLength);
	   }
	} catch (Exception e) {
	   TestContext.registerFAILED("Can't read file for post to new stream: " + fullName);
	   e.printStackTrace();
	   return;
	}
	try {
	   fis.close();
	} catch (Exception e) {
	   TestContext.registerFAILED("Can't close file for post to new stream: " + fullName);
	   e.printStackTrace();
	   return;
	}
	try {
	  rt.write(buf,0,dataLength);
	  rt.flush();
	  rt.close();
	} catch(Exception e) {
	  TestContext.registerFAILED("PlugletPeer_newStream: Error during writing to returned stream");
	  return;
	}
	//TestContext.printLog("PlugletPeer_newStream: All data writen.");
	//Result will be check via JavaScript (See checkResult for more details)
   }

   public static void checkResult(String winName, boolean isTop) 
   {
     if (targetWindow.equals("_top")&&isTop) {
       TestContext.registerPASSED("All data was posted in _top window.");
       return;
     }
     if (targetWindow.equals(winName)) {
       TestContext.registerPASSED("All data was posted in right window with name: " + winName);
     } else {
       TestContext.registerFAILED("Data was posted to the wrong window with name: " + winName +
				  " instead of: " + targetWindow);
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






