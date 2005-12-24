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
 * Created: Will Scullin <scullin@netscape.com>,  8 Sep 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import grendel.ui.addressbook2.Addressbook;
import java.awt.Component;
import java.awt.Frame;
import java.awt.event.ActionEvent;
import java.io.IOException;
import java.text.MessageFormat;
import java.util.Enumeration;
import java.util.ResourceBundle;

import javax.swing.JFrame;
import javax.swing.ToolTipManager;

import grendel.prefs.base.UIPrefs;
import grendel.ui.prefs.Identities;
import grendel.ui.prefs.Servers;
import grendel.ui.prefs.General;
import grendel.ui.prefs.UI;
import grendel.storage.MailDrop;
import grendel.search.SearchFrame;

/* Temporarily removed because FilterMaster is broken (edwin)
import grendel.filters.FilterMaster;
*/

import grendel.composition.Composition;
import com.trfenv.parsers.Event;

/**
 *Generates a list of common Grendel events for use across the application.
 */
public class ActionFactory {
  static ExitAction fExitAction = new ExitAction();
  static NewMailAction fNewMailAction = new NewMailAction();
  static ComposeMessageAction fComposeMessageAction = new ComposeMessageAction();
  static PreferencesAction fPrefsAction = new PreferencesAction();
  static SearchAction fSearchAction = new SearchAction();
  static RunFiltersAction fRunFiltersAction = new RunFiltersAction();
  static ShowTooltipsAction fShowTooltipsAction = new ShowTooltipsAction();
  static RunIdentityPrefsAction fRunIdentityPrefsAction = new RunIdentityPrefsAction();
  static RunServerPrefsAction fRunServerPrefsAction = new RunServerPrefsAction();
  static RunGeneralPrefsAction fRunGeneralPrefsAction = new RunGeneralPrefsAction();
  static RunUIPrefsAction fRunUIPrefsAction = new RunUIPrefsAction();
  static ShowAddressBookAction fShowAddressBookAction = new ShowAddressBookAction();

  private static Event[] prefEvents;

  static int fIdent = 0;

  static Runnable fComposeMessageThread = new DummyComposeMessageThread();

  public static ExitAction GetExitAction() {
    return fExitAction;
  }

  public static NewMailAction GetNewMailAction() {
    return fNewMailAction;
  }

  public static ComposeMessageAction GetComposeMessageAction() {
    return fComposeMessageAction;
  }

  public static void SetComposeMessageThread(Runnable aThread) {
    fComposeMessageThread = aThread;
  }

  public static void setIdent(int aIdent) {
    System.out.println("setIdent "+aIdent);
    fIdent = aIdent;
  }

  public static int getIdent() {
    System.out.println("getIdent "+fIdent);
    return fIdent;
  }

  public static PreferencesAction GetPreferencesAction() {
    return fPrefsAction;
  }

  public static SearchAction GetSearchAction() {
    return fSearchAction;
  }

  public static RunFiltersAction GetRunFiltersAction() {
    return fRunFiltersAction;
  }

  public static ShowTooltipsAction GetShowTooltipsAction() {
    return fShowTooltipsAction;
  }

  public static RunIdentityPrefsAction GetRunIdentityPrefsAction() {
    return fRunIdentityPrefsAction;
  }

  public static RunServerPrefsAction GetRunServerPrefsAction() {
    return fRunServerPrefsAction;
  }

  public static RunGeneralPrefsAction GetRunGeneralPrefsAction() {
    return fRunGeneralPrefsAction;
  }

  public static RunUIPrefsAction GetRunUIPrefsAction() {
    return fRunUIPrefsAction;
  }

  public static ShowAddressBookAction GetShowAddressBookAction() {
    return fShowAddressBookAction;
  }

