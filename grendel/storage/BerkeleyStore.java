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
 * Created: Terry Weissman <terry@netscape.com>, 22 Oct 1997.
 * Contributors: Joel York <joel_york@yahoo.com>
 */

package grendel.storage;

import calypso.util.Assert;

import java.awt.Component;
import java.io.File;
import java.net.URL;

import javax.mail.Folder;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Store;
import javax.mail.URLName;
//import javax.mail.event.ConnectionEvent;
import javax.mail.event.StoreEvent;

/** Store for Berkeley mail folders.
  <p>
  This class really shouldn't be public, but I haven't figured out how to
  tie into javamail's Session class properly.  So, instead of using
  <tt>Session.getStore(String)</tt>, you instead need to call
  <tt>BerkeleyStore.GetDefaultStore(Session)</tt>.
  */

public class BerkeleyStore extends Store {
  protected Folder defaultFolder;

  static protected BerkeleyStore DefaultStore = null;
  public static Store GetDefaultStore(Session s) {
    if (DefaultStore == null) {
      DefaultStore = new BerkeleyStore(s);
    }
    return DefaultStore;
  }


  public BerkeleyStore(Session s) {
    super(s, null);
  }

  public BerkeleyStore(Session s, URLName u) {
    super(s, u);
  }

  public void connect(String host,
                      String user,
                      String password) {
    notifyStoreListeners(StoreEvent.NOTICE, "opened"); // #### ???
//    notifyConnectionListeners(ConnectionEvent.OPENED);
  }

  public void connect() {
    notifyStoreListeners(StoreEvent.NOTICE, "opened"); // #### ???
//    notifyConnectionListeners(ConnectionEvent.OPENED);
  }

  public void close() {
    notifyStoreListeners(StoreEvent.NOTICE, "opened"); // #### ???
//    notifyConnectionListeners(ConnectionEvent.CLOSED);
    defaultFolder = null;
  }


  public Folder getDefaultFolder() {
    if (defaultFolder == null) {
      if (getURLName() == null || getURLName().getFile() == null) {
        defaultFolder =
          new BerkeleyFolder(this,
                             new File(session.getProperty("mail.directory")));
      } else {
        defaultFolder =
          new BerkeleyFolder(this,
                             new File(getURLName().getFile()));
      }
    }
    return defaultFolder;
  }

  public Folder getFolder(String name) throws MessagingException{
    return getDefaultFolder().getFolder(name);
  }

  public Folder getFolder(URL url) {
    URLName urlName = new URLName(url);
    return getFolder(urlName);
  }

  public Folder getFolder(URLName urlName) {
    Folder folder = new BerkeleyFolder(this, new File(urlName.getFile()));
    return folder;
  }
}
