/*
 * KTest.java
 *
 * Created on 03 December 2005, 20:54
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.structure.test;

import grendel.messaging.Console;
import grendel.messaging.NoticeBoard;
import grendel.prefs.accounts.Account_IMAP;
import grendel.prefs.accounts.Account_POP3;
import grendel.prefs.accounts.Account_SMTP;
import grendel.prefs.accounts.Identity;
import grendel.storage.addressparser.RFC822Mailbox;
import grendel.structure.Folder;
import grendel.structure.Message;
import grendel.structure.Server;
import grendel.structure.sending.AccountIdentity;
import grendel.structure.sending.MessageSendViolation;
import grendel.structure.sending.NewMessage;
import grendel.structure.sending.Sender;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Date;
import javax.mail.NoSuchProviderException;

/**
 *
 * @author hash9
 */
public class KTest {
    
    /** Creates a new instance of KTest */
    public KTest() {
    }
    
    public static void main(String... args) {
        {
            System.setProperty("socksProxyHost","localhost");
            System.setProperty("socksProxyPort","8080");
        }
        {
            NoticeBoard.getNoticeBoard().clearPublishers();
            NoticeBoard.getNoticeBoard().addPublisher(new Console(System.err));
        }
        {
            Account_SMTP s = new Account_SMTP();
            s.setName("Test");
            s.setHost("mail.betatest.kellertechnologies.com");
            s.setUsername("grendel+betatest.kellertechnologies.com");
            s.setPassword("grendel");
            s.setPort(26);
            NewMessage nm = new NewMessage();
            nm.addToAddress(new RFC822Mailbox("grendel@betatest.kellertechnologies.com"));
            nm.setAccountIdentity(new AccountIdentity(s,new Identity("Grendel Test","grendel@betatest.kellertechnologies.com","Grendel Test Ident")));
            nm.setContentType("text/plain");
            nm.setSubject("Grendel Test: "+new Date());
            nm.setBody(new StringBuilder("This is a test.\n\nSent: "+new Date()));
            try {
                Sender.send(nm);
            } catch (MessageSendViolation ex) {
                ex.printStackTrace();
            }
        }
        {
            Account_IMAP r = new Account_IMAP();
            r.setName("Test");
            r.setHost("mail.betatest.kellertechnologies.com");
            r.setUsername("grendel+betatest.kellertechnologies.com");
            r.setPassword("grendel");
            r.setPort(-1);
            try {
                Server s = new Server(r);
                Folder f = s.getRoot().getFolder("INBOX");
                System.out.println(f.getNoMessages());
                Message m = f.getMessage(1);
                InputStream is = m.renderToHTML();
                int c = is.read();
                while (c!=-1) {
                    System.out.write(c);
                    System.out.flush();
                    c = is.read();
                }
            } catch (NoSuchProviderException ex) {
                ex.printStackTrace();
                System.exit(0);
            } catch (IOException ex) {
                ex.printStackTrace();
                System.exit(0);
            }
            System.exit(0);
        }
        System.exit(0);
    }
    
}
