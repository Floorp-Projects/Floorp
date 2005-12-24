/*
 * InitShutdownTask.java
 *
 * Created on 16 October 2005, 22:41
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel;

/**
 *
 * @author hash9
 */
public class InitShutdownTask extends Task
{
  
  /** Creates a new instance of InitShutdownTask */
  public InitShutdownTask()
  {
  }

  protected void run() throws Exception
  {
    Shutdown.touch();
  }
  
}
