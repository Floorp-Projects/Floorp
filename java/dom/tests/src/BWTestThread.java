/**
 *   @author   Raju Pallath
 *   @version  1.0
 *
 *   This class is the Base Thread Class for all test cases.
 *   Using this, we can run test cases ina multi threaded fashion.
 *
 */

package org.mozilla.dom.test;

public class BWTestThread extends Thread
{
   
  /**
   *  Constructor
   *
   *  @param    str   Thread Name
   * 
   *  @return         
   *
   */
   public BWTestThread (String astr)
   {
       
       super(astr);
       threadName = astr;
       if (threadName == null) 
          threadName = "defaultThread";
   }


  /**
   *  Set the Test Case ClassObject which has to be launched by this thread.
   *  and the Base Document Object.
   *  all these objects extend from BWBaseTest class.
   *
   *  @param   aobj        Test Class Object reference 
   *                       (subclassed from BWBaseTest).
   *  @param   atargetObj  Base Document Object Reference
   *
   *
   *  @return         
   *
   */
   public void setTestObject(Object aobj, Object atargetObj)
   {
        classObj  = aobj;
        targetObj = atargetObj;
   }


  /**
   *  Get the Test Case Object Reference.
   *
   *  @return     Test Object Reference         
   *
   */
   public Object getTestObject()
   {
         return classObj;
   }


  /**
   *  Get the Thread Name
   *
   *  @return     thread name string 
   *
   */
   public String getThreadName()
   {
     return threadName;
   }

  /**
   *  Thread Run Method
   *
   *  @return     
   *
   */
   public void run()
   {
         if (classObj == null) return;
         if (targetObj == null) return;
         try {
           Thread.sleep (10);
         } catch (Exception e) {
         }
         execute();

   }

   public synchronized void execute()
   {
         if (((BWBaseTest)classObj).execute(targetObj)) {
              TestLoader.txtPrint(threadName, "PASSED");
         } else {
              TestLoader.txtPrint(threadName, "FAILED");
         } 
   }

   private Object classObj=null;
   private Object targetObj=null;
   private String threadName=null;
}
