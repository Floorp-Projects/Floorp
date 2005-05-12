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
 * Created: Will Scullin <scullin@netscape.com>, 17 Sep 1997.
 */

package grendel.widgets;

import java.util.EventObject;
import javax.swing.tree.TreePath;

/**
 * A TreeTableDataModel event. Used for reporting which node and possibly
 * which child nodes were involved in an event.
 *
 * @see TreeTableDataModel
 * @see TreeTableModelListener
 */

public class TreeTableModelEvent extends EventObject {
  TreePath fPath;
  Object fChildren[];

  /**
   * Constructor for event with a TreePath.
   */

  public TreeTableModelEvent(Object aSource, TreePath aPath) {
    this(aSource, aPath, null);
  }

  /**
   * Constructor for event with a TreePath and children.
   */

  public TreeTableModelEvent(Object aSource, TreePath aPath,
                             Object aChildren[]) {
    super(aSource);
    fPath = aPath;
    fChildren = aChildren;
  }

  /**
   * @return The TreePath associated with this event
   */

  public TreePath getPath() {
    return fPath;
  }

  /**
   * @return The node children associated with this event. Can be null.
   */

  public Object[] getChildren() {
    return fChildren;
  }
}
