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
 * Created: Will Scullin <scullin@netscape.com>,  8 Sep 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
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

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

//import netscape.orion.uimanager.AbstractUICmd;
//import netscape.orion.uimanager.IUICmd;

import grendel.storage.MailDrop;
import grendel.search.SearchFrame;
import grendel.filters.FilterMaster;

import grendel.composition.Composition;
import grendel.ui.UIAction;

public class ActionFactory {
  static ExitAction fExitAction = new ExitAction();
  static NewMailAction fNewMailAction = new NewMailAction();
  static ComposeMessageAction fComposeMessageAction = new ComposeMessageAction();
  static PreferencesAction fPrefsAction = new PreferencesAction();
  static SearchAction fSearchAction = new SearchAction();
  static RunFiltersAction fRunFiltersAction = new RunFiltersAction();
  static ShowTooltipsAction fShowTooltipsAction = new ShowTooltipsAction();
  
  static int fIdent = 0;
  
  static Runnable fComposeMessageThread = new DummyComposeMessageThread();

  static public ExitAction GetExitAction() {
    return fExitAction;
  }

  static public NewMailAction GetNewMailAction() {
    return fNewMailAction;
  }

  static public ComposeMessageAction GetComposeMessageAction() {
    return fComposeMessageAction;
  }

  static public void SetComposeMessageThread(Runnable aThread) {
    fComposeMessageThread = aThread;
  }
  
  static public void setIdent(int aIdent) {
    System.out.println("setIdent "+aIdent);
    fIdent = aIdent;
  }

  static public int getIdent() {
    System.out.println("getIdent "+fIdent);
    return fIdent;
  }

  static public PreferencesAction GetPreferencesAction() {
    return fPrefsAction;
  }

  static public SearchAction GetSearchAction() {
    return fSearchAction;
  }

  static public RunFiltersAction GetRunFiltersAction() {
    return fRunFiltersAction;
  }

  static public ShowTooltipsAction GetShowTooltipsAction() {
    return fShowTooltipsAction;
  }
}

class ExitAction extends UIAction {

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

class NewMailAction extends UIAction {

  public NewMailAction() {
    super("msgGetNew");

    setEnabled(true);
  }
  

  public void actionPerformed(ActionEvent aEvent) {
    ProgressFactory.NewMailProgress();
  }
}

class ComposeMessageAction extends UIAction {

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

class PreferencesAction extends UIAction {
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

class SearchAction extends UIAction {
  SearchAction() {
    super("appSearch");
  }

  public void actionPerformed(ActionEvent aEvent) {
    Frame frame = new SearchFrame();
    frame.show();
    frame.toFront();
    frame.requestFocus();
  }
}

class RunFiltersAction extends UIAction {

  RunFiltersAction() {
    super("appRunFilters");
  }

  public void actionPerformed(ActionEvent aEvent) {
    FilterMaster fm = FilterMaster.Get();
    fm.applyFiltersToTestInbox();
  }
}

class ShowTooltipsAction extends UIAction {

  ShowTooltipsAction() {
    super("appShowTooltips");

    boolean enabled =
      PreferencesFactory.Get().getBoolean("app.tooltips", true);
    ToolTipManager.sharedInstance().setEnabled(enabled);

    //    setSelected(enabled ? AbstractUICmd.kSelected : AbstractUICmd.kUnselected);
  }

  public void actionPerformed(ActionEvent aEvent) {
    boolean enabled = !ToolTipManager.sharedInstance().isEnabled();
    ToolTipManager.sharedInstance().setEnabled(enabled);
    //    setSelected(enabled ? AbstractUICmd.kSelected : AbstractUICmd.kUnselected);
    PreferencesFactory.Get().putBoolean("app.tooltips", enabled);
  }
}
