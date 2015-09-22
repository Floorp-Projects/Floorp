/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.presentation.enabled",
 CheckAnyPermissions="presentation"]
interface Presentation : EventTarget {
 /*
  * This should be used by the UA as the default presentation request for the
  * controller. When the UA wishes to initiate a PresentationSession on the
  * controller's behalf, it MUST start a presentation session using the default
  * presentation request (as if the controller had called |defaultRequest.start()|).
  *
  * Only used by controlling browsing context (senders).
  */
  attribute PresentationRequest? defaultRequest;

  /*
   * This should be available on the receiving browsing context in order to
   * access the controlling browsing context and communicate with them.
   */
  [SameObject]
  readonly attribute PresentationReceiver? receiver;
};
