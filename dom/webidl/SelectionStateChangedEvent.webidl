/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum SelectionState {
  "drag",
  "mousedown",
  "mouseup",
  "keypress",
  "selectall",
  "collapsetostart",
  "collapsetoend",
  "blur",
  "updateposition",
  "taponcaret"
};

dictionary SelectionStateChangedEventInit : EventInit {
  boolean visible = true;
  DOMString selectedText = "";
  DOMRectReadOnly? boundingClientRect = null;
  sequence<SelectionState> states = [];
};

[Constructor(DOMString type, optional SelectionStateChangedEventInit eventInit),
 ChromeOnly]
interface SelectionStateChangedEvent : Event {
  readonly attribute boolean visible;
  readonly attribute DOMString selectedText;
  readonly attribute DOMRectReadOnly? boundingClientRect;
  [Cached, Pure] readonly attribute sequence<SelectionState> states;
};
