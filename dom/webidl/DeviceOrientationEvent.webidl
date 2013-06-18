/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional DeviceOrientationEventInit eventInitDict), HeaderFile="GeneratedEventClasses.h"]
interface DeviceOrientationEvent : Event
{
  readonly attribute double alpha;
  readonly attribute double beta;
  readonly attribute double gamma;
  readonly attribute boolean absolute;

  // initDeviceOrientationEvent is a Gecko specific deprecated method.
  [Throws]
  void initDeviceOrientationEvent(DOMString type,
                                  boolean canBubble,
                                  boolean cancelable,
                                  double alpha,
                                  double beta,
                                  double gamma,
                                  boolean absolute);
};

dictionary DeviceOrientationEventInit : EventInit
{
  double alpha = 0;
  double beta = 0;
  double gamma = 0;
  boolean absolute = false;
};
