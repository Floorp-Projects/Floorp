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

[PrefControlled]
interface PannerNode : AudioNode {

    // Panning model 
    const unsigned short EQUALPOWER = 0;
    const unsigned short HRTF = 1;
    const unsigned short SOUNDFIELD = 2;

    // Distance model 
    const unsigned short LINEAR_DISTANCE = 0;
    const unsigned short INVERSE_DISTANCE = 1;
    const unsigned short EXPONENTIAL_DISTANCE = 2;

    // Default for stereo is HRTF 
    [SetterThrows]
    attribute unsigned short panningModel;

    // Uses a 3D cartesian coordinate system 
    void setPosition(float x, float y, float z);
    void setOrientation(float x, float y, float z);
    void setVelocity(float x, float y, float z);

    // Distance model and attributes 
    [SetterThrows]
    attribute unsigned short distanceModel;
    attribute float refDistance;
    attribute float maxDistance;
    attribute float rolloffFactor;

    // Directional sound cone 
    attribute float coneInnerAngle;
    attribute float coneOuterAngle;
    attribute float coneOuterGain;

};

