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

import java.awt.Color;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.LayoutManager;
import java.awt.SystemColor;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;

import javax.mail.search.SearchTerm;

import javax.swing.JPanel;

import grendel.widgets.SelectionEvent;
import grendel.widgets.SelectionListener;
import grendel.widgets.SelectionManager;

public abstract class TermPanel extends JPanel implements SelectionListener {
  static boolean bStyle2 = false;
  SelectionManager fSelection;
  TermPanel fThis;

  public TermPanel(SearchPanel aParent, LayoutManager l) {
    super(l, true);
    init(aParent.getSelection());
  }

  public TermPanel(SearchPanel aParent) {
    super(true);
    init(aParent.getSelection());
  }

  public NaryTermPanel getParentTerm() {
    Container parent = getParent();
    if (parent instanceof NaryTermPanel) {
      return (NaryTermPanel) parent;
    } else {
      return null;
    }
  }

  void init(SelectionManager aSelection) {
    fSelection = aSelection;
    fThis = this;

    fSelection.addSelectionListener(this);
    addMouseListener(new TermMouseListener());
  }

  public Dimension getPreferredSize() {
    return getLayout().preferredLayoutSize(this);
  }

  public abstract SearchTerm getSearch();

  public void paintComponent(Graphics g) {
    // Save foreground
    Color fg = getForeground();

    if (isSelected()) {
      g.setColor(SystemColor.textHighlight);
      setForeground(SystemColor.textHighlightText);
    } else {
      g.setColor(SystemColor.control);
      setForeground(SystemColor.textText);
    }
    Dimension size = getSize();
    g.fillRect(0, 0, size.width, size.height);

    // Restore foreground
    setForeground(fg);
  }

  boolean isSelected() {
    return fSelection.isSelected(fThis);
  }

  class TermMouseListener extends MouseAdapter {
    public void mouseClicked(MouseEvent aEvent) {
      fSelection.setSelection(fThis);
    }
  }

  public void selectionChanged(SelectionEvent aEvent) {
    Object added[] = aEvent.getAdded();
    Object removed[] = aEvent.getRemoved();
    int i;

    if (added != null) {
      for (i = 0; i < added.length; i++) {
        if (added[i] == this) {
          repaint();
        }
      }
    }
    if (removed != null) {
      for (i = 0; i < removed.length; i++) {
        if (removed[i] == this) {
          repaint();
        }
      }
    }
  }

  public void selectionDoubleClicked(MouseEvent aEvent) {
  }

  public void selectionContextClicked(MouseEvent aEvent) {
  }

  public void selectionDragged(MouseEvent aEvent) {
  }
}
