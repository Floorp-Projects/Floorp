/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary PositionStateEventInit : EventInit {
  required double duration;
  required double playbackRate;
  required double position;
};

[Exposed=Window, ChromeOnly]
interface PositionStateEvent : Event {
  constructor(DOMString type, optional PositionStateEventInit eventInitDict = {});
  readonly attribute double duration;
  readonly attribute double playbackRate;
  readonly attribute double position;
};
