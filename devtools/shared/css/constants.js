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
 * Mapping of InspectorPropertyType to old type ID.
 * Kept for backwards compatibility. Remove after Firefox 70.
 */
exports.CSS_TYPES = {
 "color": 2,
 "gradient": 4,
 "timing-function": 10,
};
