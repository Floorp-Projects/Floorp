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
 * Created: Will Scullin <scullin@netscape.com>, 13 Oct 1997.
 */

package grendel.ui;

import java.awt.BorderLayout;
import java.awt.Image;
import java.awt.datatransfer.Clipboard;
import java.util.Hashtable;
import java.util.Locale;
import java.util.MissingResourceException;
import java.util.ResourceBundle;
import java.util.StringTokenizer;

import com.sun.java.swing.Icon;
import com.sun.java.swing.ImageIcon;
import com.sun.java.swing.JPanel;

import netscape.orion.toolbars.NSButton;
import netscape.orion.toolbars.NSToolbar;
import netscape.orion.uimanager.IUICmd;

public class GeneralPanel extends JPanel {
  static ResourceBundle fLabels = ResourceBundle.getBundle("grendel.ui.Labels",
                                                          Locale.getDefault());

  static Clipboard fPrivateClipboard = new Clipboard("Grendel");

  protected String         fResourceBase = "grendel.ui";
  protected NSToolbar fToolBar;

  public GeneralPanel() {
    setLayout(new BorderLayout());
  }

  public IUICmd[] getActions() {
    return null;
  }

  protected NSButton makeToolbarButton(String aAction) {
    Icon icon = new ImageIcon(getClass().getResource("images/toolbar/" + aAction + ".gif"));
    Icon iconDisabled = new ImageIcon(getClass().getResource("images/toolbar/" + aAction + "-disabled.gif"));
    Icon iconPressed = new ImageIcon(getClass().getResource("images/toolbar/" + aAction + "-pressed.gif"));
    Icon iconRollover = new ImageIcon(getClass().getResource("images/toolbar/"  + aAction + "-rollover.gif"));

    NSButton res = new NSButton();

    res.setIcon(icon);
    res.setDisabledIcon(iconDisabled);
    res.setPressedIcon(iconPressed);
    res.setRolloverIcon(iconRollover);
    res.setActionCommand(aAction);

    return res;
  }

  protected NSToolbar buildToolBar(String aToolbar, IUICmd aActions[]) {
    NSToolbar res = null;

    Hashtable commands = new Hashtable();
    for (int i = 0; i < aActions.length; i++)
    {
      IUICmd a = aActions[i];
      commands.put(a.getText(IUICmd.NAME), a);
    }

    try {
      res = new NSToolbar();

      ResourceBundle resources = ResourceBundle.getBundle(fResourceBase + ".Menus");
      String toolbar = resources.getString(aToolbar);
      StringTokenizer tokens = new StringTokenizer(toolbar, " ", false);
      while (tokens.hasMoreTokens()) {
        String token = tokens.nextToken();
        NSButton button = makeToolbarButton(token);
        IUICmd action = (IUICmd) commands.get(token);

        if (action != null) {
          button.addActionListener(action);
        } else {
          button.setEnabled(false);
        }
        res.addItem(button);
      }
    } catch (MissingResourceException e) {
      System.err.println(e);
    }

    return res;
  }

  public NSToolbar getToolBar() {
    return fToolBar;
  }
}
