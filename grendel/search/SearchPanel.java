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
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 31 Dec 1998
 */

package grendel.search;

import java.awt.BorderLayout;
import java.awt.Canvas;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Frame;
import java.awt.GridLayout;
import java.awt.Insets;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.Enumeration;

import javax.mail.search.SearchTerm;

import javax.swing.BoxLayout;
import javax.swing.JComponent;
import javax.swing.JButton;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.border.EmptyBorder;

import grendel.widgets.SelectionManager;
import grendel.widgets.SingleSelectionManager;

public class SearchPanel extends JPanel {
  ISearchable fSearchable;
  SearchPanel fThis;

  JComponent  fOperatorPanel;
  JScrollPane fQueryPanel;
  JButton     fAndButton;
  JButton     fOrButton;
  JButton     fDeleteButton;
  NoopPanel    fOuterTerm;

  SelectionManager fSelection;

  public SearchPanel(ISearchable aSearchable) {
    super(new BorderLayout(), true);
    fThis = this;

    fSearchable = aSearchable;
    fSelection = new SingleSelectionManager();

    // Create Query Panel
    fQueryPanel = new JScrollPane();

    fOuterTerm = new NoopPanel(this);
    fOuterTerm.addTerm(new AttributePanel(this, fSearchable));

    fQueryPanel.setViewportView(fOuterTerm);
    add(fQueryPanel);

    // Create Operator buttons
    fOperatorPanel = new JPanel();
    fOperatorPanel.setLayout(new BoxLayout(fOperatorPanel, BoxLayout.Y_AXIS));
    fOperatorPanel.setBorder(new EmptyBorder(5,5,5,5));

    fAndButton = new JButton("And");
    fAndButton.addActionListener(new AndAction());
    fOperatorPanel.add(fAndButton);

    fOrButton = new JButton("Or");
    fOrButton.addActionListener(new OrAction());
    fOperatorPanel.add(fOrButton);

    fDeleteButton = new JButton("Delete");
    fDeleteButton.addActionListener(new DeleteAction());
    fOperatorPanel.add(fDeleteButton);

    add(BorderLayout.EAST, fOperatorPanel);
  }

  public void setSearchable(ISearchable aSearchable) {
    fSearchable = aSearchable;
  }

  public ISearchable getSearchable() {
    return fSearchable;
  }

  public SearchTerm getSearch() {
    return fOuterTerm.getSearch();
  }

  public SelectionManager getSelection() {
    return fSelection;
  }

  protected void sanitize(NaryTermPanel aTerm) {
    int i = 0;
    while (i < aTerm.getTermCount()) {
      TermPanel term = aTerm.getTerm(i);
      if (term instanceof NaryTermPanel) {
        NaryTermPanel nary = (NaryTermPanel) term;
        sanitize(nary);
        if (nary.getTermCount() < 1) {
          aTerm.removeTerm(nary);
        } else {
          if (nary.getTermCount() < 2) {
            aTerm.replaceTerm(nary, nary.getTerm(0));
          }
          i++;
        }
      } else {
        i++;
      }
    }
  }

  protected TermPanel getSelectedTerm() {
    TermPanel term = null;

    Enumeration selection = fSelection.getSelection();
    if (selection.hasMoreElements()) {
      term = (TermPanel) selection.nextElement();
    } else if (fOuterTerm.getTermCount() != 0) {
      term = fOuterTerm.getTerm(0);
    }

    return term;
  }

  class OrAction implements ActionListener {
    public void actionPerformed(ActionEvent aEvent) {
      TermPanel term = getSelectedTerm();

      if (term instanceof OrPanel) {
        OrPanel or = (OrPanel) term;
        or.addTerm(new AttributePanel(fThis, fSearchable));
      } else {
        NaryTermPanel parent = term != null ? term.getParentTerm() : null;
        OrPanel or = new OrPanel(fThis);
        if (parent != null) {
          parent.replaceTerm(term, or);
          or.addTerm(term);
        } else {
          fOuterTerm.addTerm(or);
        }
        or.addTerm(new AttributePanel(fThis, fSearchable));
      }
      sanitize(fOuterTerm);
      validate();
    }
  }

  class AndAction implements ActionListener {
    public void actionPerformed(ActionEvent aEvent) {
      TermPanel term = getSelectedTerm();

      if (term instanceof AndPanel) {
        AndPanel and = (AndPanel) term;
        and.addTerm(new AttributePanel(fThis, fSearchable));
      } else {
        NaryTermPanel parent = term != null ? term.getParentTerm() : null;
        AndPanel and = new AndPanel(fThis);
        if (parent != null) {
          parent.replaceTerm(term, and);
          and.addTerm(term);
        } else {
          fOuterTerm.addTerm(and);
        }
        and.addTerm(new AttributePanel(fThis, fSearchable));
      }
      validate();
    }
  }

  class DeleteAction implements ActionListener {
    public void actionPerformed(ActionEvent aEvent) {
      TermPanel term = getSelectedTerm();

      NaryTermPanel parent = term != null ? term.getParentTerm() : null;
      if (parent != null) {
        parent.removeTerm(term);
      }
      sanitize(fOuterTerm);
      validate();
    }
  }
}
