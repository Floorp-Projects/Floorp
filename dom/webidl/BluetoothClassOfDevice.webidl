/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[CheckPermissions="bluetooth"]
interface BluetoothClassOfDevice
{
  /**
   * The following constants are defined in Assigned Numbers of bluetooth
   * General Access Profile (GAP) spec. For more information see
   *   https://www.bluetooth.org/en-us/specification/assigned-numbers/baseband
   */

  // Major service class
  const unsigned short LIMITED_DISCOVERABILITY = 0x0001;
  const unsigned short POSITIONING             = 0x0008;
  const unsigned short NETWORKING              = 0x0010;
  const unsigned short RENDERING               = 0x0020;
  const unsigned short CAPTURING               = 0x0040;
  const unsigned short OBJECT_TRANSFER         = 0x0080;
  const unsigned short AUDIO                   = 0x0100;
  const unsigned short TELEPHONY               = 0x0200;
  const unsigned short INFORMATION             = 0x0400;

  // Major device class
  const octet MISC          = 0x00;
  const octet COMPUTER      = 0x01;
  const octet PHONE         = 0x02;
  const octet NETWORK       = 0x03;
  const octet AUDIO_VIDEO   = 0x04;
  const octet PERIPHERAL    = 0x05;
  const octet IMAGING       = 0x06;
  const octet WEARABLE      = 0x07;
  const octet TOY           = 0x08;
  const octet HEALTH        = 0x09;
  const octet UNCATEGORIZED = 0x1F;

  readonly attribute unsigned short majorServiceClass;
  readonly attribute octet majorDeviceClass;
  readonly attribute octet minorDeviceClass;
};
