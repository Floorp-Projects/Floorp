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
 * Created: Will Scullin <scullin@netscape.com>,  4 Nov 1997.
 */

package grendel.search;

import java.awt.Component;
import java.awt.Dimension;
import java.awt.GridBagLayout;
import java.awt.LayoutManager2;
import java.awt.SystemColor;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;

import javax.mail.search.SearchTerm;

import javax.swing.JComboBox;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JPanel;
import javax.swing.JTextField;
import javax.swing.ListCellRenderer;
import javax.swing.border.EmptyBorder;

import grendel.widgets.SelectionManager;

class AttributePanel extends TermPanel {
  ISearchable fSearchable;
  int         fMargin = 3;
  JComboBox   fAttribCombo;
  JComboBox   fOpCombo;
  Component   fValueComponent;

  public AttributePanel(SearchPanel aParent, ISearchable aSearchable) {
    super(aParent, new AttributeLayout());

    fSearchable = aSearchable;

    setBorder(new EmptyBorder(fMargin, fMargin, fMargin, fMargin));

    fAttribCombo = new JComboBox();
    add(fAttribCombo, AttributeLayout.kAttribute);
    updateAttribCombo();

    fOpCombo = new JComboBox();
    add(fOpCombo, AttributeLayout.kOperator);
    updateOpCombo();

    fValueComponent = null;
    updateValueComponent();

    fAttribCombo.addItemListener(new AttribListener());
  }

  public SearchTerm getSearch() {
    ISearchAttribute attrib =
      (ISearchAttribute) fAttribCombo.getSelectedItem();

    return attrib.getAttributeTerm(fOpCombo.getSelectedItem(),
                                   attrib.getValue(fValueComponent));
  }

  void updateAttribCombo() {
    fAttribCombo.removeAllItems();
    for (int i = 0; i < fSearchable.getSearchAttributeCount(); i++) {
      fAttribCombo.addItem(fSearchable.getSearchAttribute(i));
    }
    fAttribCombo.setSelectedIndex(0);
  }

  void updateOpCombo() {
    ISearchAttribute attrib =
      (ISearchAttribute) fAttribCombo.getSelectedItem();

    fOpCombo.removeAllItems();
    for (int i = 0; i < attrib.getOperatorCount(); i++) {
      fOpCombo.addItem(attrib.getOperator(i));
    }
    fOpCombo.setSelectedIndex(0);
  }

  void updateValueComponent() {
    ISearchAttribute attrib =
      (ISearchAttribute) fAttribCombo.getSelectedItem();

    if (fValueComponent != null) {
      remove(fValueComponent);
    }
    fValueComponent = attrib.getValueComponent();
    if (fValueComponent != null) {
      add(fValueComponent, AttributeLayout.kValue);
    }
  }

  class AttribListener implements ItemListener {
    public void itemStateChanged(ItemEvent aEvent) {
      if (aEvent.getStateChange() == ItemEvent.SELECTED) {
        updateOpCombo();
        updateValueComponent();
        repaint();
      }
    }
  }
}
