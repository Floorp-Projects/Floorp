/**
 *
 *  @version 1.00 
 *  @author Raju Pallath
 *
 *  TESTID 
 * 
 *
 */
package org.mozilla.dom.test;

import java.util.*;
import java.io.*;
import org.mozilla.dom.test.*;
import org.mozilla.dom.*;
import org.w3c.dom.*;

public class CharacterDataImpl_replaceData_int_int_String_5 extends BWBaseTest implements Execution
{

   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public CharacterDataImpl_replaceData_int_int_String_5()
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
    *  @param   tobj    Object reference (Node/Document/...)
    *  @return          true or false  depending on whether test passed or failed.
    *
    ***********************************************************
    */
   public boolean execute(Object tobj)
   {
      if (tobj == null)  {
           TestLoader.logPrint("Object is NULL...");
           return BWBaseTest.FAILED;
      }

      String os = System.getProperty("OS");
      osRoutine(os);

      Document d = (Document)tobj;
      if (d != null)
      {
             String str = "Creating a new Text Node";
             Text tn = d.createTextNode(str);
	     if (tn == null) {
                TestLoader.logErrPrint("Could not Create TextNode " + str);
                return BWBaseTest.FAILED;
             } else {
                int offset = 0;
                int count = Integer.MIN_VALUE;
                String repstr="Replaced Text";
                tn.replaceData(offset, count, repstr);

                String getstr = tn.getData();
		if (getstr == null) 
                {
                  TestLoader.logErrPrint("CharacterData should not have been null....");
                  return BWBaseTest.FAILED;
                }

System.out.println("offset is " + offset + "  count is " + count);
int x = offset + count;
System.out.println("x is " + x);
System.out.println("getstr is " + getstr);
System.out.println("str is " + str);

		if (getstr.compareTo(str) != 0)
                {
                  TestLoader.logErrPrint("CharacterData should not have been replaced....");
                  return BWBaseTest.FAILED;
                }
             }
      } else {
             System.out.println("Document is  NULL..");
             return BWBaseTest.FAILED;
      }

      return BWBaseTest.PASSED;
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
