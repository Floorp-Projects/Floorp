/*
 * DerbyShutdownTask.java
 *
 * Created on 16 October 2005, 22:38
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook;

import grendel.Task;
import grendel.messaging.NoticeBoard;
import grendel.messaging.StringNotice;
import java.sql.DriverManager;
import java.sql.SQLException;


/**
 *
 * @author hash9
 */
public class HSQLDBShutdownTask extends Task
{
  /** Creates a new instance of DerbyShutdownTask */
  public HSQLDBShutdownTask()
  {
  }

  protected void run() throws Exception
  {
    try
    {
      DriverManager.getConnection("jdbc:hsqldb:;shutdown=true");
      System.err.println("HSQLDB Shutdown Failed!");
      NoticeBoard.publish(new StringNotice("HSQLDB Shutdown Failed!"));
    }
    catch (SQLException ex)
    {
      System.out.println("HSQLDB Shutdown Suceceded");
    }
  }
}
