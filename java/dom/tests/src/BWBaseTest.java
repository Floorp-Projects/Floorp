/*
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s):
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

  /**
   *  This method indicates whether a particular method is UNSUPPORTED or 
   *  not.
   *
   */
   public void setUnsupported()
   {
     UNSUPPORTED = true;
   }

  /**
   *  This method indicates whether a particular method is UNSUPPORTED or 
   *  not.
   *
   * 
   *  @return         Value of boolean varibale UNSUPPORTED.
   *
   */
   public boolean isUnsupported()
   {
     return UNSUPPORTED;
   }


   public static boolean FAILED=false;
   public static boolean PASSED=true;
   public boolean UNSUPPORTED=false;
}
