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
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import grendel.ui.UIAction;
import grendel.ui.ToolBarLayout;

import grendel.widgets.GrendelToolBar;

import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Image;
import java.awt.datatransfer.Clipboard;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Locale;
import java.util.MissingResourceException;
import java.util.ResourceBundle;
import java.util.StringTokenizer;

import javax.swing.BorderFactory;
import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JPanel;
import javax.swing.JButton;

public class GeneralPanel extends JPanel {
  private final boolean DEBUG = true;
  static ResourceBundle fLabels = ResourceBundle.getBundle("grendel.ui.Labels",
                                                         Locale.getDefault());

  static Clipboard fPrivateClipboard = new Clipboard("Grendel");

  protected String         fResourceBase = "grendel.ui";
  protected GrendelToolBar fToolBar;

  public GeneralPanel() {
    setLayout(new BorderLayout());
  }

  public UIAction[] getActions() {
    return null;
  }

  protected JButton makeToolbarButton(String aAction) {
    Icon icon = new ImageIcon(getClass().getResource("images/toolbar/" + aAction + ".gif"));
    Icon iconDisabled = new ImageIcon(getClass().getResource("images/toolbar/" + aAction + "-disabled.gif"));
    Icon iconPressed = new ImageIcon(getClass().getResource("images/toolbar/" + aAction + "-pressed.gif"));
    Icon iconRollover = new ImageIcon(getClass().getResource("images/toolbar/"  + aAction + "-rollover.gif"));

    JButton res = new JButton();

    res.setIcon(icon);
    res.setHorizontalTextPosition(JButton.CENTER);
    res.setVerticalTextPosition(JButton.BOTTOM);
    res.setDisabledIcon(iconDisabled);
    res.setPressedIcon(iconPressed);
    res.setRolloverIcon(iconRollover);
    res.setActionCommand(aAction);
    res.setRolloverEnabled(true);
    res.setBorder(BorderFactory.createEmptyBorder());
    Font f=res.getFont();
    Font nf=new Font(f.getName(), Font.PLAIN, f.getSize()-1);
    res.setFont(nf);
    ResourceBundle resources = ResourceBundle.getBundle(fResourceBase + ".Toolbar");
    String text = resources.getString(aAction);
    res.setText(text);
    
    Dimension d=res.getPreferredSize();
    System.out.println(d.getWidth());
    System.out.println(d.getHeight());
    double w=d.getWidth();
    if (w > 48) {
      d.setSize(w, 38);
    } else {
      d.setSize(48, 38);
    }
    res.setMinimumSize(d);
    res.setMaximumSize(d);
    res.setPreferredSize(d);

    return res;
  }

  protected GrendelToolBar buildToolBar(String aToolbar, UIAction[] aActions) {
    GrendelToolBar res = null;

    Hashtable commands = new Hashtable();
    for (int i = 0; i < aActions.length; i++)
        {
           UIAction a = aActions[i];
           String name = a.getName();
           commands.put(name, a);
        }
    

    try {
      res = new GrendelToolBar();
      //  res.setLayout(new ToolBarLayout());

      ResourceBundle resources = ResourceBundle.getBundle(fResourceBase + ".Menus");
      String toolbar = resources.getString(aToolbar);
      StringTokenizer tokens = new StringTokenizer(toolbar, " ", false);
      while (tokens.hasMoreTokens()) {
        String token = tokens.nextToken();
        if (DEBUG) {
          System.out.println("Local token = " + token);
        }
        JButton button = makeToolbarButton(token);
        UIAction action = (UIAction)commands.get(token);

        if (action != null) {
          button.addActionListener(action);
        } else {
          button.setEnabled(false);
        }
        res.add(button);
      }
    } catch (MissingResourceException e) {
      System.err.println(e);
    }

    if (DEBUG) {
      System.out.println("Toolbar status:");
      if (res == null) {
        System.out.println("\tbuildToolBar failed.");
      }
      else {
        System.out.println("\tbuildToolBar succeeded.");
        System.out.println("\tGrendelToolBar res contains " + res.getComponentCount() + " components.");
      }
    }

    return res;
  }

  public GrendelToolBar getToolBar() {
    return fToolBar;
  }
}
