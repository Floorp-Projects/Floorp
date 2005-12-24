/*
 * TaskManager.java
 *
 * Created on 17 October 2005, 00:32
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel;

import grendel.addressbook.DerbyShutdownTask;
import grendel.addressbook.HSQLDBShutdownTask;

import grendel.addressbook.jdbc.JDBCRegister;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;


/**
 *
 * @author hash9
 */
public final class TaskManager
{
  private static final Task[] startup = 
    {
      new InitShutdownTask(),
      new JDBCRegister(),
      new SelectLAFTask(),
      new SelectLayoutTask()
    };
  private static final Task[] shutdown = 
    {
      new HSQLDBShutdownTask(),
      new DerbyShutdownTask()
    };
  private static final Task[] halt = 
    {
      new HSQLDBShutdownTask(),
      new DerbyShutdownTask()
    };

  /** Creates a new instance of TaskManager */
  private TaskManager()
  {
  }

  /**
   * Get the single halt task list.
   * @throws java.io.IOException if an IOException occurs
   * @return the halt TaskList.
   */
  public static void runHalt()
  {
    run(halt);
  }

  /**
   * Get the single shutdown task list.
   * @throws java.io.IOException if an IOException occurs
   * @return the shutdown TaskList.
   */
  public static void runShutdown()
  {
    run(shutdown);
  }

  /**
   * Get the single startup task list.
   * @throws java.io.IOException if an IOException occurs
   * @return the startup TaskList.
   */
  public static void runStartup()
  {
    run(startup);
  }

  private static void run(Task[] tasks)
  {
    for (Task t : tasks)
    {
      try
      {
        if (!t.isRun())
        {
          t.execute();
        }
      }
      catch (Throwable th)
      {
        NoticeBoard.publish(new ExceptionNotice(th));
      }
    }
  }
}