  /**
   *Returns an array of all the preferences events. Used in windows that
   *don't automatically have the preferences events supplied to them.
   */
  public static Event[] prefEvents() {
    if (prefEvents == null) {
      prefEvents = new Event[] {
        ActionFactory.GetRunGeneralPrefsAction(), ActionFactory.GetRunIdentityPrefsAction(),
        ActionFactory.GetRunServerPrefsAction(), ActionFactory.GetRunUIPrefsAction()
      };
    }
    return prefEvents;
  }
}

class ExitAction extends Event {

  public ExitAction() {
    super("appExit");

    setEnabled(true);
  }


  public void actionPerformed(ActionEvent aEvent) {
    GeneralFrame.CloseAllFrames();

    if (GeneralFrame.GetFrameList().length == 0) {
      System.exit(0);
    }
  }
}

class NewMailAction extends Event {

  public NewMailAction() {
    super("msgGetNew");

    setEnabled(true);
  }


  public void actionPerformed(ActionEvent aEvent) {
    ProgressFactory.NewMailProgress();
  }
}

class ComposeMessageAction extends Event {

  public ComposeMessageAction() {
    super("msgNew");

    setEnabled(true);
  }

  public void actionPerformed(ActionEvent aEvent) {
    new Thread(ActionFactory.fComposeMessageThread, "Composition Starter").start();
  }

}

class DummyComposeMessageThread implements Runnable {
  public void run() {
    System.out.println("This should not happen! DummyComposeMessageThread called.");
  }
}

class SearchAction extends Event {
  SearchAction() {
    super("appSearch");
  }

  public void actionPerformed(ActionEvent aEvent) {
    Frame frame = new SearchFrame();
    frame.setVisible(true);
    frame.toFront();
    frame.requestFocus();
  }
}

class RunFiltersAction extends Event {

  RunFiltersAction() {
    super("appRunFilters");
  }

  public void actionPerformed(ActionEvent aEvent) {
    /* Temporarily removed because FilterMaster is broken (edwin)
    FilterMaster fm = FilterMaster.Get();
    fm.applyFiltersToTestInbox();
    */
  }
}

class ShowAddressBookAction extends Event {

  ShowAddressBookAction() {
    super("openAddressBook");
  }

  public void actionPerformed(ActionEvent aEvent) {
    Addressbook AddressbookFrame = new Addressbook();
    AddressbookFrame.setVisible(true);
  }
}

class ShowTooltipsAction extends Event {

  ShowTooltipsAction() {
    super("appShowTooltips");

    boolean enabled = UIPrefs.GetMaster().getTooltips();
    ToolTipManager.sharedInstance().setEnabled(enabled);

    //    setSelected(enabled ? AbstractUICmd.kSelected : AbstractUICmd.kUnselected);
  }

  public void actionPerformed(ActionEvent aEvent) {
    boolean enabled = !ToolTipManager.sharedInstance().isEnabled();
    ToolTipManager.sharedInstance().setEnabled(enabled);
    //    setSelected(enabled ? AbstractUICmd.kSelected : AbstractUICmd.kUnselected);
    UIPrefs.GetMaster().setTooltips(enabled);
  }
}

class RunIdentityPrefsAction extends Event {

  RunIdentityPrefsAction() {
    super("prefIds");
  }

  public void actionPerformed(ActionEvent aEvent) {
    JFrame prefs = new Identities();
    prefs.setVisible(true);
  }
}

class RunServerPrefsAction extends Event {

  RunServerPrefsAction() {
    super("prefSrvs");
  }

  public void actionPerformed(ActionEvent aEvent) {
    JFrame prefs = new Servers();
    prefs.setVisible(true);
  }
}

class RunGeneralPrefsAction extends Event {

  RunGeneralPrefsAction() {
    super("prefGeneral");
  }

  public void actionPerformed(ActionEvent aEvent) {
    JFrame prefs = new General();
    prefs.setVisible(true);
  }
}

class RunUIPrefsAction extends Event {

  RunUIPrefsAction() {
    super("prefUI");
  }

  public void actionPerformed(ActionEvent aEvent) {
    JFrame prefs = new UI();
    prefs.setVisible(true);
  }
}
