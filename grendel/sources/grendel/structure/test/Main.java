/*
 * Main.java
 *
 * Created on 22 August 2005, 14:41
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.test;
import grendel.renderer.Renderer;
import grendel.structure.*;
import java.io.CharArrayWriter;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.PrintStream;
import java.util.logging.ConsoleHandler;
import java.util.logging.Logger;
import java.util.logging.SimpleFormatter;



/**
 *
 * @author hash9
 */
public class Main {
    
    /** Creates a new instance of Main */
    public Main() {
    }
    public static void main(String... args) {
        /*Logger logger = Logger.getLogger("gnu.mail.providers.nntp");
        ConsoleHandler ch = new ConsoleHandler();
        ch.setFormatter(new SimpleFormatter());
        logger.addHandler(ch);*/
        /*System.err.println("Servers: "+Servers.getServers());
        Server s = Servers.getServers().get(0);*/
        //decend(s.getRoot(),"",null);
        //NewMessage nm = new NewMessage();
        /*MessageList messages = s.getRoot().getFolders().get(0).getMessages();
        NewMessage nm = messages.get(messages.size()-1).replyTo(false);
        Account__Send ar = Preferences.getPreferances().getAccounts().getSendAccounts().get(0);
        nm.setAccountIdentity(new AccountIdentity(ar,ar.getIdentity(0)));
        nm.addCCAddress(new RFC822Mailbox("postmaster@res04-kam58.res.st-and.ac.uk"));
        try {
            Sender.send(nm);
        } catch (MessageSendViolation msv) {
            msv.printStackTrace();
        }*/
        
        /*NewMessage nm = new NewMessage();
        Account__Send ar = Preferences.getPreferances().getAccounts().getSendAccounts().get(0);
        nm.setAccountIdentity(new AccountIdentity(ar,ar.getIdentity(0)));
        nm.setSubject("Test 2");
        nm.setBody(new StringBuilder("<This is a Test"));
        nm.setContentType("text/plain");
        try {
            nm.message.addRecipient(MimeMessage.RecipientType.NEWSGROUPS,new NewsAddress("netscape.test"));
        } catch (MessagingException e) {
            e.printStackTrace();
        }
        try {
            Sender.send(nm);
        } catch (MessageSendViolation msv) {
            msv.printStackTrace();
        }*/
        System.err.println("Servers: "+Servers.getServers());
        Server s = Servers.getServers().get(2);
        System.out.println("Server: "+s);
        /*{
            FolderList folders = s.getRoot().getAllFolders();
            //System.err.println("Folders: "+folders);
            System.err.println("Folders:");
            for (Folder f: folders) {
                System.err.println('\t'+f.getName());
            }
        }*/
        {
            Folder f = s.getRoot().getFolder("netscape.test");
            Folder netscape_test =f;
            //netscape_test.setSubscribed(true);
            //FolderList folders = s.getRoot().getFolders();
            //etscape_test.
            echoSigs(netscape_test);
            /*System.out.println("Subscribed Folders:");
            for (Folder f: folders) {*/
                System.out.println('\t'+f.getName());
                try {
                    FileOutputStream fos = new FileOutputStream("C:\\out.htm");
                    InputStream is = f.getMessage(21).renderToHTML();
                    int i = is.read();
                    while (i!=-1) {
                        fos.write(i);
                        i = is.read();
                    }
                    fos.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            //}//*/
            /*for (Folder f: folders) {
                //System.err.println('\t'+f.getName());
                echoSigs(f);
            }*/
            s.disconnect();
        }
        
        /*Folder test = s.getRoot().getFolder("org.apache.james.user");
        Message m = test.getMessage(1);
        System.out.println("Message 1: " + m.toString());*/
        //echoSigs(netscape);*/
        //Account__Receive ar = Preferences.getPreferances().getAccounts().getReciveAccounts().get(0);
        /*System.out.println("Servers: "+Servers.getServers());
        Server s = Servers.getServers().get(2);
        FolderList fl =s.getRoot().getFolders();
        for (Folder f: fl) {
            System.out.println("\t" + f.getName());
        }*/
        //decend(s.getRoot(),"",System.out);
    }
    
    private static void echoSigs(Folder f) {
        System.out.println("Folder: "+f.getName());
        MessageList ml = f.getMessages();
        int ml_size = ml.size();
        for (int i = 0;(i<10)&&(i<ml_size);i++) {
            Message m = ml.get(i);
            System.out.println("\t\t"+m.toString());
        }
    }
    
    public static void decend(Folder f, String indent, PrintStream ps) {
        ps.println(indent + "Folder: "+f.getName());
        ps.println(indent + "\tMessages:");
        MessageList ml = f.getMessages();
        for (Message m: ml) {
            ps.println(indent + "\t\t"+m.toString());
            try {
                InputStream is = m.renderToHTML();
                CharArrayWriter caw = new CharArrayWriter();
                int i = 0;
                while (i!=-1) {
                    i = is.read();
                    caw.write(i);
                }
                
                ps.println(indent + "\t\t\t"+caw.toString());
            } catch (Exception ioe) {
                ps.println(indent + "\t\t\t"+ioe.toString());
            }
        }
        FolderList fl = f.getFolders();
        indent = indent+"\t";
        for (Folder nf: fl) {
            decend(nf,indent,ps);
        }
    }
    
}
