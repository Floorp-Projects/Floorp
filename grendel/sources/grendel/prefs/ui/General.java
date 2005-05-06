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
 * The Initial Developer of the Original Code is Edwin Woudt
 * <edwin@woudt.nl>.  Portions created by Edwin Woudt are
 * Copyright (C) 1999 Edwin Woudt. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

package grendel.prefs.ui;

import java.util.Vector;

import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.Insets;
import java.awt.Rectangle;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JTextField;
import javax.swing.JButton;

import javax.swing.ButtonGroup;
import javax.swing.ImageIcon;

import grendel.prefs.base.GeneralPrefs;

import grendel.ui.UnifiedMessageDisplayManager;


public class General extends JFrame {

  GeneralPrefs prefs = GeneralPrefs.GetMaster();

  JTextField tfSMTP;

  public static void main(String argv[]) {

    General ui = new General();
    ui.setVisible(true);

  }

  public General() {

    super();

    setSize(500,354);
    setDefaultCloseOperation(DISPOSE_ON_CLOSE);
    getContentPane().setLayout(null);

    JLabel label = new JLabel("SMTP Server");
    label.setBounds(12,12,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);

    tfSMTP = new JTextField("");
    tfSMTP.setBounds(100,12,300,tfSMTP.getPreferredSize().height);
    getContentPane().add(tfSMTP);

    JButton button = new JButton("Cancel");
    button.setBounds(334,290,68,button.getPreferredSize().height);
    button.addActionListener(new CancelActionListener());
    button.setMargin(new Insets(0,0,0,0));
    getContentPane().add(button);
    button = new JButton("Finish");
    button.setBounds(414,290,68,button.getPreferredSize().height);
    button.addActionListener(new FinishActionListener());
    button.setMargin(new Insets(0,0,0,0));
    getContentPane().add(button);

    getData();
  }

  void getData() {
    tfSMTP.setText(prefs.getSMTPServer());
  }

  void setData() {
    prefs.setSMTPServer(tfSMTP.getText());
  }

  class FinishActionListener implements ActionListener {

    public void actionPerformed(ActionEvent e) {

      setData();
      prefs.writePrefs();
      setVisible(false);
      dispose();

    }

  }

  class CancelActionListener implements ActionListener {

    public void actionPerformed(ActionEvent e) {

      setVisible(false);
      dispose();

    }

  }

}
