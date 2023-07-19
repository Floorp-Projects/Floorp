/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://www.w3.org/TR/webrtc-encoded-transform
 */

[Pref="media.peerconnection.enabled",
 Pref="media.peerconnection.scripttransform.enabled",
 Exposed=DedicatedWorker]
interface RTCTransformEvent : Event {
   constructor(DOMString type, RTCTransformEventInit eventInitDict);
   readonly attribute RTCRtpScriptTransformer transformer;
};

dictionary RTCTransformEventInit : EventInit {
  required RTCRtpScriptTransformer transformer;
};
