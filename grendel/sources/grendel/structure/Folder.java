/*
 * Folder.java
 *
 * Created on 18 August 2005, 23:45
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package grendel.structure;

import grendel.javamail.JMProviders;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;

import grendel.structure.events.Event;
import grendel.structure.events.JavaMailEvent;
import grendel.structure.events.JavaMailEventsListener;
import grendel.structure.events.Listener;
import grendel.structure.events.folder.FolderCreatedEvent;
import grendel.structure.events.folder.FolderDeletedEvent;
import grendel.structure.events.folder.FolderEvent;
import grendel.structure.events.folder.FolderListener;
import grendel.structure.events.folder.FolderMovedEvent;
import grendel.structure.events.folder.NewMessageEvent;
import grendel.structure.events.message.FlagChangedEvent;
import grendel.structure.events.message.MessageDeletedEvent;
import grendel.structure.events.message.MessageEvent;
import grendel.structure.events.message.MessageListener;
import grendel.structure.events.message.MessageMovedEvent;
import java.util.Arrays;

import java.util.Vector;

import javax.mail.FetchProfile;
import javax.mail.IllegalWriteException;
import javax.mail.MessagingException;


/**
 *
 * @author hash9
 */
public class Folder {
    protected javax.mail.Folder folder;
    
    /**
     * The event dispatcher class instance.
     */
    protected EventDispatcher dispatcher = new EventDispatcher();
    protected Folder parent;
    protected Server server;
    private FolderList all_folders = new FolderList();
    private FolderList sub_folders = new FolderList();
    private MessageList messages = new MessageList();
    private boolean deleted = false;
    
    /** Creates a new instance of Folder */
    public Folder(Folder parent, javax.mail.Folder folder) {
        this.parent = parent;
        this.folder = folder;
        
        JavaMailEventsListener fli = new JavaMailEventsListener(this.dispatcher);
        folder.addConnectionListener(fli);
        folder.addFolderListener(fli);
        folder.addMessageChangedListener(fli);
        folder.addMessageCountListener(fli);
    }
    
    /**
     * This returns a list of ALL folders don't call unless you're sure that's
     * what you want.
     * @return a List of the Folders contained in this folder.
     * This is seportate copy to the intenal List so addtions/removals have no
     * effect on this folder
     */
    public final FolderList getAllFolders() {
        updateAllFolderList();
        
        return new FolderList(all_folders);
    }
    
    /**
     * This returns a list of the subscribed folders in the case that this isn't
     * supported by the mail store the result is equivalent to getAllFolders.
     * This is the method that in general should be called
     * @return a List of the Folders contained in this folder.
     * This is seportate copy to the intenal List so addtions/removals have no
     * effect on this folder
     */
    public final FolderList getFolders() {
        updateSubscribedFolderList();
        
        return new FolderList(sub_folders);
    }
    
    /**
     * Get a folder by name.
     * @return the Folder, or null if it does not exist or an error occurs.
     */
    public Folder getFolder(String name) {
        try {
            javax.mail.Folder f = folder.getFolder(name);
            
            if ((f != null) && (f.exists())) {
                Folder folder = new Folder(this, f);
                all_folders.add(folder);
                
                return folder;
            }
            
            return null;
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
            
            return null;
        }
    }
    
    /**
     * Get a message by message ID, these start at <em>one</em>
     * @param i
     */
    public Message getMessage(int i) {
        //TODO This propably should update the messagelist properly, it doesn't because Tim's code takes ages to do that!
        if (!ensureOpen()) {
            return null;
        }
        
        try {
            javax.mail.Message m = folder.getMessage(i);
            
            for (Message mess : messages) {
                if (mess.equals(m)) {
                    return mess;
                }
            }
            
            Message message = new Message(this, m);
            messages.add(message);
            dispatcher.dispatch(new NewMessageEvent(this, message)); //New Messsage Event
            
            return message;
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
            
            return null;
        }
    }
    
