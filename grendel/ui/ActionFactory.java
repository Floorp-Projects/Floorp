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
 */

package grendel.ui;

import java.awt.Component;
import java.awt.Frame;
import java.awt.event.ActionEvent;
import java.io.IOException;
import java.text.MessageFormat;
import java.util.Enumeration;
import java.util.ResourceBundle;

import com.sun.java.swing.JFrame;
import com.sun.java.swing.ToolTipManager;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

import netscape.orion.uimanager.AbstractUICmd;
import netscape.orion.uimanager.IUICmd;

import grendel.storage.MailDrop;
import grendel.search.SearchFrame;
import grendel.filters.FilterMaster;

import grendel.composition.Composition;

public class ActionFactory {
  static IUICmd fExitAction = new ExitAction();
  static IUICmd fNewMailAction = new NewMailAction();
  static IUICmd fComposeMessageAction = new ComposeMessageAction();
  static IUICmd fPrefsAction = new PreferencesAction();
  static IUICmd fSearchAction = new SearchAction();
  static IUICmd fRunFiltersAction = new RunFiltersAction();
  static IUICmd fShowTooltipsAction = new ShowTooltipsAction();

  static public IUICmd GetExitAction() {
    return fExitAction;
  }

  static public IUICmd GetNewMailAction() {
    return fNewMailAction;
  }

  static public IUICmd GetComposeMessageAction() {
    return fComposeMessageAction;
  }

  static public void SetComposeMessageAction(IUICmd aAction) {
    fComposeMessageAction = aAction;
  }

  static public IUICmd GetPreferencesAction() {
    return fPrefsAction;
  }

  static public IUICmd GetSearchAction() {
    return fSearchAction;
  }

  static public IUICmd GetRunFiltersAction() {
    return fRunFiltersAction;
  }

  static public IUICmd GetShowTooltipsAction() {
    return fShowTooltipsAction;
  }
}

class ExitAction extends AbstractUICmd {
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

class NewMailAction extends AbstractUICmd {
  public NewMailAction() {
    super("msgGetNew");

    setEnabled(true);
  }

  public void actionPerformed(ActionEvent aEvent) {
    ProgressFactory.NewMailProgress();
  }
}

class ComposeMessageAction extends AbstractUICmd {
  public ComposeMessageAction() {
    super("msgNew");

    setEnabled(true);
  }

  public void actionPerformed(ActionEvent aEvent) {
    new Thread(new ComposeThread(), "Composition Starter").start();
  }

  class ComposeThread implements Runnable {
    public void run() {
      Composition frame = new Composition();

      frame.show();
      frame.requestFocus();
    }
  }
}

class PreferencesAction extends AbstractUICmd {
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
        PrefsDialog.EditPrefs(fFrame);
        setEnabled(true);
      }
    }
  }
}

class SearchAction extends AbstractUICmd {
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

class RunFiltersAction extends AbstractUICmd {
  RunFiltersAction() {
    super("appRunFilters");
  }

  public void actionPerformed(ActionEvent aEvent) {
    FilterMaster fm = FilterMaster.Get();
    fm.applyFiltersToTestInbox();
  }
}

class ShowTooltipsAction extends AbstractUICmd {
  ShowTooltipsAction() {
    super("appShowTooltips");

    boolean enabled =
      PreferencesFactory.Get().getBoolean("app.tooltips", true);
    ToolTipManager.sharedInstance().setEnabled(enabled);

    setSelected(enabled ? AbstractUICmd.kSelected : AbstractUICmd.kUnselected);
  }

  public void actionPerformed(ActionEvent aEvent) {
    boolean enabled = !ToolTipManager.sharedInstance().isEnabled();
    ToolTipManager.sharedInstance().setEnabled(enabled);
    setSelected(enabled ? AbstractUICmd.kSelected : AbstractUICmd.kUnselected);
    PreferencesFactory.Get().putBoolean("app.tooltips", enabled);
  }
}
