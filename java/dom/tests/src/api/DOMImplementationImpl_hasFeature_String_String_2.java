/**
 *
 *  @version 1.00 
 *  @author Raju Pallath
 *
 *  TESTID 
 * This is an Auto-generated Test Case.
 *
 */
package org.mozilla.dom.test;

import java.util.*;
import java.io.*;
import org.mozilla.dom.test.*;
import org.mozilla.dom.*;
import org.w3c.dom.*;

public class DOMImplementationImpl_hasFeature_String_String_2 extends BWBaseTest implements Execution
{

   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public DOMImplementationImpl_hasFeature_String_String_2()
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
           TestLoader.logErrPrint("Object is NULL...");
           return BWBaseTest.FAILED;
      }

      String os = System.getProperty("OS");
      osRoutine(os);

      Document d = (Document)tobj;
        if (d != null)
        {
             DOMImplementation di = d.getImplementation();
	     if (di == null) {
                TestLoader.logErrPrint("Document DomImplementation is  NULL..");
                return BWBaseTest.FAILED;
             } else {
               if (di.hasFeature(null, "1.0"))
               {
                  System.out.println("DomImplementation 'hasFeature(null, 1.0)'  Failed..");
                  return BWBaseTest.FAILED;
               }
             }
        } else {
             System.out.println("Doument is  NULL..");
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
