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
 */

package grendel.integrator;

import java.awt.Component;
import java.awt.Image;
import java.awt.Rectangle;
import java.awt.Toolkit;
import java.io.IOException;
import java.net.URL;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.MissingResourceException;
import java.util.ResourceBundle;
import java.util.StringTokenizer;
import java.util.Vector;

import javax.mail.Folder;

import javax.swing.Action;
import javax.swing.JMenu;
import javax.swing.JMenuItem;
import javax.swing.event.ChangeEvent;

//import netscape.orion.toolbars.NSToolbar;
//import netscape.orion.uimanager.UIMConstants;
//import netscape.orion.uimanager.IUICmd;

//import netscape.shell.IShellAnimation;
//import netscape.shell.IShellIntegrator;
//import netscape.shell.IShellView;
//import netscape.shell.IShellViewCtx;

//import netscape.orion.uimanager.AbstractUICmd;
//import netscape.orion.uimanager.ConfigFormatException;
//import netscape.orion.uimanager.IUICmd;
//import netscape.orion.uimanager.IUIMMenuBar;
//import netscape.orion.uimanager.UIMConstants;

//import xml.tree.TreeBuilder;
//import xml.tree.XMLNode;

import grendel.ui.ActionFactory;
import grendel.ui.FolderPanel;
import grendel.ui.FolderPanelListener;
import grendel.ui.MessagePanel;
import grendel.ui.MessagePanelListener;
import grendel.view.ViewedMessage;
import grendel.widgets.Splitter;
import grendel.widgets.StatusEvent;
import grendel.widgets.TreePath;

class FolderView implements IShellView {
  IShellIntegrator    fIntegrator;
  IShellAnimation     fAnimation;

  Splitter            fSplitter;
  MessagePanel        fMessagePanel;
  FolderPanel         fFolderPanel;

  PanelListener       fListener;

  Folder              fFolder;

  NSToolbar           fToolBar;

  public FolderView(Folder aFolder) {
    fFolder = aFolder;
  }

  /**
   * Instructs the view to create the ui component for this view.
   *
   * @param prevView the view that was active prior to this view
   * @param integrator the instance of the Integrator containing this view
   * @param rect a rectangle describing the size and position of the view
   *
   * @return the component ui object for displaying this view's data
   */
  public Component createViewComponent( IShellView aPrevView,
                                        IShellIntegrator aIntegrator,
                                        Rectangle aRect ) {
    fIntegrator = aIntegrator;
    fAnimation =
      (IShellAnimation) fIntegrator.getService(IShellAnimation.class, this);

    fSplitter = new Splitter(Splitter.VERTICAL);
    fFolderPanel = new FolderPanel();
    fMessagePanel = new MessagePanel();

    NSToolbar folderToolBar = fFolderPanel.getToolBar();
    NSToolbar messageToolBar = fMessagePanel.getToolBar();

    fToolBar = grendel.ui.Util.MergeToolBars(folderToolBar,
                                             messageToolBar);

    fListener = new PanelListener();
    fFolderPanel.addFolderPanelListener(fListener);
    fMessagePanel.addMessagePanelListener(fListener);

    fSplitter.add(fFolderPanel, new Float(1.0));
    fSplitter.add(fSplitter.createSeparator(4));
    fSplitter.add(fMessagePanel, new Float(1.0));

    fFolderPanel.setFolder(fFolder);

    return fSplitter;
  }

  /**
   * Instructs the view to dispose the ui component for this view.
   */
  public void disposeViewComponent() {
    fFolderPanel.removeFolderPanelListener(fListener);
    fFolderPanel.dispose();
    fMessagePanel.dispose();
  }

  /**
   * Returns the Component object for this view.
   */
  public Component getViewComponent() {
    return fSplitter;
  }

  /**
   * Called by the Integrator when the view is created or is about to be
   * disposed.  View's should use this opportunity to merge menus etc.
   *
   * @param bActivate specifies the requested state.
   */
  public void activateUI( boolean bActivate ) {
    IUICmd actions[] = {
      ActionFactory.GetNewMailAction(),
      ActionFactory.GetComposeMessageAction(),
    };

    if (bActivate) {
      try {
        URL url = getClass().getResource("menus.xml");
        XMLNode root = TreeBuilder.build(url, getClass());
        XMLNode node = root.getChild(UIMConstants.kMenubarType,
                                     UIMConstants.kIDAttribute,
                                     "FolderView");

        fIntegrator.mergeMenus(node, actions, this);
      } catch (ConfigFormatException e) {
        e.printStackTrace();
      } catch (IOException e) {
        e.printStackTrace();
      }
      fIntegrator.addBar(fToolBar.getComponent(), "", false);
    }
  }

  /**
   * Refreshes the contents of this view.
   */
  public void refresh() {
  }

  //
  // MessageSelectionListener class
  //

  class PanelListener implements FolderPanelListener, MessagePanelListener {
    // FolderPanelListener

    public void loadingFolder(ChangeEvent aEvent) {
      // start animation
      if (fAnimation != null) {
        fAnimation.start();
      }
    }

    public void folderLoaded(ChangeEvent aEvent) {
      // stop animation
      if (fAnimation != null) {
        fAnimation.stop();
      }
    }

    public void folderStatus(StatusEvent aEvent) {
      fIntegrator.setStatusText(aEvent.getStatus());
    }

    public void folderSelectionChanged(ChangeEvent aEvent) {
      TreePath path = null;
      Enumeration selection = ((FolderPanel) aEvent.getSource()).getSelection();
      if (selection.hasMoreElements()) {
        path = (TreePath) selection.nextElement();
      }
      if (path != null && !selection.hasMoreElements()) { // not multiple selection
        ViewedMessage node = (ViewedMessage) path.getTip();
        fMessagePanel.setMessage(node.getMessage());
      } else {
        fMessagePanel.setMessage(null);
      }
    }
    public void folderSelectionDoubleClicked(ChangeEvent aEvent) {
    }

    // MessagePanelListener

    public void loadingMessage(ChangeEvent aEvent) {
      // start animation
      if (fAnimation != null) {
        fAnimation.start();
      }
    }

    public void messageLoaded(ChangeEvent aEvent) {
      // stop animation
      if (fAnimation != null) {
        fAnimation.stop();
      }
    }

    public void messageStatus(StatusEvent aEvent) {
      fIntegrator.setStatusText(aEvent.getStatus());
    }
  }
}
