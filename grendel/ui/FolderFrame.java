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
 * Created: Will Scullin <scullin@netscape.com>, 18 Nov 1997.
 */

package grendel.ui;

import java.awt.BorderLayout;
import java.util.Enumeration;
import java.util.Vector;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;

import grendel.view.ViewedMessage;

import javax.swing.event.ChangeEvent;

import grendel.ui.UIAction;
import grendel.widgets.StatusEvent;
import grendel.widgets.TreePath;

public class FolderFrame extends GeneralFrame {
  static Vector fFolderFrames = new Vector();
  FolderPanel   fFolderPanel;

  /**
   * Identifying String
   */

  public static final String kID = "mail.folder";

  public FolderFrame(Folder aFolder) {
    super("folderFrameLabel", kID);

    fFolderPanel = new FolderPanel();
    fFolderPanel.addFolderPanelListener(new MessageSelectionListener());
    fPanel.add(fFolderPanel);
    //    fMenu = buildMenu("folderMain", Util.MergeActions(actions,
    //                                             fFolderPanel.getActions()));
    fMenu = buildMenu();
    getRootPane().setMenuBar(fMenu);

    fToolBar = fFolderPanel.getToolBar();
    //  fToolBar.addItem(ToolbarFactory.MakeINSToolbarItem(ToolBarLayout.CreateSpring(),
    //                                                       null));
// fToolBar.addItem(ToolbarFactory.MakeINSToolbarItem(fAnimation, null));
    fToolBarPanel.add(fToolBar);

    fStatusBar = buildStatusBar();
    fPanel.add(BorderLayout.SOUTH, fStatusBar);

    setFolder(aFolder);

    fFolderFrames.addElement(this);

    restoreBounds();
  }

  public void dispose() {
    saveBounds();
    fFolderFrames.removeElement(this);

    fFolderPanel.dispose();

    super.dispose();
  }

  public Folder getFolder() {
    return fFolderPanel.getFolder();
  }

  public void setFolder(Folder aFolder) {
    setTitle(fLabels.getString("folderFrameLabel"));
    fFolderPanel.setFolder(aFolder);
    if (aFolder != null) {
      setTitle(aFolder.getName() + fLabels.getString("folderSuffixFrameLabel"));
    }
  }

  public static FolderFrame FindFolderFrame(Folder aFolder) {
    Enumeration frames = fFolderFrames.elements();
    while (frames.hasMoreElements()) {
      FolderFrame frame = (FolderFrame) frames.nextElement();
      if (frame.getFolder() == aFolder) {
        return frame;
      }
    }
    return null;
  }

  //
  // MessageSelectionListener class
  //

  class MessageSelectionListener implements FolderPanelListener {
    public void loadingFolder(ChangeEvent aEvent) {
      startAnimation();
    }

    public void folderLoaded(ChangeEvent aEvent) {
      stopAnimation();
    }

    public void folderStatus(StatusEvent aEvent) {
      setStatusText(aEvent.getStatus());
    }

    public void folderSelectionChanged(ChangeEvent aEvent) {
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
  }
  UIAction actions[] = { ActionFactory.GetExitAction(),
                       ActionFactory.GetNewMailAction(),
                       ActionFactory.GetComposeMessageAction()};
}
