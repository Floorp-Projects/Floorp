/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothuuid_h__
#define mozilla_dom_bluetooth_bluetoothuuid_h__

namespace mozilla {
namespace dom {
namespace bluetooth {

namespace BluetoothServiceUuid {
  static unsigned long long AudioSink    = 0x0000110B00000000;
  static unsigned long long AudioSource  = 0x0000110A00000000;
  static unsigned long long AdvAudioDist = 0x0000110D00000000;
  static unsigned long long Headset      = 0x0000110800000000;
  static unsigned long long HeadsetAG    = 0x0000111200000000;
  static unsigned long long Handsfree    = 0x0000111E00000000;
  static unsigned long long HandsfreeAG  = 0x0000111F00000000;

  static unsigned long long BaseMSB     = 0x0000000000001000;
  static unsigned long long BaseLSB     = 0x800000805F9B34FB;
}

namespace BluetoothServiceUuidStr {
  static const char* AudioSink     = "0000110B-0000-1000-8000-00805F9B34FB";
  static const char* AudioSource   = "0000110A-0000-1000-8000-00805F9B34FB";
  static const char* AdvAudioDist  = "0000110D-0000-1000-8000-00805F9B34FB";
  static const char* Headset       = "00001108-0000-1000-8000-00805F9B34FB";
  static const char* HeadsetAG     = "00001112-0000-1000-8000-00805F9B34FB";
  static const char* Handsfree     = "0000111E-0000-1000-8000-00805F9B34FB";
  static const char* HandsfreeAG   = "0000111F-0000-1000-8000-00805F9B34FB";
}

}
}
}

#endif

