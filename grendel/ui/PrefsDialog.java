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
 * Contributor(s): Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
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

import grendel.prefs.Prefs;

/**
 * This class handles preference dialog box handling for Grendel. Why
 * was all this stuff static?
 */
public class PrefsDialog 
  extends JDialog {
  public static String OK = "Okay";
  public static String CANCEL = "Cancel";
  JTabbedPane pane;
  PropertyEditor[] editors;

  public static void CheckPrefs(JFrame aParent) {
    if (!ValidPrefs()) {
      new PrefsDialog(aParent);
    }
  }

  public PrefsDialog(JFrame aParent) {
    Object objs[] = {new Prefs()};
    propertyEditorDialog(aParent, new Prefs());
  }

  public static boolean ValidPrefs() {
    Preferences prefs = PreferencesFactory.Get();

    boolean res = true;

    return res;
  }

  private void propertyEditorDialog(JFrame parent, Object obj) {
    Class reference = obj.getClass();
    String name = reference.getName();
    Container content = getContentPane();
    String[] labels = {OK, CANCEL};
    pane = new JTabbedPane();

    // gui handling
    content.setLayout(new BorderLayout());
    content.add(pane, BorderLayout.CENTER);
    content.add(buttonPanel(labels,
                            new PrefsDialogListener()), 
                BorderLayout.SOUTH);

    try {
      // get the beaninfo
      Object o;
      SimpleBeanInfo info;
      Class beanie = Class.forName(name + "BeanInfo");
      PropertyDescriptor[] desc;
      o = beanie.newInstance();
      if (!(o instanceof SimpleBeanInfo)) return; // bummer

      // process the descriptors
      info = (SimpleBeanInfo)o;
      desc = info.getPropertyDescriptors();
      editors = new PropertyEditor[desc.length];

      for (int i = 0; i < desc.length; i++) {
        o = desc[i].getPropertyEditorClass().newInstance();
        if (!(o instanceof PropertyEditor)) continue;
        editors[i] = (PropertyEditor)o;
        pane.add(desc[i].getDisplayName(), editors[i].getCustomEditor());
      }
      pack();
      setVisible(true);
    } catch (Exception e) {
      System.out.println("welp, that sucked. beans is broke. And stuff");
    }
  }

  private JPanel buttonPanel(String[] labels, 
                            ActionListener listener) {
    JPanel panel = new JPanel();
    panel.setLayout(new FlowLayout());
    
    if (labels != null) {
      for (int i = 0; i < labels.length; i++) {
        JButton button = new JButton(labels[i]);
        button.setActionCommand(labels[i]);
        if (listener != null) button.addActionListener(listener);
        panel.add(button);
      }
    }

    return panel;
  }

  class PrefsDialogListener 
    implements ActionListener {
    JDialog dialog;
    
    public PrefsDialogListener() {
      dialog = PrefsDialog.this;
    }
    
    public void actionPerformed(ActionEvent event) {
      String command = event.getActionCommand();
      Prefs prefs = new Prefs();
      if (command.equals(PrefsDialog.OK)) {
        for (int i = 0; i < editors.length; i++) {
          prefs.set(editors[i].getValue());
        }
      }
      dialog.setVisible(false);
    }
  }
}
