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
interface AudioParam {

    attribute float value;
    // readonly attribute float computedValue;
    readonly attribute float minValue;
    readonly attribute float maxValue;
    readonly attribute float defaultValue;

    // Parameter automation. 
    [Throws]
    void setValueAtTime(float value, float startTime);
    [Throws]
    void linearRampToValueAtTime(float value, float endTime);
    [Throws]
    void exponentialRampToValueAtTime(float value, float endTime);

    // Exponentially approach the target value with a rate having the given time constant. 
    [Throws]
    void setTargetAtTime(float target, float startTime, float timeConstant);

    // Sets an array of arbitrary parameter values starting at time for the given duration. 
    // The number of values will be scaled to fit into the desired duration. 
    // [Throws]
    // void setValueCurveAtTime(Float32Array values, float startTime, float duration);

    // Cancels all scheduled parameter changes with times greater than or equal to startTime. 
    void cancelScheduledValues(float startTime);

};

