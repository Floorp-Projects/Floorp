/**
 *   @author   Raju Pallath
 *   @version  1.0
 *
 *   A interface which define the execute method. This interface is
 *   implemented by all Test Classes.
 *
 */

package org.mozilla.dom.test;

public interface Execution
{
  /**
   *
   *  @param    targetObj   Object instance (Node/Document/....)
   *
   *  @return               Return true or false
   *
   */
   public boolean execute(Object targetObj);


}
