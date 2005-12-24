/*
 * FolderListener.java
 *
 * Created on 23 August 2005, 11:35
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events;

import javax.mail.event.ConnectionListener;
import javax.mail.event.FolderListener;
import javax.mail.event.MessageChangedListener;
import javax.mail.event.MessageCountListener;
import javax.mail.event.StoreListener;
import javax.mail.event.TransportListener;

/**
 *
 * @author hash9
 */
public class JavaMailEventsListener implements ConnectionListener, FolderListener, MessageChangedListener, MessageCountListener, StoreListener, TransportListener {
    Listener l;
    
    /**
     * Creates a new instance of JavaMailEventsListener
     */
    public JavaMailEventsListener(Listener l) {
        this.l = l;
    }
    
    public void closed(javax.mail.event.ConnectionEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("closed: " + e.toString());
    }
    
    public void disconnected(javax.mail.event.ConnectionEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("disconnected: " + e.toString());
    }
    
    public void folderCreated(javax.mail.event.FolderEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("folderCreated: " + e.toString());
    }
    
    public void folderDeleted(javax.mail.event.FolderEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("folderDeleted: " + e.toString());
    }
    
    public void folderRenamed(javax.mail.event.FolderEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("folderRenamed: " + e.toString());
    }
    
    public void messageChanged(javax.mail.event.MessageChangedEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("messageChanged: " + e.toString());
    }
    
    public void messagesAdded(javax.mail.event.MessageCountEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("messagesAdded: " + e.toString());
    }
    
    public void messagesRemoved(javax.mail.event.MessageCountEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("messagesRemoved: " + e.toString());
    }
    
    public void opened(javax.mail.event.ConnectionEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("opened: " + e.toString());
    }
    
    public void messageDelivered(javax.mail.event.TransportEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("messageDelivered: " + e.toString());
    }
    
    public void messageNotDelivered(javax.mail.event.TransportEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("messageNotDelivered: " + e.toString());
    }
    
    public void messagePartiallyDelivered(javax.mail.event.TransportEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("messagePartiallyDelivered: " + e.toString());
    }
    
    public void notification(javax.mail.event.StoreEvent e) {
        l.javaMailEvent(new JavaMailEvent(e));
        System.err.println("notification: " + e.toString());
    }
    
}
