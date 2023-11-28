/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[ChromeOnly, Exposed=Window]
interface TreeColumn {
  readonly attribute Element element;

  readonly attribute TreeColumns? columns;

  [Throws]
  readonly attribute long x;
  [Throws]
  readonly attribute long width;

  readonly attribute DOMString id;
  readonly attribute long index;

  readonly attribute boolean primary;
  readonly attribute boolean cycler;
  readonly attribute boolean editable;

  const short TYPE_TEXT                = 1;
  const short TYPE_CHECKBOX            = 2;
  readonly attribute short type;

  TreeColumn? getNext();
  TreeColumn? getPrevious();

  /**
   * Returns the previous displayed column, if any, accounting for
   * the ordinals set on the columns.
   */
  readonly attribute TreeColumn? previousColumn;

  [Throws]
  undefined invalidate();
};
