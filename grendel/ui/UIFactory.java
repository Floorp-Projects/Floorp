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
 * Created: Will Scullin <scullin@netscape.com>, 10 Dec 1997.
 */

package grendel.ui;

import java.awt.Image;

import javax.swing.Icon;
import javax.swing.ImageIcon;

import grendel.view.ViewedFolder;
import grendel.view.ViewedStore;

public class UIFactory {
  static UIFactory fInstance;

  ImageIcon   fInboxIcon;
  ImageIcon   fFolderIcon;
  ImageIcon   fNewsgroupIcon;
  ImageIcon   fLocalStoreIcon;
  ImageIcon   fRemoteStoreIcon;
  ImageIcon   fConnectedIcon;

  private UIFactory() {
    fFolderIcon =
      new ImageIcon(getClass().getResource("images/folder-small.gif"));
    fNewsgroupIcon =
      new ImageIcon(getClass().getResource("images/newsgroup-small.gif"));
    fLocalStoreIcon =
      new ImageIcon(getClass().getResource("images/storeLocal-small.gif"));
    fRemoteStoreIcon =
      new ImageIcon(getClass().getResource("images/storeRemote-small.gif"));
    fInboxIcon =
      new ImageIcon(getClass().getResource("images/inbox-small.gif"));
    fConnectedIcon =
      new ImageIcon(getClass().getResource("images/connected-small.gif"));
  }

  public static UIFactory Instance() {
    if (fInstance == null) {
      fInstance = new UIFactory();
    }
    return fInstance;
  }

  ImageIcon getFolderImageIcon(ViewedFolder aFolder,
                               boolean open,
                               boolean large) {
    ViewedStore store = aFolder.getViewedStore();
    if (aFolder == store) {
      if (store.getHost() == null) {
        return fLocalStoreIcon;
      } else {
        return fRemoteStoreIcon;
      }
    } else {
      if (store.getProtocol().equals("news")) {
        return fNewsgroupIcon;
      } else {
        if (aFolder.isInbox()) {
          return fInboxIcon;
        } else {
          return fFolderIcon;
        }
      }
    }
  }

  ImageIcon getFolderOverlayImageIcon(ViewedFolder aFolder,
                                      boolean open,
                                      boolean large) {

    ViewedStore store = aFolder.getViewedStore();
    if (aFolder == store) {
      if (store.isConnected()) {
        return fConnectedIcon;
      }
    }
    return null;
  }

  public Icon getFolderIcon(ViewedFolder aFolder,
                            boolean open,
                            boolean large) {
    return getFolderImageIcon(aFolder, open, large);
  }

  public Icon getFolderOverlayIcon(ViewedFolder aFolder,
                                   boolean open,
                                   boolean large) {
    return getFolderOverlayImageIcon(aFolder, open, large);
  }

  public Image getFolderImage(ViewedFolder aFolder,
                             boolean open,
                             boolean large) {
    return getFolderImageIcon(aFolder, open, large).getImage();
  }
}

