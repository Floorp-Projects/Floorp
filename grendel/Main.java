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
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Joel York <yorkjoe@charlie.acc.iit.edu>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel;

import java.io.File;

import java.util.Properties;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Session;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

import grendel.storage.BerkeleyStore;
import grendel.storage.MessageExtra;

import grendel.ui.MessageDisplayManager;
import grendel.ui.MultiMessageDisplayManager;
import grendel.ui.UnifiedMessageDisplayManager;
import grendel.ui.DialogAuthenticator;

import grendel.util.Constants;

/**
 * This launches the Grendel GUI.
 */

public class Main {
  static MessageDisplayManager fManager;

  public static void main(String argv[]) throws MessagingException {
    Preferences prefs = PreferencesFactory.Get();
    String pref = prefs.getString("mail.layout", "multi_pane");
    Properties props = new Properties();
    File mailDir;
    // I'm borrowing pretty heavily from jwz's TestFolderViewer here,
    // I may change this later, then again, I may not... (talisman)

    if (prefs.getString("mail.directory", "") == "") {
      mailDir = new File(Constants.HOMEDIR, "grndlmail");
      if (!mailDir.exists()) {
        if (mailDir.mkdir()) {
          //success; put the mail directory in the prefs (talisman)
          prefs.putString("mail.directory", mailDir.getPath());
        }
      } else {
        prefs.putString("mail.directory", mailDir.getPath());
      }
    }
    props.put("mail.directory", prefs.getString("mail.directory", ""));
    System.out.println(props.get("mail.directory"));
    
    Session session = Session.getDefaultInstance(props, new DialogAuthenticator());
    System.out.println(session);
    BerkeleyStore store = new BerkeleyStore(session);
    System.out.println(store);
    // Folder folder = store.getDefaultFolder().getFolder("Inbox");  
    Folder folder = store.getDefaultFolder();
    
    if (pref.equals("multi_pane")) {
      fManager = new UnifiedMessageDisplayManager();
    } else {
      fManager = new MultiMessageDisplayManager();
    }
    MessageDisplayManager.SetDefaultManager(fManager);
    //  fManager.displayMaster();
    fManager.displayMaster(folder.getFolder("Inbox"));
  }
}

