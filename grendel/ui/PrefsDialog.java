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
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.ui;

import java.beans.SimpleBeanInfo;
import java.beans.PropertyDescriptor;
import java.beans.PropertyEditor;

import java.awt.Container;
import java.awt.BorderLayout;
import java.awt.FlowLayout;

import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;

import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JOptionPane;
import javax.swing.JDialog;
import javax.swing.JTabbedPane;
import javax.swing.JPanel;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

//import netscape.orion.propeditor.PropertyEditorDlg;

import grendel.prefs.Prefs;

public class PrefsDialog {
  // XXX do we really need these here? they sort of break abstraction
  static String sPrefNames[] = {"mail.email_address",
                                "mail.host",
                                "mail.user",
                                "mail.directory",
                                "smtp.host"};

  public static void CheckPrefs(JFrame aParent) {
    if (!ValidPrefs()) {
      EditPrefs(aParent);
    }
  }

  public static void EditPrefs(JFrame aParent) {
    Object objs[] = {new Prefs()};
    //    PropertyEditorDlg.Edit(aParent, objs, false, true, "",
    //                     PropertyEditorDlg.UI_TREE);
    // JOptionPane.showMessageDialog(aParent, "This part of the UI is\nstill being worked on.", "Under Construction", JOptionPane.INFORMATION_MESSAGE);
    propertyEditorDialog(aParent, new Prefs(), false);
  }

  public static boolean ValidPrefs() {
    Preferences prefs = PreferencesFactory.Get();

    boolean res = true;
    /*
    for (int i = 0; i < sPrefNames.length; i++) {
      res &= !prefs.getString(sPrefNames[i], "").equals("");
    }
    */
    return res;
  }

  private static void propertyEditorDialog(JFrame parent, Object obj, 
                                           boolean modal) {
    Class reference = obj.getClass();
    String name = reference.getName();
    Class beanie = null;
    SimpleBeanInfo info;
    Object o;
    JDialog dialog = new JDialog(parent, modal);
    JTabbedPane pane = new JTabbedPane();
    Container content = dialog.getContentPane();
    String[] labels = {"Okay", "Cancel"};

    content.setLayout(new BorderLayout());
    content.add(pane, BorderLayout.CENTER);
    content.add(buttonPanel(labels,
                            new DialogListener(dialog)), 
                BorderLayout.SOUTH);

    try {
      // get the beaninfo
      beanie = Class.forName(name + "BeanInfo");
      o = beanie.newInstance();
      if (!(o instanceof SimpleBeanInfo)) return; // bummer

      // process the descriptors
      PropertyDescriptor[] desc;
      info = (SimpleBeanInfo)o;
      desc = info.getPropertyDescriptors();

      for (int i = 0; i < desc.length; i++) {
        o = desc[i].getPropertyEditorClass().newInstance();
        if (!(o instanceof PropertyEditor)) continue;
        PropertyEditor editor = (PropertyEditor)o;
        pane.add(desc[i].getDisplayName(), editor.getCustomEditor());
      }
      dialog.pack();
      dialog.setVisible(true);
    } catch (Exception e) {
      System.out.println("welp, that sucked. beans is broke. And stuff");
    }
  }

  public static JPanel buttonPanel(String[] labels, 
                                   ActionListener listener) {
    JPanel panel = new JPanel();
    panel.setLayout(new FlowLayout());
    
    if (labels != null) {
      for (int i = 0; i < labels.length; i++) {
        JButton button = new JButton(labels[i]);
        if (listener != null) button.addActionListener(listener);
        panel.add(button);
      }
    }

    return panel;
  }
}

class DialogListener 
  implements ActionListener {
  JDialog dialog;
  
  public DialogListener(JDialog dialog) {
    this.dialog = dialog;
  }
  
  public void actionPerformed(ActionEvent event) {
    dialog.setVisible(false);
  }
}