    /**
     *
     * @return a MessageList of the Messages contained in this folder.
     * This is seportate copy to the intenal List so addtions/removals have no effect on this folder.
     */
    public MessageList getMessages() {
        updateMessageList();
        
        return new MessageList(messages);
    }
    
    /**
     * Returns the name of this Folder.
     */
    public String getName() {
        return folder.getName();
    }
    
    /**
     * Get the folder that is the parent of this one.
     * @return returns the parent of this folder, or <code>null</code> if this is a root folder.
     */
    public Folder getParent() {
        return parent;
    }
    
    /**
     * Return the MessageList containing only read messages.
     */
    public MessageList getReadMessages() {
        getServer().ensureConnection();
        
        MessageList ml = new MessageList(messages);
        int ml_size = ml.size();
        
        for (int i = 0; i < ml_size; i++) {
            Message m = ml.get(i);
            
            if (!m.isRead()) {
                ml.remove(i);
            }
        }
        
        return ml;
    }
    
    /**
     * Get the server in which this folder is contained.
     */
    public Server getServer() {
        if (server == null) {
            server = parent.getServer();
        }
        
        return server;
    }
    
    /**
     * Add or remove this folder to the subscribled folder list.
     * @param subscribe If true this folder is added to the subscribled folder list, otherwise it is removed.
     */
    public void setSubscribed(boolean subscribe) {
        try {
            folder.setSubscribed(true);
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }
    }
    
    /**
     * Add a listener to events in this folder.
     */
    public void addMessageEventListener(FolderListener fl) {
        dispatcher.listeners.add(fl);
    }
    
    /**
     * Copy the contents of this folder to the given folder.
     * <p><strong>Warning this method is <em>not</em> atomic. Some copying may have occured before failure.</strong></p>
     * @return Returns false on failure.
     */
    public boolean copyTo(Folder f) {
        getServer().ensureConnection();
        
        try {
            folder.copyMessages(folder.getMessages(), f.folder);
            
            FolderList fl = getAllFolders();
            
            for (Folder sub_f : fl) {
                Folder dest = f.getAllFolders().getByName(sub_f.getName());
                
                if (dest == null) {
                    dest = f.newSubfolder(sub_f.getName());
                }
                
                sub_f.copyTo(dest);
            }
            
            return true;
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
            
            return false;
        }
    }
    
    /**
     * <strong>WARNING this is a RECURSIVE call</strong>
     */
    public boolean delete() {
        getServer().ensureConnection();
        
        try {
            dispatcher.dispatch(new FolderDeletedEvent(this));
            
            return folder.delete(true);
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
            
            return false;
        }
    }
    
    /**
     * The generic equals method.
     */
    public boolean equals(Object o) {
        if (o instanceof javax.mail.Folder) {
            return equals((javax.mail.Folder) o);
        } else if (o instanceof Folder) {
            return equals((Folder) o);
        }
        
        return super.equals(o);
    }
    
    /**
     * The javax folder equals method.
     * This is used to see if this folder and that folder refer to the same underlying folder
     */
    public boolean equals(javax.mail.Folder folder) {
        String name_this = this.folder.getFullName();
        String name_that = folder.getFullName();
        
        return name_this.equals(name_that);
    }
    
    /**
     * The Folder equals method.
     * This is used to see if both folders refer to the same underlying folder
     */
    public boolean equals(Folder folder) {
        String name_this = this.folder.getFullName();
        String name_that = folder.folder.getFullName();
        
        return name_this.equals(name_that);
    }
    
    
    /**
     * Get the number of unread messages
     * @return the number of unread messages, or -1 if the method is unsupported
     */
    public int getNoUnreadMessages() {
        try {
            int no = folder.getUnreadMessageCount();
            if (no == -1) {
                ensureOpen();
                no = folder.getUnreadMessageCount();
            }
            return no;
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
            
            return -1;
        }
    }
    
