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
 * Created: Jeff Galyan (talisman@anamorphic.com), 20 March, 2001.
 *
 * Contributors:
 */

package grendel.ui;

import java.awt.Component;
import java.awt.Frame;
import java.awt.event.ActionEvent;
import java.io.IOException;
import java.text.MessageFormat;
import java.util.Enumeration;
import java.util.ResourceBundle;

import javax.swing.JFrame;
import javax.swing.ToolTipManager;

//import netscape.orion.uimanager.AbstractUICmd;
//import netscape.orion.uimanager.IUICmd;

import grendel.prefs.base.UIPrefs;
import grendel.prefs.ui.Identities;
import grendel.prefs.ui.Servers;
import grendel.prefs.ui.General;
import grendel.prefs.ui.UI;
import grendel.storage.MailDrop;
import grendel.search.SearchFrame;

/* Temporarily removed because FilterMaster is broken (edwin)
import grendel.filters.FilterMaster;
*/

import grendel.composition.Composition;

import com.trfenv.parsers.Event;

class PreferencesAction extends Event {
  PreferencesAction fThis;

  public PreferencesAction() {
    super("appPrefs");
    fThis = this;

    setEnabled(true);
  }

  public void actionPerformed(ActionEvent aEvent) {
    Object source = aEvent.getSource();
    if (source instanceof Component) {
      Frame frame = Util.GetParentFrame((Component) source);
      if (frame instanceof JFrame) {
        new Thread(new PrefThread((JFrame) frame), "Prefs").start();
      }
    }
  }

  class PrefThread implements Runnable {
    JFrame fFrame;
    PrefThread(JFrame aFrame) {
      fFrame = aFrame;
    }
    public void run() {
      synchronized(fThis) {
        setEnabled(false);
        new PrefsDialog(fFrame);
        setEnabled(true);
      }
    }
  }
}

