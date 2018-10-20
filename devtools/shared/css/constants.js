/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * All CSS <angle> types that properties can support.
 */
exports.CSS_ANGLEUNIT = {
  "deg": "deg",
  "rad": "rad",
  "grad": "grad",
  "turn": "turn",
};

/**
 * All CSS types that properties can support. This list can be manually edited.
 *
 * The existing numbers are for backward compatibility so that newer versions
 * are still able to debug an old version correctly.
 */
exports.CSS_TYPES = {
  "COLOR": 2,
  "GRADIENT": 4,
  "TIMING_FUNCTION": 10,
};
