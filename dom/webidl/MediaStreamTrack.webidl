/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

// These two enums are in the spec even though they're not used directly in the
// API due to https://www.w3.org/Bugs/Public/show_bug.cgi?id=19936
// Their binding code is used in the implementation.

enum VideoFacingModeEnum {
    "user",
    "environment",
    "left",
    "right"
};

enum MediaSourceEnum {
    "camera",
    "screen",
    "application",
    "window",
    "browser",
    "microphone",
    "audioCapture",
    "other"
};

typedef (long or ConstrainLongRange) ConstrainLong;
typedef (double or ConstrainDoubleRange) ConstrainDouble;
typedef (boolean or ConstrainBooleanParameters) ConstrainBoolean;
typedef (DOMString or sequence<DOMString> or ConstrainDOMStringParameters) ConstrainDOMString;

dictionary MediaTrackConstraintSet {
    ConstrainLong width;
    ConstrainLong height;
    ConstrainDouble frameRate;
    ConstrainDOMString facingMode;
    DOMString mediaSource = "camera";
    long long browserWindow;
    boolean scrollWithPage;
    ConstrainDOMString deviceId;
};

dictionary MediaTrackConstraints : MediaTrackConstraintSet {
    sequence<MediaTrackConstraintSet> advanced;
};

interface MediaStreamTrack {
    readonly    attribute DOMString             kind;
    readonly    attribute DOMString             id;
    readonly    attribute DOMString             label;
                attribute boolean               enabled;
//    readonly    attribute MediaStreamTrackState readyState;
//    readonly    attribute SourceTypeEnum        sourceType;
//    readonly    attribute DOMString             sourceId;
//                attribute EventHandler          onstarted;
//                attribute EventHandler          onmute;
//                attribute EventHandler          onunmute;
//                attribute EventHandler          onended;
//    any                    getConstraint (DOMString constraintName, optional boolean mandatory = false);
//    void                   setConstraint (DOMString constraintName, any constraintValue, optional boolean mandatory = false);
//    MediaTrackConstraints? constraints ();
//    void                   applyConstraints (MediaTrackConstraints constraints);
//    void                   prependConstraint (DOMString constraintName, any constraintValue);
//    void                   appendConstraint (DOMString constraintName, any constraintValue);
//                attribute EventHandler          onoverconstrained;
    void                   stop ();
};
