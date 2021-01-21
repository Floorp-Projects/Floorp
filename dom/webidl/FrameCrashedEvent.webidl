/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Exposed=Window]
interface FrameCrashedEvent : Event
{
  constructor(DOMString type,
              optional FrameCrashedEventInit eventInitDict = {});

  /**
   * The browsingContextId of the frame that crashed.
   */
  readonly attribute unsigned long long browsingContextId;

  /**
   * True if the top-most frame crashed.
   */
  readonly attribute boolean isTopFrame;

  /**
   * Internal process identifier of the frame that crashed. This will be
   * 0 if this identifier is not known, for example a process that failed
   * to start.
   */
  readonly attribute unsigned long long childID;
};

dictionary FrameCrashedEventInit : EventInit
{
  unsigned long long browsingContextId = 0;
  boolean isTopFrame = true;
  unsigned long long childID = 0;
};
