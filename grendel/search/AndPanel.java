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

import java.awt.Dimension;
import java.awt.LayoutManager2;

import javax.mail.search.AndTerm;
import javax.mail.search.SearchTerm;

import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.border.BevelBorder;
import javax.swing.border.TitledBorder;

import grendel.widgets.SelectionManager;

class AndPanel extends NaryTermPanel {
  public AndPanel(SearchPanel aParent) {
    super(aParent, new OperatorLayout());
    if (bStyle2) {
      setBorder(new TitledBorder("And"));
    } else {
      setBorder(new BevelBorder(BevelBorder.RAISED));
      add(OperatorLayout.kOperator, new JLabel("And"));
    }
  }

  public SearchTerm getSearch() {
    int count = getTermCount();
    SearchTerm term = getTerm(0).getSearch();
    for (int i = 1; i < count; i++) {
      term = new AndTerm(term, getTerm(i).getSearch());
    }
    return term;
  }
}
