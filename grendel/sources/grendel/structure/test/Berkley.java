/*
 * Berkley.java
 *
 * Created on 01 September 2005, 15:18
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.structure.test;
import grendel.javamail.JMProviders;
import grendel.storage.BerkeleyStore;
import grendel.storage.addressparser.RFC822Mailbox;
import grendel.structure.sending.MessageSendViolation;
import grendel.util.MessageIDGenerator;
import java.util.Properties;
import javax.mail.MessagingException;
import javax.mail.NoSuchProviderException;
import javax.mail.Session;
import javax.mail.URLName;
import javax.mail.internet.MimeMessage;


/**
 *
 * @author hash9
 */
public class Berkley {
    
    /** Creates a new instance of Berkley */
    public Berkley() {
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws NoSuchProviderException, MessagingException, MessageSendViolation {
        Properties props =JMProviders.getSession().getProperties();
        /*props.setProperty("mail.mbox.mailhome","C:\\grendel\\mbox");
        props.setProperty("mail.mbox.inbox","C:\\grendel\\mbox\\INBOX");*/
        Session session = JMProviders.getSession().getInstance(props);
        session.setDebug(JMProviders.getSession().getDebug());
        
        URLName url = new URLName("berkeley",null,-1,"C:\\grendel\\berkeley",null,null);
        
        javax.mail.Store store = session.getStore(url);
        if (store instanceof BerkeleyStore) {
            BerkeleyStore berkeleystore = (BerkeleyStore) store;
            store.connect();
            javax.mail.Folder inbox = berkeleystore.getDefaultFolder().getFolder("INBOX");
            //inbox.create(javax.mail.Folder.HOLDS_MESSAGES);
            inbox.open(javax.mail.Folder.READ_WRITE);
            MimeMessage mm = new MimeMessage(session);
            mm.setHeader("X-Mailer", "Grendel [Development Version New Revision]"); //and proud of it!
            mm.setHeader("Message-ID", MessageIDGenerator.generate("grendel.test"));
            mm.setSentDate(new java.util.Date());     //set date to now.
            mm.setFrom(new RFC822Mailbox("Evil","evil@life.org"));
            mm.setRecipients(javax.mail.Message.RecipientType.TO,new javax.mail.internet.InternetAddress[] {new RFC822Mailbox("Evil","evil@life.org")});
            mm.setContent("This is a test\nyou know now lets see what happens!","text/plain");
            mm.setSubject("Evil::Test");
            javax.mail.Message[] m_a = new javax.mail.Message[] {mm};
            inbox.appendMessages(m_a);
            /*javax.mail.Folder[] f_a = mboxstore.getDefaultFolder().list();
            for (int i =0;i<f_a.length;i++) {
                System.out.println(i+" => "+f_a[i].getName());
            }*/
            synchronized (Berkley.class) {
                try {
                    Berkley.class.wait(10000);
                } catch (InterruptedException ex) {
                    ex.printStackTrace();
                }
            }
            inbox.close(true);
            berkeleystore.close();
        } else {
            System.err.println("WTF!!!!!!");
        }
    }
}