    /**
     * Get the number of messages
     * @return the number of messages, or -1 if the method is unsupported
     */
    public int getNoMessages() {
        try {
            int no = folder.getMessageCount();
            if (no == -1) {
                ensureOpen();
                no = folder.getMessageCount();
            }
            return no;
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
            
            return -1;
        }
    }
    
    /**
     * Get the number of new messages
     * @return the number of new messages, or -1 if the method is unsupported
     */
    public int getNoNewMessages() {
        try {
            int no = folder.getNewMessageCount();
            if (no == -1) {
                ensureOpen();
                no = folder.getNewMessageCount();
            }
            return no;
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
            
            return -1;
        }
    }
    
    /**
     * Return the MessageList containing only unread messages.
     */
    public MessageList getUnReadMessages() {
        getServer().ensureConnection();
        
        MessageList ml = new MessageList(messages);
        int ml_size = ml.size();
        
        for (int i = 0; i < ml_size; i++) {
            Message m = ml.get(i);
            
            if (m.isRead()) {
                ml.remove(i);
            }
        }
        
        return ml;
    }
    
    /**
     * A hash code method to match the equals method.
     */
    public int hashcode() {
        return folder.getFullName().hashCode();
    }
    
    /**
     * Move the contents of this folder to the given folder.
     * This is done by calling:<br>
     * <blockquote>
     * <code>
     * copyTo(Folder f);<br>
     * delete();
     * </code>
     * </blockquote>
     * <p><strong>Warning this method is <em>not</em> atomic. Some copying or deletion may have occured before failure.</strong></p>
     * @return Returns false on failure.
     * @throws UnsupportedOperationException if the copy operation fails.
     */
    public boolean moveTo(Folder f) {
        getServer().ensureConnection();
        
        if (copyTo(f)) {
            dispatcher.dispatch(new FolderMovedEvent(this));
            
            return delete();
        }
        
        throw new UnsupportedOperationException();
    }
    
    /**
     * Create a new subfolder with the given name.
     */
    public Folder newSubfolder(String name) {
        getServer().ensureConnection();
        
        try {
            javax.mail.Folder new_folder = folder.getFolder(name);
            
            if (new_folder.exists()) {
                throw new javax.mail.MessagingException("Folder Exists");
            } else {
                try {
                    if (new_folder.create(javax.mail.Folder.HOLDS_FOLDERS |
                            javax.mail.Folder.HOLDS_MESSAGES)) {
                        Folder sub = new Folder(this, new_folder);
                        sub.folder.setSubscribed(true);
                        all_folders.add(sub);
                        sub_folders.add(sub);
                        dispatcher.dispatch(new FolderCreatedEvent(sub));
                        
                        return sub;
                    }
                    
                    throw new javax.mail.MessagingException(
                            "Folder Creation Failed!");
                } catch (MessagingException e) { // For some reason we couldn't create a folder that can contain both
                    // folders and messages. Try to make one that will hold messages.
                    
                    try {
                        if (new_folder.create(javax.mail.Folder.HOLDS_MESSAGES)) {
                            Folder sub = new Folder(this, new_folder);
                            sub.folder.setSubscribed(true);
                            all_folders.add(sub);
                            sub_folders.add(sub);
                            dispatcher.dispatch(new FolderCreatedEvent(sub));
                            
                            return sub;
                        }
                        
                        throw new javax.mail.MessagingException(
                                "Folder Creation Failed!");
                    } catch (MessagingException e1) { // For some reason we couldn't create a folder that can contains only
                        // messages. Try to make one that will hold folders.
                        
                        if (new_folder.create(javax.mail.Folder.HOLDS_FOLDERS)) {
                            Folder sub = new Folder(this, new_folder);
                            sub.folder.setSubscribed(true);
                            all_folders.add(sub);
                            sub_folders.add(sub);
                            dispatcher.dispatch(new FolderCreatedEvent(sub));
                            
                            return sub;
                        }
                    }
                }
            }
        } catch (MessagingException e) {
            NoticeBoard.publish(new ExceptionNotice(e));
        }
        
        return null;
    }
    
