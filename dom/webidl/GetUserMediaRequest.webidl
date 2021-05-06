/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This is an internal IDL file
 */

interface nsIMediaDevice;

// For gUM request start (getUserMedia:request) notification,
// rawID, mediaSource and audioOutputOptions won't be set.
// For selectAudioOutput request start (getUserMedia:request) notification,
// rawID, mediaSource and constraints won't be set.
// For gUM request stop (recording-device-stopped) notification due to page
// reload, only windowID will be set.
// For gUM request stop (recording-device-stopped) notification due to track
// stop, only type, windowID, rawID and mediaSource will be set

enum GetUserMediaRequestType {
    "getusermedia",
    "selectaudiooutput",
    "recording-device-stopped"
};

[LegacyNoInterfaceObject,
 Exposed=Window]
interface GetUserMediaRequest {
  readonly attribute GetUserMediaRequestType type;
  readonly attribute unsigned long long windowID;
  readonly attribute unsigned long long innerWindowID;
  readonly attribute DOMString callID;
  readonly attribute DOMString rawID;
  readonly attribute DOMString mediaSource;
  // The set of devices to consider
  [Constant, Cached, Frozen]
  readonly attribute sequence<nsIMediaDevice> devices;
  MediaStreamConstraints getConstraints();
  AudioOutputOptions getAudioOutputOptions();
  readonly attribute boolean isSecure;
  readonly attribute boolean isHandlingUserInput;
};
