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
 * Created: Will Scullin <scullin@netscape.com>, 24 Nov 1997.
 */

package grendel.ui;

import java.awt.Canvas;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Frame;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.Point;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.net.InetAddress;
import java.text.MessageFormat;
import java.util.ResourceBundle;

import javax.mail.Authenticator;
import javax.mail.PasswordAuthentication;

import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTextField;
import javax.swing.JPasswordField;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;


public class DialogAuthenticator extends Authenticator {

  boolean fCancelled = false;
  boolean fRemember = false;

  String fPrefBase;

  protected PasswordAuthentication getPasswordAuthentication() {
    fCancelled = false;
    Preferences prefs = PreferencesFactory.Get();

    InetAddress site = getRequestingSite();
    String protocol = getRequestingProtocol();

    fPrefBase = "mail." + protocol + "-" + site.getHostName();

    String user = null;
    user = prefs.getString(fPrefBase + ".user", null);

    String password = null;
    password = prefs.getString(fPrefBase + ".password", null);

    if (user == null) {
      user = getDefaultUserName();
    }

    if (password == null) {
      password = prefs.getString("mail.password", null);
    }

    if (user == null || password == null || password.equals("<reset>")) {
      String prompt = getRequestingPrompt();
      Frame parent = GeneralFrame.GetDefaultFrame();
      if (parent == null) {
        parent = JOptionPane.getRootFrame();
      }

      AuthenticatorDialog d =
        new AuthenticatorDialog(parent,
                                site.getHostName(), prompt, user);

      fCancelled = d.isCancelled();

      user = d.getUser();
      password = d.getPassword();
      if (d.rememberPassword()) {
        prefs.putString(fPrefBase + ".user", user);
        prefs.putString(fPrefBase + ".password", password);
      }
    }

    return new PasswordAuthentication(user, password);
  }

  public boolean isCancelled() {
    return fCancelled;
  }

  public void resetPassword() {
    Preferences prefs = PreferencesFactory.Get();
    prefs.putString(fPrefBase + ".password", "<reset>");
  }

  class AuthenticatorDialog extends GeneralDialog {
    JTextField fUserField;
    JPasswordField fPasswordField;
    JCheckBox fRememberCheckBox;

    Container fPanel;
    GridBagLayout fLayout;

    boolean fOK;
    boolean fRemember;

    public AuthenticatorDialog(Frame aParent, String aSite, String aPrompt,
                               String aUser) {
      super(aParent, "...");
      setFont(Font.decode("Dialog-12"));

      setTitle(MessageFormat.format(fLabels.getString("passwordDialogLabel"),
                                    new Object[] {aSite}));
      //      fPanel = getContentPane();
      fPanel = new JPanel();
      fLayout = new GridBagLayout();
      fPanel.setLayout(fLayout);
      setModal(true);

      fOK = false;
      fRemember = false;

      GridBagConstraints c = new GridBagConstraints();

      c.anchor = GridBagConstraints.NORTHWEST;
      c.weightx = 1;
      c.weighty = 0;
      c.gridwidth = GridBagConstraints.REMAINDER;
      c.fill = GridBagConstraints.HORIZONTAL;

      // Prompt label

      JLabel label;

      label = new JLabel(aPrompt);
      c.gridwidth = GridBagConstraints.REMAINDER;
      c.insets = new Insets(5,5,0,5);
      fPanel.add(label, c);

      // User field

      label = new JLabel(fLabels.getString("userPrompt"), JLabel.RIGHT);
      c.gridwidth = 1;
      c.insets = new Insets(5,5,0,5);
      fPanel.add(label, c);

      fUserField = new JTextField(aUser != null ? aUser : "", 20);
      c.gridwidth = GridBagConstraints.REMAINDER;
      c.insets = new Insets(5,0,0,5);
      fPanel.add(fUserField, c);

      // Password field

      label = new JLabel(fLabels.getString("passwordPrompt"), JLabel.RIGHT);
      c.gridwidth = 1;
      c.insets = new Insets(5,5,0,5);
      fPanel.add(label, c);

      fPasswordField = new JPasswordField(20);
      c.gridwidth = GridBagConstraints.REMAINDER;
      c.insets = new Insets(5,0,0,5);
      fPanel.add(fPasswordField, c);

      // Remember password checkbox

      fRememberCheckBox =
        new JCheckBox(fLabels.getString("rememberPasswordLabel"));
      fRememberCheckBox.setHorizontalAlignment(JCheckBox.LEFT);
      c.gridwidth = GridBagConstraints.REMAINDER;
      c.insets = new Insets(5,5,0,5);
      fPanel.add(fRememberCheckBox, c);

      // Action buttons
      JOptionPane actionPanel = new JOptionPane(fPanel,
                                                JOptionPane.PLAIN_MESSAGE,
                                                JOptionPane.OK_CANCEL_OPTION);
      actionPanel.addPropertyChangeListener(new OptionListener());
      add(actionPanel);

      Dimension size = getPreferredSize();
      Dimension screen = getToolkit().getScreenSize();
      // ### WHS tempory JDK 1.1.x hack
      setBounds((screen.width - size.width - 10) / 2,
                (screen.height - size.height - 32) / 2,
                size.width + 10,
                size.height + 32);

      setVisible(true);
      requestFocus();
    }

    public String getUser() {
      if (fOK) {
        return fUserField.getText();
      } else {
        return null;
      }
    }

    public String getPassword() {
      if (fOK) {
        return fPasswordField.getText();
      } else {
        return null;
      }
    }

    public boolean isCancelled() {
      return !fOK;
    }

    public boolean rememberPassword() {
      return fRemember;
    }

    class CancelAction implements ActionListener {
      public void actionPerformed(ActionEvent aEvent) {
        setVisible(false);
      }
    }

    class OKAction implements ActionListener {
      public void actionPerformed(ActionEvent aEvent) {
        fOK = true;
        fRemember = fRememberCheckBox.isSelected();
        setVisible(false);
      }
    }

    class OptionListener implements PropertyChangeListener {
      public void propertyChange(PropertyChangeEvent aEvent) {
        if (aEvent.getPropertyName().equals(JOptionPane.VALUE_PROPERTY)){
          int value = ((Integer) aEvent.getNewValue()).intValue();

          if (value == JOptionPane.OK_OPTION) {
            fOK = true;
            fRemember = fRememberCheckBox.isSelected();
          }
          setVisible(false);
        }
      }
    }
  }
}
