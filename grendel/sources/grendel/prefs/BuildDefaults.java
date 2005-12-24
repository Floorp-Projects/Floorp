/*
 * BuildDefaults.java
 *
 * Created on 07 September 2005, 16:55
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.prefs;

import com.sun.org.apache.xerces.internal.util.URI;
import grendel.prefs.accounts.Account_IMAP;
import grendel.prefs.accounts.Account_SMTP;
import grendel.prefs.accounts.Accounts;
import grendel.prefs.accounts.Identity;
import grendel.prefs.xml.XMLPreferences;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.net.URISyntaxException;
import java.net.URL;

/**
 * This class should only be run durring the build process!
 * @author hash9
 */
public final class BuildDefaults {
  
  /** This class cannot be created only run! */
  private BuildDefaults() throws URISyntaxException, IOException {
    XMLPreferences defaults = new XMLPreferences();
    Account_SMTP a_smtp=new Account_SMTP();
    a_smtp.setName("smtp");
    a_smtp.setHost("localhost");
    
    Account_IMAP a_imap=new Account_IMAP("Default");
    a_imap.setHost("127.0.0.1");
    a_imap.setPort(143);
    a_imap.setUsername("user");
    a_imap.setPassword("pass");
    a_imap.setDefaultIdentity(0);
    a_imap.setSendAccount(a_smtp);
    
    Identity i = new Identity("Joe Bloggs", "joe@bloggs.name", "default");
    a_imap.addIdentity(i);
    
    Accounts a = new Accounts();
    a.addAccount(a_smtp);
    a.addAccount(a_imap);
    defaults.setProperty("accounts", a);
    
    String this_class_file = "/"+getClass().getCanonicalName().replace('.','/')+".class";
    URL defaults_file= getClass().getResource(this_class_file);
    File f = new File(defaults_file.toURI());
    String dir = f.getParent();
    if (!dir.endsWith(File.separator)) {
      dir=dir+File.separator;
    }
    
    defaults.storeToXML(new FileWriter(new File(dir+"defaults.xml")));
  }
  
  /**
   * @param args the command line arguments
   */
  public static void main(String[] args) throws URISyntaxException, IOException {
    new BuildDefaults();
  }
  
}
