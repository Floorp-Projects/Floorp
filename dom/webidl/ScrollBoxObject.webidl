/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[NoInterfaceObject]
interface ScrollBoxObject : BoxObject {

  /**
   * Scroll to the given coordinates, in css pixels.
   * (0,0) will put the top left corner of the scrolled element's padding-box
   * at the top left corner of the scrollport (which is its inner-border-box).
   * Values will be clamped to legal values.
   */
  [Throws]
  void scrollTo(long x, long y);

  /**
   * Scroll the given amount of device pixels to the right and down.
   * Values will be clamped to make the resuling position legal.
   */
  [Throws]
  void scrollBy(long dx, long dy);
  [Throws]
  void scrollByLine(long dlines);
  [Throws]
  void scrollByIndex(long dindexes);
  [Throws]
  void scrollToLine(long line);
  [Throws]
  void scrollToElement(Element child);
  [Throws]
  void scrollToIndex(long index);

  /**
   * Get the current scroll position in css pixels.
   * @see scrollTo for the definition of x and y.
   */
  [Pure, Throws]
  readonly attribute long positionX;
  [Pure, Throws]
  readonly attribute long positionY;
  [Pure, Throws]
  readonly attribute long scrolledWidth;
  [Pure, Throws]
  readonly attribute long scrolledHeight;

  /**
   * DEPRECATED: Please use positionX and positionY
   *
   * Get the current scroll position in css pixels.
   * @see scrollTo for the definition of x and y.
   */
  [Throws]
  void getPosition(object x, object y);

  /**
   * DEPRECATED: Please use scrolledWidth and scrolledHeight
   */
  [Throws]
  void getScrolledSize(object width, object height);

  [Throws]
  void ensureElementIsVisible(Element child);
  [Throws]
  void ensureIndexIsVisible(long index);
  [Throws]
  void ensureLineIsVisible(long line);
};
