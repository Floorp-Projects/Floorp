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
 */

package grendel.ui;

import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.util.Enumeration;

import javax.swing.BoxLayout;
import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JMenuBar;
import javax.swing.JToolBar;
import javax.swing.JSplitPane;
import javax.swing.event.ChangeEvent;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

//import netscape.orion.toolbars.NSToolbar;
//import netscape.orion.toolbars.ToolbarFactory;
//import netscape.orion.toolbars.ToolBarLayout;
//import netscape.orion.uimanager.AbstractUICmd;
//import netscape.orion.uimanager.IUICmd;

import javax.mail.Store;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;

import grendel.ui.UIAction;

import grendel.view.ViewedMessage;
import grendel.widgets.CollapsiblePanel;
import grendel.widgets.GrendelToolBar;
import grendel.widgets.Splitter;
import grendel.widgets.Spring;
import grendel.widgets.StatusEvent;
import grendel.widgets.TreePath;

/**
 * The legendary three pane UI.
 */

public class UnifiedMessageDisplayManager extends MessageDisplayManager {
  UnifiedMessageFrame fMainFrame;

  public final static String SPLIT_TOP = "splitTop";
  public final static String SPLIT_LEFT = "splitLeft";
  public final static String SPLIT_RIGHT = "splitRight";
  public final static String STACKED = "stacked";

  /**
   * Displays a message given a Message object. If the message
   * is not in the currently selected folder, that folder will
   * be selected, loaded and displayed.
   */

  public void displayMessage(Message aMessage) {
    checkFrame();
    fMainFrame.display(aMessage.getFolder(), aMessage);
  }

  /**
   * Displays a folder given a folder object. If the message
   * being displayed is not in that folder, the message
   * display pane will be cleared.
   */

  public void displayFolder(Folder aFolder) {
    checkFrame();
    fMainFrame.display(aFolder, null);
  }

  /**
   * Displays folder given a Folder object and
   * selects and displays a message in that folder given a Message
   * object.
   */

  public void displayFolder(Folder aFolder, Message aMessage) {
    checkFrame();
    fMainFrame.display(aFolder, aMessage);
  }

  /**
   * Displays the master (A folder tree, for now). This should not
   * affect displayed folders or messages.
   */

  public void displayMaster() {
    checkFrame();
    fMainFrame.display(null, null);
  }

  /**
   * Displays the master with the given folder selected. If the
   * folder is not currently displayed, the folder will be loaded
   * in the folder message list pane, and the message pane will be
   * cleared.
   */

  public void displayMaster(Folder aFolder) {
    checkFrame();
    fMainFrame.display(aFolder, null);
  }

  void checkFrame() {
    if (fMainFrame == null) {
      fMainFrame = new UnifiedMessageFrame();
      fMainFrame.setVisible(true);
    }
    fMainFrame.toFront();
    fMainFrame.requestFocus();
  }
}

class UnifiedMessageFrame extends GeneralFrame {
  private final boolean DEBUG = true;
  MasterPanel   fFolders = null;
  FolderPanel   fThreads = null;
  MessagePanel  fMessage = null;
  JSplitPane    splitter1 = null, splitter2 = null;
  String        fLayout = null;
  JToolBar      fToolBar1 = null;
  int folderX = 350;
  int folderY = 225;
  int threadX = 200;
  int threadY = 200;

  public UnifiedMessageFrame() {
    super("appNameLabel", "mail.multi_pane");

    PrefsDialog.CheckPrefs(this);

    fFolders = new MasterPanel();
    fThreads = new FolderPanel();
    fMessage = new MessagePanel();

    fFolders.setPreferredSize(new Dimension(folderX, folderY));
    fThreads.setPreferredSize(new Dimension(threadX, threadY));

    splitter1 = new JSplitPane();
    splitter2 = new JSplitPane();
    splitter1.setOneTouchExpandable(true);
    splitter2.setOneTouchExpandable(true);

    PanelListener listener = new PanelListener();

    fFolders.addMasterPanelListener(listener);
    fThreads.addFolderPanelListener(listener);
    fMessage.addMessagePanelListener(listener);

    Preferences prefs = PreferencesFactory.Get();
    String layout = prefs.getString("mail.multi_pane.layout",
                                    UnifiedMessageDisplayManager.SPLIT_TOP);


    layoutPanels(layout);

    fMenu = buildMenu("menus.xml",  
                      Util.MergeActions(actions, Util.MergeActions(fFolders.getActions(), Util.MergeActions(fThreads.getActions(), fMessage.getActions()))));

    getRootPane().setJMenuBar(fMenu);

    GrendelToolBar masterToolBar = fFolders.getToolBar();
    GrendelToolBar folderToolBar = fThreads.getToolBar();
    GrendelToolBar messageToolBar = fMessage.getToolBar();


    fToolBar = Util.MergeToolBars(masterToolBar,
                             Util.MergeToolBars(folderToolBar,
                                                messageToolBar));

      fToolBarPanelConstraints.fill = GridBagConstraints.HORIZONTAL;
      fToolBarPanelConstraints.anchor = GridBagConstraints.WEST;
      // fToolBarPanelConstraints.weightx = 10.0;
      fToolBarPanel.add(fToolBar, fToolBarPanelConstraints);
      //  fToolBarPanelConstraints.weightx = 10.0;
      fToolBar.add(fToolBar.makeNewSpring());
      fToolBarPanelConstraints.weightx = 1.0;
      fToolBarPanelConstraints.fill = GridBagConstraints.NONE;
      fToolBarPanelConstraints.gridwidth = GridBagConstraints.REMAINDER;
      fToolBarPanelConstraints.anchor = GridBagConstraints.EAST;
      
      fToolBarPanel.add(fAnimation, fToolBarPanelConstraints);

    fStatusBar = buildStatusBar();
    fPanel.add(BorderLayout.SOUTH, fStatusBar);

    restoreBounds();
  }

