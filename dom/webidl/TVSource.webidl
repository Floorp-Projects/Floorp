/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://seanyhlin.github.io/TV-Manager-API/
 */

enum TVSourceType {
  "dvb-t",
  "dvb-t2",
  "dvb-c",
  "dvb-c2",
  "dvb-s",
  "dvb-s2",
  "dvb-h",
  "dvb-sh",
  "atsc",
  "atsc-m/h",
  "isdb-t",
  "isdb-tb",
  "isdb-s",
  "isdb-c",
  "1seg",
  "dtmb",
  "cmmb",
  "t-dmb",
  "s-dmb"
};

dictionary TVStartScanningOptions {
  boolean isRescanned;
};

[Pref="dom.tv.enabled", CheckAnyPermissions="tv", AvailableIn=CertifiedApps]
interface TVSource : EventTarget {
  [Throws]
  Promise<sequence<TVChannel>> getChannels();

  [Throws]
  Promise<void> setCurrentChannel(DOMString channelNumber);

  [Throws]
  Promise<void> startScanning(optional TVStartScanningOptions options);

  [Throws]
  Promise<void> stopScanning();

  readonly attribute TVTuner tuner;

  readonly attribute TVSourceType type;

  readonly attribute boolean isScanning;

  readonly attribute TVChannel? currentChannel;

  attribute EventHandler oncurrentchannelchanged;
  attribute EventHandler oneitbroadcasted;
  attribute EventHandler onscanningstatechanged;
};
