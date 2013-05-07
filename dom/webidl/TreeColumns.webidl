/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface MozTreeBoxObject;
interface MozTreeColumn;

interface TreeColumns {
  /**
   * The tree widget for these columns.
   */
  readonly attribute MozTreeBoxObject? tree;

  /**
   * The number of columns.
   */
  readonly attribute unsigned long count;

  /**
   * An alias for count (for the benefit of scripts which treat this as an
   * array).
   */
  readonly attribute unsigned long length;

  /**
   * Get the first/last column.
   */
  MozTreeColumn? getFirstColumn();
  MozTreeColumn? getLastColumn();

  /**
   * Attribute based column getters.
   */
  MozTreeColumn? getPrimaryColumn();
  MozTreeColumn? getSortedColumn();
  MozTreeColumn? getKeyColumn();

  /**
   * Get the column for the given element.
   */
  MozTreeColumn? getColumnFor(Element? element);

  /**
   * Parametric column getters.
   */
  getter MozTreeColumn? getNamedColumn(DOMString id);
  getter MozTreeColumn? getColumnAt(unsigned long index);

  /**
   * This method is called whenever a treecol is added or removed and
   * the column cache needs to be rebuilt.
   */
  void invalidateColumns();

  void restoreNaturalOrder();
};