  public void dispose() {
    saveBounds();

    Preferences prefs = PreferencesFactory.Get();

    prefs.putString("mail.multi_pane.folder_x",
		    Integer.toString(fFolders.getSize().width));
    prefs.putString("mail.multi_pane.folder_y",
		    Integer.toString(fFolders.getSize().height));
    prefs.putString("mail.multi_pane.thread_x",
		    Integer.toString(fThreads.getSize().width));
    prefs.putString("mail.multi_pane.thread_y",
		    Integer.toString(fThreads.getSize().height));

    fFolders.dispose();
    fThreads.dispose();
    fMessage.dispose();

    super.dispose();
  }

  public void display(Folder aFolder, Message aMessage) {
    if (aFolder != null) {
      fThreads.setFolder(aFolder);
      fMessage.setMessage(aMessage);
    }
  }

  void layoutPanels(String layout) {
    boolean touch = true;

    if (fLayout != null) {
      if (fLayout.equals(layout)) {
        return; // nothing to do
      }
      if (fLayout.equals(UnifiedMessageDisplayManager.STACKED)) {
	remove(splitter1);
      } else {
	remove(splitter1);
	remove(splitter2);
      }
      fPanel.remove(splitter1);
    }

    Preferences prefs = PreferencesFactory.Get();

    // read dimensions out of preferences
    try {
      int tx, ty, fx, fy; // temporary dimensions
      fx = 
	Integer.parseInt(prefs.getString("mail.multi_pane.folder_x", 
					 Integer.toString(folderX)));
      fy =
	Integer.parseInt(prefs.getString("mail.multi_pane.folder_y",
					 Integer.toString(folderY)));
      tx = 
	Integer.parseInt(prefs.getString("mail.multi_pane.thread_x",
					 Integer.toString(threadX)));
      ty =
	Integer.parseInt(prefs.getString("mail.multi_pane.thread_y",
					 Integer.toString(threadY)));
      folderX = fx;
      folderY = fy;
      threadX = tx;
      threadY = ty;
      // if the try bails, we use default
    } catch (NumberFormatException nf_ty) { 
      nf_ty.printStackTrace();
    }


    if (layout.equals(UnifiedMessageDisplayManager.STACKED)) {
      splitter1.setOrientation(JSplitPane.VERTICAL_SPLIT);
      splitter2.setOrientation(JSplitPane.VERTICAL_SPLIT);

      splitter1.setTopComponent(fFolders);
      splitter1.setBottomComponent(splitter2);
      splitter2.setTopComponent(fThreads);
      splitter2.setBottomComponent(fMessage);

      fFolders.setPreferredSize(new Dimension(folderX, folderY));
      fThreads.setPreferredSize(new Dimension(threadX, threadY));

      //      fStackedLayoutAction.setSelected(IUICmd.kSelected);
    } else if (layout.equals(UnifiedMessageDisplayManager.SPLIT_LEFT)) {
      System.out.println("Foo");
      splitter1.setOrientation(JSplitPane.HORIZONTAL_SPLIT);
      splitter2.setOrientation(JSplitPane.VERTICAL_SPLIT);
      
      splitter2.setLeftComponent(fFolders);
      splitter2.setRightComponent(fThreads);

      splitter1.setTopComponent(splitter2);
      splitter1.setBottomComponent(fMessage);
      //      fSplitLeftLayoutAction.setSelected(IUICmd.kSelected);
    } else if (layout.equals(UnifiedMessageDisplayManager.SPLIT_RIGHT)) {
      splitter2.setOrientation(JSplitPane.VERTICAL_SPLIT);
      splitter1.setOrientation(JSplitPane.HORIZONTAL_SPLIT);

      splitter2.setLeftComponent(fThreads);
      splitter2.setRightComponent(fMessage);

      splitter1.setTopComponent(fFolders);
      splitter1.setBottomComponent(splitter2);
      //      fSplitRightLayoutAction.setSelected(IUICmd.kSelected);
    } else { // Default: SPLIT_TOP
      splitter2.setOrientation(JSplitPane.HORIZONTAL_SPLIT);
      splitter1.setOrientation(JSplitPane.VERTICAL_SPLIT);

      splitter2.setLeftComponent(fFolders);
      splitter2.setRightComponent(fThreads);
      
      splitter1.setTopComponent(splitter2);
      splitter1.setBottomComponent(fMessage);

      fFolders.setPreferredSize(new Dimension(folderX, folderY));
      fThreads.setPreferredSize(new Dimension(threadX, threadY));

      //      fSplitTopLayoutAction.setSelected(IUICmd.kSelected);
    }

    fPanel.add("Center", splitter1);

    invalidate();
    validate();

    fLayout = layout;
  }

