/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://www.w3.org/TR/webrtc-encoded-transform
 */

// Spec version is commented out (uncomment if SFrameTransform is implemented)
// typedef (SFrameTransform or RTCRtpScriptTransform) RTCRtpTransform;
typedef RTCRtpScriptTransform RTCRtpTransform;

[Pref="media.peerconnection.enabled",
 Pref="media.peerconnection.scripttransform.enabled",
 Exposed=Window]
interface RTCRtpScriptTransform {
  [Throws]
  constructor(Worker worker, optional any options, optional sequence<object> transfer);
};
