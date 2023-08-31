/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary PopupPositionedEventInit : EventInit {
  boolean isAnchored = false;
  /**
   * Returns the alignment position where the popup has appeared relative to its
   * anchor node or point, accounting for any flipping that occurred.
   */
  DOMString alignmentPosition = "";
  /**
   * Returns the alignment position of the popup, e.g. "topcenter" for a popup
   * whose top center is aligned with the anchor node.
   */
  DOMString popupAlignment = "";
  long alignmentOffset = 0;
};

[ChromeOnly, Exposed=Window]
interface PopupPositionedEvent : Event {
  constructor(DOMString type, optional PopupPositionedEventInit init = {});

  readonly attribute boolean isAnchored;
  readonly attribute DOMString alignmentPosition;
  readonly attribute DOMString popupAlignment;
  readonly attribute long alignmentOffset;
};