    /**
     * Remove a listener to events in this folder.
     */
    public void removeMessageEventListener(FolderListener fl) {
        dispatcher.listeners.remove(fl);
    }
    
    /**
     * Returns a String to represent this folder of
     * <i>foldername ( <code>getClass().getName() + '@' + Integer.toHexString(hashCode())</code> )</i>
     */
    public String toString() {
        return getName() + " (" + super.toString() + ")";
    }
    
    /**
     * Ensure the folder is open.
     * This also ensures that it is  connected.
     * @return Returns false if the method failed to ensure the folder is open.
     * It is recomend that the calling method use the false notification to abort any action.
     */
    protected boolean ensureOpen() {
        try {
            getServer().ensureConnection();
            
            if (!getServer().isConnected()) {
                throw new IllegalStateException("---> Connect FAILED!!!");
            }
            
            if (!folder.exists()) {
                all_folders.clear();
                sub_folders.clear();
                messages.clear();
                
                return false;
            }
            
            if ((folder.getType() & folder.HOLDS_MESSAGES) == 0) {
                messages.clear();
                
                return false;
            }
            
            if (!folder.isOpen()) {
                try {
                    folder.open(folder.READ_WRITE);
                } catch (Exception e) {
                    try {
                        folder.open(folder.READ_ONLY);
                    } catch (Exception e1) {
                        e.printStackTrace();
                        e1.printStackTrace();
                        
                        return false;
                    }
                }
            }
            
            return true;
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
            
            return false;
        }
    }
    
    /**
     * This updates the list of <em>all</em> the availible folders.
     */
    protected void updateAllFolderList() {
        getServer().ensureConnection();
        
        try {
            if (!folder.exists()) {
                all_folders.clear();
                sub_folders.clear();
                messages.clear();
                
                return;
            }
            
            if ((folder.getType() & folder.HOLDS_FOLDERS) == 0) {
                sub_folders.clear();
                all_folders.clear();
                
                return;
            }
            
            if (!folder.isOpen()) {
                try {
                    folder.open(folder.READ_WRITE);
                } catch (Exception e) {
                    try {
                        folder.open(folder.READ_ONLY);
                    } catch (Exception e1) {
                        e.printStackTrace();
                        e1.printStackTrace();
                    }
                }
            }
            
            javax.mail.Folder[] folders_a = folder.list();
            
            for (int i = 0; i < folders_a.length; i++) {
                if (!all_folders.contains(folders_a[i])) {
                    Folder f = new Folder(this, folders_a[i]);
                    all_folders.add(f);
                    dispatcher.dispatch(new FolderCreatedEvent(f));
                }
            }
            
            int no_folders = all_folders.size();
            
            for (int i = 0; i < no_folders; i++) {
                Folder f = all_folders.get(i);
                boolean found = false;
                
                for (int j = 0; j < folders_a.length; j++) {
                    if (f.equals(folders_a[i])) {
                        found = true;
                        j = folders_a.length;
                    }
                }
                
                if (!found) {
                    dispatcher.dispatch(new FolderDeletedEvent(f));
                    all_folders.remove(i);
                }
            }
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }
    }
    
