/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum BiquadFilterType {
  // Hack: Use numbers to support alternate enum values
  "0", "1", "2", "3", "4", "5", "6", "7",

  "lowpass",
  "highpass",
  "bandpass",
  "lowshelf",
  "highshelf",
  "peaking",
  "notch",
  "allpass"
};

interface BiquadFilterNode : AudioNode {

    attribute BiquadFilterType type;
    readonly attribute AudioParam frequency; // in Hertz
    readonly attribute AudioParam detune; // in Cents
    readonly attribute AudioParam Q; // Quality factor
    readonly attribute AudioParam gain; // in Decibels

    void getFrequencyResponse(Float32Array frequencyHz,
                              Float32Array magResponse,
                              Float32Array phaseResponse);

};

/*
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html#AlternateNames
 */
partial interface BiquadFilterNode {
    [Pref="media.webaudio.legacy.BiquadFilterNode"]
    const unsigned short LOWPASS = 0;
    [Pref="media.webaudio.legacy.BiquadFilterNode"]
    const unsigned short HIGHPASS = 1;
    [Pref="media.webaudio.legacy.BiquadFilterNode"]
    const unsigned short BANDPASS = 2;
    [Pref="media.webaudio.legacy.BiquadFilterNode"]
    const unsigned short LOWSHELF = 3;
    [Pref="media.webaudio.legacy.BiquadFilterNode"]
    const unsigned short HIGHSHELF = 4;
    [Pref="media.webaudio.legacy.BiquadFilterNode"]
    const unsigned short PEAKING = 5;
    [Pref="media.webaudio.legacy.BiquadFilterNode"]
    const unsigned short NOTCH = 6;
    [Pref="media.webaudio.legacy.BiquadFilterNode"]
    const unsigned short ALLPASS = 7;
};

