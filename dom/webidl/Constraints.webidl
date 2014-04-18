/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// These dictionaries need to be in a separate file from their use in unions
// in MediaTrackConstraintSet.webidl due to a webidl compiler limitation.

enum VideoFacingModeEnum {
    "user",
    "environment",
    "left",
    "right"
};

dictionary ConstrainLongRange {
    long min = -2147483647; // +1 works around windows compiler bug
    long max = 2147483647;
};

dictionary ConstrainDoubleRange {
    unrestricted double min = -Infinity;
    unrestricted double max = Infinity;
};
