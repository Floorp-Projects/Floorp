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

import java.text.Collator;

import javax.mail.Folder;
import javax.mail.MessagingException;
import javax.mail.Store;
import javax.mail.event.ConnectionEvent;
import javax.mail.event.ConnectionListener;
import javax.mail.event.FolderEvent;
import javax.mail.event.FolderListener;
import javax.mail.event.MessageChangedEvent;
import javax.mail.event.MessageChangedListener;
import javax.mail.event.MessageCountEvent;
import javax.mail.event.MessageCountListener;

import calypso.util.Comparer;
import calypso.util.QSort;

import grendel.storage.FolderExtraFactory;

class ViewedFolderBase implements ViewedFolder {

  static final String kInbox = "INBOX";

  Folder fFolder;
  ViewedFolderBase fThis;
  ViewedFolderBase fParent, fNext, fChild;
  ViewedStoreBase fViewedStore;
  boolean fBuilt = false;
  boolean fOpened = false;
  boolean fInbox = false;
  int fMessageCount = -1;
  int fUnreadCount = -1;
  int fUndeletedCount = -1;

  Thread fUpdateThread;

  /**
   * Constructor
   */

  public ViewedFolderBase(ViewedStoreBase aStore, ViewedFolderBase aParent,
                          Folder aFolder) {
    fViewedStore = aStore;
    fParent = aParent;
    fThis = this;

    setFolder(aFolder);
  }

  /**
   *
   */

  protected void setFolder(Folder aFolder) {
    fFolder = aFolder;
    if (fFolder != null) {
      FolderChangeListener l = new FolderChangeListener();
      fFolder.addConnectionListener(l);
      fFolder.addFolderListener(l);
      fFolder.addMessageCountListener(l);
      fFolder.addMessageChangedListener(l);
      fInbox = fFolder.getName().equalsIgnoreCase(kInbox);
    }
  }

  /**
   * Returns the associated folder
   */

  public Folder getFolder() {
    return fFolder;
  }

  /**
   * Returns whether we think the folder is opened
   */

  public boolean isOpen() {
    return fOpened;
  }

  /**
   * Returns the ViewedFolder associated with the given folder.
   * The Folder object inside the ViewedFolder may not be the
   * same as the object passed in, but it will always represent
   * the same folder
   */

  public ViewedFolder getViewedFolder(Folder aFolder)
    throws MessagingException {
    Folder folder = getFolder();
    if (folder == null) {
      return null;
    }
    String path1 = folder.getFullName();
    String path2 = aFolder.getFullName();
    if (path1.equals(path2)) {
      return this;
    } else {
      if (!path2.startsWith(path1)) // We've gone down the wrong branch
        throw new IllegalStateException();

      String path = path2.substring(path1.length());
      char separator = folder.getSeparator();
      String root;

      int index = path.indexOf(separator);
      if (index < 0) {
        root = path;
      } else {
        root = path.substring(0, index);
      }
      ViewedFolder temp = getFirstSubFolder();
      while (temp != null) {
        if (temp.getFolder().getName().equals(root)) {
          return temp.getViewedFolder(aFolder);
        }
        temp = temp.getNextFolder();
      }
    }
    return null;
  }

  /**
   * Get cached message count data, since some protocols
   * will hit the server for each call.
   */

  public int getMessageCount() {
    if (fMessageCount < 0) {
      updateMessageCount();
    }
    return fMessageCount;
  }

  /**
   * Get cached unread count, since some protocols will
   * hit the server for each call.
   */

  public int getUnreadMessageCount() {
    if (fUnreadCount < 0) {
      updateUnreadCount();
    }
    return fUnreadCount;
  }

  /**
   * Get cached undeleted message count, since some protocols will
   * hit the server for each call.
   */

  public int getUndeletedMessageCount() {
    if (fUndeletedCount < 0) {
      updateUndeletedCount();
    }
    return fUndeletedCount;
  }

  /**
   * Returns the next folder at this level.
   */

  public ViewedFolder getNextFolder() {
    return fNext;
  }

  /**
   * Returns the first subfolder of this folder.
   */

  public ViewedFolder getFirstSubFolder() {
    Folder folder = getFolder();
    if (folder != null && !fBuilt) {
      try {
        fViewedStore.checkConnected();
        if ((folder.getType() & Folder.HOLDS_FOLDERS) != 0) {
          Folder list[];
          if (fViewedStore.getVisible() == ViewedStore.kAll) {
            list = folder.list();
          } else {
            list = folder.listSubscribed();
          }

          if (list != null && list.length > 0) {
            if (fViewedStore.isSorted()) {
              QSort qsort = new QSort(new FolderComparer());
              qsort.sort(list);
            }
            fChild = new ViewedFolderBase(fViewedStore, this, list[0]);
            ViewedFolderBase temp = fChild;
            for (int i = 1; i < list.length; i++) {
              ViewedFolderBase next =
                new ViewedFolderBase(fViewedStore, this, list[i]);
              temp.setNextFolder(next);
              temp = next;
            }
          }
        }
        fBuilt = true;
      } catch (MessagingException e) {
        e.printStackTrace();
      }
    }
    return fChild;
  }

