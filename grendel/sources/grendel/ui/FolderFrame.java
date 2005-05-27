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
 * Created: Will Scullin <scullin@netscape.com>, 18 Nov 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import java.awt.BorderLayout;
import java.util.Enumeration;
import java.util.Vector;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;

import grendel.view.ViewedMessage;

import javax.swing.tree.TreePath;
import javax.swing.event.ChangeEvent;

import grendel.widgets.StatusEvent;

import com.trfenv.parsers.Event;

public class FolderFrame extends GeneralFrame {
  static Vector fFolderFrames = new Vector();
  FolderPanel   fFolderPanel;

  public FolderFrame(Folder aFolder) {
    super("folderFrameLabel", "folder");

    fFolderPanel = new FolderPanel();
    fFolderPanel.addFolderPanelListener(new MessageSelectionListener());
    fPanel.add(fFolderPanel);
    //    fMenu = buildMenu("folderMain", Util.MergeActions(actions,
    //                                             fFolderPanel.getActions()));
    XMLMenuBuilder builder = new XMLMenuBuilder(Util.MergeActions(actions, fFolderPanel.getActions()));
    fMenu = builder.buildFrom("ui/grendel.xml", this);
    getRootPane().setJMenuBar(fMenu);

    fToolBar = fFolderPanel.getToolBar();
    fToolBarPanel.setComponent(fToolBar);

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
          ViewedMessage node = (ViewedMessage) path.getPath()[path.getPath().length -1];
          master.displayMessage(node.getMessage());
        }
      }
    }
  }
  Event actions[] = { ActionFactory.GetExitAction(),
                       ActionFactory.GetNewMailAction(),
                       ActionFactory.GetComposeMessageAction()};
}
