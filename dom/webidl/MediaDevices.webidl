/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Func="Navigator::HasUserMediaSupport",
 Exposed=Window]
interface MediaDevices : EventTarget {
  [Pref="media.ondevicechange.enabled"]
  attribute EventHandler ondevicechange;
  MediaTrackSupportedConstraints getSupportedConstraints();

  [NewObject, UseCounter]
  Promise<sequence<MediaDeviceInfo>> enumerateDevices();

  [NewObject, NeedsCallerType, UseCounter]
  Promise<MediaStream> getUserMedia(optional MediaStreamConstraints constraints = {});

  // We need [SecureContext] in case media.devices.insecure.enabled = true
  // because we don't want that legacy pref to expose this newer method.
  [SecureContext, Pref="media.getdisplaymedia.enabled", NewObject, NeedsCallerType, UseCounter]
  Promise<MediaStream> getDisplayMedia(optional DisplayMediaStreamConstraints constraints = {});
};

// https://w3c.github.io/mediacapture-output/#audiooutputoptions-dictionary
dictionary AudioOutputOptions {
  DOMString deviceId = "";
};
// https://w3c.github.io/mediacapture-output/#mediadevices-extensions
partial interface MediaDevices {
  [SecureContext, Pref="media.setsinkid.enabled", NewObject, NeedsCallerType]
  Promise<MediaDeviceInfo> selectAudioOutput(optional AudioOutputOptions options = {});
};
