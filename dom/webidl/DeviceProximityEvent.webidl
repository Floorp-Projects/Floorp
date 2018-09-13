/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="device.sensors.proximity.enabled", Func="nsGlobalWindowInner::DeviceSensorsEnabled", Constructor(DOMString type, optional DeviceProximityEventInit eventInitDict)]
interface DeviceProximityEvent : Event
{
  readonly attribute double value;
  readonly attribute double min;
  readonly attribute double max;
};

dictionary DeviceProximityEventInit : EventInit
{
  unrestricted double value = Infinity;
  unrestricted double min = -Infinity;
  unrestricted double max = Infinity;
};
