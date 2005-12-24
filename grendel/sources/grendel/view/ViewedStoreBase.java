/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Created: Will Scullin <scullin@netscape.com>,  2 Dec 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
 */
package grendel.view;
import grendel.prefs.base.ServerArray;

import grendel.storage.FolderExtraFactory;
import java.net.SocketException;
import java.util.Date;

import java.util.Vector;

import javax.mail.AuthenticationFailedException;
import javax.mail.Folder;
import javax.mail.MessagingException;
import javax.mail.Store;
import javax.mail.URLName;
import javax.mail.event.ConnectionEvent;
import javax.mail.event.ConnectionListener;
import javax.mail.event.FolderEvent;
import javax.mail.event.FolderListener;

import javax.swing.JOptionPane;
import javax.swing.event.EventListenerList;


public class ViewedStoreBase extends ViewedFolderBase implements ViewedStore {
    EventListenerList fListeners=new EventListenerList();
    Store fStore;
    Thread fUpdateThread;
    Vector fUpdateQueue;
    ViewedFolder fDefaultFolder;
    ViewedStore fNext;
    boolean fConnected;
    int fID;
    int fVisible=kSubscribed;
    
    /**
     * ViewedStoreBase constructor. This should be called before any
     * attempt to connect so that it can reflect the correct connection
     * state.
     */
    public ViewedStoreBase(Store aStore, int aID) {
        super(null, null, null);
        
        fID=aID;
        fViewedStore=this;
        fStore=aStore;
        
        fStore.addConnectionListener(new StoreConnectionListener());
    }
    
    /**
     * Returns the connected state of this store
     */
    public boolean isConnected() {
        return ((isLocal()) ? true : fConnected);
    }
    
    /**
     * Returns the store's default folder wrapped in a ViewedFolder
     * object.
     */
    public ViewedFolder getDefaultFolder() throws MessagingException {
        if (fDefaultFolder==null) {
            checkConnected();
            
            if (isConnected()) {
                /*if (fStore.getURLName().getProtocol().equalsIgnoreCase("nntp")) {
                    Folder root = fStore.getDefaultFolder();
                    Folder grendel = root.list("netscape.public.mozilla.mail-news")[0];
                    fDefaultFolder=new ViewedFolderBase(this, null, grendel);
                } else {*/
                    fDefaultFolder=new ViewedFolderBase(
                            this, null, fStore.getDefaultFolder());
                /*}*/
                
                setFolder(fDefaultFolder.getFolder());
            }
        }
        
        return fDefaultFolder;
    }
    
    /**
     * Returns the description for this store
     */
    public String getDescription() {
        return ServerArray.GetMaster().get(fID).getDescription();
    }
    
    /**
     * Returns the associated folder
     */
    public Folder getFolder() {
        try {
            getDefaultFolder();
            
            return super.getFolder();
        } catch (MessagingException e) {
            e.printStackTrace();
            
            return null;
        }
    }
    
    /**
     * Returns the host for this store. Returns null for a local store.
     */
    public String getHost() {
        return ServerArray.GetMaster().get(fID).getHost();
    }
    
    /**
     * Returns the id which identifies this store in the preferences
     */
    public int getID() {
        return fID;
    }
    
    public boolean isLocal() {
        URLName url=fStore.getURLName();
        
        return (url.getProtocol().equals("berkeley"));
    }
    
    /**
     * Returns the name for this store. This is the same as the description.
     */
    public String getName() {
        return getDescription();
    }
    
    /**
     * Returns the password.
     */
    public String getPassword() {
        return ServerArray.GetMaster().get(fID).getPassword();
    }
    
    /**
     * Returns the port used to connect. Returns -1 for the protocol default.
     */
    public int getPort() {
        return ServerArray.GetMaster().get(fID).getPort();
    }
    
    /**
     * Returns the protocol used by this store.
     */
    public String getProtocol() {
        return ServerArray.GetMaster().get(fID).getType();
    }
    
    /**
     * Returns the associated store.
     */
    public Store getStore() {
        return fStore;
    }
    
    /**
     * Returns the user name used to connect. May return null if
     * no user name was used.
     */
    public String getUsername() {
        return ServerArray.GetMaster().get(fID).getUsername();
    }
    
    /**
     * Sets which children to show for this store
     */
    public void setVisible(int aVisible) {
        fVisible=aVisible;
    }
    
    /**
     * Returns which children are showing for this store
     */
    public int getVisible() {
        return fVisible;
    }
    
    /**
     * Adds a ViewedStoreListener
     */
    public void addViewedStoreListener(ViewedStoreListener l) {
        fListeners.add(ViewedStoreListener.class, l);
    }
    
    /**
     * Removes a ViewedStoreListener
     */
    public void removeViewedStoreListener(ViewedStoreListener l) {
        fListeners.remove(ViewedStoreListener.class, l);
    }
    
    public String toString() {
        return ServerArray.GetMaster().get(fID).getDescription();
    }
    
    boolean isSorted() {
        return true;
    }
    
    void addFolderUpdate(ViewedFolderBase aFolder) {
        if (fUpdateQueue==null) {
            fUpdateQueue=new Vector();
        }
        
        if (!fUpdateQueue.contains(aFolder)) {
            fUpdateQueue.addElement(aFolder);
        }
        
        if (fUpdateThread==null) {
            fUpdateThread=new Thread(new UpdateCountThread(), "MsgCount");
            fUpdateThread.setPriority(Thread.MIN_PRIORITY);
            fUpdateThread.start();
        }
    }
    
