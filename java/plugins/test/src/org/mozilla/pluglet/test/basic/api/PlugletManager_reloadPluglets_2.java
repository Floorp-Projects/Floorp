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
import java.security.*;
import org.mozilla.pluglet.test.basic.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.*;
import java.lang.reflect.*;

public class PlugletManager_reloadPluglets_2  implements Test
{
   private static String destPluglet = "unset"; 
   public static Vector msgVect = new Vector();
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletManager_reloadPluglets_2()
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
	   TestContext.registerFAILED("ERROR:null context passed in execute(TestContext) of PlugletManager_reloadPluglets_2");
	   return;
	}
	if ( context.failedIsRegistered()) {
	   return;
	}
	Vector vectorOfParameters = context.getParameters();
	PlugletManager_obj = context.getPlugletManager();
	if( PlugletManager_obj == null ) {
	   TestContext.registerFAILED("ERROR:null PlugletManager passed in execute(TestContext) of PlugletManager_reloadPluglets_2");
	   return;
	}
	String destDir = context.getProperty("PLUGLET_DIR");
	String sourceDir = context.getTestDir();
	String fileName = context.getProperty("SECOND_PLUGLET_FILE");
	TestContext.printLog("Delete Applet says{ " );
	for(int i=0;i<msgVect.size();i++) {
	  TestContext.printLog((String)msgVect.elementAt(i));
	}
	TestContext.printLog("}");
	if(((Boolean)copySecondPluglet(sourceDir,destDir,fileName)).booleanValue()) {
	  context.printLog("Second pluglet library was succesfully copied..");
	  PlugletManager_obj.reloadPluglets(((Boolean)vectorOfParameters.elementAt(0)).booleanValue());
	  System.err.println("DEBUG: Void method PlugletManager_reloadPluglets(" + ((Boolean)vectorOfParameters.elementAt(0)).booleanValue() + ")"); 
	  return;
	}
	TestContext.registerFAILED("Something wrong when copying new pluglet library.\n" +
				   " Possible second pluglet jar file already exist. \n" +
				   "Remove it and restart test\n");
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
   private Object copySecondPluglet(String sDir, String dDir, String fName)
   {
     final String sourceDir = sDir;
     final String destDir = dDir;
     final String fileName = fName;
     return AccessController.doPrivileged(
	new PrivilegedAction() {
	  public Object run() {
	    String separator = System.getProperty("file.separator");
	    try {
	      File outputFile = new File(destDir + separator + fileName);
	      File inputFile = new File(sourceDir + separator + fileName);
	      int bl = (int)inputFile.length();
	      if (outputFile.exists()) {
		TestContext.printLog("SecondPluglet already exist in %PLUGLET% dir");
		return (Object)(new Boolean(false));
	      }
	      // outputFile.deleteOnExit();
	      byte buf[] = new byte[bl];
	      System.err.println(destDir + separator + fileName);
	      FileInputStream fr = new FileInputStream(inputFile);
	      FileOutputStream fw = new FileOutputStream(outputFile);
	      if(fr.read(buf)<bl) {
		TestContext.printLog("WARNING: Not all data copied to new jar");
	      }
	      fw.write(buf);
	      fw.flush();
	      fr.close();
	      fw.close();
	      return (Object)(new Boolean(true));
	    } catch (IOException e) {
	      e.printStackTrace();
	      return (Object)(new Boolean(false));
	    } 
	  }
     });
   }
}












