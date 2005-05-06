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

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.JButton;
import javax.swing.JEditorPane;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;

import javax.swing.AbstractListModel;
import javax.swing.ListSelectionModel;

import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;

import grendel.prefs.base.IdentityArray;
import grendel.prefs.base.IdentityStructure;


public class Identities extends JFrame {

  JList list;
  JTextField tfDesc;
  JTextField tfName;
  JTextField tfEMail;
  JTextField tfReply;
  JTextField tfOrg;
  JTextArea taSig;

  IdentityArray ida;
  IdentityListModel ilm;
  int currentSelection = -1;

  public static void main(String argv[]) {

    Identities ident = new Identities();
    ident.setVisible(true);

  }

  public Identities() {

    super();

    ida = IdentityArray.GetMaster();

    setSize(500,354);
    getContentPane().setLayout(null);
    setDefaultCloseOperation(DISPOSE_ON_CLOSE);

    ilm = new IdentityListModel();
    list = new JList(ilm);
    list.setSelectedIndex(0);
    list.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
    list.setBounds(12,12,142,306);
    list.addListSelectionListener(new SelectionChangedListener());
    getContentPane().add(list);

    JLabel label = new JLabel("Description");
    label.setBounds(174,12,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("Name");
    label.setBounds(174,44,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("E-Mail");
    label.setBounds(174,76,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("Reply-To");
    label.setBounds(174,108,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("Organization");
    label.setBounds(174,140,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("Signature");
    label.setBounds(174,172,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);

    tfDesc = new JTextField();
    tfDesc.setBounds(254,12,228,tfDesc.getPreferredSize().height);
    getContentPane().add(tfDesc);

    tfName = new JTextField();
    tfName.setBounds(254,44,228,tfName.getPreferredSize().height);
    getContentPane().add(tfName);

    tfEMail = new JTextField();
    tfEMail.setBounds(254,76,228,tfEMail.getPreferredSize().height);
    getContentPane().add(tfEMail);

    tfReply = new JTextField();
    tfReply.setBounds(254,108,228,tfReply.getPreferredSize().height);
    getContentPane().add(tfReply);

    tfOrg = new JTextField();
    tfOrg.setBounds(254,140,228,tfOrg.getPreferredSize().height);
    getContentPane().add(tfOrg);

    taSig = new JTextArea();
    taSig.setFont(new Font("monospaced",Font.PLAIN,12));
    JScrollPane scroll = new JScrollPane(taSig);
    scroll.setBounds(174,190,308,88);
    getContentPane().add(scroll);

    JButton button = new JButton("Add New");
    button.setBounds(174,290,94,button.getPreferredSize().height);
    button.addActionListener(new AddNewActionListener());
    getContentPane().add(button);
    button = new JButton("Delete");
    button.setBounds(281,290,94,button.getPreferredSize().height);
    button.addActionListener(new DeleteActionListener());
    getContentPane().add(button);
    button = new JButton("Finish");
    button.setBounds(388,290,94,button.getPreferredSize().height);
    button.addActionListener(new FinishActionListener());
    getContentPane().add(button);

    update();

  }

  private void update() {

    if (currentSelection > -1) {
      ida.get(currentSelection).setDescription(tfDesc.getText());
      ida.get(currentSelection).setName(tfName.getText());
      ida.get(currentSelection).setEMail(tfEMail.getText());
      ida.get(currentSelection).setReplyTo(tfReply.getText());
      ida.get(currentSelection).setOrganization(tfOrg.getText());
      ida.get(currentSelection).setSignature(taSig.getText());
    }

    tfDesc.setText(ida.get(list.getSelectedIndex()).getDescription());
    tfName.setText(ida.get(list.getSelectedIndex()).getName());
    tfEMail.setText(ida.get(list.getSelectedIndex()).getEMail());
    tfReply.setText(ida.get(list.getSelectedIndex()).getReplyTo());
    tfOrg.setText(ida.get(list.getSelectedIndex()).getOrganization());
    taSig.setText(ida.get(list.getSelectedIndex()).getSignature());

    currentSelection = list.getSelectedIndex();

  }


  class SelectionChangedListener implements ListSelectionListener {

    public void valueChanged(ListSelectionEvent e) {

      if (e.getValueIsAdjusting()) {

        update();

      }

    }

  }

  class AddNewActionListener implements ActionListener {

    public void actionPerformed(ActionEvent e) {

      ida.add(new IdentityStructure("New Identity"));
      ilm.fireAdded(ida.size()-1);
      list.setSelectedIndex(ida.size()-1);
      update();

    }

  }

  class DeleteActionListener implements ActionListener {

    public void actionPerformed(ActionEvent e) {

    	int index = list.getSelectedIndex();
      ida.remove(index);
      ilm.fireRemoved(index);
      currentSelection = -1;
      if (index >= ida.size()) {
        index = ida.size()-1;
      }
      if (ida.size() <= 0) {
        ida.add(new IdentityStructure("New Identity"));
        index = 0;
      }
      list.setSelectedIndex(index);
      update();

    }

  }

  class FinishActionListener implements ActionListener {

    public void actionPerformed(ActionEvent e) {

      update();
      ida.writePrefs();
      setVisible(false);
      dispose();

    }

  }

  class IdentityListModel extends AbstractListModel {

    public Object getElementAt(int index) {
      return ida.get(index).getDescription();
    }

    public int getSize() {
      return ida.size();
    }

    public void fireAdded(int index) {
      fireIntervalAdded(this, index, index);
    }

    public void fireRemoved(int index) {
      fireIntervalRemoved(this, index, index);
    }

  }

}