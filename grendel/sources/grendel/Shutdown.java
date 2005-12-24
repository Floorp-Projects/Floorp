/*
 * Shutdown.java
 *
 * Created on 10 October 2005, 21:54
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel;

import grendel.messaging.NoticeBoard;
import grendel.messaging.StringNotice;

import java.io.IOException;

import java.sql.DriverManager;
import java.sql.SQLException;


/**
 *
 * @author hash9
 */
public class Shutdown extends Thread
{
  private static Shutdown This;

  static
  {
    touch();
  }

  /** Creates a new instance of Shutdown */
  private Shutdown()
  {
  }

  /**
   * Exit cleanly preforming all nessisary tare down code.
   */
  public static void exit()
  {
    untouch();

    try
    {
      TaskList.getShutdownList().run();
    }
    catch (IOException ex)
    {
      ex.printStackTrace();
    }

    System.exit(0);
  }

  public void run()
  {
    term();
  }

  /**
   * Add shutdown support
   */
  public static void touch()
  {
    if (This == null)
    {
      This = new Shutdown();
      Runtime.getRuntime().addShutdownHook(This);
    }
  }

  /**
   * This method is called to quickly safely exit, this is called when a call is made to System.exit.
   * Add only essential tare down code.
   * Sockets do not normally nead taken down, connections that closs out of scope don't go in here.
   */
  private static void term()
  {
    try
    {
      TaskList.getHaltList().run();
    }
    catch (IOException ex)
    {
      ex.printStackTrace();
    }
  }

  private static void untouch()
  {
    Runtime.getRuntime().removeShutdownHook(This);
    This = null;
  }
}
