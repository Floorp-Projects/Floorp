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
 * Created: Will Scullin <scullin@netscape.com>,  9 Oct 1997.
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.search;

import java.awt.BorderLayout;
import java.awt.Canvas;
import java.awt.Component;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;

import javax.swing.Action;
import javax.swing.AbstractAction;
import javax.swing.BoxLayout;
import javax.swing.JComponent;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.KeyStroke;
import javax.swing.border.EmptyBorder;

//import netscape.orion.toolbars.BarLayout;

import grendel.ui.GeneralFrame;

public class SearchFrame extends GeneralFrame {
  SearchPanel fSearchPanel;

  JComponent  fTargetPanel;
  JComponent  fActionPanel;
  JButton     fSearchButton;
  JButton     fCloseButton;

  public SearchFrame() {
    super("Search", "mail.search");

    fSearchPanel = new SearchPanel(new MailSearch());
    getContentPane().add(fSearchPanel);

    fActionPanel = new JPanel(new BoxLayout(this, BoxLayout.X_AXIS), true);
    fActionPanel.setBorder(new EmptyBorder(5,5,5,5));
    fActionPanel.add(new Canvas());

    fCloseButton = new JButton("Close");
    fCloseButton.addActionListener(new ActionListener() {
      public void actionPerformed(ActionEvent aEvent) {
        dispose();
      }
    });
    fActionPanel.add(fCloseButton);
    fSearchButton = new JButton("Search");
    fSearchButton.addActionListener(new SearchAction());
    fActionPanel.add(fSearchButton);

    fPanel.add(BorderLayout.SOUTH, fActionPanel);

    fTargetPanel = new JPanel(new BoxLayout(this, BoxLayout.X_AXIS), true);
    fTargetPanel.setBorder(new EmptyBorder(5,5,5,5));

    fTargetPanel.add(new Canvas());
    fTargetPanel.add(new JLabel("Search In: "));
    fTargetPanel.add(fSearchPanel.getSearchable().getTargetComponent());
    fTargetPanel.add(new Canvas());

    fPanel.add(BorderLayout.NORTH, fTargetPanel);

    getRootPane().registerKeyboardAction(new SearchAction(),
                                KeyStroke.getKeyStroke(KeyEvent.VK_ENTER, 0),
                                JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    restoreBounds();
    setVisible(true);
  }

  public void dispose() {
    saveBounds();

    super.dispose();
  }

  class SearchAction extends AbstractAction {
    SearchAction() {
      super("search");
    }

    public void actionPerformed(ActionEvent aEvent) {
      ISearchable searchable = fSearchPanel.getSearchable();
      searchable.search(fSearchPanel.getSearch());
      ISearchResults results = searchable.getSearchResults();
      Component c = searchable.getResultComponent(results);
      if (c != null) {
        new ResultsFrame(c);
      }
    }
  }
}
