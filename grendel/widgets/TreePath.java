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
 * Created: Will Scullin <scullin@netscape.com>, 28 Aug 1997.
 */

package grendel.widgets;

import java.util.Vector;

/**
 * Object that represents the path through a tree to a node.
 * Provides equals and hash methods to ensure expected behaviors.
 *
 * @author Will Scullin
 */

public class TreePath {
  Object fPath[];

  /**
   * Constructs a TreePath from an array of nodes
   */

  public TreePath(Object aPath[]) {
    fPath = aPath;
  }

  /**
   * Constructs a TreePath from a Vector of nodes
   */

  public TreePath(Vector aPath) {
    fPath = new Object[aPath.size()];
    aPath.copyInto(fPath);
  }

  /**
   * Returns the tree path associated with this object.
   */

  public Object[] getPath() {
    return fPath;
  }

  /**
   * Returns the last node in the path
   */

  public Object getTip() {
    return fPath[fPath.length - 1];
  }

  /**
   * Returns the length of the path
   */

  public int getLength() {
    return fPath.length;
  }

  /**
   * Overloaded so that different objects with same contents
   * are considered equal.
   */

  public boolean equals(Object aObject) {
    if (aObject instanceof TreePath) {
      Object path[] = ((TreePath) aObject).getPath();
      int length = path.length;
      if (length == fPath.length) {
        for (int i = length - 1; i >= 0; i--) {
          if (path[i] != fPath[i]) {
            return false;
          }
        }
        return true;
      }
    }
    return false;
  }

  /**
   * Overloaded so different objects with the same contents hash
   * to the same value
   */

  public int hashCode() {
    return getTip().hashCode();
  }
}
