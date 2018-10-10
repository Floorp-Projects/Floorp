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
    // If values are added, adjust n_values in Histograms.json (2 places)
};

typedef (long or ConstrainLongRange) ConstrainLong;
typedef (double or ConstrainDoubleRange) ConstrainDouble;
typedef (boolean or ConstrainBooleanParameters) ConstrainBoolean;
typedef (DOMString or sequence<DOMString> or ConstrainDOMStringParameters) ConstrainDOMString;

// Note: When adding new constraints, remember to update the SelectSettings()
// function in MediaManager.cpp to make OverconstrainedError's constraint work!

dictionary MediaTrackConstraintSet {
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainLong width = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainLong height = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainDouble frameRate = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainDOMString facingMode = null;
    DOMString mediaSource = "camera";
    long long browserWindow;
    boolean scrollWithPage;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainDOMString deviceId = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainLong viewportOffsetX = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainLong viewportOffsetY = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainLong viewportWidth = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainLong viewportHeight = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainBoolean echoCancellation = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainBoolean noiseSuppression = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainBoolean autoGainControl = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainLong channelCount = null;

    // Deprecated with warnings:
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainBoolean mozNoiseSuppression = null;
    // FIXME: bug 1493860 or bug 1493798: should this "= null" be here?
    ConstrainBoolean mozAutoGainControl = null;
};

dictionary MediaTrackConstraints : MediaTrackConstraintSet {
    sequence<MediaTrackConstraintSet> advanced;
};

enum MediaStreamTrackState {
    "live",
    "ended"
};

[Exposed=Window]
interface MediaStreamTrack : EventTarget {
    readonly    attribute DOMString             kind;
    readonly    attribute DOMString             id;
    [NeedsCallerType]
    readonly    attribute DOMString             label;
                attribute boolean               enabled;
    readonly    attribute boolean               muted;
                attribute EventHandler          onmute;
                attribute EventHandler          onunmute;
    readonly    attribute MediaStreamTrackState readyState;
                attribute EventHandler          onended;
    MediaStreamTrack       clone ();
    void                   stop ();
//  MediaTrackCapabilities getCapabilities ();
    MediaTrackConstraints  getConstraints ();
    [NeedsCallerType]
    MediaTrackSettings     getSettings ();

    [Throws, NeedsCallerType]
    Promise<void>          applyConstraints (optional MediaTrackConstraints constraints);
//              attribute EventHandler          onoverconstrained;

    [ChromeOnly]
    void mutedChanged(boolean muted);
};
