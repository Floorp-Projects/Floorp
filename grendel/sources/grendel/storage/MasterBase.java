/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Created: Terry Weissman <terry@netscape.com>, 27 Aug 1997.
 */

package grendel.storage;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

import java.io.File;
import java.io.FilenameFilter;
import java.util.Enumeration;
import java.util.Vector;

import javax.mail.Folder;

/** This whole thing is obsolete.  I'm just keeping it around until I figure
  out how to really do maildrops. */


class MasterBase {
  Vector fFolders = null;
  Vector fMailDrops = null;

  MasterBase() {
    // Do the timebomb.
//    java.util.Date before = new java.util.Date(97, 9, 1, 0, 0);
//    java.util.Date now = new java.util.Date();
//    java.util.Date then = new java.util.Date(97, 11, 25, 12, 00);
//    if (now.before(before) || now.after(then)) {
//      System.err.println("This software has expired");
//      System.exit(-1);
//    }
  }


  public Enumeration getMailDrops() {
//    if (fMailDrops == null) {
//      synchronized(this) {
//        if (fMailDrops == null) {
//          fMailDrops = new Vector();
//          PopMailDrop drop = new PopMailDrop();
//          fMailDrops.addElement(drop);
//          Preferences prefs = PreferencesFactory.Get();
//          drop.setHost(prefs.getString("pop.host", "mail"));
//          drop.setUser(prefs.getString("pop.user",
//                                       System.getProperties().getProperty("user.name")));
//          drop.setPassword(prefs.getString("pop.password", null));
//
//          for (Enumeration e = getFolders() ; e.hasMoreElements() ; ) {
//            Folder f = (Folder) e.nextElement();
//            if (f.getName().equalsIgnoreCase("inbox")) {
//              drop.setDestinationFolder(f);
//              break;
//            }
//          }
//
//          File popstate;
//          if (Constants.ISUNIX) {
//            popstate = new File(Constants.HOMEDIR, ".popstate");
//            // ### This is the wrong place; should be in .netscape dir or
//            // some such.
//          } else {
//            popstate = new File(prefs.getFile("mail.directory", null),
//                               "popstate.dat");
//          }
//          drop.setPopStateFile(popstate);
//          drop.setLeaveMessagesOnServer
//            (prefs.getBoolean("pop.leaveMailOnServer", true));
//        }
//      }
//    }
    return fMailDrops.elements();
  }

}


