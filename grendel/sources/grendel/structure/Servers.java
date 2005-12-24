/*
 * Servers.java
 *
 * Created on 18 August 2005, 23:45
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure;

import grendel.javamail.JMProviders;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import grendel.prefs.Preferences;
import grendel.prefs.accounts.Account__Receive;
import grendel.prefs.xml.XMLPreferences;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;
import java.util.Vector;
import javax.mail.NoSuchProviderException;
import javax.mail.Session;



/**
 *
 * @author hash9
 */
public final class Servers {
    private static Servers fInstance=null;
    
    private static XMLPreferences options_mail;
    private static List<Server> servers = new Vector<Server>();
    
    /** Creates a new instance of Servers */
    static {
        options_mail=Preferences.getPreferances().getPropertyPrefs("options").getPropertyPrefs("mail");
    }
    
    public static synchronized List<Server> getServers() {
        updateServers();
        
        return new Vector<Server>(servers);
    }
    
    private static synchronized void closeStores() {
        for (Server server : servers) {
            server.disconnect();
        }
    }
    
    private static synchronized void updateServers() {        
        List<Account__Receive> recive_accounts_list= new ArrayList<Account__Receive>(Preferences.getPreferances().getAccounts().getReciveAccounts());
        //servers=new Vector<Server>(recive_accounts_list.size());
        
        for (Server s: servers) {
            boolean contains = recive_accounts_list.remove(s.getAccount());
            if (! contains) {
                servers.remove(s);
            }
        }
        for (Account__Receive account : recive_accounts_list) {
            try {
                servers.add(new Server(account));
            } catch (NoSuchProviderException e) {
                NoticeBoard.publish(new ExceptionNotice(e));
            }
        }
    }
}
