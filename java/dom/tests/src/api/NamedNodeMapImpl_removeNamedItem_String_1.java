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

public class NamedNodeMapImpl_removeNamedItem_String_1 extends BWBaseTest implements Execution
{

   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public NamedNodeMapImpl_removeNamedItem_String_1()
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
         String attrName = "TARGET";
         Attr a = d.createAttribute(attrName);
         if (a == null) {
             TestLoader.logErrPrint("Could not reate Attribute " + attrName); 
             return BWBaseTest.FAILED;
         }

         String aelement = "BASE";
         String attrValue = "";
         Element e = d.createElement(aelement);
         if (e != null)
         {
            a.setValue(attrValue);
            e.setAttributeNode(a);
         } else {
            TestLoader.logErrPrint("Element " + aelement + " could not be created..."); 
            return BWBaseTest.FAILED;
         }

         NamedNodeMap map = e.getAttributes();
         if (map != null) 
         {
             Node n  = map.removeNamedItem(attrName);
             if (n == null)
             {
                TestLoader.logErrPrint(" Could not remove Item " + attrName);
                return BWBaseTest.FAILED;
             }
         } else {
             TestLoader.logErrPrint(" Could not find Attriblist for node  " + aelement);
             return BWBaseTest.FAILED;
         } 
     } else {
         TestLoader.logErrPrint("Document is  NULL..");
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
