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
  long alignmentOffset = 0;
};

[ChromeOnly, Exposed=Window]
interface PopupPositionedEvent : Event {
  constructor(DOMString type, optional PopupPositionedEventInit init = {});

  readonly attribute boolean isAnchored;
  readonly attribute DOMString alignmentPosition;
  readonly attribute long alignmentOffset;
};
