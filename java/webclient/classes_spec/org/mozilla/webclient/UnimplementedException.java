/** This class implements the UnimplementedException Exception
  class. This derives from RuntimeException and is used as a
  placeholder definition for all API functions that have not
  yet been implemented
  */

package org.mozilla.webclient;

import java.lang.*;


public class UnimplementedException extends RuntimeException 
{
  
  public UnimplementedException() { }
  
  public UnimplementedException(String msg) 
    {
      super(msg);
    }

}
