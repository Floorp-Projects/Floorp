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
 * The Initial Developer of the Original Code is Mauro Botelho.
 * Portions created by Mauro Botelho are
 * Copyright (C) 1997 Mauro Botelho. All
 * Rights Reserved.
 *
 * Contributor(s): Giao Nguyen <grail@cafebabe.org>
 */

package grendel.addressbook;

import java.awt.GridLayout;
import java.awt.FlowLayout;
import java.awt.BorderLayout;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;


import javax.swing.JComponent;
import javax.swing.JButton;
import javax.swing.JTextField;
import javax.swing.JCheckBox;
import javax.swing.JTabbedPane;
import javax.swing.JPanel;
import javax.swing.JLabel;
import javax.swing.JDialog;
import javax.swing.JFrame;

public class SearchDirectoryDialog extends JDialog {

  JTextField eDescription;
  JTextField eLDAPServer;
  JTextField eSearchRoot;
  JTextField ePortNumber;
  JTextField eResultsToShow;
  JCheckBox cSecure;
  JCheckBox cLoginPassword;
  JCheckBox cSavePassword;

  public SearchDirectoryDialog(JFrame aParent) {
    //FIX: Resource
    super(aParent, "Directory Server Property", true);

    setResizable(true);
    setSize (300, 515);

    addWindowListener(new AppCloser());

    JTabbedPane tabbedPane = new JTabbedPane();

    getContentPane().add(tabbedPane, BorderLayout.CENTER);

    JComponent generalPanel = createGeneralPanel ();
    tabbedPane.addTab("General",null,generalPanel,"General Properties");
/*
    JComponent offLinePanel = createOfflinePanel ();
    tabbedPane.addTab("Offline Settings",null,offLinePanel,"Offline Settings");
*/

    JButton bOK = new JButton("OK");
    bOK.addActionListener(new OKEvents());

    JButton bCancel = new JButton("Cancel");
    bCancel.addActionListener(new CancelEvents());

    JPanel pOKCancel = new JPanel();
    pOKCancel.add(bOK);
    pOKCancel.add(bCancel);

    getContentPane().add(pOKCancel, BorderLayout.SOUTH);

    getRootPane().setDefaultButton(bOK);
  }

  protected final class AppCloser extends WindowAdapter {
    public void windowClosing(WindowEvent e) {
        dispose();
    }
  }

  protected final class OKEvents implements ActionListener{
      public void actionPerformed(ActionEvent e) {
         dispose();
      }
  }

  protected final class CancelEvents implements ActionListener{
      public void actionPerformed(ActionEvent e) {
         dispose();
      }
  }

  private JPanel createGeneralPanel () {
      //the outer most panel has groove etched into it.
      JPanel pane = new JPanel(false);
      pane.setLayout (new FlowLayout(FlowLayout.LEFT));

      JPanel generalPane = new JPanel(false);
      generalPane.setLayout (new GridLayout (3,2));

      eDescription        = makeField ("Description:", 20, generalPane);
      eLDAPServer = makeField ("LDAP Server:", 20, generalPane);
      eSearchRoot   = makeField ("Search Root:", 20, generalPane);

      pane.add (generalPane);

      JPanel configurationPane = new JPanel(false);
      configurationPane.setLayout (new GridLayout(2,1));

      JPanel portNumberPane = new JPanel(false);
      portNumberPane.setLayout (new GridLayout(1,2));
      ePortNumber      = makeField ("Port Number:", 20, portNumberPane);
      configurationPane.add(portNumberPane);

      JPanel resultsToShowPane = new JPanel(false);
      resultsToShowPane.setLayout (new FlowLayout(FlowLayout.LEFT));
      eResultsToShow   = makeField ("Don't show more than", 3, resultsToShowPane);
      JLabel aLabel = new JLabel ("results");
      resultsToShowPane.add (aLabel);

      configurationPane.add(resultsToShowPane);

      pane.add (configurationPane);

      return pane;
  }

  private JTextField makeField (String aTitle, int aCol, JPanel aPanel) {
      JLabel title = new JLabel (aTitle);
      aPanel.add (title);

      JTextField textField = new JTextField (aCol);
      aPanel.add (textField);

      return textField;
  }
}

