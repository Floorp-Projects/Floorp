/**
 *
 *  @version 1.00 
 *  @author Raju Pallath
 *
 *  TESTID 
 * 
 *   This class is used to just store the parent Document of a page on Mozilla
 *   apprunner for reference in other classes.
 *
 */
package org.mozilla.dom.test;

import java.util.*;
import java.io.*;
import org.mozilla.dom.test.*;
import org.mozilla.dom.*;
import org.w3c.dom.*;

public class BWStaticDoc extends BWBaseTest
{

   /**
    ********************************************************
    *
    *  Get the Parent Document
    *
    *  @return         Parent Document Reference
    *
    ********************************************************
    */
   public static Document getParentDoc()
   {
         return pageDoc;
   }

   /**
    ********************************************************
    *
    *  Set the Parent Document
    *
    *  @params   doc   Document to be set.
    *
    ********************************************************
    */
   public static void setParentDoc(Document doc)
   {
         pageDoc = doc;
   }


   private static Document pageDoc;
   
}
