/**
 *   @author   Raju Pallath
 *   @version  1.0
 *
 *   This class is the Base Class for all test cases.
 *
 */

package org.mozilla.dom.test;

public class BWBaseTest implements Execution 
{
   
  /**
   *  This is a dummy implementation of interface method
   *
   *  @param    obj   Object instance (Node/Document/....)
   * 
   *  @return         if test has passed then return true else false
   *
   *
   */
   public boolean execute(Object obj)
   {
     return true;
   }

  /**
   *  Send a null Object. Classes extending this class will override this
   *  method
   *
   * 
   *  @return         Null object reference
   *
   */
   public Object returnObject()
   {
     return null;
   }

   public static boolean FAILED=false;
   public static boolean PASSED=true;
}
