/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Will Scullin <scullin@netscape.com>,  2 Dec 1997.
 */

package grendel.view;

import java.util.Vector;

import javax.mail.AuthenticationFailedException;
import javax.mail.Folder;
import javax.mail.MessagingException;
import javax.mail.Store;
import javax.mail.event.ConnectionEvent;
import javax.mail.event.ConnectionListener;
import javax.mail.event.FolderEvent;
import javax.mail.event.FolderListener;

import javax.swing.JOptionPane;
import javax.swing.event.EventListenerList;

import calypso.util.PreferencesFactory;

import grendel.storage.FolderExtraFactory;

public class ViewedStoreBase extends ViewedFolderBase implements ViewedStore {

  Vector fUpdateQueue;
  Thread fUpdateThread;

  ViewedFolder fDefaultFolder;
  Store fStore;
  String fProto;
  String fHost;
  String fUser;
  String fPrefBase;
  int fPort;
  ViewedStore fNext;
  boolean fConnected;
  boolean fSorted;

  int fVisible = kSubscribed;

  EventListenerList fListeners = new EventListenerList();

  /**
   * ViewedStoreBase constructor. This should be called before any
   * attempt to connect so that it can reflect the correct connection
   * state.
   */

  public ViewedStoreBase(Store aStore, String aProto, String aHost,
                         int aPort, String aUser) {
    super(null, null, null);

    fViewedStore = this;
    fStore = aStore;
    fProto = aProto;
    fHost = aHost;
    fPort = aPort;
    fUser = aUser;

    fPrefBase = "mail." + aProto + (aHost != null ? "-" + aHost : "");
    fSorted = PreferencesFactory.Get().getBoolean(fPrefBase + ".sort", false);

    fStore.addConnectionListener(new StoreConnectionListener());
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
   * Returns the associated store.
   */

  public Store getStore() {
    return fStore;
  }

  /**
   * Returns the store's default folder wrapped in a ViewedFolder
   * object.
   */

  public ViewedFolder getDefaultFolder() throws MessagingException {
    if (fDefaultFolder == null) {
      checkConnected();
      if (true) {
        fDefaultFolder = new ViewedFolderBase(this, null,
                                              fStore.getDefaultFolder());

        setFolder(fDefaultFolder.getFolder());
      }
    }
    return fDefaultFolder;
  }

  /**
   * Returns the protocol used by this store.
   */

  public String getProtocol() {
    return fProto;
  }

  /**
   * Returns the host for this store. Returns null for a local store.
   */

  public String getHost() {
    return fHost;
  }

  /**
   * Returns the user name used to connect. May return null if
   * no user name was used.
   */

  public String getUsername() {
    return fUser;
  }

  /**
   * Returns the port used to connect. Returns -1 for the protocol default.
   */

  public int getPort() {
    return fPort;
  }

  /**
   * Returns the connected state of this store
   */

  public boolean isConnected() {
    return fConnected;
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

  public void removeViewedStoreListener(ViewedStoreListener l)  {
    fListeners.remove(ViewedStoreListener.class, l);
  }

  void checkConnected() throws MessagingException {
    if (!isConnected()) {
      boolean success = false;

      try {
        if (fStore != null) {
          fStore.connect(getHost(), getUsername(), null);
        }
        success = true;
      } catch (AuthenticationFailedException e) {
        JOptionPane.showMessageDialog(null,
                                      // labels.getString("loginFailedLabel"),
                                      // labels.getString("errorDialogLabel"),
                                      "Login failed",
                                      "Grendel Error",
                                      JOptionPane.ERROR_MESSAGE);

      } catch (MessagingException e) {
        System.out.println("Got exception " + e +
                           " while connecting to " + this);
        e.printStackTrace();
      }
    }
  }

  boolean isSorted() {
    return fSorted;
  }

  /**
   * Sets which children to show for this store
   */

  public void setVisible(int aVisible) {
    fVisible = aVisible;
  }

  /**
   * Returns which children are showing for this store
   */

  public int getVisible() {
    return fVisible;
  }

  public String toString() {
    StringBuffer buffer = new StringBuffer();
    buffer.append(fProto);
    if (fHost != null) {
      buffer.append(":");
      buffer.append(fHost);
    }
    return buffer.toString();
  }

  void addFolderUpdate(ViewedFolderBase aFolder) {
    if (fUpdateQueue == null) {
      fUpdateQueue = new Vector();
    }

    if (!fUpdateQueue.contains(aFolder)) {
      fUpdateQueue.addElement(aFolder);
    }

    if (fUpdateThread == null) {
      fUpdateThread = new Thread(new UpdateCountThread(), "MsgCount");
      fUpdateThread.setPriority(Thread.MIN_PRIORITY);
      fUpdateThread.start();
    }
  }

  void notifyFolderChanged(ViewedFolderBase aFolder) {
    Object[] listeners = fListeners.getListenerList();
    ViewedStoreEvent event = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      // Lazily create the event:
      if (event == null)
        event = new ViewedStoreEvent(this, aFolder);
      ((ViewedStoreListener)listeners[i+1]).folderChanged(event);
    }
  }

  void notifyFolderDeleted(ViewedFolderBase aFolder) {
  //  throws MessagingException {
    try {
    if (!aFolder.getFolder().exists()) {
      Object[] listeners = fListeners.getListenerList();
      ViewedStoreEvent event = null;
      for (int i = 0; i < listeners.length - 1; i += 2) {
        // Lazily create the event:
        if (event == null)
          event = new ViewedStoreEvent(this, aFolder);
        ((ViewedStoreListener)listeners[i+1]).folderChanged(event);
      }
    } else {
      notifyFolderChanged(aFolder);
    }
    } catch (MessagingException e) {
      e.printStackTrace();
    }
  }

  void notifyFolderCreated(Folder aFolder) {
    try{
    if (aFolder.getParent() != null) {
      try {
        ViewedFolderBase parent =
          (ViewedFolderBase) getViewedFolder(aFolder.getParent());
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

  class StoreConnectionListener implements ConnectionListener {
    public void closed(ConnectionEvent aEvent) {
      fConnected = false;
    }

    public void disconnected(ConnectionEvent aEvent) {
      // XXX Maybe throw up dialog?
      fConnected = false;
    }

    public void opened(ConnectionEvent aEvent) {
      fConnected = true;
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
    void updateFolder(ViewedFolderBase viewedFolder) {
      try {
        Folder folder = viewedFolder.getFolder();
        if (folder != null) {

          if (!folder.isOpen()) {
            folder.open(Folder.READ_WRITE);
          }
          int messageCount = folder.getMessageCount();
          int unreadCount = folder.getUnreadMessageCount();
          int undeletedCount =
            FolderExtraFactory.Get(folder).getUndeletedMessageCount();
          viewedFolder.setCounts(messageCount, unreadCount, undeletedCount);
        }
      } catch (MessagingException e) {
      } catch (IllegalStateException e) {
      } catch (Exception e) {
        e.printStackTrace();
      }
    }

    public void run() {
      while (!Thread.interrupted()) {
        if (fUpdateQueue.size() > 0) {
          ViewedFolderBase folder =
            (ViewedFolderBase) fUpdateQueue.firstElement();
          updateFolder(folder);
          fUpdateQueue.removeElement(folder);
        }
        try {
          Thread.sleep(100);
        } catch (InterruptedException e) {}
      }
    }
  }
}