  LayoutAction fSplitLeftLayoutAction =
    new LayoutAction(UnifiedMessageDisplayManager.SPLIT_LEFT);
  LayoutAction fSplitRightLayoutAction =
    new LayoutAction(UnifiedMessageDisplayManager.SPLIT_RIGHT);
  LayoutAction fSplitTopLayoutAction =
    new LayoutAction(UnifiedMessageDisplayManager.SPLIT_TOP);
  LayoutAction fStackedLayoutAction =
    new LayoutAction(UnifiedMessageDisplayManager.STACKED);

  UIAction[] actions = { ActionFactory.GetExitAction(),
                       ActionFactory.GetNewMailAction(),
                       ActionFactory.GetComposeMessageAction(),
                       ActionFactory.GetPreferencesAction(),
                       ActionFactory.GetSearchAction(),
                       ActionFactory.GetRunFiltersAction(),
                       ActionFactory.GetShowTooltipsAction(),
                       fSplitLeftLayoutAction,
                       fSplitRightLayoutAction,
                       fSplitTopLayoutAction,
                       fStackedLayoutAction
  };


  class PanelListener implements MasterPanelListener, FolderPanelListener,
                                 MessagePanelListener
  {
    // FolderPanelListener

    public void loadingFolder(ChangeEvent aEvent) {
      // start animation
      fAnimation.start();
    }

    public void folderLoaded(ChangeEvent aEvent) {
      // stop animation
      fAnimation.stop();
    }

    public void folderStatus(StatusEvent aEvent) {
      setStatusText(aEvent.getStatus());
    }

    public void folderSelectionChanged(ChangeEvent aEvent) {
      TreePath path = null;
      Enumeration selection =
        ((FolderPanel) aEvent.getSource()).getSelection();

      if (selection.hasMoreElements()) {
        path = (TreePath) selection.nextElement();
      }
      if (path != null && !selection.hasMoreElements()) {
        // not multiple selection
        ViewedMessage node = (ViewedMessage) path.getTip();
        fMessage.setMessage(node.getMessage());
      } else {
        fMessage.setMessage(null);
      }
    }
    public void folderSelectionDoubleClicked(ChangeEvent aEvent) {
      TreePath path = null;
      Enumeration selection = ((FolderPanel)aEvent.getSource()).getSelection();

      MessageDisplayManager master = MultiMessageDisplayManager.Get();

      while (selection.hasMoreElements()) {
        path = (TreePath) selection.nextElement();
        if (path != null) {
          ViewedMessage node = (ViewedMessage) path.getTip();
          master.displayMessage(node.getMessage());
        }
      }
    }

    // MasterPanelListener

    public void masterSelectionChanged(ChangeEvent aEvent) {
      TreePath path = null;
      Enumeration selection =
        ((MasterPanel) aEvent.getSource()).getSelection();
      if (selection.hasMoreElements()) {
        path = (TreePath) selection.nextElement();
      }
      Object node = null;
      Folder folder = null;
      if (path != null && !selection.hasMoreElements()) {
        // not multiple selection
        node = path.getTip();
      }

      folder = MasterPanel.getFolder(node);

      if (folder != null) {
        try {
          if ((folder.getType() & Folder.HOLDS_MESSAGES) == 0) {
            folder = null;
          }
        } catch (MessagingException e) {
          folder = null;
        }
      }

      if (folder != null) {
        setTitle(folder.getName() +
                 fLabels.getString("folderSuffixFrameLabel"));
        fThreads.setFolder(folder);
      } else {
        setTitle(fLabels.getString("appNameLabel"));
        fThreads.setFolder(null);
      }
    }

    public void masterSelectionDoubleClicked(ChangeEvent aEvent) {
    }

    // MessagePanelListener

    public void loadingMessage(ChangeEvent aEvent) {
      startAnimation();
    }

    public void messageLoaded(ChangeEvent aEvent) {
      stopAnimation();
    }

    public void messageStatus(StatusEvent aEvent) {
      setStatusText(aEvent.getStatus());
    }
  }

  //
  // LayoutAction class
  //

  class LayoutAction extends UIAction {
    ImageIcon fIcon;
    String action;
    public LayoutAction(String aAction) {
      super(aAction);
      setEnabled(true);
      action = aAction;
      fIcon = new ImageIcon(getClass().getResource("images/" +
                                                   aAction + ".gif"));
    }

    public void actionPerformed(ActionEvent aEvent) {
      layoutPanels(action);
    }

    public ImageIcon getEnabledIcon() {
      return fIcon;
    }
  }
}
