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
 * Created: Will Scullin <scullin@netscape.com>, 21 Oct 1997.
 */

package grendel.integrator;

import java.awt.Component;
import java.awt.Image;
import java.awt.Rectangle;
import java.awt.Toolkit;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.MissingResourceException;
import java.util.ResourceBundle;
import java.util.StringTokenizer;
import java.util.Vector;
import java.util.Properties;

import javax.swing.Action;
import javax.swing.JMenu;
import javax.swing.JMenuItem;
import javax.swing.event.ChangeEvent;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

//import netscape.orion.toolbars.NSToolbar;

//import netscape.shell.IShellAnimation;
//import netscape.shell.IShellIntegrator;
//import netscape.shell.IShellView;
//import netscape.shell.IShellViewCtx;

import grendel.composition.AddressBar;
import grendel.composition.CompositionPanel;
import grendel.composition.CompositionPanelListener;
import grendel.ui.ActionFactory;

import javax.mail.Session;

class CompositionView implements IShellView {
  IShellIntegrator    fIntegrator;
  IShellAnimation     fAnimation;

  NSToolbar           fToolBar;
  AddressBar          fAddressBar;
  CompositionPanel    fPanel;

  /**
   * Instructs the view to create the ui component for this view.
   *
   * @param prevView the view that was active prior to this view
   * @param integrator the instance of the Integrator containing this view
   * @param rect a rectangle describing the size and position of the view
   *
   * @return the component ui object for displaying this view's data
   */
  public Component createViewComponent( IShellView prevView,
                                        IShellIntegrator communicator,
                                        Rectangle rect ) {

    fIntegrator = communicator;
    fAnimation =
      (IShellAnimation) fIntegrator.getService(IShellAnimation.class, this);


    Preferences prefs = PreferencesFactory.Get();
    String host = prefs.getString("smtp.host", "mail");
    String addr = prefs.getString("user.email_address", "john@doe.com");

    Properties props = new Properties();
    props.put("mail.smtp.user", host);
    props.put("mail.smtp.host", addr);

    // create some properties and get the default Session
    Session session = Session.getDefaultInstance(props, null);
    fPanel = new CompositionPanel(session);
    fPanel.addCompositionPanelListener(new PanelListener());

    fToolBar = fPanel.getToolBar();
    fAddressBar = fPanel.getAddressBar();

    return fPanel;
  }

  /**
   * Instructs the view to dispose the ui component for this view.
   */
  public void disposeViewComponent() {
    fPanel.dispose();
    fPanel = null;
  }

  /**
   * Returns the Component object for this view.
   */
  public Component getViewComponent() {
    return fPanel;
  }

  /**
   * Called by the Integrator when the view is created or is about to be
   * disposed.  View's should use this opportunity to merge menus etc.
   *
   * @param bActivate specifies the requested state.
   */
  public void activateUI( boolean bActivate ) {
    if (bActivate) {
      fIntegrator.addBar(fToolBar.getComponent(), "", false);
      fIntegrator.addBar(fAddressBar.getComponent(), "", false);
    }
  }

  /**
   * Refreshes the contents of this view.
   */
  public void refresh() {

  }

  class PanelListener implements CompositionPanelListener {
    public void sendingMail(ChangeEvent aEvent) {
      if (fAnimation != null) {
        fAnimation.start();
      }
    }
    public void doneSendingMail(ChangeEvent aEvent) {
      if (fAnimation != null) {
        fAnimation.stop();
      }
      fIntegrator.closeShell();
    }
    public void sendFailed(ChangeEvent aEvent) {
      if (fAnimation != null) {
        fAnimation.stop();
      }
    }
  }
}


