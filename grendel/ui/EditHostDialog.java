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
 * Created: Will Scullin <scullin@netscape.com>, 26 Nov 1997.
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.ui;

import java.awt.Dimension;
import java.awt.Frame;
import java.awt.event.ActionEvent;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.net.URL;
import java.text.MessageFormat;
import java.util.Hashtable;
import java.util.ResourceBundle;

import javax.mail.URLName;

import javax.swing.JOptionPane;

//import netscape.orion.dialogs.AttrNotFoundException;
//import netscape.orion.dialogs.PageModel;
//import netscape.orion.dialogs.PageUI;

//import xml.tree.XMLNode;
//import xml.tree.TreeBuilder;

public class EditHostDialog extends GeneralDialog {
  static final String kIMAPRadioKey="imapRadio";
  static final String kPOPRadioKey="popRadio";
  static final String kOtherRadioKey="otherRadio";
  static final String kOtherFieldKey="otherField";
  static final String kHostFieldKey="hostField";
  static final String kUserFieldKey="userField";

  URLName fResult = null;

  Hashtable fValues = null;

  PageUI fPanel;

  class EditHostModel extends PageModel {
    public EditHostModel(URLName aURL) {
      String proto = "imap";
      String host = "mail";
      String user = "";

      if (aURL != null) {
        proto = aURL.getProtocol();
        host = aURL.getHost();
        user = aURL.getUsername();
      }

      boolean other = (!proto.equalsIgnoreCase("imap") &&
                       !proto.equalsIgnoreCase("pop3"));

      fValues = new Hashtable();
      fValues.put(kIMAPRadioKey, proto.equalsIgnoreCase("imap") ?
                  Boolean.TRUE : Boolean.FALSE);
      fValues.put(kPOPRadioKey, proto.equalsIgnoreCase("pop3") ?
                  Boolean.TRUE : Boolean.FALSE);
      fValues.put(kOtherRadioKey, other ? Boolean.TRUE : Boolean.FALSE);
      fValues.put(kOtherFieldKey, other ? proto : "");
      fValues.put(kHostFieldKey, host == null ? "" : host);
      fValues.put(kUserFieldKey, user == null ? "" : user);
    }

    public Object getAttribute(String aAttrib) throws AttrNotFoundException {
      Object res = fValues.get(aAttrib);
      if (res == null) {
        res = fLabels.getString(aAttrib);
      }
      if (res == null) {
        throw new AttrNotFoundException(aAttrib);
      }
      return res;
    }

    public void setAttribute(String aAttrib, Object aValue) {
      if (fValues.containsKey(aAttrib)) {
        fValues.put(aAttrib, aValue);
      }
    }

    public void actionPerformed(ActionEvent aEvent) {
    }
  }

  public EditHostDialog(Frame aParent, URLName aURL) {
    super(aParent);

    setModal(true);
    EditHostModel model = new EditHostModel(aURL);

    // use the XML parser to get the root XML node of the resource tree
    XMLNode root = null;
    URL url = getClass().getResource("dialogs.xml");
    try {
      root = xml.tree.TreeBuilder.build(url, getClass());
    } catch (Exception e) {
      e.printStackTrace();
    }

    XMLNode editHost = root.getChild("dialog", "id", "editHost");

    fPanel = new PageUI(url, editHost, model);

    JOptionPane actionPanel = new JOptionPane(fPanel,
                                              JOptionPane.PLAIN_MESSAGE,
                                              JOptionPane.OK_CANCEL_OPTION);
    actionPanel.addPropertyChangeListener(new OptionListener());
    add(actionPanel);

    // XXX This is a stupid hack because PageUI doesn't to a resource lookup
    // on it's title. Bleh.
    String title = fPanel.getTitle();
    if (title.charAt(0) == '$') {
      try {
        title = (String) model.getAttribute(title.substring(1));
      } catch (AttrNotFoundException e) {}
    }
    setTitle(title);

    Dimension size = getPreferredSize();
    Dimension screenSize = getToolkit().getScreenSize();
    setBounds((screenSize.width - size.width - 10) / 2,
              (screenSize.height - size.height - 32) / 2,
              size.width + 10, size.height + 32);
    pack();
    setVisible(true);
    requestFocus();
  }

  public URLName getURLName() {
    return fResult;
  }

  class OptionListener implements PropertyChangeListener {
    public void propertyChange(PropertyChangeEvent aEvent) {
      if (aEvent.getPropertyName().equals(JOptionPane.VALUE_PROPERTY)){
        int value = ((Integer) aEvent.getNewValue()).intValue();

        if (value == JOptionPane.OK_OPTION) {
          // Grab all the values
          fPanel.saveAll();

          String proto;
          Boolean imap = (Boolean) fValues.get(kIMAPRadioKey);
          Boolean pop3 = (Boolean) fValues.get(kPOPRadioKey);
          if (imap.booleanValue()) {
            proto = "imap";
          } else if (pop3.booleanValue()) {
            proto = "pop3";
          } else {
            proto = (String) fValues.get(kOtherFieldKey);
          }
          String host = (String) fValues.get(kHostFieldKey);
          String user = (String) fValues.get(kUserFieldKey);

          if (user.equals("")) {
            user = null;
          }

          if (proto.length() > 0 &&
              host.length() > 0) {
            fResult = new URLName(proto, host, -1, null, user, null);
            setVisible(false);
          } else {
            getToolkit().beep();
          }
        } else {
          setVisible(false);
        }
      }
    }
  }
}
