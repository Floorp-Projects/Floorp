/*
 * ServDB.java
 *
 * Created on 10 October 2005, 22:41
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.jdbc.derby;
import grendel.Shutdown;
import grendel.messaging.NoticeBoard;
import grendel.prefs.addressbook.Addressbook_Derby;
import java.io.PrintWriter;
import java.net.InetAddress;
import org.apache.derby.drda.NetworkServerControl;

/**
 * This class contains a main method to allow allow the addressbook databases to
 * be exported as a Derby JDBC server
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
  public static void main(String[] args) throws Exception
  {
    //import org.apache.derby.drda.NetworkServerControl;
    //import java.net.InetAddress;
    Shutdown.touch();
    
    NoticeBoard.getNoticeBoard().clearPublishers();
        
    System.setProperty("derby.system.home",Addressbook_Derby.getBaseDirectory().concat("/derby"));
    System.setProperty("derby.infolog.append","true");
    
    
    NetworkServerControl server = new NetworkServerControl(InetAddress.getByName("localhost"),1527);
    server.start(new PrintWriter(System.err));
    System.out.println(isServerStarted(server,10));
    
    //NetworkServerControl.main(new String [] {"start"});
    
    System.in.read();
    Shutdown.exit();
  }
  
  private static boolean isServerStarted(NetworkServerControl server, int ntries)
  {
    for (int i = 1; i <= ntries; i ++)
    {
      try
      {
        Thread.sleep(500);
        server.ping();
        return true;
      }
      catch (Exception e)
      {
        if (i == ntries)
          return false;
      }
    }
    return false;
  }
}
