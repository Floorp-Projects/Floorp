/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://webaudio.github.io/web-audio-api/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum OscillatorType {
  "sine",
  "square",
  "sawtooth",
  "triangle",
  "custom"
};

[Pref="dom.webaudio.enabled"]
interface OscillatorNode : AudioNode {

    [SetterThrows]
    attribute OscillatorType type;

    readonly attribute AudioParam frequency; // in Hertz
    readonly attribute AudioParam detune; // in Cents

    [Throws, UnsafeInPrerendering]
    void start(optional double when = 0);
    [Throws, UnsafeInPrerendering]
    void stop(optional double when = 0);
    void setPeriodicWave(PeriodicWave periodicWave);

    attribute EventHandler onended;

};

// Mozilla extensions
OscillatorNode implements AudioNodePassThrough;

