/*
 * ServDB.java
 *
 * Created on 15 October 2005, 23:14
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook.jdbc.hsqldb;

import grendel.prefs.addressbook.Addressbook_HSQLDB;
import java.io.IOException;
import java.io.PrintWriter;
import org.hsqldb.Server;
import org.hsqldb.WebServer;
import grendel.Shutdown;

/**
 * This class contains a main method to allow allow the addressbook databases to
 * be exported as a HSQLDB JDBC server
 * @author hash9
 */
public class ServDB
{
  /** Creates a new instance of ServDB */
  public ServDB()
  {
  }

  /**
   * @param args the command line arguments
   */
  public static void main(String[] args) throws IOException
  {
    Server server = new Server();
    Addressbook_HSQLDB addressbook = new Addressbook_HSQLDB("HSQLDB Test Database");
    server.setDatabaseName(0, "db");
    server.setDatabasePath(0, addressbook.getDirectory());
    server.setLogWriter(new PrintWriter(System.out));
    server.setErrWriter(new PrintWriter(System.err));
    server.start();
    
    System.in.read();
    Shutdown.exit();
  }
}