    void checkConnected() throws MessagingException {
        if (!isConnected()) {
            //Thread connect = new Thread(new ConnectThread());
            //connect.start();
            connectStore();
        }
    }
    
    void connectStore() {
        try {
            if (fStore!=null) {
                fStore.connect(getHost(), getPort(), getUsername(), getPassword());
            }
        } catch (AuthenticationFailedException e) {
            JOptionPane.showMessageDialog(
                    null,
                    // labels.getString("loginFailedLabel"),
                    // labels.getString("errorDialogLabel"),
                    "Login failed", "Grendel Error", JOptionPane.ERROR_MESSAGE);
            e.printStackTrace();
            try {
                fStore.close();
            } catch (MessagingException ex) {
                ex.printStackTrace();
            }
        } catch (MessagingException e) {
            System.out.println("Got exception "+e+" while connecting to "+this);
            e.printStackTrace();
            try {
                fStore.close();
            } catch (MessagingException ex) {
                ex.printStackTrace();
            }
        }
    }
    
    void notifyFolderChanged(ViewedFolderBase aFolder) {
        Object[] listeners=fListeners.getListenerList();
        ViewedStoreEvent event=null;
        
        for (int i=0; i<(listeners.length-1); i+=2) {
            // Lazily create the event:
            if (event==null) {
                event=new ViewedStoreEvent(this, aFolder);
            }
            
            ((ViewedStoreListener) listeners[i+1]).folderChanged(event);
        }
    }
    
    void notifyFolderCreated(Folder aFolder) {
        try {
            if (aFolder.getParent()!=null) {
                try {
                    ViewedFolderBase parent=(ViewedFolderBase) getViewedFolder(
                            aFolder.getParent());
                    parent.addSubFolder(new ViewedFolderBase(this, parent, aFolder));
                } catch (MessagingException e) {
                    e.printStackTrace();
                }
            } else {
                throw new IllegalStateException("Can't create default folder?");
            }
        } catch (MessagingException e) {
            e.printStackTrace();
        }
    }
    
    void notifyFolderDeleted(ViewedFolderBase aFolder) {
        //  throws MessagingException {
        try {
            if (!aFolder.getFolder().exists()) {
                Object[] listeners=fListeners.getListenerList();
                ViewedStoreEvent event=null;
                
                for (int i=0; i<(listeners.length-1); i+=2) {
                    // Lazily create the event:
                    if (event==null) {
                        event=new ViewedStoreEvent(this, aFolder);
                    }
                    
                    ((ViewedStoreListener) listeners[i+1]).folderChanged(event);
                }
            } else {
                notifyFolderChanged(aFolder);
            }
        } catch (MessagingException e) {
            e.printStackTrace();
        }
    }
    
    class ConnectThread implements Runnable {
        public void run() {
            connectStore();
        }
    }
    
    class StoreConnectionListener implements ConnectionListener {
        public void closed(ConnectionEvent aEvent) {
            fConnected=false;
        }
        
        public void disconnected(ConnectionEvent aEvent) {
            // XXX Maybe throw up dialog?
            System.err.println("WARNING DISCONNTECTED! " + new Date().toString());
            fConnected=false;
        }
        
        public void opened(ConnectionEvent aEvent) {
            fConnected=true;
        }
    }
    
    class StoreFolderListener implements FolderListener {
        public void folderCreated(FolderEvent e) {
            notifyFolderCreated(e.getFolder());
        }
        
        public void folderDeleted(FolderEvent e) {
            // We'll let ViewedFolderBase handle this
        }
        
        public void folderRenamed(FolderEvent e) {
            // We'll let ViewedFolderBase handle this
        }
    }
    
    class UpdateCountThread implements Runnable {
        public void run() {
            while (!Thread.interrupted()) {
                if (fUpdateQueue.size()>0) {
                    ViewedFolderBase folder=(ViewedFolderBase) fUpdateQueue.firstElement();
                    updateFolder(folder);
                    fUpdateQueue.removeElement(folder);
                }
                
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                }
            }
        }
        
        void updateFolder(ViewedFolderBase viewedFolder) {
            try {
                Folder folder=viewedFolder.getFolder();
                
                if (folder!=null) {
                    if (!folder.isOpen()) {
                        try {
                            folder.open(Folder.READ_WRITE);
                        } catch (MessagingException e) {
                            try {
                                folder.open(Folder.READ_ONLY);
                            } catch (MessagingException e1) {
                                e1.printStackTrace();
                            }
                        }
                    }
                    if (! isConnected()) {
                        connectStore();
                    }
                    int messageCount=folder.getMessageCount();
                    int unreadCount=folder.getUnreadMessageCount();
                    int undeletedCount=FolderExtraFactory.Get(folder)
                    .getUndeletedMessageCount();
                    viewedFolder.setCounts(messageCount, unreadCount, undeletedCount);
                }
            } catch (MessagingException e) {
                if (e.getCause() instanceof SocketException) {
                    connectStore();
                }
                e.printStackTrace();
            } catch (IllegalStateException e) {
                e.printStackTrace();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}
