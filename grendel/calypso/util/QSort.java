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

/** Implements QuickSort, on an array of objects.
    An implementor of `Comparer' is passed in to compare two objects.
  */
public class QSort {

  private Comparer comp;

  /** Create a QSort object.
      One way to use this would with dynamic class creation:
      <PRE>
      QSort sorter = new QSort(new Comparer() {
                                 public int compare(Object a, Object b) {
                                   if (a.key == b.key) return 0;
                                   else if (a.key < b.key) return -1;
                                   else return 1;
                                 }
                               });
      sorter.sort(array);
      </PRE>
   */
  public QSort(Comparer c) {
    comp = c;
  }

  /** Sorts the array, according to the Comparer. */
  public void sort(Object[] list) {
    quicksort(list, 0, list.length - 1);
  }

  /** Sorts a subsequence of the array, according to the Comparer. */
  public void sort(Object[] list, int start, int end) {
    quicksort(list, start, end - 1);
  }

  private void quicksort(Object[] list, int p, int r) {
    if (p < r) {
      int q = partition(list,p,r);
      if (q == r) {
        q--;
      }
      quicksort(list,p,q);
      quicksort(list,q+1,r);
    }
  }

  private int partition (Object[] list, int p, int r) {
    Object pivot = list[p];
    int lo = p;
    int hi = r;

    while (true) {

      while (comp.compare(list[hi], pivot) >= 0 &&
             lo < hi) {
        hi--;
      }
      while (comp.compare(list[lo], pivot) < 0 &&
             lo < hi) {
        lo++;
      }
      if (lo < hi) {
        Object T = list[lo];
        list[lo] = list[hi];
        list[hi] = T;
      }
      else return hi;
    }
  }

//  public static void main(String argv[]) {
//    String array[] = { "7", "3", "0", "4", "5", "6", "2", "1" };
//    QSort sorter = new QSort(new Comparer() {
//                               public int compare(Object a, Object b) {
//                                 int ai = Integer.parseInt((String) a);
//                                 int bi = Integer.parseInt((String) b);
//                                 if (ai == bi) return 0;
//                                 else if (ai < bi) return -1;
//                                 else return 1;
//                               }
//                             });
//    sorter.sort(array);
//    for (int i = 0; i < array.length; i++)
//      System.out.print(array[i] + " ");
//    System.out.println();
//  }

}
