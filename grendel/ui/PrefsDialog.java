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
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.ui;

import javax.swing.JFrame;
import javax.swing.JOptionPane;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

//import netscape.orion.propeditor.PropertyEditorDlg;

import grendel.prefs.Prefs;

public class PrefsDialog {
  static String sPrefNames[] = {"mail.email_address",
                                "mail.host",
                                "mail.user",
                                "mail.directory",
                                "smtp.host"};

  public static void CheckPrefs(JFrame aParent) {
    if (!ValidPrefs()) {
      EditPrefs(aParent);
    }
  }

  public static void EditPrefs(JFrame aParent) {
    Object objs[] = {new Prefs()};
    //    PropertyEditorDlg.Edit(aParent, objs, false, true, "",
    //                     PropertyEditorDlg.UI_TREE);
    JOptionPane.showMessageDialog(aParent, "This part of the UI is\nstill being worked on.", "Under Construction", JOptionPane.INFORMATION_MESSAGE);
  }

  public static boolean ValidPrefs() {
    Preferences prefs = PreferencesFactory.Get();

    boolean res = true;
    /*
    for (int i = 0; i < sPrefNames.length; i++) {
      res &= !prefs.getString(sPrefNames[i], "").equals("");
    }
    */
    return res;
  }
}

