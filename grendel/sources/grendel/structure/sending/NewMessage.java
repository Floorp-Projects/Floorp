/*
 * NewMessage.java
 *
 * Created on 23 August 2005, 16:34
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.sending;

import grendel.javamail.JMProviders;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import grendel.renderer.Renderer;
import grendel.storage.addressparser.RFC822Mailbox;
import grendel.storage.addressparser.AddressList;
import grendel.structure.Message;
import java.io.CharArrayWriter;
import java.io.IOException;
import java.io.InputStream;
import java.text.SimpleDateFormat;
import javax.mail.Address;
import javax.mail.MessagingException;
import javax.mail.internet.MimeMessage;

/**
 *
 * @author hash9
 */
public class NewMessage {
    public javax.mail.Message message;
    private boolean sent = false;
    private String content_type="";
    
    /** Creates a new instance of NewMessage */
    public NewMessage(javax.mail.Message newmessage, javax.mail.Message oldmessage, Message m) throws MessagingException, IOException {
        this.message = newmessage;
        addressees[0] = parse(message.getRecipients(javax.mail.Message.RecipientType.TO));
        addressees[1] = parse(message.getRecipients(javax.mail.Message.RecipientType.CC));
        addressees[2] = parse(message.getRecipients(javax.mail.Message.RecipientType.BCC));
        if (newmessage.getHeader("Content-Type") == null || newmessage.getHeader("Content-Type").length==0) {
            if (oldmessage.getHeader("Content-Type") != null) {
                content_type= "text/html";
                InputStream is = Renderer.render(oldmessage,true);
                CharArrayWriter caw = new CharArrayWriter();
                int i = is.read();
                while (i!=-1) {
                    caw.write(i);
                    i = is.read();
                }
                body.append("On ");
                body.append(new SimpleDateFormat("EEE, yyyy-MM-dd, 'at' HH:mm:ss Z").format(m.getSentDate()));
                body.append(", ");
                body.append(m.getAuthor().getPersonal());
                body.append(" wrote:<br>\n");
                body.append("<blockquote type=\"cite\">");
                
                StringBuilder quote = new StringBuilder();
                {
                    quote.append(caw.toCharArray());
                    /*insertQuoteAfterAll(body,"<br>");
                    insertQuoteAfterAll(quote,"<br>");
                    insertQuoteAfterAll(quote,"<div>");
                    insertQuoteAfterAll(quote,"<p>");*/
                }
                body.append(quote);
                body.append("</blockquote>");
            }
        } else if (message.getContentType().equalsIgnoreCase("text/plain")) {
            content_type= "text/plain";
            body.append(message.getContent());
        } else {
            content_type= message.getContentType();
            InputStream is = Renderer.render(message,true);
            CharArrayWriter caw = new CharArrayWriter();
            int i = 0;
            while (i!=-1) {
                i = is.read();
                caw.write(i);
            }
            body.append(caw.toCharArray());
        }
    }
    
    private void insertQuoteAfterAll(StringBuilder sb, String key) {
        insertAfterAll(sb, key, "&gt;&nbsp;");
    }
    
    private void insertAfterAll(StringBuilder sb, String key, String replace) {
        StringBuilder lcase = new StringBuilder(sb.toString().toLowerCase());
        int key_length = key.length();
        int start = 0;
        int off = sb.indexOf(key,start);
        while (off!=-1) {
            sb.insert(off+key_length,replace);
            lcase.insert(off+key_length,replace.toLowerCase());
            start = off +key_length;
            off = lcase.indexOf(key,start);
        }
    }
    
    private void insertQuoteBeforeAll(StringBuilder sb, String key) {
        insertBeforeAll(sb, key, "&gt;&nbsp;");
    }
    
    private void insertBeforeAll(StringBuilder sb, String key, String replace) {
        StringBuilder lcase = new StringBuilder(sb.toString().toLowerCase());
        int key_length = replace.length()+key.length();
        int start = 0;
        int off = sb.indexOf(key,start);
        while (off!=-1) {
            sb.insert(off,replace);
            lcase.insert(off,replace.toLowerCase());
            start = off +key_length;
            off = lcase.indexOf(key,start);
        }
    }
    
