/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="camera.control.face_detection.enabled",
 Func="Navigator::HasCameraSupport",
 Constructor(DOMString type, optional CameraFacesDetectedEventInit eventInitDict)]
interface CameraFacesDetectedEvent : Event
{
  [Pure, Cached]
  readonly attribute sequence<CameraDetectedFace>? faces;
};

dictionary CameraFacesDetectedEventInit : EventInit
{
  sequence<CameraDetectedFace>? faces = null;
};
