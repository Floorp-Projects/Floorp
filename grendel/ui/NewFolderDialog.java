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
 * Created: Will Scullin <scullin@netscape.com>, 20 Nov 1997.
 * Modified: Giao Nguyen <grail@cafebabe.org>, 20, Jan 1999.
 */

package grendel.ui;

import java.awt.Dimension;
import java.awt.Frame;
import java.awt.GridBagLayout;
import java.awt.GridBagConstraints;
import java.awt.event.ActionEvent;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.net.URL;
import java.text.MessageFormat;
import java.util.Hashtable;
import java.util.NoSuchElementException;

import javax.mail.Folder;
import javax.mail.MessagingException;

import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTextField;

//import netscape.orion.dialogs.AttrNotFoundException;
//import netscape.orion.dialogs.PageModel;
//import netscape.orion.dialogs.PageUI;

//import xml.tree.XMLNode;
//import xml.tree.TreeBuilder;

import grendel.view.ViewedFolder;

public class NewFolderDialog extends GeneralDialog {
  FolderPanel     fPanel;

  JPanel newFolderDialogPanel;
  ViewedFolder fFolder;
  FolderCombo fParentCombo;

  Hashtable fValues;

  private static final String kFolderComboKey = "folderCombo";
  private static final String kNameKey = "nameField";

  class NewFolderModel {
    public NewFolderModel(FolderCombo aCombo) {
      fValues = new Hashtable();
      fValues.put(kNameKey, "");
      fValues.put(kFolderComboKey, aCombo);
    }

    public Object getAttribute(String aAttrib) throws NoSuchElementException {
      Object res = fValues.get(aAttrib);
      if (res == null) {
        res = fLabels.getString(aAttrib);
      }
      if (res == null) {
        throw new NoSuchElementException(aAttrib);
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


  public NewFolderDialog(Frame aParent, ViewedFolder aFolder) {
    super(aParent);

    fParentCombo = new FolderCombo();
    fParentCombo.populate(Folder.HOLDS_FOLDERS, 0);
    fParentCombo.setSelectedItem(fFolder);

    setModal(true);
    NewFolderModel model = new NewFolderModel(fParentCombo);

    // use the XML parser to get the root XML node of the resource tree
    // XMLNode root = null;
    // URL url = getClass().getResource("dialogs.xml");
    // try {
    //   root = xml.tree.TreeBuilder.build(url, getClass());
    // } catch (Exception e) {
    //    e.printStackTrace();
    // }

    //   XMLNode editHost = root.getChild("dialog", "id", "newFolderDialog");

    newFolderDialogPanel = new JPanel(new GridBagLayout(), true);

    JLabel parentPrompt = new JLabel("Parent:");
    // newFolderDialogPanel.add(parentPrompt, GridBagConstraints.WEST);
    newFolderDialogPanel.add(parentPrompt);
    newFolderDialogPanel.add(fParentCombo, GridBagConstraints.REMAINDER);
    JLabel namePrompt = new JLabel("Name:");
    // newFolderDialogPanel.add(namePrompt, GridBagConstraints.WEST);
    newFolderDialogPanel.add(namePrompt);
    JTextField nameField = new JTextField();
    newFolderDialogPanel.add(nameField, GridBagConstraints.REMAINDER);
    
    JOptionPane actionPanel = new JOptionPane(newFolderDialogPanel,
                                              JOptionPane.PLAIN_MESSAGE,
                                              JOptionPane.OK_CANCEL_OPTION);
    actionPanel.addPropertyChangeListener(new OptionListener());
    getContentPane().add(actionPanel);

    // XXX This is a stupid hack because PageUI doesn't to a resource lookup
    // on it's title. Bleh.
    String title = "folderLabel";
      try {
	title = (String) model.getAttribute(title);
      } catch (NoSuchElementException e) {}
 
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

  public NewFolderDialog(Frame aParent) {
    this(aParent, null);
  }

  boolean createFolder() {
    try {
      String newName = (String) fValues.get(kNameKey);
      ViewedFolder parent = fParentCombo.getSelectedFolder();
      Folder newFolder = parent.getFolder().getFolder(newName);

      if (newFolder.exists()) {
        Object args[] = {newName};
        String err =
          MessageFormat.format(fLabels.getString("folderExistsError"),
                               args);
        JOptionPane.showMessageDialog(null, err,
                                      fLabels.getString("folderCreateError"),
                                      JOptionPane.ERROR_MESSAGE);
      } else {
        if (newFolder.create(Folder.HOLDS_FOLDERS|Folder.HOLDS_MESSAGES)) {
          return true;
        }
      }
    } catch (MessagingException e) {
      System.out.println("Create Folder: " + e);
    }
    return false;
  }

  class OptionListener implements PropertyChangeListener {
    public void propertyChange(PropertyChangeEvent aEvent) {
      if (aEvent.getPropertyName().equals(JOptionPane.VALUE_PROPERTY)){
        int value = ((Integer) aEvent.getNewValue()).intValue();

        if (value == JOptionPane.OK_OPTION) {
          //  fPanel.saveAll();
          setVisible(!createFolder());
        } else {
          setVisible(false);
        }
      }
    }
  }
}
