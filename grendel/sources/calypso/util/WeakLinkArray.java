/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 */

package calypso.util;

import java.util.Enumeration;

/**
 * An array of objects whose references are weak. Weak references
 * hold a reference to the object that the GC can break when the
 * object no longer has any hard references.
 */
public class WeakLinkArray {
  WeakLink fArray[];
  int fCount;

  public WeakLinkArray() {
  }

  public void addElement(Object aObject) {
    if (fArray == null) {
      fArray = new WeakLink[10];
    } else {
      // Grow the array if it's too small
      int len = fArray.length;
      if (fCount == len) {
        // Try to use a slot in the array that contains a dead weak
        // link instead of growing the array.
        for (int i = 0; i < fCount; i++) {
          WeakLink w = fArray[i];
          if ((w == null) || (w.get() == null)) {
            if (w == null) {
              fArray[i] = new WeakLink(aObject);
            } else {
              w.setThing(aObject);
            }
            return;
          }
        }

        // Grow the array.
        len += 10;
        WeakLink newArray[] = new WeakLink[len];
        System.arraycopy(fArray, 0, newArray, 0, fCount);
        fArray = newArray;
      }
    }
    fArray[fCount++] = new WeakLink(aObject);
  }

  public void removeElement(Object aObject) {
    int n = fCount;
    for (int i = 0; i < n; i++) {
      WeakLink w = fArray[i];
      if (w != null) {
        Object obj = w.get();
        if ((obj == null) || (obj == aObject)) {
          fArray[i] = null;
        }
      }
    }
  }

  public boolean contains(Object aObject) {
    int n = fCount;
    for (int i = 0; i < n; i++) {
      WeakLink w = fArray[i];
      if ((w != null) && (w.get() == aObject)) {
        return true;
      }
    }
    return false;
  }

  public Enumeration elements() {
    return new WeakLinkArrayEnumeration(this);
  }
  // XXX hashCode, equals, toString
}
