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
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Will Scullin <scullin@netscape.com>, 11 Nov 1997.
 */

package grendel.search;

import java.awt.Container;
import java.awt.LayoutManager;
import java.util.Vector;


public abstract class NaryTermPanel extends TermPanel {
  Vector fTerms = new Vector();

  public NaryTermPanel(SearchPanel aParent, LayoutManager aLayout) {
    super(aParent, aLayout);
  }

  public void addTerm(TermPanel aPanel) {
    add(aPanel);
    fTerms.addElement(aPanel);
  }

  public void removeTerm(TermPanel aPanel) {
    remove(aPanel);
    fTerms.removeElement(aPanel);
  }

  public int getTermCount() {
    return fTerms.size();
  }

  public TermPanel getTerm(int idx) {
    return (TermPanel) fTerms.elementAt(idx);
  }

  public void replaceTerm(TermPanel aOld, TermPanel aNew) {
    for (int i = 0; i < getComponentCount(); i++) {
      if (getComponent(i) == aOld) {
        remove(i);
        add(aNew, i);
        break;
      }
    }

    int idx = fTerms.indexOf(aOld);
    if (idx >= 0) {
      fTerms.removeElementAt(idx);
      fTerms.insertElementAt(aNew, idx);
    }
  }
}
