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

  [Throws, NeedsCallerType, UseCounter]
  Promise<sequence<MediaDeviceInfo>> enumerateDevices();

  [Throws, NeedsCallerType, UseCounter]
  Promise<MediaStream> getUserMedia(optional MediaStreamConstraints constraints = {});

  // We need [SecureContext] in case media.devices.insecure.enabled = true
  // because we don't want that legacy pref to expose this newer method.
  [SecureContext, Pref="media.getdisplaymedia.enabled", Throws, NeedsCallerType, UseCounter]
  Promise<MediaStream> getDisplayMedia(optional DisplayMediaStreamConstraints constraints = {});
};
