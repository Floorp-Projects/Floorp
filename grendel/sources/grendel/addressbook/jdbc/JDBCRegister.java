/*
 * JDBCRegister.java
 *
 * Created on 30 September 2005, 19:42
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook.jdbc;

import grendel.Task;
import grendel.TaskList;

import java.sql.DriverManager;


/**
 * This Class Looks after creating the JDBC bits so we can just throw url's around and have it work.
 * @TODO Add this to Grendel Backend Startup, when it is written.
 * @author hash9
 */
public final class JDBCRegister extends Task
{
  /** Creates a new instance of JDBCRegister */
  public JDBCRegister()
  {
  }

  public static void registerJDBCdrivers()
  {
    Task t = new JDBCRegister();

    try
    {
      t.execute();
    }
    catch (IllegalStateException ex)
    {
      ex.printStackTrace();
    }
    catch (Exception ex)
    {
      ex.printStackTrace();
    }
  }

  /**
   * Initialise the JDBC bits.
   */
  protected void run()
  {
    StringBuilder default_drivers = new StringBuilder();
    String jdbc_drivers = System.getProperty("jdbc.drivers");

    if (jdbc_drivers != null)
    {
      default_drivers.append(jdbc_drivers);
    }

    if ((default_drivers.length() > 0) &&
        (default_drivers.charAt(default_drivers.length() - 1) != ':'))
    {
      default_drivers.append(':');
    }

    default_drivers.append("com.octetstring.jdbcLdap.sql.JdbcLdapDriver"); //XXX This should be dynamic
    default_drivers.append(':');
    default_drivers.append("org.apache.derby.jdbc.EmbeddedDriver"); //XXX This should be dynamic
    default_drivers.append(':');
    default_drivers.append("org.hsqldb.jdbcDriver"); //XXX This should be dynamic
    System.setProperty("jdbc.drivers", default_drivers.toString());
  }
}
