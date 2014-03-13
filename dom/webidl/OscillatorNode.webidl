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

enum OscillatorType {
  // Hack: Use numbers to support alternate enum values
  "0", "1", "2", "3", "4",

  "sine",
  "square",
  "sawtooth",
  "triangle",
  "custom"
};

interface OscillatorNode : AudioNode {

    [SetterThrows]
    attribute OscillatorType type;

    readonly attribute AudioParam frequency; // in Hertz
    readonly attribute AudioParam detune; // in Cents

    [Throws]
    void start(optional double when = 0);
    [Throws]
    void stop(optional double when = 0);
    void setPeriodicWave(PeriodicWave periodicWave);

    attribute EventHandler onended;

};

/*
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html#AlternateNames
 */
partial interface OscillatorNode {
    // Same as start()
    [Throws,Pref="media.webaudio.legacy.OscillatorNode"]
    void noteOn(double when);

    // Same as stop()
    [Throws,Pref="media.webaudio.legacy.OscillatorNode"]
    void noteOff(double when);

    [Pref="media.webaudio.legacy.OscillatorNode"]
    const unsigned short SINE = 0;
    [Pref="media.webaudio.legacy.OscillatorNode"]
    const unsigned short SQUARE = 1;
    [Pref="media.webaudio.legacy.OscillatorNode"]
    const unsigned short SAWTOOTH = 2;
    [Pref="media.webaudio.legacy.OscillatorNode"]
    const unsigned short TRIANGLE = 3;
    [Pref="media.webaudio.legacy.OscillatorNode"]
    const unsigned short CUSTOM = 4;
};

