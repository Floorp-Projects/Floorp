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
 * Created: Will Scullin <scullin@netscape.com>, 18 Sep 1997.
 */

package grendel.widgets;

public class TreeTableModelBroadcaster implements TreeTableModelListener {
  TreeTableModelListener a, b;

  public TreeTableModelBroadcaster(TreeTableModelListener a, TreeTableModelListener b) {
    this.a = a;
    this.b = b;
  }

  public static TreeTableModelListener add(TreeTableModelListener a, TreeTableModelListener b) {
    if (a == null) return b;
    if (b == null) return a;

    return new TreeTableModelBroadcaster(a, b);
  }

  public static TreeTableModelListener remove(TreeTableModelListener a, TreeTableModelListener b) {
    if (a == b || a == null) {
      return null;
    } else if (a instanceof TreeTableModelBroadcaster) {
      return ((TreeTableModelBroadcaster)a).remove(b);
    } else {
      return a;   // it's not here
    }
  }

  public TreeTableModelListener remove(TreeTableModelListener c) {
    if (c == a) return b;
    if (c == b) return a;

    TreeTableModelListener a2 = remove(a, c);
    TreeTableModelListener b2 = remove(b, c);
    if (a2 == a && b2 == b) {
        return this;  // it's not here
    }
    return add(a2, b2);
  }

  public void nodeExpanded(TreeTableModelEvent aEvent) {
    a.nodeExpanded(aEvent);
    b.nodeExpanded(aEvent);
  }

  public void nodeCollapsed(TreeTableModelEvent aEvent) {
    a.nodeCollapsed(aEvent);
    b.nodeCollapsed(aEvent);
  }

  public void nodeInserted(TreeTableModelEvent aEvent) {
    a.nodeInserted(aEvent);
    b.nodeInserted(aEvent);
  }

  public void nodeDeleted(TreeTableModelEvent aEvent) {
    a.nodeDeleted(aEvent);
    b.nodeDeleted(aEvent);
  }

  public void nodeChanged(TreeTableModelEvent aEvent) {
    a.nodeChanged(aEvent);
    b.nodeChanged(aEvent);
  }
}
