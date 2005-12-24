/*
 * Server.java
 *
 * Created on 18 August 2005, 23:44
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure;

import grendel.javamail.JMProviders;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import grendel.prefs.accounts.Account__Receive;
import grendel.prefs.accounts.Account__Server;
import grendel.structure.events.Event;
import grendel.structure.events.JavaMailEvent;
import grendel.structure.events.JavaMailEventsListener;
import grendel.structure.events.Listener;
import grendel.structure.events.server.AutoConnectionEvent;
import grendel.structure.events.server.ConnectedEvent;
import grendel.structure.events.server.DisconnectedEvent;
import grendel.structure.events.server.ServerEvent;
import grendel.structure.events.server.ServerListener;
import java.util.Vector;
import javax.mail.AuthenticationFailedException;
import javax.mail.MessagingException;
import javax.mail.NoSuchProviderException;
import javax.mail.Store;
import javax.swing.JOptionPane;

/**
 *
 * @author hash9
 */
public class Server {
    protected Store store;
    protected Account__Receive account;
    protected boolean ensure_connection = true;
    
    /** Creates a new instance of Server */
    public Server(Account__Receive account) throws NoSuchProviderException {
        this.store=JMProviders.getJMProviders().getStore(account);
        this.account = account;
        JavaMailEventsListener fli = new JavaMailEventsListener(this.dispatcher);
        store.addConnectionListener(fli);
        store.addStoreListener(fli);
    }
    
    private String resolvePassword() {
        if (account instanceof Account__Server) {
            Account__Server account = (Account__Server) this.account;
            return account.getPassword(); //TODO Add Obsurring
        }
        return "";
    }
    
    public boolean connect() {
        try {
            if (account instanceof Account__Server) {
                Account__Server account = (Account__Server) this.account;
                store.connect(account.getHost(),account.getPort(), account.getUsername(), resolvePassword());
            } else {
                store.connect("",-1, "", "");
            }
            dispatcher.dispatch(new ConnectedEvent(this));
            return true;
        } catch (AuthenticationFailedException e) {
            NoticeBoard.publish(new ExceptionNotice(e));
            JOptionPane.showMessageDialog(null,"Login failed", "Grendel Error", JOptionPane.ERROR_MESSAGE);  // TODO REDO THIS!!!!!
            try {
                store.close();
            } catch (MessagingException ex) {
                ex.printStackTrace();
            }
            return false;
        } catch (MessagingException e) {
            
            NoticeBoard.publish(new ExceptionNotice(e.getNextException()));
            try {
                store.close();
            } catch (MessagingException ex) {
                ex.printStackTrace();
            }
            return false;
        }
    }
    
    public void disconnect() {
        try {
            store.close();
            dispatcher.dispatch(new DisconnectedEvent(this));
        } catch (MessagingException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }
    }
    
    public boolean isConnected() {
        return store.isConnected();
    }
    
    protected void ensureConnection() {
        if (ensure_connection && (! isConnected())) {
            dispatcher.dispatch(new AutoConnectionEvent(this));
            if (!connect()) {
                if (JMProviders.getSession().getDebug()) {
                    System.err.println("Connection Failed!");
                }
            }
        }
    }
    
    public void setEnsureConnection(boolean ensure_connection) {
        this.ensure_connection = ensure_connection;
    }
    
    public Account__Receive getAccount() {
        return account;
    }
    
    public Folder getRoot() {
        ensureConnection();
        try {
            return new FolderRoot(this,store.getDefaultFolder());
        } catch (java.lang.IllegalStateException ise) {
            NoticeBoard.publish(new ExceptionNotice(ise));
            return null;
        } catch (MessagingException e) {
            NoticeBoard.publish(new ExceptionNotice(e));
            return null;
        }
    }
    
    public String toString() {
        return this.account.getName();
    }
    
    
    protected void finalize() {
        try {
            System.err.println("Closing!");
            store.close();
        } catch (MessagingException ex) {
            ex.printStackTrace();
        }
    }
    
    protected  EventDispatcher dispatcher = new EventDispatcher();
    
    public void addMessageEventListener(ServerListener sl) {
        dispatcher.listeners.add(sl);
    }
    
    public void removeMessageEventListener(ServerListener sl) {
        dispatcher.listeners.remove(sl);
    }
    
    class EventDispatcher implements Runnable, Listener {
        Vector<ServerListener> listeners = new Vector<ServerListener>();
        private Event e;
        
        EventDispatcher() {
        }
        
        public void dispatch(ServerEvent e) {
            this.e = e;
            new Thread(this).start();
        }
        
        public void run() {
            for (ServerListener sl: listeners) {
                try {
                    if (e instanceof JavaMailEvent) {
                        sl.javaMailEvent((JavaMailEvent) e);
                    } else if (e instanceof ConnectedEvent) {
                        sl.connected((ConnectedEvent) e);
                    } else if (e instanceof DisconnectedEvent) {
                        sl.disconnected((DisconnectedEvent) e);
                    } else if (e instanceof AutoConnectionEvent) {
                        sl.autoReconnected((AutoConnectionEvent) e);
                    }
                } catch (Exception e) {
                    NoticeBoard.publish(new ExceptionNotice(e));
                }
            }
        }
        
        public void javaMailEvent(JavaMailEvent e) {
            this.e = e;
            new Thread(this).start();
        }
    }
}

