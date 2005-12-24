/*
 * Sun.java
 *
 * Created on 04 September 2005, 21:31
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.javamail;
import grendel.prefs.Preferences;
import grendel.prefs.accounts.Account;
import grendel.prefs.accounts.Account_Berkeley;
import grendel.prefs.accounts.Account_MBox;
import grendel.prefs.accounts.Account_NNTP;
import grendel.prefs.accounts.Account_POP3;
import java.util.Properties;
import javax.mail.Service;
import javax.mail.Session;
import javax.mail.URLName;

/**
 *
 * @author hash9
 */
public final class Grendel {
    public static Service get_pop3(URLName url, Account a) {
        Account_POP3 pop3 = (Account_POP3)a;
        Properties props = JMProviders.getSession().getProperties();
        Session s = JMProviders.getSession().getInstance(props);
        return new grendel.storage.PopStore(s,url);
    }
    
    public static Service get_berkeley(URLName url, Account a) {
        Account_Berkeley berkeley = (Account_Berkeley)a;
        Properties props = JMProviders.getSession().getProperties();
        Session s = JMProviders.getSession().getInstance(props);
        return new grendel.storage.BerkeleyStore(s,url);
    }
    
    public static Service get_nntp(URLName url, Account a) {
        Account_NNTP nntp = (Account_NNTP)a;
        Properties props = JMProviders.getSession().getProperties();
        
        /* 
         * String host = nntp.getHost();
         * if (host!=null) {
         *    props.put("mail.nntp.newsrc.file", Preferences.getPreferances().getProfilePath()+host+".newsrc");
         *    props.put("mail.nntp.host", host);
         * } else {
         *    props.put("mail.nntp.newsrc.file", Preferences.getPreferances().getProfilePath()+"news.rc");
         * }
         *
         */
        Session s = JMProviders.getSession().getInstance(props);
        return new grendel.storage.NewsStore(s,url);
    }
    
    /*public static Service get_nntp_send(URLName url, Account a) {
        Account_NNTP nntp = (Account_NNTP)a;
        Properties props = JMProviders.getSession().getProperties();
        Session s = JMProviders.getSession().getInstance(props);
        return new gnu.mail.providers.nntp.NNTPTransport(s,url);
    }*/
}
