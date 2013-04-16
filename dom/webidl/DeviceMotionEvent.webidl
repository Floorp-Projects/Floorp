/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface DeviceAcceleration;
interface DeviceRotationRate;

interface DeviceMotionEvent : Event
{
  [Throws]
  void initDeviceMotionEvent(DOMString type,
                             boolean canBubble,
                             boolean cancelable,
                             DeviceAcceleration? acceleration,
                             DeviceAcceleration? accelerationIncludingGravity,
                             DeviceRotationRate? rotationRate,
                             double interval);

  readonly attribute DeviceAcceleration? acceleration;
  readonly attribute DeviceAcceleration? accelerationIncludingGravity;
  readonly attribute DeviceRotationRate? rotationRate;
  readonly attribute double interval;
};

