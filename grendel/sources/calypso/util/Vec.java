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

import java.util.*;

/**
 * Some more methods for Vector
 */
public class Vec extends Vector {
  public Vec(int initialCapacity, int capacityIncrement) {
    super(initialCapacity, capacityIncrement);
  }

  public Vec(int initialCapacity) {
    super(initialCapacity, 0);
  }

  public Vec() {
  }

  public boolean containsIdentical(Object aElement) {
    return indexOfIdentical(aElement, 0) != -1;
  }

  public int indexOfIdentical(Object aElement) {
    return indexOfIdentical(aElement, 0);
  }

  public synchronized int indexOfIdentical(Object aElement, int aIndex) {
    for (int i = aIndex; i < elementCount ; i++) {
      if (aElement == elementData[i]) {
        return i;
      }
    }
    return -1;
  }

  public boolean insertElementAfter(Object aElement, Object aExistingElement)
  {
    if (aElement == null) {
      throw new NullPointerException(
        "It is illegal to store nulls in Vectors.");
    }
    if (aExistingElement == null) {
      return false;
    }
    int index = indexOf(aExistingElement);
    if (index == -1) {
      return false;
    }
    if (index >= elementCount - 1) {
      addElement(aElement);
    } else {
      insertElementAt(aElement, index + 1);
    }
    return true;
  }

  public void addElements(Vector aVector) {
    int addCount, i;

    if (aVector == null)
      return;
    addCount = aVector.size();
    if (elementData == null || (elementCount + addCount) >= elementData.length)
      ensureCapacity(elementCount + addCount);

    for (i = 0; i < addCount; i++)
      addElement(aVector.elementAt(i));
  }

  public synchronized boolean removeElementIdentical(Object aElement) {
    int i = indexOfIdentical(aElement);
    if (i >= 0) {
      removeElementAt(i);
      return true;
    }
    return false;
  }

  public synchronized Object removeFirstElement() {
    if (elementCount == 0) {
      return null;
    }
    Object rv = elementData[0];
    removeElementAt(0);
    return rv;
  }

  public synchronized Object removeLastElement() {
    if (elementCount == 0) {
      return null;
    }
    int index = elementCount - 1;
    Object rv = elementData[index];
    removeElementAt(index);
    return rv;
  }

  static Comparer gStringComparer;

  public synchronized void sort() {
    if (gStringComparer == null) {
      gStringComparer = new StringComparer();
    }
    QSort s = new QSort(gStringComparer);
    s.sort(elementData, 0, elementCount);
  }

  public synchronized void sort(Comparer aComparer) {
    QSort s = new QSort(aComparer);
    s.sort(elementData, 0, elementCount);
  }

  static class StringComparer implements Comparer {
    public int compare(Object a, Object b) {
      return ((String)a).compareTo((String)b);
    }
  }
}
