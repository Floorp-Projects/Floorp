/*
 * Sender.java
 *
 * Created on 23 August 2005, 16:37
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.sending;

import grendel.javamail.JMProviders;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import grendel.prefs.accounts.Account;
import grendel.prefs.accounts.Account__Send;
import grendel.storage.addressparser.AddressList;
import grendel.structure.Servers;
import java.util.Properties;
import javax.mail.Address;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Transport;

/**
 *
 * @author hash9
 */
public final class Sender {
    
    /** Creates a new instance of Sender */
    private Sender() {
    }
    
    public static void send(NewMessage message) throws MessageSendViolation {
        message.prepareSend();
        AccountIdentity ai = message.getAccountIdentity();
        Account a = ai.getAccount();
        if (a instanceof Account__Send) {
            Account__Send send = (Account__Send) a;
            try {
                //TODO Fix this so that it supports nntp and rfc822 address                
                AddressList al = message.getAddresses();
                Properties props = JMProviders.getSession().getProperties();
                props = send.setSessionProperties(props);
                Session newSession = Session.getInstance(props,null);
                Transport tp = JMProviders.getJMProviders().getTransport(send);
                tp.connect();
                tp.sendMessage(message.getJavaMailMessage(),al.getAddressArray());       // send the message.
                message.markAsSent();
                //TODO put a copy of this message in the sent items!
                
            } catch (javax.mail.SendFailedException sfe) {
                sfe.printStackTrace();
                Address addr[] = sfe.getInvalidAddresses();
                if (addr != null) {
                    System.err.println("Addresses: ");
                    for (int i = 0; i < addr.length; i++) {
                        System.err.println("  " + addr[i].toString());
                    }
                }
            /*} catch (UnsupportedEncodingException uee) {
                uee.printStackTrace();*/
            } catch (MessagingException me) {
                NoticeBoard.publish(new ExceptionNotice(me));
            }
        } else {
            throw new MessageSendViolation(); //TODO add reason
        }
    }
}
