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
 * Created: Will Scullin <scullin@netscape.com>,  6 Nov 1997.
 */

package grendel.ui;

import java.awt.Color;
import java.awt.Component;
import java.awt.Dimension;
import java.awt.Insets;
import java.util.StringTokenizer;

import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Store;
import javax.mail.Folder;

import javax.swing.ImageIcon;
import javax.swing.ListCellRenderer;
import javax.swing.JComboBox;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.border.EmptyBorder;
import javax.swing.UIManager;

import grendel.view.ViewedStore;
import grendel.view.ViewedFolder;

public class FolderCombo extends JComboBox {
  int fExclude;
  int fInclude;

  Color fTextColor;
  Color fHighlightTextColor;
  Color fWindowColor;
  Color fHighlightColor;

  public FolderCombo() {
    updateUI();
  }

  public void populate() {
    populate(-1, 0);
  }

  public void populate(int aInclude, int aExclude) {
    ViewedStore stores[] = StoreFactory.Instance().getStores();
    fInclude = aInclude;
    fExclude = aExclude;

    removeAllItems();

    try {
      for (int i = 0; i < stores.length; i++) {
        populateRecurse(stores[i], fInclude, fExclude);
      }
    } catch (MessagingException e) {
      e.printStackTrace();
    }
  }

  int populateRecurse(ViewedFolder aFolder, int fInclude, int fExclude)
    throws MessagingException {

    int total = 0;

    while (aFolder != null) {
      int idx = getItemCount();
      int count = populateRecurse(aFolder.getFirstSubFolder(),
                                  fInclude, fExclude);
      total += count;

      if ((aFolder.getFolder().getType() & fInclude) != 0 &&
          (aFolder.getFolder().getType() & fExclude) == 0) {
        insertItemAt(aFolder, idx);
        total++;
      } else if (count > 0) {
        insertItemAt(aFolder, idx);
        total++;
      }
      aFolder = aFolder.getNextFolder();
    }

    return total;
  }

  public ViewedFolder getSelectedFolder() {
    return (ViewedFolder) getSelectedItem();
  }

  class FolderRenderer extends JLabel implements ListCellRenderer {
    public FolderRenderer() {
      setIcon(new ImageIcon(getClass().getResource("/grendel/ui/images/folder-small.gif")));
      setOpaque(true);
    }

    public Component getListCellRendererComponent(JList aList,
                                                  Object aValue,
                                                  int aIndex,
                                                  boolean aIsSelected,
                                                  boolean aHasFocus) {
      boolean isEnabled = true;

      Folder folder = null;

      if (aValue != null) {
        folder = ((ViewedFolder) aValue).getFolder();
      }

      if (folder != null) {
        try {
          StringTokenizer counter =
            new StringTokenizer(folder.getFullName(),
                                "" +
                                folder.getSeparator());

          int depth = counter.countTokens();
          setBorder(new EmptyBorder(0, depth * 16, 0, 0));
          setText(folder.getName());

          if ((folder.getType() & fInclude) == 0 ||
              (folder.getType() & fExclude) != 0) {
            isEnabled = false;
          }
        } catch (MessagingException e) {
          e.printStackTrace();
        }
      } else {
        setText("");
      }

      setEnabled(isEnabled);

      if (aIsSelected) {
        setBackground(fHighlightColor);
        setForeground(fHighlightTextColor);
      } else {
        setBackground(fWindowColor);
        setForeground(fTextColor);
      }

      return this;
    }
  }

  public void updateUI() {
    super.updateUI();

    fTextColor = UIManager.getColor("textText");
    fHighlightTextColor = UIManager.getColor("textHighlightText");
    fWindowColor = UIManager.getColor("window");
    fHighlightColor = UIManager.getColor("textHighlight");

    setRenderer(new FolderRenderer());
  }
}

