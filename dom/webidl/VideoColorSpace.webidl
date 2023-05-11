/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#videocolorspace
 */

[Exposed=(Window,DedicatedWorker), Pref="dom.media.webcodecs.enabled"]
interface VideoColorSpace {
  [Throws]
  constructor(optional VideoColorSpaceInit init = {});

  readonly attribute VideoColorPrimaries? primaries;
  readonly attribute VideoTransferCharacteristics? transfer;
  readonly attribute VideoMatrixCoefficients? matrix;
  readonly attribute boolean? fullRange;

  // https://github.com/w3c/webcodecs/issues/486
  [Default] object toJSON();
};

dictionary VideoColorSpaceInit {
  VideoColorPrimaries? primaries = null;
  VideoTransferCharacteristics? transfer = null;
  VideoMatrixCoefficients? matrix = null;
  boolean? fullRange = null;
};

enum VideoColorPrimaries {
  "bt709",      // BT.709, sRGB
  "bt470bg",    // BT.601 PAL
  "smpte170m",  // BT.601 NTSC
  "bt2020",     // BT.2020, BT.2100
  "smpte432",   // P3 D65
};

enum VideoTransferCharacteristics {
  "bt709",         // BT.709
  "smpte170m",     // BT.601 (functionally the same as bt709)
  "iec61966-2-1",  // sRGB
  "linear",        // linear RGB
  "pq",            // BT.2100 PQ
  "hlg",           // BT.2100 HLG
};

enum VideoMatrixCoefficients {
  "rgb",        // sRGB
  "bt709",      // BT.709
  "bt470bg",    // BT.601 PAL
  "smpte170m",  // BT.601 NTSC (functionally the same as bt470bg)
  "bt2020-ncl", // BT.2020 NCL
};
