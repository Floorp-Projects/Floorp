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

public class  PlugletManager_postURL_Iterator implements TestListIterator
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
   public PlugletManager_postURL_Iterator()
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
      String[] paramValue = new String[12];
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
	if (paramValue[0].equals("null"))
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
	  System.err.println("PlugletManager_postURL_Iterator can't parse value \"" + paramValue[0] + "\" for Pluglet\n");
	  TestContext.registerFAILED("PlugletManager_postURL_Iterator can't parse value \"" + paramValue[0] + "\" for Pluglet\n");
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
           System.err.println("MalformedURLException thrown when compiling URL from \"" + paramValue[1] + "\" value");
	   TestContext.registerFAILED("MalformedURLException thrown when compiling URL from \"" + paramValue[1] + "\".Wrong parameters file? ");
        }
        vectorOfParameters.addElement(result);

        // Parameter number 2,parameter type is int
        try {
           if (paramValue[2].equals("Integer.MIN_VALUE"))
           {
              result = (Object)(new Integer(Integer.MIN_VALUE));
           } else if (paramValue[2].equals("Integer.MAX_VALUE"))
           {
              result = (Object)(new Integer(Integer.MAX_VALUE));
           } else if (paramValue[2].equals("0"))
           {
              result = (Object)(new Integer(0));
           } else 
           {
              result = (Object)(new Integer(paramValue[2]));
           }
        }catch(NumberFormatException  e)
        {
           System.err.println("NumberFormatException thrown when compiling Integer value. Wrong parameter");
	        TestContext.registerFAILED("NumberFormatException thrown.Wrong parameters file?");
        }
        vectorOfParameters.addElement(result);

        //Please hack this block.
        // Parameter number 3,parameter type is Arr1byte
        result = (Object)(paramValue[3].equals("null") ? null : readByteArray(paramValue[3]));
        vectorOfParameters.addElement(result);

        // Parameter number 4,parameter type is boolean
        result = (Object)(new Boolean(paramValue[4]));  
        vectorOfParameters.addElement(result);

        // Parameter number 5,parameter type is String
        result = (Object)(paramValue[5].equals("null") ? null : paramValue[5] );
        vectorOfParameters.addElement(result);

        //Please hack this block.
        // Parameter number 6,parameter type is PlugletStreamListener
        //result = (Object)(paramValue[6].equals("null") ? null : context.getPlugletStreamListener());
	if (paramValue[6].equals("null"))
	{
	  result = (Object)(null);
	} else if (paramValue[6].equals("current_streamListener")||paramValue[3].equals("NOTNULL"))
	{
	  result = (Object)(context.getPlugletStreamListener());
	} else if (paramValue[6].equals("new_streamListener"))
	{
	  PlugletStreamListener newPlugletStreamListener = new TestStreamListener(context);
	  //context.addNewPlugletStreamListener(newPlugletStreamListener);
	  System.err.println("WARNING:Currently test doesn't know how work with newPlugletStreamListener\n");
	  // TestContext.registerFAILED("Currently test doesn't know how work with newPlugletStreamListener\n");
	  result = (Object)(newPlugletStreamListener);
	} else 
	{
	  System.err.println("PlugletManager_postURL_Iterator can't parse \"" + paramValue[6] + "\" value for PlugletStreamListener\n");
	  TestContext.registerFAILED("PlugletManager_postURL_Iterator can't parse \"" + paramValue[6] + "\" value for PlugletStreamListener\n");
	}
        vectorOfParameters.addElement(result);

        // Parameter number 7,parameter type is String
        result = (Object)(paramValue[7].equals("null") ? null : paramValue[7] );
        vectorOfParameters.addElement(result);

        // Parameter number 8,parameter type is URL
        try {
           if (paramValue[8].equals("null"))
           {
              result = (Object)(null);
           } else if (paramValue[8].equals("NOTNULL"))
           {
	      System.err.println("Value NOTNULL not used NOW.Please change parameters file.\n");
	      TestContext.registerFAILED("Value NOTNULL not used NOW.Please change parameters file.\n");
              //result = (Object)(new URL("http://www.mozilla.org"));
           } else 
           {
              result = (Object)(new URL(paramValue[8]));
           }
        }catch(MalformedURLException e)
        {
           System.err.println("MalformedURLException thrown when compiling URL from \"" + paramValue[8] + "\" value");
	        TestContext.registerFAILED("MalformedURLException thrown when compiling URL from \"" + paramValue[8] + "\".Wrong parameters file? ");
        }
        vectorOfParameters.addElement(result);

        // Parameter number 9,parameter type is boolean
        result = (Object)(new Boolean(paramValue[9]));  
        vectorOfParameters.addElement(result);

        // Parameter number 10,parameter type is int
        try {
           if (paramValue[10].equals("Integer.MIN_VALUE"))
           {
              result = (Object)(new Integer(Integer.MIN_VALUE));
           } else if (paramValue[10].equals("Integer.MAX_VALUE"))
           {
              result = (Object)(new Integer(Integer.MAX_VALUE));
           } else if (paramValue[10].equals("0"))
           {
              result = (Object)(new Integer(0));
           } else 
           {
              result = (Object)(new Integer(paramValue[10]));
           }
        }catch(NumberFormatException  e)
        {
	   TestContext.registerFAILED("NumberFormatException thrown.Wrong parameters file?");
	   return false;
        }
        vectorOfParameters.addElement(result);

        //Please hack this block.
        // Parameter number 11,parameter type is Arr1byte
        result = (Object)(paramValue[11].equals("null") ? null : readByteArray(paramValue[11]));
        vectorOfParameters.addElement(result);
	context.setParameters(vectorOfParameters);
	return true;
      }
      return false;
   }

  
   private byte[] readByteArray(String fileName)
   {
      FileInputStream fis = null;
      byte cur;
      String separator = System.getProperty("file.separator");
      String fullName = context.getTestDir() + separator + fileName;
      Vector arrVect = new Vector();
      try {
	fis = new FileInputStream(fullName);
      } catch (Exception e) {
	TestContext.registerFAILED("Can't open file with Byte Array: " + fullName);
	return null;
      }
      try{
	while((cur = (byte)fis.read())!=-1) 
	{
	  arrVect.addElement((Object)(new Byte(cur)));
	}
      } catch (Exception e) {
	TestContext.registerFAILED("Can't read from file with Byte Array: " + fullName);
	return null;
      }
      try {
        fis.close();
      } catch (Exception e) {
	TestContext.registerFAILED("Can't close file with Byte Array: " + fullName);
	return null;
      }
      byte[] byteArr = new byte[arrVect.size()];
      for(int i=0;i<arrVect.size();i++)
      {
	byteArr[i] = ((Byte)arrVect.elementAt(i)).byteValue();
      }
      return byteArr;  
   }
    
}

