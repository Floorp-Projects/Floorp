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

public class  PlugletManager_getURL_Iterator implements TestListIterator
{
   Vector combstr = new Vector();
   int curElement = 0;
   Properties testProps = null;
   TestContext context = null;

   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletManager_getURL_Iterator()
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
    *  Initialize Method 
    *
    *  This method initialize parameters array and prepare for
    *  access parameters via nextParams method
    *  
    *  @param   contex  TestContext reference with properties and parameters    
    *  @return          nothing
    *
    ***********************************************************
    */
   public boolean initialize(TestContext context)
   {
        this.context = context;    
        BufferedReader br = null;
	String paramString = new String("");
	String fileName = context.getProperty("LST_FILE_NAME");
	String separator = System.getProperty("file.separator");
	String paramFile = context.getTestDir() + separator + fileName;
	try {
	   br = new BufferedReader(new InputStreamReader(new FileInputStream(paramFile)));
	} catch (Exception e) {
	   TestContext.registerFAILED("Can't open parameters file: " + paramFile);
	   return false;
	}
	try{
	   while((paramString = br.readLine())!= null) 
	   {
	      combstr.addElement(paramString);
	   }
	} catch (Exception e) {
	   TestContext.registerFAILED("Can't read from parameters file: " + paramFile);
	   return false;
	}
	try {
	   br.close();
	} catch (Exception e) {
	   TestContext.registerFAILED("Can't close parameters file: " + paramFile);
	   return false;
	}   
        return true;
   }
   public boolean next()
   { 
      String[] paramValue = new String[7];
      Vector vectorOfParameters = new Vector();
      if (context == null) 
      {
	System.err.println("context not initialized" );
	TestContext.registerFAILED("context not initialized");
	return false;

      }
      if(curElement <combstr.size())
      { 
	int i = 0;
	int last = 0;
	int cur = 0;
	String pstr = (String)combstr.elementAt(curElement++);
        while((cur = pstr.indexOf(",",last)) != -1) {
           paramValue[i] = pstr.substring(last,cur);
           paramValue[i] = paramValue[i].trim();
           //testProps.setProperty("param" + (new Integer(i)).toString(),pstr.substring(last,cur));
	   i++;
	   last = cur +1;
	}
	//For add last value
	paramValue[i] = pstr.substring(last,pstr.length());
        paramValue[i] = paramValue[i].trim();
        Object result = null;

        //Please hack this block.
        // Parameter number 0,parameter type is Pluglet
        //result = (Object)(paramValue[0].equals("null") ? null : context.getPluglet());
	if (paramValue[0].equals("null_Pluglet"))
	{
	  result = (Object)(null);
	} else if (paramValue[0].equals("this_Pluglet"))
	{
          result = (Object)(context.getPluglet());
        } else if (paramValue[0].equals("other_Pluglet"))
        {
          result = (Object)(context.getOtherPluglet());
	} else 
	{
	  System.err.println("PlugletManager_getURL_Iterator can't parse value \"" + paramValue[0] + "\" for Pluglet\n");
	}
        vectorOfParameters.addElement(result);

        // Parameter number 1,parameter type is URL
        try {
           if (paramValue[1].equals("null"))
           {
              result = (Object)(null);
           } else if (paramValue[1].equals("NOTNULL"))
           {
	     System.err.println("Value NOTNULL not used NOW.Please change parameters file.\n");
	     TestContext.registerFAILED("Value NOTNULL not used NOW.Please change parameters file.\n");
	     //result = (Object)(new URL("http://www.mozilla.org"));
           } else 
           {
              result = (Object)(new URL(paramValue[1]));
           }
        }catch(MalformedURLException e)
        {
           System.err.println("MalformedURLException thrown when compiling URL from \"" + paramValue[1] + "\" String value ");
	   TestContext.registerFAILED("MalformedURLException thrown when compiling URL from \"" + paramValue[1] + "\" .Wrong parameters file? ");
        }
        vectorOfParameters.addElement(result);

        // Parameter number 2,parameter type is String
        result = (Object)(paramValue[2].equals("null") ? null : paramValue[2] );
        vectorOfParameters.addElement(result);

        //Please hack this block.
        // Parameter number 3,parameter type is PlugletStreamListener
        //result = (Object)(paramValue[3].equals("null") ? null : context.getPlugletStreamListener());
	if (paramValue[3].equals("null_streamListener"))
	{
	  result = (Object)(null);
	} else if (paramValue[3].equals("current_streamListener")||paramValue[3].equals("NOTNULL"))
	{
	  result = (Object)(context.getPlugletStreamListener());
	} else if (paramValue[3].equals("new_streamListener"))
	{
	  PlugletStreamListener newPlugletStreamListener = new TestStreamListener(context);
	  //context.addNewPlugletStreamListener(newPlugletStreamListener);
	  result = (Object)(newPlugletStreamListener);
	  System.err.println("WARNING: Currently test doesn't know how work with newPlugletStreamListener\n");
	  //TestContext.registerFAILED("Currently test doesn't know how work with newPlugletStreamListener\n");
	} else 
	{
	  System.err.println("PlugletManager_getURL_Iterator can't parse \"" + paramValue[3] + "\" value for PlugletStreamListener\n");
	  TestContext.registerFAILED("PlugletManager_getURL_Iterator can't parse \"" + paramValue[3] + "\" value for PlugletStreamListener\n");
	}

        vectorOfParameters.addElement(result);

        // Parameter number 4,parameter type is String
        result = (Object)(paramValue[4].equals("null") ? null : paramValue[4] );
        vectorOfParameters.addElement(result);

        // Parameter number 5,parameter type is URL
        try {
           if (paramValue[5].equals("null"))
           {
              result = (Object)(null);
           } else if (paramValue[5].equals("NOTNULL"))
           {
	      System.err.println("Value NOTNULL not used NOW.Please change parameters file.\n");
	      TestContext.registerFAILED("Value NOTNULL not used NOW.Please change parameters file.\n");
              //result = (Object)(new URL("http://www.mozilla.org"));
           } else 
           {
              result = (Object)(new URL(paramValue[5]));
           }
        }catch(MalformedURLException e)
        {
           System.err.println("MalformedURLException thrown when compiling URL from \"" + paramValue[5] + "\" value");
	        TestContext.registerFAILED("MalformedURLException thrown when compiling URL from \"" + paramValue[5] + "\".Wrong parameters file? ");
        }
        vectorOfParameters.addElement(result);

        // Parameter number 6,parameter type is boolean
        result = (Object)(new Boolean(paramValue[6]));  
        vectorOfParameters.addElement(result);
	context.setParameters(vectorOfParameters);
	return true;
      }
      return false;
   }
}