    private AddressList parse(Address[] addresses) {
        AddressList mbl = new AddressList();
        if (addresses!= null) {
            for (int i=0; i<addresses.length;i++) {
                mbl.add(addresses[i]);
            }
        }
        return mbl;
    }
    
    public NewMessage() {
        this.message = new MimeMessage(JMProviders.getSession());
        for (int i =0;i<addressees.length;i++) {
            addressees[i] = new AddressList();
        }
    }
    
    public void addToAddress(RFC822Mailbox address) {
        addressees[0].add(address);
    }
    
    public void removeToAddress(RFC822Mailbox address) {
        addressees[0].remove(address);
    }
    
    public void addCCAddress(RFC822Mailbox address) {
        addressees[1].add(address);
    }
    
    public void removeCCAddress(RFC822Mailbox address) {
        addressees[1].remove(address);
    }
    
    public void addBCCAddress(RFC822Mailbox address) {
        addressees[2].add(address);
    }
    
    public void removeBCCAddress(RFC822Mailbox address) {
        addressees[2].remove(address);
    }
    
    private AccountIdentity account_identity;
    private AddressList[] addressees = new AddressList[3];
    private Address[] from = new Address[2];
    
    public void prepareSend() throws MessageSendViolation {
        check();
        if (account_identity==null) throw new MessageSendViolation();
        try {
            message.setHeader("X-Mailer", "Grendel [Development Version New Revision]"); //and proud of it!
            message.setSentDate(new java.util.Date());     //set date to now.
            message.setFrom(from[0]);
            message.setRecipients(javax.mail.Message.RecipientType.TO,addressees[0].getAddressArray());
            message.setRecipients(javax.mail.Message.RecipientType.CC,addressees[1].getAddressArray());
            message.setRecipients(javax.mail.Message.RecipientType.BCC,addressees[2].getAddressArray());
            message.setContent(body.toString(),content_type);
        } catch (MessagingException me) {
            NoticeBoard.publish(new ExceptionNotice(me));
        }
        //throw new MessageSendViolation();
    }
    
    public AccountIdentity getAccountIdentity() {
        return account_identity;
    }
    
    public void setAccountIdentity(AccountIdentity account_identity) {
        this.account_identity = account_identity;
        from[0] = new RFC822Mailbox(account_identity.getIdentity().getName(),account_identity.getIdentity().getEMail());
    }
    
    public javax.mail.Message getJavaMailMessage() {
        return message;
    }
    
    public AddressList getAddresses() {
        AddressList al = new AddressList();
        for (int i = 0; i< addressees.length; i++) {
            al.addAll(addressees[i]);
        }
        return al;
    }
    
    public AddressList getToAddresess() {
        return new AddressList(addressees[0]);
    }
    
    public AddressList getCCAddresess() {
        return new AddressList(addressees[1]);
    }
    
    public AddressList getBCCAddresess() {
        return new AddressList(addressees[2]);
    }
    
    public Address getFromAddress() {
        return from[0];
    }
    
    public void markAsSent() {
        sent = true;
    }
    
    private void check() {
        if (sent) {
            throw new IllegalStateException("Message has been sent!");
        }
    }
    
    public void setSubject(String subject) {
        try {
            message.setSubject(subject);
        } catch (MessagingException me) {
            NoticeBoard.publish(new ExceptionNotice(me));
        }
    }
    
    public String getSubject() {
        try {
            return message.getSubject();
        } catch (MessagingException me) {
            NoticeBoard.publish(new ExceptionNotice(me));
            return null;
        }
    }
    
    private StringBuilder body = new StringBuilder();
    
    public void setBody(StringBuilder body) {
        this.body = body;
    }
    
    public StringBuilder getBody() {
        return this.body;
    }
    
    public DraftMessage getDraft() {
        throw new UnsupportedOperationException();
    }

    public void setContentType(String content_type) {
        this.content_type = content_type;
    }
    

}

