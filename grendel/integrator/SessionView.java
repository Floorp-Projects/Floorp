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
 * Created: Will Scullin <scullin@netscape.com>, 10 Nov 1997.
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
//import netscape.orion.uimanager.AbstractUICmd;
//import netscape.orion.uimanager.ConfigFormatException;
//import netscape.orion.uimanager.IUICmd;
//import netscape.orion.uimanager.IUIMMenuBar;
//import netscape.orion.uimanager.UIMConstants;

//import xml.tree.TreeBuilder;
//import xml.tree.XMLNode;

//import netscape.shell.IShellAnimation;
//import netscape.shell.IShellIntegrator;
//import netscape.shell.IShellView;
//import netscape.shell.IShellViewCtx;

import grendel.ui.ActionFactory;
import grendel.ui.MasterPanel;
import grendel.ui.MasterPanelListener;
import grendel.widgets.StatusEvent;
import grendel.widgets.TreePath;

class SessionView implements IShellView {
  IShellIntegrator    fIntegrator;
  IShellAnimation     fAnimation;

  MasterPanel         fMasterPanel;
  NSToolbar           fToolBar;

  public SessionView() {
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

    fMasterPanel = new MasterPanel();

    fToolBar = fMasterPanel.getToolBar();

    return fMasterPanel;
  }

  /**
   * Instructs the view to dispose the ui component for this view.
   */
  public void disposeViewComponent() {
    fMasterPanel.dispose();
  }

  /**
   * Returns the Component object for this view.
   */
  public Component getViewComponent() {
    return fMasterPanel;
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
}
