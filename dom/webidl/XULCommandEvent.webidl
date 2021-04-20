/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This interface is supported by command events, which are dispatched to
 * XUL elements as a result of mouse or keyboard activation.
 */
[ChromeOnly,
 Exposed=Window]
interface XULCommandEvent : UIEvent
{
  /**
   * Command events support the same set of modifier keys as mouse and key
   * events.
   */
  readonly attribute boolean ctrlKey;
  readonly attribute boolean shiftKey;
  readonly attribute boolean altKey;
  readonly attribute boolean metaKey;

  /**
   * Command events use the same button values as mouse events.
   * The button will be 0 if the command was not caused by a mouse event.
   */
  readonly attribute short button;

  /**
   * The input source, if this event was triggered by a mouse event.
   */
  readonly attribute unsigned short inputSource;

  /**
   * If the command event was redispatched because of a command= attribute
   * on the original target, sourceEvent will be set to the original DOM Event.
   * Otherwise, sourceEvent is null.
   */
  readonly attribute Event? sourceEvent;

  /**
   * Creates a new command event with the given attributes.
   */
  [Throws]
  void initCommandEvent(DOMString type,
                        optional boolean canBubble = false,
                        optional boolean cancelable = false,
                        optional Window? view = null,
                        optional long detail = 0,
                        optional boolean ctrlKey = false,
                        optional boolean altKey = false,
                        optional boolean shiftKey = false,
                        optional boolean metaKey = false,
                        optional short buttonArg = 0,
                        optional Event? sourceEvent = null,
                        optional unsigned short inputSource = 0);
};
