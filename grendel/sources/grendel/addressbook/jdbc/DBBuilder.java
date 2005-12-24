/*
 * DBBuilder.java
 *
 * Created on 10 October 2005, 22:27
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.jdbc;

import grendel.Shutdown;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.sql.Connection;
import java.sql.SQLException;

/**
 * This Class should build the standard Derby Address Book Database
 * @TODO Write this class
 * @author hash9
 */
public final class DBBuilder
{
  
  /**
   * Creates a new instance of DBBuilder
   */
  private DBBuilder()
  {
    
  }
  
  /**
   * @return true if the db was built sucessfully. Output is sent to st-out and st-err
   */
  public static boolean build(Connection connection)
  {
    try
    {
      BufferedReader br =new BufferedReader( new FileReader(new File(DBBuilder.class.getResource("db_create.sql").toURI())));
      StringBuilder sb = new StringBuilder();
      while (br.ready())
      {
        sb.append(br.readLine());
        sb.append("\n");
      }
      System.out.println("SQL:");
      System.out.println(sb.toString().replace("\n","\n\t").trim());
      System.out.println("====\t====");
      connection.createStatement().execute(sb.toString());
      System.out.println("====\t====");
      System.out.println("Complete");
      return true;
    }
    catch (Exception e)
    {
      NoticeBoard.publish(new ExceptionNotice(e));
      return false;
    }
  }
  
}
