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

enum PanningModelType {
  "equalpower",
  "HRTF"
};

enum DistanceModelType {
  "linear",
  "inverse",
  "exponential"
};

[PrefControlled]
interface PannerNode : AudioNode {

    // Default for stereo is HRTF 
    attribute PanningModelType panningModel;

    // Uses a 3D cartesian coordinate system 
    void setPosition(double x, double y, double z);
    void setOrientation(double x, double y, double z);
    void setVelocity(double x, double y, double z);

    // Distance model and attributes 
    attribute DistanceModelType distanceModel;
    attribute double refDistance;
    attribute double maxDistance;
    attribute double rolloffFactor;

    // Directional sound cone 
    attribute double coneInnerAngle;
    attribute double coneOuterAngle;
    attribute double coneOuterGain;

};

