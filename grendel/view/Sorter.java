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
 * Created: Jamie Zawinski <jwz@netscape.com>, 13 Jun 1995.
 * Ported from C on 14 Aug 1997.
 */

package grendel.view;

import java.util.Vector;
import java.util.Enumeration;
import calypso.util.QSort;
import calypso.util.Comparer;

/**
   This sorts a set of messages by some metric.
   The set of messages may already be arranged into a thread hierarchy; in
   that case, siblings are sorted while leaving parent/child relationships
   intact.  The sets of messages are accessed via the ISortable interface.

   @see ISortable
   @see Threader
   @see Comparer
   @see QSort
 */

class Sorter {

  private Vector v = null;
  private QSort sorter = null;

  /** Creates an object for sorting messages.
      Use the sortMessageChildren() method to sort them.
      @param comparer    The object which compares two ISortable objects
                         for ordering.
   */
  public Sorter(Comparer comparer) {
    this.sorter = new QSort(comparer);
    v = new Vector(100);
  }

  /** Sorts the set of messages indicated by sortable_root.
      The child-list of sortable_root will be modified (reordered)
      upon completion (as will all grandchildren.)
      @param    sortable_root       The root object; it should have
                                    children, but no siblings.
    */
  public void sortMessageChildren(ISortable sortable_root) {
    sortMessageChildren_1(sortable_root);
    sorter = null;
    v.setSize(0);
    v.trimToSize();
    v = null;
  }

  private void sortMessageChildren_1(ISortable parent) {

    int count = 0;

    // Populate v with the kids.
    for (Enumeration e = parent.children();
         e != null && e.hasMoreElements();) {
      v.addElement((ISortable) e.nextElement());
      count++;
    }

    // Copy v to a, growing a if necessary.
    ISortable a[] = new ISortable[count+1];
    v.copyInto(a);
    v.setSize(0);

    // Sort a.
    sorter.sort(a, 0, count);

    // Flush new order of a into the ISortables.
    parent.setChild(a[0]);
    a[count] = null;
    for (int i = 0; i < count; i++)
      a[i].setNext(a[i+1]);

    // Repeat on the grandchildren.
    for (int i = 0; i < count; i++)
      sortMessageChildren_1(a[i]);
    a = null;
  }
}
