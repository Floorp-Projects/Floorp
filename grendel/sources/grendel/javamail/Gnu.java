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
import grendel.prefs.accounts.Account_IMAP;
import grendel.prefs.accounts.Account_MBox;
import grendel.prefs.accounts.Account_Maildir;
import grendel.prefs.accounts.Account_NNTP;
import grendel.prefs.accounts.Account_POP3;
import grendel.prefs.accounts.Account_SMTP;
import java.util.Properties;
import javax.mail.Service;
import javax.mail.Session;
import javax.mail.URLName;

/**
 *
 * @author hash9
 */
public final class Gnu {
    public static Service get_smtp(URLName url, Account a) {
        Account_SMTP smtp = (Account_SMTP) a;
        Properties props = JMProviders.getSession().getProperties();
        // Use the GNU JavaMail Properties
        // Name                            Type                       Description
        // mail.smtp.host                  IP address or hostname     The SMTP server to connect to
        // mail.smtp.port                  integer (>=1)              The port to connect to, if not specified.
        // mail.smtp.connectiontimeout     integer (>=1)              Socket connection timeout, in milliseconds. Defaults to no timeout.
        props.put("mail.smtp.connectiontimeout","10"); //XXX This should be an option
        // mail.smtp.timeout               integer (>=1)              Socket I/O timeout, in milliseconds. Defaults to no timeout.
        props.put("mail.smtp.timeout","10"); //XXX This should be an option
        // mail.smtp.from                  RFC 822 address            The mailbox to use for the SMTP MAIL command. If not set, the first InternetAddress in the From field of the message will be used, or failing that, InternetAddress.getLocalAddress().
        // mail.smtp.localhost             IP address or hostname     The host identifier for the local machine, to report in the EHLO/HELO command.
        // mail.smtp.ehlo                  boolean                    If set to false, service extensions negotiation will not be attempted.
        props.put("mail.smtp.ehlo","true");
        // mail.smtp.auth                  boolean                    If set to true, authentication will be attempted. Exceptionally, you may set this to the special value "required" to throw an exception on connect when authentication could not be performed.
        if ((smtp.getUsername()!=null)&&(smtp.getPassword()!=null)) {
            props.put("mail.smtp.auth","required");
        }
        // mail.smtp.auth.mechanisms       comma-delimited
        //                                 list of SASL mechanisms    If set, only the specified SASL mechanisms will be attempted during authentication, in the given order. If not present, the SASL mechanisms advertised by the server will be used.
        // mail.smtp.dsn.notify            string                     The RCPT NOTIFY option. Should be set to either "never", or one or more of "success", "failure", or "delay" (separated by commas and/or spaces). See RFC 3461 for details. This feature may not be supported by all servers.
        // mail.smtp.dsn.ret               string                     The MAIL RET option. Should be set to "full" or "hdrs". See RFC 3461 for details. This feature may not be supported by all servers.
        // mail.smtp.tls                   boolean                    If set to false, TLS negotiation will not be attempted. Exceptionally, you may set this to the special value "required" to throw an exception on connect when TLS is not available.
        //TODO This should be determined.
        // mail.smtp.trustmanager          String                     The name of a class implementing the javax.net.ssl.TrustManager interface, which will be used to determine trust in TLS negotiation
        //TODO what is this????
        Session s = JMProviders.getSession().getInstance(props);
        return new gnu.mail.providers.smtp.SMTPTransport(s,url);
    }
    
    public static Service get_imap(URLName url, Account a) {
        Account_IMAP imap = (Account_IMAP)a;
        Properties props = JMProviders.getSession().getProperties();
        // Name                           Type                        Description
        // mail.imap.host                 IP address or hostname      The IMAP server to connect to.
        // mail.imap.port                 integer (>=1)               The port to connect to, if not the default.
        // mail.imap.user                 username                    The default username for IMAP.
        // mail.imap.connectiontimeout    integer (>=1)               Socket connection timeout, in milliseconds. Default is no timeout.
        // mail.imap.timeout              integer (>=1)               Socket I/O timeout, in milliseconds. Default is no timeout.
        // mail.imap.tls                  boolean                     If set to false, TLS negotiation will not be attempted.
        // mail.imap.trustmanager         String                      The name of a class implementing the javax.net.ssl.TrustManager interface, which will be used to determine trust in TLS negotiation.
        // mail.imap.auth.mechanisms      Comma-delimited list of
        //                                SASL mechanisms             If set, only the specified SASL mechanisms will be attempted during authentication, in the given order. If not present, the SASL mechanisms advertised by the server will be used.
        // mail.imap.debug.ansi           boolean                     If set to true, and mail.debug is also true, and your terminal supports ANSI escape sequences, you will get syntax coloured responses.
        Session s = JMProviders.getSession().getInstance(props);
        return new gnu.mail.providers.imap.IMAPStore(s,url);
    }
    
    public static Service get_pop3(URLName url, Account a) {
        Account_POP3 pop3 = (Account_POP3)a;
        Properties props = JMProviders.getSession().getProperties();
        Session s = JMProviders.getSession().getInstance(props);
        return new gnu.mail.providers.pop3.POP3Store(s,url);
    }
    
    public static Service get_maildir(URLName url, Account a) {
        Account_Maildir maildir = (Account_Maildir)a;
        Properties props = JMProviders.getSession().getProperties();
        props.setProperty("mail.maildir.home",maildir.getStoreDirectory());
        props.setProperty("mail.maildir.maildir",maildir.getStoreInbox());
        Session s = JMProviders.getSession().getInstance(props);
        return new gnu.mail.providers.maildir.MaildirStore(s,url);
    }
    
    public static Service get_mbox(URLName url, Account a) {
        Account_MBox mbox = (Account_MBox)a;
        Properties props = JMProviders.getSession().getProperties();
        props.setProperty("mail.mbox.mailhome",mbox.getStoreDirectory());
        props.setProperty("mail.mbox.inbox",mbox.getStoreDirectory()+java.io.File.separatorChar+"INBOX");
        props.setProperty("mail.mbox.attemptfallback","true");
        Session s = JMProviders.getSession().getInstance(props);
        return new gnu.mail.providers.mbox.MboxStore(s,url);
    }
    
    public static Service get_nntp(URLName url, Account a) {
        Account_NNTP nntp = (Account_NNTP)a;
        Properties props = JMProviders.getSession().getProperties();
        //XXX This refers to the HEAD CVS of GNU JavaMail
        String host = nntp.getHost();
        if (host!=null) {
            props.put("mail.nntp.newsrc.file", Preferences.getPreferances().getProfilePath()+host+".newsrc");
            props.put("mail.nntp.host", host);
        } else {
            props.put("mail.nntp.newsrc.file", Preferences.getPreferances().getProfilePath()+"news.rc");
        }
        
        props.put("mail.nntp.listall", "true"); //XXX Mozilla.org Bug
        Session s = JMProviders.getSession().getInstance(props);
        return new gnu.mail.providers.nntp.NNTPStore(s,url);
    }
    
    public static Service get_nntp_send(URLName url, Account a) {
        Account_NNTP nntp = (Account_NNTP)a;
        Properties props = JMProviders.getSession().getProperties();
        Session s = JMProviders.getSession().getInstance(props);
        return new gnu.mail.providers.nntp.NNTPTransport(s,url);
    }
}