    /**
     * This updates the list of <em>all</em> the messages.
     */
    protected void updateMessageList() {
        getServer().ensureConnection();
        
        try {
            if (!ensureOpen()) {
                return;
            }
            
            long s = System.currentTimeMillis();
            // int me_c = folder.getMessageCount(); // DEBUG ONLY
            javax.mail.Message[] messages_a = folder.getMessages();
            long d = System.currentTimeMillis()-s;
            System.out.println("Time to collect messages: " +((d/1000)) + " s");
            
            /* TODO Should this be an option to enable prefetching? Especially with disk caching it might be good but for now it's off
             * s = System.currentTimeMillis();
             * FetchProfile fp = new FetchProfile();
             * fp.add(FetchProfile.Item.ENVELOPE);
             * folder.fetch(messages_a, fp);
             * d = System.currentTimeMillis()-s;
             * System.out.println("Time to fetch messages: " +((d/1000)) + " s");
             */
            
            s = System.currentTimeMillis();
            if (messages.size()==0) {  // Done to help get a fast first listing of a folder
                for (int i = 0; i < messages_a.length; i++) {
                    Message m = new Message(this, messages_a[i]);
                    messages.add(m);
                }
            } else { // If there is all ready stuff there then we nead to iterate though the lot
                //XXX This is wastefull if there is a better way to get the information.
                for (int i = 0; i < messages_a.length; i++) {
                    if (!messages.contains(messages_a[i])) {
                        Message m = new Message(this, messages_a[i]);
                        messages.add(m);
                        dispatcher.dispatch(new NewMessageEvent(this, m));
                    }
                }
                
                //int no_messages=messages.size();
                //XXX this is very slow because it neads to load all the message headers...
                for (int i = 0; i < messages.size(); i++) {
                    Message m = messages.get(i);
                    boolean found = false;
                    
                    for (int j = 0; j < messages_a.length; j++) {
                        if (m.equals(messages_a[i])) {
                            found = true;
                            j = messages_a.length;
                        }
                    }
                    
                    if (!found) {
                        m.dispatcher.dispatch(new MessageDeletedEvent(m));
                        messages.remove(i);
                    }
                }
            }
            d = System.currentTimeMillis()-s;
            System.out.println("Time to process messages: " +((d/1000)) + " s");
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }
    }
    
    /**
     * This updates the list of the subscribed folders.
     */
    protected void updateSubscribedFolderList() {
        getServer().ensureConnection();
        
        try {
            if (!folder.exists()) {
                all_folders.clear();
                sub_folders.clear();
                messages.clear();
                
                return;
            }
            
            if ((folder.getType() & folder.HOLDS_FOLDERS) == 0) {
                sub_folders.clear();
                all_folders.clear();
                
                return;
            }
            
            javax.mail.Folder[] folders_a = folder.listSubscribed();
            
            for (int i = 0; i < folders_a.length; i++) {
                if (!sub_folders.contains(folders_a[i])) {
                    Folder f = new Folder(this, folders_a[i]);
                    sub_folders.add(f);
                    dispatcher.dispatch(new FolderCreatedEvent(f));
                }
            }
            
            for (int i = 0; i < sub_folders.size(); i++) {
                Folder f = sub_folders.get(i);
                boolean found = false;
                
                for (int j = 0; j < folders_a.length; j++) {
                    if (f.equals(folders_a[i])) {
                        found = true;
                        j = folders_a.length;
                    }
                }
                
                if (!found) {
                    dispatcher.dispatch(new FolderDeletedEvent(f));
                    sub_folders.remove(i);
                }
            }
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }
    }
    
    /**
     * The event dispatacher class.
     */
    private class EventDispatcher implements Runnable, Listener {
        Vector<FolderListener> listeners = new Vector<FolderListener>();
        private Event e;
        
        EventDispatcher() {
        }
        
        public void dispatch(FolderEvent e) {
            this.e = e;
            new Thread(this).start();
        }
        
        public void javaMailEvent(JavaMailEvent e) {
            this.e = e;
            new Thread(this).start();
        }
        
        public void run() {
            for (FolderListener fl : listeners) {
                try {
                    if (e instanceof JavaMailEvent) {
                        fl.javaMailEvent((JavaMailEvent) e);
                    } else if (e instanceof NewMessageEvent) {
                        fl.newMessage((NewMessageEvent) e);
                    } else if (e instanceof FolderDeletedEvent) {
                        fl.folderDeleted((FolderDeletedEvent) e);
                    } else if (e instanceof FolderCreatedEvent) {
                        fl.folderCreated((FolderCreatedEvent) e);
                    } else if (e instanceof FolderMovedEvent) {
                        fl.folderMoved((FolderMovedEvent) e);
                    }
                } catch (Exception e) {
                    NoticeBoard.publish(new ExceptionNotice(e));
                }
            }
        }
    }
}