  /**
   * Returns the parent folder. Returns null for the default
   * folder for a session.
   */

  public ViewedFolder getParentFolder() {
    return fParent;
  }

  /**
   * Returns the associated store
   */

  public ViewedStore getViewedStore() {
    return fViewedStore;
  }

  /**
   * Returns whether this is an inbox
   */

  public boolean isInbox() {
    return fInbox;
  }

  void setNextFolder(ViewedFolderBase aNext) {
    fNext = aNext;
  }

  void addSubFolder(ViewedFolderBase aFolder) {
    if (fBuilt) {
      if (fChild == null) {
        fChild = aFolder;
      } else {
        ViewedFolderBase temp = fChild;
        while (temp.fNext != null) {
          temp = temp.fNext;
        }
        temp.fNext = aFolder;
      }
    } // else we don't care
  }

  void removeSubFolder(ViewedFolderBase aFolder) {
    ViewedFolderBase temp = fChild;

    if (temp == null) {
      return;
    } else if (aFolder == temp) {
      fChild = temp.fNext;
    } else {
      while (temp.fNext != null) {
        if (temp.fNext == aFolder) {
          temp.fNext = temp.fNext.fNext;
          aFolder.fNext = null;
          return;
        }
        temp = temp.fNext;
      }
    }
  }

  class FolderChangeListener implements MessageCountListener,
                                        MessageChangedListener,
                                        ConnectionListener,
                                        FolderListener
  {
    public void messagesAdded(MessageCountEvent aEvent) {
      updateCounts();
      fViewedStore.notifyFolderChanged(fThis);
    }

    public void messagesRemoved(MessageCountEvent aEvent) {
      updateCounts();
      fViewedStore.notifyFolderChanged(fThis);
    }

    public void folderCreated(FolderEvent e) {
      // It's unlikely we'll see this
    }

    public void folderDeleted(FolderEvent e) {
      if (fParent != null) {
        fParent.removeSubFolder(fThis);
      }

      fViewedStore.notifyFolderDeleted(fThis);
    }

    public void folderRenamed(FolderEvent e) {
      fViewedStore.notifyFolderChanged(fThis);
    }

    public void messageChanged(MessageChangedEvent aEvent) {
      updateCounts();
      fViewedStore.notifyFolderChanged(fThis);
    }

    public void closed(ConnectionEvent aEvent) {
      fOpened = false;
      fViewedStore.notifyFolderChanged(fThis);
    }

    public void disconnected(ConnectionEvent aEvent) {
      fOpened = false;
      fViewedStore.notifyFolderChanged(fThis);
    }

    public void opened(ConnectionEvent aEvent) {
      fOpened = true;
      updateCounts();
      fViewedStore.notifyFolderChanged(fThis);
    }
  }

  void updateMessageCount() {
    updateCounts();
  }

  void updateUnreadCount() {
    updateCounts();
  }

  void updateUndeletedCount() {
    updateCounts();
  }

  void updateCounts() {
    try {
      fViewedStore.checkConnected();
      fViewedStore.addFolderUpdate(fThis);
    } catch (MessagingException e) {
      e.printStackTrace();
    }
  }

  void setCounts(int messages, int unread, int undeleted) {
    if (messages != -1) {
      fMessageCount = messages;
    }
    if (unread != -1) {
      fUnreadCount = unread;
    }
    if (undeleted != -1) {
      fUndeletedCount = undeleted;
    }
    fViewedStore.notifyFolderChanged(fThis);
  }

  public String toString() {
    return fFolder.getFullName();
  }

  class FolderComparer implements Comparer {
    Collator fCollator;

    FolderComparer() {
      fCollator = Collator.getInstance();
      fCollator.setStrength(Collator.SECONDARY);
    }

    public int compare(Object a, Object b) {
      int res = 0;
      if (a instanceof Folder && b instanceof Folder) {
        String folderA = ((Folder) a).getName();
        String folderB = ((Folder) b).getName();
        res = fCollator.compare(folderA, folderB);
        if (res != 0) {
          if (folderA.equalsIgnoreCase(kInbox)) {
            res = -1;
          } else if (folderB.equalsIgnoreCase(kInbox)) {
            res = 1;
          }

        }
      }
      return res;
    }
  }
}
