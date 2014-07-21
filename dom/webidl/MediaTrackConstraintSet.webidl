/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html
 */

enum SupportedVideoConstraints {
    "other",
    "facingMode",
    "width",
    "height",
    "frameRate",
    "mediaSource",
    "browserWindow",
    "scrollWithPage"
};

enum SupportedAudioConstraints {
    "other"
};


dictionary MediaTrackConstraintSet {
    ConstrainLongRange width;
    ConstrainLongRange height;
    ConstrainDoubleRange frameRate;
    ConstrainVideoFacingMode facingMode;
    ConstrainMediaSource mediaSource = "camera";
    long long browserWindow;
    boolean scrollWithPage;
};

// TODO: Bug 995352 can't nest unions
//typedef (long or ConstrainLongRange) ConstrainLong;
//typedef (double or ConstrainDoubleRange) ConstrainDouble;

typedef VideoFacingModeEnum ConstrainVideoFacingMode;
typedef MediaSourceEnum ConstrainMediaSource;

// TODO: Bug 767924 sequences in unions
//typedef (VideoFacingModeEnum or sequence<VideoFacingModeEnum>) ConstrainVideoFacingMode;
//typedef (MediaSourceEnum or sequence<MediaSourceEnum>) ConstrainMediaSource;