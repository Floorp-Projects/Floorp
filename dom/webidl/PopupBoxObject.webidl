/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Func="IsChromeOrXBL"]
interface PopupBoxObject : BoxObject
{
  /**
   * Allow the popup to automatically position itself.
   */
  attribute boolean autoPosition;

  /**
   * Open the popup relative to a specified node at a specific location.
   *
   * The popup may be either anchored to another node or opened freely.
   * To anchor a popup to a node, supply an anchor node and set the position
   * to a string indicating the manner in which the popup should be anchored.
   * Possible values for position are:
   *    before_start, before_end, after_start, after_end,
   *    start_before, start_after, end_before, end_after,
   *    overlap, after_pointer
   *
   * The anchor node does not need to be in the same document as the popup.
   *
   * If the attributesOverride argument is true, the popupanchor, popupalign
   * and position attributes on the popup node override the position value
   * argument. If attributesOverride is false, the attributes are only used
   * if position is empty.
   *
   * For an anchored popup, the x and y arguments may be used to offset the
   * popup from its anchored position by some distance, measured in CSS pixels.
   * x increases to the right and y increases down. Negative values may also
   * be used to move to the left and upwards respectively.
   *
   * Unanchored popups may be created by supplying null as the anchor node.
   * An unanchored popup appears at the position specified by x and y,
   * relative to the viewport of the document containing the popup node. In
   * this case, position and attributesOverride are ignored.
   *
   * @param anchorElement the node to anchor the popup to, may be null
   * @param position manner is which to anchor the popup to node
   * @param x horizontal offset
   * @param y vertical offset
   * @param isContextMenu true for context menus, false for other popups
   * @param attributesOverride true if popup node attributes override position
   * @param triggerEvent the event that triggered this popup (mouse click for example)
   */
  void openPopup(Element? anchorElement,
                 optional DOMString position = "",
                 long x,
                 long y,
                 boolean isContextMenu,
                 boolean attributesOverride,
                 Event? triggerEvent);

  /**
   * Open the popup at a specific screen position specified by x and y. This
   * position may be adjusted if it would cause the popup to be off of the
   * screen. The x and y coordinates are measured in CSS pixels, and like all
   * screen coordinates, are given relative to the top left of the primary
   * screen.
   *
   * @param isContextMenu true for context menus, false for other popups
   * @param x horizontal screen position
   * @param y vertical screen position
   * @param triggerEvent the event that triggered this popup (mouse click for example)
   */
  void openPopupAtScreen(long x, long y,
                         boolean isContextMenu,
                         Event? triggerEvent);

  /**
   * Open the popup anchored at a specific screen rectangle. This function is
   * similar to openPopup except that that rectangle of the anchor is supplied
   * rather than an element. The anchor rectangle arguments are screen
   * coordinates.
   */
  void openPopupAtScreenRect(optional DOMString position = "",
                             long x,
                             long y,
                             long width,
                             long height,
                             boolean isContextMenu,
                             boolean attributesOverride,
                             Event? triggerEvent);

  /**
   *  Hide the popup if it is open. The cancel argument is used as a hint that
   *  the popup is being closed because it has been cancelled, rather than
   *  something being selected within the panel.
   *
   * @param cancel if true, then the popup is being cancelled.
   */
  void hidePopup(optional boolean cancel = false);

  /**
   * Returns the state of the popup:
   *   closed - the popup is closed
   *   open - the popup is open
   *   showing - the popup is in the process of being shown
   *   hiding - the popup is in the process of being hidden
   */
  readonly attribute DOMString popupState;

  /**
   * The node that triggered the popup. If the popup is not open, will return
   * null.
   */
  readonly attribute Node? triggerNode;

  /**
   * Retrieve the anchor that was specified to openPopup or for menupopups in a
   * menu, the parent menu.
   */
  readonly attribute Element? anchorNode;

  /**
   * Retrieve the screen rectangle of the popup, including the area occupied by
   * any titlebar or borders present.
   */
  DOMRect getOuterScreenRect();

  /**
   * Move the popup to a point on screen in CSS pixels.
   */
  void moveTo(long left, long top);

  /**
   * Move an open popup to the given anchor position. The arguments have the same
   * meaning as the corresponding argument to openPopup. This method has no effect
   * on popups that are not open.
   */
  void moveToAnchor(Element? anchorElement,
                    optional DOMString position = "",
                    long x, long y,
                    boolean attributesOverride);

  /**
   * Size the popup to the given dimensions
   */
  void sizeTo(long width, long height);

  /** Returns the alignment position where the popup has appeared relative to its
   *  anchor node or point, accounting for any flipping that occurred.
   */
  readonly attribute DOMString alignmentPosition;
  readonly attribute long alignmentOffset;

  void setConstraintRect(DOMRectReadOnly rect);
};
